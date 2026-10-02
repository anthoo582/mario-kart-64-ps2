// Decodificar y cache

static void decodificar_renglon(const TileRdp *tile, u32 t, u32 cantidad, u32 *salida, const u32 *lut, int es_ia16, u32 blanco)
{
    u32 base = tile->tmem * 8 + t * tile->line * 8;
    u32 intercambiar = (t & 1) ? 4 : 0;
    u32 s = 0;

    /* base es multiplo de 8 */
    u32 palabra = base >> 3;

    switch (tile->siz) {
        case G_IM_SIZ_4b:
            for (; s + 16 <= cantidad; s += 16, palabra++) {
                u64 v = tmem_64[palabra & (PALABRAS_TMEM - 1)];
                u32 k;

                if (intercambiar) {
                    v = intercambiar_mitades(v);
                }
                for (k = 0; k < 8; k++, v >>= 8) {
                    salida[s + k * 2] = lut[(v >> 4) & 0xF];
                    salida[s + k * 2 + 1] = lut[v & 0xF];
                }
            }
            for (; s + 1 < cantidad; s += 2) {
                u32 n = s_tmem[((base + (s >> 1)) ^ intercambiar) & 0xFFF];

                salida[s] = lut[n >> 4];
                salida[s + 1] = lut[n & 0xF];
            }
            if (s < cantidad) {
                salida[s] = lut[s_tmem[((base + (s >> 1)) ^ intercambiar) & 0xFFF] >> 4];
            }
            break;
        case G_IM_SIZ_8b:
            for (; s + 8 <= cantidad; s += 8, palabra++) {
                u64 v = tmem_64[palabra & (PALABRAS_TMEM - 1)];

                if (intercambiar) {
                    v = intercambiar_mitades(v);
                }
                salida[s] = lut[v & 0xFF];
                salida[s + 1] = lut[(v >> 8) & 0xFF];
                salida[s + 2] = lut[(v >> 16) & 0xFF];
                salida[s + 3] = lut[(v >> 24) & 0xFF];
                salida[s + 4] = lut[(v >> 32) & 0xFF];
                salida[s + 5] = lut[(v >> 40) & 0xFF];
                salida[s + 6] = lut[(v >> 48) & 0xFF];
                salida[s + 7] = lut[v >> 56];
            }
            for (; s < cantidad; s++) {
                salida[s] = lut[s_tmem[((base + s) ^ intercambiar) & 0xFFF]];
            }
            break;
        case G_IM_SIZ_16b:
            /* Cada texel son dos bytes en orden big endian */
            for (; s + 4 <= cantidad; s += 4, palabra++) {
                u64 v = tmem_64[palabra & (PALABRAS_TMEM - 1)];
                u32 k;

                if (intercambiar) {
                    v = intercambiar_mitades(v);
                }
                for (k = 0; k < 4; k++, v >>= 16) {
                    u32 c = ((u32) (v & 0xFF) << 8) | ((u32) (v >> 8) & 0xFF);

                    salida[s + k] = (es_ia16 ? from_ia16(c) : desde_rgba16(c)) | blanco;
                }
            }
            for (; s < cantidad; s++) {
                u32 c = tmem16((base + s * 2) ^ intercambiar);

                salida[s] = (es_ia16 ? from_ia16(c) : desde_rgba16(c)) | blanco;
            }
            break;
        default:
            for (s = 0; s < cantidad; s++) {
                u32 a = ((base + s * 2) ^ intercambiar) & 0x7FF;

                salida[s] = (s_tmem[a] | (s_tmem[a + 1] << 8) | (s_tmem[0x800 + a] << 16) | alpha_gs[s_tmem[0x800 + a + 1]]) |
                         blanco;
            }
            break;
    }
}

#define TAMANIO_CACHE 1024
#define MAX_DECODIFICACION (256 * 256)

typedef struct {
    u32 key[2];
    u16 x, y;      /* hueco en el atlas, en texels */
    u16 ranura_w, ranura_h;
    u32 w, h;
    u64 clamp;
    u8 envoltura_s, envoltura_t;
    u8 valido;
    u32 pasada_usado;  /* ultima pasada que la uso (serie_pasada) */
    const u8 *src; /* RAM de la que se cargo (reutilizacion del hueco) */
    u8 nativo;     /* hueco alineado a pagina con TEX0 propio y REPEAT del GS */
    u8 tw, th;     /* log2 del tamano (TEX0 propio) */
} EntradaCache;

static EntradaCache s_cache[TAMANIO_CACHE];
static u32 decodificacion[MAX_DECODIFICACION] __attribute__((aligned(16)));

/* Memo de las ultimas preparaciones */
#define TAMANIO_MEMO 8

typedef struct {
    u32 version;
    u32 k0, seed;
    EntradaCache *e;
    u32 ek0, ek1;
} MemoPrep;

static MemoPrep memo[TAMANIO_MEMO];
static u32 siguiente_memo;

/* Atlas de texturas */
static u32 ilog2_ceil(u32 v)
{
    u32 n = 0;

    while ((1u << n) < v) {
        n++;
    }
    return n;
}

#define ATLAS_W     1024
#define ATLAS_TBW   (ATLAS_W / 64)
#define ATLAS_TW    10
#define ATLAS_MAX_H 512 /* TH del GS: 9 */
#define CLASSES_ESTANTE 10 /* alturas 1..512 */

static u32 atlas_tbp;   /* bloque de 256 bytes donde empieza */
/* Texturas con paleta nativas del GS */
#define RENGLONES_POOL_CI   64
#define RANURAS_IDX_CI   96  /* PSMT8 de hasta 64x32: 8 bloques cada una */
#define RANURAS_CLUT_CI  64  /* CLUT CT32 de 256 colores: 4 bloques */
#define CI_CLUT_BASE   (RANURAS_IDX_CI * 8)
static u32 ci_tbp;      /* 0: sin reserva */
static u32 renglones_atlas;  /* filas utiles */
static u32 atlas_th;
static u32 anillo_y, anillo_c; /* ultima franja abierta: fila y columna siguiente */

/* El atlas se reparte en columnas: al abrir una estanteria solo se desaloja su franja dentro de
   las columnas que ocupa, no el ancho entero (una textura de 128 filas desalojaba 512 KB). */
#define ATLAS_COL_W 256
#define ATLAS_COLS  (ATLAS_W / ATLAS_COL_W)

typedef struct {
    u16 y, x, x_fin;
    u8 open;
} Estante;

static Estante estante[CLASSES_ESTANTE];
/* Estantes de huecos alineados a pagina (64x32) */
static Estante estante_pagina[CLASSES_ESTANTE];

/* Ultima pasada que uso alguna textura de cada fila de cada columna del atlas. */
static u32 uso_celda[ATLAS_COLS][ATLAS_MAX_H];

static inline void marcar_uso_atlas(u32 x, u32 w, u32 y, u32 h)
{
    u32 c, c_fin = (x + w + ATLAS_COL_W - 1) / ATLAS_COL_W;
    u32 fin = y + h < ATLAS_MAX_H ? y + h : ATLAS_MAX_H;

    for (c = x / ATLAS_COL_W; c < c_fin && c < ATLAS_COLS; c++) {
        u32 *u = uso_celda[c], r;

        for (r = y; r < fin; r++) {
            u[r] = serie_pasada;
        }
    }
}

/* Pasadas desde el ultimo uso de las filas [y, y + h) en las columnas [c0, c0 + cols) */
static u32 edad_franja(u32 c0, u32 cols, u32 y, u32 h)
{
    u32 edad = 0xFFFFFFFFu, c, r;
    u32 fin = y + h < ATLAS_MAX_H ? y + h : ATLAS_MAX_H;

    for (c = c0; c < c0 + cols && c < ATLAS_COLS; c++) {
        const u32 *u = uso_celda[c];

        for (r = y; r < fin; r++) {
            u32 e = serie_pasada - u[r];

            edad = e < edad ? e : edad;
        }
    }
    return edad;
}

void tmem_empezar_frame(void)
{
    if (renglones_atlas == 0) {
        u32 base = gs_vram_inicio_texturas();
        u32 renglones = (gs_vram_fin_texturas() - base) / (ATLAS_TBW * 8192) * 32; /* filas de paginas enteras */

        atlas_tbp = base / 256;
        renglones_atlas = renglones > ATLAS_MAX_H ? ATLAS_MAX_H : renglones;
#ifndef SMK64_SIN_CLUT
        /* Texturas con paleta nativas (ver preparar_ci) */
        if (renglones >= renglones_atlas + RENGLONES_POOL_CI) {
            ci_tbp = atlas_tbp + (renglones_atlas / 32) * ATLAS_TBW * 32;
        } else if (renglones_atlas >= 256 + RENGLONES_POOL_CI) {
            renglones_atlas -= RENGLONES_POOL_CI;
            ci_tbp = atlas_tbp + (renglones_atlas / 32) * ATLAS_TBW * 32;
        }
#endif
        atlas_th = ilog2_ceil(renglones_atlas);
        anillo_y = anillo_c = 0;
        registrar("texturas: atlas %ux%u en %x", (unsigned) ATLAS_W, (unsigned) renglones_atlas, (unsigned) base);
    }
}

static void desalojar_zona_atlas(u32 x0, u32 x1, u32 y0, u32 y1)
{
    int i;

    for (i = 0; i < TAMANIO_CACHE; i++) {
        EntradaCache *e = &s_cache[i];

        if (e->valido && e->y < y1 && y0 < (u32) e->y + e->ranura_h && e->x < x1 && x0 < (u32) e->x + e->ranura_w) {
            e->valido = 0;
            /* Solo importa si este frame ya la uso */
            if (e->pasada_usado == serie_pasada) {
                desalojado_en_pasada = 1;
            }
        }
    }
    for (i = 0; i < CLASSES_ESTANTE; i++) {
        Estante *sh = &estante[i];

        if (sh->open && sh->y < y1 && y0 < (u32) sh->y + (1u << i) && sh->x < x1 && x0 < sh->x_fin) {
            sh->open = 0;
        }
        sh = &estante_pagina[i];
        if (sh->open && sh->y < y1 && y0 < (u32) sh->y + (1u << i) && sh->x < x1 && x0 < sh->x_fin) {
            sh->open = 0;
        }
    }
}

#define CONSERVAR_REUTILIZAR 3
#define EDAD_FRANJA_LIBRE 120 /* pasadas sin uso: de 1 a 4 s segun haya frame intermedio */

static EntradaCache *reutilizar_candidato(const u8 *orig_, u32 w, u32 h, int envolver_s, int envolver_t, int nativo)
{
    int i;

    if (orig_ == NULL) {
        return NULL;
    }
    /* Se guardan hasta CONSERVAR_REUTILIZAR versiones de cada buffer */
    {
        EntradaCache *oldest = NULL;
        int n = 0;

        for (i = 0; i < TAMANIO_CACHE; i++) {
            EntradaCache *e = &s_cache[i];

            if (e->valido && e->src == orig_ && e->ranura_w == w && e->h == h && e->envoltura_s == envolver_s &&
                e->envoltura_t == envolver_t && e->nativo == nativo) {
                n++;
                if (e->pasada_usado != serie_pasada && (oldest == NULL || (s32) (e->pasada_usado - oldest->pasada_usado) < 0)) {
                    oldest = e;
                }
            }
        }
        return (n >= CONSERVAR_REUTILIZAR) ? oldest : NULL;
    }
}

/* Reserva un hueco de w x h (h potencia de 2 si repite en T */
static int reservar_atlas(u32 w, u32 h, u32 alinear_x, int pagina_alineado, u16 *salida_x, u16 *salida_y, u16 *salida_h)
{
    u32 cls = 0, estante_h, x, cols;
    Estante *sh;

    while ((1u << cls) < h) {
        cls++;
    }
    if (pagina_alineado) {
        /* Origen en una pagina */
        if (cls < 5) {
            cls = 5;
        }
        alinear_x = 64;
    }
    estante_h = 1u << cls;
    if (cls >= CLASSES_ESTANTE || estante_h > renglones_atlas || w > ATLAS_W) {
        return 0;
    }
    cols = (w + ATLAS_COL_W - 1) / ATLAS_COL_W;
    sh = pagina_alineado ? &estante_pagina[cls] : &estante[cls];
    if (sh->open) {
        x = (sh->x + alinear_x - 1) & ~(alinear_x - 1);
        if (x + w <= sh->x_fin) {
            goto colocar;
        }
        sh->open = 0;
    }
    {
        /* Franja nueva: la de uso mas viejo, recorriendo las posiciones en anillo. Las que tienen
           texturas en uso se saltan (desalojarlas obliga a decodificarlas y subirlas otra vez). */
        u32 franjas = renglones_atlas / estante_h, pos_c = ATLAS_COLS - cols + 1, total = franjas * pos_c;
        u32 fila = (anillo_y / estante_h) % franjas, i, inicio, mejor = 0, mejor_edad = 0;
        int hay = 0;

        if (anillo_c >= pos_c) {
            anillo_c = 0;
            fila = (fila + 1) % franjas;
        }
        inicio = fila * pos_c + anillo_c;

        for (i = 0; i < total; i++) {
            u32 p = (inicio + i) % total, c0 = p % pos_c, cy = p / pos_c * estante_h, e;

            if ((c0 * ATLAS_COL_W) & (alinear_x - 1)) {
                continue; /* REPEAT: el origen va alineado al ancho */
            }
            e = edad_franja(c0, cols, cy, estante_h);
            if (!hay || e > mejor_edad) {
                hay = 1;
                mejor_edad = e;
                mejor = p;
                if (e >= EDAD_FRANJA_LIBRE) {
                    break;
                }
            }
        }
        if (!hay) {
            return 0;
        }
        x = (mejor % pos_c) * ATLAS_COL_W;
        sh->y = (u16) (mejor / pos_c * estante_h);
        sh->x_fin = (u16) (x + cols * ATLAS_COL_W);
        desalojar_zona_atlas(x, sh->x_fin, sh->y, sh->y + estante_h);
        anillo_y = sh->y;
        anillo_c = mejor % pos_c + cols;
        sh->open = 1;
    }
colocar:
    sh->x = (u16) (x + w);
    *salida_x = (u16) x;
    *salida_y = sh->y;
    *salida_h = (u16) estante_h;
    return 1;
}

/* Hash de la TMEM (clave de la cache) */
static u32 clave_nh[TAMANIO_TMEM / 4 + 4];

static void inicializar_clave_nh(void)
{
    u32 x = 0x2545F491u;
    u32 i;

    for (i = 0; i < sizeof(clave_nh) / sizeof(clave_nh[0]); i++) {
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        clave_nh[i] = x;
    }
}

static inline u32 rotl32(u32 x, u32 r)
{
    return (x << r) | (x >> (32 - r));
}

static u32 palabras_hash(const u32 *p, u32 n, u32 h)
{
    const u32 *k = clave_nh;
    u32 lo0 = h, hi0 = 0, lo1 = 0, hi1 = 0;
    u32 i;

    for (i = 0; i + 4 <= n; i += 4, p += 4, k += 4) {
        u32 a0 = p[0] + k[0], b0 = p[1] + k[1];
        u32 a1 = p[2] + k[2], b1 = p[3] + k[3];
        u32 l0, h0, l1, h1;

        __asm__("multu %4, %5\n\t"
                "multu1 %6, %7\n\t"
                "mflo %0\n\t"
                "mfhi %1\n\t"
                "mflo1 %2\n\t"
                "mfhi1 %3"
                : "=&r"(l0), "=&r"(h0), "=&r"(l1), "=&r"(h1)
                : "r"(a0), "r"(b0), "r"(a1), "r"(b1)
                : "hi", "lo");
        lo0 += l0;
        hi0 += h0;
        lo1 += l1;
        hi1 += h1;
    }
    for (; i + 2 <= n; i += 2, p += 2, k += 2) {
        u32 a0 = p[0] + k[0], b0 = p[1] + k[1];
        u32 l0, h0;

        __asm__("multu %2, %3\n\t"
                "mflo %0\n\t"
                "mfhi %1"
                : "=&r"(l0), "=&r"(h0)
                : "r"(a0), "r"(b0)
                : "hi", "lo");
        lo0 += l0;
        hi0 += h0;
    }
    if (i < n) {
        lo0 += p[0] * 0x9E3779B1u;
    }
    h = lo0 ^ rotl32(hi0, 7) ^ rotl32(lo1, 13) ^ rotl32(hi1, 21) ^ (n << 2);
    h ^= h >> 16;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    h *= 0xC2B2AE35u;
    return h ^ (h >> 16);
}

/* start y len en bytes, multiplos de 8 (siempre lo son */
static u32 hash_tmem(u32 empezar, u32 largo, u32 h);

/* Parte de la clave que depende de la paleta */
static u32 clave_paleta(const TileRdp *tile, u32 k1)
{
    u32 primer = (tile->siz == G_IM_SIZ_4b) ? tile->palette : 0;
    u32 bancos = (tile->siz == G_IM_SIZ_4b) ? 1 : BANCOS_PALETA;
    u32 b;

    for (b = primer; b < primer + bancos; b++) {
        if (!firmas_paleta_validas[b]) {
#ifdef SMK64_PROF
            estadisticas_tmem.bytes_hash_paleta += bancos * 16 * 8;
#endif
            return hash_tmem((PRIMERA_PALABRA_PALETA + primer * 16) * 8, bancos * 16 * 8, k1);
        }
    }
#ifdef SMK64_PROF
    estadisticas_tmem.firmas_paleta_usadas++;
#endif
#ifdef SMK64_TMEM_CHECK
    /* Comprobacion */
    aplicar_cargas_pendientes();
    for (b = primer; b < primer + bancos; b++) {
        u32 packed[8], k;
        static u32 errores;

        for (k = 0; k < 8; k++) {
            packed[k] = (tmem16((PRIMERA_PALABRA_PALETA + b * 16 + k * 2) * 8) << 16) |
                        tmem16((PRIMERA_PALABRA_PALETA + b * 16 + k * 2 + 1) * 8);
        }
        if (palabras_hash(packed, 8, 0x50414C00u | b) != firmas_paleta[b] && errores++ < 20) {
            registrar("TMEM_CHECK: firma de paleta %u no coincide", (unsigned) b);
        }
    }
#endif
    return palabras_hash(&firmas_paleta[primer], bancos, k1 ^ 0x7A1E77E5u);
}

static u32 hash_tmem(u32 empezar, u32 largo, u32 h)
{
    const u32 *w = (const u32 *) s_tmem;

    aplicar_cargas_que_tocan(empezar / 8, (largo + 7) / 8);
#ifdef SMK64_TMEM_CHECK
    comparar_con_sombra(empezar, largo, "hash");
#endif
#ifdef SMK64_PROF
    estadisticas_tmem.hash_bytes += largo > TAMANIO_TMEM ? TAMANIO_TMEM : largo;
#endif
    empezar &= 0xFFF;
    if (largo > TAMANIO_TMEM) {
        largo = TAMANIO_TMEM;
    }
    if (empezar + largo <= TAMANIO_TMEM) {
        return palabras_hash(w + empezar / 4, largo / 4, h);
    }
    h = palabras_hash(w + empezar / 4, (TAMANIO_TMEM - empezar) / 4, h);
    return palabras_hash(w, (largo - (TAMANIO_TMEM - empezar)) / 4, h);
}

#if defined(SMK64_MEDIDOR) || defined(SMK64_DEV)
void tmem_uso_cache(u32 *entradas, u32 *bytes, u32 *capacidad)
{
    u32 n = 0, total = 0;
    int i;

    for (i = 0; i < TAMANIO_CACHE; i++) {
        if (s_cache[i].valido) {
            n++;
            total += (u32) s_cache[i].ranura_w * s_cache[i].ranura_h * 4;
        }
    }
    *entradas = n;
    *bytes = total;
    *capacidad = ATLAS_W * renglones_atlas * 4;
}
#endif

/* Tamano de un eje: region del tile, con mascara y espejo. */
static void eje(u32 lo, u32 hi, u32 mascara, u32 cm, u32 *size, u32 *decodificar, int *espejo, int *clamp)
{
    u32 tamanio_tile = ((hi >> 2) - (lo >> 2)) + 1;

    if (tamanio_tile > 1024) {
        tamanio_tile = 1024;
    }
    *espejo = 0;
    *clamp = 0;
    if (mascara != 0 && !(cm & G_TX_CLAMP)) {
        /* Repeticion: el GS repite la potencia de 2 de la mascara. */
        *decodificar = 1u << mascara;
        if (cm & G_TX_MIRROR) {
            *decodificar <<= 1;
            *espejo = 1;
        }
    } else {
        *decodificar = tamanio_tile;
        *clamp = 1;
    }
    *size = tamanio_tile;
}

static void rellenar_salida(EntradaCache *e, InfoTextura *salida)
{
    e->pasada_usado = serie_pasada;
    marcar_uso_atlas(e->x, e->ranura_w, e->y, e->ranura_h);
    salida->clamp = e->clamp;
    salida->width = e->w;
    salida->height = e->h;
    if (e->nativo) {
        /* Textura propia que empieza en la pagina de su hueco */
        u32 pagina = (e->y / 32u) * ATLAS_TBW + e->x / 64u;

        salida->tex0 = GS_SETREG_TEX0(atlas_tbp + pagina * 32, ATLAS_TBW, GS_PSM_CT32, e->tw, e->th, 1, 0, 0, 0, 0, 0, 0);
        salida->maceta_w = (float) (1u << e->tw);
        salida->maceta_h = (float) (1u << e->th);
        salida->maceta_w_inv = 1.0f / salida->maceta_w;
        salida->maceta_h_inv = 1.0f / salida->maceta_h;
        salida->origen_s = 0.0f;
        salida->origen_t = 0.0f;
    } else {
        salida->tex0 = GS_SETREG_TEX0(atlas_tbp, ATLAS_TBW, GS_PSM_CT32, ATLAS_TW, atlas_th, 1, 0, 0, 0, 0, 0, 0);
        salida->maceta_w = (float) ATLAS_W;
        salida->maceta_h = (float) (1u << atlas_th);
        salida->maceta_w_inv = 1.0f / salida->maceta_w;
        salida->maceta_h_inv = 1.0f / salida->maceta_h;
        salida->origen_s = (float) e->x;
        salida->origen_t = (float) e->y;
    }
    salida->envoltura_s = e->envoltura_s;
    salida->envoltura_t = e->envoltura_t;
}

static void decodificar_textura(const TileRdp *tile, int tlut_modo, int banderas, u32 decrementar_w, u32 decrementar_h, u32 arriba_w, int mir_s,
                           int mir_t)
{
    u32 blanco = (banderas & BLANCO_RGB_TMEM) ? 0x00FFFFFFu : 0;
    u32 medio_w = mir_s ? decrementar_w / 2 : decrementar_w;
    u32 medio_h = mir_t ? decrementar_h / 2 : decrementar_h;
    u32 lut_buf[256];
    const u32 *lut = NULL;
    int es_ia16 = 0;
    u32 x, y, n = 0;

    aplicar_cargas_pendientes();
#ifdef SMK64_TMEM_CHECK
    comparar_con_sombra(0, TAMANIO_TMEM, "decodificacion");
#endif
    /* Tabla por formato */
    if (tile->siz == G_IM_SIZ_4b || tile->siz == G_IM_SIZ_8b) {
        if (tile->fmt == G_IM_FMT_CI || tlut_modo != 0) {
            u32 primer = (tile->siz == G_IM_SIZ_4b) ? 256 + tile->palette * 16 : 256;

            n = (tile->siz == G_IM_SIZ_4b) ? 16 : 256;
            for (x = 0; x < n; x++) {
                u32 e = tmem16((primer + x) * 8);

                lut_buf[x] = ((tlut_modo == 3) ? from_ia16(e) : desde_rgba16(e)) | blanco;
            }
            lut = lut_buf;
        } else {
            const u32 *orig_;

            if (tile->siz == G_IM_SIZ_4b) {
                orig_ = (tile->fmt == G_IM_FMT_IA) ? lut_ia4 : lut_i4;
                n = 16;
            } else {
                orig_ = (tile->fmt == G_IM_FMT_IA) ? lut_ia8 : lut_i8;
                n = 256;
            }
            if (blanco) {
                for (x = 0; x < n; x++) {
                    lut_buf[x] = orig_[x] | blanco;
                }
                lut = lut_buf;
            } else {
                lut = orig_;
            }
        }
    } else if (tile->siz == G_IM_SIZ_16b) {
        es_ia16 = tile->fmt == G_IM_FMT_IA || tile->fmt == G_IM_FMT_I;
    }

    for (y = 0; y < decrementar_h; y++) {
        u32 *renglon = &decodificacion[y * arriba_w];

        if (mir_t && y >= medio_h) {
            /* Filas espejadas */
            memcpy(renglon, &decodificacion[(decrementar_h - 1 - y) * arriba_w], arriba_w * 4);
            continue;
        }
        decodificar_renglon(tile, y, medio_w, renglon, lut, es_ia16, blanco);
        for (x = medio_w; x < decrementar_w; x++) {
            renglon[x] = renglon[decrementar_w - 1 - x];
        }
        for (; x < arriba_w; x++) {
            renglon[x] = renglon[decrementar_w - 1];
        }
    }
}

static int preparar_impl_tmem(int indice_tile, int tlut_modo, int banderas, InfoTextura *salida);

#define MAX_REC_PREP  1024
#define HASH_PREP     2048

typedef struct {
    u32 contador;
    TileRdp tile;
    u8 tlut_modo, flags, ok;
    InfoTextura out;
} RecPrep;

static RecPrep rec_prep[MAX_REC_PREP];
static s16 hash_prep[HASH_PREP];
static int rec_n_prep;

static u32 preparar_clave(u32 contador, const TileRdp *tile, int tlut_modo, int banderas)
{
    const u8 *b = (const u8 *) tile;
    u32 h = contador * 0x9E3779B1u ^ ((u32) tlut_modo << 8) ^ ((u32) banderas << 12);
    u32 i;

    for (i = 0; i < sizeof(TileRdp); i++) {
        h = (h ^ b[i]) * 0x01000193u;
    }
    return h;
}

static RecPrep *buscar_prep(u32 h, u32 contador, const TileRdp *tile, int tlut_modo, int banderas, int insertar)
{
    u32 i;

    for (i = 0; i < HASH_PREP; i++) {
        s16 *ranura = &hash_prep[(h + i) & (HASH_PREP - 1)];

        if (*ranura < 0) {
            if (!insertar || rec_n_prep >= MAX_REC_PREP) {
                return NULL;
            }
            *ranura = (s16) rec_n_prep;
            return &rec_prep[rec_n_prep++];
        }
        {
            RecPrep *r = &rec_prep[*ranura];

            if (r->contador == contador && r->tlut_modo == tlut_modo && r->flags == banderas &&
                memcmp(&r->tile, tile, sizeof(TileRdp)) == 0) {
                return r;
            }
        }
    }
    return NULL;
}

void pasada_empezar_tmem(int mode)
{
    serie_pasada++;
    modo_pasada = mode;
    pasada_fallido = 0;
    desalojado_en_pasada = 0;
    contador_carga = 0;
    if (mode == REGISTRO_PASADA_TMEM) {
        memset(hash_prep, 0xFF, sizeof(hash_prep));
        rec_n_prep = 0;
    }
}

u32 cargar_serie_tmem(void)
{
    return contador_carga ^ (serie_pasada << 20);
}

int tmem_pasada_fallido(void)
{
    return pasada_fallido || (modo_pasada == REGISTRO_PASADA_TMEM && desalojado_en_pasada);
}

int preparar_textura(int indice_tile, int tlut_modo, int banderas, InfoTextura *salida)
{
    const TileRdp *tile = &tiles_rdp[indice_tile];
    int r;
    EMPEZAR_PROF(PREPARAR_PROF);

    if (modo_pasada == REPETICION_PASADA_TMEM) {
        RecPrep *rec = buscar_prep(preparar_clave(contador_carga, tile, tlut_modo, banderas), contador_carga, tile, tlut_modo, banderas, 0);

        if (rec == NULL) {
            /* Este punto no se preparo en el frame real (p */
            pasada_fallido = 1;
            FIN_PROF(PREPARAR_PROF);
            return 0;
        }
        *salida = rec->out;
        FIN_PROF(PREPARAR_PROF);
        return rec->ok;
    }
    r = preparar_impl_tmem(indice_tile, tlut_modo, banderas, salida);
    if (modo_pasada == REGISTRO_PASADA_TMEM) {
        RecPrep *rec = buscar_prep(preparar_clave(contador_carga, tile, tlut_modo, banderas), contador_carga, tile, tlut_modo, banderas, 1);

        if (rec == NULL) {
            pasada_fallido = 1;
        } else {
            rec->contador = contador_carga;
            rec->tile = *tile;
            rec->tlut_modo = (u8) tlut_modo;
            rec->flags = (u8) banderas;
            rec->ok = (u8) r;
            rec->out = *salida;
        }
    }
    FIN_PROF(PREPARAR_PROF);
    return r;
}

/* Parte de la clave que depende del contenido de la TMEM */
static u32 clave_contenido(const TileRdp *tile, int tlut_modo, u32 h, u32 decrementar_w, u32 decrementar_h, int mir_s, int mir_t,
                       int *de_pendiente, int con_paleta)
{
    u32 bytes_linea = tile->line * 8;
    u32 k1;

    *de_pendiente = 0;
    if (tile->siz == G_IM_SIZ_32b) {
        k1 = hash_tmem(tile->tmem * 8, bytes_linea * ((mir_t ? decrementar_h / 2 : decrementar_h) + 1), h);
        k1 = hash_tmem(0x800 + tile->tmem * 8, bytes_linea * decrementar_h, k1);
    } else {
        /* Exactamente lo que lee decodificar_renglon */
        u32 renglones = mir_t ? decrementar_h / 2 : decrementar_h;
        u32 texels = mir_s ? decrementar_w / 2 : decrementar_w;
        u32 bytes_renglon = (tile->siz == G_IM_SIZ_4b) ? (texels + 1) / 2 : texels << (tile->siz - 1);
        u32 largo = renglones ? (renglones - 1) * bytes_linea + ((bytes_renglon + 7) & ~7u) : 0;
        RegistroCarga *r = (largo != 0) ? carga_que_contiene(tile->tmem, largo / 8) : NULL;

        if (r != NULL) {
            u32 v[4];

            if (!r->firma_calculada) {
                /* Primer uso */
                r->sig = firma_carga(r->src, r->words, r->contador_inicial, r->dxt);
                r->firma_calculada = 1;
            }
            v[0] = r->sig;
            v[1] = tile->tmem - r->start;
            v[2] = largo;
            v[3] = 0x4C4F4144u;
            k1 = palabras_hash(v, 4, h ^ 0x1B873593u);
            *de_pendiente = r->pendiente;
#ifdef SMK64_PROF
            estadisticas_tmem.firmas_carga_usadas++;
#endif
#ifdef SMK64_TMEM_CHECK
            {
                /* Comprobacion: lo que hay en la TMEM es lo que dejo la carga. */
                static u32 errores;
                u32 w, acc = r->contador_inicial;

                aplicar_cargas_pendientes();
                *de_pendiente = 0;
                for (w = 0; w < r->words; w++, acc += r->dxt) {
                    u64 v = (acc & 0x800) ? intercambiar_mitades(r->src[w]) : r->src[w];

                    if (tmem_64[r->start + w] != v) {
                        if (errores++ < 20) {
                            registrar("TMEM_CHECK: carga en %u+%u no coincide en la palabra %u", (unsigned) r->start,
                                    (unsigned) r->words, (unsigned) w);
                        }
                        break;
                    }
                }
                if (firma_carga(r->src, r->words, r->contador_inicial, r->dxt) != r->sig && errores++ < 20) {
                    registrar("TMEM_CHECK: firma de la carga en %u desactualizada", (unsigned) r->start);
                }
            }
#endif
        } else {
            k1 = hash_tmem(tile->tmem * 8, largo, h);
        }
    }
    if (con_paleta && (tile->fmt == G_IM_FMT_CI || tlut_modo != 0)) {
        k1 = clave_paleta(tile, k1);
    }
    return k1;
}

typedef struct {
    u32 key, clave2;
    u32 pasada_usado, ultimo_uso;
    u8 valido;
} RanuraCi;

static RanuraCi ci_idx[RANURAS_IDX_CI], ci_clut[RANURAS_CLUT_CI];
static u32 reloj_ci;
static u8 ci_buf[64 * 32] __attribute__((aligned(16)));
static u32 ci_clut_buf[256] __attribute__((aligned(16)));
u32 subidas_ci, golpes_ci;

static int ranura_ci(RanuraCi *t, int n, u32 clave, u32 clave2, int *es_nuevo)
{
    int i, mejor = -1;

    for (i = 0; i < n; i++) {
        if (t[i].valido && t[i].key == clave && t[i].clave2 == clave2) {
            *es_nuevo = 0;
            return i;
        }
    }
    for (i = 0; i < n; i++) {
        if (!t[i].valido) {
            mejor = i;
            break;
        }
        if (t[i].pasada_usado != serie_pasada && (mejor < 0 || (s32) (t[i].ultimo_uso - t[mejor].ultimo_uso) < 0)) {
            mejor = i;
        }
    }
    if (mejor >= 0) {
        t[mejor].valido = 0;
        *es_nuevo = 1;
    }
    return mejor;
}

static int preparar_ci(const TileRdp *tile, int tlut_modo, int banderas, u32 decrementar_w, u32 decrementar_h, int limitar_s, int limitar_t,
                      InfoTextura *salida)
{
    u32 blanco = (banderas & BLANCO_RGB_TMEM) ? 0x00FFFFFFu : 0;
    u32 tw = ilog2_ceil(decrementar_w), th = ilog2_ceil(decrementar_h);
    u32 clave_idx, clave_idx_2, clave_pal, clave_pal_2;
    int envolver_s = !limitar_s, envolver_t = !limitar_t;
    int de_pendiente, nuevo_i, nuevo_c, ii, ci;

    if ((envolver_s && (1u << tw) != decrementar_w) || (envolver_t && (1u << th) != decrementar_h)) {
        return 0; /* REPEAT nativo: potencias de 2 */
    }
    clave_idx_2 = decrementar_w | (decrementar_h << 8) | ((u32) tile->line << 16);
    clave_idx = clave_contenido(tile, 0, clave_idx_2 ^ 0x43490000u, decrementar_w, decrementar_h, 0, 0, &de_pendiente, 0);
    clave_pal_2 = (u32) tlut_modo | (blanco ? 0x100u : 0);
    clave_pal = clave_paleta(tile, 0x434C5554u ^ clave_pal_2);

    ii = ranura_ci(ci_idx, RANURAS_IDX_CI, clave_idx, clave_idx_2, &nuevo_i);
    if (ii >= 0 && nuevo_i && de_pendiente) {
        /* Como en la ruta normal */
        aplicar_cargas_pendientes();
        clave_idx = clave_contenido(tile, 0, clave_idx_2 ^ 0x43490000u, decrementar_w, decrementar_h, 0, 0, &de_pendiente, 0);
        ii = ranura_ci(ci_idx, RANURAS_IDX_CI, clave_idx, clave_idx_2, &nuevo_i);
    }
    if (ii < 0) {
        return 0;
    }
    ci = ranura_ci(ci_clut, RANURAS_CLUT_CI, clave_pal, clave_pal_2, &nuevo_c);
    if (ci < 0) {
        return 0;
    }
    if (nuevo_i) {
        u32 y;

        aplicar_cargas_pendientes();
        for (y = 0; y < decrementar_h; y++) {
            u32 palabra = (tile->tmem * 8 + y * tile->line * 8) >> 3;
            u64 *dst = (u64 *) &ci_buf[y * decrementar_w];
            u32 s;

            for (s = 0; s < decrementar_w; s += 8, palabra++) {
                u64 v = tmem_64[palabra & (PALABRAS_TMEM - 1)];

                dst[s / 8] = (y & 1) ? intercambiar_mitades(v) : v;
            }
        }
        subir_imagen_gs(ci_tbp + ii * 8, 2, GS_PSM_T8, 0, 0, ci_buf, decrementar_w, decrementar_h, decrementar_w * decrementar_h);
        ci_idx[ii].key = clave_idx;
        ci_idx[ii].clave2 = clave_idx_2;
        ci_idx[ii].valido = 1;
        subidas_ci++;
        estadisticas_tmem.decodificaciones++; /* otra textura ocupa un hueco (interprete_f3dex.c: preparar_textura_2) */
    }
    if (nuevo_c) {
        u32 x;

        aplicar_cargas_que_tocan(256, 256);
        for (x = 0; x < 256; x++) {
            u32 e = tmem16((256 + x) * 8);
            u32 pos = (x & ~0x18u) | ((x & 0x08u) << 1) | ((x & 0x10u) >> 1);

            ci_clut_buf[pos] = ((tlut_modo == 3) ? from_ia16(e) : desde_rgba16(e)) | blanco;
        }
        subir_imagen_gs(ci_tbp + CI_CLUT_BASE + ci * 4, 1, GS_PSM_CT32, 0, 0, ci_clut_buf, 16, 16, 1024);
        ci_clut[ci].key = clave_pal;
        ci_clut[ci].clave2 = clave_pal_2;
        ci_clut[ci].valido = 1;
        subidas_ci++;
        estadisticas_tmem.decodificaciones++;
    }
    if (!nuevo_i && !nuevo_c) {
        golpes_ci++;
    }
    reloj_ci++;
    ci_idx[ii].pasada_usado = ci_clut[ci].pasada_usado = serie_pasada;
    ci_idx[ii].ultimo_uso = ci_clut[ci].ultimo_uso = reloj_ci;

    salida->tex0 = GS_SETREG_TEX0(ci_tbp + ii * 8, 2, GS_PSM_T8, tw, th, 1, 0, ci_tbp + CI_CLUT_BASE + ci * 4,
                               GS_PSM_CT32, 0, 0, 1);
    salida->clamp = GS_SETREG_CLAMP(envolver_s ? 0 : 2, envolver_t ? 0 : 2, 0, envolver_s ? 0 : decrementar_w - 1, 0, envolver_t ? 0 : decrementar_h - 1);
    salida->width = decrementar_w;
    salida->height = decrementar_h;
    salida->maceta_w = (float) (1u << tw);
    salida->maceta_h = (float) (1u << th);
    salida->maceta_w_inv = 1.0f / salida->maceta_w;
    salida->maceta_h_inv = 1.0f / salida->maceta_h;
    salida->origen_s = 0.0f;
    salida->origen_t = 0.0f;
    salida->envoltura_s = (u8) envolver_s;
    salida->envoltura_t = (u8) envolver_t;
    return 1;
}

static int buscar_en_cache(u32 k0, u32 k1, EntradaCache **encontrado)
{
    u32 ranura = (k0 * 31 + k1) & (TAMANIO_CACHE - 1);
    u32 x;

    for (x = 0; x < 8; x++) {
        EntradaCache *e = &s_cache[(ranura + x) & (TAMANIO_CACHE - 1)];

        if (e->valido && e->key[0] == k0 && e->key[1] == k1) {
            estadisticas_tmem.golpes++;
            *encontrado = e;
            return 1;
        }
    }
    return 0;
}
