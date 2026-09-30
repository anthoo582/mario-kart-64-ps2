#include <delaythread.h>
#include <kernel.h>
#include <libcdvd.h>
#include <stdio.h>
#include <string.h>

#include <ultra64.h>

#include "sistema/sistema_ps2.h"
#include "sistema/guardado_ps2.h"
#include "sistema/cronometro_fases.h"

#define DISC_ARCHIVO_ROM "\\SMK64ROM.BIN;1"
#define HOST_ARCHIVO_ROM "host:SMK64ROM.BIN"
#define TROZO         (64 * 1024)
#define SECTORS_TROZO (TROZO / 2048)
#define TROZOS_MAX    256
#define ADELANTE_LECTURA    2   /* trozos por lectura de fondo (128 KB) */
#define PRIORIDAD_CARGADOR 60
#define MS_LECTURA_CD 10000       /* 128 KB: holgado incluso para OPL por USB 1.1 */

extern u8 __rom_head_end[];

static u8 *inicio_flujo;
static u32 trozos;
static volatile u8 cargado[TROZOS_MAX];
static volatile u32 cantidad_cargado;
static volatile int todos_cargado = 1;
static volatile s32 buscado = -1;   /* trozo que alguien espera */
static volatile int fatal;
volatile int esperando_rom_ps2;        /* alguien espera datos del disco (perro guardian) */

static int desde_disc;
static u32 s_lsn;                    /* primer sector de SMK64ROM.BIN */
static FILE *archivo_host;
static u32 vblank_inicio;
static volatile u32 s_waits, vblanks_espera; /* esperas de esperar_rom_ps2 y su duracion */
static u8 pila_cargador[16 * 1024] __attribute__((aligned(16)));
extern void *_gp;

/* sceCdSync(1) no bloquea: se duerme entre consultas. Si la lectura no acaba, se cancela. */
static int esperar_cd(int ms_max)
{
    int ms;

    for (ms = 0; sceCdSync(1); ms++) {
        if (ms >= ms_max) {
            sceCdBreak();
            for (ms = 0; sceCdSync(1) && ms < ms_max; ms++) {
                DelayThread(1000);
            }
            return 0;
        }
        DelayThread(1000);
    }
    return 1;
}

static int leer_trozos(u32 primer, u32 cantidad)
{
    u8 *dst = inicio_flujo + primer * TROZO;
    u32 bytes = cantidad * TROZO;

    if (desde_disc) {
        sceCdRMode mode;
        int intentos;

        mode.trycount = 16;
        mode.spindlctrl = SCECdSpinNom;
        mode.datapattern = SCECdSecS2048;
        mode.pad = 0;
        for (intentos = 0; intentos < 8; intentos++) {
            if (sceCdRead(s_lsn + primer * SECTORS_TROZO, cantidad * SECTORS_TROZO, dst, &mode)) {
                if (esperar_cd(MS_LECTURA_CD) && sceCdGetError() == SCECdErNO) {
                    return 1;
                }
                registrar("ROM: lectura del trozo %u sin terminar o con error %d (intento %d)", (unsigned) primer,
                          sceCdGetError(), intentos + 1);
            }
            DelayThread(20000);
        }
        return 0;
    }
    if (fseek(archivo_host, (long) (primer * TROZO), SEEK_SET) != 0) {
        return 0;
    }
    return fread(dst, 1, bytes, archivo_host) == bytes;
}

static void hilo_cargador(void *parametro)
{
    u32 siguiente = 0;

    (void) parametro;
    while (cantidad_cargado < trozos) {
        u32 primer, cantidad;
        s32 querer = buscado;

        if ((querer < 0 || cargado[querer]) && cargando_memcard_ps2()) {
            /* El juego necesita la partida en su primer frame */
            DelayThread(2000);
            continue;
        }
        if (querer >= 0 && (u32) querer < trozos && !cargado[querer]) {
            primer = (u32) querer; /* lo que alguien esta esperando va primero, */
            cantidad = 1;          /* solo (64 KB): se marca al acabar la lectura */
        } else {
            while (siguiente < trozos && cargado[siguiente]) {
                siguiente++;
            }
            if (siguiente >= trozos) {
                break;
            }
            primer = siguiente;
            for (cantidad = 1; cantidad < ADELANTE_LECTURA && primer + cantidad < trozos && !cargado[primer + cantidad]; cantidad++) {
            }
        }
        if (!leer_trozos(primer, cantidad)) {
            fatal = 1;
            registrar("ROM: no se pudo leer SMK64ROM.BIN (trozo %u)", (unsigned) primer);
            detener_por_error("no se pudo leer SMK64ROM.BIN del disco");
        }
        {
            u32 i;

            for (i = 0; i < cantidad; i++) {
                cargado[primer + i] = 1;
            }
            cantidad_cargado += cantidad;
        }
    }
    todos_cargado = 1;
    if (archivo_host != NULL) {
        fclose(archivo_host);
        archivo_host = NULL;
    }
    registrar("ROM: %u KB cargados en segundo plano en %u VBlanks; %u esperas del juego (%u VBlanks en total)",
            (unsigned) (trozos * TROZO / 1024), (unsigned) (contador_vblank() - vblank_inicio), (unsigned) s_waits,
            (unsigned) vblanks_espera);
    ExitDeleteThread();
}

int inicializar_rom_ps2(const char *camino_arranque)
{
    u32 bytes_flujo = (u32) (__rom_end - __rom_head_end);
    ee_thread_t th;
    s32 id;

    if ((uintptr_t) __rom_head_end >= (uintptr_t) __rom_end) {
        return 1; /* ROM entera en el ELF */
    }
    inicio_flujo = __rom_head_end;
    trozos = (bytes_flujo + TROZO - 1) / TROZO;
    bytes_flujo = trozos * TROZO; /* el fichero va rellenado a trozos enteros */
    if (trozos > TROZOS_MAX) {
        detener_por_error("ROM: SMK64ROM.BIN mas grande de lo previsto");
    }

    /* Arrancado desde disco (OPL, disco, PCSX2 con ISO) */
    desde_disc = camino_arranque != NULL && strncmp(camino_arranque, "cdrom", 5) == 0;
    if (!desde_disc) {
        archivo_host = fopen(HOST_ARCHIVO_ROM, "rb");
        desde_disc = archivo_host == NULL; /* ultimo intento: el disco */
    }
    if (desde_disc) {
        sceCdlFILE archivo;

        sceCdInit(SCECdINoD);
        if (!sceCdSearchFile(&archivo, DISC_ARCHIVO_ROM)) {
            registrar("ROM: no se encontro %s en el disco (arranque: %s)", DISC_ARCHIVO_ROM,
                    camino_arranque != NULL ? camino_arranque : "?");
            return 0;
        }
        if (archivo.size < bytes_flujo) {
            registrar("ROM: %s mide %u y deberia medir %u", DISC_ARCHIVO_ROM, (unsigned) archivo.size, (unsigned) bytes_flujo);
            return 0;
        }
        s_lsn = archivo.lsn;
    }

    memset((void *) cargado, 0, sizeof(cargado));
    cantidad_cargado = 0;
    todos_cargado = 0;
    vblank_inicio = contador_vblank();

    memset(&th, 0, sizeof(th));
    th.func = (void *) hilo_cargador;
    th.stack = pila_cargador;
    th.stack_size = sizeof(pila_cargador);
    th.gp_reg = &_gp;
    th.initial_priority = PRIORIDAD_CARGADOR;
    id = CreateThread(&th);
    if (id < 0) {
        return 0;
    }
    StartThread(id, NULL);
    registrar("ROM: cargando %u KB de %s en segundo plano", (unsigned) (bytes_flujo / 1024),
            desde_disc ? "el disco" : HOST_ARCHIVO_ROM);
    return 1;
}

/* Espera a que [addr, addr + size) de la ROM este en la RAM. */
void esperar_rom_ps2(const void *direccion, u32 size)
{
    const u8 *p = (const u8 *) direccion;
    u32 primer, ultimo, c;

    /* El final va rellenado a trozos enteros */
    if (todos_cargado || size == 0 || p + size <= inicio_flujo || p >= inicio_flujo + trozos * TROZO) {
        return;
    }
    if (p < inicio_flujo) {
        size -= (u32) (inicio_flujo - p);
        p = inicio_flujo;
    }
    primer = (u32) (p - inicio_flujo) / TROZO;
    ultimo = (u32) (p + size - 1 - inicio_flujo) / TROZO;
    if (ultimo >= trozos) {
        ultimo = trozos - 1;
    }
    for (c = primer; c <= ultimo; c++) {
        if (!cargado[c]) {
            u32 vb0 = contador_vblank();
            int intr = DI();

            esperando_rom_ps2++;
            s_waits++;
            if (intr) {
                EI();
            }
            while (!cargado[c] && !fatal) {
                buscado = (s32) c;
                DelayThread(500);
            }
            intr = DI();
            esperando_rom_ps2--;
            vblanks_espera += contador_vblank() - vb0;
            if (intr) {
                EI();
            }
        }
    }
}

/* Progreso (para el panel de depuracion) */
void progreso_rom_ps2(u32 *cargado_2, u32 *total)
{
    *cargado_2 = cantidad_cargado;
    *total = trozos;
}
