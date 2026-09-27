#ifndef CEREMONIA_CEREMONIA_Y_CREDITOS_H
#define CEREMONIA_CEREMONIA_Y_CREDITOS_H

#include <juego/estructuras_comunes.h>
#include "carrera/camara.h"

struct struct_80283431 {
    Vec3f desconocido0;
    Vec3s desconocido_c;
};

struct struct_80283430 {
    s8 desconocido0;
    s8 desconocido1;
    u16 desconocido2;
    s8 desconocido4;
    s8 desconocido5;
    Vec3s desconocido6;
};

struct creditos_datos_1FA0 {
    Vec3f desconocido0;
    s8 pad[0x10];
    s8 desconocido_1c;
    s8 desconocido_1d;
    s16 desconocido_1e;
    f32 unk20;
    s8 pad2[0x24];
    s16 desconocido48[3];
    s16 desconocido_4e[2];
    s16 un52;
    s16 desconocido54[2];
    s16 desconocido58;
    s16 desconocido_5a[2];
    s16 desconocido_5e;
    s16 unk60;
    s16 desconocido62;
    f32 desconocido64;
    f32 desconocido68;
    s16 desconocido_6c;
    s16 desconocido_6e;
};

typedef struct {
     Vec3f pos;
     Vec3f mirar_a;
     f32 unk18;
     u8 cinematica;
     f32 unk20;
     Vec3f unk24;
     Vec3f desconocido30;
     Vec3f desconocido_3c;
     Vec3s desconocido48;
     Vec3s desconocido_4e;
     Vec3s desconocido54;
     Vec3s desconocido_5a;
     s16 unk60;
     s16 desconocido62;
     f32 desconocido64;
     f32 desconocido68;
     s16 desconocido_6c;
     s16 desconocido_6e;
} CamaraCinematica;

struct struct_80282C40 {
    s8 desconocido0;
    s8 desconocido1;
    s8 desconocido2;
    u8 desconocido3;
    s8 desconocido4;
    s8 desconocido5;
    Vec3s desconocido6;
};

struct struct_80285D80 {
    u8 desconocido0[6];
    Vec3s desconocido6;
};

struct struct_80286A04 {
    u8 desconocido0;
    u8 desconocido1;
    struct struct_80285D80* desconocido4;
    struct struct_80285D80* desconocido8;
    u16 desconocido_c;
};

extern struct struct_80286A04 dato_80286A04[];

struct PuntoSplineCinematica {
    s8 index;
    u8 speed;
    Vec3s punto;
};

struct Cinematica {
     void (*disparo)(CamaraCinematica*);
     s16 duration;
};

typedef void (*EventoCamara)(CamaraCinematica* c);
typedef EventoCamara DisparoCinematica;

void inicializar_camara_cinematica(void);
s32 funcion_80283648(Camara*);
void fijar_duplicado_vec3f(Vec3f, f32, f32, f32);
void fijar_duplicado_vec3s(Vec3s, s16, s16, s16);
void borrar_vec3f(Vec3f);
void borrar_vec3s(Vec3s);
void copiar_duplicado_retorno_vec3f(Vec3f, Vec3f);
void copiar_duplicado_vec3s(Vec3s, Vec3s);
void funcion_80282040(void);
void funcion_80282048(void);
void rotar_eje_y_vec3f(Vec3f, Vec3f, s16);
void rotar_vec3f_x(Vec3f, Vec3f, s16);
s32 interpolacion_f32(f32*, f32, f32);
s32 suavizar_transicion_salida(s16*, s16, s16);
s32 ajustar_transicion_valor_f32(f32*, f32, f32);
s32 ajustar_transicion_valor_s16(s16*, s16, s16);
void reiniciar_spline(void);
void reiniciar_envoltura_spline(CamaraCinematica*);
void calcular_angulo_y_distancia_y_angulo_y_a_xz(Vec3f, Vec3f, f32*, s16*, s16*);
void aplicar_angulo_y_distancia_y_angulo_y_a_xz(Vec3f, Vec3f, f32, s16, s16);
void funcion_cinematica_aborting(Vec3f, Vec3f, Vec3f, Vec3s);
void evaluar_spline_cubico(f32, Vec3f, f32*, f32[], f32[], f32[], f32[]);
s32 mover_punto_junto_spline(Vec3f, f32*, struct struct_80283430[], s16*, f32*);
void funcion_80282BE4(struct struct_80283430*, s8, u8, s8, Vec3s, s32);
void funcion_80282C40(struct struct_80283430*, struct struct_80282C40*, s32);
s32 mover_camara_cinematica_junto_spline(CamaraCinematica*, struct struct_80286A04*, struct struct_80286A04*, s32);
void funcion_80282E58(CamaraCinematica*, struct struct_80282C40*, s32);
void funcion_80282EAC(s32, CamaraCinematica*, s16, s16, s16);
void funcion_80282F00(s16*, s16);
void funcion_80282F44(s32, CamaraCinematica*, Camara*);
void funcion_802830B4(CamaraCinematica*, s16, s16, s16);
void funcion_80283100(CamaraCinematica*, f32*);
void funcion_80283240(s16);
s32 evento_cinematica(EventoCamara event, CamaraCinematica*, s16, s16);
s32 funcion_80283330(s32);
s32 funcion_8028336C(CamaraCinematica*, Camara*);
s32 stub_cinematica(void);
void envoltura_func_8028100C(CamaraCinematica*);
void envoltura_func_80280FFC(CamaraCinematica*);
void animacion_aparece_deslizando_bordes(CamaraCinematica*);
void animacion_desaparece_deslizando_bordes(CamaraCinematica*);
void envoltura_func_80092C80(CamaraCinematica*);
void reproducir_bienvenida_sonido(CamaraCinematica*);
void envoltura_func_800CA0CC(CamaraCinematica*);
void reproducir_felicitacion_sonido(CamaraCinematica*);
void sacar_globo_sonido_juego(CamaraCinematica*);
void reproducir_pez_sonido(CamaraCinematica*);
void reproducir_pez_sonido_2(CamaraCinematica*);
void reproducir_trofeo_disparo_sonido(CamaraCinematica*);
void reproducir_podio_sonido(CamaraCinematica*);
void reproducir_trofeo_sonido(CamaraCinematica*);
void funcion_80283A54(CamaraCinematica*);
void funcion_80283A7C(CamaraCinematica*);
void funcion_80283B6C(CamaraCinematica*);
void funcion_80283BA4(CamaraCinematica*);
void reproducir_ganador_ceremonia_secuencia_parte1(CamaraCinematica*);
void reproducir_ganador_ceremonia_secuencia_parte2(CamaraCinematica*);
void envoltura_func_800CB134(CamaraCinematica*);
void reproducir_secuencia_ceremonia_perdiendo(CamaraCinematica*);
void reproducir_ganador_ceremonia_creditos_secuencia(CamaraCinematica*);
void funcion_80283CA8(CamaraCinematica*);
void funcion_80283CD0(CamaraCinematica*);
void reproducir_despedida_sonido(CamaraCinematica*);
void funcion_80283D2C(CamaraCinematica*);
void funcion_80283EA0(CamaraCinematica*);
void copiar_jugador_dos_en_camara(CamaraCinematica*);
void interpolar_jugador_dos_en_camara(CamaraCinematica*);
void funcion_80283F6C(CamaraCinematica*);
void copiar_jugador_tres_en_camara(CamaraCinematica*);
void interpolar_jugador_tres_en_camara(CamaraCinematica*);
void funcion_80284068(CamaraCinematica*);
void funcion_802840C8(CamaraCinematica*);
void funcion_80284154(CamaraCinematica*);
void funcion_80284184(CamaraCinematica*);
void funcion_802841E8(CamaraCinematica*);
void funcion_8028422C(CamaraCinematica*);
void funcion_802842A8(CamaraCinematica*);
void funcion_802842D8(CamaraCinematica*);
void funcion_80284308(CamaraCinematica*);
void funcion_80284418(CamaraCinematica*);
void funcion_80284494(CamaraCinematica*);
void funcion_802844FC(CamaraCinematica*);
void funcion_8028454C(CamaraCinematica*);
void funcion_802845EC(CamaraCinematica*);
void funcion_8028461C(CamaraCinematica*);
void funcion_80284648(CamaraCinematica*);
void funcion_802846AC(void);
void funcion_802846B4(CamaraCinematica*);
void funcion_802846E4(CamaraCinematica*);
void funcion_802847CC(CamaraCinematica*);
void reproducir_cinematica(CamaraCinematica*);
void ceremonia_transicion_deslizando_bordes(void);

extern s32 dato_80283FCC;
extern s32 dato_80283FF4;
extern f32 dato_802856B0;
extern f32 dato_802856B4;
extern f32 ordenado_tamanio_deslizando_bordes;
extern f32 dato_802856BC;
extern f32 bordes_deslizando_tamanio;
extern s32 dato_802856C4;
extern s32 dato_802856C8[];
extern s16 disparo_cinematica;
extern s16 temporizador_disparo_cinematica;
extern s32 dato_802876D4;
extern s32 dato_802876D8;
extern s32 dato_802876DC;
extern CamaraCinematica dato_802876E0;
extern struct struct_80283431 dato_80287750[];
extern struct struct_80283430 dato_80287818[];
extern struct struct_80283430 dato_80287998[];
extern f32 cinematica_spline_segmento_progreso;
extern s16 segmento_spline_cinematica;
extern s16 dato_80287B1E;
extern s8 dato_80287B20;
extern struct struct_80282C40 dato_802856DC[];
extern struct struct_80282C40 dato_80285718[];
extern struct struct_80282C40 dato_80285754[];
extern struct struct_80282C40 dato_80285784[];
extern struct struct_80282C40 dato_802857B4[];
extern struct struct_80282C40 dato_802857CC[];
extern struct struct_80282C40 dato_802857F0[];
extern struct struct_80282C40 dato_80285850[];
extern struct struct_80282C40 dato_802858B0[];
extern struct struct_80282C40 dato_802858C8[];
extern struct struct_80282C40 dato_80285910[];
extern struct struct_80282C40 dato_80285928[];
extern struct struct_80282C40 dato_80285940[];
extern struct struct_80282C40 dato_80285A10[];
extern struct struct_80282C40 dato_80285A4C[];
extern struct struct_80282C40 dato_80285A88[];
extern struct struct_80282C40 dato_80285AB8[];
extern struct struct_80282C40 dato_80285AE8[];
extern struct struct_80282C40 dato_80285B00[];
extern struct struct_80282C40 dato_80285B18[];
extern struct struct_80282C40 dato_80285B54[];
extern struct struct_80282C40 dato_80285B90[];
extern struct struct_80282C40 dato_80285BA8[];
extern struct struct_80282C40 dato_80285C38[];
extern struct struct_80282C40 dato_80285C74[];
extern struct Cinematica escena_corte[];
extern s16 dato_80285D14;
extern s32 dato_802876D4;
extern s32 dato_802876D8;

#endif
