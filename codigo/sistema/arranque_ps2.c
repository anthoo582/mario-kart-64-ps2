#include <delaythread.h>
#include <iopcontrol.h>
#include <kernel.h>
#include <loadfile.h>
#include <sbv_patches.h>
#include <sifrpc.h>
#include <stdio.h>
#include <string.h>

#include <ultra64.h>

#include "sistema/sistema_ps2.h"
#include "graficos/interprete_f3dex.h"
#include "audio/salida_audio.h"
#include "sistema/guardado_ps2.h"
#include "sistema/cronometro_fases.h"

extern void funcion_principal(void);

/* Prioridad del hilo principal */
#define PRIORIDAD_HILO_PRINCIPAL 126

#ifdef SMK64_BOOT_STOP
#include <depuracion/depuracion_juego.h>
static void detener_arranque(int etapa)
{
    if (etapa == SMK64_BOOT_STOP) {
        init_scr();
        scr_printf("\n  SMK64 PS2: parada de diagnostico en la etapa %d\n", etapa);
        for (;;) {
            SleepThread();
        }
    }
}
#define ETAPA(n) detener_arranque(n)
#else
#define ETAPA(n) do { } while (0)
#endif

#define MS_REINICIO_IOP 10000
#define MODULO_NO_RESIDENTE 1

/* Deja el IOP en un estado conocido */
static void reiniciar_iop(void)
{
    int ms;

    SifInitRpc(0);
    for (ms = 0; !SifIopReset("", 0); ms++) {
        if (ms >= MS_REINICIO_IOP) {
            detener_por_error("el IOP no acepto el reinicio");
        }
        DelayThread(1000);
    }
    for (ms = 0; !SifIopSync(); ms++) {
        if (ms >= MS_REINICIO_IOP) {
            detener_por_error("el IOP no termino de reiniciarse");
        }
        DelayThread(1000);
    }
    SifInitRpc(0);
    SifLoadFileInit();
    sbv_patch_enable_lmb();
    sbv_patch_disable_prefix_check();
}

/* El EE core de OPL queda residente entre 0x84000 y 0x100000 con esta firma en su configuracion
   (ee_core/include/coreconfig.h de OPL). */
#define OPL_MAGIA_0 0x4D614730u
#define OPL_MAGIA_1 0x4D616731u

static int lanzado_por_opl(const char *camino_arranque)
{
    const volatile u32 *p;

    /* OPL arranca el juego como un disco */
    if (camino_arranque == NULL || strncmp(camino_arranque, "cdrom", 5) != 0) {
        return 0;
    }
    for (p = (const volatile u32 *) 0x00084000; p < (const volatile u32 *) 0x000FFFF8; p++) {
        if (p[0] == OPL_MAGIA_0 && p[1] == OPL_MAGIA_1) {
            return 1;
        }
    }
    return 0;
}

/* Bajo OPL el IOP ya viene reiniciado con su CDVDMAN y el driver del dispositivo (USB, HDD o red).
   Reiniciarlo otra vez obliga a OPL a recargar y montar ese driver (segundos por USB o SMB).
   A cambio, OPL no carga su tarjeta virtual (VMC), PADEMU ni el reinicio en juego (IGR). */
static void preparar_iop_sin_reinicio(void)
{
    SifInitRpc(0);
    SifLoadFileInit();
    sbv_patch_enable_lmb();
    sbv_patch_disable_prefix_check();
}

int cargar_modulo_iop(const char *camino)
{
    int resultado = MODULO_NO_RESIDENTE;
    int id = SifLoadStartModule(camino, 0, NULL, &resultado);

    if (id < 0 || (resultado & 3) == MODULO_NO_RESIDENTE) {
        registrar("IOP: %s no quedo cargado (id %d, resultado %d)", camino, id, resultado);
        return 0;
    }
    return 1;
}

int ejecutar_modulo_iop(const char *nombre, void *irx, u32 tamanio)
{
    int resultado = MODULO_NO_RESIDENTE;
    int id = SifExecModuleBuffer(irx, tamanio, 0, NULL, &resultado);

    if (id < 0 || (resultado & 3) == MODULO_NO_RESIDENTE) {
        registrar("IOP: %s no quedo cargado (id %d, resultado %d)", nombre, id, resultado);
        return 0;
    }
    return 1;
}

int main(int argc, char *argv[])
{

    ETAPA(0);
    empezar_tiempos_ps2("arranque");
    /* Antes del reinicio del IOP: si algo se cuelga ahi, el perro guardian muestra la pantalla de fallo
       en lugar de dejar la ultima imagen del cargador (OPL: "Loading config..."). */
    inicializar_depuracion();
#ifndef SMK64_NO_IOP_RESET
    if (lanzado_por_opl(argc > 0 ? argv[0] : NULL)) {
        preparar_iop_sin_reinicio();
        registrar("arranque: lanzado por OPL, sin reiniciar el IOP");
    } else {
        reiniciar_iop();
    }
#else
    SifInitRpc(0);
#endif
    marcar_tiempos_ps2("reinicio del IOP");
    ChangeThreadPriority(GetThreadId(), PRIORIDAD_HILO_PRINCIPAL);
    ETAPA(1);
    ETAPA(2);
    registrar("arranque: ROM %u KB en %p", (unsigned) ((__rom_end - __rom_start) / 1024), __rom_start);
    if (!inicializar_rom_ps2(argc > 0 ? argv[0] : NULL)) {
        detener_por_error("falta SMK64ROM.BIN: usa la ISO completa (o el ELF monolitico con uLaunchELF)");
    }
    marcar_tiempos_ps2("datos del disco: inicio de la carga");

    inicializar_hilos();
    inicializar_hardware_libultra();
    guardar_segmentos_iniciales();
    marcar_tiempos_ps2("depuracion, hilos, segmentos");
    ETAPA(3);
    inicializar_renderizador();
    marcar_tiempos_ps2("GS y texturas");
    ETAPA(4);
    inicializar_mandos_ps2();
    marcar_tiempos_ps2("mandos (SIO2MAN, PADMAN)");
    inicializar_memory_card();  /* necesita SIO2MAN, que carga inicializar_mandos_ps2 */
    marcar_tiempos_ps2("memory card (MCMAN, MCSERV; la lectura sigue en segundo plano)");
    ETAPA(5);
    inicializar_ps2_audio();
    marcar_tiempos_ps2("audio (LIBSD, AUDSRV)");
    inicializar_retrazo();
    ETAPA(6);

    registrar("entrando en main_func");
    funcion_principal();

    for (;;) {
        SleepThread();
    }
    return 0;
}
