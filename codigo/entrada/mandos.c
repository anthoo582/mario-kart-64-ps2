#include <kernel.h>
#include <libpad.h>
#include <loadfile.h>
#include <sifrpc.h>
#include <string.h>

#include <ultra64.h>
#include <PR/os.h>

#include "sistema/sistema_ps2.h"
#include "graficos/interprete_f3dex.h"
#include "depuracion/guiones_prueba.h"
#ifdef SMK64_MEDIDOR
#include "depuracion/medidor_rendimiento.h"
#endif

#define PUERTOS_PS2 2

enum { PUERTO_CERRADO, ESTABLE_ESPERA_PUERTO, MODO_ESPERA_PUERTO, LISTO_PUERTO };

static char buffer_relleno[PUERTOS_PS2][256] __attribute__((aligned(64)));
static int estado_puerto[PUERTOS_PS2];
static int intentos_modo[PUERTOS_PS2];
static OSContPad s_rellenos[MAXCONTROLLERS];
static int rellenos_inicializado;

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

void inicializar_mandos_ps2(void)
{
    int puerto;
    int padman_ok;

    if (rellenos_inicializado) {
        return;
    }
    padman_ok = cargar_modulo_iop("rom0:SIO2MAN") && cargar_modulo_iop("rom0:PADMAN");
    if (padman_ok && padInit(0) != 1) {
        registrar("padInit fallo");
        padman_ok = 0;
    }
    for (puerto = 0; puerto < PUERTOS_PS2; puerto++) {
        estado_puerto[puerto] =
            (padman_ok && padPortOpen(puerto, 0, buffer_relleno[puerto])) ? ESTABLE_ESPERA_PUERTO : PUERTO_CERRADO;
        if (estado_puerto[puerto] == PUERTO_CERRADO) {
            registrar("padPortOpen(%d) fallo", puerto);
        }
    }
    memset(s_rellenos, 0, sizeof(s_rellenos));
    rellenos_inicializado = 1;
    inicializar_guiones_prueba();
}

static void sondear_puerto(int puerto, OSContPad *salida)
{
    struct padButtonStatus botones;
    int estado;

    salida->button = 0;
    salida->stick_x = 0;
    salida->stick_y = 0;
    salida->errno = CONT_NO_RESPONSE_ERROR;

    if (estado_puerto[puerto] == PUERTO_CERRADO) {
        return;
    }

    estado = padGetState(puerto, 0);
    if (estado == PAD_STATE_DISCONN) {
        estado_puerto[puerto] = ESTABLE_ESPERA_PUERTO;
        return;
    }
    if (estado != PAD_STATE_STABLE && estado != PAD_STATE_FINDCTP1) {
        return;
    }

    if (estado_puerto[puerto] == ESTABLE_ESPERA_PUERTO) {
        /* Mando recien conectado */
        if (padInfoMode(puerto, 0, PAD_MODECUREXID, 0) != 0 &&
            padSetMainMode(puerto, 0, PAD_MMODE_DUALSHOCK, PAD_MMODE_LOCK) == 1) {
            estado_puerto[puerto] = MODO_ESPERA_PUERTO;
            intentos_modo[puerto] = 0;
            return;
        }
        estado_puerto[puerto] = LISTO_PUERTO;
    } else if (estado_puerto[puerto] == MODO_ESPERA_PUERTO) {
        if (padGetReqState(puerto, 0) != PAD_RSTAT_BUSY || ++intentos_modo[puerto] > 120) {
            estado_puerto[puerto] = LISTO_PUERTO;
        }
        return;
    }

#ifdef SMK64_DEV
    {
        static u32 cantidad_registro;

        if ((cantidad_registro++ % 240) == 0) {
            struct padButtonStatus b;
            int r = padRead(puerto, 0, &b);

            registrar("pad%d estado %d/%d leido %d ok %02x modo %02x btns %04x ejes %d,%d id %d", puerto, estado_puerto[puerto],
                    estado, r, b.ok, b.mode, b.btns, b.ljoy_h, b.ljoy_v, padInfoMode(puerto, 0, PAD_MODECURID, 0));
        }
    }
#endif
    if (padRead(puerto, 0, &botones) != 0) {
        int analogico = padInfoMode(puerto, 0, PAD_MODECURID, 0) == PAD_TYPE_DUALSHOCK ||
                     padInfoMode(puerto, 0, PAD_MODECURID, 0) == PAD_TYPE_ANALOG;

        convertir(&botones, analogico, salida);
        if (puerto == 0) {
            /* R3 solo (sin L3, que con R3 es el medidor): 60 FPS si/no. */
            static u32 ant_r3;
            u32 pulsado = 0xFFFFu ^ botones.btns;
            u32 r3 = (pulsado & (PAD_R3 | PAD_L3)) == PAD_R3;

            if (r3 && !ant_r3) {
                alternar_interp_gfx_ps2();
            }
            ant_r3 = r3;
        }
#ifdef SMK64_MEDIDOR
        if (puerto == 0) {
            medidor_entrada_mando(0xFFFFu ^ botones.btns);
        }
#endif
    }
}

void leer_mandos_ps2(void)
{
    int puerto;

    for (puerto = 0; puerto < MAXCONTROLLERS; puerto++) {
        if (puerto < PUERTOS_PS2) {
            sondear_puerto(puerto, &s_rellenos[puerto]);
        } else {
            memset(&s_rellenos[puerto], 0, sizeof(OSContPad));
            s_rellenos[puerto].errno = CONT_NO_RESPONSE_ERROR;
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

    /* MK64 solo mira el bit del puerto 1 para decidir si hay mando */
    for (puerto = 0; puerto < MAXCONTROLLERS; puerto++) {
        memset(&situacion[puerto], 0, sizeof(OSContStatus));
        /* Con un guion de pruebas (DEV) los 4 mandos existen */
        if ((puerto < PUERTOS_PS2 && estado_puerto[puerto] != PUERTO_CERRADO) || guion_prueba_activo()) {
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
