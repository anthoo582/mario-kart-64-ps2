// Listas dibujo

#ifdef SMK64_DEV
/* Volcado de la display list procesada de un frame a host */
#define DL_MAX_VOLCADO 60000
typedef struct {
    u32 w0, w1;
    u32 profundidad;
} DlEntradaVolcado;
static DlEntradaVolcado *dl_volcado;
static volatile int volcado_pedido;

/* Guion de pruebas (orden "volcado") */
void pedir_volcado_display_list(void)
{
    volcado_pedido = 1;
}
static u32 largo_volcado;

static void volcar_dl_a_host(const char *nombre)
{
    FILE *f;
    char path[64];

    snprintf(path, sizeof(path), "host:%s", nombre);
    bloquear_host();
    f = fopen(path, "wb");
    if (f != NULL) {
        fwrite(dl_volcado, sizeof(DlEntradaVolcado), largo_volcado, f);
        fclose(f);
    }
    desbloquear_host();
    registrar("display list volcada: %s (%u comandos)", nombre, (unsigned) largo_volcado);
}
#endif

static void ejecutar_dl(Gfx *dl)
{
    Gfx *pila[PROFUNDIDAD_DL_MAX];
    int sp = 0;
    int saltear_sp = -1;
    int descartar_en = estado_juego == 4;
    u32 guardia = 0;
    u32 t0 = leer_contador_ciclos();

    dl_ocupado = 1;
    while (dl != NULL) {
        u32 w0, w1;
        u8 op;

        /* Un puntero fuera de la RAM no es una display list */
        if ((uintptr_t) dl < 0x00100000 || (uintptr_t) dl >= 0x02000000 || ((uintptr_t) dl & 7) != 0) {
            if (dl_cuts++ < 4) {
                registrar("display list cortada: salto a %p tras %u comandos", (void *) dl, (unsigned) guardia);
            }
            dl_corte = 1;
            break;
        }
        dl_act = dl;
        dl_cantidad = guardia;
        w0 = dl->words.w0;
        w1 = dl->words.w1;
        op = w0 >> 24;

#ifdef SMK64_DEV
        if (dl_volcado != NULL && largo_volcado < DL_MAX_VOLCADO) {
            dl_volcado[largo_volcado].w0 = w0;
            dl_volcado[largo_volcado].w1 = w1;
            dl_volcado[largo_volcado].profundidad = sp;
            largo_volcado++;
        }
#endif
        dl++;
        if (++guardia > DL_MAX_COMMANDS) {
            dl_corte_2("sin fin", dl);
            break;
        }
        if ((guardia & 255) == 0 && leer_contador_ciclos() - t0 > DL_CICLOS_MAX) {
            dl_corte_2("demasiado lenta", dl);
            break;
        }
        switch (op) {
            case OP_SPNOOP:
            case G_NOOP:
            case G_RDPLOADSYNC:
            case G_RDPPIPESYNC:
            case G_RDPTILESYNC:
            case G_RDPFULLSYNC:
            case G_SETKEYGB:
            case G_SETKEYR:
            case G_SETCONVERT:
            case CARGAR_UCODE_OP:
                break;

            case OP_MTX:
                cmd_mtx(w0, w1, dl, pila, sp);
                break;
            case OP_POPMTX:
                if (St.profundidad_mv > 0) {
                    St.profundidad_mv--;
                }
                St.valido_mvp = 0;
                break;
            case OP_MOVEMEM:
                cmd_movemem(w0, w1);
                break;
            case OP_MOVEWORD:
                cmd_moveword(w0, w1);
                break;
            case OP_VTX:
#ifdef SMK64_GFX_TRACE
                if (tris_traza > 0) {
                    const Vtx *vv = (const Vtx *) direccion_seg(w1);

                    registrar("vtx w0 %08x @%08x n%d v0 %d: (%d,%d,%d) tc %d,%d", (unsigned) w0, (unsigned) w1,
                            (int) ((w0 >> 10) & 0x3F), (int) (((w0 >> 16) & 0xFF) / 2), vv->v.ob[0], vv->v.ob[1],
                            vv->v.ob[2], vv->v.tc[0], vv->v.tc[1]);
                }
#endif
                {
                    int n = (w0 >> 10) & 0x3F, d = ((w0 >> 16) & 0xFF) / 2;
                    u64 bits = (n >= 64 ? ~0ULL : ((1ULL << n) - 1)) << d;

                    if (saltear_sp >= 0) {
                        verts_viejo |= bits;
                        break;
                    }
                    verts_viejo &= ~bits;
                    EMPEZAR_PROF(PROF_VTX);
                    cargar_vertices((const Vtx *) direccion_seg(w1), n, d);
                    FIN_PROF(PROF_VTX);
                }
                break;
            case OP_TRI1:
            case OP_TRI2:
            case CUAD_OP:
                if (saltear_sp >= 0 || (verts_viejo != 0 && viejo_usa_tri(op, w0, w1))) {
                    break;
                }
                EMPEZAR_PROF(PROF_TRI);
                if (op == OP_TRI1) {
                    dibujar_triangulo(((w1 >> 16) & 0xFF) / 2, ((w1 >> 8) & 0xFF) / 2, (w1 & 0xFF) / 2);
                } else if (op == OP_TRI2) {
                    dibujar_triangulo(((w0 >> 16) & 0xFF) / 2, ((w0 >> 8) & 0xFF) / 2, (w0 & 0xFF) / 2);
                    dibujar_triangulo(((w1 >> 16) & 0xFF) / 2, ((w1 >> 8) & 0xFF) / 2, (w1 & 0xFF) / 2);
                } else {
                    dibujar_triangulo(((w1 >> 24) & 0xFF) / 2, ((w1 >> 16) & 0xFF) / 2, ((w1 >> 8) & 0xFF) / 2);
                    dibujar_triangulo(((w1 >> 24) & 0xFF) / 2, ((w1 >> 8) & 0xFF) / 2, (w1 & 0xFF) / 2);
                }
                FIN_PROF(PROF_TRI);
                break;
            case OP_CULLDL:
                if (descartar_decision(descartar_dl(w0, w1))) {
                    dl = (sp > 0) ? pila[--sp] : NULL;
                }
                break;
            case OP_SETGEOM:
                if ((St.geom | w1) != St.geom) {
                    St.geom |= w1;
                    St.sucio_estado = 1;
                    St.valido_luces = 0;
                }
                break;
            case OP_CLEARGEOM:
                if ((St.geom & ~w1) != St.geom) {
                    St.geom &= ~w1;
                    St.sucio_estado = 1;
                }
                break;
            case TEXTURA_OP: {
                int tile = (w0 >> 8) & 7, on = (w0 & 0xFF) != 0;
                float ss = (w1 >> 16) / 65536.0f, tt = (w1 & 0xFFFF) / 65536.0f;

                if (tile != St.tex_tile || on != St.tex_en || ss != St.escala_s_tex || tt != St.escala_t_tex) {
                    St.tex_tile = tile;
                    St.tex_en = on;
                    St.escala_s_tex = ss;
                    St.escala_t_tex = tt;
                    St.sucio_estado = 1;
                }
                break;
            }
            case OP_OTHERM_H:
            case OP_OTHERM_L: {
                u32 desplaz = (w0 >> 8) & 0xFF;
                u32 largo = w0 & 0xFF;
                u32 mascara = ((largo >= 32) ? 0xFFFFFFFFu : ((1u << largo) - 1)) << desplaz;
                u32 *om = (op == OP_OTHERM_H) ? &St.om_h : &St.om_l;
                u32 v = (*om & ~mascara) | (w1 & mascara);

                if (v != *om) {
                    *om = v;
                    St.sucio_estado = 1;
                }
                break;
            }
            case G_RDPSETOTHERMODE:
                if ((w0 & 0x00FFFFFF) != St.om_h || w1 != St.om_l) {
                    St.om_h = w0 & 0x00FFFFFF;
                    St.om_l = w1;
                    St.sucio_estado = 1;
                }
                break;
            case OP_DL: {
                Gfx *objetivo = (Gfx *) direccion_seg(w1);

                if (((w0 >> 16) & 0xFF) == G_DL_PUSH) {
                    if (sp >= PROFUNDIDAD_DL_MAX) {
                        break;
                    }
                    pila[sp++] = dl;
                    if (descartar_en && saltear_sp < 0 && seg_estatico(w1)) {
                        DlDescarte *e = busqueda_dlc(w1);

                        if (e != NULL && e->state == 1) {
                            dlc_tested++;
                            if (descartar_decision(fuera_dlc(e))) {
                                dlc_descartado++;
                                saltear_sp = sp;
                            }
                        }
                    }
                }
                dl = objetivo;
                break;
            }
            case OP_ENDDL:
                dl = (sp > 0) ? pila[--sp] : NULL;
                if (sp < saltear_sp) {
                    saltear_sp = -1;
                }
                break;
            case RAMA_Z_OP:
                dl = (Gfx *) direccion_seg(St.mitad_rdp_1);
                break;
            case OP_RDPHALF_1:
                St.mitad_rdp_1 = w1;
                break;
            case OP_RDPHALF_2:
            case OP_RDPHALF_C:
                break;

            case G_SETCIMG:
                St.cimg = (uintptr_t) direccion_seg(w1) & 0x1FFFFFFF;
                break;
            case G_SETZIMG:
                St.zimg = (uintptr_t) direccion_seg(w1) & 0x1FFFFFFF;
                break;
            case G_SETTIMG:
                DIAG_CC_TIMG(w1);
                tmem_fijar_imagen(direccion_seg(w1), (w0 >> 21) & 7, (w0 >> 19) & 3, (w0 & 0xFFF) + 1);
                break;
            case G_SETCOMBINE:
                if ((w0 & 0x00FFFFFF) != St.combinacion0 || w1 != St.combinacion1) {
                    St.combinacion0 = w0 & 0x00FFFFFF;
                    St.combinacion1 = w1;
                    St.sucio_estado = 1;
                }
                break;
            case G_SETPRIMCOLOR:
                if (fijar_color(St.prim, w1) | (St.prim_lod_frac != (w0 & 0xFF))) {
                    St.prim_lod_frac = w0 & 0xFF;
                    St.sucio_estado = 1;
                }
                break;
            case G_SETENVCOLOR:
                St.sucio_estado |= fijar_color(St.amb, w1);
                break;
            case G_SETBLENDCOLOR:
                St.sucio_estado |= fijar_color(St.mezcla, w1);
                break;
            case G_SETFOGCOLOR:
                St.sucio_estado |= fijar_color(St.fogc, w1);
                break;
            case G_SETFILLCOLOR:
                St.color_relleno = w1;
                break;
            case G_SETPRIMDEPTH:
                St.profundidad_prim = w1;
                break;
            case G_SETSCISSOR: {
                float sx = GS_ANCHO / 320.0f, sy = GS_ALTO / 240.0f;
                int viejo[4] = { St.tijera[0], St.tijera[1], St.tijera[2], St.tijera[3] };

                St.tijera[0] = (int) (((w0 >> 12) & 0xFFF) * 0.25f * sx);
                St.tijera[1] = (int) ((w0 & 0xFFF) * 0.25f * sy);
                St.tijera[2] = (int) (((w1 >> 12) & 0xFFF) * 0.25f * sx);
                St.tijera[3] = (int) ((w1 & 0xFFF) * 0.25f * sy);
                if (St.tijera[2] > GS_ANCHO) St.tijera[2] = GS_ANCHO;
                if (St.tijera[3] > GS_ALTO) St.tijera[3] = GS_ALTO;
                if (St.tijera[2] <= St.tijera[0]) St.tijera[2] = St.tijera[0] + 1;
                if (St.tijera[3] <= St.tijera[1]) St.tijera[3] = St.tijera[1] + 1;
#ifdef SMK64_DEV
                if (diag_frame) {
                    registrar("scissor %d,%d-%d,%d", St.tijera[0], St.tijera[1], St.tijera[2], St.tijera[3]);
                }
#endif
                if (memcmp(viejo, St.tijera, sizeof(viejo)) != 0) {
                    St.sucio_estado = 1;
                }
                break;
            }
            case G_SETTILE:
            case G_SETTILESIZE: {
                int ti = (w1 >> 24) & 7;
                TileRdp antes = tiles_rdp[ti];

                if (op == G_SETTILE) {
                    tmem_fijar_tile(w0, w1);
                } else {
                    tmem_fijar_tamanio_tile(w0, w1);
                }
                if (ti == St.tex_tile && memcmp(&antes, &tiles_rdp[ti], sizeof(antes)) != 0) {
                    St.sucio_estado = 1;
                }
                break;
            }
            case G_LOADBLOCK:
            case G_LOADTILE:
            case G_LOADTLUT: {
                if (cargar_repeated(op, w0, w1)) {
                    break; /* la TMEM ya tiene exactamente esto */
                }
                EMPEZAR_PROF(CARGA_PROF);
                if (op == G_LOADBLOCK) {
                    tmem_cargar_bloque(w0, w1);
                } else if (op == G_LOADTILE) {
                    tmem_cargar_tile(w0, w1);
                } else {
                    tmem_cargar_tlut(w0, w1);
                }
                FIN_PROF(CARGA_PROF);
                St.sucio_estado = 1;
                break;
            }
            case G_FILLRECT: {
                EMPEZAR_PROF(PROF_RECT);
                dibujar_fillrect(w0, w1);
                FIN_PROF(PROF_RECT);
                break;
            }
            case G_TEXRECT:
            case G_TEXRECTFLIP: {
                /* F3D: le siguen RDPHALF_2 (s, t) y RDPHALF_CONT (dsdx, dtdy). */
                u32 h2 = dl[0].words.w1;
                u32 hc = dl[1].words.w1;

                EMPEZAR_PROF(PROF_RECT);

                dl += 2;
                dibujar_texrect(w0, w1, h2, hc, op == G_TEXRECTFLIP);
                FIN_PROF(PROF_RECT);
                break;
            }
            default:
                ops_desconocido[op]++;
                break;
        }
    }
    dl_ocupado = 0;
}

void inicializar_renderizador(void)
{
    memset(&St, 0, sizeof(St));
#ifdef SMK64_DEV
    {
        /* Autoprueba de la transformacion con la VU0. */
        static Vtx tv[1] __attribute__((aligned(16)));
        static float res[4] __attribute__((aligned(16)));
        u32 situacion;
        int i, j;

        __asm__ __volatile__("mfc0 %0, $12" : "=r"(situacion));
        for (i = 0; i < 4; i++) {
            for (j = 0; j < 4; j++) {
                St.mvp[i][j] = (float) (i * 4 + j + 1);
            }
        }
        tv[0].v.ob[0] = 2;
        tv[0].v.ob[1] = -3;
        tv[0].v.ob[2] = 5;
        cargar_mvp_vu0();
        transformar_vu0(&tv[0].v, res);
        registrar("VU0: status %08x, (2,-3,5)*M = %d %d %d %d (esperado 45 50 55 60)", (unsigned) situacion, (int) res[0],
                (int) res[1], (int) res[2], (int) res[3]);
        memset(&St, 0, sizeof(St));
    }
#endif
    /* Valores por defecto del microcodigo */
    St.mirada[0][0] = 127;
    St.mirada[1][1] = 127;
    gs_inicializar();
    tmem_inicializar();
}

static void reiniciar_estado_rsp(void)
{
    int i;

    ultimo_carga.valido = 0;
    ultimo_prep.valido = 0;
    verts_viejo = 0;
    memset(St.seg, 0, sizeof(St.seg));
    for (i = 0; i < 4; i++) {
        int j;

        for (j = 0; j < 4; j++) {
            St.mv[0][i][j] = St.proy[i][j] = (i == j) ? 1.0f : 0.0f;
        }
    }
    St.profundidad_mv = 0;
    St.valido_mvp = 0;
    St.proporcion_recorte = 2;
    St.escala_vp[0] = 160.0f;
    St.escala_vp[1] = 120.0f;
    St.escala_vp[2] = 511.0f;
    St.vp_trans[0] = 160.0f;
    St.vp_trans[1] = 120.0f;
    St.vp_trans[2] = 511.0f;
    St.tijera[0] = 0;
    St.tijera[1] = 0;
    St.tijera[2] = GS_ANCHO;
    St.tijera[3] = GS_ALTO;
    St.sucio_estado = 1;
}

static u32 render_ciclos, frame_ciclos, ultimo_inicio_frame;
#ifdef SECCIONES_PROF_PS2
u32 ciclos_prof[CANTIDAD_PROF];
u32 prof_calls[CANTIDAD_PROF];
u32 inicio_prof[CANTIDAD_PROF];
u32 inicio_pre_prof[CANTIDAD_PROF];
volatile u32 prof_preempt;
#endif

#ifdef SMK64_DEV
/* Marca de control en builds de desarrollo */
static void dibujar_overlay_dev(void)
{
    static const u8 barras[8][3] = { { 0, 0, 0 }, { 0x40, 0x40, 0x40 }, { 0x80, 0x80, 0x80 }, { 0xFF, 0xFF, 0xFF },
                                   { 0xFF, 0, 0 }, { 0, 0xFF, 0 }, { 0, 0, 0xFF }, { 0x20, 0x20, 0x20 } };
    int i;

    gs_rellenar_rectangulo(0, 0, 16, 16, 0xFF, 0x00, 0xFF);
    for (i = 0; i < 8; i++) {
        gs_rellenar_rectangulo(i * 80, GS_ALTO - 24, i * 80 + 80, GS_ALTO, barras[i][0], barras[i][1], barras[i][2]);
    }
}
#else
#define dibujar_overlay_dev() ((void) 0)
#endif

static int interp_planned;
static int s_disp; /* buffer en pantalla al empezar la tarea */
#ifdef SMK64_DEV_ONLYINTERP
static int paquete_soltado;
#endif

/* Media movil de ciclos (7/8 lo anterior). */
static inline u32 ema(u32 avg, u32 muestra)
{
    return avg == 0 ? muestra : avg - (avg >> 3) + (muestra >> 3);
}

static void cancelar_gancho_interp(void)
{
    interp_planned = 0;
    redirigir_paquete_gs(REAL_PAQUETE_GS, s_disp ^ 1);
}

/* Se dibuja el intermedio si */
#define CICLOS_PRESUPUESTO_INTERP (25u * 294912u) /* 25 ms de los 33 del frame (lo libre medido decide) */
#define CICLOS_FRAME         (294912u * 100u / 3u)
/* Margenes amplios: en una PS2 real el costo de un frame varia mas que en el emulador y un
   intermedio que no entra atrasa la imagen real un retrazo entero (un tiron visible). */
#define MARGEN_INTERP_EN     (294912u * 8u)
#define APAGADO_MARGEN_INTERP    (294912u * 6u)
static u32 apagado_libre, libre_en; /* ciclos libres por frame, sin y con intermedio */
static u32 proporcion_interp = 176;
static u32 samp_total, inactivo_samp;
static int valido_libre;

static void liberar_measure(int interp_usado)
{
    u32 total, inactivo, dt, di, liberar_ciclos;

    cantidades_muestreador_ps2(&total, &inactivo);
    dt = total - samp_total;
    di = inactivo - inactivo_samp;
    samp_total = total;
    inactivo_samp = inactivo;
    if (dt < 8 || di > dt) {
        return; /* muestreador parado, o un periodo demasiado corto */
    }
    liberar_ciclos = (u32) ((u64) CICLOS_FRAME * di / dt);
    if (interp_usado) {
        libre_en = ema(libre_en, liberar_ciclos);
    } else {
        apagado_libre = ema(apagado_libre, liberar_ciclos);
    }
    valido_libre = 1;
}

static int candidato_interp(void)
{
    return interp_user && estado_juego == 4 && juego_en_pausa == 0;
}

/* Tras apagar el intermedio por falta de tiempo, tareas a 30 FPS antes de volver a probar:
   sin esto, cerca del limite se alterna 60 y 30 de un frame a otro y el ritmo tironea.
   Un pico aislado (un giro que mete mas geometria) solo espera unas tareas; si se repite antes
   de TAREAS_CALMA_INTERP tareas estables, la espera se duplica hasta el maximo. */
#define TAREAS_ESPERA_MIN_INTERP 6
#define TAREAS_ESPERA_MAX_INTERP 45
#define TAREAS_CALMA_INTERP      90
static u32 espera_reactivar, espera_escalada = TAREAS_ESPERA_MIN_INTERP, tareas_estables;
u32 cambios_modo_interp;

static int interp_decide(void)
{
    const TablaMtx *ant = &mtx_tab[act_mtx];

    if (!candidato_interp()) {
        return 0;
    }
    if (bloqueo_interp) {
        por_que_saltear_interp[3]++;
        return 0;
    }
    if (retroceso > 0) {
        retroceso--;
        por_que_saltear_interp[3]++;
        return 0;
    }
    if (espera_reactivar > 0) {
        espera_reactivar--;
        por_que_saltear_interp[3]++;
        return 0;
    }
    if (!ant->valido || ant->tarea != serie_tarea) {
        por_que_saltear_interp[4]++;
        return 0; /* la tarea anterior no grabo sus matrices */
    }
    {
        /* Costo del intermedio */
        u32 real = ultimo_real != 0 ? ultimo_real : real_costo;
        u32 interp = (edad_interp > 60 || interp_costo == 0) ? (u32) (((u64) real * proporcion_interp) >> 8) : interp_costo;
        int cabe;

        if (real + interp > CICLOS_PRESUPUESTO_INTERP) {
            cabe = 0;
            por_que_saltear_interp[0]++;
        } else if (!valido_libre) {
            cabe = 1;
        } else if (ultimo_usado_interp) {
            cabe = libre_en >= APAGADO_MARGEN_INTERP;
            por_que_saltear_interp[1] += !cabe;
            if (!cabe) {
                apagado_libre = libre_en + interp;
            }
        } else {
            cabe = interp + MARGEN_INTERP_EN <= apagado_libre;
            por_que_saltear_interp[2] += !cabe;
            if (cabe) {
                /* Lo libre con intermedio aun no se midio: lo previsto. */
                libre_en = apagado_libre - interp;
            }
        }
        if (!cabe) {
            edad_interp++;
            interp_salteado++;
            if (ultimo_usado_interp) {
                espera_reactivar = espera_escalada;
                espera_escalada = espera_escalada * 2 < TAREAS_ESPERA_MAX_INTERP ? espera_escalada * 2 : TAREAS_ESPERA_MAX_INTERP;
                tareas_estables = 0;
            }
            return 0;
        }
    }
    if (++tareas_estables >= TAREAS_CALMA_INTERP) {
        espera_escalada = TAREAS_ESPERA_MIN_INTERP;
    }
    return 1;
}

/* Ritmo real del juego: retrazos entre tareas de graficos. Con intermedio cada tarea dura 2;
   si dura mas, la imagen real se mostro tarde y se noto un tiron. Entonces se vuelve a 30 FPS
   estables: 5 s la primera vez, 20 s la segunda y, desde la tercera, el resto de la carrera.
   Mejor 30 parejos que alternar 60 y 30. */
#define FALLOS_BLOQUEO_INTERP 3
static u32 fallos_interp;

static void interp_feedback(u32 periodo)
{
    if (estado_juego != 4 || (estado_carrera & 0xFFFF) < CARRERA_EN_PROGRESO) {
        /* Cada carrera empieza de nuevo */
        fallos_interp = 0;
        bloqueo_interp = 0;
        retroceso = 0;
        return;
    }
    /* 3 o 4 retrazos: el intermedio no entro. Una pausa mas larga viene de otra cosa (carga, E/S). */
    if (!ultimo_usado_interp || periodo <= 2 || periodo > 4) {
        return;
    }
    fallos_interp++;
    if (fallos_interp >= FALLOS_BLOQUEO_INTERP) {
        bloqueo_interp = 1;
        rend_registro_ps2("60 FPS: %u tirones en esta carrera; sigue a 30 FPS hasta la proxima (libre %u us)",
                          (unsigned) fallos_interp, (unsigned) (libre_en / 295));
    } else {
        retroceso = fallos_interp == 1 ? 150 : 600;
        rend_registro_ps2("60 FPS: tiron (tarea de %u retrazos); %d tareas a 30 FPS (libre %u us)", (unsigned) periodo,
                          retroceso, (unsigned) (libre_en / 295));
    }
}

/* Ciclos de la sintesis de audio (tareas_rsp.c) */
extern volatile u32 ps2_audio_tarea_ciclos;
