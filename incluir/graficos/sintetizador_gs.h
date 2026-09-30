#ifndef GRAFICOS_SINTETIZADOR_GS_H
#define GRAFICOS_SINTETIZADOR_GS_H

#include <tamtypes.h>

/* Resolucion de salida */
#define GS_ANCHO  640
#define GS_ALTO 448

/* Registros generales del GS (numeros de direccion para A+D). */
enum {
    GSR_PRIM = 0x00, GSR_RGBAQ = 0x01, GSR_ST = 0x02, GSR_UV = 0x03, GSR_XYZF2 = 0x04,
    GSR_XYZ2 = 0x05, GSR_TEX0_1 = 0x06, LIMITE_GSR_1 = 0x08, NIEBLA_GSR = 0x0A,
    GSR_TEX1_1 = 0x14, GSR_XYOFFSET_1 = 0x18, GSR_PRMODECONT = 0x1A, GSR_TEXCLUT = 0x1C,
    GSR_TEXA = 0x3B, GSR_FOGCOL = 0x3D, GSR_TEXFLUSH = 0x3F, TIJERA_GSR_1 = 0x40,
    GSR_ALPHA_1 = 0x42, GSR_DTHE = 0x45, GSR_COLCLAMP = 0x46, PRUEBA_GSR_1 = 0x47,
    GSR_PABE = 0x49, GSR_FBA_1 = 0x4A, GSR_FRAME_1 = 0x4C, GSR_ZBUF_1 = 0x4E,
    GSR_BITBLTBUF = 0x50, GSR_TRXPOS = 0x51, GSR_TRXREG = 0x52, GSR_TRXDIR = 0x53,
    META_GSR = 0x61
};

/* Vertice listo para el GS */
typedef struct {
    float x, y;
    u32 z;
    float s, t, q;
    u8 r, g, b, a;
    u8 niebla;
} VerticeGs;

/* Estado de dibujo completo */
typedef struct {
    u64 prueba;
    u64 alpha;
    u64 zbuf;
    u64 tex0;
    u64 tex1;
    u64 clamp;
    u64 tijera;
    u64 fogcol;
    u32 prim;   /* bits de PRIM salvo el tipo: IIP, TME, FGE, ABE, FST */
    int texturizado;
    /* DTHE: el framebuffer es de 16 bits; sin tramado los degradados salen en bandas. */
    int dither;
} EstadoGs;

void gs_inicializar(void);
void gs_empezar_frame(void);
void gs_terminar_frame(int negro);

#define PAQUETES_GS          2
#define REAL_PAQUETE_GS      0
#define INTERP_PAQUETE_GS    1
#define GS_PAQUETE_SIN_ENVIO   1 /* no se puede enviar a medias: si no cabe, falla */
#define GS_PAQUETE_CONSERVAR_ESTADISTICAS 2 /* no reinicia estadisticas_gs (segundo paquete del frame) */
int buffer_mostrado_gs(void);
void empezar_paquete_gs(int paquete, int buffer, int banderas);
void meta_paquete_gs(int negro);
void redirigir_paquete_gs(int paquete, int buffer);
void mostrar_paquete_gs(int paquete);
int gs_paquete_fallido(int paquete);
int gs_paquete_enviado_parcial(int paquete);
void fijar_gancho_desborde_gs(void (*gancho)(void));
void subidas_capturar_gs(int on);
int gs_capturar_fallido(void);
void gs_emitir_capturado_subidas(void);
void gs_en_vblank(void); /* llamado en la interrupcion de VBLANK */
void gs_esperar_cambio_buffer(void);

void gs_aplicar_estado(const EstadoGs *st);
void gs_triangulo(const VerticeGs *v0, const VerticeGs *v1, const VerticeGs *v2);
void vertice_paquete_gs(const VerticeGs *v, u128 salida[3]);
void gs_triangulo_empaquetado(const u128 *a, const u128 *b, const u128 *c);
void gs_sprite(const VerticeGs *tl, const VerticeGs *br);
void gs_limpiar_z(void);
void gs_rellenar_rectangulo(int x0, int y0, int x1, int y1, u8 r, u8 g, u8 b);

/* Registro inicial del framebuffer/zbuffer para gs_aplicar_estado. */
u64 gs_valor_zbuf(int escribir_activacion);
u32 gs_vram_inicio_texturas(void);
u32 gs_vram_fin_texturas(void);
#ifdef SMK64_MEDIDOR
/* Zona fija de la VRAM para el panel del medidor (src/debug). */
u32 gs_vram_medidor(void);
#endif

#ifdef SMK64_DEV
u32 gs_pantalla_a_ct32(void);
#endif

void gs_copiar_desde_pantalla(float x, float y, float w, float h, u32 dst_vram, int dw, int dh);

void gs_subir_textura(u32 tbp, u32 tbw, const u32 *pixeles, u32 w, u32 h);
void subir_rect_textura_gs(u32 tbp, u32 tbw, u32 x, u32 y, const u32 *pixeles, u32 w, u32 h);
void subir_imagen_gs(u32 tbp, u32 tbw, u32 psm, u32 x, u32 y, const void *pixeles, u32 w, u32 h, u32 bytes);

typedef struct {
    u32 triangulos;
    u32 sprites;
    u32 subidas;
    u32 bytes_subidos;
    u32 bytes_paquete;
    u32 frames;
    u32 volteos;       /* cambios de buffer en pantalla (acumulado) */
    u32 mantenido[4];
} EstadisticasGs;
extern EstadisticasGs estadisticas_gs;

u32 gs_vram_en_pantalla(void);

#endif
