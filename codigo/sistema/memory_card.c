#include <delaythread.h>
#include <kernel.h>
#include <libmc.h>
#include <loadfile.h>
#include <sifrpc.h>
#include <stdio.h>
#include <string.h>

#include <ultra64.h>

#include "sistema/sistema_ps2.h"
#include "entrada/multitap.h"
#include "sistema/guardado_ps2.h"
#include "sistema/cronometro_fases.h"

#define PUERTO_MC 0
#define RANURA_MC 0
#define DIR_GUARDADO  "/BASLUS-99999SMK64"
#define ARCHIVO_A_GUARDADO DIR_GUARDADO "/SMK64A.SAV"
#define ARCHIVO_B_GUARDADO DIR_GUARDADO "/SMK64B.SAV"
#define ARCHIVO_GUARDADO_VIEJO DIR_GUARDADO "/SMK64.SAV"
#define ARCHIVO_TEMPORAL_VIEJO DIR_GUARDADO "/SMK64.TMP"
#define ARCHIVO_ICONO DIR_GUARDADO "/SMK64.ICO"
#define ARCHIVO_ICONO_SYS DIR_GUARDADO "/icon.sys"

extern u8 icono_partida[];
extern u32 tamanio_icono_partida;

static int mc_ok;
static s32 sema_guardado = -1;
static s32 sema_tic = -1;
static u8 pila_guardado[16 * 1024] __attribute__((aligned(16)));
static ImagenGuardadoPs2 copia_escritura __attribute__((aligned(64)));
static int ranura_siguiente;      /* copia que recibe el proximo guardado */
static int archivos_legacy;   /* quedan SMK64.SAV / SMK64.TMP de builds anteriores */

enum { PENDIENTE_CARGA, HECHO_CARGA, CARGA_ABANDONED };
static volatile int estado_carga = PENDIENTE_CARGA;

#define PRIORIDAD_HILO_CARGA 100 /* mientras lee: por encima del hilo de juego */
#define PRIORIDAD_HILO_GUARDADO 125 /* despues: por debajo de todos los del juego */
#define MS_TIMEOUT_CARGA 5000
#define MS_ESPERA_ORDEN_MC 5000
extern void *_gp;

static u32 crc32(const u8 *p, u32 n)
{
    u32 crc = 0xFFFFFFFFu;

    while (n--) {
        int k;

        crc ^= *p++;
        for (k = 0; k < 8; k++) {
            crc = (crc >> 1) ^ (0xEDB88320u & -(crc & 1));
        }
    }
    return ~crc;
}

static u32 crc_imagen(const ImagenGuardadoPs2 *img)
{
    const u8 *empezar = (const u8 *) &img->eeprom;

    return crc32(empezar, sizeof(*img) - (u32) (empezar - (const u8 *) img));
}

/* Espera el resultado de la ultima llamada a libmc */
static int resultado_llamar_mc(void)
{
    int cmd, result = -1; /* mcSync devuelve -1 sin tocarlo si no habia llamada */
    int ms;

    for (ms = 0; mcSync(MC_NOWAIT, &cmd, &result) == 0; ms++) {
        if (ms >= MS_ESPERA_ORDEN_MC) {
            /* libmc atiende una orden a la vez: si esta no vuelve, las siguientes tampoco. */
            mc_ok = 0;
            registrar("memcard: la tarjeta no contesto en %d ms; se deja de usar", ms);
            return -1;
        }
        DelayThread(1000);
    }
    return result;
}

static int listo_tarjeta_mc(void)
{
    int type, free, format;

    if (!mc_ok || mcGetInfo(PUERTO_MC, RANURA_MC, &type, &free, &format) != 0) {
        return 0;
    }
    return resultado_llamar_mc() >= -1 && type == sceMcTypePS2 && format;
}

static int escribir_archivo_mc(const char *path, const void *datos, int size)
{
    int fd, n;

    if (mcOpen(PUERTO_MC, RANURA_MC, path, sceMcFileCreateFile | sceMcFileAttrWriteable) != 0) {
        return -1;
    }
    fd = resultado_llamar_mc();
    if (fd < 0) {
        return fd;
    }
    mcWrite(fd, datos, size);
    n = resultado_llamar_mc();
    mcClose(fd);
    resultado_llamar_mc();
    return n == size ? 0 : -1;
}

static int leer_archivo_mc(const char *path, void *datos, int size)
{
    int fd, n;

    if (mcOpen(PUERTO_MC, RANURA_MC, path, sceMcFileAttrReadable) != 0) {
        return -1;
    }
    fd = resultado_llamar_mc();
    if (fd < 0) {
        return fd;
    }
    mcRead(fd, datos, size);
    n = resultado_llamar_mc();
    mcClose(fd);
    resultado_llamar_mc();
    return n;
}

static void titulo_sjis(u8 *dst, const char *orig_, int cars_max)
{
    int i;

    for (i = 0; i < cars_max && orig_[i]; i++) {
        unsigned char c = (unsigned char) orig_[i];
        u16 codigo;

        if (c >= '0' && c <= '9') {
            codigo = 0x824F + (c - '0');
        } else if (c >= 'A' && c <= 'Z') {
            codigo = 0x8260 + (c - 'A');
        } else if (c >= 'a' && c <= 'z') {
            codigo = 0x8281 + (c - 'a');
        } else if (c == '-') {
            codigo = 0x817C;
        } else if (c == ':') {
            codigo = 0x8146;
        } else {
            codigo = 0x8140;
        }
        dst[i * 2] = (u8) (codigo >> 8);
        dst[i * 2 + 1] = (u8) codigo;
    }
}

static int escribir_archivos_icono(void)
{
    static mcIcon icono __attribute__((aligned(64)));
    static const iconIVECTOR bg[4] = {
        { 0x60, 0x00, 0x00, 0 }, { 0x60, 0x00, 0x00, 0 }, { 0x10, 0x10, 0x40, 0 }, { 0x10, 0x10, 0x40, 0 },
    };
    static const iconFVECTOR dir_luz[3] = {
        { 0.5f, 0.5f, 0.5f, 0.0f }, { 0.0f, -0.4f, -0.1f, 0.0f }, { -0.5f, -0.5f, 0.5f, 0.0f },
    };
    static const iconFVECTOR col_luz[3] = {
        { 0.3f, 0.3f, 0.3f, 0.0f }, { 0.4f, 0.4f, 0.4f, 0.0f }, { 0.5f, 0.5f, 0.5f, 0.0f },
    };
    static const iconFVECTOR ambiente = { 0.6f, 0.6f, 0.6f, 0.0f };

    memset(&icono, 0, sizeof(icono));
    memcpy(icono.head, "PS2D", 4);
    icono.type = MCICON_TYPE_SAVED_DATA;
    icono.nlOffset = 18;
    icono.trans = 0x60;
    memcpy(icono.bgCol, bg, sizeof(bg));
    memcpy(icono.lightDir, dir_luz, sizeof(dir_luz));
    memcpy(icono.lightCol, col_luz, sizeof(col_luz));
    memcpy(icono.lightAmbient, ambiente, sizeof(ambiente));
    titulo_sjis((u8 *) icono.title, "SMK64 PS2Partida", 16);
    strcpy((char *) icono.view, "SMK64.ICO");
    strcpy((char *) icono.copy, "SMK64.ICO");
    strcpy((char *) icono.del, "SMK64.ICO");

    if (escribir_archivo_mc(ARCHIVO_ICONO_SYS, &icono, sizeof(icono)) != 0) {
        return -1;
    }
    return escribir_archivo_mc(ARCHIVO_ICONO, icono_partida, (int) tamanio_icono_partida);
}

static int valido_imagen(const ImagenGuardadoPs2 *img)
{
    return img->magic == MAGICO_GUARDADO_PS2 && img->version == VERSION_GUARDADO_PS2 && img->checksum == crc_imagen(img);
}

static int leer_imagen(const char *path, ImagenGuardadoPs2 *img)
{
    return leer_archivo_mc(path, img, sizeof(*img)) == (int) sizeof(*img) && valido_imagen(img);
}

static int leer_cabecera(const char *path, u32 *generacion)
{
    static u32 hdr[16] __attribute__((aligned(64)));

    if (leer_archivo_mc(path, hdr, 16) != 16 || hdr[0] != MAGICO_GUARDADO_PS2 || hdr[1] != VERSION_GUARDADO_PS2) {
        return 0;
    }
    *generacion = hdr[2];
    return 1;
}

static int publish(const ImagenGuardadoPs2 *img)
{
    int ok, intr = DI();

    ok = estado_carga == PENDIENTE_CARGA;
    if (ok) {
        memcpy(&guardado_ps2, img, sizeof(guardado_ps2));
        estado_carga = HECHO_CARGA;
    }
    if (intr) {
        EI();
    }
    return ok;
}

void cargar_partida(void)
{
    u32 generar_a = 0, generar_b = 0;
    int tiene_a, tiene_b, primer;

    if (!mc_ok || !listo_tarjeta_mc()) {
        registrar("memcard: sin tarjeta en la ranura 1; la partida no se guardara hasta insertarla");
        return;
    }
    tiene_a = leer_cabecera(ARCHIVO_A_GUARDADO, &generar_a);
    tiene_b = leer_cabecera(ARCHIVO_B_GUARDADO, &generar_b);
    /* La mas nueva primero */
    primer = (tiene_a && (!tiene_b || generar_a >= generar_b)) ? 0 : 1;
    if ((primer == 0 ? tiene_a : tiene_b) && leer_imagen(primer == 0 ? ARCHIVO_A_GUARDADO : ARCHIVO_B_GUARDADO, &copia_escritura)) {
        ranura_siguiente = primer ^ 1;
    } else if ((primer == 0 ? tiene_b : tiene_a) && leer_imagen(primer == 0 ? ARCHIVO_B_GUARDADO : ARCHIVO_A_GUARDADO, &copia_escritura)) {
        ranura_siguiente = primer; /* la rota es la que se reescribe */
    } else if (leer_imagen(ARCHIVO_GUARDADO_VIEJO, &copia_escritura) || leer_imagen(ARCHIVO_TEMPORAL_VIEJO, &copia_escritura)) {
        /* Partida de un build anterior */
        ranura_siguiente = 0;
        archivos_legacy = 1;
        if (publish(&copia_escritura)) {
            registrar("memcard: partida de un build anterior (generacion %u)", (unsigned) copia_escritura.generacion);
        }
        return;
    } else {
        registrar("memcard: sin partida guardada (se crea al guardar)");
        return;
    }
    if (publish(&copia_escritura)) {
        registrar("memcard: partida cargada (copia %c, generacion %u)", ranura_siguiente ? 'A' : 'B',
                (unsigned) copia_escritura.generacion);
    }
}

int cargando_memcard_ps2(void)
{
    return estado_carga == PENDIENTE_CARGA;
}

void esperar_cargado_memcard_ps2(void)
{
    int ms, intr;

    for (ms = 0; estado_carga == PENDIENTE_CARGA && ms < MS_TIMEOUT_CARGA; ms++) {
        DelayThread(1000);
    }
    intr = DI();
    if (estado_carga == PENDIENTE_CARGA) {
        estado_carga = CARGA_ABANDONED;
    }
    if (intr) {
        EI();
    }
    if (estado_carga == CARGA_ABANDONED && ms > 0) {
        registrar("memcard: la tarjeta no contesto en %d ms; se sigue sin la partida y sin guardar", ms);
    } else if (ms > 0) {
        registrar("memcard: el juego espero la lectura %d ms", ms);
    }
}

static int guardar_ahora(void)
{
    int r;

    if (!listo_tarjeta_mc()) {
        return -1;
    }
    memcpy(&copia_escritura, &guardado_ps2, sizeof(copia_escritura));
    copia_escritura.magic = MAGICO_GUARDADO_PS2;
    copia_escritura.version = VERSION_GUARDADO_PS2;
    copia_escritura.checksum = crc_imagen(&copia_escritura);

    mcMkDir(PUERTO_MC, RANURA_MC, DIR_GUARDADO);
    r = resultado_llamar_mc();
    if (r == 0) { /* directorio nuevo: primera vez en esta tarjeta */
        if (escribir_archivos_icono() != 0) {
            registrar("memcard: no se pudo escribir el icono");
        }
    } else if (r != -4) {
        return -2;
    }
    /* La copia mas vieja; si la escritura se corta, queda la otra. */
    if (escribir_archivo_mc(ranura_siguiente ? ARCHIVO_B_GUARDADO : ARCHIVO_A_GUARDADO, &copia_escritura, sizeof(copia_escritura)) != 0) {
        return -3;
    }
    ranura_siguiente ^= 1;
    if (archivos_legacy) {
        /* Los ficheros del formato anterior ya no hacen falta. */
        mcDelete(PUERTO_MC, RANURA_MC, ARCHIVO_TEMPORAL_VIEJO);
        resultado_llamar_mc();
        mcDelete(PUERTO_MC, RANURA_MC, ARCHIVO_GUARDADO_VIEJO);
        resultado_llamar_mc();
        archivos_legacy = 0;
    }
    return 0;
}

static void alarma_tic(s32 id, u16 time, void *parametro)
{
    (void) id;
    (void) time;
    (void) parametro;
    iSignalSema(sema_tic);
    ExitHandler();
}

static void guardar_hilo(void *parametro)
{
    u32 t0 = ciclos_ps2();
    int intr;

    (void) parametro;
    cargar_partida();
    intr = DI();
    if (estado_carga == PENDIENTE_CARGA) { /* sin tarjeta o sin partida: imagen vacia */
        estado_carga = HECHO_CARGA;
    }
    if (intr) {
        EI();
    }
    registrar("memcard: lectura en segundo plano %u ms", (unsigned) ((ciclos_ps2() - t0) / 294912));
    ChangeThreadPriority(GetThreadId(), PRIORIDAD_HILO_GUARDADO);

    for (;;) {
        u32 generar;
        int r;

        WaitSema(sema_guardado);
        do {
            generar = guardado_ps2.generacion;
            SetAlarm(15734, alarma_tic, NULL);
            WaitSema(sema_tic);
        } while (generar != guardado_ps2.generacion);
        while (PollSema(sema_guardado) >= 0) {
        }
        if (!tomar_partida_modificada()) {
            continue;
        }
        if (estado_carga == CARGA_ABANDONED) {
            continue; /* no se sabe que hay en la tarjeta: no se pisa */
        }
        r = guardar_ahora();
        if (r == 0) {
            registrar("memcard: partida guardada (generacion %u)", (unsigned) generar);
        } else {
            registrar("memcard: no se pudo guardar (%d); se reintentara", r);
            marcar_partida_modificada_sin_aviso();
        }
    }
}

void pedir_guardado(void)
{
    if (sema_guardado >= 0) {
        SignalSema(sema_guardado);
    }
}

void inicializar_memory_card(void)
{
    ee_sema_t sema;
    ee_thread_t th;
    s32 tid;
    int devuelto;

    int x = modulos_mando_x_ps2();

    /* SIO2MAN (o XSIO2MAN) ya lo cargo inicializar_mandos_ps2(); MCMAN tiene que ser de la misma familia. */
    if (!cargar_modulo_iop(x ? "rom0:XMCMAN" : "rom0:MCMAN") || !cargar_modulo_iop(x ? "rom0:XMCSERV" : "rom0:MCSERV") ||
        (devuelto = mcInit(x ? MC_TYPE_XMC : MC_TYPE_MC)) < 0) {
        registrar("memcard: no se pudo iniciar libmc; se juega sin guardar");
    } else {
        mc_ok = 1;
    }

    memset(&sema, 0, sizeof(sema));
    sema.init_count = 0;
    sema.max_count = 64;
    sema_guardado = CreateSema(&sema);
    sema.max_count = 1;
    sema_tic = CreateSema(&sema);

    memset(&th, 0, sizeof(th));
    th.func = (void *) guardar_hilo;
    th.stack = pila_guardado;
    th.stack_size = sizeof(pila_guardado);
    th.gp_reg = &_gp;
    /* Empieza leyendo la partida (PRIORIDAD_HILO_CARGA */
    th.initial_priority = PRIORIDAD_HILO_CARGA;
    tid = CreateThread(&th);
    if (tid >= 0) {
        StartThread(tid, NULL);
    } else {
        estado_carga = HECHO_CARGA; /* sin hilo no hay lectura: imagen vacia */
        registrar("memcard: no se pudo crear el hilo de guardado");
    }
}
