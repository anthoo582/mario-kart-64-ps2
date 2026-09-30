// Tarea graficos

void ejecutar_tarea_graficos(Gfx *dl)
{
    u32 t0 = leer_contador_ciclos();
    u32 audio0 = ps2_audio_tarea_ciclos;
    u32 t_real;
    int negro = pantalla_en_negro();
    int candidato;

    if (ultimo_inicio_frame != 0) {
        frame_ciclos += t0 - ultimo_inicio_frame;
    }
    ultimo_inicio_frame = t0;
#ifdef SMK64_MEDIDOR
    medidor_inicio_frame();
#endif
    {
        u32 vb = contador_vblank();

        ultimo_periodo = vb - ultimo_vblank_tarea;
        suma_periodos += ultimo_periodo;
        liberar_measure(ultimo_usado_interp);
        interp_feedback(ultimo_periodo);
        ultimo_vblank_tarea = vb;
    }
    s_disp = buffer_mostrado_gs();
    candidato = candidato_interp();
    interp_planned = interp_decide();
    serie_tarea++;

    empezar_frame_dlc();

    act_mtx ^= 1;
    reiniciar_tabla(&mtx_tab[act_mtx]);
    mtx_tab[act_mtx].tarea = serie_tarea;
    mtx_tab[act_mtx].valido = candidato;
    s_recording = candidato;
    cantidad_descarte = 0;
    desborde_descarte = 0;
    pasada = REAL_PASADA;
    dl_corte = 0;
    pasada_empezar_tmem(interp_planned ? REGISTRO_PASADA_TMEM : NORMAL_PASADA_TMEM);
    estado_anticipado = interp_planned;
    pasada_para_estado = interp_planned ? 1 : 0;
    rec_n_estado = 0;
    ok_rec_estado = 1;
    subidas_capturar_gs(interp_planned);
    /* Con intermedio, N va al buffer que ahora esta en pantalla */
    empezar_paquete_gs(REAL_PAQUETE_GS, interp_planned ? s_disp : s_disp ^ 1, 0);
    fijar_gancho_desborde_gs(interp_planned ? cancelar_gancho_interp : NULL);
    tmem_empezar_frame();
    empezar_frame_pantallas_gigantes();
    reiniciar_estado_rsp();
#ifdef SMK64_GFX_TRACE
    if ((s_frame % 120) == 100) {
        tris_traza = 40;
    }
#endif
#ifdef SMK64_DEV_BLUE
    gs_rellenar_rectangulo(0, 0, GS_ANCHO, GS_ALTO, 0, 0, 0xC0);
#endif
#ifdef SMK64_DEV
    {
        static DlEntradaVolcado buf_volcado[DL_MAX_VOLCADO];

        static u32 frames_carrera;

        frames_carrera = (estado_juego == 4) ? frames_carrera + 1 : 0;
        if (s_frame == 150 || frames_carrera == 90 || volcado_pedido) {
            volcado_pedido = 0;
            dl_volcado = buf_volcado;
            largo_volcado = 0;
        }
    }
#endif
#ifdef SMK64_DEV
    diag_frame = dl_volcado != NULL;
#endif
#ifndef SMK64_DEV_NODL
    {
        EMPEZAR_PROF(PROF_DL);
        ejecutar_dl(dl);
        FIN_PROF(PROF_DL);
    }
#endif
#ifdef SMK64_DEV
    diag_frame = 0;
#endif
#ifdef SMK64_DEV
    if (dl_volcado != NULL) {
        char nombre[32];

        snprintf(nombre, sizeof(nombre), "dl_frame%u.bin", (unsigned) s_frame);
        volcar_dl_a_host(nombre);
        dl_volcado = NULL;
    }
#endif
    dibujar_overlay_dev();
#ifdef SMK64_MEDIDOR
    medidor_dibujar();
#endif
    meta_paquete_gs(negro);
    fijar_gancho_desborde_gs(NULL);
    subidas_capturar_gs(0);
    s_recording = 0;
    if (desborde_descarte) {
        mtx_tab[act_mtx].valido = 0;
    }
    t_real = leer_contador_ciclos() - t0 - (ps2_audio_tarea_ciclos - audio0);
    real_costo = ema(real_costo, t_real);
    ultimo_real = t_real;
    if (interp_planned && (dl_corte || tmem_pasada_fallido() || gs_capturar_fallido() || !mtx_tab[act_mtx].valido)) {
        interp_planned = 0;
#ifdef SMK64_PROF
        por_que_interp[4 + (tmem_pasada_fallido() ? 0 : gs_capturar_fallido() ? 1 : 2)]++;
#endif
    }

    if (interp_planned) {
        u32 t1 = leer_contador_ciclos();
        u32 audio1 = ps2_audio_tarea_ciclos;

        pasada = INTERP_PASADA;
        pasada_para_estado = 2;
        pos_rec_estado = 0;
        interp_abort = 0;
        pos_descarte = 0;
        reiniciar_conteos(&mtx_cnt_interp);
        pasada_empezar_tmem(REPETICION_PASADA_TMEM);
        empezar_paquete_gs(INTERP_PAQUETE_GS, s_disp ^ 1, GS_PAQUETE_SIN_ENVIO | GS_PAQUETE_CONSERVAR_ESTADISTICAS);
        gs_emitir_capturado_subidas();
        reiniciar_estado_rsp();
#ifndef SMK64_DEV_NODL
        ejecutar_dl(dl);
#endif
        dibujar_overlay_dev();
#ifdef SMK64_MEDIDOR
        medidor_dibujar();
#endif
        meta_paquete_gs(negro);
        if (tmem_pasada_fallido() || interp_abort || gs_paquete_fallido(INTERP_PAQUETE_GS) || pos_descarte != cantidad_descarte) {
            interp_planned = 0;
            interp_salteado++;
#ifdef SMK64_PROF
            por_que_interp[tmem_pasada_fallido() ? 0 : interp_abort ? 1 : gs_paquete_fallido(INTERP_PAQUETE_GS) ? 2 : 3]++;
#endif
        }
        pasada_empezar_tmem(NORMAL_PASADA_TMEM);
        pasada = REAL_PASADA;
        {
            u32 t_interp = leer_contador_ciclos() - t1 - (ps2_audio_tarea_ciclos - audio1);

            interp_costo = (edad_interp > 60) ? t_interp : ema(interp_costo, t_interp);
            if (t_real > 0 && t_interp < t_real * 2) {
                proporcion_interp = (u32) (((u64) t_interp << 8) / t_real);
            }
        }
        edad_interp = 0;
    }
    estado_anticipado = 0;
    pasada_para_estado = 0;

    /* --- Presentacion --- */
    if (interp_planned) {
#ifdef SMK64_PROF
        u32 vb_i0 = contador_vblank(), vb_i1, c_i = leer_contador_ciclos();
#endif
        mostrar_paquete_gs(INTERP_PAQUETE_GS);
        interp_frames++;
#ifdef SMK64_MEDIDOR
        medidor_frame_intermedio();
#endif
#ifndef SMK64_DEV_ONLYINTERP
        mostrar_paquete_gs(REAL_PAQUETE_GS); /* se muestra un retrazo despues de I */
#else
        /* Depuracion */
        paquete_soltado = 1;
#endif
#ifdef SMK64_PROF
        vb_i1 = contador_vblank();
        {
            static int logged;

            if (logged < 80) {
                logged++;
                rend_registro_ps2("interp: tarea vb %u (+%u) real %u us interp %u us envioI vb %u envioN vb %u espera %u us",
                        (unsigned) ultimo_vblank_tarea, (unsigned) ultimo_periodo, (unsigned) (t_real / 295),
                        (unsigned) (interp_costo / 295), (unsigned) vb_i0, (unsigned) vb_i1,
                        (unsigned) ((leer_contador_ciclos() - c_i) / 295));
            }
        }
#endif
    } else {
        redirigir_paquete_gs(REAL_PAQUETE_GS, s_disp ^ 1);
        mostrar_paquete_gs(REAL_PAQUETE_GS);
    }
    cambios_modo_interp += (ultimo_usado_interp != 0) != (interp_planned != 0);
    ultimo_usado_interp = interp_planned;
#ifdef SMK64_MEDIDOR
    medidor_fin_frame();
#endif
    alimentar_perro_guardian();
    ps2_tiempos_frame_shown(dl);
    render_ciclos += leer_contador_ciclos() - t0;
#ifdef SMK64_PROF
    ciclos_prof[RENDER_PROF] += leer_contador_ciclos() - t0;
    prof_calls[RENDER_PROF]++;
#endif

    s_frame++;
#if defined(SMK64_DEBUG) || defined(SMK64_DEV)
    if ((s_frame % 120) == 1) {
        int i;

        rend_registro_ps2("perf: render %u ms/frame, frame completo %u ms (media de 120)", (unsigned) (render_ciclos / 120 / 294912),
                (unsigned) (frame_ciclos / 120 / 294912));
#ifdef SMK64_PROF
        {
            static const char *const nombres[CANTIDAD_PROF] = { "render", "dl", "vtx", "tri", "rect", "state",
                                                           "load", "prep", "decode", "kick", "audio", "juego",
                                                           "audiojuego", "comb", "clip", "emit" };
            char line[220];
            int k, largo = 0;

            /* Microsegundos por frame (ciclos / 294,912 / 120 frames). */
            largo += snprintf(line + largo, sizeof(line) - largo, "prof us/f:");
            for (k = 0; k < CANTIDAD_PROF && largo < (int) sizeof(line) - 16; k++) {
                largo += snprintf(line + largo, sizeof(line) - largo, " %s %u", nombres[k],
                                (unsigned) ((ciclos_prof[k] / 295u) / 120u));
            }
            rend_registro_ps2("%s frame %u", line, (unsigned) ((frame_ciclos / 295u) / 120u));
            largo = snprintf(line, sizeof(line), "prof llamadas/f:");
            for (k = 1; k < CANTIDAD_PROF && largo < (int) sizeof(line) - 16; k++) {
                largo += snprintf(line + largo, sizeof(line) - largo, " %s %u", nombres[k], (unsigned) (prof_calls[k] / 120u));
            }
            rend_registro_ps2("%s", line);
            {
                extern u32 prof_x[8], prof_xn[8];

                rend_registro_ps2("prof estado us/f: analiza %u (%u) afecta %u (%u) aditiva %u (%u) gs %u (%u) generaciones %u (%u)",
                        (unsigned) (prof_x[0] / 295 / 120), (unsigned) (prof_xn[0] / 120), (unsigned) (prof_x[1] / 295 / 120),
                        (unsigned) (prof_xn[1] / 120), (unsigned) (prof_x[2] / 295 / 120), (unsigned) (prof_xn[2] / 120),
                        (unsigned) (prof_x[3] / 295 / 120), (unsigned) (prof_xn[3] / 120), (unsigned) (prof_x[4] / 295 / 120),
                        (unsigned) (prof_xn[4] / 120));
                memset(prof_x, 0, sizeof(prof_x));
                memset(prof_xn, 0, sizeof(prof_xn));
            }
            rend_registro_ps2("prof tmem/f: copia %u B hash %u B (paletas %u B) firmas %u B; memo %u claves %u firmas: paleta %u "
                    "carga %u",
                    (unsigned) (estadisticas_tmem.bytes_carga / 120), (unsigned) (estadisticas_tmem.hash_bytes / 120),
                    (unsigned) (estadisticas_tmem.bytes_hash_paleta / 120), (unsigned) (estadisticas_tmem.bytes_firmas / 120),
                    (unsigned) (estadisticas_tmem.golpes_memo / 120), (unsigned) (estadisticas_tmem.claves_calculadas / 120),
                    (unsigned) (estadisticas_tmem.firmas_paleta_usadas / 120), (unsigned) (estadisticas_tmem.firmas_carga_usadas / 120));
#ifdef SMK64_SIGMAP
            {
                extern u32 mapa_sig[512];
                extern uintptr_t tabla_segmento[16];
                int j, mejor;

                rend_registro_ps2("sigmap: seg1 %x seg2 %x seg3 %x seg4 %x seg5 %x seg6 %x seg7 %x seg9 %x segD %x segF %x",
                        (unsigned) tabla_segmento[1], (unsigned) tabla_segmento[2], (unsigned) tabla_segmento[3],
                        (unsigned) tabla_segmento[4], (unsigned) tabla_segmento[5], (unsigned) tabla_segmento[6],
                        (unsigned) tabla_segmento[7], (unsigned) tabla_segmento[9], (unsigned) tabla_segmento[13],
                        (unsigned) tabla_segmento[15]);
                for (k = 0; k < 8; k++) {
                    mejor = -1;
                    for (j = 0; j < 512; j++) {
                        if (mapa_sig[j] != 0 && (mejor < 0 || mapa_sig[j] > mapa_sig[mejor])) {
                            mejor = j;
                        }
                    }
                    if (mejor < 0) {
                        break;
                    }
                    rend_registro_ps2("sigmap: %06x-%06x %u B/f", (unsigned) (mejor << 16), (unsigned) ((mejor + 1) << 16),
                            (unsigned) (mapa_sig[mejor] / 120));
                    mapa_sig[mejor] = 0;
                }
                memset(mapa_sig, 0, sizeof(mapa_sig));
            }
#endif
            rend_registro_ps2("prof firmas/f: memorizadas %u, viejas (SMK64_SIGMEMO_CHECK) %u total",
                    (unsigned) (estadisticas_tmem.golpes_memo_sig / 120), (unsigned) estadisticas_tmem.malo_memo_sig);
            estadisticas_tmem.golpes_memo_sig = 0;
            estadisticas_tmem.bytes_carga = estadisticas_tmem.hash_bytes = estadisticas_tmem.golpes_memo = estadisticas_tmem.bytes_firmas = 0;
            estadisticas_tmem.bytes_hash_paleta = estadisticas_tmem.claves_calculadas = estadisticas_tmem.firmas_paleta_usadas = estadisticas_tmem.firmas_carga_usadas = 0;
            memset(ciclos_prof, 0, sizeof(ciclos_prof));
            memset(prof_calls, 0, sizeof(prof_calls));
        }
#endif
        render_ciclos = frame_ciclos = 0;
        rend_registro_ps2("60 FPS: %s, intermedios %u, omitidos %u, costo real %u us intermedio %u us, cambios de pantalla %u, "
                     "libre %u us sin intermedio y %u us con el",
                interp_user ? "si" : "no", (unsigned) interp_frames, (unsigned) interp_salteado,
                (unsigned) (real_costo / 295), (unsigned) (interp_costo / 295), (unsigned) estadisticas_gs.volteos,
                (unsigned) (apagado_libre / 295), (unsigned) (libre_en / 295));
#ifdef SMK64_PROF
        rend_registro_ps2("60 FPS descartes: I tex %u recorrido %u lleno %u cull %u; N desalojo %u subidas %u matrices %u",
                (unsigned) por_que_interp[0], (unsigned) por_que_interp[1], (unsigned) por_que_interp[2], (unsigned) por_que_interp[3],
                (unsigned) por_que_interp[4], (unsigned) por_que_interp[5], (unsigned) por_que_interp[6]);
#endif
        rend_registro_ps2("60 FPS sin intermedio por: tope %u, libre con el %u, libre sin el %u, pausa %u, matrices %u",
                (unsigned) por_que_saltear_interp[0], (unsigned) por_que_saltear_interp[1], (unsigned) por_que_saltear_interp[2],
                (unsigned) por_que_saltear_interp[3], (unsigned) por_que_saltear_interp[4]);
        memset(por_que_saltear_interp, 0, sizeof(por_que_saltear_interp));
        rend_registro_ps2("ritmo: %u.%02u retrazos por tarea (media de 120)", (unsigned) (suma_periodos / 120),
                (unsigned) (suma_periodos % 120 * 100 / 120));
        rend_registro_ps2("listas fuera de la vista: %u de %u probadas por tarea; triangulos con vertices sin cargar %u; "
                "estado del intermedio sin reproducir %u",
                (unsigned) (dlc_descartado / 120), (unsigned) (dlc_tested / 120), (unsigned) usa_viejo,
                (unsigned) miss_rec_estado);
        dlc_descartado = dlc_tested = 0;
        {
            extern u32 subidas_ci, golpes_ci;

            rend_registro_ps2("texturas con paleta nativas: %u subidas, %u sin subir (por tarea); decodificaciones %u",
                    (unsigned) (subidas_ci / 120), (unsigned) (golpes_ci / 120), (unsigned) estadisticas_tmem.decodificaciones);
            subidas_ci = golpes_ci = 0;
        }
        rend_registro_ps2("imagenes: %u de 1 retrazo, %u de 2, %u de 3, %u de 4 o mas; cambios 60/30 %u",
                (unsigned) estadisticas_gs.mantenido[0], (unsigned) estadisticas_gs.mantenido[1],
                (unsigned) estadisticas_gs.mantenido[2], (unsigned) estadisticas_gs.mantenido[3], (unsigned) cambios_modo_interp);
        cambios_modo_interp = 0;
        memset(estadisticas_gs.mantenido, 0, sizeof(estadisticas_gs.mantenido));
        suma_periodos = 0;
        rend_registro_ps2("frame %u estado %d: tris %u rect %u subidas %u (%u KB) cache %u/%u paquete %u KB", (unsigned) s_frame,
                (int) estado_juego, (unsigned) estadisticas_gs.triangulos, (unsigned) estadisticas_gs.sprites,
                (unsigned) estadisticas_gs.subidas, (unsigned) (estadisticas_gs.bytes_subidos / 1024), (unsigned) estadisticas_tmem.golpes,
                (unsigned) estadisticas_tmem.decodificaciones, (unsigned) (estadisticas_gs.bytes_paquete / 1024));
#ifdef SMK64_DEV
        {
            extern s32 seleccion_menu;
            extern u32 duracion_transicion[];
            extern u32 tiempo_transicion_actual[];
            extern s8 tipo_transicion[];
            extern u16 juego_en_pausa;

            rend_registro_ps2("  menu %d pausa %d transiciones: %d:%u/%u %d:%u/%u %d:%u/%u %d:%u/%u 4:%d:%u/%u",
                    (int) seleccion_menu, (int) juego_en_pausa, tipo_transicion[0], (unsigned) tiempo_transicion_actual[0],
                    (unsigned) duracion_transicion[0], tipo_transicion[1], (unsigned) tiempo_transicion_actual[1],
                    (unsigned) duracion_transicion[1], tipo_transicion[2], (unsigned) tiempo_transicion_actual[2],
                    (unsigned) duracion_transicion[2], tipo_transicion[3], (unsigned) tiempo_transicion_actual[3],
                    (unsigned) duracion_transicion[3], tipo_transicion[4], (unsigned) tiempo_transicion_actual[4],
                    (unsigned) duracion_transicion[4]);
        }
#endif
        for (i = 0; i < 256; i++) {
            if (ops_desconocido[i]) {
                registrar("  opcode desconocido %02x x%u", i, (unsigned) ops_desconocido[i]);
                ops_desconocido[i] = 0;
            }
        }
    }
#else
    if ((s_frame % 120) == 1) {
        int i;

        render_ciclos = frame_ciclos = 0;
        suma_periodos = 0;
        for (i = 0; i < 256; i++) {
            if (ops_desconocido[i]) {
                registrar("opcode desconocido %02x x%u", i, (unsigned) ops_desconocido[i]);
                ops_desconocido[i] = 0;
            }
        }
    }
#endif

#ifdef SMK64_DEV
    if (captura_pendiente_guion() != NULL) {
        gs_esperar_cambio_buffer();
        volcar_frame(captura_pendiente_guion(), gs_pantalla_a_ct32() / 256, GS_ANCHO, GS_ALTO, GS_PSM_CT32);
        captura_hecha_guion();
    }
#endif
#ifdef SMK64_DEV_FRAMEDUMP
    /* Volcados del framebuffer */
    if (s_frame == 60 || s_frame == 200 || s_frame == 400 || s_frame == 700 || (s_frame % 1500) == 0) {
        char nombre[32];

        /* Espera a que el frame este en pantalla y lo guarda. */
        gs_esperar_cambio_buffer();
        snprintf(nombre, sizeof(nombre), "frame%05u", (unsigned) s_frame);
        volcar_frame(nombre, gs_vram_en_pantalla() / 256, GS_ANCHO, GS_ALTO, GS_PSM_CT16S);
    }
#endif
}
