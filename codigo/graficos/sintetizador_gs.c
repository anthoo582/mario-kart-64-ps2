#include <dmaKit.h>
#include <gsKit.h>
#include <kernel.h>
#include <string.h>

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

static void packet_full(void);

/* Ensures there is room for n qwords */
static inline void reserve(u32 n)
{
    if ((u32) (cur - s_pkt) + n + 8 >= PACKET_QWORDS) {
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

u32 gs_vram_texture_end(void)
{
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
    for (b = 0; b < 2; b++) {
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

    s_gs = gsKit_init_global();
    s_gs->Mode = GS_MODE_NTSC;
    s_gs->Interlace = GS_INTERLACED;
    s_gs->Field = GS_FIELD;
    s_gs->Width = GS_WIDTH;
    s_gs->Height = GS_HEIGHT;
    s_gs->PSM = GS_PSM_CT16S;
    s_gs->PSMZ = GS_PSMZ_24;
    s_gs->ZBuffering = GS_SETTING_ON;
    s_gs->DoubleBuffering = GS_SETTING_ON;
    s_gs->PrimAlphaEnable = GS_SETTING_ON;
    s_gs->Dithering = GS_SETTING_ON;

    dmaKit_init(D_CTRL_RELE_OFF, D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC, D_CTRL_STD_OFF, D_CTRL_RCYC_8,
                1 << DMA_CHANNEL_GIF);
    dmaKit_chan_init(DMA_CHANNEL_GIF);
    gsKit_init_screen(s_gs);
    gsKit_mode_switch(s_gs, GS_ONESHOT);
    gsKit_queue_exec(s_gs);

    memset(&sema, 0, sizeof(sema));
    sema.init_count = 0;
    sema.max_count = 1;
    flip_sema = CreateSema(&sema);

    display_buffer = 0;
    draw_buffer = 1;
    pending_flip = -1;
    set_display(0);
    clear_screens();

    registrar("GS: fb %x/%x z %x textures %x-%x", (unsigned) s_gs->ScreenBuffer[0], (unsigned) s_gs->ScreenBuffer[1],
            (unsigned) s_gs->ZBuffer, (unsigned) gs_vram_texture_start(), (unsigned) gs_vram_texture_end());
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
static volatile u32 slot_busy; /* packet buffers queued or in the GS */

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

/* Called from the VBLANK interrupt (retrazo_vertical.c). */
void gs_on_vblank(void)
{
    static u32 since_flip;

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
void gs_wait_flip(void)
{
    while (pending_flip >= 0 || queue_count > 0) {
        WaitSema(flip_sema);
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

    if (packet == GS_PACKET_REAL) {
        packet_slot[packet] = (packet_slot[packet] == 0) ? 2 : 0;
    }
    wait_slot(packet_slot[packet]);
    cur_packet = packet;
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
    ad(GSR_FRAME_1, frame_reg(draw_buffer));
    ad(GSR_ZBUF_1, gs_zbuf_value(1));
    ad(GSR_XYOFFSET_1, GS_SETREG_XYOFFSET(XYOFFSET_X << 4, XYOFFSET_Y << 4));
    ad(GSR_SCISSOR_1, GS_SETREG_SCISSOR(0, GS_WIDTH - 1, 0, GS_HEIGHT - 1));
    ad(GSR_PRMODECONT, 1);
    ad(GSR_COLCLAMP, 1);
    ad(GSR_DTHE, 0);
    ad(GSR_TEXA, GS_SETREG_TEXA(0, 0, 0x80));
    ad(GSR_FBA_1, 0);
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
    packet_info[cur_packet].end = cur;
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

    for (i = 0; i < pi->num_fixups; i++) {
        pi->frame_fixups[i]->d[0] = frame_reg(buffer);
    }
    pi->buffer = buffer;
    if (packet == cur_packet) {
        draw_buffer = buffer;
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

#define CAPTURE_QWORDS (32 * 1024)

static Qword capture_buf[CAPTURE_QWORDS] __attribute__((aligned(64)));
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
    begin_ad(&tag, 9);
    ad(GSR_TEST_1, st->test);
    ad(GSR_ALPHA_1, st->alpha);
    ad(GSR_ZBUF_1, st->zbuf);
    ad(GSR_SCISSOR_1, st->scissor);
    ad(GSR_FOGCOL, st->fogcol);
    ad(GSR_DTHE, st->dither ? 1 : 0);
    if (st->textured) {
        ad(GSR_TEX0_1, st->tex0);
        ad(GSR_TEX1_1, st->tex1);
        ad(GSR_CLAMP_1, st->clamp);
        n = 9;
    } else {
        n = 6;
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
    cur->d[1] = ((u64) (v->z & 0xFFFFFF) << 4) | ((u64) v->fog << 36);
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
    q[2].d[1] = ((u64) (v->z & 0xFFFFFF) << 4) | ((u64) v->fog << 36);
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

void gs_sprite(const GsVertex *tl, const GsVertex *br)
{
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

    begin_ad(&tag, 4);
    ad(GSR_FRAME_1, GS_SETREG_FRAME(s_gs->ZBuffer / 8192, GS_WIDTH / 64, GS_PSM_CT32, 0));
    ad(GSR_TEST_1, GS_SETREG_TEST(0, 0, 0, 0, 0, 0, 1, 1));
    ad(GSR_ZBUF_1, gs_zbuf_value(0));
    ad(GSR_SCISSOR_1, GS_SETREG_SCISSOR(0, GS_WIDTH - 1, 0, GS_HEIGHT - 1));
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
    ad(GSR_FRAME_1, frame_reg(draw_buffer));
}

void gs_copy_from_screen(float x, float y, float w, float h, u32 dst_vram, int dw, int dh)
{
    Qword *tag;
    /* The previous frame */
    u32 orig_ = s_gs->ScreenBuffer[gs_displayed_buffer()] / 256;
    union {
        float f;
        u32 u;
    } s0, t0, s1, t1, q;

    flush_prims();
    s0.f = x / 1024.0f;
    t0.f = y / 512.0f;
    s1.f = (x + w) / 1024.0f;
    t1.f = (y + h) / 512.0f;
    q.f = 1.0f;
    begin_ad(&tag, 16);
    ad(GSR_FRAME_1, GS_SETREG_FRAME(dst_vram / 8192, 1, GS_PSM_CT32, 0));
    ad(GSR_ZBUF_1, gs_zbuf_value(0));
    ad(GSR_TEST_1, GS_SETREG_TEST(0, 0, 0, 0, 0, 0, 1, 1));
    ad(GSR_SCISSOR_1, GS_SETREG_SCISSOR(0, dw - 1, 0, dh - 1));
    ad(GSR_ALPHA_1, GS_SETREG_ALPHA(0, 1, 0, 1, 0));
    ad(GSR_TEX0_1, GS_SETREG_TEX0(orig_, GS_WIDTH / 64, s_gs->PSM, 10, 9, 0, 1, 0, 0, 0, 0, 0));
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
    AD_DEV(GSR_TEX0_1, GS_SETREG_TEX0(orig, GS_WIDTH / 64, GS_PSM_CT16S, 10, 9, 0, 1, 0, 0, 0, 0, 0));
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
