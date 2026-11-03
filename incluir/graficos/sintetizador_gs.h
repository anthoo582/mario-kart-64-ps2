#ifndef GRAFICOS_SINTETIZADOR_GS_H
#define GRAFICOS_SINTETIZADOR_GS_H

#include <tamtypes.h>

/* Video modes, same table as mk64-dc's gfx_ps2_wapi.c. 720p and 1080i use banded multi-pass
   rendering (the gsHires technique, see sintetizador_gs.c): no 60 FPS interpolation in those
   modes, and giant screens are copied at the end of the frame rather than the start. */
enum {
    GS_VIDEO_240P,
    GS_VIDEO_480I, /* 576i in PAL builds */
    GS_VIDEO_480P, /* 576p in PAL builds */
    GS_VIDEO_720P,
    GS_VIDEO_1080I,
    GS_NUM_VIDEO_MODES
};

/* Mode used at boot: make EXTRA_DEFINES=-DGS_VIDEO_MODE=GS_VIDEO_480P */
#ifndef GS_VIDEO_MODE
#define GS_VIDEO_MODE GS_VIDEO_1080I
#endif

/* Only before gs_init: the VRAM layout (framebuffers, texture atlas) is fixed after that.
   Returns 0 if the mode is out of range or the GS is already initialised. */
int gs_set_video_mode(int mode);
/* 1 in 720p/1080i (banded multi-pass rendering), valid after gs_init */
int gs_hires(void);
/* Hires: 0 if an upload of this size would go over this frame's upload budget (every band pass
   resends the frame's uploads); the texture cache then leaves the texture for a later frame */
int gs_upload_fits(u32 bytes);
/* Hires: 1 once after an upload was skipped, so the texture cache stops trusting VRAM */
int gs_take_lost_uploads(void);
/* Hires diagnostics: CRTC line (per field in 1080i) each band's pass last started on, HSINTs in
   the last refresh, measured refresh period in us, times HSINT had to be re-armed */
void gs_hires_diag(u32 out[6]);

/* Output resolution, set by the video mode in gs_init */
extern int gs_width, gs_height;
extern float gs_scale_x, gs_scale_y; /* GS pixels per N64 pixel (320x240) */
#define GS_WIDTH  gs_width
#define GS_HEIGHT gs_height

/* GS general-purpose registers (address numbers for A+D). */
enum {
    GSR_PRIM = 0x00, GSR_RGBAQ = 0x01, GSR_ST = 0x02, GSR_UV = 0x03, GSR_XYZF2 = 0x04,
    GSR_XYZ2 = 0x05, GSR_TEX0_1 = 0x06, GSR_CLAMP_1 = 0x08, GSR_FOG = 0x0A,
    GSR_TEX1_1 = 0x14, GSR_XYOFFSET_1 = 0x18, GSR_PRMODECONT = 0x1A, GSR_TEXCLUT = 0x1C,
    GSR_TEXA = 0x3B, GSR_FOGCOL = 0x3D, GSR_TEXFLUSH = 0x3F, GSR_SCISSOR_1 = 0x40,
    GSR_ALPHA_1 = 0x42, GSR_DTHE = 0x45, GSR_COLCLAMP = 0x46, GSR_TEST_1 = 0x47,
    GSR_PABE = 0x49, GSR_FBA_1 = 0x4A, GSR_FRAME_1 = 0x4C, GSR_ZBUF_1 = 0x4E,
    GSR_BITBLTBUF = 0x50, GSR_TRXPOS = 0x51, GSR_TRXREG = 0x52, GSR_TRXDIR = 0x53,
    GSR_FINISH = 0x61
};

/* Vertex ready for the GS */
typedef struct {
    float x, y;
    u32 z;
    float s, t, q;
    u8 r, g, b, a;
    u8 fog;
} GsVertex;

/* Complete draw state */
typedef struct {
    u64 test;
    u64 alpha;
    u64 zbuf;
    u64 tex0;
    u64 tex1;
    u64 clamp;
    u64 scissor;
    u64 fogcol;
    u32 prim;   /* PRIM bits except the type: IIP, TME, FGE, ABE, FST */
    int textured;
    /* DTHE: the framebuffer is 16-bit; without dithering, gradients come out banded. */
    int dither;
} GsState;

void gs_init(void);
void gs_begin_frame(void);
void gs_end_frame(int black);

#define GS_NUM_PACKETS       2
#define GS_PACKET_REAL       0
#define GS_PACKET_INTERP     1
#define GS_PACKET_NO_PARTIAL 1 /* cannot be sent in pieces: if it does not fit, it fails */
#define GS_PACKET_KEEP_STATS 2 /* does not reset gs_stats (second packet of the frame) */
int gs_displayed_buffer(void);
void gs_begin_packet(int packet, int buffer, int flags);
void gs_finish_packet(int black);
void gs_retarget_packet(int packet, int buffer);
void gs_present_packet(int packet);
int gs_packet_failed(int packet);
int gs_packet_partially_sent(int packet);
void gs_set_overflow_hook(void (*hook)(void));
void gs_capture_uploads(int on);
int gs_capture_failed(void);
void gs_emit_captured_uploads(void);
void gs_on_vblank(void); /* called from the VBLANK interrupt */
void gs_wait_flip(void);
void gs_log_gif_stall(void); /* crash screen: GS state and the data before the stall */

void gs_apply_state(const GsState *st);
void gs_triangle(const GsVertex *v0, const GsVertex *v1, const GsVertex *v2);
void gs_pack_vertex(const GsVertex *v, u128 out[3]);
void gs_triangle_packed(const u128 *a, const u128 *b, const u128 *c);
void gs_sprite(const GsVertex *tl, const GsVertex *br);
void gs_clear_z(void);
void gs_fill_rect(int x0, int y0, int x1, int y1, u8 r, u8 g, u8 b);

/* Initial framebuffer/zbuffer register for gs_apply_state. */
u64 gs_zbuf_value(int write_enable);
u32 gs_vram_texture_start(void);
u32 gs_vram_texture_end(void);
#ifdef SMK64_MEDIDOR
/* Fixed VRAM area for the performance meter panel (src/debug). */
u32 gs_vram_meter(void);
#endif

#ifdef SMK64_DEV
u32 gs_screen_to_ct32(void);
/* Hires screenshot to a 24-bit TGA (path includes "host:"); renders the bands one by one */
int gs_hires_capture(const char *path);
void gs_hires_log_status(void);
#endif

/* Copies a screen area into a CT32 texture. Normally reads the previous frame, so it goes at the
   start of the frame; in hires it reads each band after its pass, so it goes at the end.
   Returns 0 when the copy will not happen (hires: the frame was cut short). */
int gs_copy_from_screen(float x, float y, float w, float h, u32 dst_vram, int dw, int dh);

void gs_upload_texture(u32 tbp, u32 tbw, const u32 *pixels, u32 w, u32 h);
void gs_upload_texture_rect(u32 tbp, u32 tbw, u32 x, u32 y, const u32 *pixels, u32 w, u32 h);
void gs_upload_image(u32 tbp, u32 tbw, u32 psm, u32 x, u32 y, const void *pixels, u32 w, u32 h, u32 bytes);

typedef struct {
    u32 triangles;
    u32 sprites;
    u32 uploads;
    u32 upload_bytes;
    u32 packet_bytes;
    u32 frames;
    u32 flips;       /* on-screen buffer swaps (cumulative) */
    u32 held[4];
    /* Hires (cumulative) */
    u32 band_passes;   /* band passes sent to the GS */
    u32 band_overruns; /* passes skipped: the previous one was still being sent */
    u32 dropped;       /* frames cut short: packet or band fixup list full */
} GsStats;
extern GsStats gs_stats;

u32 gs_vram_on_screen(void);

#endif
