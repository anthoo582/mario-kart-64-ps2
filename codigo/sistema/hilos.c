#include <kernel.h>
#include <string.h>

#include <ultra64.h>
#include <PR/os.h>

#include "sistema/sistema_ps2.h"

#define MAX_HILOS_PS2    16
#define PILA_HILO_PS2   (64 * 1024)

typedef struct {
    OSThread *thread;
    s32 ee_id;           /* id del hilo en el kernel del EE, -1 si no creado */
    s32 sema;           /* semaforo privado para bloquearse */
    void (*entry)(void *);
    void *arg;
} HiloPs2;

static HiloPs2 hilos[MAX_HILOS_PS2];
static u8 pilas[MAX_HILOS_PS2][PILA_HILO_PS2] __attribute__((aligned(16)));
static OSThread hilos_ajeno[MAX_HILOS_PS2]; /* hilos EE no creados por el juego */

extern void *_gp;

int prioridad_ee(OSPri prio)
{
    int ee;

    if (prio < 0) {
        prio = 0;
    } else if (prio > 255) {
        prio = 255;
    }
    ee = 127 - (prio * 125) / 255;
    return ee;
}

static s32 hacer_sema(int inicial, int max)
{
    ee_sema_t s;

    memset(&s, 0, sizeof(s));
    s.init_count = inicial;
    s.max_count = max;
    s.option = 0;
    return CreateSema(&s);
}

static inline int bloquear(void)
{
    return DI();
}

static inline void desbloquear(int intr)
{
    if (intr) {
        EI();
    }
}

static HiloPs2 *buscar_por_hilo(OSThread *t)
{
    int i;

    for (i = 0; i < MAX_HILOS_PS2; i++) {
        if (hilos[i].thread == t) {
            return &hilos[i];
        }
    }
    return NULL;
}

static HiloPs2 *reservar_ranura(void)
{
    int i;

    for (i = 0; i < MAX_HILOS_PS2; i++) {
        if (hilos[i].thread == NULL) {
            return &hilos[i];
        }
    }
    detener_por_error("sin huecos para hilos");
    return NULL;
}

/* Devuelve la entrada del hilo que esta corriendo */
static HiloPs2 *actual_(void)
{
    s32 id = GetThreadId();
    int i;
    HiloPs2 *ranura;

    for (i = 0; i < MAX_HILOS_PS2; i++) {
        if (hilos[i].thread != NULL && hilos[i].ee_id == id) {
            return &hilos[i];
        }
    }

    ranura = reservar_ranura();
    i = ranura - hilos;
    memset(&hilos_ajeno[i], 0, sizeof(OSThread));
    hilos_ajeno[i].priority = OS_PRIORITY_APPMAX;
    hilos_ajeno[i].state = OS_STATE_RUNNING;
    ranura->thread = &hilos_ajeno[i];
    ranura->ee_id = id;
    ranura->sema = hacer_sema(0, 1024);
    return ranura;
}

OSThread *hilo_actual(void)
{
    return actual_()->thread;
}

static void encolar(OSThread **cola, OSThread *t)
{
    while (*cola != NULL && (*cola)->priority >= t->priority) {
        cola = &(*cola)->next;
    }
    t->next = *cola;
    *cola = t;
}

static OSThread *pop(OSThread **cola)
{
    OSThread *t = *cola;

    if (t != NULL) {
        *cola = t->next;
        t->next = NULL;
    }
    return t;
}

static void despertar(OSThread *t)
{
    HiloPs2 *p = buscar_por_hilo(t);

    if (p != NULL) {
        t->state = OS_STATE_RUNNABLE;
        SignalSema(p->sema);
    }
}

void inicializar_hilos(void)
{
    memset(hilos, 0, sizeof(hilos));
}

static void trampolin_hilo(void *parametro)
{
    HiloPs2 *p = (HiloPs2 *) parametro;

    p->thread->state = OS_STATE_RUNNING;
    p->entry(p->arg);

    p->thread->state = OS_STATE_STOPPED;
    ExitThread();
}

s32 id_hilo_ee_de(OSId id)
{
    int i;

    for (i = 0; i < MAX_HILOS_PS2; i++) {
        if (hilos[i].thread != NULL && hilos[i].thread->id == id) {
            return hilos[i].ee_id;
        }
    }
    return -1;
}

void osCreateThread(OSThread *t, OSId id, void (*entry)(void *), void *parametro, void *sp, OSPri prio)
{
    HiloPs2 *p;
    ee_thread_t th;
    int index;

    (void) sp; /* Las pilas de N64 son de pocos KB: se usa una propia. */

    p = buscar_por_hilo(t);
    if (p == NULL) {
        p = reservar_ranura();
        p->sema = hacer_sema(0, 1024);
    }
    index = p - hilos;

    memset(t, 0, sizeof(OSThread));
    t->id = id;
    t->priority = prio;
    t->state = OS_STATE_STOPPED;

    p->thread = t;
    p->entry = entry;
    p->arg = parametro;

    memset(&th, 0, sizeof(th));
    th.func = (void *) trampolin_hilo;
    th.stack = pilas[index];
    th.stack_size = PILA_HILO_PS2;
    th.gp_reg = &_gp;
    th.initial_priority = prioridad_ee(prio);
    p->ee_id = CreateThread(&th);
    if (p->ee_id < 0) {
        detener_por_error("CreateThread fallo");
    }
}

void osStartThread(OSThread *t)
{
    HiloPs2 *p = buscar_por_hilo(t);

    if (p == NULL || p->ee_id < 0) {
        return;
    }
    if (t->state == OS_STATE_STOPPED) {
        t->state = OS_STATE_RUNNABLE;
        StartThread(p->ee_id, p);
    } else {
        ResumeThread(p->ee_id);
    }
}

void osStopThread(OSThread *t)
{
    HiloPs2 *p = (t == NULL) ? actual_() : buscar_por_hilo(t);

    if (p != NULL) {
        p->thread->state = OS_STATE_STOPPED;
        if (p->ee_id == GetThreadId()) {
            SleepThread();
        } else {
            SuspendThread(p->ee_id);
        }
    }
}

void osDestroyThread(OSThread *t)
{
    HiloPs2 *p = (t == NULL) ? actual_() : buscar_por_hilo(t);

    if (p == NULL) {
        return;
    }
    if (p->ee_id == GetThreadId()) {
        p->thread = NULL;
        ExitDeleteThread();
    }
    TerminateThread(p->ee_id);
    DeleteThread(p->ee_id);
    p->thread = NULL;
    p->ee_id = -1;
}

void osSetThreadPri(OSThread *t, OSPri prio)
{
    HiloPs2 *p = (t == NULL) ? actual_() : buscar_por_hilo(t);

    if (p == NULL) {
        return;
    }
    p->thread->priority = prio;
    ChangeThreadPriority(p->ee_id, prioridad_ee(prio));
}

OSPri osGetThreadPri(OSThread *t)
{
    HiloPs2 *p = (t == NULL) ? actual_() : buscar_por_hilo(t);

    return (p != NULL) ? p->thread->priority : 0;
}

OSId osGetThreadId(OSThread *t)
{
    HiloPs2 *p = (t == NULL) ? actual_() : buscar_por_hilo(t);

    return (p != NULL) ? p->thread->id : 0;
}

void osYieldThread(void)
{
    HiloPs2 *p = actual_();

    RotateThreadReadyQueue(prioridad_ee(p->thread->priority));
}

void osCreateMesgQueue(OSMesgQueue *mq, OSMesg *mens, s32 cantidad)
{
    mq->mtqueue = NULL;
    mq->fullqueue = NULL;
    mq->validCount = 0;
    mq->first = 0;
    mq->msgCount = cantidad;
    mq->msg = mens;
}

#ifdef SMK64_DEV
void comprobar_vigilancia(const char *where, void *llamador);
#define VIGILANCIA(w) comprobar_vigilancia(w, __builtin_return_address(0))
#else
#define VIGILANCIA(w) ((void) 0)
#endif

s32 osRecvMesg(OSMesgQueue *mq, OSMesg *mens, s32 bandera)
{
    OSThread *a_despertar;
    int intr;

    VIGILANCIA("osRecvMesg");
    intr = bloquear();
    while (mq->validCount == 0) {
        HiloPs2 *propio;

        if (bandera == OS_MESG_NOBLOCK) {
            desbloquear(intr);
            return -1;
        }
        propio = actual_();
        propio->thread->state = OS_STATE_WAITING;
        encolar(&mq->mtqueue, propio->thread);
        desbloquear(intr);
        WaitSema(propio->sema);
        intr = bloquear();
    }

    if (mens != NULL) {
        *mens = mq->msg[mq->first];
    }
    mq->first = (mq->first + 1) % mq->msgCount;
    mq->validCount--;

    a_despertar = pop(&mq->fullqueue);
    desbloquear(intr);

    if (a_despertar != NULL) {
        despertar(a_despertar);
    }
    return 0;
}

static s32 send(OSMesgQueue *mq, OSMesg mens, s32 bandera, int atascar)
{
    OSThread *a_despertar;
    int intr;

    VIGILANCIA("osSendMesg");
    intr = bloquear();
    while (mq->validCount >= mq->msgCount) {
        HiloPs2 *propio;

        if (bandera == OS_MESG_NOBLOCK) {
            desbloquear(intr);
            return -1;
        }
        propio = actual_();
        propio->thread->state = OS_STATE_WAITING;
        encolar(&mq->fullqueue, propio->thread);
        desbloquear(intr);
        WaitSema(propio->sema);
        intr = bloquear();
    }

    if (atascar) {
        mq->first = (mq->first + mq->msgCount - 1) % mq->msgCount;
        mq->msg[mq->first] = mens;
    } else {
        mq->msg[(mq->first + mq->validCount) % mq->msgCount] = mens;
    }
    mq->validCount++;

    a_despertar = pop(&mq->mtqueue);
    desbloquear(intr);

    if (a_despertar != NULL) {
        despertar(a_despertar);
    }
    return 0;
}

s32 osSendMesg(OSMesgQueue *mq, OSMesg mens, s32 bandera)
{
    return send(mq, mens, bandera, 0);
}

s32 osJamMesg(OSMesgQueue *mq, OSMesg mens, s32 bandera)
{
    return send(mq, mens, bandera, 1);
}

typedef struct {
    OSMesgQueue *queue;
    OSMesg msg;
} EventoPs2;

static EventoPs2 eventos[OS_NUM_EVENTS];

void osSetEventMesg(OSEvent e, OSMesgQueue *mq, OSMesg mens)
{
    if (e < OS_NUM_EVENTS) {
        eventos[e].queue = mq;
        eventos[e].msg = mens;
    }
}

void enviar_evento_sistema(OSEvent e)
{
    if (e < OS_NUM_EVENTS && eventos[e].queue != NULL &&
        osSendMesg(eventos[e].queue, eventos[e].msg, OS_MESG_NOBLOCK) != 0) {
        registrar("evento %d perdido: cola llena (%d mensajes)", (int) e, (int) eventos[e].queue->msgCount);
    }
}

static s32 duenio_critico = -1;
static int profundidad_critico;
static s32 sema_critico = -1;

u32 __osDisableInt(void)
{
    s32 id = GetThreadId();

    if (sema_critico < 0) {
        sema_critico = hacer_sema(1, 1);
    }
    if (duenio_critico != id) {
        WaitSema(sema_critico);
        duenio_critico = id;
    }
    profundidad_critico++;
    return 1;
}

void __osRestoreInt(u32 mascara)
{
    (void) mascara;
    if (duenio_critico == GetThreadId() && --profundidad_critico == 0) {
        duenio_critico = -1;
        SignalSema(sema_critico);
    }
}
