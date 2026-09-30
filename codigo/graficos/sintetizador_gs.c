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

#define QWORDS_PAQUETE 0xFFF0 /* casi 1 MB: un solo envio DMA normal (QWC de 16 bits) */
/* Memorias de paquete */
#define RANURAS_PAQUETE 3
#define DESPLAZAMIENTO_X_XY   (2048 - GS_ANCHO / 2)
#define DESPLAZAMIENTO_Y_XY   (2048 - GS_ALTO / 2)

typedef union {
    u128 q;
    u64 d[2];
    u32 w[4];
    float f[4];
} Qword __attribute__((aligned(16)));

static Qword paquetes[RANURAS_PAQUETE][QWORDS_PAQUETE] __attribute__((aligned(64)));

#define MAX_FRAME_FIXUPS 16

typedef struct {
    Qword *end;          /* fin del contenido ya cerrado */
    int buffer;          /* framebuffer de destino */
    int flags;
    int desborde;        /* no cupo (solo con GS_PAQUETE_SIN_ENVIO): descartar */
    int parcial_enviado;     /* ya se envio una parte a mitad del armado */
    Qword *frame_fixups[MAX_FRAME_FIXUPS];
    int num_fixups;
} InfoPaquete;

static InfoPaquete info_paquete[PAQUETES_GS];
static int indice_paquete;  /* paquete que se esta armando */
static int ranura_paquete[PAQUETES_GS] = { 0, 1 }; /* memoria de cada paquete */
static void (*gancho_desborde)(void);
static Qword *s_pkt;       /* inicio del paquete actual */
static Qword *act;
static Qword *etiqueta_prim;   /* etiqueta GIF de la tanda de primitivas abierta, o NULL */
static u32 prim_nloop;
static u32 prim_regs;     /* numero de registros por vertice de la tanda */

static GSGLOBAL *s_gs;
static int buffer_dibujo;           /* buffer donde se dibuja este frame */
static volatile int buffer_pantalla;
static volatile int pendiente_volteo = -1;
static s32 sema_volteo;
static EstadoGs estado;            /* ultimo estado emitido */
static int valido_estado;

EstadisticasGs estadisticas_gs;

#define GIF_FLG_PACKED 0
#define GIF_FLG_IMAGE  2
#define GIF_REG_AD     0xE

static inline u64 giftag_lo(u32 nloop, int eop, int pre, u32 prim, int flg, int nreg)
{
    return (u64) (nloop & 0x7FFF) | ((u64) eop << 15) | ((u64) pre << 46) | ((u64) (prim & 0x7FF) << 47) |
           ((u64) flg << 58) | ((u64) (nreg & 0xF) << 60);
}

static void vaciar_prims(void)
{
    if (etiqueta_prim != NULL) {
        etiqueta_prim->d[0] = (etiqueta_prim->d[0] & ~0x7FFFULL) | prim_nloop;
        etiqueta_prim = NULL;
    }
}

static void lanzar(int wait);

static void lleno_paquete(void);

/* Garantiza espacio para n qwords */
static inline void reservar(u32 n)
{
    if ((u32) (act - s_pkt) + n + 8 >= QWORDS_PAQUETE) {
        lleno_paquete();
    }
}

static void empezar_ad(Qword **etiqueta, u32 n)
{
    vaciar_prims();
    reservar(n + 1);
    *etiqueta = act++;
    (*etiqueta)->d[0] = giftag_lo(n, 0, 0, 0, GIF_FLG_PACKED, 1);
    (*etiqueta)->d[1] = GIF_REG_AD;
}

static inline void ad(u32 reg, u64 value)
{
    act->d[0] = value;
    act->d[1] = reg;
    act++;
}

#define D2_CHCR ((volatile u32 *) 0x1000A000)

/* Espera a que acabe el DMA del GIF */
static void esperar_gif(void)
{
    u32 giros = 0;
#ifdef SMK64_MEDIDOR
    u32 inicio = medidor_leer_ciclos();
#endif

    while (*D2_CHCR & 0x100) {
        if (++giros == 20u * 1000u * 1000u) { /* lecturas sin cache: ~1-2 s */
            detener_por_gif_trabado("GIF DMA sin terminar tras ~20M lecturas");
        }
    }
#ifdef SMK64_MEDIDOR
    if (giros != 0) {
        medidor_sumar_espera_dma(medidor_leer_ciclos() - inicio);
    }
#endif
}

static void enviar_dma(Qword *empezar, u32 qwc)
{
    while (qwc > 0) {
        u32 n = qwc > 0xFFF0 ? 0xFFF0 : qwc;

        esperar_gif();
        dmaKit_send(DMA_CHANNEL_GIF, empezar, n);
#ifdef SMK64_MEDIDOR
        medidor_contar_envio_dma();
#endif
        empezar += n;
        qwc -= n;
    }
}

/* Envia el paquete en curso */
static void lanzar(int wait)
{
    u32 qwc = act - s_pkt;

    EMPEZAR_PROF(ENVIO_PROF);

    if (qwc == 0) {
        return;
    }
    estadisticas_gs.bytes_paquete += qwc * 16;
    FlushCache(0);
    enviar_dma(s_pkt, qwc);
    if (wait) {
        esperar_gif();
        act = s_pkt;
    }
    FIN_PROF(ENVIO_PROF);
}

static u64 frame_reg(int buffer)
{
    return GS_SETREG_FRAME(s_gs->ScreenBuffer[buffer] / 8192, GS_ANCHO / 64, s_gs->PSM, 0);
}

u64 gs_valor_zbuf(int escribir_activacion)
{
    return GS_SETREG_ZBUF(s_gs->ZBuffer / 8192, s_gs->PSMZ & 0xF, escribir_activacion ? 0 : 1);
}

#ifdef SMK64_MEDIDOR
/* El panel del medidor va justo detras del Z-buffer */
u32 gs_vram_medidor(void)
{
    return (s_gs->CurrentPointer + 8191) & ~8191u;
}

u32 gs_vram_inicio_texturas(void)
{
    return gs_vram_medidor() + MEDIDOR_VRAM_BYTES;
}
#else
u32 gs_vram_inicio_texturas(void)
{
    return (s_gs->CurrentPointer + 8191) & ~8191u;
}
#endif

u32 gs_vram_fin_texturas(void)
{
    return 4 * 1024 * 1024 - BYTES_VRAM_PANTALLAS_GIGANTES;
}

u32 gs_vram_en_pantalla(void)
{
    return s_gs->ScreenBuffer[buffer_pantalla];
}

static void fijar_pantalla(int buffer)
{
    u64 v = ((u64) (s_gs->ScreenBuffer[buffer] / 8192)) | ((u64) (GS_ANCHO / 64) << 9) | ((u64) s_gs->PSM << 15);

    *(volatile u64 *) 0x12000070 = v;
    *(volatile u64 *) 0x12000090 = v;
}

/* La VRAM conserva la ultima imagen del cargador (OPL, navegador) hasta el primer frame. */
static void limpiar_pantallas(void)
{
    static Qword pkt[20] __attribute__((aligned(64)));
    Qword *q = pkt;
    int b;

    q->d[0] = giftag_lo(15, 1, 0, 0, GIF_FLG_PACKED, 1);
    q->d[1] = GIF_REG_AD;
    q++;
#define AD_INI(r, v) (q->d[0] = (v), q->d[1] = (r), q++)
    AD_INI(GSR_ZBUF_1, gs_valor_zbuf(0));
    AD_INI(PRUEBA_GSR_1, GS_SETREG_TEST(0, 0, 0, 0, 0, 0, 1, 1));
    AD_INI(GSR_XYOFFSET_1, GS_SETREG_XYOFFSET(DESPLAZAMIENTO_X_XY << 4, DESPLAZAMIENTO_Y_XY << 4));
    AD_INI(TIJERA_GSR_1, GS_SETREG_SCISSOR(0, GS_ANCHO - 1, 0, GS_ALTO - 1));
    AD_INI(GSR_DTHE, 0);
    for (b = 0; b < 2; b++) {
        AD_INI(GSR_FRAME_1, frame_reg(b));
        AD_INI(GSR_PRIM, 6);
        AD_INI(GSR_RGBAQ, 0x80000000ULL);
        AD_INI(GSR_XYZ2, (u64) (DESPLAZAMIENTO_X_XY << 4) | ((u64) (DESPLAZAMIENTO_Y_XY << 4) << 16));
        AD_INI(GSR_XYZ2, (u64) ((DESPLAZAMIENTO_X_XY + GS_ANCHO) << 4) | ((u64) ((DESPLAZAMIENTO_Y_XY + GS_ALTO) << 4) << 16));
    }
#undef AD_INI
    FlushCache(0);
    esperar_gif();
    dmaKit_send(DMA_CHANNEL_GIF, pkt, (u32) (q - pkt));
    esperar_gif();
}

void gs_inicializar(void)
{
    ee_sema_t sema;

    s_gs = gsKit_init_global();
    s_gs->Mode = GS_MODE_NTSC;
    s_gs->Interlace = GS_INTERLACED;
    s_gs->Field = GS_FIELD;
    s_gs->Width = GS_ANCHO;
    s_gs->Height = GS_ALTO;
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
    sema_volteo = CreateSema(&sema);

    buffer_pantalla = 0;
    buffer_dibujo = 1;
    pendiente_volteo = -1;
    fijar_pantalla(0);
    limpiar_pantallas();

    registrar("GS: fb %x/%x z %x texturas %x-%x", (unsigned) s_gs->ScreenBuffer[0], (unsigned) s_gs->ScreenBuffer[1],
            (unsigned) s_gs->ZBuffer, (unsigned) gs_vram_inicio_texturas(), (unsigned) gs_vram_fin_texturas());
}

#define COLA_PRESENTE 4

typedef struct {
    Qword *start;
    u32 qwc;
    int buffer;
    int slot;
} FrameEnCola;

static volatile FrameEnCola cola[COLA_PRESENTE];
static volatile int cabeza_q, cantidad_q;
static volatile int ranura_gs = -1;
static volatile u32 ocupado_ranura; /* memorias de paquete encoladas o en el GS */

#define CSR ((volatile u64 *) 0x12001000)

static void mostrar_inicio(Qword *empezar, u32 qwc, int buffer, int ranura)
{
    *CSR = 2; /* limpia un FINISH anterior */
    *(volatile u32 *) 0x1000A020 = qwc;
    *(volatile u32 *) 0x1000A010 = (u32) empezar & 0x0FFFFFFF;
    *(volatile u32 *) 0x1000A000 = 0x101;
    pendiente_volteo = buffer;
    ranura_gs = ranura;
}

/* Se llama desde la interrupcion de VBLANK (retrazo_vertical.c). */
void gs_en_vblank(void)
{
    static u32 desde_volteo;

    if (en_panico_ps2) {
        return; /* la pantalla de fallo es la que se ve: no cambiar DISPFB */
    }
    desde_volteo++;
    if (pendiente_volteo >= 0 && (*CSR & 2)) {
        *CSR = 2;
        estadisticas_gs.volteos++;
        /* Cuanto estuvo en pantalla la imagen que se reemplaza. */
        estadisticas_gs.mantenido[desde_volteo >= 4 ? 3 : desde_volteo - 1]++;
        desde_volteo = 0;
        fijar_pantalla(pendiente_volteo);
        buffer_pantalla = pendiente_volteo;
        pendiente_volteo = -1;
        if (ranura_gs >= 0) {
            ocupado_ranura &= ~(1u << ranura_gs);
            ranura_gs = -1;
        }
        if (cantidad_q > 0) {
            volatile FrameEnCola *p = &cola[cabeza_q];

            cabeza_q = (cabeza_q + 1) % COLA_PRESENTE;
            cantidad_q--;
            mostrar_inicio(p->start, p->qwc, p->buffer, p->slot);
        }
        iSignalSema(sema_volteo);
    }
}

/* Espera a que todo lo encolado este en pantalla. */
void gs_esperar_cambio_buffer(void)
{
    while (pendiente_volteo >= 0 || cantidad_q > 0) {
        WaitSema(sema_volteo);
    }
}

/* Espera a que una memoria de paquete quede libre. */
static void esperar_ranura(int ranura)
{
#ifdef SMK64_MEDIDOR
    if (ocupado_ranura & (1u << ranura)) {
        u32 inicio = medidor_leer_ciclos();

        while (ocupado_ranura & (1u << ranura)) {
            WaitSema(sema_volteo);
        }
        medidor_sumar_espera_gs(medidor_leer_ciclos() - inicio);
    }
#else
    while (ocupado_ranura & (1u << ranura)) {
        WaitSema(sema_volteo);
    }
#endif
}

int buffer_mostrado_gs(void)
{
    int result;
    int intr = DI();

    if (cantidad_q > 0) {
        result = cola[(cabeza_q + cantidad_q - 1) % COLA_PRESENTE].buffer;
    } else if (pendiente_volteo >= 0) {
        result = pendiente_volteo;
    } else {
        result = buffer_pantalla;
    }
    if (intr) {
        EI();
    }
    return result;
}

/* No cabe mas en el paquete */
static void lleno_paquete(void)
{
    InfoPaquete *pi = &info_paquete[indice_paquete];

    vaciar_prims();
    if (pi->flags & GS_PAQUETE_SIN_ENVIO) {
        pi->desborde = 1;
        act = s_pkt + 16;
        return;
    }
    if (gancho_desborde != NULL) {
        void (*gancho)(void) = gancho_desborde;

        gancho_desborde = NULL;
        gancho();
    }
    gs_esperar_cambio_buffer();
    pi->parcial_enviado = 1;
    lanzar(1);
    /* Lo enviado ya no se puede cambiar, y su memoria se reutiliza */
    pi->num_fixups = 0;
}

static void correccion_frame_nota(Qword *q)
{
    InfoPaquete *pi = &info_paquete[indice_paquete];

    if (pi->num_fixups < MAX_FRAME_FIXUPS) {
        pi->frame_fixups[pi->num_fixups++] = q;
    }
}

void fijar_gancho_desborde_gs(void (*gancho)(void))
{
    gancho_desborde = gancho;
}

void empezar_paquete_gs(int paquete, int buffer, int banderas)
{
    InfoPaquete *pi = &info_paquete[paquete];
    Qword *etiqueta;

    if (paquete == REAL_PAQUETE_GS) {
        ranura_paquete[paquete] = (ranura_paquete[paquete] == 0) ? 2 : 0;
    }
    esperar_ranura(ranura_paquete[paquete]);
    indice_paquete = paquete;
    s_pkt = act = paquetes[ranura_paquete[paquete]];
    etiqueta_prim = NULL;
    valido_estado = 0;
    buffer_dibujo = buffer;
    memset(pi, 0, sizeof(*pi));
    pi->buffer = buffer;
    pi->flags = banderas;
    if (!(banderas & GS_PAQUETE_CONSERVAR_ESTADISTICAS)) {
        estadisticas_gs.triangulos = estadisticas_gs.sprites = estadisticas_gs.subidas = estadisticas_gs.bytes_subidos = 0;
        estadisticas_gs.bytes_paquete = 0;
    }

    empezar_ad(&etiqueta, 9);
    correccion_frame_nota(act);
    ad(GSR_FRAME_1, frame_reg(buffer_dibujo));
    ad(GSR_ZBUF_1, gs_valor_zbuf(1));
    ad(GSR_XYOFFSET_1, GS_SETREG_XYOFFSET(DESPLAZAMIENTO_X_XY << 4, DESPLAZAMIENTO_Y_XY << 4));
    ad(TIJERA_GSR_1, GS_SETREG_SCISSOR(0, GS_ANCHO - 1, 0, GS_ALTO - 1));
    ad(GSR_PRMODECONT, 1);
    ad(GSR_COLCLAMP, 1);
    ad(GSR_DTHE, 0);
    ad(GSR_TEXA, GS_SETREG_TEXA(0, 0, 0x80));
    ad(GSR_FBA_1, 0);
}

void meta_paquete_gs(int negro)
{
    Qword *etiqueta;

    if (negro) {
        gs_rellenar_rectangulo(0, 0, GS_ANCHO, GS_ALTO, 0, 0, 0);
    }
    empezar_ad(&etiqueta, 1);
    ad(META_GSR, 0);
    etiqueta->d[0] |= 1ULL << 15;
    vaciar_prims();
    info_paquete[indice_paquete].end = act;
}

int gs_paquete_fallido(int paquete)
{
    return info_paquete[paquete].desborde;
}

int gs_paquete_enviado_parcial(int paquete)
{
    return info_paquete[paquete].parcial_enviado;
}

void redirigir_paquete_gs(int paquete, int buffer)
{
    InfoPaquete *pi = &info_paquete[paquete];
    int i;

    for (i = 0; i < pi->num_fixups; i++) {
        pi->frame_fixups[i]->d[0] = frame_reg(buffer);
    }
    pi->buffer = buffer;
    if (paquete == indice_paquete) {
        buffer_dibujo = buffer;
    }
}

void mostrar_paquete_gs(int paquete)
{
    InfoPaquete *pi = &info_paquete[paquete];
    int ranura = ranura_paquete[paquete];
    Qword *empezar = paquetes[ranura];
    u32 qwc = pi->end - empezar;
    int intr;

    estadisticas_gs.bytes_paquete += qwc * 16;
    estadisticas_gs.frames++;
    FlushCache(0);
    intr = DI();
    ocupado_ranura |= 1u << ranura;
    if (pendiente_volteo < 0 && cantidad_q == 0) {
        mostrar_inicio(empezar, qwc, pi->buffer, ranura); /* el GS esta libre */
    } else {
        volatile FrameEnCola *p = &cola[(cabeza_q + cantidad_q) % COLA_PRESENTE];

        p->start = empezar;
        p->qwc = qwc;
        p->buffer = pi->buffer;
        p->slot = ranura;
        cantidad_q++;
    }
    if (intr) {
        EI();
    }
#ifdef SMK64_MEDIDOR
    medidor_contar_envio_dma();
#endif
}

/* Frame normal (un solo paquete) */
void gs_empezar_frame(void)
{
    gs_esperar_cambio_buffer();
    empezar_paquete_gs(REAL_PAQUETE_GS, buffer_pantalla ^ 1, 0);
}

void gs_terminar_frame(int negro)
{
    meta_paquete_gs(negro);
    mostrar_paquete_gs(REAL_PAQUETE_GS);
}

#define QWORDS_CAPTURAR (32 * 1024)

static Qword capturar[QWORDS_CAPTURAR] __attribute__((aligned(64)));
static u32 largo_capturar;
static int capturing, desborde_capturar;

void subidas_capturar_gs(int on)
{
    if (on) {
        largo_capturar = 0;
        desborde_capturar = 0;
    }
    capturing = on;
}

int gs_capturar_fallido(void)
{
    return desborde_capturar;
}

static void capturar_2(const Qword *from, u32 qwc)
{
    if (!capturing) {
        return;
    }
    if (largo_capturar + qwc > QWORDS_CAPTURAR) {
        desborde_capturar = 1;
        return;
    }
    memcpy(&capturar[largo_capturar], from, qwc * 16);
    largo_capturar += qwc;
}

void gs_emitir_capturado_subidas(void)
{
    u32 hecho = 0;

    vaciar_prims();
    while (hecho < largo_capturar) {
        u32 n = largo_capturar - hecho;
        u32 lugar = QWORDS_PAQUETE - (u32) (act - s_pkt) - 16;

        if (n > lugar) {
            n = lugar;
        }
        memcpy(act, &capturar[hecho], n * 16);
        act += n;
        hecho += n;
        if (hecho < largo_capturar) {
            reservar(QWORDS_PAQUETE); /* no cabe: el paquete queda marcado como fallido */
        }
    }
    valido_estado = 0;
}

void gs_aplicar_estado(const EstadoGs *st)
{
    Qword *etiqueta;
    u32 n = 0;

    if (valido_estado && st->prueba == estado.prueba && st->alpha == estado.alpha && st->zbuf == estado.zbuf &&
        st->tex0 == estado.tex0 && st->tex1 == estado.tex1 && st->clamp == estado.clamp &&
        st->tijera == estado.tijera && st->fogcol == estado.fogcol && st->dither == estado.dither) {
        if (st->prim != estado.prim || st->texturizado != estado.texturizado) {
            vaciar_prims();
            estado.prim = st->prim;
            estado.texturizado = st->texturizado;
        }
        return;
    }
    empezar_ad(&etiqueta, 9);
    ad(PRUEBA_GSR_1, st->prueba);
    ad(GSR_ALPHA_1, st->alpha);
    ad(GSR_ZBUF_1, st->zbuf);
    ad(TIJERA_GSR_1, st->tijera);
    ad(GSR_FOGCOL, st->fogcol);
    ad(GSR_DTHE, st->dither ? 1 : 0);
    if (st->texturizado) {
        ad(GSR_TEX0_1, st->tex0);
        ad(GSR_TEX1_1, st->tex1);
        ad(LIMITE_GSR_1, st->clamp);
        n = 9;
    } else {
        n = 6;
    }
    etiqueta->d[0] = giftag_lo(n, 0, 0, 0, GIF_FLG_PACKED, 1);
    act = etiqueta + 1 + n;
    estado = *st;
    valido_estado = 1;
}

#define PRIM_TRIANGLE 3
#define PRIM_SPRITE   6

static inline void abrir_prims(u32 type)
{
    u32 prim = type | estado.prim;
    u32 nreg = estado.texturizado ? 3 : 2;

    if (etiqueta_prim != NULL && ((etiqueta_prim->d[0] >> 47) & 0x7FF) == prim && prim_nloop < 0x7000) {
        return;
    }
    vaciar_prims();
    etiqueta_prim = act++;
    etiqueta_prim->d[0] = giftag_lo(0, 0, 1, prim, GIF_FLG_PACKED, nreg);
    etiqueta_prim->d[1] = estado.texturizado ? (0x2ULL | (0x1ULL << 4) | (0x4ULL << 8)) : (0x1ULL | (0x4ULL << 4));
    prim_nloop = 0;
    prim_regs = nreg;
}

static inline void emitir_vertice(const VerticeGs *v)
{
    /* Con signo */
    u32 x = (u32) (s32) ((v->x + DESPLAZAMIENTO_X_XY) * 16.0f);
    u32 y = (u32) (s32) ((v->y + DESPLAZAMIENTO_Y_XY) * 16.0f);

    if (prim_regs == 3) {
        act->f[0] = v->s;
        act->f[1] = v->t;
        act->f[2] = v->q;
        act->w[3] = 0;
        act++;
    }
    act->w[0] = v->r;
    act->w[1] = v->g;
    act->w[2] = v->b;
    act->w[3] = v->a;
    act++;
    act->d[0] = (u64) (x & 0xFFFF) | ((u64) (y & 0xFFFF) << 32);
    act->d[1] = ((u64) (v->z & 0xFFFFFF) << 4) | ((u64) v->niebla << 36);
    act++;
    prim_nloop++;
}

/* Los tres qwords de emitir_vertice ya armados (ST, RGBAQ, XYZF2) */
void vertice_paquete_gs(const VerticeGs *v, u128 salida[3])
{
    Qword *q = (Qword *) salida;
    u32 x = (u32) (s32) ((v->x + DESPLAZAMIENTO_X_XY) * 16.0f);
    u32 y = (u32) (s32) ((v->y + DESPLAZAMIENTO_Y_XY) * 16.0f);

    q[0].f[0] = v->s;
    q[0].f[1] = v->t;
    q[0].f[2] = v->q;
    q[0].w[3] = 0;
    q[1].w[0] = v->r;
    q[1].w[1] = v->g;
    q[1].w[2] = v->b;
    q[1].w[3] = v->a;
    q[2].d[0] = (u64) (x & 0xFFFF) | ((u64) (y & 0xFFFF) << 32);
    q[2].d[1] = ((u64) (v->z & 0xFFFFFF) << 4) | ((u64) v->niebla << 36);
}

void gs_triangulo_empaquetado(const u128 *a, const u128 *b, const u128 *c)
{
    reservar(12);
    abrir_prims(PRIM_TRIANGLE);
    if (prim_regs == 3) {
        act[0].q = a[0];
        act[1].q = a[1];
        act[2].q = a[2];
        act[3].q = b[0];
        act[4].q = b[1];
        act[5].q = b[2];
        act[6].q = c[0];
        act[7].q = c[1];
        act[8].q = c[2];
        act += 9;
    } else {
        act[0].q = a[1];
        act[1].q = a[2];
        act[2].q = b[1];
        act[3].q = b[2];
        act[4].q = c[1];
        act[5].q = c[2];
        act += 6;
    }
    prim_nloop += 3;
    estadisticas_gs.triangulos++;
}

void gs_triangulo(const VerticeGs *v0, const VerticeGs *v1, const VerticeGs *v2)
{
    reservar(12);
    abrir_prims(PRIM_TRIANGLE);
    emitir_vertice(v0);
    emitir_vertice(v1);
    emitir_vertice(v2);
    estadisticas_gs.triangulos++;
}

void gs_sprite(const VerticeGs *tl, const VerticeGs *br)
{
    reservar(10);
    abrir_prims(PRIM_SPRITE);
    emitir_vertice(tl);
    emitir_vertice(br);
    estadisticas_gs.sprites++;
}

void gs_rellenar_rectangulo(int x0, int y0, int x1, int y1, u8 r, u8 g, u8 b)
{
    EstadoGs st = estado;
    EstadoGs guardado = estado;
    int valido = valido_estado;
    VerticeGs a, c;

    st.prueba = GS_SETREG_TEST(0, 0, 0, 0, 0, 0, 1, 1);
    st.zbuf = gs_valor_zbuf(0);
    st.alpha = GS_SETREG_ALPHA(0, 1, 0, 1, 0);
    st.tijera = GS_SETREG_SCISSOR(0, GS_ANCHO - 1, 0, GS_ALTO - 1);
    st.prim = 0;
    st.texturizado = 0;
    st.dither = 0;
    gs_aplicar_estado(&st);
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
    a.niebla = 0xFF;
    c = a;
    c.x = (float) x1;
    c.y = (float) y1;
    gs_sprite(&a, &c);
    if (valido) {
        gs_aplicar_estado(&guardado);
    }
}

void gs_limpiar_z(void)
{
    Qword *etiqueta;
    VerticeGs a, c;

    empezar_ad(&etiqueta, 4);
    ad(GSR_FRAME_1, GS_SETREG_FRAME(s_gs->ZBuffer / 8192, GS_ANCHO / 64, GS_PSM_CT32, 0));
    ad(PRUEBA_GSR_1, GS_SETREG_TEST(0, 0, 0, 0, 0, 0, 1, 1));
    ad(GSR_ZBUF_1, gs_valor_zbuf(0));
    ad(TIJERA_GSR_1, GS_SETREG_SCISSOR(0, GS_ANCHO - 1, 0, GS_ALTO - 1));
    valido_estado = 0;
    memset(&a, 0, sizeof(a));
    a.niebla = 0xFF;
    c = a;
    c.x = GS_ANCHO;
    c.y = GS_ALTO;
    estado.prim = 0;
    estado.texturizado = 0;
    gs_sprite(&a, &c);
    empezar_ad(&etiqueta, 1);
    correccion_frame_nota(act);
    ad(GSR_FRAME_1, frame_reg(buffer_dibujo));
}

void gs_copiar_desde_pantalla(float x, float y, float w, float h, u32 dst_vram, int dw, int dh)
{
    Qword *etiqueta;
    /* El frame anterior */
    u32 orig_ = s_gs->ScreenBuffer[buffer_mostrado_gs()] / 256;
    union {
        float f;
        u32 u;
    } s0, t0, s1, t1, q;

    vaciar_prims();
    s0.f = x / 1024.0f;
    t0.f = y / 512.0f;
    s1.f = (x + w) / 1024.0f;
    t1.f = (y + h) / 512.0f;
    q.f = 1.0f;
    empezar_ad(&etiqueta, 16);
    ad(GSR_FRAME_1, GS_SETREG_FRAME(dst_vram / 8192, 1, GS_PSM_CT32, 0));
    ad(GSR_ZBUF_1, gs_valor_zbuf(0));
    ad(PRUEBA_GSR_1, GS_SETREG_TEST(0, 0, 0, 0, 0, 0, 1, 1));
    ad(TIJERA_GSR_1, GS_SETREG_SCISSOR(0, dw - 1, 0, dh - 1));
    ad(GSR_ALPHA_1, GS_SETREG_ALPHA(0, 1, 0, 1, 0));
    ad(GSR_TEX0_1, GS_SETREG_TEX0(orig_, GS_ANCHO / 64, s_gs->PSM, 10, 9, 0, 1, 0, 0, 0, 0, 0));
    ad(GSR_TEX1_1, GS_SETREG_TEX1(1, 0, 1, 1, 0, 0, 0));
    ad(LIMITE_GSR_1, GS_SETREG_CLAMP(2, 2, (int) x, (int) (x + w) - 1, (int) y, (int) (y + h) - 1));
    ad(GSR_PRIM, 6 | (1 << 4)); /* sprite con textura (ST) */
    ad(GSR_RGBAQ, 0x80808080ULL | ((u64) q.u << 32));
    ad(GSR_ST, (u64) s0.u | ((u64) t0.u << 32));
    ad(GSR_XYZ2, (u64) (DESPLAZAMIENTO_X_XY << 4) | ((u64) (DESPLAZAMIENTO_Y_XY << 4) << 16));
    ad(GSR_ST, (u64) s1.u | ((u64) t1.u << 32));
    ad(GSR_XYZ2, (u64) ((DESPLAZAMIENTO_X_XY + dw) << 4) | ((u64) ((DESPLAZAMIENTO_Y_XY + dh) << 4) << 16));
    /* De vuelta al framebuffer del frame */
    correccion_frame_nota(act);
    ad(GSR_FRAME_1, frame_reg(buffer_dibujo));
    ad(TIJERA_GSR_1, GS_SETREG_SCISSOR(0, GS_ANCHO - 1, 0, GS_ALTO - 1));
    valido_estado = 0;
}

#ifdef SMK64_DEV
/* Copia la imagen en pantalla (CT16S) a la zona del Z-buffer como CT32: ps2_screenshot no lee CT16S.
   Solo entre frames: el Z-buffer se vuelve a limpiar al empezar el siguiente. */
u32 gs_pantalla_a_ct32(void)
{
    static Qword pkt[16] __attribute__((aligned(64)));
    Qword *q = pkt;
    u32 orig = s_gs->ScreenBuffer[buffer_pantalla] / 256;

    q->d[0] = giftag_lo(14, 1, 0, 0, GIF_FLG_PACKED, 1);
    q->d[1] = GIF_REG_AD;
    q++;
#define AD_DEV(r, v) (q->d[0] = (v), q->d[1] = (r), q++)
    AD_DEV(GSR_FRAME_1, GS_SETREG_FRAME(s_gs->ZBuffer / 8192, GS_ANCHO / 64, GS_PSM_CT32, 0));
    AD_DEV(GSR_ZBUF_1, gs_valor_zbuf(0));
    AD_DEV(PRUEBA_GSR_1, GS_SETREG_TEST(0, 0, 0, 0, 0, 0, 1, 1));
    AD_DEV(GSR_XYOFFSET_1, GS_SETREG_XYOFFSET(DESPLAZAMIENTO_X_XY << 4, DESPLAZAMIENTO_Y_XY << 4));
    AD_DEV(TIJERA_GSR_1, GS_SETREG_SCISSOR(0, GS_ANCHO - 1, 0, GS_ALTO - 1));
    AD_DEV(GSR_TEX0_1, GS_SETREG_TEX0(orig, GS_ANCHO / 64, GS_PSM_CT16S, 10, 9, 0, 1, 0, 0, 0, 0, 0));
    AD_DEV(GSR_TEX1_1, GS_SETREG_TEX1(1, 0, 0, 0, 0, 0, 0));
    AD_DEV(LIMITE_GSR_1, GS_SETREG_CLAMP(1, 1, 0, 0, 0, 0));
    AD_DEV(GSR_ALPHA_1, GS_SETREG_ALPHA(0, 1, 0, 1, 0));
    AD_DEV(GSR_PRIM, 6 | (1 << 4) | (1 << 8)); /* sprite, textura, coordenadas UV */
    AD_DEV(GSR_UV, 0);
    AD_DEV(GSR_XYZ2, (u64) (DESPLAZAMIENTO_X_XY << 4) | ((u64) (DESPLAZAMIENTO_Y_XY << 4) << 16));
    AD_DEV(GSR_UV, (u64) (GS_ANCHO << 4) | ((u64) (GS_ALTO << 4) << 16));
    AD_DEV(GSR_XYZ2, (u64) ((DESPLAZAMIENTO_X_XY + GS_ANCHO) << 4) | ((u64) ((DESPLAZAMIENTO_Y_XY + GS_ALTO) << 4) << 16));
#undef AD_DEV
    FlushCache(0);
    esperar_gif();
    dmaKit_send(DMA_CHANNEL_GIF, pkt, (u32) (q - pkt));
    esperar_gif();
    valido_estado = 0;
    return s_gs->ZBuffer;
}
#endif

void gs_subir_textura(u32 tbp, u32 tbw, const u32 *pixeles, u32 w, u32 h)
{
    subir_rect_textura_gs(tbp, tbw, 0, 0, pixeles, w, h);
}

void subir_rect_textura_gs(u32 tbp, u32 tbw, u32 x, u32 y, const u32 *pixeles, u32 w, u32 h)
{
    subir_imagen_gs(tbp, tbw, GS_PSM_CT32, x, y, pixeles, w, h, w * h * 4);
}

void subir_imagen_gs(u32 tbp, u32 tbw, u32 psm, u32 x, u32 y, const void *pixeles, u32 w, u32 h, u32 bytes)
{
    Qword *etiqueta;
    u32 qwc = (bytes + 15) / 16;

    /* Todo de una vez */
    vaciar_prims();
    reservar(qwc + 12);
    empezar_ad(&etiqueta, 4);
    ad(GSR_BITBLTBUF, ((u64) tbp << 32) | ((u64) tbw << 48) | ((u64) psm << 56));
    ad(GSR_TRXPOS, ((u64) (x & 0x7FF) << 32) | ((u64) (y & 0x7FF) << 48));
    ad(GSR_TRXREG, (u64) w | ((u64) h << 32));
    ad(GSR_TRXDIR, 0);
    reservar(qwc + 4);
    act->d[0] = giftag_lo(qwc, 0, 0, 0, GIF_FLG_IMAGE, 0);
    act->d[1] = 0;
    act++;
    memcpy(act, pixeles, qwc * 16);
    act += qwc;
    empezar_ad(&etiqueta, 1);
    ad(GSR_TEXFLUSH, 0);
    /* Cabecera A+D (5), etiqueta IMAGE (1), datos y TEXFLUSH (2). */
    capturar_2(act - (qwc + 8), qwc + 8);
    estadisticas_gs.subidas++;
    estadisticas_gs.bytes_subidos += qwc * 16;
}
