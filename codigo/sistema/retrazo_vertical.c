#include <kernel.h>
#include <string.h>

#include <ultra64.h>
#include <PR/os.h>

#include "sistema/sistema_ps2.h"

#define PRIORIDAD_HILO_VI 1 /* por encima de todos los hilos del juego */
/* Huecos que los retrazos dejan libres en la cola del VI. El planificador (hilo3_video) ejecuta
   la tarea de graficos dentro de su bucle: si tarda 16 retrazos, la cola se llenaba, el aviso de
   fin de tarea (SP/DP) se perdia y el juego esperaba ese frame para siempre. */
#define HUECOS_LIBRES_VI 4

static s32 vi_sema = -1;
static volatile u32 cantidad_vblank;
static u8 pila_vi[16 * 1024] __attribute__((aligned(16)));

static OSMesgQueue *cola_vi;
static OSMesg mens_vi;
static u32 cantidad_retrazo_vi = 1;
static OSMesgQueue *volatile cola_audio;
static OSMesg mens_audio;

extern void *_gp;

void gs_en_vblank(void);

static s32 manejador_vblank(s32 causa)
{
    (void) causa;
    cantidad_vblank++;
    gs_en_vblank(); /* cambio de framebuffer, como el VI del N64 */
    iSignalSema(vi_sema);
    ExitHandler();
    return 0;
}

u32 contador_vblank(void)
{
    return cantidad_vblank;
}

static void hilo_vi(void *parametro)
{
    u32 retrazo = 0;

    (void) parametro;
    for (;;) {
        WaitSema(vi_sema);

        retrazo++;
        if (cola_vi != NULL && cantidad_retrazo_vi != 0 && (retrazo % cantidad_retrazo_vi) == 0 &&
            (cola_vi->msgCount <= 2 * HUECOS_LIBRES_VI || cola_vi->validCount + HUECOS_LIBRES_VI < cola_vi->msgCount)) {
            osSendMesg(cola_vi, mens_vi, OS_MESG_NOBLOCK);
        }
        if (cola_audio != NULL) {
            osSendMesg(cola_audio, mens_audio, OS_MESG_NOBLOCK);
        }
        avanzar_temporizadores();
    }
}

void inicializar_retrazo(void)
{
    ee_sema_t sema;
    ee_thread_t th;
    s32 id;

    memset(&sema, 0, sizeof(sema));
    sema.init_count = 0;
    sema.max_count = 1; /* retrazos perdidos no se acumulan */
    vi_sema = CreateSema(&sema);

    memset(&th, 0, sizeof(th));
    th.func = (void *) hilo_vi;
    th.stack = pila_vi;
    th.stack_size = sizeof(pila_vi);
    th.gp_reg = &_gp;
    th.initial_priority = PRIORIDAD_HILO_VI;
    id = CreateThread(&th);
    if (id < 0) {
        detener_por_error("no se pudo crear el hilo del VI");
    }
    StartThread(id, NULL);

    AddIntcHandler(INTC_VBLANK_S, manejador_vblank, 0);
    EnableIntc(INTC_VBLANK_S);
}

void osViSetEvent(OSMesgQueue *mq, OSMesg mens, u32 cantidad_retrazo)
{
    cola_vi = mq;
    mens_vi = mens;
    cantidad_retrazo_vi = cantidad_retrazo;
}

/* El hilo de audio del juego trabaja un bloque por retrazo */
void fijar_evento_audio_vi_os_ps2(OSMesgQueue *mq, OSMesg mens)
{
    mens_audio = mens;
    cola_audio = mq;
}

static OSTimer *temporizadores;

u32 osSetTimer(OSTimer *t, OSTime cuenta_regresiva, OSTime intervalo, OSMesgQueue *mq, OSMesg mens)
{
    OSTimer **p;
    int intr = DI();

    t->interval = intervalo;
    t->remaining = tiempo_actual() + ((cuenta_regresiva != 0) ? cuenta_regresiva : intervalo);
    t->mq = mq;
    t->msg = mens;

    /* Si ya estaba en la lista, se reprograma sin duplicarlo. */
    for (p = &temporizadores; *p != NULL; p = &(*p)->next) {
        if (*p == t) {
            if (intr) {
                EI();
            }
            return 0;
        }
    }
    t->next = temporizadores;
    t->prev = NULL;
    temporizadores = t;
    if (intr) {
        EI();
    }
    return 0;
}

s32 osStopTimer(OSTimer *t)
{
    OSTimer **p;
    int intr = DI();
    s32 devuelto = -1;

    for (p = &temporizadores; *p != NULL; p = &(*p)->next) {
        if (*p == t) {
            *p = t->next;
            devuelto = 0;
            break;
        }
    }
    if (intr) {
        EI();
    }
    return devuelto;
}

void avanzar_temporizadores(void)
{
    OSTimer **p = &temporizadores;
    u64 ahora = tiempo_actual();
    struct {
        OSMesgQueue *mq;
        OSMesg msg;
    } debido[16];
    int ndue = 0;
    int guardia = 0;
    int roto = 0;
    int i;
    int intr = DI();

    while (*p != NULL) {
        OSTimer *t = *p;

        /* El juego usa unos pocos temporizadores */
        if (++guardia > 64) {
            temporizadores = NULL;
            roto = 1;
            break;
        }
        if (ahora >= t->remaining) {
            if (t->mq != NULL && ndue < (int) (sizeof(debido) / sizeof(debido[0]))) {
                debido[ndue].mq = t->mq;
                debido[ndue].msg = (OSMesg) t->msg;
                ndue++;
            }
            if (t->interval != 0) {
                t->remaining += t->interval;
            } else {
                *p = t->next;
                continue;
            }
        }
        p = &t->next;
    }
    if (intr) {
        EI();
    }
    if (roto) {
        registrar("temporizadores: lista rota (ciclo), se vacia");
    }
    for (i = 0; i < ndue; i++) {
        osSendMesg(debido[i].mq, debido[i].msg, OS_MESG_NOBLOCK);
    }
}
