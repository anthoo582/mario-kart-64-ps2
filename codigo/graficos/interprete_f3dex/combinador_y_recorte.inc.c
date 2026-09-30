// Combinador y recorte

struct VtxColor {
    float r, g, b, a;
    float ar, ag, ab;
};

static void combinar_color(const u8 sombreado[4], VtxColor *o)
{
    ColoresCombinador col;
    Simbolo cc[4];

    colores_cc(&col);
    combinador_combinar(&s_cc, &col, sombreado, cc);
    o->ar = o->ag = o->ab = 0.0f;
    if (St.gs.texturizado) {
        if (agregar_dividir && cc[0].m >= -0.002f && cc[1].m >= -0.002f && cc[2].m >= -0.002f) {
            o->r = cc[0].m * 128.0f;
            o->g = cc[1].m * 128.0f;
            o->b = cc[2].m * 128.0f;
            o->ar = cc[0].a * 128.0f;
            o->ag = cc[1].a * 128.0f;
            o->ab = cc[2].a * 128.0f;
        } else {
            o->r = (cc[0].m + cc[0].a) * 128.0f;
            o->g = (cc[1].m + cc[1].a) * 128.0f;
            o->b = (cc[2].m + cc[2].a) * 128.0f;
        }
        if (alpha_desde_tex && cc[3].m > 0.001f) {
            o->a = (cc[3].m + cc[3].a) * 128.0f;
        } else {
            o->a = cc[3].a * 128.0f; /* alfa constante: se usa con TCC abajo */
        }
    } else {
        o->r = (cc[0].m + cc[0].a) * 255.0f;
        o->g = (cc[1].m + cc[1].a) * 255.0f;
        o->b = (cc[2].m + cc[2].a) * 255.0f;
        o->a = (cc[3].m + cc[3].a) * 128.0f;
    }
}

static int agregar_eval_pasada(void)
{
    static const u8 probes[2][4] = { { 0, 0, 0, 0 }, { 255, 255, 255, 255 } };
    int guardado_texturizado = St.gs.texturizado;
    int i, posible = 0;

    /* Se llama desde construir_estado antes de fijar St.gs.textured */
    St.gs.texturizado = 1;
    for (i = 0; i < 2 && !posible; i++) {
        VtxColor c;

        combinar_color(probes[i], &c);
        posible = c.ar >= 0.5f || c.ag >= 0.5f || c.ab >= 0.5f;
    }
    St.gs.texturizado = guardado_texturizado;
    return posible;
}

/* Memo por color de sombreado dentro de una generacion */
#define COLOR_MEMO 32

static struct {
    u32 gen, key;
    VtxColor c;
} color_memo[COLOR_MEMO];

static void color_vertice(VerticeRsp *v)
{
    u32 clave;
    int ranura;

    if (v->gen_cc == gen_cc) {
        return;
    }
    clave = v->r | ((u32) v->g << 8) | ((u32) v->b << 16) | ((u32) v->a << 24);
    ranura = (clave ^ (clave >> 9) ^ (clave >> 18) ^ (clave >> 27)) & (COLOR_MEMO - 1);
    if (color_memo[ranura].gen != gen_cc || color_memo[ranura].key != clave) {
        const u8 sombreado[4] = { v->r, v->g, v->b, v->a };

        EMPEZAR_PROF(COMBINACION_PROF);
        combinar_color(sombreado, &color_memo[ranura].c);
        FIN_PROF(COMBINACION_PROF);
        color_memo[ranura].gen = gen_cc;
        color_memo[ranura].key = clave;
    }
    v->cr = color_memo[ranura].c.r;
    v->cg = color_memo[ranura].c.g;
    v->cb = color_memo[ranura].c.b;
    v->ca = color_memo[ranura].c.a;
    v->automovil = color_memo[ranura].c.ar;
    v->cag = color_memo[ranura].c.ag;
    v->cabina = color_memo[ranura].c.ab;
    v->gen_cc = gen_cc;
}

static void a_vertice_recorte(VerticeRsp *v, VerticeRecorte *o)
{
    color_vertice(v);
    o->x = v->x;
    o->y = v->y;
    o->z = v->z;
    o->w = v->w;
    o->s = v->s;
    o->t = v->t;
    o->r = v->cr;
    o->g = v->cg;
    o->b = v->cb;
    o->a = v->ca;
    o->ar = v->automovil;
    o->ag = v->cag;
    o->ab = v->cabina;
    o->niebla = v->niebla;
}

static void interpolar_recorte(VerticeRecorte *o, const VerticeRecorte *a, const VerticeRecorte *b, float t)
{
    o->x = a->x + (b->x - a->x) * t;
    o->y = a->y + (b->y - a->y) * t;
    o->z = a->z + (b->z - a->z) * t;
    o->w = a->w + (b->w - a->w) * t;
    o->s = a->s + (b->s - a->s) * t;
    o->t = a->t + (b->t - a->t) * t;
    o->r = a->r + (b->r - a->r) * t;
    o->g = a->g + (b->g - a->g) * t;
    o->b = a->b + (b->b - a->b) * t;
    o->a = a->a + (b->a - a->a) * t;
    o->ar = a->ar + (b->ar - a->ar) * t;
    o->ag = a->ag + (b->ag - a->ag) * t;
    o->ab = a->ab + (b->ab - a->ab) * t;
    o->niebla = a->niebla + (b->niebla - a->niebla) * t;
}

static float dist_plano(const VerticeRecorte *v, int plano)
{
    switch (plano) {
        case RECORTE_CERCA: return v->z + v->w;
        case IZQUIERDA_RECORTE: return v->x + GUARDIA * v->w;
        case DERECHA_RECORTE: return GUARDIA * v->w - v->x;
        case ARRIBA_RECORTE: return GUARDIA * v->w - v->y;
        default: return v->y + GUARDIA * v->w;
    }
}

static int poligono_recorte(VerticeRecorte *in, int n, VerticeRecorte *tmp, int plano)
{
    int i, salida = 0;

    for (i = 0; i < n; i++) {
        VerticeRecorte *a = &in[i];
        VerticeRecorte *b = &in[(i + 1) % n];
        float da = dist_plano(a, plano);
        float db = dist_plano(b, plano);

        if (da >= 0.0f) {
            tmp[salida++] = *a;
        }
        if ((da >= 0.0f) != (db >= 0.0f)) {
            interpolar_recorte(&tmp[salida++], a, b, da / (da - db));
        }
    }
    memcpy(in, tmp, sizeof(VerticeRecorte) * salida);
    return salida;
}

/* floorf para |x| < 2^31 (texels) */
static inline float rapido_floor(float x)
{
    float i;

    if (__builtin_fabsf(x) >= 8388608.0f) {
        return x; /* ya es entero */
    }
    i = (float) (s32) x;
    return i > x ? i - 1.0f : i;
}

/* Proyeccion de un vertice ya combinado */
static inline void proyectar_wi(const VerticeRecorte *c, VerticeGs *o, float *sn, float *tn, float wi)
{
    float xn = c->x * wi, yn = c->y * wi, zn = c->z * wi;
    float sx = xn * St.escala_vp[0] + St.vp_trans[0];
    float sy = -yn * St.escala_vp[1] + St.vp_trans[1];
    float zf = __builtin_fminf(__builtin_fmaxf((1.0f - zn) * 0.5f, 0.0f), 1.0f);

    o->x = sx * (GS_ANCHO / 320.0f);
    o->y = sy * (GS_ALTO / 240.0f);
    o->z = (u32) (s32) (zf * 16777000.0f) + (decal ? 1024 : 0); /* zf en [0, 1]: cabe en s32 */
    if (St.valido_tex) {
        float s = c->s * mul_st[0] + agregar_st[0];
        float tt = c->t * mul_st[1] + agregar_st[1];

        *sn = s;
        *tn = tt;
        o->q = wi;
        o->s = s * wi;
        o->t = tt * wi;
    } else {
        *sn = *tn = 0.0f;
        o->s = o->t = 0.0f;
        o->q = 1.0f;
    }
    o->r = limitar_u8(c->r);
    o->g = limitar_u8(c->g);
    o->b = limitar_u8(c->b);
    o->a = limitar_u8(c->a > 128.0f ? 128.0f : c->a);
    o->niebla = 255 - limitar_u8(c->niebla);
}

static void proyectar(const VerticeRecorte *c, VerticeGs *o, float *sn, float *tn)
{
    proyectar_wi(c, o, sn, tn, (c->w > 1e-5f) ? 1.0f / c->w : 1e5f);
}

/* Proyeccion con cache */
static void proyeccion_vertice(VerticeRsp *v)
{
    if (v->gen_proy != gen_proy || v->gen_cc != gen_cc) {
        VerticeRecorte c;

        a_vertice_recorte(v, &c);
        proyectar_wi(&c, &v->proy, &v->proy_s, &v->proy_t, (c.w > 1e-5f) ? v->wi : 1e5f);
        vertice_paquete_gs(&v->proy, v->gif);
        v->agregar_necesitar = c.ar >= 0.5f || c.ag >= 0.5f || c.ab >= 0.5f;
        /* Periodo de la textura en que cae el vertice */
        v->k_s = v->k_t = 0;
        if (St.gs.texturizado) {
            if (St.tex.envoltura_s) {
                v->k_s = (s16) rapido_floor((v->proy_s - St.tex.origen_s * St.tex.maceta_w_inv) * reb_inv_s);
            }
            if (St.tex.envoltura_t) {
                v->k_t = (s16) rapido_floor((v->proy_t - St.tex.origen_t * St.tex.maceta_h_inv) * reb_inv_t);
            }
        }
        v->gen_proy = gen_proy;
    }
}

/* Segunda pasada de m*TEXEL + a */
static void agregar_pasada_dibujo(const float agregar_color[][3], VerticeGs *salida, int n)
{
    EstadoGs guardado = St.gs;
    EstadoGs agregar = St.gs;
    InfoTextura blanco;
    int i;

    if (!preparar_textura(St.tex_tile, (St.om_h >> G_MDSFT_TEXTLUT) & 3, BLANCO_RGB_TMEM, &blanco)) {
        return;
    }
    agregar.tex0 = blanco.tex0 | ((u64) (alpha_desde_tex ? 1 : 0) << 34);
    agregar.clamp = blanco.clamp;
    /* La version blanca ocupa otro hueco del atlas */
    {
        float ds = (blanco.origen_s - St.tex.origen_s) * St.tex.maceta_w_inv;
        float dt = (blanco.origen_t - St.tex.origen_t) * St.tex.maceta_h_inv;

        for (i = 0; i < n; i++) {
            salida[i].s += ds * salida[i].q;
            salida[i].t += dt * salida[i].q;
        }
    }
    agregar.alpha = GS_SETREG_ALPHA(0, 2, 0, 1, 0);
    agregar.zbuf = gs_valor_zbuf(0);
    agregar.prim = (agregar.prim | (1 << 6)) & ~(1u << 5); /* ABE si, FGE no */
    gs_aplicar_estado(&agregar);
    for (i = 0; i < n; i++) {
        salida[i].r = limitar_u8(agregar_color[i][0]);
        salida[i].g = limitar_u8(agregar_color[i][1]);
        salida[i].b = limitar_u8(agregar_color[i][2]);
        if (!mezcla_alpha) {
            salida[i].a = 0x80; /* opaco: la cobertura la pone el alfa del texel */
        }
    }
    for (i = 1; i + 1 < n; i++) {
        gs_triangulo(&salida[0], &salida[i], &salida[i + 1]);
    }
    St.gs = guardado;
    St.sucio_estado = 1;
}

static void rebase_uv(VerticeGs *salida, const float *sn, const float *tn, int n)
{
    int i;

    if (!St.gs.texturizado || !(St.tex.envoltura_s | St.tex.envoltura_t)) {
        return;
    }
    if (St.tex.envoltura_s) {
        float min_s = sn[0], periodo = (float) St.tex.width * St.tex.maceta_w_inv;
        float k;

        for (i = 1; i < n; i++) {
            min_s = sn[i] < min_s ? sn[i] : min_s;
        }
        k = rapido_floor((min_s - St.tex.origen_s * St.tex.maceta_w_inv) / periodo);
        if (k != 0.0f) {
            k *= periodo;
            for (i = 0; i < n; i++) {
                salida[i].s -= k * salida[i].q;
            }
        }
    }
    if (St.tex.envoltura_t) {
        float min_t = tn[0], periodo = (float) St.tex.height * St.tex.maceta_h_inv;
        float k;

        for (i = 1; i < n; i++) {
            min_t = tn[i] < min_t ? tn[i] : min_t;
        }
        k = rapido_floor((min_t - St.tex.origen_t * St.tex.maceta_h_inv) / periodo);
        if (k != 0.0f) {
            k *= periodo;
            for (i = 0; i < n; i++) {
                salida[i].t -= k * salida[i].q;
            }
        }
    }
}

static void dibujar_triangulo(int i0, int i1, int i2)
{
    VerticeRsp *v0 = &St.v[i0], *v1 = &St.v[i1], *v2 = &St.v[i2];
    VerticeGs salida[16];
    float sn[16], tn[16];
    float agregar[16][3];
    int n, i;
    u8 o_all = v0->clip | v1->clip | v2->clip;

#ifdef SMK64_DEV
    {
        static int kart_pre;

        if (((St.om_h >> G_MDSFT_TEXTLUT) & 3) == 2 && estado_juego == 4 && kart_pre < 16 && tiles_rdp[0].fmt == G_IM_FMT_CI) {
            kart_pre++;
            registrar("pre-tri CI clip %x/%x/%x w %d %d %d x %d y %d z %d geom %x", v0->clip, v1->clip, v2->clip,
                    (int) v0->w, (int) v1->w, (int) v2->w, (int) v0->x, (int) v0->y, (int) v0->z, (unsigned) St.geom);
        }
    }
#endif

    if (St.sucio_estado && estado_anticipado) {
        construir_estado(St.tex_tile, 0);
    }
    if ((v0->clip & v1->clip & v2->clip) | (v0->rej & v1->rej & v2->rej)) {
        return; /* fuera por completo del mismo lado */
    }
#ifdef SMK64_DEV_NOTRIS
    return;
#endif

    if ((St.geom & G_CULL_BOTH) && !(o_all & RECORTE_CERCA)) {
        /* Mismo convenio que el RSP: (v0 - v1) x (v2 - v1). */
        float cruce = (v0->ndc_x - v1->ndc_x) * (v2->ndc_y - v1->ndc_y) - (v0->ndc_y - v1->ndc_y) * (v2->ndc_x - v1->ndc_x);

        if ((St.geom & G_CULL_BOTH) == G_CULL_BOTH) {
            return;
        }
        if ((St.geom & G_CULL_FRONT) && cruce <= 0.0f) {
            return;
        }
        if ((St.geom & G_CULL_BACK) && cruce >= 0.0f) {
            return;
        }
    }

    if (St.sucio_estado) {
        construir_estado(St.tex_tile, 0);
    }

    if (!o_all) {
        VerticeRsp *vv[3] = { v0, v1, v2 };
        int k_s = 0, k_t = 0;

        proyeccion_vertice(v0);
        proyeccion_vertice(v1);
        proyeccion_vertice(v2);
        if (St.gs.texturizado) {
            k_s = v0->k_s < v1->k_s ? v0->k_s : v1->k_s;
            k_s = k_s < v2->k_s ? k_s : v2->k_s;
            k_t = v0->k_t < v1->k_t ? v0->k_t : v1->k_t;
            k_t = k_t < v2->k_t ? k_t : v2->k_t;
            if ((k_s | k_t | v0->agregar_necesitar | v1->agregar_necesitar | v2->agregar_necesitar) == 0) {
                EMPEZAR_PROF(EMITIR_PROF);
                gs_triangulo_empaquetado(v0->gif, v1->gif, v2->gif);
                FIN_PROF(EMITIR_PROF);
                return;
            }
        } else {
            EMPEZAR_PROF(EMITIR_PROF);
            gs_triangulo_empaquetado(v0->gif, v1->gif, v2->gif);
            FIN_PROF(EMITIR_PROF);
            return;
        }
        for (i = 0; i < 3; i++) {
            salida[i] = vv[i]->proy;
            agregar[i][0] = vv[i]->automovil;
            agregar[i][1] = vv[i]->cag;
            agregar[i][2] = vv[i]->cabina;
            /* rebase_uv con el periodo ya calculado por vertice. */
            if (k_s != 0) {
                salida[i].s -= (float) k_s * periodo_s_reb * salida[i].q;
            }
            if (k_t != 0) {
                salida[i].t -= (float) k_t * periodo_t_reb * salida[i].q;
            }
        }
        n = 3;
    } else {
        VerticeRecorte poli[16], tmp[16];
        int plano;

        EMPEZAR_PROF(RECORTE_PROF);
        a_vertice_recorte(v0, &poli[0]);
        a_vertice_recorte(v1, &poli[1]);
        a_vertice_recorte(v2, &poli[2]);
        n = 3;
        for (plano = RECORTE_CERCA; plano <= BOT_RECORTE && n >= 3; plano <<= 1) {
            if (o_all & plano) {
                n = poligono_recorte(poli, n, tmp, plano);
            }
        }
        if (n < 3) {
            FIN_PROF(RECORTE_PROF);
            return;
        }
        for (i = 0; i < n; i++) {
            proyectar(&poli[i], &salida[i], &sn[i], &tn[i]);
            agregar[i][0] = poli[i].ar;
            agregar[i][1] = poli[i].ag;
            agregar[i][2] = poli[i].ab;
        }
        FIN_PROF(RECORTE_PROF);
        rebase_uv(salida, sn, tn, n);
    }
#ifdef SMK64_DEV
    {
        static int registro_kart;

        if (((St.om_h >> G_MDSFT_TEXTLUT) & 3) == 2 && tiles_rdp[St.tex_tile].fmt == G_IM_FMT_CI &&
            tiles_rdp[St.tex_tile].siz == G_IM_SIZ_8b && estado_juego == 4 && registro_kart < 12) {
            registro_kart++;
            registrar("kart tri (%d,%d)(%d,%d)(%d,%d) z%x w%d a%02x rgb%02x%02x%02x tex%d test%x zbuf%x omL%x cc%06x/%08x",
                    (int) salida[0].x, (int) salida[0].y, (int) salida[1].x, (int) salida[1].y, (int) salida[2].x, (int) salida[2].y,
                    (unsigned) salida[0].z, (int) v0->w, salida[0].a, salida[0].r, salida[0].g, salida[0].b, St.gs.texturizado,
                    (unsigned) St.gs.prueba, (unsigned) (St.gs.zbuf >> 32), (unsigned) St.om_l, (unsigned) St.combinacion0,
                    (unsigned) St.combinacion1);
        }
    }
#endif
#ifdef SMK64_GFX_TRACE
    {
        float minx = salida[0].x, maxx = salida[0].x, miny = salida[0].y, maxy = salida[0].y;

        for (i = 1; i < n; i++) {
            if (salida[i].x < minx) minx = salida[i].x;
            if (salida[i].x > maxx) maxx = salida[i].x;
            if (salida[i].y < miny) miny = salida[i].y;
            if (salida[i].y > maxy) maxy = salida[i].y;
        }
        if ((maxx - minx) * (maxy - miny) > 40000.0f && (s_frame % 120) == 100) {
            registrar("tri GRANDE n%d (%d,%d)-(%d,%d) c%02x%02x%02x a%02x tex%d w %d %d %d clip%x", n, (int) minx,
                    (int) miny, (int) maxx, (int) maxy, salida[0].r, salida[0].g, salida[0].b, salida[0].a, St.gs.texturizado,
                    (int) v0->w, (int) v1->w, (int) v2->w, o_all);
        }
    }
    if (tris_traza > 0) {
        tris_traza--;
        registrar("tri (%d,%d z%x)(%d,%d)(%d,%d) w%d c%02x%02x%02x a%02x tex%d geom%x omL%x cc%06x/%08x", (int) salida[0].x,
                (int) salida[0].y, (unsigned) salida[0].z, (int) salida[1].x, (int) salida[1].y, (int) salida[2].x, (int) salida[2].y,
                (int) v0->w, salida[0].r, salida[0].g, salida[0].b, salida[0].a, St.gs.texturizado, (unsigned) St.geom,
                (unsigned) St.om_l, (unsigned) St.combinacion0, (unsigned) St.combinacion1);
    }
#endif
    EMPEZAR_PROF(EMITIR_PROF);
    for (i = 1; i + 1 < n; i++) {
        gs_triangulo(&salida[0], &salida[i], &salida[i + 1]);
    }
    FIN_PROF(EMITIR_PROF);
    if (St.gs.texturizado) {
        for (i = 0; i < n; i++) {
            if (agregar[i][0] >= 0.5f || agregar[i][1] >= 0.5f || agregar[i][2] >= 0.5f) {
                agregar_pasada_dibujo(agregar, salida, n);
                break;
            }
        }
    }
}

static void dibujar_texrect(u32 w0, u32 w1, u32 mitad2, u32 halfc, int voltear)
{
    int ciclo = (St.om_h >> G_MDSFT_CYCLETYPE) & 3;
    float lrx = ((w0 >> 12) & 0xFFF) * 0.25f;
    float lry = (w0 & 0xFFF) * 0.25f;
    int tile = (w1 >> 24) & 7;
    float ulx = ((w1 >> 12) & 0xFFF) * 0.25f;
    float uly = (w1 & 0xFFF) * 0.25f;
    float s0 = (s16) (mitad2 >> 16) / 32.0f;
    float t0 = (s16) (mitad2 & 0xFFFF) / 32.0f;
    float dsdx = (s16) (halfc >> 16) / 1024.0f;
    float dtdy = (s16) (halfc & 0xFFFF) / 1024.0f;
    VerticeRsp rv;
    VerticeRecorte cv;
    VerticeGs a, b;
    const TileRdp *t = &tiles_rdp[tile];
    float s1, t1;

    if (ciclo == (G_CYC_COPY >> G_MDSFT_CYCLETYPE)) {
        dsdx /= 4.0f;
        lrx += 1.0f;
        lry += 1.0f;
    }
    if (lrx <= ulx || lry <= uly) {
        return;
    }
#ifdef SMK64_GFX_TRACE
    if (tris_traza > 0) {
        registrar("texrect cyc%d tile%d (%d,%d)-(%d,%d) st %d,%d d %d,%d omL%x cc%06x/%08x", ciclo, tile, (int) ulx,
                (int) uly, (int) lrx, (int) lry, (int) s0, (int) t0, (int) (dsdx * 1000), (int) (dtdy * 1000),
                (unsigned) St.om_l, (unsigned) St.combinacion0, (unsigned) St.combinacion1);
    }
#endif
    construir_estado(tile, 1);
    if (!St.valido_tex && prim_texturizado) {
        return;
    }

    memset(&rv, 0, sizeof(rv));
    /* Los rectangulos del RDP no tienen color de sombreado */
    rv.r = rv.g = rv.b = rv.a = 0;
    a_vertice_recorte(&rv, &cv);

    s1 = s0 + (voltear ? dtdy : dsdx) * (voltear ? (lry - uly) : (lrx - ulx));
    t1 = t0 + (voltear ? dsdx : dtdy) * (voltear ? (lrx - ulx) : (lry - uly));
    (void) voltear;

    memset(&a, 0, sizeof(a));
    a.x = ulx * (GS_ANCHO / 320.0f);
    a.y = uly * (GS_ALTO / 240.0f);
    a.z = 0;
    a.q = 1.0f;
    a.r = limitar_u8(cv.r);
    a.g = limitar_u8(cv.g);
    a.b = limitar_u8(cv.b);
    a.a = limitar_u8(cv.a > 128.0f ? 128.0f : cv.a);
    a.niebla = 255;
    b = a;
    b.x = lrx * (GS_ANCHO / 320.0f);
    b.y = lry * (GS_ALTO / 240.0f);
    if (St.valido_tex) {
        float mitad = 0.0f;

        if (FILTRADO_TEXTURAS && ((St.om_h >> G_MDSFT_TEXTFILT) & 3) != 0 && ciclo != (G_CYC_COPY >> G_MDSFT_CYCLETYPE)) {
            u64 clamp = St.gs.clamp;
            float ds = voltear ? dtdy : dsdx;
            float dt = voltear ? dsdx : dtdy;
            float w = voltear ? (lry - uly) : (lrx - ulx);
            float h = voltear ? (lrx - ulx) : (lry - uly);
            float sa = s0 - t->uls * 0.25f, sb = sa + ds * (w - 1.0f);
            float ta = t0 - t->ult * 0.25f, tb = ta + dt * (h - 1.0f);
            int lento = (int) floorf(sa < sb ? sa : sb), shi = (int) floorf(sa < sb ? sb : sa);
            int tlo = (int) floorf(ta < tb ? ta : tb), thi = (int) floorf(ta < tb ? tb : ta);

            mitad = 0.5f;
            if (!St.tex.envoltura_s || (lento >= 0 && shi < (int) St.tex.width)) {
                int maxu = (int) St.tex.width - 1;
                int lo = lento < 0 ? 0 : (lento > maxu ? maxu : lento);
                int hi = shi < lo ? lo : (shi > maxu ? maxu : shi);
                u32 apagado = (u32) St.tex.origen_s;

                clamp = (clamp & ~((u64) 3 | ((u64) 0xFFFFF << 4))) | 2 | ((u64) (apagado + lo) << 4) |
                        ((u64) (apagado + hi) << 14);
            }
            if (!St.tex.envoltura_t || (tlo >= 0 && thi < (int) St.tex.height)) {
                int maxv = (int) St.tex.height - 1;
                int lo = tlo < 0 ? 0 : (tlo > maxv ? maxv : tlo);
                int hi = thi < lo ? lo : (thi > maxv ? maxv : thi);
                u32 apagado = (u32) St.tex.origen_t;

                clamp = (clamp & ~(((u64) 3 << 2) | ((u64) 0xFFFFF << 24))) | ((u64) 2 << 2) | ((u64) (apagado + lo) << 24) |
                        ((u64) (apagado + hi) << 34);
            }
            if (clamp != St.gs.clamp) {
                St.gs.clamp = clamp;
                gs_aplicar_estado(&St.gs);
            }
        }
        a.s = (s0 - t->uls * 0.25f + mitad + St.tex.origen_s) * St.tex.maceta_w_inv;
        a.t = (t0 - t->ult * 0.25f + mitad + St.tex.origen_t) * St.tex.maceta_h_inv;
        b.s = (s1 - t->uls * 0.25f + mitad + St.tex.origen_s) * St.tex.maceta_w_inv;
        b.t = (t1 - t->ult * 0.25f + mitad + St.tex.origen_t) * St.tex.maceta_h_inv;
    }
    gs_sprite(&a, &b);
    St.sucio_estado = 1;
}

static void dibujar_fillrect(u32 w0, u32 w1)
{
    int ciclo = (St.om_h >> G_MDSFT_CYCLETYPE) & 3;
    float lrx = ((w0 >> 12) & 0xFFF) * 0.25f;
    float lry = (w0 & 0xFFF) * 0.25f;
    float ulx = ((w1 >> 12) & 0xFFF) * 0.25f;
    float uly = (w1 & 0xFFF) * 0.25f;

#ifdef SMK64_GFX_TRACE
    if (tris_traza > 0) {
        registrar("fill cyc%d (%d,%d)-(%d,%d) fill%08x prim%02x%02x%02x%02x omL%x cc%06x/%08x cimg%x zimg%x", ciclo,
                (int) ulx, (int) uly, (int) lrx, (int) lry, (unsigned) St.color_relleno, St.prim[0], St.prim[1], St.prim[2],
                St.prim[3], (unsigned) St.om_l, (unsigned) St.combinacion0, (unsigned) St.combinacion1, (unsigned) St.cimg,
                (unsigned) St.zimg);
    }
#endif
    if (ciclo >= 2) { /* copia/relleno: la esquina inferior es inclusiva */
        lrx += 1.0f;
        lry += 1.0f;
    }
    if (St.cimg == St.zimg && St.zimg != 0) {
#ifndef SMK64_DEV_NOZCLEAR
        gs_limpiar_z();
#endif
        St.sucio_estado = 1;
        return;
    }
    if (ciclo == (G_CYC_FILL >> G_MDSFT_CYCLETYPE)) {
        u32 c = St.color_relleno >> 16;
        u8 r = ((c >> 11) & 0x1F) << 3, g = ((c >> 6) & 0x1F) << 3, b = ((c >> 1) & 0x1F) << 3;

        gs_rellenar_rectangulo((int) (ulx * GS_ANCHO / 320.0f), (int) (uly * GS_ALTO / 240.0f),
                     (int) (lrx * GS_ANCHO / 320.0f), (int) (lry * GS_ALTO / 240.0f), r, g, b);
    } else {
        /* 1/2 ciclos: color del combinador (fundidos, cajas del HUD). */
        VerticeRsp rv;
        VerticeRecorte cv;
        VerticeGs a, bb;
        int tex_guardado = St.tex_en;

        St.tex_en = 0;
        construir_estado(0, 1);
        St.tex_en = tex_guardado;
        memset(&rv, 0, sizeof(rv));
        /* Los rectangulos del RDP no tienen color de sombreado */
    rv.r = rv.g = rv.b = rv.a = 0;
        a_vertice_recorte(&rv, &cv);
        memset(&a, 0, sizeof(a));
        a.x = ulx * (GS_ANCHO / 320.0f);
        a.y = uly * (GS_ALTO / 240.0f);
        a.q = 1.0f;
        a.r = limitar_u8(cv.r);
        a.g = limitar_u8(cv.g);
        a.b = limitar_u8(cv.b);
        a.a = limitar_u8(cv.a > 128.0f ? 128.0f : cv.a);
        a.niebla = 255;
        bb = a;
        bb.x = lrx * (GS_ANCHO / 320.0f);
        bb.y = lry * (GS_ALTO / 240.0f);
        gs_sprite(&a, &bb);
    }
    St.sucio_estado = 1;
}

enum { REAL_PASADA, INTERP_PASADA };

#define MAX_DL_PROFUNDIDAD_LA 18

#define MTX_REC_MAX 2048
#define MTX_HASH    4096
#define MTX_CNT     2048
#define MAX_DESCARTE    8192
#define MV_TIPO_MTX   0
#define PROY_TIPO_MTX 1

typedef struct {
    u32 addr;
    u16 occ;
    u8 kind;
    Mat4 m;
} MtxRec;

typedef struct {
    u32 addr;
    u16 count;
    u8 kind, gen;
} CantidadMtx;

typedef struct {
    CantidadMtx e[MTX_CNT];
    u8 gen;
} TablaConteos;

typedef struct {
    MtxRec rec[MTX_REC_MAX];
    u32 hash[MTX_HASH]; /* (generacion << 16) | indice en rec */
    u16 generacion_hash;
    TablaConteos cnt;
    int n;
    int valido;
    u32 tarea;
} TablaMtx;

static TablaMtx mtx_tab[2];
static int act_mtx;                 /* tabla del frame real que se arma */
static TablaConteos mtx_cnt_interp;
static int pasada = REAL_PASADA;
static int s_recording;              /* el frame real graba (hay intermedio previsto) */
static int interp_abort;
static u8 bits_descarte[MAX_DESCARTE / 8];
static int cantidad_descarte, pos_descarte, desborde_descarte;

static int interp_user = 1;         /* R3 lo activa/desactiva */
static u32 serie_tarea;
static int ultimo_usado_interp;         /* la tarea anterior mostro un intermedio */
static u32 ultimo_vblank_tarea, ultimo_periodo;
static u32 suma_periodos;
static int racha_lento, retroceso;
static u32 real_costo, interp_costo;  /* ciclos, media movil */
static u32 ultimo_real;               /* ciclos del ultimo frame real */
static u32 edad_interp;              /* tareas desde la ultima medida del intermedio */
u32 interp_frames, interp_salteado;  /* estadisticas (registro, medidor) */
/* Por que no hubo intermedio */
static u32 por_que_saltear_interp[5];
#ifdef SMK64_PROF
/* Motivos de descarte */
static u32 por_que_interp[8];
#endif

extern struct GfxPool gfx_pools[2];
extern u16 juego_en_pausa;

void alternar_interp_gfx_ps2(void)
{
    interp_user = !interp_user;
    registrar("60 FPS (frame intermedio): %s", interp_user ? "activado" : "desactivado");
}

int ps2_gfx_interp_activado(void)
{
    return interp_user;
}

static u32 direccion_norma(uintptr_t a)
{
    uintptr_t p0 = (uintptr_t) &gfx_pools[0];
    uintptr_t size = sizeof(struct GfxPool);

    if (a >= p0 && a < p0 + 2 * size) {
        return 0x40000000u | (u32) ((a - p0) % size);
    }
    return (u32) a;
}

static inline u32 mtx_hash(u32 direccion, u32 occ, u32 tipo)
{
    u32 h = direccion * 0x9E3779B1u ^ (occ * 0x85EBCA6Bu) ^ tipo;

    return h ^ (h >> 15);
}

static void reiniciar_conteos(TablaConteos *tab)
{
    if (++tab->gen == 0) {
        memset(tab->e, 0, sizeof(tab->e));
        tab->gen = 1;
    }
}

static CantidadMtx *ranura_cnt(TablaConteos *tab, u32 direccion, int tipo, int insertar)
{
    u32 h = mtx_hash(direccion, 0, tipo);
    u32 i;

    for (i = 0; i < MTX_CNT; i++) {
        CantidadMtx *c = &tab->e[(h + i) & (MTX_CNT - 1)];

        if (c->gen != tab->gen) {
            if (!insertar) {
                return NULL;
            }
            c->gen = tab->gen;
            c->addr = direccion;
            c->kind = (u8) tipo;
            c->count = 0;
            return c;
        }
        if (c->addr == direccion && c->kind == tipo) {
            return c;
        }
    }
    return NULL;
}

static const MtxRec *buscar_rec(const TablaMtx *t, u32 direccion, u32 occ, int tipo)
{
    u32 h = mtx_hash(direccion, occ, tipo);
    u32 i;

    for (i = 0; i < MTX_HASH; i++) {
        u32 ranura = t->hash[(h + i) & (MTX_HASH - 1)];
        u32 idx = ranura & 0xFFFF;

        if ((ranura >> 16) != t->generacion_hash) {
            return NULL;
        }
        if (t->rec[idx].addr == direccion && t->rec[idx].occ == occ && t->rec[idx].kind == tipo) {
            return &t->rec[idx];
        }
    }
    return NULL;
}

static void agregar_rec(TablaMtx *t, u32 direccion, u32 occ, int tipo, Mat4 m)
{
    u32 h = mtx_hash(direccion, occ, tipo);
    u32 i;

    if (t->n >= MTX_REC_MAX) {
        t->valido = 0; /* demasiadas: este frame no sirve de referencia */
        return;
    }
    for (i = 0; i < MTX_HASH; i++) {
        u32 *ranura = &t->hash[(h + i) & (MTX_HASH - 1)];

        if ((*ranura >> 16) != t->generacion_hash) {
            MtxRec *r = &t->rec[t->n];

            r->addr = direccion;
            r->occ = (u16) occ;
            r->kind = (u8) tipo;
            memcpy(r->m, m, sizeof(Mat4));
            *ranura = ((u32) t->generacion_hash << 16) | (u32) t->n++;
            return;
        }
    }
    t->valido = 0;
}

static void reiniciar_tabla(TablaMtx *t)
{
    if (++t->generacion_hash == 0) {
        memset(t->hash, 0, sizeof(t->hash));
        t->generacion_hash = 1;
    }
    reiniciar_conteos(&t->cnt);
    t->n = 0;
    t->valido = 1;
}

static u32 vtx_anticipacion(const Gfx *dl, Gfx **pila, int sp)
{
    const Gfx *lstack[MAX_DL_PROFUNDIDAD_LA];
    int lsp = sp < MAX_DL_PROFUNDIDAD_LA ? sp : MAX_DL_PROFUNDIDAD_LA;
    u32 mitad1 = St.mitad_rdp_1;
    int pasos;

    memcpy(lstack, pila + (sp - lsp), lsp * sizeof(Gfx *));
    for (pasos = 0; pasos < 96 && dl != NULL; pasos++) {
        u32 w0 = dl->words.w0, w1 = dl->words.w1;
        u8 op = w0 >> 24;

        dl++;
        switch (op) {
            case OP_VTX:
                return direccion_norma((uintptr_t) direccion_seg(w1));
            case OP_DL: {
                const Gfx *objetivo = (const Gfx *) direccion_seg(w1);

                if (((w0 >> 16) & 0xFF) == G_DL_PUSH) {
                    if (lsp >= MAX_DL_PROFUNDIDAD_LA) {
                        return 0;
                    }
                    lstack[lsp++] = dl;
                }
                dl = objetivo;
                break;
            }
            case OP_ENDDL:
                dl = (lsp > 0) ? lstack[--lsp] : NULL;
                break;
            case OP_RDPHALF_1:
                mitad1 = w1;
                break;
            case RAMA_Z_OP:
                dl = (const Gfx *) direccion_seg(mitad1);
                break;
            case G_TEXRECT:
            case G_TEXRECTFLIP:
                dl += 2;
                break;
            default:
                break;
        }
    }
    return 0;
}
