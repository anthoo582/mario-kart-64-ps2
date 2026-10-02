#include <kernel.h>
#include <libpad.h>
#include <loadfile.h>
#include <sifrpc.h>
#include <string.h>

#include <ultra64.h>
#include <PR/os.h>

#include "sistema/sistema_ps2.h"
#include "entrada/multitap.h"
#include "graficos/interprete_f3dex.h"
#include "depuracion/guiones_prueba.h"
#ifdef SMK64_MEDIDOR
#include "depuracion/medidor_rendimiento.h"
#endif

enum { PUERTO_CERRADO, ESTABLE_ESPERA_PUERTO, MODO_ESPERA_PUERTO, LISTO_PUERTO };

/* Un buffer de 256 bytes por conector (puerto y ranura del multitap), alineado a 64 para XPADMAN */
static char buffer_relleno[PUERTOS_MANDO_PS2][RANURAS_MULTITAP][256] __attribute__((aligned(64)));
static u8 estado_conector[PUERTOS_MANDO_PS2][RANURAS_MULTITAP];
static u8 intentos_modo[PUERTOS_MANDO_PS2][RANURAS_MULTITAP];
static OSContPad s_rellenos[MAXCONTROLLERS];
static int rellenos_inicializado;
static int padman_ok;

u8 __osContLastCmd;
u8 __osMaxControllers = MAXCONTROLLERS;

#define CENTRO_PALANCA   128
#define ZONA_MUERTA_PALANCA 24
#define MAX_PALANCA_N64  80 /* recorrido tipico de un stick de N64 en MK64 */

static s8 convertir_eje(u8 crudo)
{
    int v = (int) crudo - CENTRO_PALANCA;
    int sign = (v < 0) ? -1 : 1;

    v *= sign;
    if (v < ZONA_MUERTA_PALANCA) {
        return 0;
    }
    v = (v - ZONA_MUERTA_PALANCA) * MAX_PALANCA_N64 / (127 - ZONA_MUERTA_PALANCA);
    if (v > MAX_PALANCA_N64) {
        v = MAX_PALANCA_N64;
    }
    return (s8) (v * sign);
}

static void convertir(const struct padButtonStatus *in, int analogico, OSContPad *salida)
{
    u16 b = (u16) (0xFFFF ^ in->btns);
    u16 n = 0;
    s8 sx = 0;
    s8 sy = 0;

    if (b & PAD_CROSS)    n |= A_BUTTON;
    if (b & PAD_SQUARE)   n |= B_BUTTON;
    if (b & PAD_L1)       n |= Z_TRIG;
    if (b & PAD_L2)       n |= Z_TRIG;
    if (b & PAD_R1)       n |= R_TRIG;
    if (b & PAD_R2)       n |= R_TRIG;
    if (b & PAD_SELECT)   n |= L_TRIG;
    if (b & PAD_START)    n |= START_BUTTON;
    if (b & PAD_TRIANGLE) n |= U_CBUTTONS;
    if (b & PAD_CIRCLE)   n |= D_CBUTTONS;
    if (b & PAD_UP)       n |= U_JPAD;
    if (b & PAD_DOWN)     n |= D_JPAD;
    if (b & PAD_LEFT)     n |= L_JPAD;
    if (b & PAD_RIGHT)    n |= R_JPAD;

    if (analogico) {
        sx = convertir_eje(in->ljoy_h);
        sy = (s8) -convertir_eje(in->ljoy_v); /* N64: arriba es positivo */

        if (in->rjoy_h < CENTRO_PALANCA - 64) n |= L_CBUTTONS;
        if (in->rjoy_h > CENTRO_PALANCA + 64) n |= R_CBUTTONS;
        if (in->rjoy_v < CENTRO_PALANCA - 64) n |= U_CBUTTONS;
        if (in->rjoy_v > CENTRO_PALANCA + 64) n |= D_CBUTTONS;
    }

    if (sx == 0 && sy == 0) {
        if (b & PAD_LEFT)  sx = -MAX_PALANCA_N64;
        if (b & PAD_RIGHT) sx = MAX_PALANCA_N64;
        if (b & PAD_UP)    sy = MAX_PALANCA_N64;
        if (b & PAD_DOWN)  sy = -MAX_PALANCA_N64;
    }

    salida->button = n;
    salida->stick_x = sx;
    salida->stick_y = sy;
    salida->errno = 0;
}

/* Abre las ranuras que existen ahora: las 4 con multitap, solo la primera sin el. */
static void abrir_conectores(void)
{
    int puerto, ranura;

    for (puerto = 0; puerto < PUERTOS_MANDO_PS2; puerto++) {
        int ranuras = multitap_conectado_ps2(puerto) ? RANURAS_MULTITAP : 1;

        for (ranura = 0; ranura < RANURAS_MULTITAP; ranura++) {
            int abierto = estado_conector[puerto][ranura] != PUERTO_CERRADO;

            if (ranura < ranuras && !abierto) {
                if (padPortOpen(puerto, ranura, buffer_relleno[puerto][ranura])) {
                    estado_conector[puerto][ranura] = ESTABLE_ESPERA_PUERTO;
                } else {
                    registrar("padPortOpen(%d, %d) fallo", puerto, ranura);
                }
            } else if (ranura >= ranuras && abierto) {
                padPortClose(puerto, ranura);
                estado_conector[puerto][ranura] = PUERTO_CERRADO;
            }
        }
    }
}

void inicializar_mandos_ps2(void)
{
    if (rellenos_inicializado) {
        return;
    }
    padman_ok = cargar_modulos_mando_ps2();
    if (padman_ok && padInit(0) != 1) {
        registrar("padInit fallo");
        padman_ok = 0;
    }
    if (padman_ok) {
        inicializar_multitap_ps2();
        abrir_conectores();
    }
    memset(s_rellenos, 0, sizeof(s_rellenos));
    rellenos_inicializado = 1;
    inicializar_guiones_prueba();
}

/* Lee un conector. Devuelve 1 y deja los botones crudos (activos a 1) si hay un mando listo. */
static int sondear_conector(int puerto, int ranura, OSContPad *salida, u32 *crudos)
{
    struct padButtonStatus botones;
    u8 *estado_c = &estado_conector[puerto][ranura];
    int estado;

    salida->button = 0;
    salida->stick_x = 0;
    salida->stick_y = 0;
    salida->errno = CONT_NO_RESPONSE_ERROR;

    if (*estado_c == PUERTO_CERRADO) {
        return 0;
    }

    estado = padGetState(puerto, ranura);
    if (estado == PAD_STATE_DISCONN) {
        *estado_c = ESTABLE_ESPERA_PUERTO;
        return 0;
    }
    if (estado != PAD_STATE_STABLE && estado != PAD_STATE_FINDCTP1) {
        return 0;
    }

    if (*estado_c == ESTABLE_ESPERA_PUERTO) {
        /* Mando recien conectado: modo analogico bloqueado si lo admite */
        if (padInfoMode(puerto, ranura, PAD_MODECUREXID, 0) != 0 &&
            padSetMainMode(puerto, ranura, PAD_MMODE_DUALSHOCK, PAD_MMODE_LOCK) == 1) {
            *estado_c = MODO_ESPERA_PUERTO;
            intentos_modo[puerto][ranura] = 0;
            return 0;
        }
        *estado_c = LISTO_PUERTO;
    } else if (*estado_c == MODO_ESPERA_PUERTO) {
        if (padGetReqState(puerto, ranura) != PAD_RSTAT_BUSY || ++intentos_modo[puerto][ranura] > 120) {
            *estado_c = LISTO_PUERTO;
        }
        return 0;
    }

    if (padRead(puerto, ranura, &botones) == 0) {
        return 0;
    }
    {
        int id = padInfoMode(puerto, ranura, PAD_MODECURID, 0);

        convertir(&botones, id == PAD_TYPE_DUALSHOCK || id == PAD_TYPE_ANALOG, salida);
    }
    *crudos = 0xFFFFu ^ botones.btns;
    return 1;
}

#ifdef SMK64_DEV
static void registrar_conectores(void)
{
    static u32 cantidad_registro;
    int puerto, ranura;

    if ((cantidad_registro++ % 240) != 0) {
        return;
    }
    for (puerto = 0; puerto < PUERTOS_MANDO_PS2; puerto++) {
        for (ranura = 0; ranura < RANURAS_MULTITAP; ranura++) {
            struct padButtonStatus b;
            int r;

            if (estado_conector[puerto][ranura] == PUERTO_CERRADO) {
                continue;
            }
            r = padRead(puerto, ranura, &b);
            registrar("pad%d%c estado %d/%d leido %d ok %02x modo %02x btns %04x ejes %d,%d id %d", puerto + 1, 'A' + ranura,
                      estado_conector[puerto][ranura], padGetState(puerto, ranura), r, b.ok, b.mode, b.btns, b.ljoy_h,
                      b.ljoy_v, padInfoMode(puerto, ranura, PAD_MODECURID, 0));
        }
    }
}
#endif

void leer_mandos_ps2(void)
{
    int jugador;

    if (padman_ok && vigilar_multitap_ps2()) {
        abrir_conectores();
    }
#ifdef SMK64_DEV
    if (padman_ok) {
        registrar_conectores();
    }
#endif
    for (jugador = 0; jugador < MAXCONTROLLERS; jugador++) {
        int puerto, ranura;
        u32 crudos = 0;

        if (!conector_jugador_ps2(jugador, &puerto, &ranura) ||
            !sondear_conector(puerto, ranura, &s_rellenos[jugador], &crudos)) {
            memset(&s_rellenos[jugador], 0, sizeof(OSContPad));
            s_rellenos[jugador].errno = CONT_NO_RESPONSE_ERROR;
            continue;
        }
        if (jugador == 0) {
            /* R3 solo (sin L3, que con R3 es el medidor): 60 FPS si/no. */
            static u32 ant_r3;
            u32 r3 = (crudos & (PAD_R3 | PAD_L3)) == PAD_R3;

            if (r3 && !ant_r3) {
                alternar_interp_gfx_ps2();
            }
            ant_r3 = r3;
#ifdef SMK64_MEDIDOR
            medidor_entrada_mando(crudos);
#endif
        }
    }
    if (guion_prueba_activo()) {
        avanzar_guion_prueba(s_rellenos); /* el guion sustituye a los mandos */
    }
}

s32 osContInit(OSMesgQueue *mq, u8 *bitpattern, OSContStatus *situacion)
{
    int puerto;
    u8 bits = 0;

    (void) mq;
    inicializar_mandos_ps2();

    /* MK64 solo mira el bit del jugador 1 para decidir si hay mando */
    for (puerto = 0; puerto < MAXCONTROLLERS; puerto++) {
        int p, r;

        memset(&situacion[puerto], 0, sizeof(OSContStatus));
        /* Con un guion de pruebas (DEV) los 4 mandos existen */
        if ((padman_ok && conector_jugador_ps2(puerto, &p, &r) && estado_conector[p][r] != PUERTO_CERRADO) ||
            guion_prueba_activo()) {
            bits |= (u8) (1 << puerto);
            situacion[puerto].type = CONT_TYPE_NORMAL;
        } else {
            situacion[puerto].errnum = CONT_NO_RESPONSE_ERROR;
        }
    }
    *bitpattern = bits;
    return 0;
}

s32 osContStartReadData(OSMesgQueue *mq)
{
    leer_mandos_ps2();
    if (mq != NULL) {
        osSendMesg(mq, (OSMesg) 0, OS_MESG_NOBLOCK);
    }
    return 0;
}

void osContGetReadData(OSContPad *relleno)
{
    memcpy(relleno, s_rellenos, sizeof(OSContPad) * MAXCONTROLLERS);
}

s32 osContStartQuery(OSMesgQueue *mq)
{
    if (mq != NULL) {
        osSendMesg(mq, (OSMesg) 0, OS_MESG_NOBLOCK);
    }
    return 0;
}

void osContGetQuery(OSContStatus *situacion)
{
    u8 bits;

    osContInit(NULL, &bits, situacion);
}

s32 osContSetCh(u8 ch)
{
    (void) ch;
    return 0;
}
