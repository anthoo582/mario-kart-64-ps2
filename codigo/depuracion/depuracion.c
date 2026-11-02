#include <kernel.h>
#include <depuracion/depuracion_juego.h>
#include <ee_debug.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <ultra64.h>

#include "graficos/interprete_f3dex.h"
#include "graficos/sintetizador_gs.h"
#include "sistema/sistema_ps2.h"
#include "sistema/cronometro_fases.h"

#define LINEAS_REGISTRO 32
#define ANCHO_REGISTRO 200

#define RENGLONES_PANTALLA 27
#define COLS_PANTALLA 79

static char registro[LINEAS_REGISTRO][ANCHO_REGISTRO];
static int siguiente_registro;
volatile int en_panico_ps2;

/* Punto de control por hilo del EE (id < 64). */
#define RANURAS_CKPT 64
static const char *volatile punto_control[RANURAS_CKPT];

static void mostrar_pantalla(const char *titulo);

#if defined(SMK64_DEV) || defined(SMK64_DEBUG)
#define REGISTRO_A_PRINTF 1
#endif

#ifdef SMK64_DEV
#include <screenshot.h>
static char host_buf[48 * 1024];
static int largo_host;

static s32 host_sema = -1;

void bloquear_host(void)
{
    if (host_sema < 0) {
        return;
    }
    if (en_panico_ps2) {
        /* Si el hilo que tenia el cerrojo es el que fallo, no esperar. */
        PollSema(host_sema);
        return;
    }
    WaitSema(host_sema);
}

void desbloquear_host(void)
{
    if (host_sema >= 0) {
        SignalSema(host_sema);
    }
}

static u32 host_vaciado; /* VBlank del ultimo volcado del registro */

static void vaciar_host(void)
{
    FILE *f = fopen("host:smk64_log.txt", "w");

    if (f != NULL) {
        fwrite(host_buf, 1, largo_host, f);
        fclose(f);
    }
    host_vaciado = contador_vblank();
}

static void escribir_host(const char *line)
{
    FILE *f;
    int n;
    int lugar;

    bloquear_host();
    /* Una linea ocupa hasta ANCHO_REGISTRO + 10 bytes */
    if (largo_host > (int) sizeof(host_buf) - (ANCHO_REGISTRO + 16)) {
        static int parte;
        char path[40];

        snprintf(path, sizeof(path), "host:smk64_log_%02d.txt", parte++);
        f = fopen(path, "w");
        if (f != NULL) {
            fwrite(host_buf, 1, largo_host, f);
            fclose(f);
        }
        largo_host = 0;
    }
    lugar = (int) sizeof(host_buf) - largo_host;
    n = snprintf(host_buf + largo_host, lugar, "[%6u] %s\n", (unsigned) contador_vblank(), line);
    if (n > 0) {
        largo_host += (n < lugar) ? n : lugar - 1; /* snprintf da la longitud sin recortar */
    }
    if (en_panico_ps2 || strstr(line, "PANIC") != NULL || strstr(line, "cuelgue") != NULL ||
        strstr(line, "GIF") != NULL || contador_vblank() - host_vaciado >= 60) {
        vaciar_host();
    }
    desbloquear_host();
}

void volcar_frame(const char *nombre, u32 direccion_vram, u32 ancho, u32 altura, u32 psm)
{
    char path[64];

    snprintf(path, sizeof(path), "host:%s.tga", nombre);
    bloquear_host();
    ps2_screenshot_file(path, direccion_vram, ancho, altura, psm);
    desbloquear_host();
}
#else
#define escribir_host(line) ((void) 0)

void bloquear_host(void)
{
}

void desbloquear_host(void)
{
}

void volcar_frame(const char *nombre, u32 direccion_vram, u32 ancho, u32 altura, u32 psm)
{
    (void) nombre;
    (void) direccion_vram;
    (void) ancho;
    (void) altura;
    (void) psm;
}
#endif

static void registrar_salida(const char *line)
{
#ifdef REGISTRO_A_PRINTF
    printf("smk64: %s\n", line);
#endif
    escribir_host(line);
}

void registrar(const char *fmt, ...)
{
    va_list ap;
    char *line = registro[siguiente_registro];

    va_start(ap, fmt);
    vsnprintf(line, ANCHO_REGISTRO, fmt, ap);
    va_end(ap);
    siguiente_registro = (siguiente_registro + 1) % LINEAS_REGISTRO;
    registrar_salida(line);
}

void rend_registro_ps2(const char *fmt, ...)
{
#if defined(REGISTRO_A_PRINTF) || defined(SMK64_DEV)
    va_list ap;
    char line[ANCHO_REGISTRO];

    va_start(ap, fmt);
    vsnprintf(line, ANCHO_REGISTRO, fmt, ap);
    va_end(ap);
    registrar_salida(line);
#else
    (void) fmt;
#endif
}

#ifdef SMK64_DEV
/* Punto de observacion por software */
static volatile u32 *direccion_vigilancia;
static u32 valor_vigilancia;
static int vigilancia_disparado;

void fijar_vigilancia(void *direccion)
{
    direccion_vigilancia = (volatile u32 *) direccion;
    valor_vigilancia = *direccion_vigilancia;
    vigilancia_disparado = 0;
    registrar("vigilando %p = %08x", direccion, (unsigned) valor_vigilancia);
}

void rango_vigilancia_ps2(const void *dst, u32 size, void *llamador)
{
    if (direccion_vigilancia != NULL && (const u8 *) direccion_vigilancia >= (const u8 *) dst &&
        (const u8 *) direccion_vigilancia < (const u8 *) dst + size) {
        registrar("DMA sobre la direccion vigilada: destino %p +%x (llamante %p)", dst, (unsigned) size, llamador);
    }
}

void comprobar_vigilancia(const char *where, void *llamador)
{
    if (direccion_vigilancia != NULL && !vigilancia_disparado && *direccion_vigilancia != valor_vigilancia) {
        int i;

        vigilancia_disparado = 1;
        registrar("VIGILANCIA: %p cambio %08x -> %08x, visto en %s (llamante %p, hilo %d)", (void *) direccion_vigilancia,
                (unsigned) valor_vigilancia, (unsigned) *direccion_vigilancia, where, llamador, (int) GetThreadId());
        for (i = 0; i < RANURAS_CKPT; i++) {
            if (punto_control[i] != NULL) {
                registrar("  hilo %d: %s", i, punto_control[i]);
            }
        }
    }
}
#endif

static int cantidad_punto_control;

void marcar_punto_control(const char *where)
{
    s32 id = GetThreadId();

    punto_control[id & (RANURAS_CKPT - 1)] = where;
    cantidad_punto_control++;
#ifdef SMK64_TRAZA
    if (contador_vblank() >= SMK64_TRAZA) {
        registrar("traza cuelgue hilo %d: %s", (int) id, where);
    }
#endif
#ifdef SMK64_DEV_TRACE
    registrar("ckpt %d hilo %d: %s", cantidad_punto_control, (int) id, where);
#endif
#ifdef SMK64_CKPT_STOP
    if (cantidad_punto_control == SMK64_CKPT_STOP) {
        ChangeThreadPriority(id, 0);
        registrar("parada en punto de control %d: %s", cantidad_punto_control, where);
        mostrar_pantalla("PARADA DE DIAGNOSTICO");
        for (;;) {
        }
    }
#endif
}

static char estado_gif[ANCHO_REGISTRO];

static void rescate_gif(void)
{
    volatile u32 *chcr = (volatile u32 *) 0x1000A000;
    volatile u32 *madr = (volatile u32 *) 0x1000A010;
    volatile u32 *qwc = (volatile u32 *) 0x1000A020;
    volatile u32 *est_gif = (volatile u32 *) 0x10003020;
    volatile u32 *gif_ctrl = (volatile u32 *) 0x10003000;
    volatile u32 *activar_r = (volatile u32 *) 0x1000F520;
    volatile u32 *activar_w = (volatile u32 *) 0x1000F590;

    *(volatile u64 *) 0x12001040 = 0;
    if (!(*chcr & 0x100)) {
        return;
    }
    snprintf(estado_gif, sizeof(estado_gif), "GIF atascado: D2_CHCR %08x MADR %08x QWC %x GIF_STAT %08x",
             (unsigned) *chcr, (unsigned) *madr, (unsigned) *qwc, (unsigned) *est_gif);
    gs_log_gif_stall(); /* before the reset below: what the GS stopped on */
    *activar_w = *activar_r | 0x10000; /* suspende el DMA */
    *chcr &= ~0x100u;
    *activar_w = *activar_r & ~0x10000u;
    *gif_ctrl = 1; /* reinicio del GIF */
}

static const char *const nombres_situacion[] = { "?", "RUN", "READY", "?", "WAIT", "?", "?", "?", "SUSP", "?", "?", "?",
                                            "WSUSP" };

static int linea_hilo(int id, char *salida, int size)
{
    ee_thread_status_t ts;
    const char *ck = punto_control[id & (RANURAS_CKPT - 1)];
    char wait[12];

    if (ReferThreadStatus(id, &ts) < 0 || ts.status == 0 || ts.status == THS_DORMANT) {
        return 0;
    }
    if (ts.waitType == TSW_SEMA) {
        snprintf(wait, sizeof(wait), "sem%d", (int) ts.waitId);
    } else if (ts.waitType == TSW_SLEEP) {
        snprintf(wait, sizeof(wait), "dorm");
    } else {
        snprintf(wait, sizeof(wait), "-");
    }
    snprintf(salida, size, "%2d %3d %-5s %-6s %08x %.44s", id, ts.current_priority,
             ts.status <= 12 ? nombres_situacion[ts.status] : "?", wait, (unsigned) (uintptr_t) ts.func,
             ck != NULL ? ck : "");
    return 1;
}

static void linea_pantalla(int *renglon, const char *text)
{
    char buf[COLS_PANTALLA + 1];

    if (*renglon >= RENGLONES_PANTALLA) {
        return;
    }
    snprintf(buf, sizeof(buf), "%s", text);
    scr_setXY(0, *renglon);
    scr_printf("%s", buf);
    (*renglon)++;
}

static void mostrar_pantalla(const char *titulo)
{
    char line[ANCHO_REGISTRO];
    const char *group = ps2_tiempos_activo_grupo();
    int renglon = 0, i, registrar_renglones, primer, cantidad;

    rescate_gif();
    init_scr();
    scr_setbgcolor(0x00400000); /* azul oscuro: distinto de cualquier frame del juego */
    scr_clear();

    snprintf(line, sizeof(line), " SMK64 PS2 %s - %s  (VBlank %u)", PS2_BUILD_ID, titulo,
             (unsigned) contador_vblank());
    linea_pantalla(&renglon, line);
    if (estado_gif[0] != '\0') {
        linea_pantalla(&renglon, estado_gif);
    }
    if (group != NULL) {
        snprintf(line, sizeof(line), " Fase: %s, despues de: %s", group, ps2_tiempos_ultimo_paso());
        linea_pantalla(&renglon, line);
    }
    if (ps2_gfx_cuelgue_info(line, sizeof(line))) {
        linea_pantalla(&renglon, line);
    }
    linea_pantalla(&renglon, "");
    linea_pantalla(&renglon, "id pri estado espera pc       ultimo punto de control");
    for (i = 1; i < 64 && renglon < RENGLONES_PANTALLA - 6; i++) {
        if (linea_hilo(i, line, sizeof(line))) {
            linea_pantalla(&renglon, line);
        }
    }
    linea_pantalla(&renglon, "");
    linea_pantalla(&renglon, "Registro (lo mas reciente al final):");
    /* Las ultimas lineas del registro que quepan. */
    registrar_renglones = RENGLONES_PANTALLA - renglon;
    cantidad = 0;
    for (i = 0; i < LINEAS_REGISTRO; i++) {
        if (registro[i][0] != '\0') {
            cantidad++;
        }
    }
    if (cantidad > registrar_renglones) {
        cantidad = registrar_renglones;
    }
    primer = (siguiente_registro - cantidad + LINEAS_REGISTRO) % LINEAS_REGISTRO;
    for (i = 0; i < cantidad; i++) {
        const char *l = registro[(primer + i) % LINEAS_REGISTRO];

        if (l[0] != '\0') {
            linea_pantalla(&renglon, l);
        }
    }
}

static void ps2_depuracion_informe_hilos(void)
{
    char line[ANCHO_REGISTRO];
    int i;

    if (ps2_gfx_cuelgue_info(line, sizeof(line))) {
        registrar("%s", line);
    }

    for (i = 1; i < 64; i++) {
        if (linea_hilo(i, line, sizeof(line))) {
            registrar("  hilo %s", line);
        }
    }
}

static void freeze_others(void)
{
    s32 propio = GetThreadId();
    int i;

    for (i = 1; i < 64; i++) {
        ee_thread_status_t ts;

        if (i == propio || ReferThreadStatus(i, &ts) < 0 || ts.status == 0 || ts.status == THS_DORMANT) {
            continue;
        }
        SuspendThread(i);
    }
}

void detener_por_gif_trabado(const char *por_que)
{
    rescate_gif();
    registrar("%s", por_que);
    registrar("%s", estado_gif);
    ps2_depuracion_informe_hilos();
    detener_por_error(por_que);
}

void detener_por_error(const char *mens)
{
    if (en_panico_ps2) {
        for (;;) {
            SleepThread();
        }
    }
    en_panico_ps2 = 1;
    registrar("PANIC: %s", mens);
    /* Nada mas debe correr */
    ChangeThreadPriority(GetThreadId(), 0);
    mostrar_pantalla("ERROR");
    freeze_others();
    for (;;) {
        SleepThread();
    }
}

int en_panico_depuracion_ps2(void)
{
    return en_panico_ps2;
}

#define SEGUNDOS_PERRO_GUARDIAN      6
#define SEGUNDOS_CARGA_PERRO_GUARDIAN 30

static volatile u32 alimentacion;
static s32 sema_perro = -1;
static u8 pila_perro[8 * 1024] __attribute__((aligned(16)));
extern void *_gp;

void alimentar_perro_guardian(void)
{
    alimentacion++;
}

static void alarma_perro(s32 id, u16 time, void *parametro)
{
    (void) id;
    (void) time;
    (void) parametro;
    iSignalSema(sema_perro);
    ExitHandler();
}

static void hilo_perro(void *parametro)
{
    u32 ultimo = 0;
    int trabado = 0;
#ifdef SMK64_THREAD_DUMP
    int segundos = 0;
#endif

    (void) parametro;
    for (;;) {
        int limite;

        /* 15 734 lineas H por segundo en NTSC */
        SetAlarm(15734, alarma_perro, NULL);
        WaitSema(sema_perro);
#ifdef SMK64_THREAD_DUMP
        if (++segundos % SMK64_THREAD_DUMP == 0) {
            rend_registro_ps2("hilos (VBlank %u, frames %u):", (unsigned) contador_vblank(), (unsigned) alimentacion);
            {
                char line[ANCHO_REGISTRO];
                int i;

                for (i = 1; i < 64; i++) {
                    if (linea_hilo(i, line, sizeof(line))) {
                        rend_registro_ps2("  %s", line);
                    }
                }
            }
        }
#endif
        /* Carga (fase cronometrada) o espera de datos del disco */
        limite = (ps2_tiempos_activo_grupo() != NULL || esperando_rom_ps2 > 0) ? SEGUNDOS_CARGA_PERRO_GUARDIAN
                                                                           : SEGUNDOS_PERRO_GUARDIAN;
        if (alimentacion != ultimo) {
            ultimo = alimentacion;
            trabado = 0;
        } else if (++trabado >= limite && !en_panico_ps2) {
            char titulo[48];

            en_panico_ps2 = 1;
            snprintf(titulo, sizeof(titulo), "CUELGUE (sin frames en %d s)", limite);
            mostrar_pantalla(titulo);
            freeze_others();
            registrar("sin frames en %d s: cuelgue", limite);
            if (estado_gif[0] != '\0') {
                registrar("%s", estado_gif);
            }
            ps2_depuracion_informe_hilos();
            for (;;) {
                SleepThread();
            }
        }
    }
}

static const char *const nombres_causa[16] = { "INT",  "TLB mod", "TLB carga", "TLB escritura", "direccion (carga)",
                                             "direccion (escritura)", "bus (instr.)", "bus (datos)", "syscall",
                                             "break", "instruccion reservada", "coprocesador",
                                             "desbordamiento", "trap", "?", "?" };
static volatile u32 s_exc[7];
static u8 pila_exc[16 * 1024] __attribute__((aligned(16)));

static void informe_excepcion(void)
{
    char mens[ANCHO_REGISTRO];
    u32 codigo = (s_exc[0] >> 2) & 0x1F;

    snprintf(mens, sizeof(mens), "excepcion %s (%u) en PC %08x dir %08x RA %08x SP %08x hilo %d",
             codigo < 16 ? nombres_causa[codigo] : "?", (unsigned) codigo, (unsigned) s_exc[1], (unsigned) s_exc[2],
             (unsigned) s_exc[3], (unsigned) s_exc[4], (int) s_exc[5]);
    detener_por_error(mens);
}

static int manejador_excepcion(EE_RegFrame *f)
{
    s_exc[0] = f->cause;
    s_exc[1] = f->epc;
    s_exc[2] = f->badvaddr;
    s_exc[3] = f->ra[0];
    s_exc[4] = f->sp[0];
    s_exc[5] = (u32) GetThreadId();
    s_exc[6] = f->status;
    f->epc = (u32) informe_excepcion;
    f->sp[0] = (u32) (pila_exc + sizeof(pila_exc) - 64);
    return 1;
}

static void instalar_manejadores_excepcion(void)
{
    static const int causas[] = { 1, 2, 3, 4, 5, 6, 7, 10, 12, 13 };
    unsigned i;

    if (ee_dbg_install(1) != 0) {
        registrar("ee_dbg_install fallo: sin manejador de excepciones");
        return;
    }
    for (i = 0; i < sizeof(causas) / sizeof(causas[0]); i++) {
        ee_dbg_set_level1_handler(causas[i], manejador_excepcion);
    }
}

void inicializar_depuracion(void)
{
    ee_sema_t sema;
    ee_thread_t th;
    s32 id;

    memset(registro, 0, sizeof(registro));
    memset(&sema, 0, sizeof(sema));
    sema.max_count = 1;
    sema_perro = CreateSema(&sema);
#ifdef SMK64_DEV
    sema.init_count = 1;
    host_sema = CreateSema(&sema);
    sema.init_count = 0;
#endif

    memset(&th, 0, sizeof(th));
    th.func = (void *) hilo_perro;
    th.stack = pila_perro;
    th.stack_size = sizeof(pila_perro);
    th.gp_reg = &_gp;
    th.initial_priority = 0;
    id = CreateThread(&th);
    StartThread(id, NULL);
    instalar_manejadores_excepcion();
}
