#include <kernel.h>
#include <stdio.h>
#include <string.h>
#include <timer.h>

#include <ultra64.h>

#include "sistema/sistema_ps2.h"

#define DESPLAZAMIENTO_CUBOS  4
#define CANTIDAD_CUBOS ((1600 * 1024) >> DESPLAZAMIENTO_CUBOS)

#define RELOJ_BUS_256 2
#ifdef SMK64_PROF
#define PERIODO_MUESTREO 288
#else
#define PERIODO_MUESTREO 576
#endif

#define TMODE_ZRET (1 << 6)
#define TMODE_CUE  (1 << 7)
#define TMODE_CMPE (1 << 8)
#define TMODE_EQUF (1 << 10)
#define TMODE_OVFF (1 << 11)

extern char _ftext[];

static int manejador_muestreo = -1;
static u32 inicio_bucle_inactivo, largo_bucle_inactivo;
static volatile s32 hilo_inactivo = -1;
static volatile u32 muestras_totales, muestras_inactivo;
static volatile u32 muestras_por_hilo[HILOS_MUESTREO];

#ifdef SMK64_MEDIDOR
/* On-screen profile (meter page): busy samples per 128 bytes of code */
#define PERFIL_DESPL 7
#define PERFIL_CUBOS ((1536 * 1024) >> PERFIL_DESPL)
static u32 perfil_cubos[PERFIL_CUBOS];
static volatile u32 perfil_total, perfil_kernel;
static volatile int perfil_activo_pantalla;
#endif

static void contar_muestra_cpu(u32 epc)
{
    s32 th = _iGetThreadId();
    int inactivo = th == hilo_inactivo || epc - inicio_bucle_inactivo < largo_bucle_inactivo;

    muestras_totales++;
    if (inactivo) {
        muestras_inactivo++;
    }
    muestras_por_hilo[(th > 0 && th < HILOS_MUESTREO) ? th : 0]++;
#ifdef SMK64_MEDIDOR
    if (perfil_activo_pantalla && !inactivo) {
        u32 apagado = epc - (u32) _ftext;

        perfil_total++;
        if (epc >= 0x80000000u) {
            perfil_kernel++;
        } else if ((apagado >> PERFIL_DESPL) < PERFIL_CUBOS) {
            perfil_cubos[apagado >> PERFIL_DESPL]++;
        }
    }
#endif
}

#ifdef SMK64_MEDIDOR
void perfil_pantalla_empezar(void)
{
    perfil_activo_pantalla = 0;
    memset(perfil_cubos, 0, sizeof(perfil_cubos));
    perfil_total = perfil_kernel = 0;
    perfil_activo_pantalla = 1;
}

/* The n hottest 128-byte code blocks: address and samples, hottest first */
int perfil_pantalla_top(u32 *dir, u32 *muestras, int n, u32 *total, u32 *kernel)
{
    int i, k, cuantos = 0;

    for (i = 0; i < PERFIL_CUBOS; i++) {
        u32 v = perfil_cubos[i];

        if (v == 0 || (cuantos == n && v <= muestras[n - 1])) {
            continue;
        }
        k = cuantos < n ? cuantos++ : n - 1;
        for (; k > 0 && muestras[k - 1] < v; k--) {
            muestras[k] = muestras[k - 1];
            dir[k] = dir[k - 1];
        }
        muestras[k] = v;
        dir[k] = (u32) _ftext + ((u32) i << PERFIL_DESPL);
    }
    *total = perfil_total;
    *kernel = perfil_kernel;
    return cuantos;
}
#endif

#ifndef SMK64_PROF
static s32 muestreador_manejador(s32 causa)
{
    u32 epc;

    (void) causa;
    __asm__ __volatile__("mfc0 %0, $14" : "=r"(epc));
    *T1_MODE = RELOJ_BUS_256 | TMODE_ZRET | TMODE_CUE | TMODE_CMPE | TMODE_EQUF;
    contar_muestra_cpu(epc);
    ExitHandler();
    return 0;
}
#endif

static void iniciar_temporizador_muestreo(void);

void registrar_bucle_inactivo(u32 empezar, u32 largo)
{
    inicio_bucle_inactivo = empezar;
    largo_bucle_inactivo = largo;
    hilo_inactivo = GetThreadId(); /* lo llama el propio bucle inactivo */
    iniciar_temporizador_muestreo();
}

void cantidades_muestreador_ps2(u32 *total, u32 *inactivo)
{
    *total = muestras_totales;
    *inactivo = muestras_inactivo;
}

void leer_muestras_cpu(MuestrasCpu *salida)
{
    int i;

    salida->total = muestras_totales;
    salida->inactivo = muestras_inactivo;
    for (i = 0; i < HILOS_MUESTREO; i++) {
        salida->thread[i] = muestras_por_hilo[i];
    }
}

#ifdef SMK64_PROF

static u32 cubos[CANTIDAD_CUBOS];
static u32 cubos_llamador[CANTIDAD_CUBOS];
static volatile u32 llamador;
static volatile int dentro_de_rutina;

void *__real_memcpy(void *dst, const void *orig_, unsigned int n);
void *__real_memset(void *dst, int c, unsigned int n);

void *__wrap_memcpy(void *dst, const void *orig_, unsigned int n)
{
    int ant = dentro_de_rutina;
    u32 llamador_ant = llamador;

    llamador = (u32) __builtin_return_address(0);
    dentro_de_rutina = 1;
    __real_memcpy(dst, orig_, n);
    dentro_de_rutina = ant;
    llamador = llamador_ant;
    return dst;
}

void *__wrap_memset(void *dst, int c, unsigned int n)
{
    int ant = dentro_de_rutina;
    u32 llamador_ant = llamador;

    llamador = (u32) __builtin_return_address(0);
    dentro_de_rutina = 1;
    __real_memset(dst, c, n);
    dentro_de_rutina = ant;
    llamador = llamador_ant;
    return dst;
}

/* Doble precision por software (el EE no tiene FPU de 64 bits) */
#define ENVOLTURA_SUAVE_A(devuelto, nombre, params, parametros, objetivo) \
    devuelto objetivo params;                                \
    devuelto __wrap_##nombre params                          \
    {                                                 \
        int ant = dentro_de_rutina;                           \
        u32 llamador_ant = llamador;                     \
        devuelto r;                                        \
                                                      \
        llamador = (u32) __builtin_return_address(0);  \
        dentro_de_rutina = 1;                                  \
        r = objetivo parametros;                              \
        dentro_de_rutina = ant;                               \
        llamador = llamador_ant;                         \
        return r;                                     \
    }
#define ENVOLVER_DOUBLE_SOFTWARE(devuelto, nombre, params, parametros) ENVOLTURA_SUAVE_A(devuelto, nombre, params, parametros, rapido_##nombre)

ENVOLVER_DOUBLE_SOFTWARE(double, __adddf3, (double a, double b), (a, b))
ENVOLVER_DOUBLE_SOFTWARE(double, __subdf3, (double a, double b), (a, b))
ENVOLVER_DOUBLE_SOFTWARE(double, __muldf3, (double a, double b), (a, b))
ENVOLTURA_SUAVE_A(double, __divdf3, (double a, double b), (a, b), __real___divdf3)
ENVOLVER_DOUBLE_SOFTWARE(double, __extendsfdf2, (float a), (a))
ENVOLVER_DOUBLE_SOFTWARE(float, __truncdfsf2, (double a), (a))
ENVOLVER_DOUBLE_SOFTWARE(double, __floatsidf, (int a), (a))
ENVOLVER_DOUBLE_SOFTWARE(int, __fixdfsi, (double a), (a))
ENVOLVER_DOUBLE_SOFTWARE(int, __ltdf2, (double a, double b), (a, b))
ENVOLVER_DOUBLE_SOFTWARE(int, __gtdf2, (double a, double b), (a, b))
ENVOLVER_DOUBLE_SOFTWARE(int, __ledf2, (double a, double b), (a, b))
ENVOLVER_DOUBLE_SOFTWARE(int, __gedf2, (double a, double b), (a, b))

double __real___extendsfdf2(float a);
#define EN_RUTINAS_DOUBLE(pc) ((pc) - (u32) __real___extendsfdf2 < 0x3000u)
static volatile u32 muestras_kernel, muestras_fuera, s_total;
static volatile int perfil_activo;

static s32 muestreador_manejador(s32 causa)
{
    u32 epc;

    (void) causa;
    __asm__ __volatile__("mfc0 %0, $14" : "=r"(epc));
    *T1_MODE = RELOJ_BUS_256 | TMODE_ZRET | TMODE_CUE | TMODE_CMPE | TMODE_EQUF;
    contar_muestra_cpu(epc);
    if (perfil_activo) {
        u32 apagado = epc - (u32) _ftext;

        s_total++;
        if (epc >= 0x80000000u) {
            muestras_kernel++;
        } else if ((apagado >> DESPLAZAMIENTO_CUBOS) < CANTIDAD_CUBOS) {
            cubos[apagado >> DESPLAZAMIENTO_CUBOS]++;
            if (dentro_de_rutina &&
                (epc - (u32) __real_memcpy < 0x400 || epc - (u32) __real_memset < 0x400 || EN_RUTINAS_DOUBLE(epc))) {
                u32 coff = llamador - (u32) _ftext;

                if ((coff >> DESPLAZAMIENTO_CUBOS) < CANTIDAD_CUBOS) {
                    cubos_llamador[coff >> DESPLAZAMIENTO_CUBOS]++;
                }
            }
        } else {
            muestras_fuera++;
        }
    }
    ExitHandler();
    return 0;
}

void iniciar_perfil_muestreo(void)
{
    perfil_activo = 0;
    memset(cubos, 0, sizeof(cubos));
    memset(cubos_llamador, 0, sizeof(cubos_llamador));
    muestras_kernel = muestras_fuera = s_total = 0;
    iniciar_temporizador_muestreo();
    perfil_activo = 1;
}
#endif

static void iniciar_temporizador_muestreo(void)
{
    if (manejador_muestreo < 0) {
        *T1_MODE = 0;
        *T1_COUNT = 0;
        *T1_COMP = PERIODO_MUESTREO;
        manejador_muestreo = AddIntcHandler(INTC_TIM1, muestreador_manejador, 0);
        EnableIntc(INTC_TIM1);
        *T1_MODE = RELOJ_BUS_256 | TMODE_ZRET | TMODE_CUE | TMODE_CMPE | TMODE_EQUF | TMODE_OVFF;
    }
}

#ifdef SMK64_PROF

static void escribir_tabla_muestras(FILE *f, const char *magico, const u32 *buckets)
{
    u32 i, pairs = 0;
    u32 hdr[7];

    for (i = 0; i < CANTIDAD_CUBOS; i++) {
        pairs += buckets[i] != 0;
    }
    memcpy(&hdr[0], magico, 4);
    hdr[1] = s_total;
    hdr[2] = muestras_kernel;
    hdr[3] = muestras_fuera;
    hdr[4] = DESPLAZAMIENTO_CUBOS;
    hdr[5] = (u32) _ftext;
    hdr[6] = pairs;
    fwrite(hdr, sizeof(hdr), 1, f);
    for (i = 0; i < CANTIDAD_CUBOS; i++) {
        if (buckets[i] != 0) {
            u32 pair[2] = { i, buckets[i] };

            fwrite(pair, sizeof(pair), 1, f);
        }
    }
}

void terminar_perfil_muestreo(const char *nombre)
{
    char path[80];
    FILE *f;

    perfil_activo = 0;
    snprintf(path, sizeof(path), "host:perfil_%s.bin", nombre);
    bloquear_host();
    f = fopen(path, "wb");
    if (f != NULL) {
        escribir_tabla_muestras(f, "SMPL", cubos);
        escribir_tabla_muestras(f, "CALL", cubos_llamador);
        fclose(f);
    }
    desbloquear_host();
    registrar("perfil %s: %u muestras (kernel %u, fuera %u)", nombre, (unsigned) s_total, (unsigned) muestras_kernel,
            (unsigned) muestras_fuera);
}
#endif
