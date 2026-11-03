#include <dmaKit.h>
#include <gsKit.h>
#include <kernel.h>
#include <malloc.h>
#include <string.h>
#ifdef SMK64_DEV
#include <stdio.h>
#include <screenshot.h>
#endif

#include <ultra64.h>

#include "graficos/sintetizador_gs.h"
#include "sistema/sistema_ps2.h"
#include "graficos/pantallas_gigantes.h"
#include "sistema/perfilado.h"
#ifdef SMK64_MEDIDOR
#include "depuracion/medidor_rendimiento.h"
#endif

#define PACKET_QWORDS 0xFFF0 /* almost 1 MB: a single normal DMA transfer (16-bit QWC) */
/* Packet buffers */
#define PACKET_SLOTS 3
#define XYOFFSET_X   (2048 - GS_WIDTH / 2)
#define XYOFFSET_Y   (2048 - GS_HEIGHT / 2)

typedef union {
    u128 q;
    u64 d[2];
    u32 w[4];
    float f[4];
} Qword __attribute__((aligned(16)));

static Qword packets[PACKET_SLOTS][PACKET_QWORDS] __attribute__((aligned(64)));

/* Upload capture for the 60 FPS frame; in hires (no 60 FPS) the sink for a frame that is cut short */
#define CAPTURE_QWORDS (32 * 1024)
static Qword capture_buf[CAPTURE_QWORDS] __attribute__((aligned(64)));
static int sinking; /* hires: the packet is full; the rest of the frame goes to capture_buf, unsent */

#define MAX_FRAME_FIXUPS 16

typedef struct {
    Qword *end;          /* end of the already-closed contents */
    int buffer;          /* target framebuffer */
    int flags;
    int overflow;        /* did not fit (only with GS_PACKET_NO_PARTIAL): discard */
    int partial_sent;    /* part of it was already sent while it was being built */
    Qword *frame_fixups[MAX_FRAME_FIXUPS];
    int num_fixups;
} PacketInfo;

static PacketInfo packet_info[GS_NUM_PACKETS];
static int cur_packet;  /* packet being built */
static int packet_slot[GS_NUM_PACKETS] = { 0, 1 }; /* buffer used by each packet */
static void (*overflow_hook)(void);
static Qword *s_pkt;       /* start of the current packet */
static Qword *cur;
static Qword *prim_tag;   /* GIF tag of the open primitive batch, or NULL */
static u32 prim_nloop;
static u32 prim_regs;     /* number of registers per vertex in the batch */

static GSGLOBAL *s_gs;
static int draw_buffer;           /* buffer this frame is drawn into */
static volatile int display_buffer;
static volatile int pending_flip = -1;
static s32 flip_sema;
static GsState state;            /* last state emitted */
static int state_valid;

GsStats gs_stats;

struct VideoMode {
    const char *name;
    s16 mode;
    s16 interlace;
    s16 field;
    int width;  /* multiple of 64 (FBW) */
    int height; /* framebuffer lines (per field in interlaced FRAME mode) */
    int hires;  /* banded multi-pass rendering */
};

static const struct VideoMode video_modes[GS_NUM_VIDEO_MODES] = {
    { "240p",  GS_MODE_NTSC,      GS_NONINTERLACED, GS_FRAME,  320, 224, 0 },
#if !defined(VERSION_EU)
    { "480i",  GS_MODE_NTSC,      GS_INTERLACED,    GS_FIELD,  704, 452, 0 },
    { "480p",  GS_MODE_DTV_480P,  GS_NONINTERLACED, GS_FRAME,  704, 452, 0 },
#else
    { "576i",  GS_MODE_PAL,       GS_INTERLACED,    GS_FIELD,  704, 536, 0 },
    { "576p",  GS_MODE_DTV_576P,  GS_NONINTERLACED, GS_FRAME,  704, 536, 0 },
#endif
    { "720p",  GS_MODE_DTV_720P,  GS_NONINTERLACED, GS_FRAME, 1280, 720, 1 },
    /* 540 lines per field; alternate fields are drawn half a line apart */
    { "1080i", GS_MODE_DTV_1080I, GS_INTERLACED,    GS_FRAME, 1920, 540, 1 },
};

static const struct VideoMode *video_mode = &video_modes[GS_VIDEO_MODE];
int gs_width, gs_height;
float gs_scale_x, gs_scale_y;
static int screen_tex_th; /* log2 of the texture height that covers the framebuffer (TEX0.TH) */
static int z_shift;       /* XYZ2 Z is 24-bit; the hires Z-buffer is 16-bit */

int gs_set_video_mode(int mode)
{
    if (mode < 0 || mode >= GS_NUM_VIDEO_MODES || s_gs != NULL) {
        return 0;
    }
    video_mode = &video_modes[mode];
    return 1;
}

/* --- Hires: banded multi-pass rendering -----------------------------------------------------
   720p/1080i double-buffered with a Z-buffer do not fit in 4 MB of VRAM. As in gsKit's gsHires,
   there is a single front buffer split into HIRES_BANDS horizontal bands, and every refresh the
   GS draws the frame once per band, one band ahead of the CRTC: while the CRTC shows band b-1,
   the GS draws band b with its Z-buffer in band b+1 (which is drawn over before it is shown).
   Passes are started from the hsync interrupt, and the same packet is replayed until the next
   one is presented.

   Unlike gsHires, packets keep setting FRAME/ZBUF/XYOFFSET/SCISSOR themselves (state changes,
   split-screen viewports, Z clears), so every write of those registers is recorded as a band
   fixup and rewritten for each band before the packet is sent. The colour framebuffer is written
   with FBP = band_base, a Z-buffer written as colour (gs_clear_z) with any other FBP. */
#define HIRES_BANDS     3
#define MAX_BAND_FIXUPS 2048 /* ZBUF/SCISSOR are only written when they change: a race frame needs ~12 */
/* Fixup reg for a SCISSOR_1 kept as written in one band's pass (BAND_ONLY_SCISSOR + band) and
   empty in the others: giant screen copies read each band right after its own pass */
#define BAND_ONLY_SCISSOR 0x100u

typedef struct {
    u32 off;   /* qword offset in the packet */
    u32 reg;
    u64 value; /* as emitted: full-screen coordinates */
} BandFixup;

static int hires;
static int band_h;             /* lines per band, multiple of 64 (CT16S/Z16S page height) */
static u32 band_base;          /* FBP of band 0 */
static u32 band_pages;         /* 8 KB pages per band */
static int band_field_offset;  /* 1080i: XYOFFSET Y offset (1/16 px) for the even field */
static u32 band_trigger[HIRES_BANDS]; /* CRTC line that starts each band's pass */
static u32 band_nominal_lines;        /* scanlines per refresh (per field in 1080i) */
/* The CRTC line is worked out from the CPU cycle counter since the last VBLANK, not by counting
   HSINTs: a slow pass in the interrupt merges several HSINTs into one, so a count falls behind
   (and never reached band 0's line in races), and emulators do not raise one per line either. */
static volatile u32 refresh_start;    /* COP0 Count at the last VBLANK */
static volatile u32 refresh_cycles;   /* measured refresh period */
static volatile u32 line_mul;         /* band_nominal_lines / refresh_cycles in 0.32 fixed point */
static volatile u32 bands_fired;      /* bands started (or queued) this refresh */
static BandFixup *band_fixups[PACKET_SLOTS];
static int band_num_fixups[PACKET_SLOTS];
static int band_fixup_overflow[PACKET_SLOTS];
static Qword *band_packet_end[PACKET_SLOTS];
static int build_slot;                    /* slot of the packet being built */
static volatile u32 slot_busy; /* packet buffers queued or in the GS */
static volatile int hires_cur = -1;       /* slot being replayed */
static volatile int hires_next = -1;      /* presented, shown from the next band 0 pass */
static volatile int render_field;         /* 1080i: field the current refresh is drawn for */
static volatile int hires_paused;
static volatile int band_waiting = -1; /* triggered while the previous pass was still being sent */
static volatile int switch_due;        /* band 0's turn came: show hires_next from the next pass */
static int uploads_lost;               /* an upload after the frame was cut never reached VRAM */

/* Texture uploads go inline in the packet, as with gsKit's texture manager under gsHires: every
   pass replays them before the draws that use them, so VRAM is right for the frame on every pass
   however late it is. Each pass resends them, so a frame's uploads are capped (the texture cache
   leaves the rest for the next frame), and the packet keeps room for the geometry after them. */
#define UPLOAD_BUDGET       (384 * 1024)
#define UPLOAD_GEOMETRY_QWC (16 * 1024) /* 256 KB */
static u32 upload_bytes_frame;
static volatile u32 hires_vblanks, hires_last_lines; /* diagnostics: last line seen per refresh */
static volatile u32 band_start_line[HIRES_BANDS];     /* diagnostics: CRTC line each band last started on */
static volatile u32 hsint_count, hsint_last;          /* diagnostics: HSINTs this refresh / last refresh */
static volatile u32 hsint_rearms;                     /* refreshes with no HSINT, after which it was re-armed */
static u32 band_fixups_max, band_qwc_max, band_dropped_overflow;

int gs_hires(void)
{
    return hires;
}

int gs_upload_fits(u32 bytes)
{
    return !hires || (!sinking && upload_bytes_frame + bytes <= UPLOAD_BUDGET &&
                      (u32) (cur - s_pkt) + (bytes + 15) / 16 + UPLOAD_GEOMETRY_QWC < PACKET_QWORDS);
}

void gs_hires_diag(u32 out[6])
{
    int i;

    for (i = 0; i < 3; i++) {
        out[i] = i < HIRES_BANDS ? band_start_line[i] : 0;
    }
    out[3] = hsint_last;
    out[4] = refresh_cycles / 295; /* us */
    out[5] = hsint_rearms;
}

int gs_take_lost_uploads(void)
{
    int lost = uploads_lost;

    uploads_lost = 0;
    return lost;
}

#ifdef SMK64_DEV
/* Called from the VI thread every couple of seconds */
void gs_hires_log_status(void)
{
    if (hires) {
        printf("smk64: GS hires: vblank %u last line %u (band 0 at %u) passes %u overruns %u flips %u dropped %u (packet full %u) "
               "max fixups %u max qwc %u busy %x cur %d next %d\n",
               (unsigned) hires_vblanks, (unsigned) hires_last_lines, (unsigned) band_trigger[0], (unsigned) gs_stats.band_passes,
               (unsigned) gs_stats.band_overruns, (unsigned) gs_stats.flips, (unsigned) gs_stats.dropped,
               (unsigned) band_dropped_overflow, (unsigned) band_fixups_max, (unsigned) band_qwc_max, (unsigned) slot_busy,
               hires_cur, hires_next);
    }
}
#endif

#define GIF_FLG_PACKED 0
#define GIF_FLG_IMAGE  2
#define GIF_REG_AD     0xE

static inline u64 giftag_lo(u32 nloop, int eop, int pre, u32 prim, int flg, int nreg)
{
    return (u64) (nloop & 0x7FFF) | ((u64) eop << 15) | ((u64) pre << 46) | ((u64) (prim & 0x7FF) << 47) |
           ((u64) flg << 58) | ((u64) (nreg & 0xF) << 60);
}

static void flush_prims(void)
{
    if (prim_tag != NULL) {
        prim_tag->d[0] = (prim_tag->d[0] & ~0x7FFFULL) | prim_nloop;
        prim_tag = NULL;
    }
}

static void kick(int wait);

static void hires_init(void);

static void packet_full(void);

/* Ensures there is room for n qwords */
static inline void reserve(u32 n)
{
    if ((u32) (cur - s_pkt) + n + 8 >= (sinking ? CAPTURE_QWORDS : PACKET_QWORDS)) {
        packet_full();
    }
}

static void begin_ad(Qword **tag, u32 n)
{
    flush_prims();
    reserve(n + 1);
    *tag = cur++;
    (*tag)->d[0] = giftag_lo(n, 0, 0, 0, GIF_FLG_PACKED, 1);
    (*tag)->d[1] = GIF_REG_AD;
}

static inline void ad(u32 reg, u64 value)
{
    cur->d[0] = value;
    cur->d[1] = reg;
    cur++;
}

/* ad() for FRAME_1/ZBUF_1/XYOFFSET_1/SCISSOR_1 inside a packet: in hires they depend on the band */
static inline void ad_band(u32 reg, u64 value)
{
    if (hires && !sinking) {
        if (band_num_fixups[build_slot] < MAX_BAND_FIXUPS) {
            BandFixup *f = &band_fixups[build_slot][band_num_fixups[build_slot]++];

            f->off = (u32) (cur - s_pkt);
            f->reg = reg;
            f->value = value;
        } else {
            band_fixup_overflow[build_slot] = 1; /* not reached: band_room() is checked first */
        }
    }
    ad(reg, value);
}

/* Hires: the frame does not fit (packet or band fixups) and cannot be sent in pieces, since every
   band replays all of it. It is shown cut short at this point, and the rest of the frame is
   written into capture_buf and never sent. Dropping the whole frame instead would show nothing
   at all when every frame is too big. */
static void hires_sink(void)
{
    flush_prims();
    packet_info[cur_packet].end = cur;
    sinking = 1;
    s_pkt = cur = capture_buf;
    gs_stats.dropped++;
}

/* Before a block that writes k band fixups: the list must not run out halfway through it */
static inline void band_room(int k)
{
    if (hires && !sinking && band_num_fixups[build_slot] + k > MAX_BAND_FIXUPS) {
        hires_sink();
    }
}

#define D2_CHCR ((volatile u32 *) 0x1000A000)

/* Waits for the GIF DMA to finish */
static void wait_gif(void)
{
    u32 spins = 0;
#ifdef SMK64_MEDIDOR
    u32 start = medidor_leer_ciclos();
#endif

    while (*D2_CHCR & 0x100) {
        if (++spins == 20u * 1000u * 1000u) { /* uncached reads: ~1-2 s */
            detener_por_gif_trabado("GIF DMA not finished after ~20M reads");
        }
    }
#ifdef SMK64_MEDIDOR
    if (spins != 0) {
        medidor_sumar_espera_dma(medidor_leer_ciclos() - start);
    }
#endif
}

static void send_dma(Qword *start, u32 qwc)
{
    while (qwc > 0) {
        u32 n = qwc > 0xFFF0 ? 0xFFF0 : qwc;

        wait_gif();
        dmaKit_send(DMA_CHANNEL_GIF, start, n);
#ifdef SMK64_MEDIDOR
        medidor_contar_envio_dma();
#endif
        start += n;
        qwc -= n;
    }
}

/* Sends the packet in progress */
static void kick(int wait)
{
    u32 qwc = cur - s_pkt;

    EMPEZAR_PROF(ENVIO_PROF);

    if (qwc == 0) {
        return;
    }
    gs_stats.packet_bytes += qwc * 16;
    FlushCache(0);
    send_dma(s_pkt, qwc);
    if (wait) {
        wait_gif();
        cur = s_pkt;
    }
    FIN_PROF(ENVIO_PROF);
}

static u64 frame_reg(int buffer)
{
    if (hires) {
        return GS_SETREG_FRAME(band_base, GS_WIDTH / 64, s_gs->PSM, 0); /* single front buffer */
    }
    return GS_SETREG_FRAME(s_gs->ScreenBuffer[buffer] / 8192, GS_WIDTH / 64, s_gs->PSM, 0);
}

u64 gs_zbuf_value(int write_enable)
{
    return GS_SETREG_ZBUF(s_gs->ZBuffer / 8192, s_gs->PSMZ & 0xF, write_enable ? 0 : 1);
}

#ifdef SMK64_MEDIDOR
/* The meter panel sits right after the Z-buffer */
u32 gs_vram_meter(void)
{
    return (s_gs->CurrentPointer + 8191) & ~8191u;
}

u32 gs_vram_texture_start(void)
{
    return gs_vram_meter() + MEDIDOR_VRAM_BYTES;
}
#else
u32 gs_vram_texture_start(void)
{
    return (s_gs->CurrentPointer + 8191) & ~8191u;
}
#endif

#ifdef SMK64_DEV
/* Hires: one strip of CT32 for gs_hires_capture at the end of VRAM */
static u32 vram_capture_strip(void)
{
    return 4 * 1024 * 1024 - (u32) (GS_WIDTH / 64) * 8192;
}
#endif

/* The giant screen slots go right after the textures */
u32 gs_vram_texture_end(void)
{
#ifdef SMK64_DEV
    if (hires) {
        return vram_capture_strip() - BYTES_VRAM_PANTALLAS_GIGANTES;
    }
#endif
    return 4 * 1024 * 1024 - BYTES_VRAM_PANTALLAS_GIGANTES;
}

u32 gs_vram_on_screen(void)
{
    return s_gs->ScreenBuffer[display_buffer];
}

static void set_display(int buffer)
{
    u64 v = ((u64) (s_gs->ScreenBuffer[buffer] / 8192)) | ((u64) (GS_WIDTH / 64) << 9) | ((u64) s_gs->PSM << 15);

    *(volatile u64 *) 0x12000070 = v;
    *(volatile u64 *) 0x12000090 = v;
}

/* VRAM keeps the loader's last image (OPL, browser) until the first frame. */
static void clear_screens(void)
{
    static Qword pkt[20] __attribute__((aligned(64)));
    Qword *q = pkt;
    int b;

    q->d[0] = giftag_lo(15, 1, 0, 0, GIF_FLG_PACKED, 1);
    q->d[1] = GIF_REG_AD;
    q++;
#define AD_INI(r, v) (q->d[0] = (v), q->d[1] = (r), q++)
    AD_INI(GSR_ZBUF_1, gs_zbuf_value(0));
    AD_INI(GSR_TEST_1, GS_SETREG_TEST(0, 0, 0, 0, 0, 0, 1, 1));
    AD_INI(GSR_XYOFFSET_1, GS_SETREG_XYOFFSET(XYOFFSET_X << 4, XYOFFSET_Y << 4));
    AD_INI(GSR_SCISSOR_1, GS_SETREG_SCISSOR(0, GS_WIDTH - 1, 0, GS_HEIGHT - 1));
    AD_INI(GSR_DTHE, 0);
    for (b = 0; b < (hires ? 1 : 2); b++) {
        AD_INI(GSR_FRAME_1, frame_reg(b));
        AD_INI(GSR_PRIM, 6);
        AD_INI(GSR_RGBAQ, 0x80000000ULL);
        AD_INI(GSR_XYZ2, (u64) (XYOFFSET_X << 4) | ((u64) (XYOFFSET_Y << 4) << 16));
        AD_INI(GSR_XYZ2, (u64) ((XYOFFSET_X + GS_WIDTH) << 4) | ((u64) ((XYOFFSET_Y + GS_HEIGHT) << 4) << 16));
    }
#undef AD_INI
    FlushCache(0);
    wait_gif();
    dmaKit_send(DMA_CHANNEL_GIF, pkt, (u32) (q - pkt));
    wait_gif();
}

void gs_init(void)
{
    ee_sema_t sema;

    gs_width = video_mode->width;
    gs_height = video_mode->height;
    gs_scale_x = gs_width / 320.0f;
    gs_scale_y = gs_height / 240.0f;
    for (screen_tex_th = 0; (1 << screen_tex_th) < gs_height; screen_tex_th++) {
    }
    hires = video_mode->hires;
    z_shift = hires ? 8 : 0;

    s_gs = gsKit_init_global();
    s_gs->Mode = video_mode->mode;
    s_gs->Interlace = video_mode->interlace;
    s_gs->Field = video_mode->field;
    s_gs->Width = GS_WIDTH;
    s_gs->Height = GS_HEIGHT;
    s_gs->PSM = GS_PSM_CT16S;
    /* Hires: the Z-buffer lives in the front buffer's bands, so it has the same layout */
    s_gs->PSMZ = hires ? GS_PSMZ_16S : GS_PSMZ_24;
    s_gs->ZBuffering = hires ? GS_SETTING_OFF : GS_SETTING_ON;
    s_gs->DoubleBuffering = hires ? GS_SETTING_OFF : GS_SETTING_ON;
    s_gs->PrimAlphaEnable = GS_SETTING_ON;
    s_gs->Dithering = GS_SETTING_ON;

    dmaKit_init(D_CTRL_RELE_OFF, D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC, D_CTRL_STD_OFF, D_CTRL_RCYC_8,
                1 << DMA_CHANNEL_GIF);
    dmaKit_chan_init(DMA_CHANNEL_GIF);
    gsKit_init_screen(s_gs);
    gsKit_mode_switch(s_gs, GS_ONESHOT);
    gsKit_queue_exec(s_gs);
    if (hires) {
        hires_init();
    }

    memset(&sema, 0, sizeof(sema));
    sema.init_count = 0;
    sema.max_count = 1;
    flip_sema = CreateSema(&sema);

    display_buffer = 0;
    draw_buffer = 1;
    pending_flip = -1;
    set_display(0);
    clear_screens();

    registrar("GS: %s %dx%d fb %x/%x z %x textures %x-%x", video_mode->name, GS_WIDTH, GS_HEIGHT,
            (unsigned) s_gs->ScreenBuffer[0], (unsigned) s_gs->ScreenBuffer[1], (unsigned) s_gs->ZBuffer, (unsigned) gs_vram_texture_start(), (unsigned) gs_vram_texture_end());
    if (hires) {
        registrar("GS: %d bands of %d lines, passes at hsync %u/%u/%u", HIRES_BANDS, band_h, (unsigned) band_trigger[0],
                (unsigned) band_trigger[1], (unsigned) band_trigger[2]);
    }
}

#define PRESENT_QUEUE 4

typedef struct {
    Qword *start;
    u32 qwc;
    int buffer;
    int slot;
} QueuedFrame;

static volatile QueuedFrame queue[PRESENT_QUEUE];
static volatile int queue_head, queue_count;
static volatile int gs_slot = -1;

#define CSR ((volatile u64 *) 0x12001000)

static void present_start(Qword *start, u32 qwc, int buffer, int slot)
{
    *CSR = 2; /* clears an earlier FINISH */
    *(volatile u32 *) 0x1000A020 = qwc;
    *(volatile u32 *) 0x1000A010 = (u32) start & 0x0FFFFFFF;
    *(volatile u32 *) 0x1000A000 = 0x101;
    pending_flip = buffer;
    gs_slot = slot;
}

/* Rewrites a packet's band fixups for one band. Runs in the GS interrupt (or with the passes
   paused) on a packet the CPU no longer touches: writes go through the uncached mirror, so the
   DMA sees them without a cache flush. */
static void patch_band(int slot, int band)
{
    Qword *base = (Qword *) ((u32) packets[slot] | 0x20000000);
    const BandFixup *f = band_fixups[slot];
    int y0 = band * band_h, y1 = y0 + band_h - 1, i;
    u64 fbp = band_base + band * band_pages;
    u64 zbp = band_base + ((band + 1) % HIRES_BANDS) * band_pages;
    u64 ofy = (u64) (y0 * 16 + (render_field == 0 ? band_field_offset : 0));

    if (y1 > GS_HEIGHT - 1) {
        y1 = GS_HEIGHT - 1;
    }
    for (i = band_num_fixups[slot]; i > 0; i--, f++) {
        u64 v = f->value;

        if (f->reg >= BAND_ONLY_SCISSOR) {
            if (f->reg - BAND_ONLY_SCISSOR != (u32) band) {
                v = GS_SETREG_SCISSOR(1, 0, 1, 0);
            }
        }
        switch (f->reg) {
            case GSR_FRAME_1:
                v = (v & ~0x1FFULL) | (((v & 0x1FF) == band_base) ? fbp : zbp);
                break;
            case GSR_ZBUF_1:
                v = (v & ~0x1FFULL) | zbp;
                break;
            case GSR_XYOFFSET_1:
                v += ofy << 32; /* screen line y0 becomes line 0 of the band */
                break;
            case GSR_SCISSOR_1: {
                int sy0 = (int) ((v >> 32) & 0x7FF) - y0, sy1 = (int) ((v >> 48) & 0x7FF) - y0;

                sy0 = sy0 < 0 ? 0 : sy0;
                sy1 = sy1 > y1 - y0 ? y1 - y0 : sy1;
                if (sy0 > sy1) {
                    v = GS_SETREG_SCISSOR(1, 0, 1, 0); /* no part of the viewport in this band */
                } else {
                    v = (v & 0xFFFFFFFFULL) | ((u64) sy0 << 32) | ((u64) sy1 << 48);
                }
                break;
            }
        }
        base[f->off].d[0] = v;
    }
}

static void band_kick(int slot)
{
    __asm__ __volatile__("sync.l" ::: "memory"); /* the fixups were written uncached: let them land first */
    *(volatile u32 *) 0x1000A020 = (u32) (band_packet_end[slot] - packets[slot]);
    *(volatile u32 *) 0x1000A010 = (u32) packets[slot] & 0x0FFFFFFF;
    *(volatile u32 *) 0x1000A000 = 0x101;
}

static u32 crtc_line(void);

static void band_start(int band)
{
    if (en_panico_ps2 || hires_paused) {
        return;
    }
    /* Switch on the first pass after band 0's turn, even if band 0 itself was late: if the switch
       waited for an on-time band 0 pass, a GS that keeps falling behind would freeze the picture */
    if (switch_due) {
        switch_due = 0;
        if (hires_next >= 0) {
            if (hires_cur >= 0) {
                slot_busy &= ~(1u << hires_cur);
            }
            hires_cur = hires_next;
            hires_next = -1;
            gs_stats.flips++;
            iSignalSema(flip_sema);
        }
    }
    if (hires_cur < 0) {
        return;
    }
    patch_band(hires_cur, band);
    band_kick(hires_cur);
    band_start_line[band] = crtc_line();
    gs_stats.band_passes++;
}

static void band_pass(int band)
{
    if (band == 0) {
        switch_due = 1;
        /* The CRTC is showing the last band of the other field */
        render_field = ((*CSR >> 13) & 1) ? 0 : 1;
    }
    if (*D2_CHCR & 0x100) {
        /* The GS fell behind the CRTC: start this band as soon as the previous pass is sent
           (a band still waiting is skipped and shows the previous refresh) */
        gs_stats.band_overruns++;
        band_waiting = band;
        return;
    }
    band_waiting = -1;
    band_start(band);
}

static inline u32 read_cycles(void)
{
    u32 c;

    __asm__ __volatile__("mfc0 %0, $9" : "=r"(c));
    return c;
}

/* Current CRTC line (per field in 1080i) */
static u32 crtc_line(void)
{
    u32 elapsed = read_cycles() - refresh_start;

    /* A multiply: this runs on every HSINT (~34k/s in 1080i), where a 64-bit divide is a libgcc call */
    return (u32) (((u64) elapsed * line_mul) >> 32);
}

/* Starts every band whose line has passed and that has not started this refresh */
static void band_poll(void)
{
    u32 line = crtc_line();
    int band;

    hires_last_lines = line;
    /* In trigger order: bands 1 and 2 during the display, band 0 while the CRTC shows band 2 */
    for (band = 1; band <= HIRES_BANDS; band++) {
        int b = band % HIRES_BANDS;

        if (!(bands_fired & (1u << b)) && line >= band_trigger[b]) {
            bands_fired |= 1u << b;
            band_pass(b);
        }
    }
}

static int band_gs_interrupt(int cause)
{
    u64 csr = *CSR;
    u32 events = (u32) csr & 0x1F; /* SIGNAL, FINISH, HSINT, VSINT, EDWINT */
    int band;

    (void) cause;
    /* Clear every pending event, not just HSINT: one left set keeps the GS interrupt line high, the
       INTC only sees rising edges, and HSINT never arrives again (bands 1 and 2 stopped for good) */
    if (events != 0) {
        *CSR = events;
    }
    if (events & 4) { /* HSINT: just a regular tick to check the line and the DMA */
        hsint_count++;
        if (band_waiting >= 0 && !(*D2_CHCR & 0x100)) {
            band = band_waiting;
            band_waiting = -1;
            band_start(band);
        }
        band_poll();
    }
    ExitHandler();
    return 0;
}

static void hires_init(void)
{
    u32 fb_bytes = gsKit_texture_size(GS_WIDTH, GS_HEIGHT, s_gs->PSM);
    u32 y_start;
    int band, i;

    band_h = ((GS_HEIGHT + HIRES_BANDS - 1) / HIRES_BANDS + 63) & ~63;
    band_pages = (u32) (band_h / 64) * (GS_WIDTH / 64);
    band_base = s_gs->ScreenBuffer[0] / 8192;
    /* gsKit allocated one W x H buffer: pad it to whole bands */
    if (band_pages * 8192 * HIRES_BANDS > fb_bytes) {
        gsKit_vram_alloc(s_gs, band_pages * 8192 * HIRES_BANDS - fb_bytes, GSKIT_ALLOC_SYSBUFFER);
    }
    s_gs->ZBuffer = 0; /* every ZBUF write is a band fixup */
    band_field_offset = (s_gs->Interlace == GS_INTERLACED && s_gs->Field == GS_FRAME) ? 8 : 0;

    /* Each band's pass starts when the CRTC reaches the band before it (same timing as gsHires) */
    y_start = s_gs->StartY + s_gs->StartYOffset + 4;
    if (s_gs->Interlace == GS_INTERLACED) {
        y_start /= 2;
    }
    for (band = 0; band < HIRES_BANDS; band++) {
        u32 scan = (u32) ((band + HIRES_BANDS - 1) % HIRES_BANDS) * band_h * (s_gs->MagV + 1);

        /* As in gsHires: halved only in FIELD mode. Halving it in 1080i too (FRAME, MagV 1) looks
           right on paper but put the passes out of step with the CRTC on real hardware. */
        if (s_gs->Interlace == GS_INTERLACED && s_gs->Field == GS_FIELD) {
            scan /= 2;
        }
        band_trigger[band] = y_start + scan;
    }
    band_nominal_lines = (s_gs->Mode == GS_MODE_DTV_1080I) ? 562 : 750;
    refresh_cycles = 294912000u / 60; /* refined from the measured VBLANK period */
    line_mul = (u32) (((u64) band_nominal_lines << 32) / refresh_cycles);
    refresh_start = read_cycles();

    for (i = 0; i < PACKET_SLOTS; i++) {
        band_fixups[i] = memalign(64, sizeof(BandFixup) * MAX_BAND_FIXUPS);
        if (band_fixups[i] == NULL) {
            detener_por_error("GS: no memory for the hires band fixups");
        }
    }

    /* gsKit_init_screen masked every GS interrupt: unmask HSINT */
    DI();
    AddIntcHandler(INTC_GS, band_gs_interrupt, 0);
    EnableIntc(INTC_GS);
    GsPutIMR(GsGetIMR() & ~0x0400);
    EI();
}

/* Called from the VBLANK interrupt (retrazo_vertical.c). */
void gs_on_vblank(void)
{
    static u32 since_flip;

    if (hires) {
        /* Flips happen in band_pass. The hsync count restarts here too: the GS VSINT bit is
           polled in band_gs_interrupt, but not every emulator sets it with VSINT masked. */
        u32 now = read_cycles();

        hires_vblanks++;
        /* Band 0 must start every refresh or no new frame is ever shown: if its line was not
           seen (passes too slow for the HSINT ticks), start it now, late rather than never */
        if (!(bands_fired & 1u)) {
            band_pass(0);
        }
        /* Refresh period, ignoring VBLANKs that are far off (the first, or after a long pause) */
        {
            u32 period = now - refresh_start;

            if (refresh_cycles == 0 || (period > refresh_cycles / 2 && period < refresh_cycles * 2)) {
                refresh_cycles = refresh_cycles == 0 ? period : (refresh_cycles * 7 + period) / 8;
                line_mul = (u32) (((u64) band_nominal_lines << 32) / refresh_cycles);
            }
        }
        refresh_start = now;
        bands_fired = 0;
        /* A whole refresh without HSINT: clear the GS events (one stuck set blocks the interrupt
           line) and make sure the GS interrupt is enabled, so bands 1 and 2 start again */
        if (hsint_count == 0 && hires_vblanks > 2) {
            *CSR = 0x1F;
            iEnableIntc(INTC_GS);
            hsint_rearms++;
        }
        hsint_last = hsint_count;
        hsint_count = 0;
        iSignalSema(flip_sema); /* lets gs_wait_flip notice a flip that never comes */
        return;
    }
    if (en_panico_ps2) {
        return; /* the crash screen is what is showing: do not change DISPFB */
    }
    since_flip++;
    if (pending_flip >= 0 && (*CSR & 2)) {
        *CSR = 2;
        gs_stats.flips++;
        /* How long the image being replaced was on screen. */
        gs_stats.held[since_flip >= 4 ? 3 : since_flip - 1]++;
        since_flip = 0;
        set_display(pending_flip);
        display_buffer = pending_flip;
        pending_flip = -1;
        if (gs_slot >= 0) {
            slot_busy &= ~(1u << gs_slot);
            gs_slot = -1;
        }
        if (queue_count > 0) {
            volatile QueuedFrame *p = &queue[queue_head];

            queue_head = (queue_head + 1) % PRESENT_QUEUE;
            queue_count--;
            present_start(p->start, p->qwc, p->buffer, p->slot);
        }
        iSignalSema(flip_sema);
    }
}

/* Waits until everything queued is on screen. */
/* For the crash screen (also called from its stuck-GIF handler, before the GIF is reset): hires
   status, and the 16 qwords before MADR. When the GS stops taking data it is on one of those: the
   GIF FIFO holds up to 16 qwords past it. */
void gs_log_gif_stall(void)
{
    u32 madr = *(volatile u32 *) 0x1000A010 & 0x0FFFFFFF;
    int k;

    if (hires) {
        registrar("GS hires: line %u passes %u late %u flips %u cut %u", (unsigned) hires_last_lines,
                (unsigned) gs_stats.band_passes, (unsigned) gs_stats.band_overruns, (unsigned) gs_stats.flips,
                (unsigned) gs_stats.dropped);
        registrar("waiting %d cur %d next %d fixups %d pkt %x-%x upl %uK", band_waiting, hires_cur, hires_next,
                hires_cur >= 0 ? band_num_fixups[hires_cur] : -1,
                hires_cur >= 0 ? (unsigned) ((u32) packets[hires_cur] & 0x0FFFFFFF) : 0u,
                hires_cur >= 0 ? (unsigned) ((u32) band_packet_end[hires_cur] & 0x0FFFFFFF) : 0u,
                (unsigned) (upload_bytes_frame / 1024));
    }
    registrar("chcr %x qwc %x madr %x tadr %x csr %x", (unsigned) *D2_CHCR, (unsigned) *(volatile u32 *) 0x1000A020,
            (unsigned) madr, (unsigned) *(volatile u32 *) 0x1000A030, (unsigned) *CSR);
    if (madr >= 16 * 16 && madr < 32 * 1024 * 1024) {
        const volatile u64 *q = (const volatile u64 *) ((madr - 16 * 16) | 0x20000000);

        /* Each qword as its high then low 64 bits, oldest first */
        for (k = 0; k < 16; k += 2, q += 4) {
            registrar("-%02d %08x%08x%08x%08x %08x%08x%08x%08x", 16 - k, (unsigned) (q[1] >> 32), (unsigned) q[1],
                    (unsigned) (q[0] >> 32), (unsigned) q[0], (unsigned) (q[3] >> 32), (unsigned) q[3],
                    (unsigned) (q[2] >> 32), (unsigned) q[2]);
        }
    }
}

void gs_wait_flip(void)
{
    u32 since = hires_vblanks;
    int logged = 0;

    while (pending_flip >= 0 || queue_count > 0 || hires_next >= 0) {
        WaitSema(flip_sema);
        if (hires && !logged && hires_vblanks - since > 120) {
            logged = 1;
            gs_log_gif_stall();
        }
    }
}

/* Waits for a packet buffer to become free. */
static void wait_slot(int slot)
{
#ifdef SMK64_MEDIDOR
    if (slot_busy & (1u << slot)) {
        u32 start = medidor_leer_ciclos();

        while (slot_busy & (1u << slot)) {
            WaitSema(flip_sema);
        }
        medidor_sumar_espera_gs(medidor_leer_ciclos() - start);
    }
#else
    while (slot_busy & (1u << slot)) {
        WaitSema(flip_sema);
    }
#endif
}

int gs_displayed_buffer(void)
{
    int result;
    int intr = DI();

    if (queue_count > 0) {
        result = queue[(queue_head + queue_count - 1) % PRESENT_QUEUE].buffer;
    } else if (pending_flip >= 0) {
        result = pending_flip;
    } else {
        result = display_buffer;
    }
    if (intr) {
        EI();
    }
    return result;
}

/* Nothing more fits in the packet */
static void packet_full(void)
{
    PacketInfo *pi = &packet_info[cur_packet];

    flush_prims();
    if (hires) {
        if (sinking) {
            cur = s_pkt; /* the sink is never sent: start it over */
        } else {
            hires_sink();
        }
        return;
    }
    if (pi->flags & GS_PACKET_NO_PARTIAL) {
        pi->overflow = 1;
        cur = s_pkt + 16;
        return;
    }
    if (overflow_hook != NULL) {
        void (*hook)(void) = overflow_hook;

        overflow_hook = NULL;
        hook();
    }
    gs_wait_flip();
    pi->partial_sent = 1;
    kick(1);
    /* What was sent can no longer be changed, and its buffer gets reused */
    pi->num_fixups = 0;
}

static void note_frame_fixup(Qword *q)
{
    PacketInfo *pi = &packet_info[cur_packet];

    if (pi->num_fixups < MAX_FRAME_FIXUPS) {
        pi->frame_fixups[pi->num_fixups++] = q;
    }
}

void gs_set_overflow_hook(void (*hook)(void))
{
    overflow_hook = hook;
}

void gs_begin_packet(int packet, int buffer, int flags)
{
    PacketInfo *pi = &packet_info[packet];
    Qword *tag;

    if (hires) {
        int s;

        /* Any slot that is neither on screen nor waiting to be: with a dropped frame nothing
           would free the slot a strict alternation waits for */
        for (s = 0; s < PACKET_SLOTS && (slot_busy & (1u << s)); s++) {
        }
        packet_slot[packet] = s < PACKET_SLOTS ? s : 0;
        flags |= GS_PACKET_NO_PARTIAL; /* the whole packet is replayed for every band */
    } else if (packet == GS_PACKET_REAL) {
        packet_slot[packet] = (packet_slot[packet] == 0) ? 2 : 0;
    }
    wait_slot(packet_slot[packet]);
    cur_packet = packet;
    build_slot = packet_slot[packet];
    band_num_fixups[build_slot] = 0;
    band_fixup_overflow[build_slot] = 0;
    upload_bytes_frame = 0;
    sinking = 0;
    s_pkt = cur = packets[packet_slot[packet]];
    prim_tag = NULL;
    state_valid = 0;
    draw_buffer = buffer;
    memset(pi, 0, sizeof(*pi));
    pi->buffer = buffer;
    pi->flags = flags;
    if (!(flags & GS_PACKET_KEEP_STATS)) {
        gs_stats.triangles = gs_stats.sprites = gs_stats.uploads = gs_stats.upload_bytes = 0;
        gs_stats.packet_bytes = 0;
    }

    begin_ad(&tag, 9);
    note_frame_fixup(cur);
    ad_band(GSR_FRAME_1, frame_reg(draw_buffer));
    ad_band(GSR_ZBUF_1, gs_zbuf_value(1));
    ad_band(GSR_XYOFFSET_1, GS_SETREG_XYOFFSET(XYOFFSET_X << 4, XYOFFSET_Y << 4));
    ad_band(GSR_SCISSOR_1, GS_SETREG_SCISSOR(0, GS_WIDTH - 1, 0, GS_HEIGHT - 1));
    ad(GSR_PRMODECONT, 1);
    ad(GSR_COLCLAMP, 1);
    ad(GSR_DTHE, 0);
    ad(GSR_TEXA, GS_SETREG_TEXA(0, 0, 0x80));
    ad(GSR_FBA_1, 0);
    if (hires) {
        /* The band held the previous pass's Z-buffer: clear colour and Z before drawing */
        begin_ad(&tag, 5);
        ad(GSR_TEST_1, GS_SETREG_TEST(0, 0, 0, 0, 0, 0, 1, 1));
        ad(GSR_PRIM, 6);
        ad(GSR_RGBAQ, 0x80000000ULL);
        ad(GSR_XYZ2, (u64) (XYOFFSET_X << 4) | ((u64) (XYOFFSET_Y << 4) << 16));
        ad(GSR_XYZ2, (u64) ((XYOFFSET_X + GS_WIDTH) << 4) | ((u64) ((XYOFFSET_Y + GS_HEIGHT) << 4) << 16));
    }
}

void gs_finish_packet(int black)
{
    Qword *tag;

    if (black) {
        gs_fill_rect(0, 0, GS_WIDTH, GS_HEIGHT, 0, 0, 0);
    }
    begin_ad(&tag, 1);
    ad(GSR_FINISH, 0);
    tag->d[0] |= 1ULL << 15;
    flush_prims();
    if (!sinking) {
        packet_info[cur_packet].end = cur; /* otherwise hires_sink set where it was cut */
    }
}

int gs_packet_failed(int packet)
{
    return packet_info[packet].overflow;
}

int gs_packet_partially_sent(int packet)
{
    return packet_info[packet].partial_sent;
}

void gs_retarget_packet(int packet, int buffer)
{
    PacketInfo *pi = &packet_info[packet];
    int i;

    if (hires) {
        return; /* single front buffer: FRAME is set per band */
    }
    for (i = 0; i < pi->num_fixups; i++) {
        pi->frame_fixups[i]->d[0] = frame_reg(buffer);
    }
    pi->buffer = buffer;
    if (packet == cur_packet) {
        draw_buffer = buffer;
    }
}

/* Hires: a frame is replayed every pass, so one malformed GIF tag or misplaced band fixup would
   leave the GS waiting for data forever and freeze the picture with no clue why. Walking the tags
   costs little next to building the frame: stop with what is wrong instead. */
static void hires_check_packet(int slot, const Qword *end)
{
    const Qword *start = packets[slot], *q = start;
    int i;

    while (q < end) {
        u64 t = q->d[0];
        u32 nloop = (u32) (t & 0x7FFF), flg = (u32) (t >> 58) & 3, nreg = (u32) (t >> 60) & 0xF, n;

        nreg = nreg == 0 ? 16 : nreg;
        n = flg == GIF_FLG_PACKED ? nloop * nreg : flg == 1 ? (nloop * nreg + 1) / 2 : nloop;
        if (q + 1 + n > end) {
            registrar("GS: bad hires packet: tag %08x%08x at %u runs %u qwords past the end (%u)",
                    (unsigned) (t >> 32), (unsigned) t, (unsigned) (q - start), (unsigned) (q + 1 + n - end),
                    (unsigned) (end - start));
            detener_por_error("GS: malformed hires packet (GIF tag)");
        }
        q += 1 + n;
    }
    for (i = 0; i < band_num_fixups[slot]; i++) {
        const BandFixup *f = &band_fixups[slot][i];
        u32 reg = f->reg >= BAND_ONLY_SCISSOR ? GSR_SCISSOR_1 : f->reg;

        if (f->off >= (u32) (end - start) || start[f->off].d[1] != reg || start[f->off].d[0] != f->value) {
            registrar("GS: bad band fixup %d/%d: off %u reg %x, packet has reg %x (%u qwords)", i,
                    band_num_fixups[slot], (unsigned) f->off, (unsigned) f->reg,
                    f->off < (u32) (end - start) ? (unsigned) start[f->off].d[1] : 0u, (unsigned) (end - start));
            detener_por_error("GS: malformed hires packet (band fixup)");
        }
    }
}

void gs_present_packet(int packet)
{
    PacketInfo *pi = &packet_info[packet];
    int slot = packet_slot[packet];
    Qword *start = packets[slot];
    u32 qwc = pi->end - start;
    int intr;

    gs_stats.packet_bytes += qwc * 16;
    gs_stats.frames++;
    if (hires) {
        if (band_num_fixups[slot] > band_fixups_max) {
            band_fixups_max = band_num_fixups[slot];
        }
        if (qwc > band_qwc_max) {
            band_qwc_max = qwc;
        }
        band_packet_end[slot] = pi->end;
        hires_check_packet(slot, pi->end);
        /* A frame's uploads are only in its own packet, so a frame that is never shown would leave
           textures the cache thinks are in VRAM: wait (at most a refresh) for the pending one
           instead of replacing it */
#ifdef SMK64_MEDIDOR
        {
            u32 t0 = medidor_leer_ciclos();

            gs_wait_flip();
            medidor_sumar_espera_gs(medidor_leer_ciclos() - t0); /* "GS" on the meter */
        }
#else
        gs_wait_flip();
#endif
        FlushCache(0);
        intr = DI();
        slot_busy |= 1u << slot;
        hires_next = slot;
        if (intr) {
            EI();
        }
        return;
    }
    FlushCache(0);
    intr = DI();
    slot_busy |= 1u << slot;
    if (pending_flip < 0 && queue_count == 0) {
        present_start(start, qwc, pi->buffer, slot); /* the GS is idle */
    } else {
        volatile QueuedFrame *p = &queue[(queue_head + queue_count) % PRESENT_QUEUE];

        p->start = start;
        p->qwc = qwc;
        p->buffer = pi->buffer;
        p->slot = slot;
        queue_count++;
    }
    if (intr) {
        EI();
    }
#ifdef SMK64_MEDIDOR
    medidor_contar_envio_dma();
#endif
}

/* Normal frame (a single packet) */
void gs_begin_frame(void)
{
    gs_wait_flip();
    gs_begin_packet(GS_PACKET_REAL, display_buffer ^ 1, 0);
}

void gs_end_frame(int black)
{
    gs_finish_packet(black);
    gs_present_packet(GS_PACKET_REAL);
}

static u32 capture_len;
static int capturing, capture_overflow;

void gs_capture_uploads(int on)
{
    if (on) {
        capture_len = 0;
        capture_overflow = 0;
    }
    capturing = on;
}

int gs_capture_failed(void)
{
    return capture_overflow;
}

static void capture_append(const Qword *from, u32 qwc)
{
    if (!capturing) {
        return;
    }
    if (capture_len + qwc > CAPTURE_QWORDS) {
        capture_overflow = 1;
        return;
    }
    memcpy(&capture_buf[capture_len], from, qwc * 16);
    capture_len += qwc;
}

void gs_emit_captured_uploads(void)
{
    u32 done = 0;

    flush_prims();
    while (done < capture_len) {
        u32 n = capture_len - done;
        u32 room = PACKET_QWORDS - (u32) (cur - s_pkt) - 16;

        if (n > room) {
            n = room;
        }
        memcpy(cur, &capture_buf[done], n * 16);
        cur += n;
        done += n;
        if (done < capture_len) {
            reserve(PACKET_QWORDS); /* does not fit: the packet gets marked as failed */
        }
    }
    state_valid = 0;
}

void gs_apply_state(const GsState *st)
{
    Qword *tag;
    u32 n = 0;
    int zs;

    if (state_valid && st->test == state.test && st->alpha == state.alpha && st->zbuf == state.zbuf &&
        st->tex0 == state.tex0 && st->tex1 == state.tex1 && st->clamp == state.clamp &&
        st->scissor == state.scissor && st->fogcol == state.fogcol && st->dither == state.dither) {
        if (st->prim != state.prim || st->textured != state.textured) {
            flush_prims();
            state.prim = st->prim;
            state.textured = st->textured;
        }
        return;
    }
    /* Hires: ZBUF and SCISSOR are band fixups, rewritten for every band; they rarely change, so
       they are only written when they do */
    zs = !hires || !state_valid || st->zbuf != state.zbuf || st->scissor != state.scissor;
    if (zs) {
        band_room(2);
    }
    begin_ad(&tag, 9);
    ad(GSR_TEST_1, st->test);
    ad(GSR_ALPHA_1, st->alpha);
    n = 2;
    if (zs) {
        ad_band(GSR_ZBUF_1, st->zbuf);
        ad_band(GSR_SCISSOR_1, st->scissor);
        n += 2;
    }
    ad(GSR_FOGCOL, st->fogcol);
    ad(GSR_DTHE, st->dither ? 1 : 0);
    n += 2;
    if (st->textured) {
        ad(GSR_TEX0_1, st->tex0);
        ad(GSR_TEX1_1, st->tex1);
        ad(GSR_CLAMP_1, st->clamp);
        n += 3;
    }
    tag->d[0] = giftag_lo(n, 0, 0, 0, GIF_FLG_PACKED, 1);
    cur = tag + 1 + n;
    state = *st;
    state_valid = 1;
}

#define PRIM_TRIANGLE 3
#define PRIM_SPRITE   6

static inline void open_prims(u32 type)
{
    u32 prim = type | state.prim;
    u32 nreg = state.textured ? 3 : 2;

    if (prim_tag != NULL && ((prim_tag->d[0] >> 47) & 0x7FF) == prim && prim_nloop < 0x7000) {
        return;
    }
    flush_prims();
    prim_tag = cur++;
    prim_tag->d[0] = giftag_lo(0, 0, 1, prim, GIF_FLG_PACKED, nreg);
    prim_tag->d[1] = state.textured ? (0x2ULL | (0x1ULL << 4) | (0x4ULL << 8)) : (0x1ULL | (0x4ULL << 4));
    prim_nloop = 0;
    prim_regs = nreg;
}

static inline void emit_vertex(const GsVertex *v)
{
    /* Signed */
    u32 x = (u32) (s32) ((v->x + XYOFFSET_X) * 16.0f);
    u32 y = (u32) (s32) ((v->y + XYOFFSET_Y) * 16.0f);

    if (prim_regs == 3) {
        cur->f[0] = v->s;
        cur->f[1] = v->t;
        cur->f[2] = v->q;
        cur->w[3] = 0;
        cur++;
    }
    cur->w[0] = v->r;
    cur->w[1] = v->g;
    cur->w[2] = v->b;
    cur->w[3] = v->a;
    cur++;
    cur->d[0] = (u64) (x & 0xFFFF) | ((u64) (y & 0xFFFF) << 32);
    cur->d[1] = ((u64) ((v->z >> z_shift) & 0xFFFFFF) << 4) | ((u64) v->fog << 36);
    cur++;
    prim_nloop++;
}

/* The three qwords of emit_vertex, pre-built (ST, RGBAQ, XYZF2) */
void gs_pack_vertex(const GsVertex *v, u128 out[3])
{
    Qword *q = (Qword *) out;
    u32 x = (u32) (s32) ((v->x + XYOFFSET_X) * 16.0f);
    u32 y = (u32) (s32) ((v->y + XYOFFSET_Y) * 16.0f);

    q[0].f[0] = v->s;
    q[0].f[1] = v->t;
    q[0].f[2] = v->q;
    q[0].w[3] = 0;
    q[1].w[0] = v->r;
    q[1].w[1] = v->g;
    q[1].w[2] = v->b;
    q[1].w[3] = v->a;
    q[2].d[0] = (u64) (x & 0xFFFF) | ((u64) (y & 0xFFFF) << 32);
    q[2].d[1] = ((u64) ((v->z >> z_shift) & 0xFFFFFF) << 4) | ((u64) v->fog << 36);
}

void gs_triangle_packed(const u128 *a, const u128 *b, const u128 *c)
{
    reserve(12);
    open_prims(PRIM_TRIANGLE);
    if (prim_regs == 3) {
        cur[0].q = a[0];
        cur[1].q = a[1];
        cur[2].q = a[2];
        cur[3].q = b[0];
        cur[4].q = b[1];
        cur[5].q = b[2];
        cur[6].q = c[0];
        cur[7].q = c[1];
        cur[8].q = c[2];
        cur += 9;
    } else {
        cur[0].q = a[1];
        cur[1].q = a[2];
        cur[2].q = b[1];
        cur[3].q = b[2];
        cur[4].q = c[1];
        cur[5].q = c[2];
        cur += 6;
    }
    prim_nloop += 3;
    gs_stats.triangles++;
}

void gs_triangle(const GsVertex *v0, const GsVertex *v1, const GsVertex *v2)
{
    reserve(12);
    open_prims(PRIM_TRIANGLE);
    emit_vertex(v0);
    emit_vertex(v1);
    emit_vertex(v2);
    gs_stats.triangles++;
}

/* Clips one axis of a sprite to lo..hi, moving the texture coordinate with it. 0: nothing left. */
static int clip_sprite_axis(float *a, float *b, float *ta, float *tb, float lo, float hi)
{
    float a0 = *a, b0 = *b, ta0 = *ta, tb0 = *tb;
    int flip = a0 > b0;

    if (flip) {
        a0 = *b, b0 = *a, ta0 = *tb, tb0 = *ta;
    }
    if (b0 <= lo || a0 >= hi) {
        return 0;
    }
    if (a0 < lo) {
        *(flip ? tb : ta) = ta0 + (tb0 - ta0) * (lo - a0) / (b0 - a0);
        *(flip ? b : a) = lo;
    }
    if (b0 > hi) {
        *(flip ? ta : tb) = ta0 + (tb0 - ta0) * (hi - a0) / (b0 - a0);
        *(flip ? a : b) = hi;
    }
    return 1;
}

void gs_sprite(const GsVertex *tl, const GsVertex *br)
{
    /* Rectangles can reach N64 x/y 1023: keep them where the GS can draw (window coordinates up to
       2047, primitive coordinates from 0; hires bands shift window y up by under GS_HEIGHT) */
    if (tl->x < -XYOFFSET_X || br->x < -XYOFFSET_X || tl->x > 2040.0f || br->x > 2040.0f ||
        tl->y < GS_HEIGHT - 2040.0f || br->y < GS_HEIGHT - 2040.0f || tl->y > 2040.0f || br->y > 2040.0f) {
        GsVertex a = *tl, b = *br;

        if (!clip_sprite_axis(&a.x, &b.x, &a.s, &b.s, (float) -XYOFFSET_X, 2040.0f) ||
            !clip_sprite_axis(&a.y, &b.y, &a.t, &b.t, GS_HEIGHT - 2040.0f, 2040.0f)) {
            return;
        }
        reserve(10);
        open_prims(PRIM_SPRITE);
        emit_vertex(&a);
        emit_vertex(&b);
        gs_stats.sprites++;
        return;
    }
    reserve(10);
    open_prims(PRIM_SPRITE);
    emit_vertex(tl);
    emit_vertex(br);
    gs_stats.sprites++;
}

void gs_fill_rect(int x0, int y0, int x1, int y1, u8 r, u8 g, u8 b)
{
    GsState st = state;
    GsState saved = state;
    int valid = state_valid;
    GsVertex a, c;

    st.test = GS_SETREG_TEST(0, 0, 0, 0, 0, 0, 1, 1);
    st.zbuf = gs_zbuf_value(0);
    st.alpha = GS_SETREG_ALPHA(0, 1, 0, 1, 0);
    st.scissor = GS_SETREG_SCISSOR(0, GS_WIDTH - 1, 0, GS_HEIGHT - 1);
    st.prim = 0;
    st.textured = 0;
    st.dither = 0;
    gs_apply_state(&st);
    memset(&a, 0, sizeof(a));
    a.x = (float) x0;
    a.y = (float) y0;
    a.r = r;
    a.g = g;
    a.b = b;
#ifdef SMK64_DEV_ALPHA0
    a.a = (b == 0xC0 && r == 0) ? 0 : 0x80;
#else
    a.a = 0x80;
#endif
    a.fog = 0xFF;
    c = a;
    c.x = (float) x1;
    c.y = (float) y1;
    gs_sprite(&a, &c);
    if (valid) {
        gs_apply_state(&saved);
    }
}

void gs_clear_z(void)
{
    Qword *tag;
    GsVertex a, c;

    band_room(4);
    begin_ad(&tag, 4);
    if (hires) {
        /* Z16S has the CT16S layout; any FBP other than band_base means "this band's Z-buffer" */
        ad_band(GSR_FRAME_1, GS_SETREG_FRAME(band_base + 1, GS_WIDTH / 64, GS_PSM_CT16S, 0));
    } else {
        ad(GSR_FRAME_1, GS_SETREG_FRAME(s_gs->ZBuffer / 8192, GS_WIDTH / 64, GS_PSM_CT32, 0));
    }
    ad(GSR_TEST_1, GS_SETREG_TEST(0, 0, 0, 0, 0, 0, 1, 1));
    ad_band(GSR_ZBUF_1, gs_zbuf_value(0));
    ad_band(GSR_SCISSOR_1, GS_SETREG_SCISSOR(0, GS_WIDTH - 1, 0, GS_HEIGHT - 1));
    state_valid = 0;
    memset(&a, 0, sizeof(a));
    a.fog = 0xFF;
    c = a;
    c.x = GS_WIDTH;
    c.y = GS_HEIGHT;
    state.prim = 0;
    state.textured = 0;
    gs_sprite(&a, &c);
    begin_ad(&tag, 1);
    note_frame_fixup(cur);
    ad_band(GSR_FRAME_1, frame_reg(draw_buffer));
}

/* First destination row whose source line (row centre) is at or below screen line `line` */
static int copy_row_at(int line, float y, float h, int dh)
{
    float r = ((float) line - y) * (float) dh / h - 0.5f;
    int ri = (int) r;

    ri += (float) ri < r ? 1 : 0; /* ceil */
    return ri < 0 ? 0 : ri > dh ? dh : ri;
}

/* Hires: there is never a complete frame in VRAM (each band is the next pass's Z-buffer), but
   band b is complete right after its own pass. Called at the end of the packet, which every pass
   replays: one copy per band, each enabled only in its own band's pass (BAND_ONLY_SCISSOR) and
   writing only the destination rows whose source lines are in that band. The region clamp keeps
   filtering inside the band, whose neighbour holds Z at that moment. */
static int copy_from_bands(float x, float y, float w, float h, u32 dst_vram, int dw, int dh)
{
    Qword *tag;
    /* Texture origin at the page holding the top-left corner: TW tops out at 1024 texels */
    int px = (int) x / 64, py = (int) y / 64;
    u32 tbp = (band_base + (u32) (py * (GS_WIDTH / 64) + px)) * 32;
    float lx = x - (float) (px * 64), ly = y - (float) (py * 64);
    int x0 = (int) lx, x1 = (int) (lx + w) - 1;
    int band;

    band_room(HIRES_BANDS + 3);
    begin_ad(&tag, 8 + 8 * HIRES_BANDS + 4);
    ad(GSR_TEXFLUSH, 0); /* the source was just drawn */
    ad(GSR_FRAME_1, GS_SETREG_FRAME(dst_vram / 8192, 1, GS_PSM_CT32, 0));
    ad(GSR_ZBUF_1, gs_zbuf_value(0));
    ad(GSR_TEST_1, GS_SETREG_TEST(0, 0, 0, 0, 0, 0, 1, 1));
    ad(GSR_XYOFFSET_1, GS_SETREG_XYOFFSET(XYOFFSET_X << 4, XYOFFSET_Y << 4)); /* not per band */
    ad(GSR_ALPHA_1, GS_SETREG_ALPHA(0, 1, 0, 1, 0));
    ad(GSR_TEX0_1, GS_SETREG_TEX0(tbp, GS_WIDTH / 64, s_gs->PSM, 10, 10, 0, 1, 0, 0, 0, 0, 0));
    ad(GSR_TEX1_1, GS_SETREG_TEX1(1, 0, 1, 1, 0, 0, 0));
    for (band = 0; band < HIRES_BANDS; band++) {
        int line0 = band * band_h, line1 = line0 + band_h;
        int r0 = copy_row_at(line0, y, h, dh), r1 = copy_row_at(line1, y, h, dh);
        int v0 = (line0 > (int) y ? line0 : (int) y) - py * 64;
        int v1 = (line1 < (int) (y + h) ? line1 : (int) (y + h)) - 1 - py * 64;
        u64 scissor = r0 < r1 ? GS_SETREG_SCISSOR(0, dw - 1, r0, r1 - 1) : GS_SETREG_SCISSOR(1, 0, 1, 0);

        v1 = v1 < v0 ? v0 : v1;
        ad(GSR_CLAMP_1, GS_SETREG_CLAMP(2, 2, x0, x1, v0, v1));
        if (!sinking) {
            BandFixup *f = &band_fixups[build_slot][band_num_fixups[build_slot]++];

            f->off = (u32) (cur - s_pkt);
            f->reg = BAND_ONLY_SCISSOR + (u32) band;
            f->value = scissor;
        }
        ad(GSR_SCISSOR_1, scissor);
        ad(GSR_PRIM, 6 | (1 << 4) | (1 << 8)); /* sprite, textured, UV coordinates */
        ad(GSR_RGBAQ, 0x80808080ULL);
        ad(GSR_UV, (u64) (lx * 16.0f) | ((u64) (ly * 16.0f) << 16));
        ad(GSR_XYZ2, (u64) (XYOFFSET_X << 4) | ((u64) (XYOFFSET_Y << 4) << 16));
        ad(GSR_UV, (u64) ((lx + w) * 16.0f) | ((u64) ((ly + h) * 16.0f) << 16));
        ad(GSR_XYZ2, (u64) ((XYOFFSET_X + dw) << 4) | ((u64) ((XYOFFSET_Y + dh) << 4) << 16));
    }
    ad(GSR_TEXFLUSH, 0); /* the slot is sampled by the next pass */
    ad_band(GSR_FRAME_1, frame_reg(draw_buffer));
    ad_band(GSR_XYOFFSET_1, GS_SETREG_XYOFFSET(XYOFFSET_X << 4, XYOFFSET_Y << 4));
    ad_band(GSR_SCISSOR_1, GS_SETREG_SCISSOR(0, GS_WIDTH - 1, 0, GS_HEIGHT - 1));
    state_valid = 0;
    return !sinking;
}

int gs_copy_from_screen(float x, float y, float w, float h, u32 dst_vram, int dw, int dh)
{
    Qword *tag;
    /* The previous frame */
    u32 orig_ = s_gs->ScreenBuffer[gs_displayed_buffer()] / 256;
    union {
        float f;
        u32 u;
    } s0, t0, s1, t1, q;

    if (hires) {
        return copy_from_bands(x, y, w, h, dst_vram, dw, dh);
    }

    flush_prims();
    s0.f = x / 1024.0f;
    t0.f = y / (float) (1 << screen_tex_th);
    s1.f = (x + w) / 1024.0f;
    t1.f = (y + h) / (float) (1 << screen_tex_th);
    q.f = 1.0f;
    begin_ad(&tag, 16);
    ad(GSR_FRAME_1, GS_SETREG_FRAME(dst_vram / 8192, 1, GS_PSM_CT32, 0));
    ad(GSR_ZBUF_1, gs_zbuf_value(0));
    ad(GSR_TEST_1, GS_SETREG_TEST(0, 0, 0, 0, 0, 0, 1, 1));
    ad(GSR_SCISSOR_1, GS_SETREG_SCISSOR(0, dw - 1, 0, dh - 1));
    ad(GSR_ALPHA_1, GS_SETREG_ALPHA(0, 1, 0, 1, 0));
    ad(GSR_TEX0_1, GS_SETREG_TEX0(orig_, GS_WIDTH / 64, s_gs->PSM, 10, screen_tex_th, 0, 1, 0, 0, 0, 0, 0));
    ad(GSR_TEX1_1, GS_SETREG_TEX1(1, 0, 1, 1, 0, 0, 0));
    ad(GSR_CLAMP_1, GS_SETREG_CLAMP(2, 2, (int) x, (int) (x + w) - 1, (int) y, (int) (y + h) - 1));
    ad(GSR_PRIM, 6 | (1 << 4)); /* textured sprite (ST) */
    ad(GSR_RGBAQ, 0x80808080ULL | ((u64) q.u << 32));
    ad(GSR_ST, (u64) s0.u | ((u64) t0.u << 32));
    ad(GSR_XYZ2, (u64) (XYOFFSET_X << 4) | ((u64) (XYOFFSET_Y << 4) << 16));
    ad(GSR_ST, (u64) s1.u | ((u64) t1.u << 32));
    ad(GSR_XYZ2, (u64) ((XYOFFSET_X + dw) << 4) | ((u64) ((XYOFFSET_Y + dh) << 4) << 16));
    /* Back to the frame's framebuffer */
    note_frame_fixup(cur);
    ad(GSR_FRAME_1, frame_reg(draw_buffer));
    ad(GSR_SCISSOR_1, GS_SETREG_SCISSOR(0, GS_WIDTH - 1, 0, GS_HEIGHT - 1));
    state_valid = 0;
    return 1;
}

#ifdef SMK64_DEV
/* Copies the on-screen image (CT16S) into the Z-buffer area as CT32: ps2_screenshot cannot read CT16S.
   Only between frames: the Z-buffer is cleared again when the next one begins. */
u32 gs_screen_to_ct32(void)
{
    static Qword pkt[16] __attribute__((aligned(64)));
    Qword *q = pkt;
    u32 orig = s_gs->ScreenBuffer[display_buffer] / 256;

    q->d[0] = giftag_lo(14, 1, 0, 0, GIF_FLG_PACKED, 1);
    q->d[1] = GIF_REG_AD;
    q++;
#define AD_DEV(r, v) (q->d[0] = (v), q->d[1] = (r), q++)
    AD_DEV(GSR_FRAME_1, GS_SETREG_FRAME(s_gs->ZBuffer / 8192, GS_WIDTH / 64, GS_PSM_CT32, 0));
    AD_DEV(GSR_ZBUF_1, gs_zbuf_value(0));
    AD_DEV(GSR_TEST_1, GS_SETREG_TEST(0, 0, 0, 0, 0, 0, 1, 1));
    AD_DEV(GSR_XYOFFSET_1, GS_SETREG_XYOFFSET(XYOFFSET_X << 4, XYOFFSET_Y << 4));
    AD_DEV(GSR_SCISSOR_1, GS_SETREG_SCISSOR(0, GS_WIDTH - 1, 0, GS_HEIGHT - 1));
    AD_DEV(GSR_TEX0_1, GS_SETREG_TEX0(orig, GS_WIDTH / 64, GS_PSM_CT16S, 10, screen_tex_th, 0, 1, 0, 0, 0, 0, 0));
    AD_DEV(GSR_TEX1_1, GS_SETREG_TEX1(1, 0, 0, 0, 0, 0, 0));
    AD_DEV(GSR_CLAMP_1, GS_SETREG_CLAMP(1, 1, 0, 0, 0, 0));
    AD_DEV(GSR_ALPHA_1, GS_SETREG_ALPHA(0, 1, 0, 1, 0));
    AD_DEV(GSR_PRIM, 6 | (1 << 4) | (1 << 8)); /* sprite, textured, UV coordinates */
    AD_DEV(GSR_UV, 0);
    AD_DEV(GSR_XYZ2, (u64) (XYOFFSET_X << 4) | ((u64) (XYOFFSET_Y << 4) << 16));
    AD_DEV(GSR_UV, (u64) (GS_WIDTH << 4) | ((u64) (GS_HEIGHT << 4) << 16));
    AD_DEV(GSR_XYZ2, (u64) ((XYOFFSET_X + GS_WIDTH) << 4) | ((u64) ((XYOFFSET_Y + GS_HEIGHT) << 4) << 16));
#undef AD_DEV
    FlushCache(0);
    wait_gif();
    dmaKit_send(DMA_CHANNEL_GIF, pkt, (u32) (q - pkt));
    wait_gif();
    state_valid = 0;
    return s_gs->ZBuffer;
}

#define CAPTURE_LINES 32 /* one CT32 page row */

static u32 shot_buf[1920 * CAPTURE_LINES] __attribute__((aligned(64)));
static u8 shot_row[1920 * 3];

/* Converts n lines of a band (CT16S) into the CT32 scratch strip at the end of VRAM. Textures are
   at most 1024 texels wide, so wide modes are copied in 1024-pixel chunks. */
static void capture_strip(int band, int row, int n)
{
    static Qword pkt[24] __attribute__((aligned(64)));
    Qword *q = pkt + 1;
    u32 scratch = vram_capture_strip();
    int chunks = (GS_WIDTH + 1023) / 1024, c;

#define AD_CAP(r, v) (q->d[0] = (v), q->d[1] = (r), q++)
    AD_CAP(GSR_FRAME_1, GS_SETREG_FRAME(scratch / 8192, GS_WIDTH / 64, GS_PSM_CT32, 0));
    AD_CAP(GSR_ZBUF_1, gs_zbuf_value(0));
    AD_CAP(GSR_TEST_1, GS_SETREG_TEST(0, 0, 0, 0, 0, 0, 1, 1));
    AD_CAP(GSR_XYOFFSET_1, GS_SETREG_XYOFFSET(XYOFFSET_X << 4, XYOFFSET_Y << 4));
    AD_CAP(GSR_SCISSOR_1, GS_SETREG_SCISSOR(0, GS_WIDTH - 1, 0, n - 1));
    AD_CAP(GSR_TEX1_1, GS_SETREG_TEX1(1, 0, 0, 0, 0, 0, 0));
    AD_CAP(GSR_CLAMP_1, GS_SETREG_CLAMP(1, 1, 0, 0, 0, 0));
    AD_CAP(GSR_ALPHA_1, GS_SETREG_ALPHA(0, 1, 0, 1, 0));
    for (c = 0; c < chunks; c++) {
        int x = c * 1024, w = GS_WIDTH - x > 1024 ? 1024 : GS_WIDTH - x;
        u32 tbp = (band_base + band * band_pages + (u32) (x / 64)) * 32;

        AD_CAP(GSR_TEX0_1, GS_SETREG_TEX0(tbp, GS_WIDTH / 64, GS_PSM_CT16S, 10, 8, 0, 1, 0, 0, 0, 0, 0));
        AD_CAP(GSR_PRIM, 6 | (1 << 4) | (1 << 8)); /* sprite, textured, UV coordinates */
        AD_CAP(GSR_UV, (u64) ((u64) (row << 4) << 16));
        AD_CAP(GSR_XYZ2, (u64) ((XYOFFSET_X + x) << 4) | ((u64) (XYOFFSET_Y << 4) << 16));
        AD_CAP(GSR_UV, (u64) (w << 4) | ((u64) ((row + n) << 4) << 16));
        AD_CAP(GSR_XYZ2, (u64) ((XYOFFSET_X + x + w) << 4) | ((u64) ((XYOFFSET_Y + n) << 4) << 16));
    }
#undef AD_CAP
    pkt[0].d[0] = giftag_lo((u32) (q - pkt - 1), 1, 0, 0, GIF_FLG_PACKED, 1);
    pkt[0].d[1] = GIF_REG_AD;
    FlushCache(0);
    wait_gif();
    dmaKit_send(DMA_CHANNEL_GIF, pkt, (u32) (q - pkt));
    wait_gif();
}

/* No moment has all bands intact (each is the next pass's Z-buffer), so the band passes are paused
   and each band is drawn and read back on its own. The screen glitches for that long. */
int gs_hires_capture(const char *path)
{
    const u32 *px = (const u32 *) ((u32) shot_buf | 0x20000000);
    u8 header[18];
    FILE *f;
    int band;

    if (!hires || hires_cur < 0) {
        return 0;
    }
    f = fopen(path, "wb");
    if (f == NULL) {
        return 0;
    }
    memset(header, 0, sizeof(header));
    header[2] = 2; /* uncompressed true colour */
    header[12] = GS_WIDTH & 0xFF;
    header[13] = GS_WIDTH >> 8;
    header[14] = GS_HEIGHT & 0xFF;
    header[15] = GS_HEIGHT >> 8;
    header[16] = 24;
    header[17] = 0x20; /* top-left origin */
    fwrite(header, 1, sizeof(header), f);

    hires_paused = 1;
    wait_gif();
    for (band = 0; band < HIRES_BANDS; band++) {
        int lines = GS_HEIGHT - band * band_h, row;

        lines = lines > band_h ? band_h : lines;
        if (lines <= 0) {
            break;
        }
        patch_band(hires_cur, band);
        band_kick(hires_cur);
        wait_gif();
        for (row = 0; row < lines; row += CAPTURE_LINES) {
            int n = lines - row > CAPTURE_LINES ? CAPTURE_LINES : lines - row, y, i;

            capture_strip(band, row, n);
            ps2_screenshot(shot_buf, vram_capture_strip() / 256, 0, 0, GS_WIDTH, n, GS_PSM_CT32);
            FlushCache(0);
            for (y = 0; y < n; y++) {
                const u32 *src = px + y * GS_WIDTH;

                for (i = 0; i < GS_WIDTH; i++) {
                    shot_row[i * 3 + 0] = (u8) (src[i] >> 16);
                    shot_row[i * 3 + 1] = (u8) (src[i] >> 8);
                    shot_row[i * 3 + 2] = (u8) src[i];
                }
                fwrite(shot_row, 1, GS_WIDTH * 3, f);
            }
        }
    }
    hires_paused = 0;
    fclose(f);
    return 1;
}
#endif

void gs_upload_texture(u32 tbp, u32 tbw, const u32 *pixels, u32 w, u32 h)
{
    gs_upload_texture_rect(tbp, tbw, 0, 0, pixels, w, h);
}

void gs_upload_texture_rect(u32 tbp, u32 tbw, u32 x, u32 y, const u32 *pixels, u32 w, u32 h)
{
    gs_upload_image(tbp, tbw, GS_PSM_CT32, x, y, pixels, w, h, w * h * 4);
}

void gs_upload_image(u32 tbp, u32 tbw, u32 psm, u32 x, u32 y, const void *pixels, u32 w, u32 h, u32 bytes)
{
    Qword *tag;
    u32 qwc = (bytes + 15) / 16;

    if (hires && (sinking || (u32) (cur - s_pkt) + qwc + 24 >= PACKET_QWORDS)) {
        /* The frame was cut short (the texture cache checks gs_upload_fits first, so only the
           debug text gets here): the texture never reaches VRAM */
        uploads_lost = 1;
        return;
    }
    upload_bytes_frame += qwc * 16;

    /* All in one go */
    flush_prims();
    reserve(qwc + 12);
    begin_ad(&tag, 4);
    ad(GSR_BITBLTBUF, ((u64) tbp << 32) | ((u64) tbw << 48) | ((u64) psm << 56));
    ad(GSR_TRXPOS, ((u64) (x & 0x7FF) << 32) | ((u64) (y & 0x7FF) << 48));
    ad(GSR_TRXREG, (u64) w | ((u64) h << 32));
    ad(GSR_TRXDIR, 0);
    reserve(qwc + 4);
    cur->d[0] = giftag_lo(qwc, 0, 0, 0, GIF_FLG_IMAGE, 0);
    cur->d[1] = 0;
    cur++;
    memcpy(cur, pixels, qwc * 16);
    cur += qwc;
    begin_ad(&tag, 1);
    ad(GSR_TEXFLUSH, 0);
    /* A+D header (5), IMAGE tag (1), data and TEXFLUSH (2). */
    capture_append(cur - (qwc + 8), qwc + 8);
    gs_stats.uploads++;
    gs_stats.upload_bytes += qwc * 16;
}
