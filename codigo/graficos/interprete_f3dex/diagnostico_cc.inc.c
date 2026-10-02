// Diagnostico del combinador (solo con -DSMK64_DIAG_CC): compara el color exacto del RDP con el que
// sale del GS con las pasadas que arma combinar_color(), para tres valores de texel.

#ifdef SMK64_DIAG_CC

static float diag_rgb(const ColoresCombinador *col, const u8 *sh, int sel, int cual, int c, float t, const float comb[4])
{
    if (cual == 2) {
        switch (sel) {
            case 0: return comb[c];
            case 1: case 2: return t;
            case 3: return col->prim[c] / 255.0f;
            case 4: return sh[c] / 255.0f;
            case 5: return col->amb[c] / 255.0f;
            case 7: return comb[3];
            case 8: case 9: return t;
            case 10: return col->prim[3] / 255.0f;
            case 11: return sh[3] / 255.0f;
            case 12: return col->amb[3] / 255.0f;
            case 13: return 1.0f;
            case 14: return col->prim_lod_frac / 255.0f;
            default: return 0.0f;
        }
    }
    switch (sel) {
        case 0: return comb[c];
        case 1: case 2: return t;
        case 3: return col->prim[c] / 255.0f;
        case 4: return sh[c] / 255.0f;
        case 5: return col->amb[c] / 255.0f;
        case 6: return cual == 1 ? 0.0f : 1.0f;
        default: return 0.0f;
    }
}

static float diag_alfa(const ColoresCombinador *col, const u8 *sh, int sel, int cual, float t, const float comb[4])
{
    if (cual == 2) {
        switch (sel) {
            case 0: return 1.0f;
            case 1: case 2: return t;
            case 3: return col->prim[3] / 255.0f;
            case 4: return sh[3] / 255.0f;
            case 5: return col->amb[3] / 255.0f;
            case 6: return col->prim_lod_frac / 255.0f;
            default: return 0.0f;
        }
    }
    switch (sel) {
        case 0: return comb[3];
        case 1: case 2: return t;
        case 3: return col->prim[3] / 255.0f;
        case 4: return sh[3] / 255.0f;
        case 5: return col->amb[3] / 255.0f;
        case 6: return 1.0f;
        default: return 0.0f;
    }
}

static float diag_sat(float v)
{
    return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

/* Color exacto del combinador del RDP para un texel t (RGB y alfa iguales) */
static void diag_exacto(const u8 *sh, float t, float salida[4])
{
    ColoresCombinador col;
    float comb[4] = { 0, 0, 0, 0 };
    int cyc, c;

    colores_cc(&col);
    for (cyc = 0; cyc < (s_cc.dos_ciclo ? 2 : 1); cyc++) {
        float sig[4];

        for (c = 0; c < 3; c++) {
            sig[c] = (diag_rgb(&col, sh, s_cc.rgb_a[cyc], 0, c, t, comb) - diag_rgb(&col, sh, s_cc.rgb_b[cyc], 1, c, t, comb)) *
                         diag_rgb(&col, sh, s_cc.rgb_c[cyc], 2, c, t, comb) +
                     diag_rgb(&col, sh, s_cc.rgb_d[cyc], 3, c, t, comb);
        }
        sig[3] = (diag_alfa(&col, sh, s_cc.a_a[cyc], 0, t, comb) - diag_alfa(&col, sh, s_cc.a_b[cyc], 1, t, comb)) *
                     diag_alfa(&col, sh, s_cc.a_c[cyc], 2, t, comb) +
                 diag_alfa(&col, sh, s_cc.a_d[cyc], 3, t, comb);
        for (c = 0; c < 4; c++) {
            comb[c] = diag_sat(sig[c]);
        }
    }
    memcpy(salida, comb, sizeof(comb));
}

/* Lo que dibuja el GS con el color ya combinado */
static void diag_gs(const VtxColor *o, float t, float salida[4])
{
    const float rgb[3] = { o->r, o->g, o->b };
    const float agregar[3] = { o->ar, o->ag, o->ab };
    int c;

    for (c = 0; c < 3; c++) {
        if (!St.gs.texturizado) {
            salida[c] = diag_sat(rgb[c] / 255.0f);
        } else {
            float tex = diag_blanco ? 1.0f : t;
            float suma = agregar[c] >= 0.5f ? agregar[c] / 128.0f : 0.0f;

            salida[c] = diag_sat(tex * diag_sat(rgb[c] / 128.0f) + suma);
        }
    }
    salida[3] = diag_sat((St.gs.texturizado && alpha_desde_tex ? t : 1.0f) * diag_sat(o->a / 128.0f));
}

static void diag_combinador(const u8 sh[4], const struct VtxColor *o)
{
    static u32 vistos[512];
    static int n_vistos;
    static const float sondas[3] = { 0.0f, 0.5f, 1.0f };
    u32 prim, amb, clave;
    int i, k;

    if (n_vistos >= 512 || (!St.gs.texturizado && !prim_texturizado)) {
        return;
    }
    memcpy(&prim, St.prim, 4);
    memcpy(&amb, St.amb, 4);
    clave = St.combinacion0 * 0x9E3779B1u ^ St.combinacion1 * 0x85EBCA6Bu ^ prim * 0xC2B2AE35u ^ amb * 0x27D4EB2Fu ^
            ((u32) (sh[0] >> 5) << 1) ^ ((u32) (sh[3] >> 5) << 4) ^ (u32) diag_es_rect ^ ((u32) diag_blanco << 8);
    for (i = 0; i < n_vistos; i++) {
        if (vistos[i] == clave) {
            return;
        }
    }
    for (k = 0; k < 3; k++) {
        float e[4], g[4];

        diag_exacto(sh, sondas[k], e);
        diag_gs(o, sondas[k], g);
        /* Con alfa 0 no se ve el color */
        if (e[3] < 0.02f && g[3] < 0.02f) {
            continue;
        }
        /* Con G_AC_DITHER el alfa se promedia a proposito (ver alfa_dither): solo se compara el color */
        for (i = 0; i < (alfa_dither ? 3 : 4); i++) {
            float d = e[i] - g[i];

            if (d > 0.06f || d < -0.06f) {
                break;
            }
        }
        if (i < (alfa_dither ? 3 : 4)) {
            const TileRdp *t = &tiles_rdp[St.tex_tile];

            vistos[n_vistos++] = clave;
            registrar("DIAGCC cc %06x/%08x prim %02x%02x%02x%02x env %02x%02x%02x%02x sh %02x%02x%02x%02x rect %d blanco %d "
                      "tex %d fmt %d/%d timg %08x t %.1f exacto %.2f %.2f %.2f %.2f gs %.2f %.2f %.2f %.2f omL %08x",
                      (unsigned) St.combinacion0, (unsigned) St.combinacion1, St.prim[0], St.prim[1], St.prim[2], St.prim[3],
                      St.amb[0], St.amb[1], St.amb[2], St.amb[3], sh[0], sh[1], sh[2], sh[3], diag_es_rect, diag_blanco,
                      St.gs.texturizado, t->fmt, t->siz, (unsigned) diag_timg_seg, (double) sondas[k], (double) e[0],
                      (double) e[1], (double) e[2], (double) e[3], (double) g[0], (double) g[1], (double) g[2], (double) g[3],
                      (unsigned) St.om_l);
            return;
        }
    }
    vistos[n_vistos++] = clave;
}
#endif
