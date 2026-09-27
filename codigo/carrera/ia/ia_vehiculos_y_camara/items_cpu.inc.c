// Items cpu

void funcion_8001A518(s32 parametro0, s32 parametro1, s32 parametro2) {
    switch (parametro1) { /* irregular */
        case 0:
            dato_80164680[parametro2] = 1;
            break;
        case 1:
        case 2:
        case 3:
            funcion_8001A450(parametro0, parametro2, parametro0);
            break;
        default:
            dato_80164680[parametro2] = 0;
            break;
    }
}

void funcion_8001A588(SIN_USO u16* local_d_80152300, Camara* camara, Jugador* jugador, s8 index, s32 indice_camara) {
    s32 variable_v1;
    desconocido_struct_46D0* temporal_v0_4;
    s32 sp44;
    s32 id_jugador;
    id_jugador = camara->id_jugador;

    if (seleccion_modo == CONTRARRELOJ) {
        id_jugador = 0;
    }
    funcion_80019FB4(indice_camara);

    if ((s32) (camara->pos[0] * 10.0) == (s32) ((f64) camara->mirar_a[0] * 10.0)) {

        if ((s32) (camara->pos[2] * 10.0) == (s32) ((f64) camara->mirar_a[2] * 10.0)) {
            camara->pos[0] = (f32) (camara->pos[0] + 100.0);
            camara->pos[2] = (f32) (camara->pos[2] + 100.0);
        }
    }
    if ((seleccion_modo != BATALLA) && (dato_80164680[indice_camara] == -1) && (jugador->type & MODO_CINEMATICA_JUGADOR) &&
        ((u16) dato_801646CC == 0) && (dato_801646C8 == 0)) {
        if (seleccion_modo == VERSUS) {
            funcion_8001A220(id_jugador, indice_camara);
        } else {
            funcion_8001A124((s32) id_jugador, indice_camara);
        }
        empezar_disparo_cinematica_camara((s32) id_jugador, indice_camara);
    }

    if ((dato_80164680[indice_camara] == 14) || (dato_80164680[indice_camara] == 0)) {
        funcion_80019D2C(camara, jugador, indice_camara);
    } else {
        dato_801646C0[indice_camara] = 0;
        calcular_vector_arriba_camara(camara, indice_camara);
    }
    switch ((u16) dato_801646CC) {
        case 1:
            dato_801646C8 += 1;
            if (dato_801646C8 >= 501) {
                dato_801646C8 = 0;
            }
            if ((indice_camara == 0) && (((dato_801646C8 == 10)) || (dato_801646C8 == 11))) {
                funcion_8001A518((s32) id_jugador, gp_actual_carrera_puesto_por_id_jugador[id_jugador], 0);
            }
            if ((seleccion_modo != CONTRARRELOJ) && (indice_camara == 1) &&
                (((dato_801646C8 == 260)) || (dato_801646C8 == 261))) {

                variable_v1 = 0;
                if (cantidad_jugador == 2) {
                    funcion_8001A518((s32) id_jugador, gp_actual_carrera_puesto_por_id_jugador[id_jugador], 1);
                } else {
                    sp44 = (s32) id_jugador;
                    while (variable_v1 != 8) {
                        id_jugador += 1;
                        variable_v1 += 1;
                        if (id_jugador >= 8) {
                            id_jugador = 1;
                        }
                        if ((!(jugadores[id_jugador].lakitu_props & MANTENIDO_POR_LAKITU) &&
                             !(jugadores[id_jugador].lakitu_props & LAKITU_ESCENA))) {
                            break;
                        }
                    }
                    funcion_8001A450(sp44, indice_camara, (s32) id_jugador);
                }
            }
            break;
        case 2:
            dato_801646C8 += 1;
            if (dato_801646C8 > 250) {
                dato_801646C8 = 0;
            }
            if ((indice_camara == 0) && (dato_801646C8 == 10)) {
                funcion_8001A450((s32) id_jugador, indice_camara, (s32) id_jugador);
            }
            break;
        default:
            temporal_v0_4 = &dato_801646D0[indice_camara];
            if (temporal_v0_4->desconocido0 == (s16) 1) {
                id_jugador = temporal_v0_4->desconocido4;
                temporal_v0_4->desconocido0 = 0;
                camaras[indice_camara].id_jugador = id_jugador;
                funcion_8001A3D8(indice_camara, 0.0f, (s32) temporal_v0_4->desconocido2);
            }
            break;
    }
    funcion_80019C50(indice_camara);
    switch (dato_80164680[indice_camara]) {
        case 0:
            funcion_80015390(camara, jugador, index);
            break;
        case 2:
        case 3:
            funcion_8001577C(camara, jugador, index, indice_camara);
            break;
        case 6:
        case 7:
            funcion_80015C94(camara, jugador, index, indice_camara);
            break;
        case 4:
        case 5:
            funcion_80016494(camara, jugador, index, indice_camara);
            break;
        case 9:
            funcion_80017054(camara, jugador, index, indice_camara);
            break;
        case 1:
            funcion_800178F4(camara, jugador, index, indice_camara);
            break;
        case 14:
            funcion_800180F0(camara, jugador, index, indice_camara);
            break;
        case 8:
            funcion_800188F4(camara, jugador, index, indice_camara);
            break;
        case 12:
        case 13:
            funcion_8001933C(camara, jugador, index, indice_camara);
            break;
        case 15:
        case 16:
            funcion_80019760(camara, jugador, index, indice_camara);
            break;
        default:
            funcion_80015390(camara, jugador, index);
            break;
    }
}

void funcion_8001AAAC(s16 parametro0, s16 parametro1, s16 parametro2) {
    if (dato_801646D0[parametro0].desconocido0 == 0) {
        dato_801646D0[parametro0].desconocido0 = 1;
        dato_801646D0[parametro0].desconocido2 = parametro1;
        dato_801646D0[parametro0].desconocido4 = parametro2;
    }
}

#include "estrategia_items_cpu.inc.c"

void cpu_usar_item_estrategia(s32 id_jugador) {
    Jugador* jugador = &jugador_uno[id_jugador];
    struct Actor* actor;
    CpuItemEstrategiaDatos* estrategia_cpu = &cpu_item_estrategia[id_jugador];
    PuntoCaminoPista* punto_camino;
    bool es_banana_valido_1;
    bool es_banana_valido_2;

#define BANANA_ACTOR(actor) ((struct BananaActor*)(actor))
#define ACTOR_CAPARAZON(actor) ((struct ActorCaparazon*)(actor))
#define ACTOR_CAJA_ITEM_FALSO(actor) ((struct CajaItemFalsa*)(actor))
#define ACTOR_GRUPO_BANANA(actor) ((struct PadreGrupoBanana*)(actor))

    if (seleccion_modo == CONTRARRELOJ) {
        return;
    }

    if ((u16) dato_801646CC == 1) {
        return;
    }

    if (jugador->type & MODO_CINEMATICA_JUGADOR) {
        return;
    }

    switch (estrategia_cpu->rama) {
        case CPU_ESTRATEGIA_ESPERA_SIGUIENTE_ITEM:
            estrategia_cpu->indice_actor = -1;
            if ((((id_jugador * 20) + 100) < num_camino_puntos_recorrido[id_jugador]) && (estrategia_cpu->temporizador >= 0x259) &&
                (estrategia_cpu->usar_item_num < 3) && (cantidad_vuelta_por_id_jugador[id_jugador] < 3)) {
                cpu_decisiones_rama_item(id_jugador, &estrategia_cpu->rama,
                                            cpu_gen_aleatorio_item((s16) cantidad_vuelta_por_id_jugador[id_jugador],
                                                                gp_actual_carrera_puesto_por_id_jugador[id_jugador]));
            } else {
                funcion_8001ABE0(id_jugador, estrategia_cpu);
            }
            break;

        case CPU_ESTRATEGIA_ITEM_BANANA:
            // never true
            if ((cantidad_vuelta_por_id_jugador[id_jugador] > 0) && (gp_actual_carrera_puesto_por_id_jugador[id_jugador] > gp_actual_carrera_puesto_por_id_jugador[mejor_clasificado_humano_jugador]) && (gp_actual_carrera_puesto_por_id_jugador[mejor_clasificado_humano_jugador] == PRIMER_LUGAR)) {
                switch (jugador->id_personaje) {
                    case DK:
                        if (es_punto_camino_en_rango(punto_camino_mas_cercano_por_id_jugador[id_jugador],
                                                    punto_camino_mas_cercano_por_id_jugador[mejor_clasificado_humano_jugador], 40, 2,
                                                    cantidad_camino_seleccionado) > 0) {
                            estrategia_cpu->rama = CPU_ESTRATEGIA_LANZAMIENTO_BANANA;
                        }
                        break;

                    case PEACH:
                        if (es_punto_camino_en_rango(punto_camino_mas_cercano_por_id_jugador[id_jugador],
                                                    punto_camino_mas_cercano_por_id_jugador[mejor_clasificado_humano_jugador], 4, 2,
                                                    cantidad_camino_seleccionado) > 0) {
                            estrategia_cpu->rama = CPU_ESTRATEGIA_LANZAMIENTO_BANANA;
                        }
                        break;

                    default:
                        if (es_punto_camino_en_rango(punto_camino_mas_cercano_por_id_jugador[id_jugador],
                                                    punto_camino_mas_cercano_por_id_jugador[mejor_clasificado_humano_jugador], 10, 2,
                                                    cantidad_camino_seleccionado) > 0) {
                            estrategia_cpu->rama = CPU_ESTRATEGIA_LANZAMIENTO_BANANA;
                        }
                        break;
                }
            } else if (estrategia_cpu->rama == CPU_ESTRATEGIA_ITEM_BANANA) {
                estrategia_cpu->indice_actor = usar_item_banana(jugador);
                if ((estrategia_cpu->indice_actor >= 0) && (estrategia_cpu->indice_actor < 100)) {
                    jugador->disparadores |= EFECTO_ITEM_ARRASTRE;
                    estrategia_cpu->rama = CPU_ESTRATEGIA_MANTENIDO_BANANA;
                    estrategia_cpu->temporizador = 0;
                    estrategia_cpu->usar_item_num += 1;
                    estrategia_cpu->tiempo_antes_lanzamiento = (int_aleatorio(3) * 20) + 10;
                } else {
                    estrategia_cpu->rama = CPU_ESTRATEGIA_ESPERA_SIGUIENTE_ITEM;
                    estrategia_cpu->temporizador = 0;
                }
            }
            break;

        case CPU_ESTRATEGIA_MANTENIDO_BANANA:
            actor = &lista_actor[estrategia_cpu->indice_actor];
            if ((!(BANANA_ACTOR(actor)->flags & 0x8000)) || (BANANA_ACTOR(actor)->type != ACTOR_BANANA) || (BANANA_ACTOR(actor)->state != BANANA_MANTENIDO) ||
                (id_jugador != BANANA_ACTOR(actor)->id_jugador)) {

                if (!(BANANA_ACTOR(actor)->flags & 0x8000)) {}
                if (BANANA_ACTOR(actor)->type != 6) {}
                if (BANANA_ACTOR(actor)->state != 0) {}
                if (BANANA_ACTOR(actor)->rot[0] != id_jugador) {}

                estrategia_cpu->rama = CPU_ESTRATEGIA_ESPERA_SIGUIENTE_ITEM;
                estrategia_cpu->temporizador = 0;
                jugador->disparadores &= ~EFECTO_ITEM_ARRASTRE;
            } else if (estrategia_cpu->tiempo_antes_lanzamiento < estrategia_cpu->temporizador) {
                estrategia_cpu->rama = CPU_ESTRATEGIA_CAIDA_BANANA;
            }
            break;

        case CPU_ESTRATEGIA_CAIDA_BANANA:
            actor = &lista_actor[estrategia_cpu->indice_actor];
            if ((((!(BANANA_ACTOR(actor)->flags & 0x8000)) || (BANANA_ACTOR(actor)->type != ACTOR_BANANA)) ||
                    (BANANA_ACTOR(actor)->state != BANANA_MANTENIDO)) ||
                (id_jugador != BANANA_ACTOR(actor)->id_jugador)) {

                if (!(BANANA_ACTOR(actor)->flags & 0x8000)) {}
                if (BANANA_ACTOR(actor)->type != 6) {}
                if (BANANA_ACTOR(actor)->state != 0) {}
                if (BANANA_ACTOR(actor)->rot[0] != id_jugador) {}

            } else {
                BANANA_ACTOR(actor)->state = BANANA_SOLTADO;
                BANANA_ACTOR(actor)->velocidad[0] = 0.0f;
                BANANA_ACTOR(actor)->velocidad[1] = 0.0f;
                BANANA_ACTOR(actor)->velocidad[2] = 0.0f;
                if (dato_801631E0[id_jugador] == true) {
                    BANANA_ACTOR(actor)->pos[1] =
                        obtener_altura_superficie(jugador->pos[0], jugador->pos[1] + 30.0, jugador->pos[2]) +
                        (BANANA_ACTOR(actor)->tamanio_caja_envolvente + 1.0f);
                }
            }
            jugador->disparadores &= ~EFECTO_ITEM_ARRASTRE;
            estrategia_cpu->temporizador = 0;
            estrategia_cpu->rama = CPU_ESTRATEGIA_ESPERA_SIGUIENTE_ITEM;
            break;

        case CPU_ESTRATEGIA_LANZAMIENTO_BANANA:
            estrategia_cpu->indice_actor = usar_item_banana(jugador);
            if ((estrategia_cpu->indice_actor >= 0) && (estrategia_cpu->indice_actor < 100)) {
                actor = &lista_actor[estrategia_cpu->indice_actor];
                BANANA_ACTOR(actor)->state = BANANA_EN_SUELO;
                jugador->disparadores |= EFECTO_ITEM_ARRASTRE;
                estrategia_cpu->rama = CPU_ESTRATEGIA_MANTENIDO_LANZAMIENTO_BANANA;
                estrategia_cpu->temporizador = 0;
                estrategia_cpu->usar_item_num += 1;
                punto_camino = &caminos_pista[indice_camino_por_id_jugador[0]]
                                        [(punto_camino_mas_cercano_por_id_jugador[mejor_clasificado_humano_jugador] + 30) %
                                            cantidad_camino_por_indice_camino[indice_camino_por_id_jugador[mejor_clasificado_humano_jugador]]];
                BANANA_ACTOR(actor)->velocidad[0] = (punto_camino->pos_x - jugador->pos[0]) / 20.0;
                BANANA_ACTOR(actor)->velocidad[1] = ((punto_camino->pos_y - jugador->pos[1]) / 20.0) + 4.0;
                BANANA_ACTOR(actor)->velocidad[2] = (punto_camino->pos_z - jugador->pos[2]) / 20.0;
                BANANA_ACTOR(actor)->pos[1] = jugador->pos[1];
                funcion_800C92CC(id_jugador, SONIDO_CARGA_PARAMETRO(0x29, 0x00, 0x80, 0x09));
                funcion_800C98B8(jugador->pos, jugador->velocidad, SONIDO_CARGA_PARAMETRO(0x19, 0x01, 0x80, 0x14));
            } else {
                estrategia_cpu->temporizador = 0;
                estrategia_cpu->rama = CPU_ESTRATEGIA_ESPERA_SIGUIENTE_ITEM;
            }
            break;

        case CPU_ESTRATEGIA_MANTENIDO_LANZAMIENTO_BANANA:
            actor = &lista_actor[estrategia_cpu->indice_actor];
            if ((((!(BANANA_ACTOR(actor)->flags & 0x8000)) || (BANANA_ACTOR(actor)->type != ACTOR_BANANA)) ||
                    (BANANA_ACTOR(actor)->state != BANANA_EN_SUELO)) ||
                (id_jugador != BANANA_ACTOR(actor)->id_jugador)) {

                if (!(BANANA_ACTOR(actor)->flags & 0x8000)) {}
                if (BANANA_ACTOR(actor)->type != 6) {}
                if (BANANA_ACTOR(actor)->state != 0) {}
                if (BANANA_ACTOR(actor)->rot[0] != id_jugador) {}

                estrategia_cpu->temporizador = 0;
                estrategia_cpu->rama = CPU_ESTRATEGIA_ESPERA_SIGUIENTE_ITEM;
                jugador->disparadores &= ~EFECTO_ITEM_ARRASTRE;
            } else {
                BANANA_ACTOR(actor)->velocidad[1] -= 0.4;
                BANANA_ACTOR(actor)->pos[0] += BANANA_ACTOR(actor)->velocidad[0];
                BANANA_ACTOR(actor)->pos[1] += BANANA_ACTOR(actor)->velocidad[1];
                BANANA_ACTOR(actor)->pos[2] += BANANA_ACTOR(actor)->velocidad[2];
                if (estrategia_cpu->temporizador > 20) {
                    estrategia_cpu->rama = CPU_ESTRATEGIA_FIN_LANZAMIENTO_BANANA;
                }
            }
            break;

        case CPU_ESTRATEGIA_FIN_LANZAMIENTO_BANANA:
            actor = &lista_actor[estrategia_cpu->indice_actor];
            if ((((!(BANANA_ACTOR(actor)->flags & 0x8000)) || (BANANA_ACTOR(actor)->type != ACTOR_BANANA)) ||
                    (BANANA_ACTOR(actor)->state != BANANA_EN_SUELO)) ||
                (id_jugador != BANANA_ACTOR(actor)->id_jugador)) {

                if (!(BANANA_ACTOR(actor)->flags & 0x8000)) {}
                if (BANANA_ACTOR(actor)->type != 6) {}
                if (BANANA_ACTOR(actor)->state != 0) {}
                if (BANANA_ACTOR(actor)->rot[0] != id_jugador) {}

            } else {
                BANANA_ACTOR(actor)->state = BANANA_SOLTADO;
                BANANA_ACTOR(actor)->velocidad[0] = 0.0f;
                BANANA_ACTOR(actor)->velocidad[1] = 0.0f;
                BANANA_ACTOR(actor)->velocidad[2] = 0.0f;
                BANANA_ACTOR(actor)->pos[1] =
                    obtener_altura_superficie(BANANA_ACTOR(actor)->pos[0], BANANA_ACTOR(actor)->pos[1] + 30.0, BANANA_ACTOR(actor)->pos[2]) +
                    (BANANA_ACTOR(actor)->tamanio_caja_envolvente + 1.0f);
            }
            jugador->disparadores &= ~EFECTO_ITEM_ARRASTRE;
            estrategia_cpu->rama = CPU_ESTRATEGIA_ESPERA_SIGUIENTE_ITEM;
            estrategia_cpu->temporizador = 0;
            break;

        case ITEM_ESTRATEGIA_CPU_CAPARAZON_VERDE:
            if (actores_num < 80) {
                estrategia_cpu->indice_actor = usar_caparazon_verde_item(jugador);
                if ((estrategia_cpu->indice_actor >= 0) && (estrategia_cpu->indice_actor < 100)) {
                    estrategia_cpu->rama = MANTENIDO_ESTRATEGIA_CPU_CAPARAZON_VERDE;
                    estrategia_cpu->temporizador = 0;
                    estrategia_cpu->usar_item_num += 1;
                    estrategia_cpu->tiempo_antes_lanzamiento = (int_aleatorio(3) * 20) + 10;
                } else {
                    estrategia_cpu->rama = CPU_ESTRATEGIA_ESPERA_SIGUIENTE_ITEM;
                }
            } else {
                estrategia_cpu->rama = CPU_ESTRATEGIA_ESPERA_SIGUIENTE_ITEM;
            }
            break;

        case MANTENIDO_ESTRATEGIA_CPU_CAPARAZON_VERDE:
            actor = &lista_actor[estrategia_cpu->indice_actor];
            if ((((!(actor->flags & 0x8000)) || (actor->type != ACTOR_CAPARAZON_VERDE)) ||
                    (actor->state != CAPARAZON_MANTENIDO)) ||
                (id_jugador != actor->rot[2])) {

                if (!(actor->flags & 0x8000)) {}
                if (actor->type != 7) {}
                if (actor->state != 0) {}
                if (actor->rot[0] != id_jugador) {}

                estrategia_cpu->rama = CPU_ESTRATEGIA_ESPERA_SIGUIENTE_ITEM;
                estrategia_cpu->temporizador = 0;
            } else if (estrategia_cpu->tiempo_antes_lanzamiento < estrategia_cpu->temporizador) {
                estrategia_cpu->rama = LANZAMIENTO_ESTRATEGIA_CPU_CAPARAZON_VERDE;
                estrategia_cpu->temporizador = 0;
            }
            break;

        case LANZAMIENTO_ESTRATEGIA_CPU_CAPARAZON_VERDE:
            actor = &lista_actor[estrategia_cpu->indice_actor];
            if ((((!(actor->flags & 0x8000)) || (actor->type != ACTOR_CAPARAZON_VERDE)) ||
                    (actor->state != CAPARAZON_MANTENIDO)) ||
                (id_jugador != actor->rot[2])) {

                if (!(actor->flags & 0x8000)) {}
                if (actor->type != 7) {}
                if (actor->state != 0) {}
                if (actor->rot[0] != id_jugador) {}

                estrategia_cpu->rama = CPU_ESTRATEGIA_ESPERA_SIGUIENTE_ITEM;
                estrategia_cpu->temporizador = 0;
            } else {
                actor->state = CAPARAZON_SOLTADO;
                estrategia_cpu->temporizador = 0;
                estrategia_cpu->rama = CPU_ESTRATEGIA_ESPERA_SIGUIENTE_ITEM;
            }
            break;

        case ITEM_ESTRATEGIA_CPU_CAPARAZON_ROJO:
            if (actores_num < 80) {
                estrategia_cpu->indice_actor = usar_caparazon_rojo_item(jugador);
                if ((estrategia_cpu->indice_actor >= 0) && (estrategia_cpu->indice_actor < 100)) {
                    estrategia_cpu->rama = MANTENIDO_ESTRATEGIA_CPU_CAPARAZON_ROJO;
                    estrategia_cpu->temporizador = 0;
                    estrategia_cpu->usar_item_num += 1;
                    estrategia_cpu->tiempo_antes_lanzamiento = (int_aleatorio(3) * 20) + 10;
                } else {
                    estrategia_cpu->rama = CPU_ESTRATEGIA_ESPERA_SIGUIENTE_ITEM;
                }
            } else {
                estrategia_cpu->rama = CPU_ESTRATEGIA_ESPERA_SIGUIENTE_ITEM;
            }
            break;

        case MANTENIDO_ESTRATEGIA_CPU_CAPARAZON_ROJO:
            actor = &lista_actor[estrategia_cpu->indice_actor];
            if ((((!(ACTOR_CAPARAZON(actor)->flags & 0x8000)) || (ACTOR_CAPARAZON(actor)->type != ACTOR_CAPARAZON_ROJO)) ||
                    (ACTOR_CAPARAZON(actor)->state != CAPARAZON_MANTENIDO)) ||
                (id_jugador != ACTOR_CAPARAZON(actor)->id_jugador)) {

                if (!(actor->flags & 0x8000)) {}
                if (actor->type != 8) {}
                if (actor->state != 0) {}
                if (actor->rot[0] != id_jugador) {}

                estrategia_cpu->rama = CPU_ESTRATEGIA_ESPERA_SIGUIENTE_ITEM;
                estrategia_cpu->temporizador = 0;
            } else if (estrategia_cpu->tiempo_antes_lanzamiento < estrategia_cpu->temporizador) {
                estrategia_cpu->rama = LANZAMIENTO_ESTRATEGIA_CPU_CAPARAZON_ROJO;
            }
            break;

        case LANZAMIENTO_ESTRATEGIA_CPU_CAPARAZON_ROJO:
            borrar_estrategias_vencido(estrategia_cpu);
            actor = &lista_actor[estrategia_cpu->indice_actor];
            if ((((!(ACTOR_CAPARAZON(actor)->flags & 0x8000)) || (ACTOR_CAPARAZON(actor)->type != ACTOR_CAPARAZON_ROJO)) ||
                    (ACTOR_CAPARAZON(actor)->state != CAPARAZON_MANTENIDO)) ||
                (id_jugador != ACTOR_CAPARAZON(actor)->id_jugador)) {

                if (!(actor->flags & 0x8000)) {}
                if (actor->type != 8) {}
                if (actor->state != 0) {}
                if (actor->rot[0] != id_jugador) {}

                estrategia_cpu->rama = CPU_ESTRATEGIA_ESPERA_SIGUIENTE_ITEM;
                estrategia_cpu->temporizador = 0;
            } else {
                ACTOR_CAPARAZON(actor)->state = CAPARAZON_SOLTADO;
                estrategia_cpu->temporizador = 0;
                estrategia_cpu->rama = CPU_ESTRATEGIA_ESPERA_SIGUIENTE_ITEM;
            }
            break;

        case CPU_ESTRATEGIA_ITEM_BANANA_GRUPO:
            if (actores_num < 80) {
                estrategia_cpu->indice_actor = usar_item_grupo_banana(jugador);
                if ((estrategia_cpu->indice_actor >= 0) && (estrategia_cpu->indice_actor < 100)) {
                    estrategia_cpu->rama = CPU_ESTRATEGIA_ESPERA_INICIALIZACION_BANANA_GRUPO;
                    estrategia_cpu->temporizador = 0;
                    estrategia_cpu->usar_item_num += 1;
                    estrategia_cpu->tiempo_antes_lanzamiento = (int_aleatorio(3) * 20) + 60;
                } else {
                    estrategia_cpu->rama = CPU_ESTRATEGIA_ESPERA_SIGUIENTE_ITEM;
                }
            } else {
                estrategia_cpu->rama = CPU_ESTRATEGIA_ESPERA_SIGUIENTE_ITEM;
            }
            break;

        case CPU_ESTRATEGIA_ESPERA_INICIALIZACION_BANANA_GRUPO:
            actor = &lista_actor[estrategia_cpu->indice_actor];
            if (ACTOR_GRUPO_BANANA(actor)->state == 6) {

                if (ACTOR_GRUPO_BANANA(actor)->state != -1) {}
                if (ACTOR_GRUPO_BANANA(actor)->state == 6) {}

                es_banana_valido_2 = false;

                if (ACTOR_GRUPO_BANANA(actor)->banana_indices[4] != (-1)) {
                    es_banana_valido_2 = true;
                }
                if (ACTOR_GRUPO_BANANA(actor)->banana_indices[3] != (-1)) {
                    es_banana_valido_2 = true;
                }
                if (ACTOR_GRUPO_BANANA(actor)->banana_indices[2] != (-1)) {
                    es_banana_valido_2 = true;
                }
                if (ACTOR_GRUPO_BANANA(actor)->banana_indices[1] != (-1)) {
                    es_banana_valido_2 = true;
                }
                if (ACTOR_GRUPO_BANANA(actor)->banana_indices[0] != (-1)) {
                    es_banana_valido_2 = true;
                }
                if ((ACTOR_GRUPO_BANANA(actor)->type != GRUPO_BANANA_ACTOR) || (es_banana_valido_2 == false)) {
                    if (ACTOR_GRUPO_BANANA(actor)->type != 14) {}
                    estrategia_cpu->rama = CPU_ESTRATEGIA_ESPERA_SIGUIENTE_ITEM;
                    estrategia_cpu->temporizador = 0;
                } else if (estrategia_cpu->tiempo_antes_lanzamiento < estrategia_cpu->temporizador) {
                    estrategia_cpu->rama = CPU_ESTRATEGIA_CAIDA_BANANA_GRUPO;
                    estrategia_cpu->num_soltado_banana_grupo = 0;
                    estrategia_cpu->temporizador = 0;
                }
            }
            break;

        case CPU_ESTRATEGIA_CAIDA_BANANA_GRUPO:
            if (((estrategia_cpu->temporizador) % 10) == 0) {
                if (estrategia_cpu->num_soltado_banana_grupo < 5) {
                    es_banana_valido_1 = 0;
                    actor = &lista_actor[estrategia_cpu->indice_actor];
                    switch (estrategia_cpu->num_soltado_banana_grupo) {
                        case 0:
                            if (ACTOR_GRUPO_BANANA(actor)->banana_indices[4] != (-1)) {
                                es_banana_valido_1 = true;
                            }
                            break;

                        case 1:
                            if (ACTOR_GRUPO_BANANA(actor)->banana_indices[3] != (-1)) {
                                es_banana_valido_1 = true;
                            }
                            break;

                        case 2:
                            if (ACTOR_GRUPO_BANANA(actor)->banana_indices[2] != (-1)) {
                                es_banana_valido_1 = true;
                            }
                            break;

                        case 3:
                            if (ACTOR_GRUPO_BANANA(actor)->banana_indices[1] != (-1)) {
                                es_banana_valido_1 = true;
                            }
                            break;

                        case 4:
                            if (ACTOR_GRUPO_BANANA(actor)->banana_indices[0] != (-1)) {
                                es_banana_valido_1 = true;
                            }
                            break;
                    }

                    if (((ACTOR_GRUPO_BANANA(actor)->type == GRUPO_BANANA_ACTOR) && (ACTOR_GRUPO_BANANA(actor)->state == 6)) &&
                        (es_banana_valido_1 == true)) {
                        soltar_banana_en_grupo_banana((struct PadreGrupoBanana*)actor);
                    }
                    estrategia_cpu->num_soltado_banana_grupo += 1;
                } else {
                    estrategia_cpu->rama = CPU_ESTRATEGIA_ESPERA_SIGUIENTE_ITEM;
                    estrategia_cpu->temporizador = 0;
                }
            }
            break;

        case ITEM_ESTRATEGIA_CPU_CAJA_ITEM_FALSA:
            estrategia_cpu->indice_actor = usar_item_caja_item_falso(jugador);
            if ((estrategia_cpu->indice_actor >= 0) && (estrategia_cpu->indice_actor < 100)) {
                estrategia_cpu->rama = MANTENIDO_ESTRATEGIA_CPU_CAJA_ITEM_FALSA;
                estrategia_cpu->temporizador = 0;
                estrategia_cpu->usar_item_num += 1;
                estrategia_cpu->tiempo_antes_lanzamiento = (int_aleatorio(3) * 20) + 10;
            } else {
                estrategia_cpu->rama = CPU_ESTRATEGIA_ESPERA_SIGUIENTE_ITEM;
            }
            break;

        case MANTENIDO_ESTRATEGIA_CPU_CAJA_ITEM_FALSA:
            actor = &lista_actor[estrategia_cpu->indice_actor];
            if ((((!(ACTOR_CAJA_ITEM_FALSO(actor)->flags & 0x8000)) || (ACTOR_CAJA_ITEM_FALSO(actor)->type != ACTOR_CAJA_ITEM_FALSA)) ||
                    (ACTOR_CAJA_ITEM_FALSO(actor)->state != 0)) ||
                (id_jugador != ((s32) ACTOR_CAJA_ITEM_FALSO(actor)->id_jugador))) {

                if (!(actor->flags & 0x8000)) {}
                if (actor->type != 13) {}
                if (actor->state != 0) {}
                if (actor->rot[0] != id_jugador) {}

                estrategia_cpu->rama = CPU_ESTRATEGIA_ESPERA_SIGUIENTE_ITEM;
                estrategia_cpu->temporizador = 0;
            } else if (estrategia_cpu->tiempo_antes_lanzamiento < estrategia_cpu->temporizador) {
                estrategia_cpu->rama = LANZAMIENTO_ESTRATEGIA_CPU_CAJA_ITEM_FALSA;
            }
            break;

        case LANZAMIENTO_ESTRATEGIA_CPU_CAJA_ITEM_FALSA:
            actor = &lista_actor[estrategia_cpu->indice_actor];
            if ((((!(ACTOR_CAJA_ITEM_FALSO(actor)->flags & 0x8000)) || (ACTOR_CAJA_ITEM_FALSO(actor)->type != ACTOR_CAJA_ITEM_FALSA)) ||
                    (ACTOR_CAJA_ITEM_FALSO(actor)->state != 0)) ||
                (id_jugador != ((s32) ACTOR_CAJA_ITEM_FALSO(actor)->id_jugador))) {

                if (!(ACTOR_CAJA_ITEM_FALSO(actor)->flags & 0x8000)) {}
                if (ACTOR_CAJA_ITEM_FALSO(actor)->type != 13) {}
                if (ACTOR_CAJA_ITEM_FALSO(actor)->state != 0) {}
                if (ACTOR_CAJA_ITEM_FALSO(actor)->rot[0] != id_jugador) {}

            } else {
                funcion_802A1064((struct CajaItemFalsa*)actor);
                if (dato_801631E0[id_jugador] == true) {
                    ACTOR_CAJA_ITEM_FALSO(actor)->pos[1] =
                        obtener_altura_superficie(ACTOR_CAJA_ITEM_FALSO(actor)->pos[0], ACTOR_CAJA_ITEM_FALSO(actor)->pos[1] + 30.0, ACTOR_CAJA_ITEM_FALSO(actor)->pos[2]) +
                        ACTOR_CAJA_ITEM_FALSO(actor)->tamanio_caja_envolvente;
                }
            }
            estrategia_cpu->rama = CPU_ESTRATEGIA_ESPERA_SIGUIENTE_ITEM;
            estrategia_cpu->temporizador = 0;
            break;

        case CPU_ESTRATEGIA_ITEM_RAYO:
            usar_item_trueno(jugador);
            funcion_800CAC60(id_jugador);
            funcion_8009E5BC();
            estrategia_cpu->rama = CPU_ESTRATEGIA_FIN_RAYO;
            estrategia_cpu->temporizador = 0;
            estrategia_cpu->usar_item_num += 1;
            break;

        case CPU_ESTRATEGIA_FIN_RAYO:
            if (estrategia_cpu->temporizador >= 0xF1) {
                funcion_800CAD40((s32) ((u8) id_jugador));
                estrategia_cpu->rama = CPU_ESTRATEGIA_ESPERA_SIGUIENTE_ITEM;
                estrategia_cpu->temporizador = 0;
            }
            break;

        case CPU_ESTRATEGIA_ITEM_ESTRELLA:
            jugador->disparadores |= DISPARADOR_ESTRELLA;
            estrategia_cpu->rama = CPU_ESTRATEGIA_FIN_ITEM_ESTRELLA;
            estrategia_cpu->temporizador = 0;
            estrategia_cpu->usar_item_num += 1;
            break;

        case CPU_ESTRATEGIA_FIN_ITEM_ESTRELLA:
            if (!(jugador->efectos & EFECTO_ESTRELLA)) {
                estrategia_cpu->rama = CPU_ESTRATEGIA_ESPERA_SIGUIENTE_ITEM;
            }
            estrategia_cpu->temporizador = 0;
            break;

        case ITEM_ESTRATEGIA_CPU_BOO:
            jugador->disparadores |= BOO_DISPARADOR;
            estrategia_cpu->rama = CPU_ESTRATEGIA_ESPERA_FIN_BOO;
            estrategia_cpu->temporizador = 0;
            estrategia_cpu->usar_item_num += 1;
            break;

        case CPU_ESTRATEGIA_ESPERA_FIN_BOO:
            if (!(jugador->efectos & BOO_EFECTO)) {
                estrategia_cpu->rama = CPU_ESTRATEGIA_ESPERA_SIGUIENTE_ITEM;
            }
            estrategia_cpu->temporizador = 0;
            break;

        case CPU_ESTRATEGIA_ITEM_HONGO:
            jugador->disparadores |= DISPARADOR_HONGO;
            estrategia_cpu->rama = CPU_ESTRATEGIA_ESPERA_SIGUIENTE_ITEM;
            estrategia_cpu->temporizador = 0;
            estrategia_cpu->usar_item_num += 1;
            break;

        case CPU_ESTRATEGIA_ITEM_DOBLE_HONGO:
            if (estrategia_cpu->temporizador >= 0x3D) {
                jugador->disparadores |= DISPARADOR_HONGO;
                estrategia_cpu->rama = CPU_ESTRATEGIA_ITEM_HONGO;
                estrategia_cpu->temporizador = 0;
            }
            break;

        case ITEM_ESTRATEGIA_CPU_TRIPLE_HONGO:
            if (estrategia_cpu->temporizador >= 0x3D) {
                jugador->disparadores |= DISPARADOR_HONGO;
                estrategia_cpu->rama = CPU_ESTRATEGIA_ITEM_DOBLE_HONGO;
                estrategia_cpu->temporizador = 0;
            }
            break;

        case ITEM_ESTRATEGIA_CPU_SUPER_HONGO:
            estrategia_cpu->rama = USAR_ESTRATEGIA_CPU_SUPER_HONGO;
            estrategia_cpu->temporizador = 0;
            estrategia_cpu->tiempo_antes_lanzamiento = 0x0258;
            break;

        case USAR_ESTRATEGIA_CPU_SUPER_HONGO:
            if ((((s16) estrategia_cpu->temporizador) % 60) == 0) {
                jugador->disparadores |= DISPARADOR_HONGO;
                if (estrategia_cpu->tiempo_antes_lanzamiento < estrategia_cpu->temporizador) {
                    estrategia_cpu->temporizador = 0;
                    estrategia_cpu->rama = CPU_ESTRATEGIA_ESPERA_SIGUIENTE_ITEM;
                }
            }
            break;

        default:
            break;
    }

    if (estrategia_cpu->temporizador < 10000) {
        estrategia_cpu->temporizador += 1;
    }
    if (jugador->efectos & (BOO_EFECTO | EFECTO_HONGO | EFECTO_ESTRELLA)) {
        estrategia_cpu->temporizador = 0;
    }
}

#undef BANANA_ACTOR
#undef ACTOR_CAPARAZON
#undef ACTOR_CAJA_ITEM_FALSO
#undef GRUPO_BANANA

void funcion_8001BE78(void) {
    Jugador* temporal_s1;
    PuntoCaminoPista* temporal_s0;
    s32 i;

    inicializar_jugadores();
    for (i = 0; i < 4; i++) {
        temporal_s1 = &jugador_uno[i];
        temporal_s1->type &= 0xDFFF;
        indice_camino_por_id_jugador[i] = i;
        jugador_pista_posicion_factor_instruccion[i].desconocido_c = 0.0f;
        jugador_pista_posicion_factor_instruccion[i].target = 0.0f;
        jugador_pista_posicion_factor_instruccion[i].current = 0.0f;
        switch (i) {
            case 0:
                punto_camino_mas_cercano_por_id_jugador[i] = 6;
                break;
            case 1:
                punto_camino_mas_cercano_por_id_jugador[i] = 1;
                break;
            case 2:
                punto_camino_mas_cercano_por_id_jugador[i] = 6;
                break;
            case 3:
                punto_camino_mas_cercano_por_id_jugador[i] = 1;
                break;
        }
        temporal_s0 = &caminos_pista[i][punto_camino_mas_cercano_por_id_jugador[i]];
        temporal_s1->pos[0] = (f32) temporal_s0->pos_x;
        temporal_s1->pos[1] =
            obtener_altura_superficie((f32) temporal_s0->pos_x, 2000.0f, (f32) temporal_s0->pos_z) + temporal_s1->tamanio_caja_envolvente;
        temporal_s1->pos[2] = (f32) temporal_s0->pos_z;
        temporal_s1->rotacion[1] = (s16) *rotacion_esperado_camino[i];
        aplicar_giro_cpu(temporal_s1, 0);
        temporal_s1++;
        dato_80163410[i] = 0;
    }
}

#ifdef TARGET_PS2
static s16 espera_podio_total[4];
static s16 espera_podio_cerca[4];

static void asegurar_llegada_podio(s32 id_jugador, Jugador* jugador) {
    f32 objetivo_x = dato_80163418[id_jugador];
    f32 objetivo_z = dato_80163438[id_jugador];
    f32 altura;

    if (jugador->type & SECUENCIA_INICIO_JUGADOR) {
        return;
    }
    if (espera_podio_total[id_jugador] < 0x7FFF) {
        espera_podio_total[id_jugador]++;
    }
    if ((dato_80163410[id_jugador] >= 3) && (espera_podio_cerca[id_jugador] < 0x7FFF)) {
        espera_podio_cerca[id_jugador]++;
    }
    altura = obtener_altura_superficie(objetivo_x, 2000.0f, objetivo_z);
    if ((jugador->pos[1] > (altura - 100.0f)) && (espera_podio_total[id_jugador] < 1800) &&
        (espera_podio_cerca[id_jugador] < ((dato_80163410[id_jugador] >= 4) ? 90 : 450))) {
        return;
    }
    jugador->pos[0] = objetivo_x;
    jugador->pos[1] = altura + jugador->tamanio_caja_envolvente;
    jugador->pos[2] = objetivo_z;
    jugador->pos_viejo[0] = jugador->pos[0];
    jugador->pos_viejo[1] = jugador->pos[1];
    jugador->pos_viejo[2] = jugador->pos[2];
    jugador->velocidad[0] = 0.0f;
    jugador->velocidad[1] = 0.0f;
    jugador->velocidad[2] = 0.0f;
    jugador->speed = 0.0f;
    jugador->actual_rapidez = 0.0f;
    jugador->velocidad_salto_kart = 0.0f;
    jugador->desconocido_08C = 0.0f;
    jugador->efectos &= ~EFECTO_EN_EL_AIRE;
    dato_80163410[id_jugador] = 4;
}
#endif

void funcion_8001C05C(void) {
    inicializar_carrera_segmento();
    id_circuito_actual = CEREMONIA_PREMIO_CIRCUITO;
    dato_8016347C = 0;
    dato_8016347E = 0;
    dato_80163480 = 0;
    dato_80163484 = 0;
#ifdef TARGET_PS2
    {
        s32 i;

        for (i = 0; i < 4; i++) {
            espera_podio_total[i] = 0;
            espera_podio_cerca[i] = 0;
        }
    }
#endif
    inicializar_punto_camino_circuito();
    funcion_80014DE4(0);
    funcion_8001BE78();
    dato_80163418[0] = -3202.475097656f;
    dato_80163428[0] = 19.166999817f;
    dato_80163438[0] = -477.623992920f;
    dato_80163418[1] = -3205.080078125f;
    dato_80163428[1] = 19.166999817f;
    dato_80163438[1] = -462.851989746f;
    dato_80163418[2] = -3199.870117188f;
    dato_80163428[2] = 19.166999817f;
    dato_80163438[2] = -492.395996094f;
    dato_80163418[3] = -2409.197021484f;
    dato_80163428[3] = 0.0f;
    dato_80163438[3] = -355.253997803;
}

void funcion_8001C14C(void) {
    f32 temporal_f0;
    f32 temporal_f2;
    s32 id_jugador;
    Jugador* temporal_s0;

    if (dato_8016347C == 1) {
        dato_80163480 += 1;
    }
    if ((dato_8016347E == 1) && (karts_bomba[0].state == 0) && (dato_802874D8.desconocido_1d >= 3)) {
        dato_80163484++;
        if (dato_80163484 >= 0xF) {
            dato_80163484 = 0;
            dato_8016347E = 2;
            funcion_8009265C();
        }
    }
    for (id_jugador = 0; id_jugador < 4; id_jugador++) {
        if ((id_jugador == 3) && (dato_8016347C == 0)) {
            break;
        }

        temporal_s0 = &jugador_uno[id_jugador];
        actualizar_jugador(id_jugador);
#ifdef TARGET_PS2
        asegurar_llegada_podio(id_jugador, temporal_s0);
#endif
        if (!(temporal_s0->type & SECUENCIA_INICIO_JUGADOR)) {
            temporal_f0 = dato_80163418[id_jugador] - temporal_s0->pos[0];
            temporal_f2 = dato_80163438[id_jugador] - temporal_s0->pos[2];
            if ((f64) ((temporal_f0 * temporal_f0) + (temporal_f2 * temporal_f2)) < 1.0) {
                if (id_jugador != 3) {
                    if (1) {}
                    (dato_8016347C == 0) ? (temporal_s0->type |= SECUENCIA_INICIO_JUGADOR)
                                      : (temporal_s0->type &= ~SECUENCIA_INICIO_JUGADOR);
                    if ((jugador_uno->type & SECUENCIA_INICIO_JUGADOR) && (jugador_dos->type & SECUENCIA_INICIO_JUGADOR) &&
                        (jugador_tres->type & SECUENCIA_INICIO_JUGADOR)) {
                        dato_8016347C = 1;
                        dato_80163480 = 0;
                    }
                } else if (dato_8016347E == 0) {
                    if (!(temporal_s0->efectos & EFECTO_ERROR_EXPLOSION)) {
                        temporal_s0->type |= SECUENCIA_INICIO_JUGADOR;
                    }
                    dato_8016347E = 1;
                    dato_80163484 = 0;
                } else if (!(temporal_s0->efectos & EFECTO_ERROR_EXPLOSION)) {
                    temporal_s0->type |= SECUENCIA_INICIO_JUGADOR;
                }
            }
        }
    }
}

void renderizar_karts_bomba_envoltura(s32 id_camara) {
    if (id_circuito_actual == CEREMONIA_PREMIO_CIRCUITO) {
        if (karts_bomba[0].indice_punto_camino >= 16) {
            funcion_80057114(JUGADOR_CUATRO);
        }
    } else {
        if (seleccion_modo == VERSUS) {
            funcion_80057114(id_camara);
        }
    }
}

SIN_USO void funcion_8001C42C(void) {
    if (dato_800DDB20 == 0) {
        if ((mando_tres->boton_pulsado & L_TRIG) != 0) {
            dato_800DDB20 = 1;
        }
    } else {
        if ((mando_tres->boton_pulsado & L_TRIG) != 0) {
            dato_800DDB20 = 0;
        }
        funcion_80057C60();
        gSPDisplayList(display_list_cabeza++, dato_0D0076F8);
        funcion_80057CE4();
    }
}
