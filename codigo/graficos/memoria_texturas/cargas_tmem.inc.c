// Cargas tmem

#define TAMANIO_TMEM 4096

TileRdp tiles_rdp[8];
EstadisticasTmem estadisticas_tmem;
u32 loads_malo_tmem; /* cargas descartadas o recortadas (display list danada) */

static u8 s_tmem[TAMANIO_TMEM] __attribute__((aligned(16)));
static u32 tmem_version;

static int modo_pasada;
static int pasada_fallido;
static int desalojado_en_pasada;
static u32 serie_pasada = 1;
static u32 contador_carga; /* cargas desde el principio de la pasada */
/* Cargas recientes (anillo) */
#define ANILLO_CARGA 32

static struct {
    u16 start, end; /* palabras de 64 bits [start, end) */
    const u8 *src;
} anillo_carga[ANILLO_CARGA];
static u32 siguiente_anillo_carga;

static void cargar_nota(u32 palabra_tmem, u32 palabras, const u8 *orig_)
{
    u32 end = palabra_tmem + palabras;

    if (palabra_tmem >= TAMANIO_TMEM / 8) {
        return;
    }
    if (end > TAMANIO_TMEM / 8 || end < palabra_tmem) {
        end = TAMANIO_TMEM / 8;
    }
    anillo_carga[siguiente_anillo_carga % ANILLO_CARGA].start = (u16) palabra_tmem;
    anillo_carga[siguiente_anillo_carga % ANILLO_CARGA].end = (u16) (end > palabra_tmem ? end : palabra_tmem + 1);
    anillo_carga[siguiente_anillo_carga % ANILLO_CARGA].src = orig_;
    siguiente_anillo_carga++;
}

static const u8 *cargar_orig_en(u32 palabra_tmem)
{
    u32 i;

    for (i = 1; i <= ANILLO_CARGA && i <= siguiente_anillo_carga; i++) {
        const typeof(anillo_carga[0]) *l = &anillo_carga[(siguiente_anillo_carga - i) % ANILLO_CARGA];

        if (l->start <= palabra_tmem && palabra_tmem < l->end) {
            return (l->start == palabra_tmem) ? l->src : NULL;
        }
    }
    return NULL;
}

/* Firmas de paleta */
#define BANCOS_PALETA 16
#define PRIMERA_PALABRA_PALETA 256

static u32 firmas_paleta[BANCOS_PALETA];
static u8 firmas_paleta_validas[BANCOS_PALETA];

static void invalidar_firmas_paleta(u32 empezar, u32 palabras)
{
    u32 b, end;

    empezar &= 511;
    if (palabras >= 512 || empezar + palabras > 512) {
        memset(firmas_paleta_validas, 0, sizeof(firmas_paleta_validas)); /* da la vuelta: todo */
        return;
    }
    end = empezar + palabras;
    if (end <= PRIMERA_PALABRA_PALETA) {
        return;
    }
    if (empezar < PRIMERA_PALABRA_PALETA) {
        empezar = PRIMERA_PALABRA_PALETA;
    }
    for (b = (empezar - PRIMERA_PALABRA_PALETA) / 16; b <= (end - 1 - PRIMERA_PALABRA_PALETA) / 16; b++) {
        firmas_paleta_validas[b] = 0;
    }
}

static u32 palabras_hash(const u32 *p, u32 n, u32 h);

static struct {
    const u8 *addr;
    u32 fmt, siz, width;
} s_timg;

void tmem_fijar_imagen(const void *direccion, u32 fmt, u32 siz, u32 ancho)
{
    s_timg.addr = (const u8 *) direccion;
    s_timg.fmt = fmt;
    s_timg.siz = siz;
    s_timg.width = ancho;
}

const void *tmem_timg(u32 *ancho_siz_fmt)
{
    *ancho_siz_fmt = (s_timg.fmt << 28) | (s_timg.siz << 24) | s_timg.width;
    return s_timg.addr;
}

void tmem_fijar_tile(u32 w0, u32 w1)
{
    TileRdp *t = &tiles_rdp[(w1 >> 24) & 7];

    t->fmt = (w0 >> 21) & 7;
    t->siz = (w0 >> 19) & 3;
    t->line = (w0 >> 9) & 0x1FF;
    t->tmem = w0 & 0x1FF;
    t->palette = (w1 >> 20) & 0xF;
    t->cmt = (w1 >> 18) & 3;
    t->maskt = (w1 >> 14) & 0xF;
    t->shiftt = (w1 >> 10) & 0xF;
    t->cms = (w1 >> 8) & 3;
    t->masks = (w1 >> 4) & 0xF;
    t->shifts = w1 & 0xF;
}

void tmem_fijar_tamanio_tile(u32 w0, u32 w1)
{
    TileRdp *t = &tiles_rdp[(w1 >> 24) & 7];

    t->uls = (w0 >> 12) & 0xFFF;
    t->ult = w0 & 0xFFF;
    t->lrs = (w1 >> 12) & 0xFFF;
    t->lrt = w1 & 0xFFF;
}

#ifdef SMK64_TMEM_CHECK
static u8 tmem_sombra[TAMANIO_TMEM] __attribute__((aligned(16)));
#define SOMBRA8(a, v)  (tmem_sombra[(a)] = (v))
#define SOMBRA64(w, v) (((u64 *) tmem_sombra)[(w)] = (v))
#else
#define SOMBRA8(a, v)  ((void) 0)
#define SOMBRA64(w, v) ((void) 0)
#endif

static inline void escribir_tmem(u32 direccion, u8 value)
{
    s_tmem[direccion & (TAMANIO_TMEM - 1)] = value;
    SOMBRA8(direccion & (TAMANIO_TMEM - 1), value);
}

/* La TMEM vista como 512 palabras de 64 bits */
#define PALABRAS_TMEM (TAMANIO_TMEM / 8)
#define tmem_64 ((u64 *) s_tmem)

static inline u64 cargar_u64(const u8 *p)
{
    u64 v;

    memcpy(&v, p, 8);
    return v;
}

static inline u64 intercambiar_mitades(u64 v)
{
    return (v >> 32) | (v << 32);
}

/* Cargas diferidas (LOADBLOCK) */
#define MAX_REGISTROS_CARGA 16

typedef struct {
    const u64 *src;  /* primera palabra que se aplica */
    u32 sig;
    u32 contador_inicial, dxt;   /* contador de filas (dxt) al empezar */
    u16 start, words; /* palabras [start, start + words), sin dar la vuelta */
    u8 pendiente;      /* aun no copiada a la TMEM */
    u8 intacta;       /* nada la piso despues: la firma describe la TMEM */
    u8 firma_calculada;     /* la firma se calcula al primer uso (o al copiarla) */
} RegistroCarga;

static RegistroCarga registros_carga[MAX_REGISTROS_CARGA];
static u32 cantidad_registros_carga;
static u32 cargas_pendientes;

#ifdef SMK64_SIGMAP
u32 mapa_sig[512];
#endif

static u32 cargar_hash_sig(const u64 *orig_, u32 palabras, u32 acc0, u32 dxt)
{
#if defined(SMK64_PROF) || defined(SMK64_MEDIDOR)
    estadisticas_tmem.bytes_firmas += palabras * 8;
#endif
#ifdef SMK64_SIGMAP
    if (((uintptr_t) orig_ >> 16) < 512) {
        mapa_sig[(uintptr_t) orig_ >> 16] += palabras * 8;
    }
#endif
    return palabras_hash((const u32 *) orig_, palabras * 2, (dxt * 0x9E3779B1u) ^ (acc0 * 0x85EBCA6Bu) ^ palabras);
}

#define TAMANIO_SIGMEMO 2048 /* potencia de 2; una colision solo recalcula */

typedef struct {
    const u64 *src;
    u32 contador_inicial;
    u32 sig;
    u32 gen;
    u16 words;
    u16 dxt;
} SigMemo;

static SigMemo sig_memo[TAMANIO_SIGMEMO];
static uintptr_t inicio_estatico, fin_estatico;
static s32 estado_juego_estatico;
static volatile u32 gen_estatico = 1;
extern s32 estado_juego;

#define MAX_BLOQUE 32
static struct {
    uintptr_t start, end;
} bloque[MAX_BLOQUE];
static volatile int cantidad_bloque;
static uintptr_t inicio_perm, fin_perm;

void ps2_tmem_estatico_rango(const void *empezar, u32 size)
{
    int intr = DI();

    gen_estatico++;
    inicio_estatico = (uintptr_t) empezar;
    fin_estatico = (empezar != NULL) ? (uintptr_t) empezar + size : 0;
    if (empezar == NULL) {
        cantidad_bloque = 0; /* otra escena: los bloques tambien */
    }
    estado_juego_estatico = estado_juego;
    if (intr) {
        EI();
    }
}

void ps2_tmem_estatico_bloque(const void *empezar, u32 size, int permanente)
{
    uintptr_t a = (uintptr_t) empezar;
    int intr = DI();

    gen_estatico++;
    if (permanente) {
        inicio_perm = a;
        fin_perm = a + size;
    } else if (cantidad_bloque > 0 && bloque[cantidad_bloque - 1].end == a) {
        bloque[cantidad_bloque - 1].end = a + size;
    } else if (cantidad_bloque < MAX_BLOQUE) {
        bloque[cantidad_bloque].start = a;
        bloque[cantidad_bloque].end = a + size;
        cantidad_bloque++;
        if (fin_estatico == 0) {
            estado_juego_estatico = estado_juego;
        }
    }
    if (intr) {
        EI();
    }
}

static int bloque_estatico(uintptr_t a, u32 largo)
{
    int i;

    if (a >= inicio_perm && a + largo <= fin_perm) {
        return 2;
    }
    for (i = 0; i < cantidad_bloque; i++) {
        if (a >= bloque[i].start && a + largo <= bloque[i].end) {
            return 1;
        }
    }
    return 0;
}

int estatico_origen_tmem(const void *orig_, u32 largo, u32 *generar)
{
    uintptr_t a = (uintptr_t) orig_;
    int bloque_2;

    *generar = gen_estatico;
    if (a >= inicio_estatico && a + largo <= fin_estatico) {
        return estado_juego == estado_juego_estatico;
    }
    bloque_2 = bloque_estatico(a, largo);
    return bloque_2 == 2 || (bloque_2 == 1 && estado_juego == estado_juego_estatico);
}

void escribir_ram_tmem_ps2(const void *dst, u32 size)
{
    uintptr_t a = (uintptr_t) dst;
    int golpear = (a < fin_estatico && a + size > inicio_estatico) || (a < fin_perm && a + size > inicio_perm);
    int i;

    for (i = 0; i < cantidad_bloque && !golpear; i++) {
        golpear = a < bloque[i].end && a + size > bloque[i].start;
    }
    if (golpear) {
        int intr = DI();

        gen_estatico++;
        if (intr) {
            EI();
        }
    }
}

int ps2_kart_sprite_clave(const void *p, u32 largo, u32 *clave);

static u32 firma_carga(const u64 *orig_, u32 palabras, u32 acc0, u32 dxt)
{
    uintptr_t a = (uintptr_t) orig_;
    SigMemo *m;
    u32 generar, sig;

    if (dxt > 0xFFFF) {
        return cargar_hash_sig(orig_, palabras, acc0, dxt);
    }
    /* Sprite de kart */
    if (ps2_kart_sprite_clave(orig_, palabras * 8, &sig)) {
        u32 v[4] = { sig, palabras, acc0, dxt };

        return palabras_hash(v, 4, 0x4B415254u);
    }
    if (a < inicio_estatico || a + palabras * 8 > fin_estatico) {
        int bloque_2 = bloque_estatico(a, palabras * 8);

        if (bloque_2 == 0) {
            return cargar_hash_sig(orig_, palabras, acc0, dxt);
        }
        if (bloque_2 == 1 && estado_juego != estado_juego_estatico) {
            ps2_tmem_estatico_rango(NULL, 0); /* otra escena: la RAM ya es de otros */
            return cargar_hash_sig(orig_, palabras, acc0, dxt);
        }
    } else if (estado_juego != estado_juego_estatico) {
        ps2_tmem_estatico_rango(NULL, 0); /* otra escena: la RAM ya es de otros */
        return cargar_hash_sig(orig_, palabras, acc0, dxt);
    }
    generar = gen_estatico;
    m = &sig_memo[((a >> 3) ^ (a >> 13) ^ (acc0 * 7u) ^ palabras) & (TAMANIO_SIGMEMO - 1)];
    if (m->gen == generar && m->src == orig_ && m->words == palabras && m->contador_inicial == acc0 && m->dxt == dxt) {
#ifdef SMK64_PROF
        estadisticas_tmem.golpes_memo_sig++;
#endif
#ifdef SMK64_SIGMEMO_CHECK
        if (cargar_hash_sig(orig_, palabras, acc0, dxt) != m->sig) {
            estadisticas_tmem.malo_memo_sig++;
            registrar("SIGMEMO: firma memorizada vieja en %p (%u palabras)", orig_, (unsigned) palabras);
        }
#endif
        return m->sig;
    }
    sig = cargar_hash_sig(orig_, palabras, acc0, dxt);
    m->src = orig_;
    m->contador_inicial = acc0;
    m->sig = sig;
    m->words = (u16) palabras;
    m->dxt = (u16) dxt;
    m->gen = generar; /* la de antes de leer: si hubo una escritura, ya caduco */
    return sig;
}

static void aplicar_carga(RegistroCarga *r)
{
    u64 *dst = &tmem_64[r->start];
    u32 acc = r->contador_inicial;
    u32 w;

    for (w = 0; w < r->words; w++) {
        u64 v = r->src[w];

        /* dxt cuenta las filas: en las impares se cruzan las mitades. */
        dst[w] = (acc & 0x800) ? intercambiar_mitades(v) : v;
        acc += r->dxt;
    }
    r->pendiente = 0;
}

/* Aplica en orden las cargas diferidas registros_carga[0..upto). */
static void aplicar_cargas_hasta(u32 hasta)
{
    u32 i;

    for (i = 0; i < hasta; i++) {
        RegistroCarga *r = &registros_carga[i];

        if (r->pendiente) {
            aplicar_carga(r);
            cargas_pendientes--;
            if (r->intacta) {
                /* Lo que quedo en la TMEM es lo que se lee ahora de la RAM. */
                r->sig = firma_carga(r->src, r->words, r->contador_inicial, r->dxt);
                r->firma_calculada = 1;
            }
#ifdef SMK64_PROF
            estadisticas_tmem.bytes_carga += r->words * 8;
#endif
        }
    }
}

#ifdef SMK64_TMEM_CHECK
static void comparar_con_sombra(u32 empezar, u32 largo, const char *where)
{
    static u32 errores;
    u32 i;

    for (i = 0; i < largo && i < TAMANIO_TMEM; i++) {
        u32 a = (empezar + i) & (TAMANIO_TMEM - 1);

        if (s_tmem[a] != tmem_sombra[a]) {
            if (errores++ < 20) {
                registrar("TMEM_CHECK: %s lee %u+%u y difiere de la sombra en %u", where, (unsigned) empezar,
                        (unsigned) largo, (unsigned) a);
            }
            return;
        }
    }
}
#endif

/* Todas las cargas diferidas */
static void aplicar_cargas_pendientes(void)
{
    if (cargas_pendientes != 0) {
        aplicar_cargas_hasta(cantidad_registros_carga);
    }
}

static void aplicar_cargas_que_tocan(u32 empezar, u32 palabras)
{
    u32 i, hasta = 0;

    if (cargas_pendientes == 0) {
        return;
    }
    empezar &= PALABRAS_TMEM - 1;
    if (palabras >= PALABRAS_TMEM || empezar + palabras > PALABRAS_TMEM) {
        aplicar_cargas_pendientes();
        return;
    }
    for (i = 0; i < cantidad_registros_carga; i++) {
        const RegistroCarga *r = &registros_carga[i];

        if (r->pendiente && r->start < empezar + palabras && empezar < (u32) r->start + r->words) {
            hasta = i + 1;
        }
    }
    aplicar_cargas_hasta(hasta);
}

static void marcar_cargas_pisadas(u32 empezar, u32 palabras)
{
    u32 i, n = 0;
    int da_la_vuelta = (empezar & (PALABRAS_TMEM - 1)) + palabras > PALABRAS_TMEM;
    int cubre_todo = palabras >= PALABRAS_TMEM;

    empezar &= PALABRAS_TMEM - 1;
    for (i = 0; i < cantidad_registros_carga; i++) {
        RegistroCarga *r = &registros_carga[i];
        int tapada = cubre_todo || (!da_la_vuelta && r->start >= empezar && (u32) r->start + r->words <= empezar + palabras);

        if (tapada || da_la_vuelta || (r->start < empezar + palabras && empezar < (u32) r->start + r->words)) {
            r->intacta = 0;
        }
        if (tapada && r->pendiente) {
            r->pendiente = 0; /* tapada entera: no hace falta copiarla */
            cargas_pendientes--;
        }
        if (r->intacta || r->pendiente) {
            registros_carga[n++] = *r;
        }
    }
    cantidad_registros_carga = n;
}

/* Carga inmediata (no diferible) */
static void antes_de_escritura_directa(u32 empezar, u32 palabras)
{
    aplicar_cargas_que_tocan(empezar, palabras);
    marcar_cargas_pisadas(empezar, palabras);
}

/* Registro intacto que contiene [start, start + words), o NULL */
static RegistroCarga *carga_que_contiene(u32 empezar, u32 palabras)
{
    u32 i;

    for (i = 0; i < cantidad_registros_carga; i++) {
        RegistroCarga *r = &registros_carga[i];

        if (r->intacta && r->start <= empezar && empezar + palabras <= (u32) r->start + r->words) {
            return r;
        }
    }
    return NULL;
}

/* Carga descartada o recortada (display list danada) */
static void cargar_malo(const char *op, const char *por_que, u32 w0, u32 w1)
{
    if (loads_malo_tmem++ < 4) {
        registrar("%s %s: %08x %08x timg %p ancho %u siz %u linea %u", op, por_que, (unsigned) w0, (unsigned) w1,
                (const void *) s_timg.addr, (unsigned) s_timg.width, (unsigned) s_timg.siz,
                (unsigned) tiles_rdp[(w1 >> 24) & 7].line);
    }
}

/* [p, p + bytes) esta en la RAM del programa (1-32 MB) */
static int orig_en_ram(const u8 *p, u32 bytes)
{
    uintptr_t a = (uintptr_t) p;

    return a >= 0x00100000 && a < 0x02000000 && bytes <= 0x02000000 - a;
}

void tmem_cargar_bloque(u32 w0, u32 w1)
{
    TileRdp *t = &tiles_rdp[(w1 >> 24) & 7];
    u32 uls = (w0 >> 12) & 0xFFF;
    u32 ult = w0 & 0xFFF;
    u32 lrs = (w1 >> 12) & 0xFFF;
    u32 dxt = w1 & 0xFFF;
    u32 texels;
    u32 dst = t->tmem * 8;
    u32 i;

    if (s_timg.addr == NULL) {
        return;
    }
    t->uls = uls << 2;
    t->ult = ult << 2;
    t->lrs = lrs << 2;
    contador_carga++;
    if (modo_pasada == REPETICION_PASADA_TMEM) {
        return; /* la TMEM no se usa: las texturas salen de lo grabado */
    }
    tmem_version++;
    /* Un LOADBLOCK con lrs < uls no carga nada util */
    if (lrs < uls) {
        return;
    }
    texels = lrs - uls + 1;
    if (texels > 2048) {
        texels = 2048;
    }

    if (s_timg.siz == G_IM_SIZ_32b) {
        const u8 *orig_ = s_timg.addr + (ult * s_timg.width + uls) * 4;

        if (!orig_en_ram(orig_, texels * 4)) {
            cargar_malo("LOADBLOCK", "fuera de la RAM", w0, w1);
            return;
        }
        cargar_nota(t->tmem, (texels * 2 + 7) / 8, orig_);
        antes_de_escritura_directa(0, PALABRAS_TMEM); /* escribe en las dos mitades */
        memset(firmas_paleta_validas, 0, sizeof(firmas_paleta_validas));
        for (i = 0; i < texels; i++) {
            u32 a = (dst + i * 2) & 0x7FF;

            s_tmem[a] = orig_[i * 4 + 0];
            s_tmem[a + 1] = orig_[i * 4 + 1];
            s_tmem[0x800 + a] = orig_[i * 4 + 2];
            s_tmem[0x800 + a + 1] = orig_[i * 4 + 3];
            SOMBRA8(a, orig_[i * 4 + 0]);
            SOMBRA8(a + 1, orig_[i * 4 + 1]);
            SOMBRA8(0x800 + a, orig_[i * 4 + 2]);
            SOMBRA8(0x800 + a + 1, orig_[i * 4 + 3]);
        }
        return;
    } else {
        u32 bytes = (s_timg.siz == G_IM_SIZ_4b) ? (texels + 1) / 2 : texels << (s_timg.siz - 1);
        u32 palabras = (bytes + 7) / 8;
        const u8 *orig_ = s_timg.addr + (((ult * s_timg.width + uls) << s_timg.siz) >> 1);
        u32 primer = (palabras > PALABRAS_TMEM) ? palabras - PALABRAS_TMEM : 0; /* lo anterior queda pisado */
        u32 empezar = t->tmem + primer;
        u32 n = palabras - primer;
        u32 acc = primer * dxt;
        u32 w;

        if (!orig_en_ram(orig_, palabras * 8)) {
            cargar_malo("LOADBLOCK", "fuera de la RAM", w0, w1);
            return;
        }
        cargar_nota(t->tmem, palabras, orig_);
        invalidar_firmas_paleta(empezar, n);

        if (((uintptr_t) orig_ & 7) == 0 && empezar + n <= PALABRAS_TMEM) {
            /* Lo habitual (el N64 exige texturas alineadas a 8 bytes) */
            RegistroCarga *r;

            marcar_cargas_pisadas(empezar, n);
            if (cantidad_registros_carga == MAX_REGISTROS_CARGA) {
                aplicar_cargas_pendientes();
                marcar_cargas_pisadas(0, 0); /* quita los que ya no sirven */
                if (cantidad_registros_carga == MAX_REGISTROS_CARGA) {
                    memmove(&registros_carga[0], &registros_carga[1], sizeof(registros_carga[0]) * (MAX_REGISTROS_CARGA - 1));
                    cantidad_registros_carga--;
                }
            }
            r = &registros_carga[cantidad_registros_carga++];
            r->src = (const u64 *) orig_ + primer;
            r->words = (u16) n;
            r->start = (u16) empezar;
            r->contador_inicial = acc;
            r->dxt = dxt;
            r->firma_calculada = 0; /* muchas cargas no las usa ninguna preparacion */
            r->pendiente = 1;
            r->intacta = 1;
            cargas_pendientes++;
#ifdef SMK64_TMEM_CHECK
            for (w = 0; w < n; w++, acc += dxt) {
                SOMBRA64(empezar + w, (acc & 0x800) ? intercambiar_mitades(r->src[w]) : r->src[w]);
            }
#endif
            return;
        }
        antes_de_escritura_directa(empezar, n);
#ifdef SMK64_PROF
        estadisticas_tmem.bytes_carga += n * 8;
#endif
        for (w = primer; w < palabras; w++) {
            u64 v = cargar_u64(orig_ + w * 8);

            if (acc & 0x800) {
                v = intercambiar_mitades(v);
            }
            tmem_64[(t->tmem + w) & (PALABRAS_TMEM - 1)] = v;
            SOMBRA64((t->tmem + w) & (PALABRAS_TMEM - 1), v);
            acc += dxt;
        }
    }
}

void tmem_cargar_tile(u32 w0, u32 w1)
{
    TileRdp *t = &tiles_rdp[(w1 >> 24) & 7];
    u32 sl = ((w0 >> 12) & 0xFFF) >> 2;
    u32 tl = (w0 & 0xFFF) >> 2;
    u32 sh = ((w1 >> 12) & 0xFFF) >> 2;
    u32 th = (w1 & 0xFFF) >> 2;
    u32 renglon, primer_renglon, primer_byte, bytes_renglon, stride;

    if (s_timg.addr == NULL) {
        return;
    }
    t->uls = (w0 >> 12) & 0xFFF;
    t->ult = w0 & 0xFFF;
    t->lrs = (w1 >> 12) & 0xFFF;
    t->lrt = w1 & 0xFFF;
    contador_carga++;
    if (modo_pasada == REPETICION_PASADA_TMEM) {
        return;
    }
    tmem_version++;
    /* Esquina inferior antes que la superior */
    if (th < tl || sh < sl) {
        return;
    }

    /* Bytes de una fila en el origen y en la TMEM (32 bits */
    bytes_renglon = ((sh - sl + 1) << s_timg.siz) >> 1;
    {
        u32 primer = ((tl * s_timg.width + sl) << s_timg.siz) >> 1;
        u32 ultimo = (((th * s_timg.width + sl) << s_timg.siz) >> 1) + bytes_renglon;

        if (!orig_en_ram(s_timg.addr + primer, ultimo - primer)) {
            cargar_malo("LOADTILE", "fuera de la RAM", w0, w1);
            return;
        }
    }

    /* El RDP copia a una TMEM de 4 KB (2 KB por mitad con 32 bits) */
    {
        u32 cap = (s_timg.siz == G_IM_SIZ_32b) ? TAMANIO_TMEM / 2 : TAMANIO_TMEM;
        u32 dst_bytes = (s_timg.siz == G_IM_SIZ_32b) ? (sh - sl + 1) * 2 : bytes_renglon;
        u32 renglones = th - tl + 1;
        u32 fit_renglones;

        stride = t->line * 8;
        fit_renglones = (stride == 0) ? 1 : (cap + stride - 1) / stride + 1;
        primer_renglon = (renglones > fit_renglones) ? renglones - fit_renglones : 0;
        primer_byte = (dst_bytes > cap) ? (dst_bytes - cap) & ~7u : 0;
        if (primer_renglon != 0 || primer_byte != 0) {
            cargar_malo("LOADTILE", "mayor que la TMEM", w0, w1);
        }
    }

    cargar_nota(t->tmem, (th - tl + 1) * t->line, s_timg.addr + (((tl * s_timg.width + sl) << s_timg.siz) >> 1));
    if (s_timg.siz == G_IM_SIZ_32b) {
        antes_de_escritura_directa(0, PALABRAS_TMEM);
        memset(firmas_paleta_validas, 0, sizeof(firmas_paleta_validas));
    } else {
        u32 span = (th - tl) * t->line + ((((sh - sl + 1) << s_timg.siz) >> 1) + 7) / 8;

        antes_de_escritura_directa(t->tmem, span);
        invalidar_firmas_paleta(t->tmem, span);
    }

#ifdef SMK64_PROF
    estadisticas_tmem.bytes_carga += (th - tl + 1) * (((sh - sl + 1) << s_timg.siz) >> 1);
#endif
    for (renglon = primer_renglon; renglon <= th - tl; renglon++) {
        u32 renglon_dst = t->tmem * 8 + renglon * stride;
        u32 intercambiar = (renglon & 1) ? 4 : 0;
        u32 s;

        if (s_timg.siz == G_IM_SIZ_32b) {
            for (s = primer_byte / 2; s <= sh - sl; s++) {
                const u8 *p = s_timg.addr + ((tl + renglon) * s_timg.width + sl + s) * 4;
                u32 a = ((renglon_dst + s * 2) ^ intercambiar) & 0x7FF;

                s_tmem[a] = p[0];
                s_tmem[a + 1] = p[1];
                s_tmem[0x800 + a] = p[2];
                s_tmem[0x800 + a + 1] = p[3];
                SOMBRA8(a, p[0]);
                SOMBRA8(a + 1, p[1]);
                SOMBRA8(0x800 + a, p[2]);
                SOMBRA8(0x800 + a + 1, p[3]);
            }
        } else {
            u32 bpr = bytes_renglon;
            const u8 *p = s_timg.addr + ((((tl + renglon) * s_timg.width + sl) << s_timg.siz) >> 1);

            /* renglon_dst y primer_byte son multiplos de 8 */
            s = primer_byte;
            if (((uintptr_t) p & 7) == 0) {
                const u64 *p64 = (const u64 *) (p + s);
                u32 dw = (renglon_dst + s) >> 3;

                for (; s + 8 <= bpr; s += 8, dw++) {
                    u64 v = *p64++;

                    tmem_64[dw & (PALABRAS_TMEM - 1)] = intercambiar ? intercambiar_mitades(v) : v;
                    SOMBRA64(dw & (PALABRAS_TMEM - 1), intercambiar ? intercambiar_mitades(v) : v);
                }
            }
            for (; s + 8 <= bpr; s += 8) {
                u64 v = cargar_u64(p + s);

                tmem_64[((renglon_dst + s) >> 3) & (PALABRAS_TMEM - 1)] = intercambiar ? intercambiar_mitades(v) : v;
                SOMBRA64(((renglon_dst + s) >> 3) & (PALABRAS_TMEM - 1), intercambiar ? intercambiar_mitades(v) : v);
            }
            for (; s < bpr; s++) {
                escribir_tmem((renglon_dst + s) ^ intercambiar, p[s]);
            }
        }
    }
}

int ps2_kart_paleta_clave(const void *p, u32 largo, u32 *clave);

/* Ultima paleta copiada a la TMEM (ver tmem_cargar_tlut). */
static struct {
    u32 key, tmem, n;
    int valido;
} act_pal;

void tmem_cargar_tlut(u32 w0, u32 w1)
{
    TileRdp *t = &tiles_rdp[(w1 >> 24) & 7];
    u32 sl = ((w0 >> 12) & 0xFFF) >> 2;
    u32 sh = ((w1 >> 12) & 0xFFF) >> 2;
    u32 i, n;

    if (s_timg.addr == NULL) {
        return;
    }
    contador_carga++;
    if (modo_pasada == REPETICION_PASADA_TMEM) {
        return;
    }
    tmem_version++;
    if (sh < sl) {
        return;
    }
    n = sh - sl + 1;
    if (n > 256) {
        n = 256; /* la TMEM alta solo tiene 256 entradas */
    }
    if (!orig_en_ram(s_timg.addr + sl * 2, n * 2)) {
        cargar_malo("LOADTLUT", "fuera de la RAM", w0, w1);
        return;
    }
    {
        u32 clave = 0, generar, b, ok;
        int known = ps2_kart_paleta_clave(s_timg.addr + sl * 2, n * 2, &clave);

        if (!known && estatico_origen_tmem(s_timg.addr + sl * 2, n * 2, &generar)) {
            clave = (u32) (uintptr_t) (s_timg.addr + sl * 2) * 0x9E3779B1u ^ generar;
            known = 1;
        }
        ok = known && act_pal.valido && act_pal.key == clave && act_pal.tmem == t->tmem && act_pal.n == n &&
             t->tmem >= PRIMERA_PALABRA_PALETA && ((t->tmem - PRIMERA_PALABRA_PALETA) & 15) == 0 && (n & 15) == 0;
        for (b = 0; ok && b < n / 16; b++) {
            ok = firmas_paleta_validas[(t->tmem - PRIMERA_PALABRA_PALETA) / 16 + b];
        }
        if (ok) {
            tmem_version--; /* la TMEM queda igual */
            return;
        }
        act_pal.valido = known;
        act_pal.key = clave;
        act_pal.tmem = t->tmem;
        act_pal.n = n;
    }
    antes_de_escritura_directa(t->tmem, n);
    invalidar_firmas_paleta(t->tmem, n);
    for (i = 0; i < n; i++) {
        const u8 *p = s_timg.addr + (sl + i) * 2;

        /* La entrada se repite en los 4 bancos de la palabra */
        tmem_64[(t->tmem + i) & (PALABRAS_TMEM - 1)] = (u64) (p[0] | (p[1] << 8)) * 0x0001000100010001ull;
        SOMBRA64((t->tmem + i) & (PALABRAS_TMEM - 1), (u64) (p[0] | (p[1] << 8)) * 0x0001000100010001ull);
    }
    /* Firmas de los bancos cargados enteros */
    for (i = 0; i < n; i++) {
        u32 palabra = (t->tmem + i) & (PALABRAS_TMEM - 1);

        if (palabra >= PRIMERA_PALABRA_PALETA && ((palabra - PRIMERA_PALABRA_PALETA) & 15) == 0 && i + 16 <= n) {
            u32 banco = (palabra - PRIMERA_PALABRA_PALETA) / 16;
            u32 packed[8];
            u32 k;

            for (k = 0; k < 8; k++) {
                const u8 *p = s_timg.addr + (sl + i + k * 2) * 2;

                packed[k] = ((u32) p[0] << 24) | ((u32) p[1] << 16) | ((u32) p[2] << 8) | p[3];
            }
            firmas_paleta[banco] = palabras_hash(packed, 8, 0x50414C00u | banco);
            firmas_paleta_validas[banco] = 1;
            i += 15;
        }
    }
}

/* Texel del GS en CT32 */
static u32 alpha_gs[256];  /* alfa de 8 bits del N64 -> A del GS, ya desplazado */
static u32 lut_ia8[256], lut_i8[256], lut_ia4[16], lut_i4[16];

static inline u8 alpha8_a_gs(u32 a)
{
    return (u8) ((a * 128 + 127) / 255);
}

static inline u32 rgba(u32 r, u32 g, u32 b, u32 a)
{
    return r | (g << 8) | (b << 16) | ((u32) alpha8_a_gs(a) << 24);
}

static void inicializar_luts(void)
{
    u32 n;

    for (n = 0; n < 256; n++) {
        u32 i = (n >> 4) * 17;

        alpha_gs[n] = (u32) alpha8_a_gs(n) << 24;
        lut_ia8[n] = rgba(i, i, i, (n & 0xF) * 17);
        lut_i8[n] = rgba(n, n, n, n);
    }
    for (n = 0; n < 16; n++) {
        u32 i = (n >> 1) * 255 / 7;
        u32 v = n * 17;

        lut_ia4[n] = rgba(i, i, i, (n & 1) ? 255 : 0);
        lut_i4[n] = rgba(v, v, v, v);
    }
}

static inline u32 desde_rgba16(u32 c)
{
    u32 r = (c >> 11) & 0x1F;
    u32 g = (c >> 6) & 0x1F;
    u32 b = (c >> 1) & 0x1F;

    return ((r << 3) | (r >> 2)) | (((g << 3) | (g >> 2)) << 8) | (((b << 3) | (b >> 2)) << 16) | ((c & 1) << 31);
}

static inline u32 from_ia16(u32 c)
{
    return (c >> 8) * 0x010101u | alpha_gs[c & 0xFF];
}

static inline u32 tmem16(u32 direccion)
{
    return ((u32) s_tmem[direccion & 0xFFF] << 8) | s_tmem[(direccion + 1) & 0xFFF];
}

static void inicializar_clave_nh(void);

void tmem_inicializar(void)
{
    memset(s_tmem, 0, sizeof(s_tmem));
    memset(tiles_rdp, 0, sizeof(tiles_rdp));
    inicializar_luts();
    inicializar_clave_nh();
}
