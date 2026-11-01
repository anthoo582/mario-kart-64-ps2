#ifndef GRAFICOS_SINTETIZADOR_GS_H
#define GRAFICOS_SINTETIZADOR_GS_H

#include <tamtypes.h>

/* Output resolution */
#define GS_WIDTH  640
#define GS_HEIGHT 448

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
#endif

void gs_copy_from_screen(float x, float y, float w, float h, u32 dst_vram, int dw, int dh);

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
} GsStats;
extern GsStats gs_stats;

u32 gs_vram_on_screen(void);

#endif
