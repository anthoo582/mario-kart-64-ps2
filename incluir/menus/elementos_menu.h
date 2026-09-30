#ifndef MENUS_ELEMENTOS_MENU_H
#define MENUS_ELEMENTOS_MENU_H

#include <juego/estructuras_comunes.h>
#include "datos/texturas.h"
#include "sistema/bucle_principal.h"

extern u32 _course_mario_raceway_dl_mio0SegmentRomStart[];

typedef struct {
     s32 type;
     s32 state;
     s32 estado_sub;
     s32 column;
     s32 row;
     s8 priority;
     bool8 visible;
     s16 unused;
     s32 d_8018DEE0_indice;
     s32 param1;
     s32 param2;
     f32 paramf;
} MenuItem;

typedef struct {
     AnimacionMk* textura_secuencia;
     s32 indice_secuencia;
     s32 abajo_cantidad_frame;
     u32 visible;
     s32 indice_textura_menu;
     s32 unk14;
} struct_8018DEE0_entrada;

typedef struct {
     TexturaMenu* texture;
     s32 tex_num;
} struct_8018E060_entrada;

typedef struct {
     TexturaMenu* textura_mk64;
     s16 desconocido4;
     s16 desconocido6;
} struct_8018E0E8_entrada;

typedef struct {
     u64* textura_datos;
     s32 offset;
} TexturaMapa;

typedef struct {
     TexturaMenu* texturas;
     Gfx* display_list;
} struct_8018E768_entrada;

struct desconocido_struct_8018E7E8 {
     s16 x;
     s16 y;
     s16 desconocido1;
     s16 desconocido2;
};

typedef struct {
     s16 column;
     s16 row;
     s16 pad0;
     s16 pad1;
} desconocido_d_800E70A0;

enum PRIORIDAD_ITEM_MENU {
    PRIORIDAD_ITEM_MENU_0,
    PRIORIDAD_ITEM_MENU_1,
    PRIORIDAD_ITEM_MENU_2,
    PRIORIDAD_ITEM_MENU_3,
    PRIORIDAD_ITEM_MENU_4,
    PRIORIDAD_ITEM_MENU_5,
    PRIORIDAD_ITEM_MENU_6,
    PRIORIDAD_ITEM_MENU_7,
    PRIORIDAD_ITEM_MENU_8,
    PRIORIDAD_ITEM_MENU_9,
    PRIORIDAD_A_ITEM_MENU,
    PRIORIDAD_B_ITEM_MENU,
    PRIORIDAD_C_ITEM_MENU,
    PRIORIDAD_D_ITEM_MENU,
    PRIORIDAD_E_ITEM_MENU,
    PRIORIDAD_F_ITEM_MENU,
    MENU_ITEM_PRIORIDAD_MAX
};

enum TextoCentro { TEXTO_IZQUIERDA = 1, MODO_TEXTO_CENTRO_1, TEXTO_DERECHA, MODO_TEXTO_CENTRO_2 };

enum TIPO_ITEM_MENU {
    MENU_ITEM_IU_NINGUNO,
    MENU_ITEM_IU_INICIO_FONDO,
    MENU_ITEM_IU_LOGO_Y_COPYRIGHT,
    MENU_ITEM_IU_EMPUJE_INICIO_BOTON,
    MENU_ITEM_IU_SIN_MANDO,
    MENU_ITEM_IU_INICIO_REGISTRO_TIEMPO,
    MENU_ITEM_IU_JUEGO_SELECCION = 0xA,
    MENU_ITEM_IU_1J_JUEGO,
    MENU_ITEM_IU_2J_JUEGO,
    MENU_ITEM_IU_3J_JUEGO,
    MENU_ITEM_IU_4J_JUEGO,
    MENU_ITEM_IU_OK,
    MENU_PRINCIPAL_GFX_OPCION,
    MENU_PRINCIPAL_GFX_DATOS,
    MENU_PRINCIPAL_50CC,
    MENU_PRINCIPAL_100CC,
    MENU_PRINCIPAL_150CC,
    MENU_PRINCIPAL_CC_EXTRA,
    TIPO_ITEM_MENU_016,
    TIPO_ITEM_MENU_017,
    MENU_PRINCIPAL_CONTRARRELOJ_EMPEZAR,
    MENU_PRINCIPAL_CONTRARRELOJ_DATOS,
    TIPO_ITEM_MENU_01B = 0x1B,
    MENU_PRINCIPAL_FONDO = 0x23,
    FONDO_SELECCION_PERSONAJE,
    FONDO_SELECCION_CIRCUITO,
    PERSONAJE_SELECCION_MENU_JUGADOR_SELECCION_CARTEL = 0x2A,
    MENU_SELECCION_PERSONAJE_MARIO,
    MENU_SELECCION_PERSONAJE_LUIGI,
    MENU_SELECCION_PERSONAJE_TOAD,
    MENU_SELECCION_PERSONAJE_PEACH,
    MENU_SELECCION_PERSONAJE_YOSHI,
    MENU_SELECCION_PERSONAJE_DK,
    MENU_SELECCION_PERSONAJE_WARIO,
    MENU_SELECCION_PERSONAJE_BOWSER,
    PERSONAJE_SELECCION_MENU_OK,
    PERSONAJE_SELECCION_MENU_1J_CURSOR,
    PERSONAJE_SELECCION_MENU_2J_CURSOR,
    PERSONAJE_SELECCION_MENU_3J_CURSOR,
    PERSONAJE_SELECCION_MENU_4J_CURSOR,
    TIPO_ITEM_MENU_043 = 0x43,
    TIPO_ITEM_MENU_044,
    TIPO_ITEM_MENU_045,
    TIPO_ITEM_MENU_046,
    TIPO_ITEM_MENU_047,
    TIPO_ITEM_MENU_048,
    TIPO_ITEM_MENU_049,
    TIPO_ITEM_MENU_050 = 0x50,
    CIRCUITO_SELECCION_MAPA_SELECCION = 0x52,
    SELECCION_CIRCUITO_COPA_HONGO,
    SELECCION_CIRCUITO_COPA_FLOR,
    SELECCION_CIRCUITO_COPA_ESTRELLA,
    SELECCION_CIRCUITO_COPA_ESPECIAL,
    TIPO_ITEM_MENU_058 = 0x58,
    CIRCUITO_SELECCION_CIRCUITO_NOMBRES,
    TIPO_ITEM_MENU_05A,
    TIPO_ITEM_MENU_05B,
    CIRCUITO_SELECCION_BATALLA_NOMBRES,
    OK_SELECCION_CIRCUITO,
    TIPO_ITEM_MENU_05E,
    TIPO_ITEM_MENU_05F,
    TIPO_ITEM_MENU_060,
    TIPO_ITEM_MENU_061,
    TIPO_ITEM_MENU_062,
    TIPO_ITEM_MENU_064 = 0x64,
    TIPO_ITEM_MENU_065,
    TIPO_ITEM_MENU_066,
    TIPO_ITEM_MENU_067,
    TIPO_ITEM_MENU_068,
    TIPO_ITEM_MENU_069,
    TIPO_ITEM_MENU_06E = 0x6E,
    TIPO_ITEM_MENU_078 = 0x78,
    TIPO_ITEM_MENU_079,
    TIPO_ITEM_MENU_07A,
    TIPO_ITEM_MENU_07B,
    TIPO_ITEM_MENU_07C,
    TIPO_ITEM_MENU_07D,
    TIPO_ITEM_MENU_07E,
    TIPO_ITEM_MENU_07F,
    TIPO_ITEM_MENU_080,
    TIPO_ITEM_MENU_081,
    TIPO_ITEM_MENU_082,
    TIPO_ITEM_MENU_083,
    TIPO_ITEM_MENU_084,
    TIPO_ITEM_MENU_085,
    TIPO_ITEM_MENU_086,
    TIPO_ITEM_MENU_087,
    TIPO_ITEM_MENU_088,
    TIPO_ITEM_MENU_089,
    TIPO_ITEM_MENU_08A,
    TIPO_ITEM_MENU_08B,
    TIPO_ITEM_MENU_08C,
    TIPO_ITEM_MENU_08D,
    TIPO_ITEM_MENU_096 = 0x96,
    TIPO_ITEM_MENU_097,
    TIPO_ITEM_MENU_098,
    TIPO_ITEM_MENU_0A0 = 0xA0,
    TIPO_ITEM_MENU_0A1,
    TIPO_ITEM_MENU_0AA = 0xAA,
    TIPO_ITEM_MENU_0AB,
    TIPO_ITEM_MENU_0AC,
    TIPO_ITEM_MENU_0AF = 0XAF,
    TIPO_ITEM_MENU_0B0,
    TIPO_ITEM_MENU_0B1,
    TIPO_ITEM_MENU_0B2,
    TIPO_ITEM_MENU_0B3,
    TIPO_ITEM_MENU_0B4,
    TIPO_ITEM_MENU_0B9 = 0xB9,
    TIPO_ITEM_MENU_0BA,
    TIPO_ITEM_MENU_0BB,
    MENU_ITEM_ANUNCIO_FANTASMA,
    MENU_ITEM_FIN_CIRCUITO_OPCION,
    TIPO_ITEM_MENU_0BE,
    PAUSA_ITEM_MENU = 0xC7,
    TIPO_ITEM_MENU_0D2 = 0xD2,
    TIPO_ITEM_MENU_0D3,
    TIPO_ITEM_MENU_0D4,
    TIPO_ITEM_MENU_0D5,
    TIPO_ITEM_MENU_0D6,
    TIPO_ITEM_MENU_0D7,
    TIPO_ITEM_MENU_0D8,
    TIPO_ITEM_MENU_0D9,
    TIPO_ITEM_MENU_0DA,
    MENU_ITEM_DATOS_CIRCUITO_IMAGEN = 0xE6,
    MENU_ITEM_DATOS_CIRCUITO_INFO,
    MENU_ITEM_DATOS_CIRCUITO_SELECCIONABLE,
    TIPO_ITEM_MENU_0E9,
    TIPO_ITEM_MENU_0EA,
    TIPO_ITEM_MENU_0F0 = 0xF0,
    TIPO_ITEM_MENU_0F1,
    MENU_ITEM_IU_LOGO_INTRO = 0xFA,
    MENU_INICIO_BANDERA,
    TIPO_ITEM_MENU_10E = 0x10E,
    TIPO_ITEM_MENU_12B = 0X12B,
    TIPO_ITEM_MENU_12C,
    TIPO_ITEM_MENU_12D,
    TIPO_ITEM_MENU_12E,
    TIPO_ITEM_MENU_12F,
    TIPO_ITEM_MENU_130,
    TIPO_ITEM_MENU_190 = 0x190,
    TIPO_ITEM_MENU_191,
    TIPO_ITEM_MENU_192,
    TIPO_ITEM_MENU_193,
    TIPO_ITEM_MENU_194,
    TIPO_ITEM_MENU_195,
    TIPO_ITEM_MENU_196,
    TIPO_ITEM_MENU_197,
    TIPO_ITEM_MENU_198,
    TIPO_ITEM_MENU_199,
    TIPO_ITEM_MENU_19A,
    TIPO_ITEM_MENU_19B,
    TIPO_ITEM_MENU_19C,
    TIPO_ITEM_MENU_19D,
    TIPO_ITEM_MENU_19E,
    TIPO_ITEM_MENU_19F,
    TIPO_ITEM_MENU_1A0,
    TIPO_ITEM_MENU_1A1,
    TIPO_ITEM_MENU_1A2,
    TIPO_ITEM_MENU_1A3,
    TIPO_ITEM_MENU_1A4,
    TIPO_ITEM_MENU_1A5,
    TIPO_ITEM_MENU_1A6,
    TIPO_ITEM_MENU_1A7,
    TIPO_ITEM_MENU_1A8,
    TIPO_ITEM_MENU_1A9,
    TIPO_ITEM_MENU_1AA,
    TIPO_ITEM_MENU_1AB,
    TIPO_ITEM_MENU_1AC,
    TIPO_ITEM_MENU_1AD,
    TIPO_ITEM_MENU_1AE,
    TIPO_ITEM_MENU_1AF,
    TIPO_ITEM_MENU_1B0,
    TIPO_ITEM_MENU_1B1,
    TIPO_ITEM_MENU_1B2,
    TIPO_ITEM_MENU_1B3,
    TIPO_ITEM_MENU_1B4,
    TIPO_ITEM_MENU_1B5,
    TIPO_ITEM_MENU_1B6,
    TIPO_ITEM_MENU_1B7,
    TIPO_ITEM_MENU_1B8,
    TIPO_ITEM_MENU_1B9,
    TIPO_ITEM_MENU_1BA,
    TIPO_ITEM_MENU_1BB,
    TIPO_ITEM_MENU_1BC,
    TIPO_ITEM_MENU_1BD,
    TIPO_ITEM_MENU_1BE,
    TIPO_ITEM_MENU_1BF,
    TIPO_ITEM_MENU_1C0,
    TIPO_ITEM_MENU_1C1,
    TIPO_ITEM_MENU_1C2,
    TIPO_ITEM_MENU_1C3,
    TIPO_ITEM_MENU_1C4,
    TIPO_ITEM_MENU_1C5,
    TIPO_ITEM_MENU_1C6,
    TIPO_ITEM_MENU_1C7,
    TIPO_ITEM_MENU_1C8,
    TIPO_ITEM_MENU_1C9,
    TIPO_ITEM_MENU_1CA,
    TIPO_ITEM_MENU_1CB,
    TIPO_ITEM_MENU_1CC,
    TIPO_ITEM_MENU_1CD,
    TIPO_ITEM_MENU_1CE
};

enum CargaImgCompTipo {
    CARGA_MENU_IMG_MIO0_UNA_VEZ = -1,
    CARGA_MENU_IMG_TKMK00_UNA_VEZ,
    CARGA_MENU_IMG_FORZAR = CARGA_MENU_IMG_TKMK00_UNA_VEZ,
    CARGA_MENU_IMG_MIO0_FORZAR,
    CARGA_MENU_IMG_TKMK00_FORZAR
};

enum ID_MENU_TEXTO { JUEGO_CONTINUAR, REINTENTO, CAMBIO_CIRCUITO, CAMBIO_CONDUCTOR, MENU_TEXTO_ABANDONAR, REPETICION, FANTASMA_GUARDADO };

f64 exponente_por_elevar_cuadrado(f64, s32);
f64 pot_menu(f64, f64);
f64 menu_ln(f64);
f64 exponencial_menu(f64);
f64 pot2_menu(f64, s32);
f64 normalizar_a_intervalo_unidad(f64, s32*);
void intercambiar_valores(s32*, s32*);
s32 funcion_80091D74(void);
void funcion_80091EE4(void);
void funcion_80091FA4(void);
void funcion_80092148(void);
void funcion_800921B4(void);
void efecto_rainbow_texto(s32, s32, s32);
void fijar_rainbow_color_texto_si_seleccionado(s32, s32, s32);
void funcion_80092258(void);
void funcion_80092290(s32, s32*, s32*);
void funcion_80092500(void);
void funcion_80092564(void);
void funcion_800925A0(void);
void funcion_800925CC(void);
void funcion_80092604(void);
void funcion_80092630(void);
void funcion_8009265C(void);
void funcion_80092688(void);
void funcion_80092C80(void);
s32 car_a_indice_glifo(char*);
s32 funcion_80092DF8(char*);
s32 funcion_80092E1C(char*);
s32 funcion_80092EE4(char*);
s32 obtener_ancho_cadena(char*);
void fijar_color_texto(s32);
void funcion_800930E4(s32, s32, char*);
void imprimir_texto0(s32, s32, char*, s32, f32, f32, s32);
void imprimir_modo_texto_1(s32, s32, char*, s32, f32, f32);
void imprimir_modo_texto_2(s32, s32, char*, s32, f32, f32);
void imprimir_texto1(s32, s32, char*, s32, f32, f32, s32);
void imprimir_izquierda_texto1(s32, s32, char*, s32, f32, f32);
void imprimir_modo_centro_texto1_1(s32, s32, char*, s32, f32, f32);
void imprimir_derecha_texto1(s32, s32, char*, s32, f32, f32);
void imprimir_modo_centro_texto1_2(s32, s32, char*, s32, f32, f32);
void imprimir_texto2(s32, s32, char*, s32, f32, f32, s32);
void funcion_800939C8(s32, s32, char*, s32, f32, f32);
void dibujar_texto(s32, s32, char*, s32, f32, f32);
void funcion_80093A30(s32);
void funcion_80093A5C(u32);
void funcion_80093B70(u32);
void funcion_80093C1C(s32);
void funcion_80093C88(void);
void funcion_80093C90(void);
void funcion_80093C98(s32);
void funcion_80093E20(void);
void funcion_80093E40(void);
void funcion_80093E60(void);
void funcion_80093F10(void);
void funcion_800940EC(s32);
void funcion_800942D0(void);
void funcion_80094660(struct GfxPool*, s32);
void renderizar_bandera_a_cuadros(struct GfxPool*, s32);
void funcion_80094A64(struct GfxPool*);
void preparar_menus(void);
void funcion_80095574(void);
Gfx* seleccionar_case_destello_dibujo(Gfx*, s32, s32, s32, s32, s32);
Gfx* seleccionar_lento_case_destello_dibujo(Gfx*, s32, s32, s32, s32);
Gfx* seleccionar_rapido_case_destello_dibujo(Gfx*, s32, s32, s32, s32);
Gfx* funcion_800959F8(Gfx*, Vtx*);
Gfx* funcion_80095BD0(Gfx*, u8*, f32, f32, u32, u32, f32, f32);
Gfx* funcion_80095E10(Gfx*, s8, s32, s32, s32, s32, s32, s32, s32, s32, u8*, u32, u32);
Gfx* funcion_800963F0(Gfx*, s8, s32, s32, f32, f32, s32, s32, s32, s32, s32, s32, u8*, u32, u32);
Gfx* funcion_80096CD8(Gfx* display_list_cabeza_2, s32 x_pos, s32 y_pos, u32 ancho, u32 altura);
Gfx* funcion_80097274(Gfx* display_list_cabeza_2, s8 parametro1, s32 parametro2, s32 parametro3, s32 parametro4, s32 parametro5, s32 parametro6, s32 parametro7, s32 parametro8,
                   s32 parametro9, u16* parametro_a, u32 parametro_b, u32 parametro_c, s32 parametro_d);
Gfx* funcion_80097A14(Gfx*, s8, s32, s32, s32, s32, s32, s32, u8*, u32, u32);
Gfx* funcion_80097AE4(Gfx*, s8, s32, s32, u8*, s32);
Gfx* funcion_80097E58(Gfx* display_list_cabeza_2, s8 fmt, u32 parametro2, u32 parametro3, u32 parametro4, u32 parametro5, s32 parametro6, s32 parametro7,
                   u8* algun_textura, u32 parametro9, u32 parametro_a, s32 ancho);
Gfx* funcion_80098558(Gfx*, u32, u32, u32, u32, u32, u32, s32, s32);
Gfx* funcion_800987D0(Gfx*, u32, u32, u32, u32, s32, s32, u8*, u32, s32);
Gfx* dibujar_relleno_caja(Gfx*, s32, s32, s32, s32, s32, s32, s32, s32);
Gfx* dibujar_caja(Gfx*, s32, s32, s32, s32, u32, u32, u32, u32);
Gfx* funcion_80098FC8(Gfx*, s32, s32, s32, s32);
void copiar_base_dma_729a30(u64*, size_t, void*);
void copiar_base_dma_7fa3c0(u64*, size_t, void*);
void borrar_texturas_menu(void);
void cargar_img_menu(TexturaMenu*);
void* segmentado_a_duplicado_virtual(const void*);
void* segmentado_a_duplicado_virtual_2(const void*);
void cargar_menu_img_mio0_forzado(TexturaMenu*);
void cargar_menu_img_comp_tipo(TexturaMenu*, s32);
void funcion_80099958(TexturaMenu*, s32, s32);
void funcion_80099E54(void);
void funcion_80099E60(TexturaMenu*, s32, s32);
void funcion_80099EC4(void);
void funcion_80099A70(void);
void funcion_80099A94(TexturaMenu*, s32);
void funcion_80099AEC(void);
void funcion_8009A238(TexturaMenu*, s32);
void funcion_8009A2F0(struct_8018E0E8_entrada*);
void funcion_8009A344(void);
s32 seleccionar_menu_personaje_animar(AnimacionMk*);
s32 funcion_8009A478(AnimacionMk*, s32);
void funcion_8009A594(s32, s32, AnimacionMk*);
void funcion_8009A640(s32, s32, s32, AnimacionMk*);
void funcion_8009A6D4(void);
void funcion_8009A76C(s32, s32, s32, s32);
void funcion_8009A7EC(s32, s32, s32, s32, s32);
TexturaMenu* funcion_8009A878(struct_8018DEE0_entrada*);
TexturaMenu* funcion_8009A944(struct_8018DEE0_entrada*, s32);
void funcion_8009A9FC(s32, s32, u32, s32);
void funcion_8009AB7C(s32);
void funcion_8009AD78(s32, s32);
void convertir_img_a_escala_grises(s32, u32);
void ajustar_color_img(s32, s32, s32, s32, s32);
u16* funcion_8009B8C4(u64*);
void funcion_8009B938(void);
void funcion_8009B954(TexturaMenu*);
void funcion_8009B998(void);
Gfx* funcion_8009B9D0(Gfx*, TexturaMenu*);
Gfx* renderizar_texturas_menu(Gfx*, TexturaMenu*, s32, s32);
Gfx* funcion_8009BC9C(Gfx*, TexturaMenu*, s32, s32, s32, s32);
Gfx* imprimir_letra(Gfx*, TexturaMenu*, f32, f32, s32, f32, f32);
Gfx* funcion_8009C204(Gfx*, TexturaMenu*, s32, s32, s32);
Gfx* funcion_8009C434(Gfx*, struct_8018DEE0_entrada*, s32, s32, s32);
Gfx* funcion_8009C708(Gfx*, struct_8018DEE0_entrada*, s32, s32, s32, s32);
void funcion_8009C918(void);
void funcion_8009CA2C(void);
void funcion_8009CA6C(s32);
void dibujar_fundido_en(s32, s32, s32);
void dibujar_fundido_negro_en(s32, s32);
void dibujar_fundido_blanco_en(s32, s32);
void funcion_8009CE1C(void);
void funcion_8009CE64(s32);
void funcion_8009D77C(s32, s32, s32);
void funcion_8009D958(s32, s32);
void funcion_8009D978(s32, s32);
void funcion_8009D998(s32);
void funcion_8009DAA8(void);
void funcion_8009DB8C(void);
void funcion_8009DEF8(u32, u32);
void funcion_8009DF4C(s32);
void funcion_8009DF6C(s32);
void funcion_8009DF8C(u32, u32);
void funcion_8009DFE0(s32);
void funcion_8009E000(s32);
void funcion_8009E020(s32, s32);
void funcion_8009E088(s32, s32);
void funcion_8009E0F0(s32);
void funcion_8009E1C0(void);
void funcion_8009E1E4(void);
void funcion_8009E208(void);
void funcion_8009E230(void);
void funcion_8009E258(void);
void funcion_8009E280(void);
void funcion_8009E2A8(s32);
void funcion_8009E2F0(s32);
void funcion_8009E5BC(void);
void funcion_8009E5FC(s32);
void borrar_menus(void);
void agregar_item_menu(s32, s32, s32, s8);
void renderizar_menus(MenuItem*);
void funcion_800A08D8(u8, s32, s32);
s32 funcion_800A095C(char*, s32, s32, s32);
void funcion_800A09E0(MenuItem*);
void funcion_800A0AD0(MenuItem*);
void funcion_800A0B80(MenuItem*);
void funcion_800A0DFC(void);
void funcion_800A0EB8(MenuItem*, s32);
void funcion_800A0FA4(MenuItem*, s32);
void funcion_800A10CC(MenuItem*);
void renderizar_jugador_cursor(MenuItem*, s32, s32);
void funcion_800A12BC(MenuItem*, TexturaMenu*);
void funcion_800A1350(MenuItem*);
void funcion_800A143C(MenuItem*, s32);
void funcion_800A1500(MenuItem*);
void funcion_800A15EC(MenuItem*);
void funcion_800A1780(MenuItem*);
void renderizar_menu_item_datos_circuito_imagen(MenuItem*);
void renderizar_menu_item_datos_circuito_info(MenuItem*);
void menu_item_datos_circuito_seleccionable(MenuItem*);
void funcion_800A1DE0(MenuItem*);
void funcion_800A1F30(MenuItem*);
void funcion_800A1FB0(MenuItem*);
void funcion_800A2D1C(MenuItem*);
void funcion_800A2EB8(MenuItem*);
void funcion_800A32B4(s32, s32, s32, s32);
void funcion_800A34A8(MenuItem*);
void funcion_800A3A10(s8*);
void funcion_800A3ADC(MenuItem*, s32, s32, s32, s32, s8*);
void renderizar_contrarreloj_texto_meta(MenuItem*);
void funcion_800A3E60(MenuItem*);
void renderizar_tiempo_vuelta(s32, s32, s32);
void renderizar_veces_vuelta(s32, s32, s32);
void renderizar_menu_item_anuncio_fantasma(MenuItem*);
void renderizar_menu_pausa(MenuItem*);
void renderizar_menu_pausa_contrarreloj(MenuItem*);
void renderizar_menu_pausa_versus(MenuItem*);
void renderizar_pausa_gran_premio(MenuItem*);
void renderizar_batalla_pausa(MenuItem*);
void funcion_800A54EC(void);
void renderizar_menu_item_fin_circuito_opcion(MenuItem*);
void funcion_800A6034(MenuItem*);
void funcion_800A6154(MenuItem*);
void funcion_800A638C(MenuItem*);
void funcion_800A66A8(MenuItem*, desconocido_d_800E70A0*);
void funcion_800A69C8(MenuItem*);
void funcion_800A6BEC(MenuItem*);
void funcion_800A6CC0(MenuItem*);
void funcion_800A6D94(s32, s32, u8*);
void funcion_800A6E94(s32, s32, u8*);
void funcion_800A70E8(MenuItem*);
void funcion_800A7258(MenuItem*);
void funcion_800A72FC(MenuItem*);
void funcion_800A7448(MenuItem*);
void funcion_800A75A0(MenuItem*);
void funcion_800A761C(MenuItem*);
void renderizar_creditos_item_menu(MenuItem*);
void convertir_numero_a_ascii(s32, char*);
void escribir_guiones(char*);
void obtener_minutos_registro_tiempo(s32, char*);
void obtener_segundos_registro_tiempo(s32, char*);
void obtener_centesimas_registro_tiempo(s32, char*);
void funcion_800A79F4(s32, char*);
void manejar_menus_con_parametro_prio(s32);
void manejar_predeterminado_menus(void);
void manejar_especial_menus(void);
void funcion_800A8270(s32, MenuItem*);
void funcion_800A8564(MenuItem*);
void funcion_800A86E8(MenuItem*);
void funcion_800A874C(MenuItem*);
void funcion_800A890C(s32, MenuItem*);
void funcion_800A8A98(MenuItem*);
void funcion_800A8CA4(MenuItem*);
void renderizar_introduccion_batalla(MenuItem*);
void funcion_800A8EC0(MenuItem*);
void funcion_800A8F48(MenuItem*);
void funcion_800A90D4(s32, MenuItem*);
void funcion_800A91D8(MenuItem*, s32, s32);
void funcion_800A9208(MenuItem*, s32);
void funcion_800A9278(MenuItem*, s32);
void funcion_800A92E8(MenuItem*, s32);
void funcion_800A939C(MenuItem*, s32);
void funcion_800A940C(MenuItem*, s32);
void funcion_800A94C8(MenuItem*, s32, s32);
void funcion_800A954C(MenuItem*);
void funcion_800A9710(MenuItem*);
void funcion_800A97BC(MenuItem*);
void actualizar_item_menu_ok(MenuItem*);
void funcion_800A9B9C(MenuItem*);
void funcion_800A9C40(MenuItem*);
void funcion_800A9D5C(MenuItem*);
void funcion_800A9E58(MenuItem*);
void funcion_800AA280(MenuItem*);
void funcion_800AA2EC(MenuItem*);
void funcion_800AA5C8(MenuItem*, s8);
void funcion_800AA69C(MenuItem*);
void funcion_800AAA9C(MenuItem*);
void funcion_800AAB90(MenuItem*);
void funcion_800AAC18(MenuItem*);
void actualizar_cursor(MenuItem*);
void funcion_800AAE18(MenuItem*);
MenuItem* obtener_menu_item_jugador_cantidad(void);
MenuItem* obtener_personaje_item_menu(s32);
MenuItem* buscar_duplicado_items_menu(s32);
MenuItem* buscar_items_menu(s32);
s32 obtener_estado_menu_personaje(s32);
void flotar_cursor_sobre_retrato_personaje(MenuItem*, s32);
s32 funcion_800AAFCC(s32);
void funcion_800AB020(MenuItem*);
void funcion_800AB098(MenuItem*);
void funcion_800AB164(MenuItem*);
void funcion_800AB260(MenuItem*);
void funcion_800AB290(MenuItem*);
void funcion_800AB314(MenuItem*);
void funcion_800AB904(MenuItem*);
void funcion_800AB9B0(MenuItem*);
void funcion_800ABAE8(MenuItem*);
void funcion_800ABB24(MenuItem*);
void funcion_800ABBCC(MenuItem*);
void funcion_800ABC38(MenuItem*);
void funcion_800ABCF4(MenuItem*);
void funcion_800ABEAC(MenuItem*);
void funcion_800ABF68(MenuItem*);
void funcion_800AC128(MenuItem*);
void funcion_800AC300(MenuItem*);
void funcion_800AC324(MenuItem*);
void funcion_800AC458(MenuItem*);
void funcion_800AC978(MenuItem*);
void funcion_800ACA14(MenuItem*);
void funcion_800ACC50(MenuItem*);
void funcion_800ACF40(MenuItem*);
void funcion_800AD1A4(MenuItem*);
void funcion_800AD2E8(MenuItem*);
void funcion_800ADF48(MenuItem*);
void funcion_800AE218(MenuItem*);
void funcion_800AEC54(MenuItem*);
void funcion_800AEDBC(MenuItem*);
void funcion_800AEE90(MenuItem*);
void funcion_800AEEBC(MenuItem*);
void funcion_800AEEE8(MenuItem*);
void funcion_800AEF14(MenuItem*);
void funcion_800AEF74(MenuItem*);
void funcion_800AF004(MenuItem*);
void funcion_800AF1AC(MenuItem*);
void funcion_800AF270(MenuItem*);
void funcion_800AF480(MenuItem*);
void funcion_800AF4DC(MenuItem*);
void funcion_800AF740(MenuItem*);

void rmon_printf(const char*, ...);
void tkmk00decode(u32*, u8*, u16*, s32);

#define MENU_ITEMS_MAX 0x20
#define TAMANIO_D_8018DEE0 0x10
#define TAMANIO_D_8018E060 0x10
#define TAMANIO_D_8018E0E8 0x05
#define TEXTURA_MAX_MAPA 0xC8
#define TAMANIO_D_8018E768 0x08
#define TAMANIO_D_8018E7E8 0x05
#define TAMANIO_D_8018E810 0x05

extern s32 dato_800DDB24;
extern s16 jugador_obtener_por_id_personaje[];

extern u16* buffer_textura_menu;
extern u32* buffer_comprimido_menu;
extern u8* tkmk_00_bajo_res_buffer;
extern u8* copia_puntos_gp;
extern void* algun_buffer_dl;
extern s8 puntos_gp_por_id_personaje[8];
extern s8 id_personaje_por_puesto_total_gp[];
extern s8 dato_8018D9D8;
extern s8 dato_8018D9D9;
extern MenuItem menu_items[MENU_ITEMS_MAX];
extern struct_8018DEE0_entrada dato_8018DEE0[TAMANIO_D_8018DEE0];
extern struct_8018E060_entrada dato_8018E060[TAMANIO_D_8018E060];
extern struct_8018E0E8_entrada dato_8018E0E8[TAMANIO_D_8018E0E8];
extern s32 menu_textura_buffer_indice;
extern TexturaMapa mapa_textura_menu[TEXTURA_MAX_MAPA];
extern s32 entradas_textura_menu;
extern Gfx* gfx_ptr;
extern s32 num_d_8018E768_entradas;
extern struct_8018E768_entrada dato_8018E768[TAMANIO_D_8018E768];
extern s32 menu_destello_ciclo;
extern s8 tipo_transicion[];
extern u32 duracion_transicion[];
extern u32 tiempo_transicion_actual[];
extern s32 dato_8018E7E0;
extern struct desconocido_struct_8018E7E8 dato_8018E7E8[TAMANIO_D_8018E7E8];
extern struct desconocido_struct_8018E7E8 dato_8018E810[TAMANIO_D_8018E810];
extern s8 g_color_texto;
extern u8 dato_8018ED90;
extern u8 dato_8018ED91;
extern s8 dato_8018E838[];
extern s32 dato_8018E840[];
extern s32 dato_8018E850[];
extern s32 dato_8018E854;
extern s32 dato_8018E858[];
extern s32 dato_8018E85C;

extern u8 _textures_0aSegmentRomStart[];
extern u8 _textures_0bSegmentRomStart[];

extern desconocido_d_800E70A0 dato_800E70A0[];
extern desconocido_d_800E70A0 dato_800E70E8[];
extern desconocido_d_800E70A0 dato_800E7108[][4];
extern desconocido_d_800E70A0 dato_800E7148[];
extern desconocido_d_800E70A0 dato_800E7168[];
extern desconocido_d_800E70A0 dato_800E7188[];
extern desconocido_d_800E70A0 dato_800E7208[][2];
extern desconocido_d_800E70A0 dato_800E7248[];
extern desconocido_d_800E70A0 dato_800E7258[];
extern desconocido_d_800E70A0 dato_800E7268[];
extern desconocido_d_800E70A0 dato_800E7278[];
extern desconocido_d_800E70A0 dato_800E72F8;
extern desconocido_d_800E70A0 dato_800E7300[];
extern desconocido_d_800E70A0 dato_800E7360[];
extern desconocido_d_800E70A0 dato_800E7380[];
extern desconocido_d_800E70A0 dato_800E7390[];
extern desconocido_d_800E70A0 dato_800E73C0[];
extern desconocido_d_800E70A0 dato_800E73D0[];
extern desconocido_d_800E70A0 dato_800E73E0[];
extern desconocido_d_800E70A0 dato_800E7410[];
extern desconocido_d_800E70A0 dato_800E7420[];
extern desconocido_d_800E70A0 dato_800E7430[];
extern desconocido_d_800E70A0 dato_800E7458[];
extern desconocido_d_800E70A0 dato_800E7480[];
extern RGBA16 dato_800E74A8[];
extern RGBA16 dato_800E74D0[];
extern RGBA16 color_fondo[];
extern const s16 ancho_pantalla_glifo[];
extern char* nombres_copa[];
extern char* duplicar_nombres_circuito_2[];
extern const s8 por_indice_copa_por_id_circuito[];
extern const s8 dato_800EFD64[];
extern s8 seleccion_copa_por_id_circuito[];
extern char* texto_copa[];
extern char* nombres_personaje_depuracion[];
extern char* dato_800E76A8[];
extern char* dato_800E76CC[];
extern char* dato_800E76DC[];
extern char* depuracion_pantalla_modo_nombres[];
extern char* depuracion_sonido_modo_nombres[];
extern char* sonido_nombres_modo[];
extern char* texto_perder_victoria[];
extern char* texto_tiempo_mejor[];
extern char* texto_tiempo_vuelta;
extern char* texto_tiempo_prefijo[];
extern char* dato_800E7744[];
extern char* boton_pausa_texto[];
extern char* dato_800E7778[];
extern char texto_menu_anuncio_fantasma[];
extern char* dato_800E77A0[];
extern char* introduccion_batalla_texto[];
extern char datos_menu_texto[];
extern char dato_800E77D8[];
extern char* longitudes_circuito[];
extern char* opcion_menu_texto[];
extern char* dato_800E7840[];
extern char* borrar_mejor_fantasma_texto[];
extern char* dato_800E7860[];
extern char* menu_opcion_texto[];
extern char* dato_800E7878[];
extern char* dato_800E7884[];
extern char* dato_800E7890[];
extern char* dato_800E78D0[];
extern char* dato_800E7900[];
extern char* dato_800E7918[];
extern char* dato_800E7920[];
extern char* dato_800E7928[];
extern char* dato_800E7930[];
extern char* dato_800E7938[];
extern char* dato_800E7940[];
extern char* dato_800E7980[];
extern char* dato_800E798C[];
extern char* dato_800E7A34[];
extern char* dato_800E7A3C[];
extern char* dato_800E7A44;
extern char* dato_800E7A48[];
extern char* dato_800E7A54[];
extern char* dato_800E7A60[];
extern char* dato_800E7A6C[];
extern char* dato_800E7A74[];
extern char* dato_800E7A80[];
extern char* dato_800E7A88[];
extern char* dato_800E7A98;
extern char* dato_800E7A9C[];
extern char* texto_lugar[];
extern const s8 premios_punto_gp[];
extern const s8 dato_800F0B1C[];
extern const s8 dato_800F0B28[];
extern const s8 dato_800F0B50[];
extern const s8 dato_800F0B54[];
extern RGBA16 dato_800E7AC8[];
extern RGBA16 dato_800E7AE8[];
extern TexturaMenu* dato_800E7AF8[];
extern TexturaMenu* dato_800E7D0C[];
extern AnimacionMk* dato_800E7D34[];
extern TexturaMenu* fondo_texturas_menu[];
extern TexturaMenu* dato_800E7D54[];
extern TexturaMenu* dato_800E7D74[];
extern TexturaMenu* dato_800E7DC4[];
extern AnimacionMk* dato_800E7E14[];
#ifdef AVOID_UB
#define dato_800E7E20 (&dato_800E7E14[3])
#else
extern AnimacionMk* dato_800E7E20[];
#endif
extern AnimacionMk* dato_800E7E34[];
extern TexturaMenu* lut_textura_glifo[];
#ifdef AVOID_UB
#define dato_800E7FF0 (&lut_textura_glifo[91])
#define dato_800E80A0 (&lut_textura_glifo[135])
#define dato_800E8114 (&lut_textura_glifo[164])
#define dato_800E8174 (&lut_textura_glifo[188])
#define dato_800E817C (&lut_textura_glifo[190])
#define dato_800E81E4 (&lut_textura_glifo[216])
#define dato_800E822C (&lut_textura_glifo[234])
#else
extern TexturaMenu* dato_800E7FF0[];
extern TexturaMenu* dato_800E80A0[];
extern TexturaMenu* dato_800E8114[];
extern TexturaMenu* dato_800E8174[];
extern TexturaMenu* dato_800E817C[];
extern TexturaMenu* dato_800E81E4[];
extern TexturaMenu* dato_800E822C[];
#endif
extern TexturaMenu* dato_800E8234[];
extern TexturaMenu* dato_800E8254[];
extern TexturaMenu* dato_800E8274[];
extern TexturaMenu* dato_800E8294[];
extern TexturaMenu* menu_texturas_borde_jugador[];
extern TexturaMenu* menu_texturas_pista_seleccion[];
extern TexturaMenu* dato_800E82F4[];
extern AnimacionMk* dato_800E8320[];
extern AnimacionMk* dato_800E8340[];
extern AnimacionMk* dato_800E8360[];
extern AnimacionMk* animacion_celebracion_personaje[];
extern AnimacionMk* dato_800E83A0[];
extern AnimacionMk* animacion_deseleccionar_personaje[];
extern AnimacionMk* personaje_simple_parpadear_animacion[];
extern AnimacionMk* personaje_doble_parpadear_animacion[];
extern AnimacionMk* animacion_derrota_personaje[];
extern s32 dato_800E8440[];
extern s32 dato_800E8460[];
extern s32 dato_800E8480[];
extern s32 dato_800E84A0[];
extern Vtx* dato_800E84C0[];
extern Gfx* dato_800E84CC[];
extern Gfx* dato_800E84EC[];
extern Gfx* dato_800E850C[];
extern s8 dato_800E852C;
extern f32 intro_modelo_movimiento_rapidez;
extern f32 intro_modelo_rapidez;
extern desconocido_d_800E70A0 dato_800E8538[];
extern desconocido_d_800E70A0 dato_800E8540[];
extern desconocido_d_800E70A0 dato_800E85C0[];
extern desconocido_d_800E70A0 dato_800E8600[];

extern s32 controller_pak_libre_paginas_1_num;
extern s32 controller_pak_nota_1_archivo;
extern s32 controller_pak_nota_2_archivo;

extern f32 dato_8018ED98;
extern f32 dato_8018ED9C;
extern f32 dato_8018EDA0;

extern f32 dato_8018EDA4;
extern f32 dato_8018EDA8;
extern f32 dato_8018EDAC;

#endif
