// Preparar textura

static int preparar_impl_tmem(int indice_tile, int tlut_modo, int banderas, InfoTextura *salida)
{
    const TileRdp *tile = &tiles_rdp[indice_tile];
    u32 tamanio_s, tamanio_t, decrementar_w, decrementar_h;
    int mir_s, mir_t, limitar_s, limitar_t;
    u32 k0, k1, semilla, x, ranura;
    EntradaCache *e;
    MemoPrep *m;
    int nativo;
    int de_pendiente;

    /* Pantallas gigantes */
    if (buscar_pantalla_gigante(cargar_orig_en(tile->tmem & (TAMANIO_TMEM / 8 - 1)), salida)) {
        return 1;
    }

    eje(tile->uls, tile->lrs, tile->masks, tile->cms, &tamanio_s, &decrementar_w, &mir_s, &limitar_s);
    eje(tile->ult, tile->lrt, tile->maskt, tile->cmt, &tamanio_t, &decrementar_h, &mir_t, &limitar_t);
    if (decrementar_w * decrementar_h > MAX_DECODIFICACION) {
        return 0;
    }

    /* Clave: parametros + contenido de la TMEM que se va a leer. */
    k0 = tile->fmt | (tile->siz << 3) | ((u32) tile->line << 5) | ((u32) tile->tmem << 14) |
         ((u32) tile->palette << 23) | ((u32) tlut_modo << 27) | ((u32) tile->cms << 29) |
         ((banderas & BLANCO_RGB_TMEM) ? 0x80000000u : 0);
    semilla = decrementar_w | (decrementar_h << 11) | ((u32) tile->cmt << 22) | ((u32) tile->masks << 24) | ((u32) tile->maskt << 28);
    /* Triangulos que repiten la textura */
    nativo = !(banderas & TMEM_PARA_RECT) && (!limitar_s || !limitar_t);
    if (nativo) {
        semilla ^= 0x5BD1E995u;
    }
    banderas &= ~TMEM_PARA_RECT;

    for (x = 0; x < TAMANIO_MEMO; x++) {
        m = &memo[x];
        if (m->version == tmem_version && m->k0 == k0 && m->seed == semilla && m->e != NULL && m->e->valido &&
            m->e->key[0] == m->ek0 && m->e->key[1] == m->ek1) {
            estadisticas_tmem.golpes++;
            estadisticas_tmem.golpes_memo++;
            rellenar_salida(m->e, salida);
            return 1;
        }
    }

    if (ci_tbp != 0 && tile->siz == G_IM_SIZ_8b && (tile->fmt == G_IM_FMT_CI || tlut_modo != 0) && !mir_s && !mir_t &&
        decrementar_w <= 64 && decrementar_h <= 32 && (decrementar_w & 7) == 0 && decrementar_h > 0 && tile->line * 8 >= decrementar_w &&
        preparar_ci(tile, tlut_modo, banderas, decrementar_w, decrementar_h, limitar_s, limitar_t, salida)) {
        return 1;
    }
#ifdef SMK64_PROF
    estadisticas_tmem.claves_calculadas++;
#endif
    k1 = clave_contenido(tile, tlut_modo, k0 ^ semilla, decrementar_w, decrementar_h, mir_s, mir_t, &de_pendiente, 1);
    if (buscar_en_cache(k0, k1, &e)) {
        goto encontrado;
    }
    if (de_pendiente) {
        /* La clave salio de la firma de una carga aun no copiada */
        u32 antes = k1;

        aplicar_cargas_pendientes();
        k1 = clave_contenido(tile, tlut_modo, k0 ^ semilla, decrementar_w, decrementar_h, mir_s, mir_t, &de_pendiente, 1);
        if (k1 != antes && buscar_en_cache(k0, k1, &e)) {
            goto encontrado;
        }
    }
    ranura = (k0 * 31 + k1) & (TAMANIO_CACHE - 1);

    {
        u32 arriba_w = (decrementar_w + 3) & ~3u;
        EMPEZAR_PROF(DECODIFICACION_PROF);

        decodificar_textura(tile, tlut_modo, banderas, decrementar_w, decrementar_h, arriba_w, mir_s, mir_t);

        {
            u16 ax, ay, ah;
            int envolver_s = !limitar_s, envolver_t = !limitar_t;

            const u8 *orig_ = cargar_orig_en(tile->tmem & (TAMANIO_TMEM / 8 - 1));
            EntradaCache *viejo = reutilizar_candidato(orig_, arriba_w, decrementar_h, envolver_s, envolver_t, nativo);

            if (viejo != NULL) {
                ax = viejo->x;
                ay = viejo->y;
                ah = viejo->ranura_h;
                viejo->valido = 0;
            } else if (!reservar_atlas(arriba_w, decrementar_h, envolver_s ? (decrementar_w > 4 ? decrementar_w : 4) : 4, nativo, &ax, &ay, &ah)) {
                /* Si repite, el hueco va alineado a su tamano (potencia de 2). */
                FIN_PROF(DECODIFICACION_PROF);
                return 0;
            }
            e = &s_cache[ranura];
            for (x = 0; x < 8; x++) {
                if (!s_cache[(ranura + x) & (TAMANIO_CACHE - 1)].valido) {
                    e = &s_cache[(ranura + x) & (TAMANIO_CACHE - 1)];
                    break;
                }
            }
            e->x = ax;
            e->y = ay;
            e->ranura_w = (u16) arriba_w;
            e->ranura_h = ah;
            e->key[0] = k0;
            e->key[1] = k1;
            e->w = decrementar_w;
            e->h = decrementar_h;
            e->envoltura_s = (u8) envolver_s;
            e->envoltura_t = (u8) envolver_t;
            e->src = orig_;
            e->nativo = (u8) nativo;
            e->tw = (u8) ilog2_ceil(decrementar_w);
            e->th = (u8) ilog2_ceil(decrementar_h);
            if (nativo) {
                /* Coordenadas propias */
                e->clamp = GS_SETREG_CLAMP(envolver_s ? 0 : 2, envolver_t ? 0 : 2, 0, envolver_s ? 0 : decrementar_w - 1, 0,
                                           envolver_t ? 0 : decrementar_h - 1);
            } else {
                e->clamp = GS_SETREG_CLAMP(envolver_s ? 3 : 2, envolver_t ? 3 : 2, envolver_s ? decrementar_w - 1 : ax,
                                           envolver_s ? ax : ax + decrementar_w - 1, envolver_t ? decrementar_h - 1 : ay,
                                           envolver_t ? ay : ay + decrementar_h - 1);
            }
            e->valido = 1;
            gs_upload_texture_rect(atlas_tbp, ATLAS_TBW, ax, ay, decodificacion, arriba_w, decrementar_h);
        }
        estadisticas_tmem.decodificaciones++;
        FIN_PROF(DECODIFICACION_PROF);
#ifdef SMK64_PROF
        {
            static u32 logged;

            if (estadisticas_tmem.decodificaciones > 20000 && logged < 200) {
                logged++;
                registrar("dec %ux%u up%u fmt%d siz%d tmem%u line%u pal%d tl%d cm%d%d m%d%d en %u,%u k %08x %08x", (unsigned) decrementar_w,
                        (unsigned) decrementar_h, (unsigned) arriba_w, tile->fmt, tile->siz, tile->tmem, tile->line, tile->palette,
                        tlut_modo, tile->cms, tile->cmt, tile->masks, tile->maskt, (unsigned) e->x, (unsigned) e->y,
                        (unsigned) k0, (unsigned) k1);
            }
        }
#endif
#ifdef SMK64_DEV_TEXDUMP
        {
            static int volcado;
            extern s32 estado_juego;

            if (estado_juego == 4 && volcado < 400) {
                char path[96];
                FILE *f;

                snprintf(path, sizeof(path), "host:tex_%03d_f%d_s%d_%ux%u_t%u_l%u_c%d%d_m%d%d_tl%d.raw", volcado,
                         tile->fmt, tile->siz, (unsigned) arriba_w, (unsigned) decrementar_h, tile->tmem, tile->line, tile->cms,
                         tile->cmt, tile->masks, tile->maskt, tlut_modo);
                bloquear_host();
                f = fopen(path, "wb");
                if (f) {
                    fwrite(decodificacion, 4, arriba_w * decrementar_h, f);
                    fclose(f);
                }
                desbloquear_host();
                volcado++;
            }
        }
#endif
#ifdef SMK64_GFX_TRACE
        registrar("tex fmt%d siz%d %ux%u tmem%u line%u tlut%d: %08x %08x %08x %08x", tile->fmt, tile->siz,
                (unsigned) decrementar_w, (unsigned) decrementar_h, tile->tmem, tile->line, tlut_modo, (unsigned) decodificacion[0],
                (unsigned) decodificacion[decrementar_w / 2], (unsigned) decodificacion[arriba_w * (decrementar_h / 2) + decrementar_w / 2],
                (unsigned) decodificacion[arriba_w * (decrementar_h - 1)]);
#endif
    }

encontrado:
    m = &memo[siguiente_memo++ % TAMANIO_MEMO];
    m->version = tmem_version;
    m->k0 = k0;
    m->seed = semilla;
    m->e = e;
    m->ek0 = e->key[0];
    m->ek1 = e->key[1];
    rellenar_salida(e, salida);
    return 1;
}
