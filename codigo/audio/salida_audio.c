#include <audsrv.h>
#include <kernel.h>
#include <loadfile.h>
#include <math.h>
#include <sifrpc.h>
#include <stdio.h>
#include <string.h>

#include <ultra64.h>
#include <PR/os.h>

#include "sistema/sistema_ps2.h"
#include "audio/salida_audio.h"
#include "audio/microcodigo_audio.h"
#ifdef SMK64_MEDIDOR
#include "depuracion/medidor_rendimiento.h"
#endif

/* Modulos del IOP embebidos (ver Makefile). */
extern u8 irx_libsd[];
extern u32 tamanio_irx_libsd;
extern u8 irx_audsrv[];
extern u32 tamanio_irx_audsrv;
/* Datos del microcodigo de audio (big-endian). */
extern u8 datos_microcodigo_audio[];

/* Contadores del reparto de notas del motor (reproduccion.c) */
u32 notas_robadas, sonidos_sin_nota, notas_tomadas_apagandose;

#define TASA_SALIDA 48000
/* Reloj del VI del N64 NTSC */
#define RELOJ_VI_N64 48681812u

static int audio_ok;
static EstadisticasAudioPs2 estadisticas;

static inline u32 ciclos_ee(void)
{
    u32 c;

    __asm__ volatile("mfc0 %0, $9" : "=r"(c));
    return c;
}
/* Frecuencia real del conversor del N64, en 1/1024 Hz. */
static u32 en_tasa_1024 = 26800u << 10;

void ejecutar_tarea_ps2_audio(u64 *lista_cmd, u32 bytes_tamanio)
{
    u32 inicio, us;

    if (bytes_tamanio == 0) {
        return;
    }
    inicio = ciclos_ee();
    ejecutar_tarea_aspmain(lista_cmd, bytes_tamanio);
    us = (ciclos_ee() - inicio) / 295;
    if (us > estadisticas.max_us_tarea) {
        estadisticas.max_us_tarea = us;
    }
#ifdef SMK64_MEDIDOR
    medidor_sumar_audio(us * 295);
#endif
#ifdef SMK64_DEBUG_AUDIO
    {
        extern void ps2_audio_monitor_frame(void);

        ps2_audio_monitor_frame();
    }
#endif
}

void obtener_estadisticas_audio_ps2(EstadisticasAudioPs2 *salida)
{
    *salida = estadisticas;
}

#define RS_TAPS 32
#define RS_PHASES 256
#define RS_HIST (RS_TAPS - 1)
static s16 coef[RS_PHASES + 1][RS_TAPS] __attribute__((aligned(16)));
#define SALIDA_EN_MAX 2048
#define FRAMES_MAX_SALIDA 4096 /* salida de un bloque (48 kHz) */
static s16 hist[(RS_HIST + SALIDA_EN_MAX) * 2 + 8] __attribute__((aligned(16)));
static u64 s_pos; /* posicion en la entrada, 32.32, relativa a hist */

/* Version MMI (la que se usa */
#if defined(_EE) && !defined(PS2_AUDIO_NO_MMI)
#define PS2_AUDIO_MMI 1
static s16 s_dif[RS_PHASES][RS_TAPS] __attribute__((aligned(16)));
/* Copias desplazadas 1-3 muestras (la 0 es hist) */
static s16 hist_4[4][(RS_HIST + SALIDA_EN_MAX + 1) * 2] __attribute__((aligned(16)));
static int mmi_remuestreo = 1;
#define RS_COEF(g, reg)                         \
    "lq     $8, " #g "*16(%[dd])\n\t"           \
    "pmulth $10, $8, $9\n\t"                    \
    "psraw  $11, $8, 16\n\t"                    \
    "pmulth $11, $11, $9\n\t"                   \
    "psraw  $10, $10, 8\n\t"                    \
    "psraw  $11, $11, 8\n\t"                    \
    "pinteh $10, $11, $10\n\t"                  \
    "lq     $8, " #g "*16(%[c0])\n\t"           \
    "paddh  " reg ", $8, $10\n\t"
/* Las tomas del grupo g */
#define RS_TAPS8(reg, g, op)                    \
    "pextlh $8, " reg ", " reg "\n\t"           \
    "lq     $10, " #g "*32(%[h])\n\t"           \
    op "  $10, $10, $8\n\t"                     \
    "pextuh $8, " reg ", " reg "\n\t"           \
    "lq     $10, " #g "*32+16(%[h])\n\t"        \
    "pmaddh $10, $10, $8\n\t"
static const u32 s_k_4000[4] __attribute__((aligned(16))) = { 0x4000, 0x4000, 0x4000, 0x4000 };
static const u32 sk7_fff[4] __attribute__((aligned(16))) = { 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF };

/* dst (alineado a 16) <- src (alineado a 4), de 16 en 16 bytes */
static void copiar_a_alineado(s16 *dst, const s16 *orig_, s32 bytes)
{
    __asm__ __volatile__(
        "1:\n\t"
        "ldl    $8, 7(%[s])\n\t"
        "ldr    $8, 0(%[s])\n\t"
        "ldl    $9, 15(%[s])\n\t"
        "ldr    $9, 8(%[s])\n\t"
        "pcpyld $8, $9, $8\n\t"
        "sq     $8, 0(%[d])\n\t"
        "addiu  %[s], %[s], 16\n\t"
        "addiu  %[d], %[d], 16\n\t"
        "addiu  %[n], %[n], -16\n\t"
        "bgtz   %[n], 1b\n\t"
        : [s] "+r"(orig_), [d] "+r"(dst), [n] "+r"(bytes)
        :
        : "$8", "$9", "memory");
}
#endif

static float bessel_i0(float x)
{
    float sum = 1.0f, term = 1.0f;
    int k;

    for (k = 1; k < 30; k++) {
        term *= (x / (2.0f * k)) * (x / (2.0f * k));
        sum += term;
    }
    return sum;
}

static void inicializar_resampler(void)
{
    const float beta = 8.0f;
    const float cutoff = 12600.0f / 26807.0f; /* respecto de la frecuencia de entrada */
    float i0beta = bessel_i0(beta);
    int p, t;

    for (p = 0; p <= RS_PHASES; p++) {
        float frac = (float) p / RS_PHASES;
        float c[RS_TAPS], sum = 0.0f;
        int acc = 0;

        for (t = 0; t < RS_TAPS; t++) {
            float x = (float) (t - (RS_TAPS / 2 - 1)) - frac;
            float r = x / (RS_TAPS / 2.0f);
            float w = (r * r < 1.0f) ? bessel_i0(beta * sqrtf(1.0f - r * r)) / i0beta : 0.0f;
            float s = (x == 0.0f) ? cutoff : sinf((float) M_PI * cutoff * x) / ((float) M_PI * x);

            c[t] = s * w;
            sum += c[t];
        }
        /* Ganancia exacta 1 en continua, con redondeo que suma 32768. */
        for (t = 0; t < RS_TAPS; t++) {
            int q = (int) lrintf(c[t] / sum * 32768.0f);

            coef[p][t] = (s16) q;
            acc += q;
        }
        coef[p][RS_TAPS / 2 - 1 + (frac >= 0.5f)] += (s16) (32768 - acc);
    }
#ifdef PS2_AUDIO_MMI
    for (p = 0; p < RS_PHASES; p++) {
        for (t = 0; t < RS_TAPS; t++) {
            int d = coef[p + 1][t] - coef[p][t];

            s_dif[p][t] = (s16) d;
            if (d != s_dif[p][t]) {
                mmi_remuestreo = 0; /* no cabe en 16 bits: no pasa con estas tablas */
            }
        }
    }
#endif
    memset(hist, 0, sizeof(hist));
    s_pos = 0;
}

static inline s16 sat16(s32 v)
{
    return v < -0x8000 ? -0x8000 : (v > 0x7FFF ? 0x7FFF : (s16) v);
}

static u32 remuestrear(const s16 *in, u32 frames, s16 *salida, u32 max_salida)
{
    u64 paso = ((u64) en_tasa_1024 << 22) / TASA_SALIDA; /* entrada por salida, 32.32 */
    u32 avail = RS_HIST + frames;                   /* muestras en hist */
    u32 n = 0;

    memcpy(&hist[RS_HIST * 2], in, frames * 4);
#ifdef PS2_AUDIO_MMI
    if (mmi_remuestreo) {
        int k;

        for (k = 1; k < 4; k++) {
            copiar_a_alineado(hist_4[k], &hist[k * 2], (avail - k) * 4);
        }
        while ((u32) (s_pos >> 32) + RS_TAPS <= avail && n < max_salida) {
            u32 idx = (u32) (s_pos >> 32);
            u32 fr = (u32) (s_pos >> 16) & 0xFFFF;
            u32 mezclar = fr & 0xFF;
            const s16 *h = (idx & 3) ? &hist_4[idx & 3][(idx & ~3u) * 2] : &hist[idx * 2];

            __asm__ __volatile__(
                "pcpyld $9, %[mezclar], %[mezclar]\n\t"
                "pcpyh  $9, $9\n\t"
                RS_COEF(0, "$12")
                RS_COEF(1, "$13")
                RS_COEF(2, "$14")
                RS_COEF(3, "$15")
                RS_TAPS8("$12", 0, "pmulth")
                RS_TAPS8("$13", 1, "pmaddh")
                RS_TAPS8("$14", 2, "pmaddh")
                RS_TAPS8("$15", 3, "pmaddh")
                "pmfhl.lw $8\n\t"
                "pmfhl.uw $10\n\t"
                "pextlw $11, $10, $8\n\t"
                "pextuw $8, $10, $8\n\t"
                "paddw  $11, $11, $8\n\t"
                "pcpyud $8, $11, $11\n\t"
                "paddw  $11, $11, $8\n\t"
                "lq     $8, 0(%[kr])\n\t"
                "paddw  $11, $11, $8\n\t"
                "psraw  $11, $11, 15\n\t"
                "lq     $8, 0(%[kw])\n\t"
                "pminw  $11, $11, $8\n\t"
                "pnor   $8, $8, $0\n\t"
                "pmaxw  $11, $11, $8\n\t"
                "ppach  $11, $0, $11\n\t"
                "sw     $11, 0(%[o])\n\t"
                :
                : [mezclar] "r"(mezclar), [dd] "r"(s_dif[fr >> 8]), [c0] "r"(coef[fr >> 8]), [h] "r"(h),
                  [o] "r"(&salida[n * 2]), [kr] "r"(s_k_4000), [kw] "r"(sk7_fff)
                : "$8", "$9", "$10", "$11", "$12", "$13", "$14", "$15", "hi", "lo", "memory");
            n++;
            s_pos += paso;
        }
    } else
#endif
    /* Hacen falta RS_TAPS muestras desde la posicion entera. */
    while ((u32) (s_pos >> 32) + RS_TAPS <= avail && n < max_salida) {
        u32 idx = (u32) (s_pos >> 32);
        u32 fr = (u32) (s_pos >> 16) & 0xFFFF; /* 16 bits de fraccion */
        u32 ph = fr >> 8, mezclar = fr & 0xFF;    /* fase y peso hacia la siguiente */
        const s16 *c0 = coef[ph], *c1 = coef[ph + 1];
        const s16 *h = &hist[idx * 2];
        s32 l = 0, r = 0;
        int t;

        for (t = 0; t < RS_TAPS; t++) {
            s32 c = c0[t] + (((c1[t] - c0[t]) * (s32) mezclar) >> 8);

            l += h[t * 2] * c;
            r += h[t * 2 + 1] * c;
        }
        salida[n * 2] = sat16((l + 0x4000) >> 15);
        salida[n * 2 + 1] = sat16((r + 0x4000) >> 15);
        n++;
        s_pos += paso;
    }
    /* Las ultimas RS_HIST muestras pasan al principio */
    if ((s_pos >> 32) < avail - RS_HIST) {
        s_pos = (u64) (avail - RS_HIST) << 32;
    }
    memmove(hist, &hist[(avail - RS_HIST) * 2], RS_HIST * 4);
    s_pos -= (u64) (avail - RS_HIST) << 32;
    return n;
}

#if defined(PS2_AUDIO_MMI) && defined(SMK64_DEV)
/* El remuestreo MMI contra el C */
static int remuestrear_autoprueba(void)
{
    static s16 in[SALIDA_EN_MAX * 2] __attribute__((aligned(16)));
    static s16 salida_c[FRAMES_MAX_SALIDA * 2], salida_m[FRAMES_MAX_SALIDA * 2];
    static s16 hist_antes[RS_HIST * 2], hist_c[RS_HIST * 2];
    u32 rnd = 0x9E3779B9;
    int round, errores = 0;

    for (round = 0; round < 64; round++) {
        u32 frames = 1 + (round * 997u) % SALIDA_EN_MAX, i, n_c, n_m;
        u64 pos_antes = s_pos, pos_c;

        for (i = 0; i < frames * 2; i++) {
            rnd ^= rnd << 13;
            rnd ^= rnd >> 17;
            rnd ^= rnd << 5;
            in[i] = (round & 3) == 0 ? ((rnd & 1) ? 0x7FFF : -0x8000) : (s16) rnd;
        }
        memcpy(hist_antes, hist, sizeof(hist_antes));
        mmi_remuestreo = 0;
        n_c = remuestrear(in, frames, salida_c, FRAMES_MAX_SALIDA);
        pos_c = s_pos;
        memcpy(hist_c, hist, sizeof(hist_c));
        memcpy(hist, hist_antes, sizeof(hist_antes));
        s_pos = pos_antes;
        mmi_remuestreo = 1;
        n_m = remuestrear(in, frames, salida_m, FRAMES_MAX_SALIDA);
        if (n_c != n_m || memcmp(salida_c, salida_m, n_c * 4) != 0 || s_pos != pos_c ||
            memcmp(hist_c, hist, sizeof(hist_c)) != 0) {
            if (errores++ < 8) {
                u32 k = 0;

                while (k < n_c * 2 && salida_c[k] == salida_m[k]) {
                    k++;
                }
                registrar("remuestreo MMI: difiere el bloque %d (%u muestras: %u/%u salidas, primera distinta %u: %d/%d)",
                        round, (unsigned) frames, (unsigned) n_c, (unsigned) n_m, (unsigned) k / 2,
                        k < n_c * 2 ? salida_c[k] : 0, k < n_c * 2 ? salida_m[k] : 0);
            }
        }
    }
    memset(hist, 0, sizeof(hist));
    s_pos = 0;
    registrar("remuestreo MMI: autoprueba 64 bloques: %d diferencias", errores);
    return errores;
}
#endif

static s16 s_salida[FRAMES_MAX_SALIDA * 2] __attribute__((aligned(64)));
static int tamanio_anillo;

#ifdef SMK64_DEV
/* Captura de los primeros segundos de audio en host */
static FILE *capturar;
static u32 capturado;
static u32 limite_capturar = TASA_SALIDA * 4 * 20; /* los 20 s del arranque */
static char nombre_capturar[64] = "host:audio_48k_s16le_stereo.raw";
static volatile int pedido_capturar;
#endif

void inicializar_ps2_audio(void)
{
    struct audsrv_fmt_t fmt;
    int devuelto;

    inicializar_aspmain(datos_microcodigo_audio);
    inicializar_resampler();
#if defined(PS2_AUDIO_MMI) && defined(SMK64_DEV)
    /* La version MMI del audio contra la de C (en el registro). */
    autoprueba_aspmain(4000);
    remuestrear_autoprueba();
#endif

    if (!ejecutar_modulo_iop("libsd.irx", irx_libsd, tamanio_irx_libsd) ||
        !ejecutar_modulo_iop("audsrv.irx", irx_audsrv, tamanio_irx_audsrv)) {
        return; /* sin audio, pero sin quedarse esperando a audsrv */
    }
    if (audsrv_init() != 0) {
        registrar("audsrv_init fallo: %s", audsrv_get_error_string());
        return;
    }
    fmt.bits = 16;
    fmt.freq = TASA_SALIDA;
    fmt.channels = 2;
    if (audsrv_set_format(&fmt) != 0) {
        registrar("audsrv_set_format fallo: %s", audsrv_get_error_string());
        return;
    }
    audsrv_set_volume(MAX_VOLUME);
    tamanio_anillo = audsrv_available();
    audio_ok = 1;
    registrar("audio: audsrv listo, anillo %d bytes", tamanio_anillo);
}

/* Como el osAiSetFrequency del N64 */
s32 osAiSetFrequency(u32 frecuencia)
{
    u32 div = (RELOJ_VI_N64 + frecuencia / 2) / frecuencia;

    if (div < 1) {
        div = 1;
    }
    en_tasa_1024 = (u32) (((u64) RELOJ_VI_N64 << 10) / div);
    return (s32) (RELOJ_VI_N64 / div);
}

/* Bytes (en el formato del AI */
#define AI_EXTRA_COLA_BYTES (TASA_SALIDA / 60 * 4 * 2)
static int empezado;
static int ultimo_pico; /* pico del ultimo bloque entregado */
#if defined(SMK64_DEBUG) || defined(SMK64_DEV)
static u32 vaciados, ai_frames;
#endif

u32 osAiGetLength(void)
{
    int en_cola;

    if (!audio_ok) {
        return 0;
    }
    en_cola = tamanio_anillo - audsrv_available();
    estadisticas.vaciados += empezado && en_cola <= 0 && ultimo_pico > 0;
    estadisticas.ms_cola = en_cola > 0 ? (u32) en_cola / (TASA_SALIDA * 4 / 1000) : 0;
#if defined(SMK64_DEBUG) || defined(SMK64_DEV)
    /* Anillo vacio al entregar el bloque siguiente: corte audible. */
    /* Solo cuenta si lo ultimo que sono tenia sonido */
    vaciados += empezado && en_cola <= 0 && ultimo_pico > 0;
    {
        static u32 logged;
        extern s32 estado_juego;

        if (empezado && en_cola <= 0 && ultimo_pico > 0 && logged < 40) {
            logged++;
            rend_registro_ps2("audio: corte en VBlank %u (escena %d, esperas de la ROM %d, pico del bloque anterior %d)",
                    (unsigned) contador_vblank(), (int) estado_juego, esperando_rom_ps2, ultimo_pico);
        }
    }
    if ((++ai_frames % 1800) == 0) {
        rend_registro_ps2("audio: %u cortes en los ultimos %u bloques", (unsigned) vaciados, 1800u);
        vaciados = 0;
    }
#endif
    en_cola -= AI_EXTRA_COLA_BYTES;
    if (en_cola < 0) {
        en_cola = 0;
    }
    /* bytes a 48 kHz -> bytes a la frecuencia del juego */
    return (u32) (((u64) en_cola * en_tasa_1024 / TASA_SALIDA) >> 10) & ~3u;
}

u32 osAiGetStatus(void)
{
    return 0;
}

s32 osAiSetNextBuffer(void *buf, u32 size)
{
    const s16 *in = (const s16 *) buf;
    u32 frames = size / 4;
    u32 frames_salida = 0;
#ifdef SMK64_MEDIDOR
    u32 inicio_medidor = medidor_leer_ciclos();
#endif

    if (!audio_ok || frames == 0) {
        return 0;
    }
    while (frames > 0) {
        u32 trozo = frames > SALIDA_EN_MAX ? SALIDA_EN_MAX : frames;

        frames_salida += remuestrear(in, trozo, &s_salida[frames_salida * 2], FRAMES_MAX_SALIDA - frames_salida);
        in += trozo * 2;
        frames -= trozo;
    }
#ifdef SMK64_MEDIDOR
    medidor_sumar_audio(medidor_leer_ciclos() - inicio_medidor);
#endif

#ifdef SMK64_DEV
    bloquear_host();
    if (pedido_capturar) {
        if (capturar != NULL) {
            fclose(capturar);
            capturar = NULL;
        }
        capturado = 0;
        pedido_capturar = 0;
    }
    if (capturado < limite_capturar) {
        if (capturar == NULL) {
            capturar = fopen(nombre_capturar, "wb");
        }
        if (capturar != NULL) {
            fwrite(s_salida, 4, frames_salida, capturar);
            capturado += frames_salida * 4;
            if (capturado >= limite_capturar) {
                fclose(capturar);
                capturar = NULL;
                desbloquear_host();
                registrar("audio: capturados %u s en %s", (unsigned) (limite_capturar / (TASA_SALIDA * 4)), nombre_capturar);
                bloquear_host();
            }
        }
    }
    desbloquear_host();
#endif
#ifdef SMK64_MEDIDOR
    {
        int encolado = tamanio_anillo - audsrv_available();

        /* Anillo vacio tras un bloque en silencio */
        if (encolado > 0 || ultimo_pico > 0) {
            medidor_cola_audio(encolado > 0 ? (u32) encolado : 0);
        }
    }
#endif
    {
        static int primed;
        static u8 s_silence[AI_EXTRA_COLA_BYTES] __attribute__((aligned(64)));

        if (!primed) {
            primed = 1;
            audsrv_play_audio((const char *) s_silence, sizeof(s_silence));
        }
    }
    audsrv_wait_audio(frames_salida * 4);
    audsrv_play_audio((const char *) s_salida, frames_salida * 4);
    empezado = 1;
    {
        const s16 *o = (const s16 *) s_salida;
        u32 k;

        ultimo_pico = 0;
        for (k = 0; k < frames_salida * 2; k++) {
            int v = o[k] < 0 ? -o[k] : o[k];

            ultimo_pico = v > ultimo_pico ? v : ultimo_pico;
        }
    }
    estadisticas.pico = (s16) (ultimo_pico > 0x7FFF ? 0x7FFF : ultimo_pico);
    estadisticas.blocks++;
    return 0;
}

#ifdef SMK64_DEV
void grabar_salida_audio(const char *nombre, int segundos)
{
    snprintf(nombre_capturar, sizeof(nombre_capturar), "host:%s", nombre);
    limite_capturar = (u32) segundos * TASA_SALIDA * 4;
    pedido_capturar = 1;
}
#endif
