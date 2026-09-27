#include <ultra64.h>
#include <juego/macros.h>
#include <juego/definiciones.h>
#include <depuracion/depuracion_juego.h>
#include <PR/gu.h>
#include <juego/mk64.h>
#include <juego/pista.h>

#include "sistema/bucle_principal.h"
#include <juego/segmentos.h>
#include "carrera/preparacion_carrera.h"
#include "carrera/camara.h"
#include "memoria/memoria_carrera.h"
#include "sistema/matematicas.h"
#include "ceremonia/bucle_creditos.h"
#include "ceremonia/carga_ceremonia.h"
#include "graficos/cielo_y_pantalla_dividida.h"
#include "menus/elementos_menu.h"
#include "menus/menus.h"
#include "carrera/inicio_hud_y_objetos.h"
#include "carrera/preparacion_carrera.h"
#include "ceremonia/ceremonia_y_creditos.h"
#include "ceremonia/actores_podio.h"
#include "ceremonia/dibujar_podio.h"
#include "carrera/objetos_y_efectos.h"
#include "carrera/actores.h"
#include "graficos/dibujar_pistas.h"
#include "sistema/bucle_principal.h"
#include "graficos/dibujar_jugador.h"

s32 dato_802874A0;

void funcion_80280000(void) {
    actualizar_agua_circuito();
    funcion_80059AC8();
    funcion_80059AC8();
    funcion_8005A070();
}

void funcion_80280038(void) {
    u16 norma_persp;
    Camara* camara = &camaras[0];
    SIN_USO s32 relleno;
    Mat4 matriz;

    cantidad_objeto_matriz = 0;
    cantidad_efecto_matriz = 0;
    cantidad_hud_matriz = 0;
    inicializar_rdp();
    funcion_802A53A4();
    inicializar_rdp();
    funcion_80057FC4(0);

    gSPSetGeometryMode(display_list_cabeza++, G_ZBUFFER | G_SHADE | G_CULL_BACK | G_SHADING_SMOOTH);
    guPerspective(&gfx_pool->mtx_persp[0], &norma_persp, acercar_camara[0], aspecto_pantalla, circuito_cerca_persp, persp_lejos_circuito,
                  1.0f);
    gSPPerspNormalize(display_list_cabeza++, norma_persp);
    gSPMatrix(display_list_cabeza++, VIRTUAL_A_FISICO(&gfx_pool->mtx_persp[0]),
              G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
    guLookAt(&gfx_pool->mtx_mirar_a[0], camara->pos[0], camara->pos[1], camara->pos[2], camara->mirar_a[0],
             camara->mirar_a[1], camara->mirar_a[2], camara->arriba[0], camara->arriba[1], camara->arriba[2]);
    gSPMatrix(display_list_cabeza++, VIRTUAL_A_FISICO(&gfx_pool->mtx_mirar_a[0]),
              G_MTX_NOPUSH | G_MTX_MUL | G_MTX_PROJECTION);
    id_circuito_actual = id_circuito_creditos;
    identidad_mtxf(matriz);
    fijar_posicion_render(matriz, 0);
    renderizar_circuito(dato_800DC5EC);
    renderizar_actores_circuito(dato_800DC5EC);
    renderizar_objeto(JUGADOR_UNO + MODO_PANTALLA_1P);
    renderizar_efecto_nieve_jugador(JUGADOR_UNO + MODO_PANTALLA_1P);
    ceremonia_transicion_deslizando_bordes();
    funcion_80281C40();
    inicializar_rdp();
    funcion_80093F10();
    inicializar_rdp();
}

void funcion_80280268(s32 parametro0) {
    es_en_abandonar_a_transicion_menu = 1;
    abandonar_a_contador_transicion_menu = 5;
    dato_802874A0 = 1;
    if ((parametro0 < 0) || ((parametro0 >= 20))) {
        parametro0 = 0;
    }
    id_circuito_creditos = parametro0;
}

void bucle_creditos(void) {
    Camara* camara = &camaras[0];

    f32 temporal_f12;
    f32 temporal_;
    f32 temporal_f14;

    dato_802874A0 = 0;
    if (es_en_abandonar_a_transicion_menu) {
        abandonar_a_contador_transicion_menu--;
        if (abandonar_a_contador_transicion_menu == 0) {
            es_en_abandonar_a_transicion_menu = 0;
            siguiente_estado_juego = SECUENCIA_CREDITOS;
            estado_juego = 255;
        }
    } else {
#ifdef TARGET_PS2
        if ((dato_80286A04[dato_800DC5E4].desconocido0 == 2) && (mando_uno->boton_pulsado & (START_BUTTON | A_BUTTON))) {
            dato_800DC5E4 = 0;
            seleccion_menu = MENU_INICIO;
            siguiente_estado_juego = MENU_INICIO_DESDE_ABANDONAR;
            return;
        }
#endif

        dato_802874FC = 0;
        funcion_80283648(camara);
        temporal_f12 = camara->mirar_a[0] - camara->pos[0];
        temporal_ = camara->mirar_a[1] - camara->pos[1];
        temporal_f14 = camara->mirar_a[2] - camara->pos[2];
        camara->rot[1] = atan2s(temporal_f12, temporal_f14);
        camara->rot[0] = atan2s(sqrtf((temporal_f12 * temporal_f12) + (temporal_f14 * temporal_f14)), temporal_);
        camara->rot[2] = 0;
        if (dato_802874A0 != 0) {
            dato_800DC5E4++;
        } else {
            funcion_80280000();
            funcion_80280038();
#if DVDL
            mostrar_dvdl();
#endif
            gDPFullSync(display_list_cabeza++);
            gSPEndDisplayList(display_list_cabeza++);
        }
    }
}

void cargar_creditos(void) {
    Camara* camara = &camaras[0];

    id_circuito_actual = id_circuito_creditos;
    dato_800DC5B4 = 1;
    modo_render_creditos = 1;
    fijar_perspectiva_y_proporcion_aspecto();
    funcion_802A74BC();
    camara->desconocido_B4 = 60.0f;
    acercar_camara[0] = 60.0f;
    dato_800DC5EC->ancho_pantalla = ANCHO_PANTALLA;
    dato_800DC5EC->altura_pantalla = ALTURA_PANTALLA;
    dato_800DC5EC->inicio_x_pantalla = 160;
    dato_800DC5EC->inicio_y_pantalla = 120;
    seleccion_modo_pantalla = MODO_PANTALLA_1P;
    modo_pantalla_activo = MODO_PANTALLA_1P;
    siguiente_libre_memoria_direccion = libre_memoria_reinicio_ancla;
    cargar_circuito(id_circuito_actual);
    dato_8015F730 = siguiente_libre_memoria_direccion;
    fijar_direccion_base_segmento(0xB, (void*) descomprimir_segmentos((u8*) CEREMONIA_DATOS_ROM_INICIO, (u8*) CEREMONIA_DATOS_ROM_FIN));

    min_x_circuito = -0x15A1;
    min_y_circuito = -0x15A1;
    min_z_circuito = -0x15A1;

    max_x_circuito = 0x15A1;
    max_y_circuito = 0x15A1;
    max_z_circuito = 0x15A1;
    dato_8015F59C = 0;
    dato_8015F5A0 = 0;
    dato_8015F58C = 0;
    cantidad_malla_colision = 0;
    dato_800DC5BC = 0;
    dato_800DC5C8 = 0;
    malla_colision = (TrianguloColision*) siguiente_libre_memoria_direccion;
    camara->pos[0] = 1400.0f;
    camara->pos[1] = 300.0f;
    camara->pos[2] = 1400.0f;
    camara->mirar_a[0] = 0.0f;
    camara->mirar_a[1] = 0.0f;
    camara->mirar_a[2] = 0.0f;
    camara->arriba[0] = 0.0f;
    camara->arriba[1] = 1.0f;
    camara->arriba[2] = 0.0f;
    inicializar_camara_cinematica();
    funcion_80003040();
    inicializar_hud();
    funcion_80093E60();
    funcion_80092688();
    if (dato_800DC5EC) {}
    dato_801625F8 = ((s32) ptr_fin_monton - siguiente_libre_memoria_direccion);
    dato_801625FC = ((f32) dato_801625F8 / 1000.0f);
}
