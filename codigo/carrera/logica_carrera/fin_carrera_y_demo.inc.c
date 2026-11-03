// Fin carrera y demo

void funcion_8028FCBC(void) {
    Jugador* ply = &jugadores[0];
    s32 i;
    u32 phi_v0_4;

    if (mando_usar_demo) {
        actualizar_demo_fin();
    }
    switch (estado_carrera) {
        case NINGUNO_CARRERA:
            if (!modo_demo) {
                if (seleccion_modo == GRAN_PREMIO) {
                    reproducir_secuencia2(SEC_INICIAL_CARRERA_EVENTO);
                    reproducir_sonido2(SONIDO_MOTOR_REV_ACCION);
                    reproducir_sonido2(SONIDO_MOTOR_REV_ACCION_2);
                } else {
                    reproducir_secuencia2(SEC_INICIAL_CARRERA_EVENTO_VS);
                }
            }
            funcion_80002DAC();
            estado_carrera = PREP_CARRERA;
            dato_80150118 = 3.0f;
            modo_render_creditos = 0;
            dato_802BA032 = 0;
            dato_8015011E = 0;
            temporizador_circuito = 0.0f;
            temporizador_vblank = 0.0f;
            dato_800DC5B0 = 1;
            dato_800DC5B4 = 1;
            dato_802BA034 = 0.008f;
            dato_8015F894 = 0;
            if (seleccion_modo_pantalla != MODO_PANTALLA_1P) {
                funcion_8005C64C(&dato_8018D2AC);
            }
            for (i = 0; i < JUGADORES_NUM; i++) {
                if ((ply->type & EXISTE_JUGADOR) == 0) {
                    continue;
                }
                ply->type |= SECUENCIA_INICIO_JUGADOR;
                ply++;
            }
            dato_800DC5B8 = 1;
            break;
        case PREP_CARRERA:
            funcion_8028F914();
            if (dato_802BA034 == 1.0f) {
                if (modo_pantalla_activo != MODO_PANTALLA_1P) {
                    if (id_circuito_actual == CIRCUITO_LUIGI_RACEWAY) {
                        funcion_802A7940();
                    } else if (id_circuito_actual == CIRCUITO_WARIO_STADIUM) {
                        funcion_802A7728();
                    }
                }
                estado_carrera = PREPARACION_CARRERA;
                dato_800DC5B0 = 0;
                dato_800DC5B8 = 1;
                funcion_80078F64();
                if ((seleccion_modo == CONTRARRELOJ) && (b_circuito_fantasma_desactivado == 0)) {
                    phi_v0_4 = 0x1;
                    for (i = 0; i < id_circuito_actual; i++) {
                        phi_v0_4 <<= 1;
                    }
                    if ((dato_8015F890 == 0) && (!(dato_800DC5AC & phi_v0_4))) {
                        funcion_80092630();
                        dato_800DC5AC |= phi_v0_4;
                    }
                }
                if (seleccion_cantidad_jugador_1 == 3) {
                    funcion_800925A0();
                }
            }
            funcion_8028F4E8();
            break;
        case PREPARACION_CARRERA:
            if (modo_demo) {
                empezar_carrera();
            }
            if ((modo_depuracion_activacion) && (mando_cinco->boton_pulsado & Z_TRIG)) {
                empezar_carrera();
            }
            funcion_8028F4E8();
            break;
        case CARRERA_EN_PROGRESO:
            if (seleccion_modo == BATALLA) {
                actualizar_situacion_batalla_jugador();
            } else {
                actualizar_datos_posicion_carrera();
                funcion_8028EF28();
            }
            funcion_8028F4E8();
            funcion_8028F970();
            break;
        case CARRERA_HUMANO_TERMINADO:

            switch (seleccion_modo) {
                case GRAN_PREMIO:
                    funcion_8028F4E8();
                    actualizar_datos_posicion_carrera();
                    funcion_8028EF28();
                    funcion_8028F970();

                    switch (seleccion_modo_pantalla) {
                        case MODO_PANTALLA_1P:
                            temporizador_demo = 690;
                            estado_carrera = HECHO_CARRERA;
                            funcion_8028E298();
                            break;
                        case PANTALLA_MODO_2J_PANTALLA_DIVIDIDA_HORIZONTAL:
                        case PANTALLA_MODO_2J_PANTALLA_DIVIDIDA_VERTICAL:
                            if (((jugador_uno->type & MODO_CINEMATICA_JUGADOR) != 0) &&
                                ((jugador_dos->type & MODO_CINEMATICA_JUGADOR) != 0)) {

                                if (jugador_uno->puesto_actual < jugador_dos->puesto_actual) {
                                    indice_ganador_jugador = 1;
                                } else {
                                    indice_ganador_jugador = 0;
                                }

                                funcion_8028E298();
                                temporizador_demo = 600;
                                estado_carrera = HECHO_CARRERA;
                            }
                            break;
                    }
                    break;
                case VERSUS:
                    funcion_8028F4E8();
                    actualizar_datos_posicion_carrera();
                    funcion_8028EF28();
                    funcion_8028F970();
                    break;
                case CONTRARRELOJ:
                    temporizador_demo = 360;
                    if (dato_8015F890 != 0) {
                        estado_carrera = RESULTADOS_CUADRANTE_CARRERA;
                    } else {
                        estado_carrera = HECHO_CARRERA;
                    }
                    break;
            }
            break;
        case HECHO_CARRERA:
            if (temporizador_demo != 0) {
                temporizador_demo--;
            } else {
                switch (seleccion_modo) {
                    case GRAN_PREMIO:
                        if (dato_80150120 != 0) {
                            funcion_8028E678();
                        } else if (seleccion_modo_pantalla == MODO_PANTALLA_1P) {
                            funcion_80092564();
                            estado_carrera = RESULTADOS_CUADRANTE_CARRERA;
                        } else {
                            funcion_8028E438();
                        }
                        break;
                    case CONTRARRELOJ:
                        funcion_8028E678();
                        break;
                    case VERSUS:
                    case BATALLA:
                        funcion_8028E438();
                        break;
                }
            }
            funcion_8028F4E8();
            break;
        case ABANDONANDO_CARRERA:
            funcion_8028F8BC();
            if (dato_802BA034 <= 0) {
                es_en_abandonar_a_transicion_menu = 1;
                abandonar_a_contador_transicion_menu = 5;
            }
            break;
        case RESULTADOS_CUADRANTE_CARRERA:
            break;
    }
}

SIN_USO void funcion_80290314(void) {
    es_en_abandonar_a_transicion_menu = 1;
    abandonar_a_contador_transicion_menu = 5;
    modo_goto = MENU_INICIO_DESDE_ABANDONAR;
}

void funcion_80290338(void) {
    es_en_abandonar_a_transicion_menu = 1;
    abandonar_a_contador_transicion_menu = 5;
    modo_goto = MENU_PRINCIPAL_DESDE_ABANDONAR;
}

void funcion_80290360(void) {
    es_en_abandonar_a_transicion_menu = 1;
    abandonar_a_contador_transicion_menu = 5;
    modo_goto = MENU_SELECCION_JUGADOR_DESDE_ABANDONAR;
}

void funcion_80290388(void) {
    es_en_abandonar_a_transicion_menu = 1;
    abandonar_a_contador_transicion_menu = 5;
    modo_goto = MENU_SELECCION_CIRCUITO_DESDE_ABANDONAR;
}

void funcion_802903B0(void) {
    es_en_abandonar_a_transicion_menu = 1;
    abandonar_a_contador_transicion_menu = 5;
    modo_goto = CARRERA;
}

void funcion_802903D8(Jugador* jugador_uno_2, Jugador* jugador_dos_2) {
    f32 sp70 = (jugador_uno_2->tamanio_caja_envolvente + jugador_dos_2->tamanio_caja_envolvente) - 5.0f;
    f32 temporal_f0;
    f32 sp74;
    Vec3f sp60;
    Vec3f sp54;
    f32 temporal_f0_2;
    f32 temporal_f16;
    f32 temporal_f2;

    f32 arreglo_flotante_802B8790[] = { 1.2, 1.0, 0.9, 0.7, 2.0, 1.8, 0.9, 2.3 };

    f32 sp24 = arreglo_flotante_802B8790[jugador_uno_2->id_personaje];
    f32 sp20 = arreglo_flotante_802B8790[jugador_dos_2->id_personaje];

    sp60[0] = jugador_uno_2->pos[0] - jugador_dos_2->pos[0];
    sp60[1] = (jugador_uno_2->pos[1] - jugador_uno_2->tamanio_caja_envolvente) - (jugador_dos_2->pos[1] - jugador_dos_2->tamanio_caja_envolvente);
    sp60[2] = jugador_uno_2->pos[2] - jugador_dos_2->pos[2];

    sp54[0] = jugador_dos_2->velocidad[0] - jugador_uno_2->velocidad[0];
    sp54[1] = jugador_dos_2->velocidad[1] - jugador_uno_2->velocidad[1];
    sp54[2] = jugador_dos_2->velocidad[2] - jugador_uno_2->velocidad[2];

    temporal_f0 = sqrtf((sp60[0] * sp60[0]) + (sp60[1] * sp60[1]) + (sp60[2] * sp60[2]));

    if (temporal_f0 < 0.1f) {
        return;
    }

    sp74 = temporal_f0 - sp70;
    if (sp74 > 0) {
        return;
    }
    vibracion_choque_jugador(jugador_uno_2);
    vibracion_choque_jugador(jugador_dos_2);

    if (jugador_uno_2->type & jugador_desconocido_0_x40) {
        if (jugador_dos_2->type & jugador_desconocido_0_x40) {
            funcion_8008FC1C(jugador_uno_2);
            funcion_8008FC1C(jugador_dos_2);
            funcion_800C9060((jugador_dos_2 - jugador_uno), 0x19008001U);
            return;
        } else {
            jugador_dos_2->disparadores |= DISPARADOR_VUELCO_VERTICAL;
            funcion_8008FC1C(jugador_uno_2);
            funcion_800C9060((jugador_dos_2 - jugador_uno), 0x19008001U);
        }
    } else if (jugador_dos_2->type & jugador_desconocido_0_x40) {
        jugador_uno_2->disparadores |= DISPARADOR_VUELCO_VERTICAL;
        funcion_8008FC1C(jugador_dos_2);
        funcion_800C9060(jugador_uno_2 - jugador_uno, 0x19008001U);
        return;
    }
    if (jugador_uno_2->efectos & EFECTO_ESTRELLA) {
        if (!(jugador_dos_2->efectos & EFECTO_ESTRELLA)) {
            jugador_dos_2->disparadores |= GOLPE_POR_DISPARADOR_ESTRELLA;
        }
    } else if (jugador_dos_2->efectos & EFECTO_ESTRELLA) {
        jugador_uno_2->disparadores |= GOLPE_POR_DISPARADOR_ESTRELLA;
    } else {
        jugador_uno_2->efectos |= EFECTO_GOLPE_ENEMIGO;
        jugador_dos_2->efectos |= EFECTO_GOLPE_ENEMIGO;
    }
    temporal_f0_2 = sqrtf((sp54[0] * sp54[0]) + (sp54[1] * sp54[1]) + (sp54[2] * sp54[2]));
    sp60[0] /= temporal_f0;
    sp60[1] /= temporal_f0;
    sp60[2] /= temporal_f0;
    if (temporal_f0_2 < 0.2f) {
        temporal_f0 = (jugador_uno_2->tamanio_caja_envolvente + jugador_dos_2->tamanio_caja_envolvente) * 0.55f;
        jugador_uno_2->pos[0] = jugador_dos_2->pos[0] + (sp60[0] * temporal_f0);
        jugador_uno_2->pos[1] = jugador_dos_2->pos[1] + (sp60[1] * temporal_f0);
        jugador_uno_2->pos[2] = jugador_dos_2->pos[2] + (sp60[2] * temporal_f0);
        jugador_dos_2->pos[0] -= temporal_f0 * sp60[0];
        jugador_dos_2->pos[1] -= temporal_f0 * sp60[1];
        jugador_dos_2->pos[2] -= temporal_f0 * sp60[2];
        return;
    } else {
        temporal_f16 = ((sp60[0] * sp54[0]) + (sp60[1] * sp54[1]) + (sp60[2] * sp54[2])) / temporal_f0_2;
    }
    temporal_f0_2 = temporal_f0_2 * temporal_f16 * 0.85;
    if ((jugador_uno_2->efectos & EFECTO_ESTRELLA) != EFECTO_ESTRELLA) {
        temporal_f2 = (temporal_f0_2 * sp20) / sp24;
        jugador_uno_2->velocidad[0] += sp60[0] * temporal_f2;
        jugador_uno_2->velocidad[1] += sp60[1] * temporal_f2;
        jugador_uno_2->velocidad[2] += sp60[2] * temporal_f2;
        jugador_uno_2->pos[0] -= sp60[0] * sp74 * 0.5f;
        jugador_uno_2->pos[1] -= sp60[1] * sp74 * 0.5f;
        jugador_uno_2->pos[2] -= sp60[2] * sp74 * 0.5f;
    }
    if ((jugador_dos_2->efectos & EFECTO_ESTRELLA) != EFECTO_ESTRELLA) {
        temporal_f2 = (temporal_f0_2 * sp24) / sp20;
        jugador_dos_2->velocidad[0] -= sp60[0] * temporal_f2;
        jugador_dos_2->velocidad[1] -= sp60[1] * temporal_f2;
        jugador_dos_2->velocidad[2] -= sp60[2] * temporal_f2;
        jugador_dos_2->pos[0] += sp60[0] * sp74 * 0.5f;
        jugador_dos_2->pos[1] += sp60[1] * sp74 * 0.5f;
        jugador_dos_2->pos[2] += sp60[2] * sp74 * 0.5f;
    }
    if (jugador_uno_2->type & HUMANO_JUGADOR) {
        funcion_800C9060((jugador_uno_2 - jugador_uno), 0x19008001U);
        return;
    }
    if (jugador_dos_2->type & HUMANO_JUGADOR) {
        funcion_800C9060((jugador_dos_2 - jugador_uno), 0x19008001U);
    }
}

void funcion_802909F0(void) {
    Jugador* ply;
    Jugador* ply2;
    s32 i;
    s32 k;

    for (i = 0; i < 7; i++) {
        ply = &jugadores[i];

        if ((ply->type & EXISTE_JUGADOR) && (!(ply->efectos & BOO_EFECTO)) &&
            (!(ply->type & INVISIBLE_JUGADOR_O_BOMBA)) && (!(ply->efectos & EFECTO_APLASTAMIENTO))) {

            for (k = i + 1; k < JUGADORES_NUM; k++) {
                ply2 = &jugadores[k];

                if ((ply2->type & EXISTE_JUGADOR) && (!(ply2->efectos & BOO_EFECTO)) &&
                    (!(ply2->type & INVISIBLE_JUGADOR_O_BOMBA)) && (!(ply2->efectos & EFECTO_APLASTAMIENTO))) {

                    funcion_802903D8(ply, ply2);
                }
            }
        }
    }
}

void funcion_80290B14(void) {

    funcion_80059C50();

    switch (modo_pantalla_activo) {
        case MODO_PANTALLA_1P:
            funcion_8001EE98(copia_jugador_uno, camara1, 0);
            break;
        case PANTALLA_MODO_2J_PANTALLA_DIVIDIDA_HORIZONTAL:
        case PANTALLA_MODO_2J_PANTALLA_DIVIDIDA_VERTICAL:
            funcion_8001EE98(copia_jugador_uno, camara1, 0);
            funcion_8001EE98(copia_jugador_dos, camara2, 1);
            break;
        case PANTALLA_DIVIDIDA_MODO_3J_4J_PANTALLA:
            funcion_8001EE98(copia_jugador_uno, camara1, 0);
            funcion_8001EE98(jugador_dos, camara2, 1);
            funcion_8001EE98(jugador_tres, camara3, 2);
            funcion_8001EE98(jugador_cuatro, camara4, 3);
            break;
    }
}
