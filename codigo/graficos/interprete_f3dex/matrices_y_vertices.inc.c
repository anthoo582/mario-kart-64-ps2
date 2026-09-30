// Matrices y vertices

extern s32 estado_juego;

#define OP_SPNOOP     0x00
#define OP_MTX        0x01
#define OP_MOVEMEM    0x03
#define OP_VTX        0x04
#define OP_DL         0x06
#define CARGAR_UCODE_OP  0xAF
#define RAMA_Z_OP   0xB0
#define OP_TRI2       0xB1
#define OP_RDPHALF_C  0xB2
#define OP_RDPHALF_2  0xB3
#define OP_RDPHALF_1  0xB4
#define CUAD_OP       0xB5
#define OP_CLEARGEOM  0xB6
#define OP_SETGEOM    0xB7
#define OP_ENDDL      0xB8
#define OP_OTHERM_L   0xB9
#define OP_OTHERM_H   0xBA
#define TEXTURA_OP    0xBB
#define OP_MOVEWORD   0xBC
#define OP_POPMTX     0xBD
#define OP_CULLDL     0xBE
#define OP_TRI1       0xBF

#define MAX_VERTS   64
#define PILA_MTX   18
#define LUCES_MAX  8

typedef float Mat4[4][4];

/* Vertice ya proyectado, como lo recibe el GS (sintetizador_gs.h). */
typedef struct {
    /* x..w los escribe la VU0 de una vez (sqc2): alineados a 16. */
    float x __attribute__((aligned(16)));
    float y, z, w;      /* espacio de recorte */
    float s, t;         /* coordenadas de textura (s10.5 ya escaladas) */
    float wi;           /* 1 / w (calculado al cargar) */
    float ndc_x, ndc_y;   /* x / w, y / w: descarte de caras sin dividir */
    u8 r, g, b, a;      /* color de vertice o resultado de iluminacion */
    u8 niebla;             /* 0..255, factor de niebla del N64 */
    u8 clip;            /* bits de recorte (banda de guarda del GS) */
    u8 rej;             /* fuera del volumen del RSP (proporcion_recorte veces la vista) */
    u8 agregar_necesitar;         /* (cache) pide la pasada aditiva del combinador */
    s16 k_s, k_t;         /* (cache) periodo de la textura en que cae s / t */
    /* Caches por vertice */
    u32 gen_cc, gen_proy;
    float cr, cg, cb, ca, automovil, cag, cabina;
    VerticeGs proy;
    float proy_s, proy_t; /* s y t normalizadas, sin multiplicar por 1/w */
    /* (cache) proj ya empaquetado para el GS */
    u128 gif[3] __attribute__((aligned(16)));
} __attribute__((aligned(16))) VerticeRsp;

_Static_assert(sizeof(VerticeRsp) % 16 == 0, "RspVertex: tamano multiplo de 16");

typedef struct {
    float x, y, z, w;
    float s, t;
    float r, g, b, a;
    float ar, ag, ab;
    float niebla;
} VerticeRecorte;

#define RECORTE_CERCA  1
#define IZQUIERDA_RECORTE  2
#define DERECHA_RECORTE 4
#define ARRIBA_RECORTE   8
#define BOT_RECORTE   16
#define GUARDIA      3.0f

static struct {
    uintptr_t seg[16];
    Mat4 mv[PILA_MTX];
    int profundidad_mv;
    Mat4 proy;
    Mat4 mvp __attribute__((aligned(16))); /* se carga en la VU0 (lqc2) */
    int valido_mvp;

    VerticeRsp v[MAX_VERTS];

    u32 geom;
    u32 om_h, om_l;
    u32 combinacion0, combinacion1;
    u8 prim[4], amb[4], fogc[4], mezcla[4];
    u8 prim_lod_frac;
    u32 color_relleno;
    u32 profundidad_prim;

    int tex_en, tex_tile;
    float escala_s_tex, escala_t_tex;

    float escala_vp[3], vp_trans[3];
    int luces_num;
    struct {
        u8 col[3];
        s8 dir[3];
    } luces[LUCES_MAX + 1];
    float obj_luz[LUCES_MAX][3];
    int valido_luces;
    s8 mirada[2][3];

    s16 mul_niebla, apagado_niebla;

    int tijera[4]; /* x0,y0,x1,y1 en pixeles del GS */
    uintptr_t cimg, zimg;
    u32 mitad_rdp_1;

    int sucio_estado;
    int proporcion_recorte;
    EstadoGs gs;
    InfoTextura tex;
    int valido_tex;
} St;

/* Estadisticas y diagnostico */
static u32 ops_desconocido[256];
static u32 s_frame;
static int tris_traza; /* >0: registrar los siguientes triangulos (SMK64_GFX_TRACE) */
#ifdef SMK64_DEV
static int diag_frame;    /* este frame se vuelca: registrar tambien la franja inferior */
#endif

#ifdef SMK64_DEV
static void bajo_seg(u32 direccion, uintptr_t a)
{
    static int logged;

    if (logged < 8) {
        logged++;
        registrar("segmento sin asignar: %08x -> %p", (unsigned) direccion, (void *) a);
    }
}
#endif

static inline void *direccion_seg(u32 direccion)
{
    uintptr_t a = St.seg[(direccion >> 24) & 0xF] + (direccion & 0x00FFFFFF);

#ifdef SMK64_DEV
    if (a < 0x00100000 && direccion != 0) {
        bajo_seg(direccion, a);
    }
#endif
    return (void *) a;
}

static void mul_mat(Mat4 salida, Mat4 a, Mat4 b)
{
    Mat4 t;
    int i, j;

    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            t[i][j] = a[i][0] * b[0][j] + a[i][1] * b[1][j] + a[i][2] * b[2][j] + a[i][3] * b[3][j];
        }
    }
    memcpy(salida, t, sizeof(Mat4));
}

/* Mtx de libultra */
static void mtx_desde_n64(Mat4 salida, const u32 *w)
{
    float *o = &salida[0][0];
    int j;

    /* De a pares */
    for (j = 0; j < 8; j++) {
        u32 hi = w[j], lo = w[8 + j];

        o[2 * j] = (float) (s32) ((hi & 0xFFFF0000u) | (lo >> 16)) * (1.0f / 65536.0f);
        o[2 * j + 1] = (float) (s32) ((hi << 16) | (lo & 0xFFFFu)) * (1.0f / 65536.0f);
    }
}

/* max.s/min.s del EE y conversion con signo */
static inline u8 limitar_u8(float v)
{
    return (u8) (s32) __builtin_fminf(__builtin_fmaxf(v, 0.0f), 255.0f);
}

static void actualizar_mvp(void)
{
    if (!St.valido_mvp) {
        mul_mat(St.mvp, St.mv[St.profundidad_mv], St.proy);
        St.valido_mvp = 1;
        St.valido_luces = 0;
    }
}

static void actualizar_luces(void)
{
    int i;
    Mat4 *m = &St.mv[St.profundidad_mv];

    if (St.valido_luces) {
        return;
    }
    for (i = 0; i < St.luces_num; i++) {
        float x = St.luces[i].dir[0], y = St.luces[i].dir[1], z = St.luces[i].dir[2];
        float ox = (*m)[0][0] * x + (*m)[0][1] * y + (*m)[0][2] * z;
        float oy = (*m)[1][0] * x + (*m)[1][1] * y + (*m)[1][2] * z;
        float oz = (*m)[2][0] * x + (*m)[2][1] * y + (*m)[2][2] * z;
        float largo = sqrtf(ox * ox + oy * oy + oz * oz);

        if (largo > 0.0f) {
            largo = 1.0f / largo;
        }
        St.obj_luz[i][0] = ox * largo;
        St.obj_luz[i][1] = oy * largo;
        St.obj_luz[i][2] = oz * largo;
    }
    St.valido_luces = 1;
}

static inline void cargar_mvp_vu0(void)
{
    __asm__ __volatile__("lqc2 $vf4, 0x00(%0)\n\t"
                         "lqc2 $vf5, 0x10(%0)\n\t"
                         "lqc2 $vf6, 0x20(%0)\n\t"
                         "lqc2 $vf7, 0x30(%0)\n\t"
                         :
                         : "r"(St.mvp)
                         : "memory");
}

static inline void transformar_vu0(const Vtx_t *in, float *salida)
{
    u64 tmp;

    __asm__ __volatile__("ld %0, 0(%1)\n\t"
                         "pextlh %0, %0, %0\n\t"
                         "psraw %0, %0, 16\n\t"
                         "qmtc2 %0, $vf8\n\t"
                         "vitof0.xyz $vf8, $vf8\n\t"
                         "vmulax.xyzw $ACC, $vf4, $vf8x\n\t"
                         "vmadday.xyzw $ACC, $vf5, $vf8y\n\t"
                         "vmaddaz.xyzw $ACC, $vf6, $vf8z\n\t"
                         "vmaddw.xyzw $vf9, $vf7, $vf0w\n\t"
                         "sqc2 $vf9, 0(%2)\n\t"
                         : "=&r"(tmp)
                         : "r"(in), "r"(salida)
                         : "memory");
}

static void cargar_vertices(const Vtx *orig_, int n, int dst)
{
    int i;

    float proporcion = (float) St.proporcion_recorte;

    actualizar_mvp();
    cargar_mvp_vu0();
    if (St.geom & G_LIGHTING) {
        actualizar_luces();
    }
    /* Todo se calcula en variables locales y se guarda al final */
    for (i = 0; i < n && dst + i < MAX_VERTS; i++) {
        const Vtx_t *in = &orig_[i].v;
        VerticeRsp *o = &St.v[dst + i];
        float cx, cy, cz, cw, wi, gw;
        float s, t;
        u8 recorte;

        transformar_vu0(in, &o->x);
        cx = o->x;
        cy = o->y;
        cz = o->z;
        cw = o->w;

        s = in->tc[0] * St.escala_s_tex;
        t = in->tc[1] * St.escala_t_tex;

        if (St.geom & G_LIGHTING) {
            const Vtx_tn *vn = &orig_[i].n;
            float nx = vn->n[0], ny = vn->n[1], nz = vn->n[2];
            float cr = St.luces[St.luces_num].col[0];
            float cg = St.luces[St.luces_num].col[1];
            float cb = St.luces[St.luces_num].col[2];
            int l;

            for (l = 0; l < St.luces_num; l++) {
                float d = (nx * St.obj_luz[l][0] + ny * St.obj_luz[l][1] + nz * St.obj_luz[l][2]) * (1.0f / 127.0f);

                if (d > 0.0f) {
                    cr += d * St.luces[l].col[0];
                    cg += d * St.luces[l].col[1];
                    cb += d * St.luces[l].col[2];
                }
            }
            o->r = limitar_u8(cr);
            o->g = limitar_u8(cg);
            o->b = limitar_u8(cb);
            o->a = vn->a;

            if (St.geom & G_TEXTURE_GEN) {
                /* Mapa de reflejo */
                Mat4 *m = &St.mv[St.profundidad_mv];
                float ex = nx * (*m)[0][0] + ny * (*m)[1][0] + nz * (*m)[2][0];
                float ey = nx * (*m)[0][1] + ny * (*m)[1][1] + nz * (*m)[2][1];
                float ez = nx * (*m)[0][2] + ny * (*m)[1][2] + nz * (*m)[2][2];
                float largo = sqrtf(ex * ex + ey * ey + ez * ez);
                float dx, dy;

                if (largo > 0.0f) {
                    largo = 1.0f / largo;
                }
                dx = (ex * St.mirada[0][0] + ey * St.mirada[0][1] + ez * St.mirada[0][2]) * largo / 127.0f;
                dy = (ex * St.mirada[1][0] + ey * St.mirada[1][1] + ez * St.mirada[1][2]) * largo / 127.0f;
                /* texScale ya incluye el /65536 de G_TEXTURE */
                s = (dx + 1.0f) * 0.25f * (St.escala_s_tex * 65536.0f);
                t = (dy + 1.0f) * 0.25f * (St.escala_t_tex * 65536.0f);
            }
        } else {
            o->r = in->cn[0];
            o->g = in->cn[1];
            o->b = in->cn[2];
            o->a = in->cn[3];
        }
        o->s = s;
        o->t = t;

        /* Una sola division por vertice: la niebla usa el mismo 1/w. */
        wi = (cw != 0.0f) ? 1.0f / cw : 1e30f;
        o->wi = wi;
        if (St.geom & G_FOG) {
            float fw = (cw > 0.001f) ? wi : 1000.0f;
            float f = cz * fw * St.mul_niebla + St.apagado_niebla;

            o->niebla = limitar_u8(f);
        } else {
            o->niebla = 0;
        }

        o->ndc_x = cx * wi;
        o->ndc_y = cy * wi;
        o->gen_cc = 0;
        o->gen_proy = 0;

        gw = GUARDIA * cw;
        recorte = 0;
        if (cz < -cw) recorte |= RECORTE_CERCA;
        if (cx < -gw) recorte |= IZQUIERDA_RECORTE;
        if (cx > gw) recorte |= DERECHA_RECORTE;
        if (cy > gw) recorte |= ARRIBA_RECORTE;
        if (cy < -gw) recorte |= BOT_RECORTE;
        o->clip = recorte;
        /* El RSP recorta a proporcion_recorte veces la vista */
        gw = proporcion * cw;
        recorte &= RECORTE_CERCA;
        if (cx < -gw) recorte |= IZQUIERDA_RECORTE;
        if (cx > gw) recorte |= DERECHA_RECORTE;
        if (cy > gw) recorte |= ARRIBA_RECORTE;
        if (cy < -gw) recorte |= BOT_RECORTE;
        o->rej = recorte;
    }
}

static Combinador s_cc;

static void colores_cc(ColoresCombinador *col)
{
    memcpy(col->prim, St.prim, 4);
    memcpy(col->amb, St.amb, 4);
    col->prim_lod_frac = St.prim_lod_frac;
}

/* Analisis del combinador con memo */
static struct {
    int valido, valido_afecta;
    u32 c0, c1, dos_ciclo;
    int alpha_lecturas, lecturas_rgb;
    u32 prim, amb;
    u8 lod;
    int afecta;
} cc_memo;

static void analizar_cc(int dos_ciclo)
{
    if (!cc_memo.valido || cc_memo.c0 != St.combinacion0 || cc_memo.c1 != St.combinacion1 ||
        cc_memo.dos_ciclo != (u32) dos_ciclo) {
        combinador_decodificar(&s_cc, St.combinacion0, St.combinacion1, St.om_h);
        cc_memo.c0 = St.combinacion0;
        cc_memo.c1 = St.combinacion1;
        cc_memo.dos_ciclo = (u32) dos_ciclo;
        cc_memo.alpha_lecturas = combinador_lee_alfa_texel(&s_cc);
        cc_memo.lecturas_rgb = combinador_lee_rgb_texel(&s_cc);
        cc_memo.valido = 1;
        cc_memo.valido_afecta = 0;
    }
}

#define TAMANIO_CCX 64

static struct {
    u32 c0, c1, prim, amb;
    u8 dos_ciclo, lod, valido, afecta;
    u8 valido_agregar, tex_alpha_agregar, agregar;
} ccx[TAMANIO_CCX];

static int agregar_eval_pasada(void);
static int alpha_desde_tex;

static inline int ranura_ccx(u32 prim, u32 amb)
{
    u32 h = St.combinacion0 * 0x9E3779B1u ^ St.combinacion1 * 0x85EBCA6Bu ^ prim * 0xC2B2AE35u ^ amb ^ St.prim_lod_frac;

    return (int) ((h ^ (h >> 16)) & (TAMANIO_CCX - 1));
}

static int entrada_ccx(void)
{
    u32 prim, amb;
    int i;

    memcpy(&prim, St.prim, 4);
    memcpy(&amb, St.amb, 4);
    i = ranura_ccx(prim, amb);
    if (!ccx[i].valido || ccx[i].c0 != St.combinacion0 || ccx[i].c1 != St.combinacion1 || ccx[i].prim != prim ||
        ccx[i].amb != amb || ccx[i].lod != St.prim_lod_frac || ccx[i].dos_ciclo != (u8) cc_memo.dos_ciclo) {
        ColoresCombinador col;

        colores_cc(&col);
        ccx[i].afecta = (u8) combinador_texel_afecta_rgb(&s_cc, &col);
        ccx[i].c0 = St.combinacion0;
        ccx[i].c1 = St.combinacion1;
        ccx[i].prim = prim;
        ccx[i].amb = amb;
        ccx[i].lod = St.prim_lod_frac;
        ccx[i].dos_ciclo = (u8) cc_memo.dos_ciclo;
        ccx[i].valido_agregar = 0;
        ccx[i].valido = 1;
    }
    return i;
}

static int afecta_rgb_texel(void)
{
    return ccx[entrada_ccx()].afecta;
}

static int agregar_posible_pasada(void)
{
    int i = entrada_ccx();

    if (!ccx[i].valido_agregar || ccx[i].tex_alpha_agregar != (u8) alpha_desde_tex) {
        ccx[i].agregar = (u8) agregar_eval_pasada();
        ccx[i].tex_alpha_agregar = (u8) alpha_desde_tex;
        ccx[i].valido_agregar = 1;
    }
    return ccx[i].agregar;
}

/* Suavizado de texturas: bilineal donde el N64 filtra (G_TF_BILERP). 0: vecino mas cercano. */
#ifndef FILTRADO_TEXTURAS
#define FILTRADO_TEXTURAS 1
#endif

#define ZMODE(l)  (((l) >> 10) & 3)

/* Generaciones de las caches por vertice (VerticeRsp) */
static u32 gen_cc = 1, gen_proy = 1;

static int estado_anticipado;    /* pasadas del frame intermedio (ver dibujar_triangulo) */
static float periodo_s_reb, periodo_t_reb, reb_inv_s, reb_inv_t;
/* Coordenadas de textura */
static float mul_st[2], agregar_st[2];

/* Escala de G_SETTILE (shift) */
static const float mul_desplaz_k[16] = {
    1.0f, 1.0f / 2, 1.0f / 4, 1.0f / 8, 1.0f / 16, 1.0f / 32, 1.0f / 64, 1.0f / 128,
    1.0f / 256, 1.0f / 512, 1.0f / 1024, 32.0f, 16.0f, 8.0f, 4.0f, 2.0f,
};

static int prim_texturizado;  /* el combinador usa texel */
static int agregar_dividir;      /* m*TEXEL + a en dos pasadas (solo triangulos) */
static int decal;
static int niebla_en;
static int mezcla_alpha;

static u64 reg_tijera(void)
{
    return GS_SETREG_SCISSOR(St.tijera[0], St.tijera[2] - 1, St.tijera[1], St.tijera[3] - 1);
}

/* Triangulos */
static u64 tri_reg_tijera(void)
{
    float sx = GS_ANCHO / 320.0f, sy = GS_ALTO / 240.0f;
    float hw = (St.escala_vp[0] < 0 ? -St.escala_vp[0] : St.escala_vp[0]) * St.proporcion_recorte;
    float hh = (St.escala_vp[1] < 0 ? -St.escala_vp[1] : St.escala_vp[1]) * St.proporcion_recorte;
    int x0 = (int) ((St.vp_trans[0] - hw) * sx + 0.5f), x1 = (int) ((St.vp_trans[0] + hw) * sx + 0.5f);
    int y0 = (int) ((St.vp_trans[1] - hh) * sy + 0.5f), y1 = (int) ((St.vp_trans[1] + hh) * sy + 0.5f);

    x0 = x0 > St.tijera[0] ? x0 : St.tijera[0];
    y0 = y0 > St.tijera[1] ? y0 : St.tijera[1];
    x1 = x1 < St.tijera[2] ? x1 : St.tijera[2];
    y1 = y1 < St.tijera[3] ? y1 : St.tijera[3];
    if (x1 <= x0 || y1 <= y0) {
        return reg_tijera(); /* viewport raro: sin interseccion, como antes */
    }
    return GS_SETREG_SCISSOR(x0, x1 - 1, y0, y1 - 1);
}

/* Tras reconstruir el estado */
/* memcmp de la libreria compara byte a byte */
static inline int equal_palabras(const void *a, const void *b, int n)
{
    const u32 *x = (const u32 *) a, *y = (const u32 *) b;
    u32 d = 0;
    int i;

    for (i = 0; i < n; i++) {
        d |= x[i] ^ y[i];
    }
    return d == 0;
}

static void actualizar_generaciones_vertice(int es_rect)
{
    static struct {
        u32 c0, c1, om_h;
        u8 prim[4], amb[4];
        u8 lod, texturizado, agregar_dividir, alpha_tex;
    } cc_sig;
    /* Firmas sin relleno implicito (se comparan con memcmp) */
    static struct {
        float origen_s, origen_t, maceta_w, maceta_h;
        float escala_vp[2], vp_trans[2];
        u16 uls, ult, tex_w, tex_h;
        u8 valido_tex, decal, filtro, shifts, shiftt;
        u8 pad[3];
    } sig_proy;
    int cc_cambiado;

    _Static_assert(sizeof(cc_sig) == 24 && sizeof(sig_proy) == 48, "firmas con relleno");

    if (es_rect) {
        cc_sig.texturizado = 0xFF;
        if (++gen_cc == 0) {
            gen_cc = 1;
        }
        if (++gen_proy == 0) {
            gen_proy = 1;
        }
        return;
    }
    {
        typeof(cc_sig) cs;

        cs.c0 = St.combinacion0;
        cs.c1 = St.combinacion1;
        cs.om_h = St.om_h & (3u << G_MDSFT_CYCLETYPE);
        memcpy(cs.prim, St.prim, 4);
        memcpy(cs.amb, St.amb, 4);
        cs.lod = St.prim_lod_frac;
        cs.texturizado = (u8) St.gs.texturizado;
        cs.agregar_dividir = (u8) agregar_dividir;
        cs.alpha_tex = (u8) alpha_desde_tex;
        cc_cambiado = !equal_palabras(&cs, &cc_sig, sizeof(cs) / 4);
        if (cc_cambiado) {
            cc_sig = cs;
            if (++gen_cc == 0) {
                gen_cc = 1;
            }
        }
    }
    {
        typeof(sig_proy) ps;
        const TileRdp *t = &tiles_rdp[St.tex_tile];

        ps.valido_tex = (u8) St.valido_tex;
        ps.decal = (u8) decal;
        ps.filtro = ((St.om_h >> G_MDSFT_TEXTFILT) & 3) != 0;
        ps.pad[0] = ps.pad[1] = ps.pad[2] = 0;
        if (St.valido_tex) {
            ps.shifts = t->shifts;
            ps.shiftt = t->shiftt;
            ps.uls = t->uls;
            ps.ult = t->ult;
            ps.origen_s = St.tex.origen_s;
            ps.origen_t = St.tex.origen_t;
            ps.maceta_w = St.tex.maceta_w;
            ps.maceta_h = St.tex.maceta_h;
            ps.tex_w = (u16) St.tex.width;
            ps.tex_h = (u16) St.tex.height;
        } else {
            ps.shifts = ps.shiftt = 0;
            ps.uls = ps.ult = ps.tex_w = ps.tex_h = 0;
            ps.origen_s = ps.origen_t = ps.maceta_w = ps.maceta_h = 0.0f;
        }
        ps.escala_vp[0] = St.escala_vp[0];
        ps.escala_vp[1] = St.escala_vp[1];
        ps.vp_trans[0] = St.vp_trans[0];
        ps.vp_trans[1] = St.vp_trans[1];
        if (cc_cambiado || !equal_palabras(&ps, &sig_proy, sizeof(ps) / 4)) {
            sig_proy = ps;
            if (++gen_proy == 0) {
                gen_proy = 1;
            }
        }
    }
}

typedef struct VtxColor VtxColor;
static void combinar_color(const u8 sombreado[4], VtxColor *o);

static int agregar_posible_pasada(void);

/* Prepara el estado del GS para dibujar con el tile dado. */
static void construir_impl_estado(int tile, int es_rect);

typedef struct {
    EstadoGs gs;
    InfoTextura tex;
    float mul_st[2], agregar_st[2], reb[4];
    u32 comprobacion;
    u8 valido_tex, decal, alpha_desde_tex, prim_texturizado, agregar_dividir, niebla_en, mezcla_alpha, inc_cc, inc_proy;
} RecEstado;

#define MAX_REC_ESTADO 1536
static RecEstado rec_estado[MAX_REC_ESTADO];
static int rec_n_estado, pos_rec_estado, ok_rec_estado;
static int pasada_para_estado; /* 1 grabando (real con intermedio), 2 reproduciendo */
static u32 miss_rec_estado;  /* registro: reproducciones que no coincidieron */

static u32 comprobar_estado(int tile, int es_rect)
{
    u32 prim, amb;

    memcpy(&prim, St.prim, 4);
    memcpy(&amb, St.amb, 4);
    return St.combinacion0 ^ (St.combinacion1 * 3u) ^ (St.om_l * 5u) ^ (St.om_h * 7u) ^ (St.geom * 11u) ^ (prim * 13u) ^
           (amb * 17u) ^ ((u32) St.tex_tile << 3) ^ ((u32) tile << 8) ^ ((u32) es_rect << 12) ^ ((u32) St.tex_en << 13) ^
           ((u32) St.tijera[2] << 16) ^ ((u32) St.tijera[3] << 22) ^ (u32) St.proporcion_recorte;
}

static void construir_estado(int tile, int es_rect)
{
    EMPEZAR_PROF(ESTADO_PROF);
    if (pasada_para_estado == 2 && ok_rec_estado) {
        if (pos_rec_estado < rec_n_estado && rec_estado[pos_rec_estado].comprobacion == comprobar_estado(tile, es_rect)) {
            const RecEstado *r = &rec_estado[pos_rec_estado++];

            analizar_cc(((St.om_h >> G_MDSFT_CYCLETYPE) & 3) == 1);
            St.gs = r->gs;
            St.tex = r->tex;
            St.valido_tex = r->valido_tex;
            decal = r->decal;
            alpha_desde_tex = r->alpha_desde_tex;
            prim_texturizado = r->prim_texturizado;
            agregar_dividir = r->agregar_dividir;
            niebla_en = r->niebla_en;
            mezcla_alpha = r->mezcla_alpha;
            mul_st[0] = r->mul_st[0];
            mul_st[1] = r->mul_st[1];
            agregar_st[0] = r->agregar_st[0];
            agregar_st[1] = r->agregar_st[1];
            periodo_s_reb = r->reb[0];
            periodo_t_reb = r->reb[1];
            reb_inv_s = r->reb[2];
            reb_inv_t = r->reb[3];
            gs_aplicar_estado(&St.gs);
            St.sucio_estado = es_rect;
            if (r->inc_cc && ++gen_cc == 0) {
                gen_cc = 1;
            }
            if (r->inc_proy && ++gen_proy == 0) {
                gen_proy = 1;
            }
            FIN_PROF(ESTADO_PROF);
            return;
        }
        ok_rec_estado = 0; /* el recorrido se separo: se calcula */
        miss_rec_estado++;
    }
    {
        u32 cc0 = gen_cc, pj0 = gen_proy;

        construir_impl_estado(tile, es_rect);
        if (pasada_para_estado == 1 && ok_rec_estado) {
            if (rec_n_estado < MAX_REC_ESTADO) {
                RecEstado *r = &rec_estado[rec_n_estado++];

                r->comprobacion = comprobar_estado(tile, es_rect);
                r->gs = St.gs;
                r->tex = St.tex;
                r->valido_tex = (u8) St.valido_tex;
                r->decal = (u8) decal;
                r->alpha_desde_tex = (u8) alpha_desde_tex;
                r->prim_texturizado = (u8) prim_texturizado;
                r->agregar_dividir = (u8) agregar_dividir;
                r->niebla_en = (u8) niebla_en;
                r->mezcla_alpha = (u8) mezcla_alpha;
                r->mul_st[0] = mul_st[0];
                r->mul_st[1] = mul_st[1];
                r->agregar_st[0] = agregar_st[0];
                r->agregar_st[1] = agregar_st[1];
                r->reb[0] = periodo_s_reb;
                r->reb[1] = periodo_t_reb;
                r->reb[2] = reb_inv_s;
                r->reb[3] = reb_inv_t;
                r->inc_cc = cc0 != gen_cc;
                r->inc_proy = pj0 != gen_proy;
            } else {
                ok_rec_estado = 0; /* no cabe: el intermedio calculara desde aqui */
            }
        }
    }
    FIN_PROF(ESTADO_PROF);
}

/* La ultima preparacion de textura */
static struct {
    u32 serie, decodificaciones;
    TileRdp tile;
    int indice_tile, tlut, flags, ok;
    InfoTextura tex;
    int valido;
} ultimo_prep;

static int preparar_textura_2(int indice_tile, int tlut, int banderas)
{
    u32 serie = cargar_serie_tmem();
    const TileRdp *t = &tiles_rdp[indice_tile];

    if (ultimo_prep.valido && ultimo_prep.serie == serie && ultimo_prep.decodificaciones == estadisticas_tmem.decodificaciones &&
        ultimo_prep.indice_tile == indice_tile && ultimo_prep.tlut == tlut &&
        ultimo_prep.flags == banderas && memcmp(&ultimo_prep.tile, t, sizeof(*t)) == 0) {
        St.tex = ultimo_prep.tex;
        return ultimo_prep.ok;
    }
    ultimo_prep.ok = preparar_textura(indice_tile, tlut, banderas, &St.tex);
    ultimo_prep.serie = cargar_serie_tmem();
    ultimo_prep.decodificaciones = estadisticas_tmem.decodificaciones;
    ultimo_prep.tile = *t;
    ultimo_prep.indice_tile = indice_tile;
    ultimo_prep.tlut = tlut;
    ultimo_prep.flags = banderas;
    ultimo_prep.tex = St.tex;
    ultimo_prep.valido = 1;
    return ultimo_prep.ok;
}

#ifdef SMK64_PROF
u32 prof_x[8], prof_xn[8], prof_xt;
#define EMPEZAR_PX() (prof_xt = ahora_prof())
#define FIN_PX(i) (prof_x[i] += ahora_prof() - prof_xt, prof_xn[i]++)
#else
#define EMPEZAR_PX() ((void) 0)
#define FIN_PX(i) ((void) 0)
#endif
static void construir_impl_estado(int tile, int es_rect)
{
    EstadoGs *st = &St.gs;
    u32 l = St.om_l;
    int ciclo = (St.om_h >> G_MDSFT_CYCLETYPE) & 3;
    int zcmp = (l & Z_CMP) && (St.geom & G_ZBUFFER) && !es_rect;
    int zupd = (l & Z_UPD) && (St.geom & G_ZBUFFER) && !es_rect;
    int ate = 0, aref = 0;
    u32 mezclar_bits;
    int p, a, m, b;
    int filter;

    EMPEZAR_PX();
    analizar_cc(ciclo == 1);
    FIN_PX(0);
    decal = ZMODE(l) == 3;

    {
        int usa;

        alpha_desde_tex = cc_memo.alpha_lecturas;
        usa = cc_memo.lecturas_rgb || alpha_desde_tex;
        if (ciclo == G_CYC_COPY >> G_MDSFT_CYCLETYPE) {
            usa = 1;
            alpha_desde_tex = 1;
        }
        prim_texturizado = usa && (St.tex_en || es_rect);
    }

    mezclar_bits = l >> 16;
    if (ciclo == 1) {
        p = (mezclar_bits >> 12) & 3;
        a = (mezclar_bits >> 8) & 3;
        m = (mezclar_bits >> 4) & 3;
        b = mezclar_bits & 3;
    } else {
        p = (mezclar_bits >> 14) & 3;
        a = (mezclar_bits >> 10) & 3;
        m = (mezclar_bits >> 6) & 3;
        b = (mezclar_bits >> 2) & 3;
    }
    niebla_en = (ciclo == 1) && (((mezclar_bits >> 14) & 3) == G_BL_CLR_FOG) && (St.geom & G_FOG);
    mezcla_alpha = 0;
    if ((l & FORCE_BL) || m == G_BL_CLR_MEM) {
        if (m == G_BL_CLR_MEM && b == G_BL_1MA) {
            st->alpha = GS_SETREG_ALPHA(0, 1, 0, 1, 0);
            mezcla_alpha = 1;
        } else if (m == G_BL_CLR_MEM && b == G_BL_1) {
            st->alpha = GS_SETREG_ALPHA(0, 2, 0, 1, 0);
            mezcla_alpha = 1;
        }
    }
    if (!mezcla_alpha) {
        st->alpha = GS_SETREG_ALPHA(0, 1, 0, 1, 0);
    }
    (void) p;
    (void) a;

    /* Prueba de alfa */
    if ((l & 3) == G_AC_THRESHOLD) {
        ate = 1;
        aref = (St.mezcla[3] * 128 + 127) / 255;
        if (aref == 0) {
            aref = 1;
        }
    } else if ((l & 3) == G_AC_DITHER) {
        ate = 1;
        aref = 0x40;
    } else if ((l & CVG_X_ALPHA) && (l & ALPHA_CVG_SEL)) {
        ate = 1;
        aref = 0x40;
    } else if (l & CVG_X_ALPHA) {
        /* Cobertura por alfa (el kart con el Boo, humo) */
        ate = 1;
        aref = 0x10;
    } else if (ciclo == (G_CYC_COPY >> G_MDSFT_CYCLETYPE)) {
        ate = 1; /* en modo copia el alfa 0 no se dibuja */
        aref = 1;
    }

    if (!ate && mezcla_alpha) {
        /* Con mezcla por alfa, un pixel de alfa 0 deja el fondo igual */
        ate = 1;
        aref = 1;
    }
    st->prueba = GS_SETREG_TEST(ate, ate ? 5 : 1, aref, 0, 0, 0, 1, zcmp ? 2 : 1);
    st->zbuf = gs_valor_zbuf(zupd);
    st->tijera = es_rect ? reg_tijera() : tri_reg_tijera();
    st->fogcol = (u64) St.fogc[0] | ((u64) St.fogc[1] << 8) | ((u64) St.fogc[2] << 16);

    filter = FILTRADO_TEXTURAS && ((St.om_h >> G_MDSFT_TEXTFILT) & 3) != 0 && ciclo != (G_CYC_COPY >> G_MDSFT_CYCLETYPE);
    St.valido_tex = 0;
    agregar_dividir = 0;
    if (prim_texturizado) {
        int tlut = (St.om_h >> G_MDSFT_TEXTLUT) & 3;

        int banderas_tex = 0;

        /* Si el color no depende del texel (p */
        int aff;

        EMPEZAR_PX();
        aff = afecta_rgb_texel();
        FIN_PX(1);
        if (!aff) {
            if (alpha_desde_tex) {
                banderas_tex |= BLANCO_RGB_TMEM;
            }
        } else {
            agregar_dividir = !es_rect;
        }
        if (preparar_textura_2(es_rect ? tile : St.tex_tile, tlut, banderas_tex | (es_rect ? TMEM_PARA_RECT : 0))) {
            St.valido_tex = 1;
            int app;

            EMPEZAR_PX();
            app = agregar_dividir && estado_anticipado && agregar_posible_pasada();
            FIN_PX(2);
            if (app) {
                InfoTextura blanco;

                preparar_textura(St.tex_tile, tlut, BLANCO_RGB_TMEM, &blanco);
            }
            st->tex0 = St.tex.tex0 | ((u64) (alpha_desde_tex ? 1 : 0) << 34) | ((u64) 0 << 35) ;
            st->tex1 = GS_SETREG_TEX1(1, 0, filter, filter, 0, 0, 0);
            st->clamp = St.tex.clamp;
            {
                const TileRdp *tt = &tiles_rdp[St.tex_tile];
                float mitad = (FILTRADO_TEXTURAS && ((St.om_h >> G_MDSFT_TEXTFILT) & 3) != 0) ? 0.5f : 0.0f;

                mul_st[0] = (1.0f / 32.0f) * mul_desplaz_k[tt->shifts & 15] * St.tex.maceta_w_inv;
                mul_st[1] = (1.0f / 32.0f) * mul_desplaz_k[tt->shiftt & 15] * St.tex.maceta_h_inv;
                agregar_st[0] = (-(float) tt->uls * 0.25f + mitad + St.tex.origen_s) * St.tex.maceta_w_inv;
                agregar_st[1] = (-(float) tt->ult * 0.25f + mitad + St.tex.origen_t) * St.tex.maceta_h_inv;
            }
            periodo_s_reb = (float) St.tex.width * St.tex.maceta_w_inv;
            periodo_t_reb = (float) St.tex.height * St.tex.maceta_h_inv;
            reb_inv_s = St.tex.width ? St.tex.maceta_w / (float) St.tex.width : 0.0f;
            reb_inv_t = St.tex.height ? St.tex.maceta_h / (float) St.tex.height : 0.0f;
        }
    }
    st->texturizado = prim_texturizado && St.valido_tex;
    st->prim = (1 << 3)  | (st->texturizado ? (1 << 4) : 0) | (niebla_en ? (1 << 5) : 0) |
               (mezcla_alpha ? (1 << 6) : 0);
    /* Los triangulos 3D llevan Gouraud, niebla o transparencias; el HUD, los menus y el texto son rectangulos. */
    st->dither = !es_rect && ciclo < (G_CYC_COPY >> G_MDSFT_CYCLETYPE);
    EMPEZAR_PX();
    gs_aplicar_estado(st);
    FIN_PX(3);
    St.sucio_estado = es_rect;
    EMPEZAR_PX();
    actualizar_generaciones_vertice(es_rect);
    FIN_PX(4);
}
