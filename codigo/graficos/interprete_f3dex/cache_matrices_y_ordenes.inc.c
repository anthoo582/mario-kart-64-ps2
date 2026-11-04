// Cache matrices y ordenes

static int cerca_mtx(Mat4 a, Mat4 b)
{
    int i;
    float dx = a[3][0] - b[3][0], dy = a[3][1] - b[3][1], dz = a[3][2] - b[3][2];
    float escalar = 1.0f;

    for (i = 0; i < 3; i++) {
        float la = a[i][0] * a[i][0] + a[i][1] * a[i][1] + a[i][2] * a[i][2];
        float lb = b[i][0] * b[i][0] + b[i][1] * b[i][1] + b[i][2] * b[i][2];
        float d0 = a[i][0] - b[i][0], d1 = a[i][1] - b[i][1], d2 = a[i][2] - b[i][2];

        if (la > 4.0f * lb + 1e-6f || lb > 4.0f * la + 1e-6f) {
            return 0;
        }
        /* Giro de mas de ~60 grados en medio frame */
        if (d0 * d0 + d1 * d1 + d2 * d2 > 1.0f * (la > lb ? la : lb) + 1e-6f) {
            return 0;
        }
        if (la > escalar) {
            escalar = la;
        }
    }
    /* Traslacion */
    return dx * dx + dy * dy + dz * dz <= 300.0f * 300.0f * escalar;
}

/* Matriz cargada por G_MTX (ya convertida) */
static void matriz_interp(Mat4 m, u32 direccion_orig, int tipo, const Gfx *dl, Gfx **pila, int sp)
{
    u32 clave = (tipo == PROY_TIPO_MTX) ? direccion_norma(direccion_orig) : vtx_anticipacion(dl, pila, sp);
    TablaMtx *act = &mtx_tab[act_mtx];

    if (clave == 0 && tipo == MV_TIPO_MTX) {
        clave = direccion_norma(direccion_orig);
    }
    if (pasada == REAL_PASADA) {
        CantidadMtx *c = ranura_cnt(&act->cnt, clave, tipo, 1);

        if (c == NULL) {
            act->valido = 0;
            return;
        }
        agregar_rec(act, clave, c->count++, tipo, m);
        return;
    }
    {
        const TablaMtx *ant = &mtx_tab[act_mtx ^ 1];
        CantidadMtx *ci = ranura_cnt(&mtx_cnt_interp, clave, tipo, 1);
        const CantidadMtx *cc = ranura_cnt(&act->cnt, clave, tipo, 0);
        const CantidadMtx *cp = ranura_cnt((TablaConteos *) &ant->cnt, clave, tipo, 0);
        const MtxRec *rp;
        u32 occ;
        int i, j;

        if (ci == NULL || cc == NULL || cp == NULL || cc->count != cp->count) {
            return;
        }
        occ = ci->count++;
        rp = buscar_rec(ant, clave, occ, tipo);
        if (rp == NULL || !cerca_mtx(rp->m, m)) {
            return;
        }
        for (i = 0; i < 4; i++) {
            for (j = 0; j < 4; j++) {
                m[i][j] = 0.5f * (rp->m[i][j] + m[i][j]);
            }
        }
    }
}

static int descartar_decision(int descartado)
{
    if (pasada == REAL_PASADA) {
        if (cantidad_descarte < MAX_DESCARTE) {
            if (descartado) {
                bits_descarte[cantidad_descarte >> 3] |= (u8) (1 << (cantidad_descarte & 7));
            } else {
                bits_descarte[cantidad_descarte >> 3] &= (u8) ~(1 << (cantidad_descarte & 7));
            }
            cantidad_descarte++;
        } else {
            desborde_descarte = 1;
        }
        return descartado;
    }
    if (pos_descarte >= cantidad_descarte) {
        interp_abort = 1; /* el recorrido se separo del real */
        return descartado;
    }
    descartado = (bits_descarte[pos_descarte >> 3] >> (pos_descarte & 7)) & 1;
    pos_descarte++;
    return descartado;
}

static void cmd_mtx(u32 w0, u32 w1, const Gfx *siguiente, Gfx **pila, int sp)
{
    u32 p = (w0 >> 16) & 0xFF;
    Mat4 m;

    mtx_desde_n64(m, (const u32 *) direccion_seg(w1));
    if (s_recording || pasada == INTERP_PASADA) {
        matriz_interp(m, (uintptr_t) direccion_seg(w1), (p & G_MTX_PROJECTION) ? PROY_TIPO_MTX : MV_TIPO_MTX, siguiente, pila,
                      sp);
    }
#ifdef SMK64_GFX_TRACE
    if (tris_traza > 0) {
        registrar("mtx p%x @%08x -> %p: %d %d %d %d / %d %d %d %d / %d %d %d %d (x1000)", (unsigned) p, (unsigned) w1,
                direccion_seg(w1), (int) (m[0][0] * 1000), (int) (m[0][1] * 1000), (int) (m[0][2] * 1000),
                (int) (m[0][3] * 1000), (int) (m[1][1] * 1000), (int) (m[2][2] * 1000), (int) (m[2][3] * 1000),
                (int) (m[3][3] * 1000), (int) (m[3][0] * 1000), (int) (m[3][1] * 1000), (int) (m[3][2] * 1000),
                (int) (m[2][3] * 1000));
    }
#endif
    if (p & G_MTX_PROJECTION) {
        if (p & G_MTX_LOAD) {
            memcpy(St.proy, m, sizeof(Mat4));
        } else {
            mul_mat(St.proy, m, St.proy);
        }
    } else {
        if ((p & G_MTX_PUSH) && St.profundidad_mv < PILA_MTX - 1) {
            memcpy(St.mv[St.profundidad_mv + 1], St.mv[St.profundidad_mv], sizeof(Mat4));
            St.profundidad_mv++;
        }
        if (p & G_MTX_LOAD) {
            memcpy(St.mv[St.profundidad_mv], m, sizeof(Mat4));
        } else {
            mul_mat(St.mv[St.profundidad_mv], m, St.mv[St.profundidad_mv]);
        }
    }
    St.valido_mvp = 0;
}

static void cmd_movemem(u32 w0, u32 w1)
{
    u32 idx = (w0 >> 16) & 0xFF;
    const u8 *p = (const u8 *) direccion_seg(w1);

    if (idx == G_MV_VIEWPORT) {
        const Vp_t *vp = (const Vp_t *) p;

        St.escala_vp[0] = vp->vscale[0] / 4.0f;
        St.escala_vp[1] = vp->vscale[1] / 4.0f;
        St.escala_vp[2] = vp->vscale[2] / 4.0f;
        St.vp_trans[0] = vp->vtrans[0] / 4.0f;
        St.vp_trans[1] = vp->vtrans[1] / 4.0f;
        St.vp_trans[2] = vp->vtrans[2] / 4.0f;
        ws_viewport();
        St.sucio_estado = 1; /* el scissor de los triangulos depende del viewport */
#ifdef SMK64_DEV
        if (diag_frame) {
            registrar("viewport %p escala %d %d trasl %d %d", (const void *) vp, (int) vp->vscale[0], (int) vp->vscale[1],
                    (int) vp->vtrans[0], (int) vp->vtrans[1]);
        }
#endif
    } else if (idx >= G_MV_L0 && idx <= G_MV_L7) {
        int n = (idx - G_MV_L0) / 2;
        const Light_t *l = (const Light_t *) p;

        St.luces[n].col[0] = l->col[0];
        St.luces[n].col[1] = l->col[1];
        St.luces[n].col[2] = l->col[2];
        St.luces[n].dir[0] = l->dir[0];
        St.luces[n].dir[1] = l->dir[1];
        St.luces[n].dir[2] = l->dir[2];
        St.valido_luces = 0;
    } else if (idx == G_MV_LOOKATX || idx == G_MV_LOOKATY) {
        const Light_t *l = (const Light_t *) p;
        int n = (idx == G_MV_LOOKATX) ? 0 : 1;

        St.mirada[n][0] = l->dir[0];
        St.mirada[n][1] = l->dir[1];
        St.mirada[n][2] = l->dir[2];
    }
}

static void cmd_moveword(u32 w0, u32 w1)
{
    u32 index = w0 & 0xFF;
    u32 desplazamiento = (w0 >> 8) & 0xFFFF;

    switch (index) {
        case G_MW_SEGMENT:
            St.seg[(desplazamiento / 4) & 0xF] = w1 & 0x1FFFFFFF;
            break;
        case G_MW_NUMLIGHT:
            St.luces_num = (int) ((w1 - 0x80000000u) / 32) - 1;
            if (St.luces_num < 0) {
                St.luces_num = 0;
            } else if (St.luces_num > LUCES_MAX) {
                St.luces_num = LUCES_MAX;
            }
            St.valido_luces = 0;
            break;
        case G_MW_CLIP:
            if (desplazamiento == G_MWO_CLIP_RNX && (s16) (w1 & 0xFFFF) >= 1 && (s16) (w1 & 0xFFFF) <= 6) {
                St.proporcion_recorte = (int) (s16) (w1 & 0xFFFF);
                St.sucio_estado = 1;
            }
            break;
        case G_MW_FOG:
            St.mul_niebla = (s16) (w1 >> 16);
            St.apagado_niebla = (s16) (w1 & 0xFFFF);
            break;
        case G_MW_LIGHTCOL: {
            int n = desplazamiento / 0x20;

            if (n <= LUCES_MAX && (desplazamiento & 7) == 0) {
                St.luces[n].col[0] = w1 >> 24;
                St.luces[n].col[1] = w1 >> 16;
                St.luces[n].col[2] = w1 >> 8;
            }
            break;
        }
        case G_MW_POINTS: {
            int vi = desplazamiento / 40;
            int where = desplazamiento % 40;

            if (vi < MAX_VERTS) {
                St.v[vi].gen_cc = 0;
                St.v[vi].gen_proy = 0;
                if (where == G_MWO_POINT_RGBA) {
                    St.v[vi].r = w1 >> 24;
                    St.v[vi].g = w1 >> 16;
                    St.v[vi].b = w1 >> 8;
                    St.v[vi].a = w1;
                } else if (where == G_MWO_POINT_ST) {
                    St.v[vi].s = (s16) (w1 >> 16);
                    St.v[vi].t = (s16) (w1 & 0xFFFF);
                }
            }
            break;
        }
        default:
            break;
    }
}

static int descartar_dl(u32 w0, u32 w1)
{
    int vs = (w0 & 0xFFFF) / 40, ve = w1 / 40 - 1;
    int i;
    u8 y_recorte = 0xFF;
    /* In widescreen the view reaches ws_ndc_x. With 60 FPS the interpolated frame reuses this
       decision with the camera half a step back: keep a margin so it does not lose what is at the
       edges of its view. */
    float my = s_recording ? 1.25f : 1.0f, mx = my * ws_ndc_x;

    if (vs < 0 || ve >= MAX_VERTS || ve < vs) {
        return 0;
    }
    for (i = vs; i <= ve; i++) {
        const VerticeRsp *v = &St.v[i];
        u8 c = 0;

        if (v->x < -mx * v->w) c |= IZQUIERDA_RECORTE;
        if (v->x > mx * v->w) c |= DERECHA_RECORTE;
        if (v->y > my * v->w) c |= ARRIBA_RECORTE;
        if (v->y < -my * v->w) c |= BOT_RECORTE;
        if (v->z < -v->w) c |= RECORTE_CERCA;
        y_recorte &= c;
    }
    return y_recorte != 0;
}

#define TAMANIO_DLC 4096
#define DLC_MIN_VERTS 8

typedef struct {
    u32 addr;  /* direccion segmentada de la lista (G_DL) */
    u16 gen;
    u8 state;  /* 1: se puede descartar; 2: no */
    float mn[3], mx[3];
} DlDescarte;

static DlDescarte dlc[TAMANIO_DLC];
static u16 gen_dlc = 1;
static s32 circuito_dlc = -1;
static u64 verts_viejo;   /* vertices que una lista descartada no cargo */
static u32 dlc_descartado, dlc_tested, usa_viejo;

extern s16 id_circuito_actual;

static int ram_ptr(uintptr_t a, u32 bytes)
{
    return a >= 0x00100000 && a + bytes <= 0x02000000 && (a & 7) == 0;
}

/* Direccion segmentada de datos estaticos */
static inline int seg_estatico(u32 w1)
{
    u32 seg = (w1 >> 24) & 0xFF;

    return seg >= 2 && seg <= 15;
}

static int analizar_dlc(u32 direccion, DlDescarte *e)
{
    const Gfx *pila[8];
    const Gfx *dl = (const Gfx *) direccion_seg(direccion);
    int sp = 0, nv = 0, pasos = 0;
    u64 cargado_2 = 0;
    float mn[3] = { 1e30f, 1e30f, 1e30f }, mx[3] = { -1e30f, -1e30f, -1e30f };

    for (;;) {
        u32 w0, w1;
        u8 op;

        if (++pasos > 1024 || !ram_ptr((uintptr_t) dl, 8)) {
            return 0;
        }
        w0 = dl->words.w0;
        w1 = dl->words.w1;
        op = w0 >> 24;
        dl++;
        switch (op) {
            case OP_VTX: {
                int n = (w0 >> 10) & 0x3F, dst = ((w0 >> 16) & 0xFF) / 2, i;
                const Vtx *v = (const Vtx *) direccion_seg(w1);

                if (!seg_estatico(w1) || n == 0 || dst + n > MAX_VERTS || !ram_ptr((uintptr_t) v, n * sizeof(Vtx))) {
                    return 0;
                }
                for (i = 0; i < n; i++) {
                    int c;

                    for (c = 0; c < 3; c++) {
                        float f = v[i].v.ob[c];

                        mn[c] = f < mn[c] ? f : mn[c];
                        mx[c] = f > mx[c] ? f : mx[c];
                    }
                }
                cargado_2 |= ((n >= 64) ? ~0ULL : ((1ULL << n) - 1)) << dst;
                nv += n;
                break;
            }
            case OP_TRI1:
            case OP_TRI2:
            case CUAD_OP: {
                /* Solo vertices cargados dentro de la propia lista. */
                u32 idx[6] = { ((w1 >> 16) & 0xFF) / 2, ((w1 >> 8) & 0xFF) / 2, (w1 & 0xFF) / 2,
                               ((w0 >> 16) & 0xFF) / 2, ((w0 >> 8) & 0xFF) / 2, (w0 & 0xFF) / 2 };
                int k, cnt = 3;

                if (op == OP_TRI2) {
                    cnt = 6;
                } else if (op == CUAD_OP) {
                    idx[0] = ((w1 >> 24) & 0xFF) / 2;
                    idx[3] = ((w1 >> 16) & 0xFF) / 2;
                    idx[4] = ((w1 >> 8) & 0xFF) / 2;
                    idx[5] = (w1 & 0xFF) / 2;
                    cnt = 6;
                }
                for (k = 0; k < cnt; k++) {
                    if (idx[k] >= MAX_VERTS || !((cargado_2 >> idx[k]) & 1)) {
                        return 0;
                    }
                }
                break;
            }
            case OP_DL:
                if (!seg_estatico(w1)) {
                    return 0;
                }
                if (((w0 >> 16) & 0xFF) == G_DL_PUSH) {
                    if (sp >= 8) {
                        return 0;
                    }
                    pila[sp++] = dl;
                }
                dl = (const Gfx *) direccion_seg(w1);
                break;
            case OP_ENDDL:
                if (sp == 0) {
                    if (nv < DLC_MIN_VERTS) {
                        return 0;
                    }
                    memcpy(e->mn, mn, sizeof(mn));
                    memcpy(e->mx, mx, sizeof(mx));
                    return 1;
                }
                dl = pila[--sp];
                break;
            case OP_MTX:
            case OP_POPMTX:
            case OP_MOVEMEM:
            case OP_MOVEWORD:
            case RAMA_Z_OP:
            case OP_CULLDL:
            case CARGAR_UCODE_OP:
            case OP_RDPHALF_1:
            case OP_RDPHALF_2:
            case OP_RDPHALF_C:
            case G_TEXRECT:
            case G_TEXRECTFLIP:
            case G_FILLRECT:
            case G_SETCIMG:
            case G_SETZIMG:
                return 0;
            default:
                break; /* estado del RDP y cargas: se aplican igual */
        }
    }
}

static DlDescarte *busqueda_dlc(u32 direccion)
{
    u32 h = (direccion * 0x9E3779B1u) >> 20;
    u32 i;

    for (i = 0; i < 16; i++) {
        DlDescarte *e = &dlc[(h + i) & (TAMANIO_DLC - 1)];

        if (e->gen != gen_dlc) {
            e->gen = gen_dlc;
            e->addr = direccion;
            e->state = analizar_dlc(direccion, e) ? 1 : 2;
            return e;
        }
        if (e->addr == direccion) {
            return e;
        }
    }
    return NULL; /* tabla llena por aqui: se dibuja sin probar */
}

static int fuera_dlc(const DlDescarte *e)
{
    float r = (float) St.proporcion_recorte * 1.25f * ws_ndc_x; /* widescreen: the view is wider */
    u32 all = 0x1F;
    int i;

    actualizar_mvp();
    for (i = 0; i < 8 && all != 0; i++) {
        float x = (i & 1) ? e->mx[0] : e->mn[0];
        float y = (i & 2) ? e->mx[1] : e->mn[1];
        float z = (i & 4) ? e->mx[2] : e->mn[2];
        float cx = x * St.mvp[0][0] + y * St.mvp[1][0] + z * St.mvp[2][0] + St.mvp[3][0];
        float cy = x * St.mvp[0][1] + y * St.mvp[1][1] + z * St.mvp[2][1] + St.mvp[3][1];
        float cz = x * St.mvp[0][2] + y * St.mvp[1][2] + z * St.mvp[2][2] + St.mvp[3][2];
        float cw = x * St.mvp[0][3] + y * St.mvp[1][3] + z * St.mvp[2][3] + St.mvp[3][3];
        float rw = r * cw;
        u32 c = 0;

        if (cx < -rw) c |= 1;
        if (cx > rw) c |= 2;
        if (cy < -rw) c |= 4;
        if (cy > rw) c |= 8;
        if (cz < -cw) c |= 16;
        all &= c;
    }
    return all != 0;
}

/* Al empezar cada tarea */
static void empezar_frame_dlc(void)
{
    s32 clave = (estado_juego == 4) ? id_circuito_actual : -1 - estado_juego;

    if (clave != circuito_dlc) {
        circuito_dlc = clave;
        if (++gen_dlc == 0) {
            memset(dlc, 0, sizeof(dlc));
            gen_dlc = 1;
        }
    }
    verts_viejo = 0;
}

/* Cargas repetidas */
static struct {
    u32 op, w0, w1, timg_fmt, gen;
    const void *timg;
    TileRdp tile;
    int valido;
} ultimo_carga;

static int cargar_repeated(u32 op, u32 w0, u32 w1)
{
    u32 fmt, generar;
    const void *timg = tmem_timg(&fmt);
    const TileRdp *t = &tiles_rdp[(w1 >> 24) & 7];
    int mismo = ultimo_carga.valido && ultimo_carga.op == op && ultimo_carga.w0 == w0 && ultimo_carga.w1 == w1 &&
               ultimo_carga.timg == timg && ultimo_carga.timg_fmt == fmt &&
               /* uls..lrt los pone la propia carga (salen del comando) */
               memcmp(&ultimo_carga.tile, t, offsetof(TileRdp, uls)) == 0;
    /* Como mucho lo que cabe en la TMEM (4 KB) desde la imagen. */
    int es_estatico = timg != NULL && estatico_origen_tmem(timg, 4096, &generar);

    if (mismo && es_estatico && ultimo_carga.gen == generar) {
        return 1;
    }
    ultimo_carga.op = op;
    ultimo_carga.w0 = w0;
    ultimo_carga.w1 = w1;
    ultimo_carga.timg = timg;
    ultimo_carga.timg_fmt = fmt;
    ultimo_carga.tile = *t;
    ultimo_carga.gen = generar;
    ultimo_carga.valido = es_estatico;
    return 0;
}

static int viejo_usa_tri(u8 op, u32 w0, u32 w1)
{
    {
        u64 used = 0;
        u32 a = ((w1 >> 16) & 0xFF) / 2, b = ((w1 >> 8) & 0xFF) / 2, c = (w1 & 0xFF) / 2;

        if (op == CUAD_OP) {
            a = ((w1 >> 24) & 0xFF) / 2;
            b = ((w1 >> 16) & 0xFF) / 2;
            c = ((w1 >> 8) & 0xFF) / 2;
            used |= 1ULL << ((w1 & 0xFF) / 2 & 63);
        }
        used |= (1ULL << (a & 63)) | (1ULL << (b & 63)) | (1ULL << (c & 63));
        if (op == OP_TRI2) {
            used |= (1ULL << (((w0 >> 16) & 0xFF) / 2 & 63)) | (1ULL << (((w0 >> 8) & 0xFF) / 2 & 63)) |
                    (1ULL << ((w0 & 0xFF) / 2 & 63));
        }
        if ((used & verts_viejo) == 0) {
            return 0;
        }
    }
    usa_viejo++;
#ifdef SMK64_DEV
    if (usa_viejo <= 8) {
        registrar("descarte de listas: un triangulo usa vertices de una lista descartada (%08x %08x)", (unsigned) w0,
                (unsigned) w1);
    }
#endif
    return 1;
}

/* Guarda un color RGBA de un comando del RDP; 1 si cambio. */
static inline int fijar_color(u8 c[4], u32 w1)
{
    u32 viejo = ((u32) c[0] << 24) | ((u32) c[1] << 16) | ((u32) c[2] << 8) | c[3];

    if (viejo == w1) {
        return 0;
    }
    c[0] = w1 >> 24;
    c[1] = w1 >> 16;
    c[2] = w1 >> 8;
    c[3] = w1;
    return 1;
}

static inline u32 leer_contador_ciclos(void)
{
    u32 c;

    __asm__ __volatile__("mfc0 %0, $9" : "=r"(c));
    return c;
}

#define PROFUNDIDAD_DL_MAX 18

/* Limites del recorrido */
#define DL_MAX_COMMANDS 400000
#define DL_CICLOS_MAX   (294912000u / 2) /* 0,5 s del EE (294,912 MHz) */

static const Gfx *volatile dl_act;
static volatile u32 dl_cantidad;
static volatile int dl_ocupado;
static u32 dl_cuts;
static int dl_corte; /* el frame en curso se corto: sin frame intermedio */

static void dl_corte_2(const char *por_que, const Gfx *dl)
{
    dl_corte = 1;
    if (dl_cuts++ < 4) {
        registrar("display list cortada (%s): %u comandos, en %p: %08x %08x", por_que, (unsigned) dl_cantidad,
                (const void *) (dl - 1), (unsigned) dl[-1].words.w0, (unsigned) dl[-1].words.w1);
    }
}

int ps2_gfx_cuelgue_info(char *salida, int size)
{
    const Gfx *dl = dl_act;

    if (!dl_ocupado || dl == NULL) {
        return 0;
    }
    snprintf(salida, size, " Render: dl %p %08x %08x, %u comandos; cortes %u, cargas malas %u", (const void *) dl,
             (unsigned) dl->words.w0, (unsigned) dl->words.w1, (unsigned) dl_cantidad, (unsigned) dl_cuts,
             (unsigned) loads_malo_tmem);
    return 1;
}
