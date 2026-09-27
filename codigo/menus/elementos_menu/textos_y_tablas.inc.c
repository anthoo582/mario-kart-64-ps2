// Textos y tablas

void guMtxCatL(Mtx* m, Mtx* n, Mtx* res);

u16* buffer_textura_menu;
u32* buffer_comprimido_menu;
u8* tkmk_00_bajo_res_buffer;
u8* copia_puntos_gp;
void* algun_buffer_dl;
s8 puntos_gp_por_id_personaje[8];
s8 id_personaje_por_puesto_total_gp[8];
s8 dato_8018D9D8;
s8 dato_8018D9D9;
MenuItem menu_items[MENU_ITEMS_MAX];
struct_8018DEE0_entrada dato_8018DEE0[TAMANIO_D_8018DEE0];
struct_8018E060_entrada dato_8018E060[TAMANIO_D_8018E060];
SIN_USO u8 menu_item_bss_margen0[8];
struct_8018E0E8_entrada dato_8018E0E8[TAMANIO_D_8018E0E8];
s32 menu_textura_buffer_indice;
TexturaMapa mapa_textura_menu[TEXTURA_MAX_MAPA];
#ifdef AVOID_UB
static void fijar_buffer_textura_menu(u16* buffer, u32 size);
#endif
s32 entradas_textura_menu;
Gfx* gfx_ptr;
s32 num_d_8018E768_entradas;
struct_8018E768_entrada dato_8018E768[TAMANIO_D_8018E768];
s32 menu_destello_ciclo;
s8 tipo_transicion[5];
u32 duracion_transicion[5];
u32 tiempo_transicion_actual[4];
s32 dato_8018E7E0;
struct desconocido_struct_8018E7E8 dato_8018E7E8[TAMANIO_D_8018E7E8];
struct desconocido_struct_8018E7E8 dato_8018E810[TAMANIO_D_8018E810];
s8 dato_8018E838[4];
s32 dato_8018E83C;

s32 dato_8018E840[4];
s32 dato_8018E850[2];
s32 dato_8018E858[2];
s8 g_color_texto;
s32 d_8018E864_relleno;
OSPfs controller_pak_manejador_1_archivo;
OSPfs controller_pak_manejador_2_archivo;
OSPfsState estado_pfs[16];
s32 pfs_error[16];
s32 controller_pak_1_num_archivos_usado;
s32 controller_pak_archivos_escribible_1_max;

s32 controller_pak_libre_paginas_1_num;
s32 controller_pak_nota_1_archivo;
s32 controller_pak_nota_2_archivo;
s32 menu_item_bss_relleno2;
ALIGNED8 DatosGuardado datos_guardado;

u8 dato_8018ED90;
u8 dato_8018ED91;
s32 temporizador_modelo_intro;

desconocido_d_800E70A0 dato_800E70A0[] = {
    { 0x3d, 0x11, 0x00, 0x00 }, { 0x15, 0x3e, 0x00, 0x00 }, { 0x5c, 0x3e, 0x00, 0x00 },
    { 0xa3, 0x3e, 0x00, 0x00 }, { 0xea, 0x3e, 0x00, 0x00 }, { 0x10a, 0xc8, 0x00, 0x00 },
    { 0x15, 0xc8, 0x00, 0x00 }, { 0x55, 0xc8, 0x00, 0x00 }, { 0x00, 0x00, 0x00, 0x00 },
};

desconocido_d_800E70A0 dato_800E70E8[] = {
    { 0x40, 0x41, 0x00, 0x00 },
    { 0x40, 0x53, 0x00, 0x00 },
    { 0x40, 0x65, 0x00, 0x00 },
    { 0x40, 0x77, 0x00, 0x00 },
};

desconocido_d_800E70A0 dato_800E7108[][4] = {
    {
        { 0x18, 0x3f, 0x00, 0x00 },
        { 0x5d, 0x3f, 0x00, 0x00 },
        { 0xa2, 0x3f, 0x00, 0x00 },
        { 0xe7, 0x3f, 0x00, 0x00 },
    },
    {
        { 0x18, 0x91, 0x00, 0x00 },
        { 0x5d, 0x91, 0x00, 0x00 },
        { 0xa2, 0x91, 0x00, 0x00 },
        { 0xe7, 0x91, 0x00, 0x00 },
    },
};

desconocido_d_800E70A0 dato_800E7148[] = {
    { 0x17, 0x3b, 0x00, 0x00 },
    { 0x5d, 0x3b, 0x00, 0x00 },
    { 0xa2, 0x3b, 0x00, 0x00 },
    { 0xe8, 0x3b, 0x00, 0x00 },
};

desconocido_d_800E70A0 dato_800E7168[] = {
    { 0x17, 0x70, 0x00, 0x00 },
    { 0x57, 0x70, 0x00, 0x00 },
    { 0x17, 0x97, 0x00, 0x00 },
    { 0x57, 0x97, 0x00, 0x00 },
};

desconocido_d_800E70A0 dato_800E7188[] = {
    { 0x80, 0x58, 0x00, 0x00 }, { 0x00, 0x00, 0x00, 0x00 }, { 0x00, 0x00, 0x00, 0x00 }, { 0x00, 0x00, 0x00, 0x00 },

    { 0x80, 0x3f, 0x00, 0x00 }, { 0x80, 0x91, 0x00, 0x00 }, { 0x00, 0x00, 0x00, 0x00 }, { 0x00, 0x00, 0x00, 0x00 },

    { 0x5a, 0x58, 0x00, 0x00 }, { 0xa6, 0x58, 0x00, 0x00 }, { 0x00, 0x00, 0x00, 0x00 }, { 0x00, 0x00, 0x00, 0x00 },

    { 0x5a, 0x3f, 0x00, 0x00 }, { 0xa6, 0x3f, 0x00, 0x00 }, { 0x5a, 0x91, 0x00, 0x00 }, { 0xa6, 0x91, 0x00, 0x00 },
};

desconocido_d_800E70A0 dato_800E7208[][2] = {
    {
        { 0x9d, 0x70, 0x00, 0x00 },
        { 0x128, 0x81, 0x00, 0x00 },
    },
    {
        { 0x9d, 0x88, 0x00, 0x00 },
        { 0x128, 0x99, 0x00, 0x00 },
    },
    {
        { 0x9d, 0xa0, 0x00, 0x00 },
        { 0x128, 0xb1, 0x00, 0x00 },
    },
    {
        { 0x9d, 0xb8, 0x00, 0x00 },
        { 0x128, 0xc9, 0x00, 0x00 },
    },
};

desconocido_d_800E70A0 dato_800E7248[] = {
    { 0xff6a, 0x3b, 0x00, 0x00 },
    { 0x172, 0x3b, 0x00, 0x00 },
};

desconocido_d_800E70A0 dato_800E7258[] = {
    { 0x17, 0x3b, 0x00, 0x00 },
    { 0xc5, 0x3b, 0x00, 0x00 },
};

desconocido_d_800E70A0 dato_800E7268[] = {
    { 0x28, 0x73, 0x00, 0x00 },
    { 0x28, 0x3c, 0x00, 0x00 },
};

desconocido_d_800E70A0 dato_800E7278[] = {
    { 0x3e, 0x43, 0x00, 0x00 },     { 0xa1, 0x43, 0x00, 0x00 },
    { 0x3e, 0xc5, 0x00, 0x00 },     { 0xa1, 0xc5, 0x00, 0x00 },

    { 0xffc0, 0xf0, 0x00, 0x00 },   { 0x140, 0xf0, 0x00, 0x00 },
    { 0xffc0, 0xffc0, 0x00, 0x00 }, { 0xffc0, 0xffc0, 0x00, 0x00 },

    { 0xffc0, 0xffc0, 0x00, 0x00 }, { 0x140, 0xffc0, 0x00, 0x00 },
    { 0xffc0, 0xf0, 0x00, 0x00 },   { 0xffc0, 0xffc0, 0x00, 0x00 },

    { 0xffc0, 0xffc0, 0x00, 0x00 }, { 0x140, 0xffc0, 0x00, 0x00 },
    { 0xffc0, 0xf0, 0x00, 0x00 },   { 0x140, 0xf0, 0x00, 0x00 },
};

desconocido_d_800E70A0 dato_800E72F8 = { 0x140, 0x23, 0x00, 0x00 };

desconocido_d_800E70A0 dato_800E7300[] = {
    { 0x50, 0x23, 0x00, 0x00 }, { 0xb0, 0x23, 0x00, 0x00 }, { 0x00, 0x00, 0x00, 0x00 }, { 0x00, 0x00, 0x00, 0x00 },

    { 0x32, 0x23, 0x00, 0x00 }, { 0x80, 0x23, 0x00, 0x00 }, { 0xce, 0x23, 0x00, 0x00 }, { 0x00, 0x00, 0x00, 0x00 },

    { 0x18, 0x23, 0x00, 0x00 }, { 0x5d, 0x23, 0x00, 0x00 }, { 0xa2, 0x23, 0x00, 0x00 }, { 0xe7, 0x23, 0x00, 0x00 },
};

desconocido_d_800E70A0 dato_800E7360[] = {
    { 0x61, 0xa7, 0x00, 0x00 },
    { 0x61, 0xb6, 0x00, 0x00 },
    { 0x61, 0xc5, 0x00, 0x00 },
    { 0x61, 0xd4, 0x00, 0x00 },
};

desconocido_d_800E70A0 dato_800E7380[] = {
    { 0x30, 0x4b, 0x00, 0x00 },
    { 0x109, 0x4b, 0x00, 0x00 },
};

desconocido_d_800E70A0 dato_800E7390[] = {
    { 0xad, 0x8d, 0x00, 0x00 }, { 0xad, 0x9a, 0x00, 0x00 }, { 0xad, 0xa7, 0x00, 0x00 },
    { 0xad, 0xb4, 0x00, 0x00 }, { 0xad, 0xc1, 0x00, 0x00 }, { 0xad, 0xce, 0x00, 0x00 },
};

desconocido_d_800E70A0 dato_800E73C0[] = {
    { 0xac, 0xa5, 0x00, 0x00 },
    { 0xac, 0xc3, 0x00, 0x00 },
};

desconocido_d_800E70A0 dato_800E73D0[] = {
    { 0xc0, 0xb3, 0x00, 0x00 },
    { 0xc0, 0xc2, 0x00, 0x00 },
};

desconocido_d_800E70A0 dato_800E73E0[] = {
    { 0x61, 0x94, 0x00, 0x00 }, { 0x61, 0xa1, 0x00, 0x00 }, { 0x61, 0xae, 0x00, 0x00 },
    { 0x61, 0xbb, 0x00, 0x00 }, { 0x61, 0xc8, 0x00, 0x00 }, { 0x61, 0xd5, 0x00, 0x00 },
};

desconocido_d_800E70A0 dato_800E7410[] = {
    { 0x52, 0x90, 0x00, 0x00 },
    { 0x52, 0xa4, 0x00, 0x00 },
};

desconocido_d_800E70A0 dato_800E7420[] = {
    { 0x76, 0x95, 0x00, 0x00 },
    { 0x76, 0xa4, 0x00, 0x00 },
};

desconocido_d_800E70A0 dato_800E7430[] = {
    { 0x17, 0xa, 0x00, 0x00 }, { 0x5d, 0xa, 0x00, 0x00 }, { 0xa2, 0xa, 0x00, 0x00 },
    { 0xe8, 0xa, 0x00, 0x00 }, { 0x17, 0xa, 0x00, 0x00 },
};

desconocido_d_800E70A0 dato_800E7458[] = {
    { 0x14a, 0x32, 0x00, 0x00 },  { 0xff60, 0xd4, 0x00, 0x00 }, { 0xa0, 0x10e, 0x00, 0x00 },
    { 0xff60, 0xbe, 0x00, 0x00 }, { 0x143, 0x5a, 0x00, 0x00 },
};

desconocido_d_800E70A0 dato_800E7480[] = {
    { 0xa0, 0x32, 0x00, 0x00 }, { 0x9b, 0xd4, 0x00, 0x00 }, { 0xa0, 0x50, 0x00, 0x00 },
    { 0x9b, 0xbe, 0x00, 0x00 }, { 0x80, 0x5a, 0x00, 0x00 },
};

RGBA16 dato_800E74A8[] = {
    { 0x00, 0xf3, 0xf3, 0xff }, { 0xff, 0xa8, 0xc3, 0xff }, { 0xff, 0xfe, 0x7a, 0xff },
    { 0x7b, 0xfc, 0x7b, 0xff }, { 0xff, 0xff, 0x00, 0xff },
};

RGBA16 dato_800E74D0[] = {
    { 0x00, 0xf3, 0xf3, 0xff },
    { 0xff, 0xa8, 0xc3, 0xff },
    { 0xff, 0xff, 0x00, 0xff },
};

RGBA16 color_fondo[] = {
    { 0xff, 0xaf, 0xaf, 0xff },
    { 0xaf, 0xff, 0xaf, 0xff },
    { 0xaf, 0xaf, 0xff, 0xff },
};

const s16 ancho_pantalla_glifo[] = {
    0x000c, 0x000d, 0x000b, 0x000b, 0x000a, 0x000b, 0x000b, 0x000d, 0x0007, 0x000a, 0x000c, 0x000a, 0x0012, 0x000d,
    0x000c, 0x000c, 0x000c, 0x000c, 0x000b, 0x000d, 0x000c, 0x000c, 0x0012, 0x000d, 0x000c, 0x000c, 0x000a, 0x000a,
    0x000a, 0x0006, 0x001e, 0x0006, 0x000a, 0x0008, 0x000b, 0x000c, 0x000c, 0x000d, 0x000a, 0x000b, 0x000a, 0x000a,
    0x0008, 0x001c, 0x000a, 0x0010, 0x000f, 0x0010, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f,
    0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f,
    0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000e, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f,
    0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f,
    0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000e, 0x000f, 0x000e,
    0x000f, 0x000e, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000e, 0x000e, 0x000e,
    0x000e, 0x000e, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f,
    0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f,
    0x000f, 0x000f, 0x000f, 0x000e, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f,
    0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f,
    0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000e, 0x000f, 0x000e, 0x000f, 0x000e, 0x000f, 0x000f,
    0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000e, 0x000e, 0x000e, 0x000e, 0x000e, 0x000b, 0x000f,
    0x000f, 0x000f, 0x000f, 0x001d, 0x001d, 0x001d, 0x001d, 0x001d, 0x001d, 0x001d, 0x001d, 0x001d, 0x001d, 0x000f,
    0x000f, 0x0017, 0x000f, 0x0017, 0x0017, 0x0017, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f,
};

char* nombres_copa[] = {
    "mushroom cup",
    "flower cup",
    "star cup",
    "special cup",
    "battle",
    "mushroom cup",
    "flower cup",
    "star cup",
    "special cup",
};

#if !ACTIVACION_PERSONALIZADO_CIRCUITO_MOTOR
char* nombres_circuito[] = {
#include "recursos/pistas/metadatos/nombres_circuito.inc.c"
};

char* duplicar_nombres_circuito[] = {
#include "recursos/pistas/metadatos/nombres_circuito.inc.c"
};
#else

#endif

char* duplicar_nombres_circuito_2[] = {
#include "recursos/pistas/metadatos/nombres_circuito.inc.c"
};

#if !ACTIVACION_PERSONALIZADO_CIRCUITO_MOTOR
char* nombres_circuito_depuracion[] = {
#include "recursos/pistas/metadatos/nombres_depuracion_circuito.inc.c"
};
#else

#endif

const s8 por_indice_copa_por_id_circuito[] = {
#include "recursos/pistas/metadatos/por_indice_copa_por_id_circuito.inc.c"
};

const s8 dato_800EFD64[] = { 0, 1, 4, 3, 5, 6, 2, 7 };

s8 seleccion_copa_por_id_circuito[] = {
#include "recursos/pistas/metadatos/seleccion_copa_por_id_circuito.inc.c"
};

char* texto_copa[] = {
    "none",
    "bronze",
    "silver",
    "gold",
};

char* nombres_personaje_depuracion[] = {
    "MARIO", "LUIGI", "YOSHI", "KINOPIO", "D.KONG", "WARIO", "PEACH", "KOOPA",
};

char* dato_800E76A8[] = {
    "MARIO",    "LUIGI", "YOSHI", "TOAD", "D.K.", "WARIO", "PEACH", "BOWSER",
    "ーーーー",
};

char* dato_800E76CC[] = {
    "50(",
    "100(",
    "150(",
    "extra",
};

char* dato_800E76DC[] = {
    "50(",
    "100(",
    "150(",
    "extra",
};

char* depuracion_pantalla_modo_nombres[] = {
    "1p", "2players UD", "2players LR", "3players", "4players",
};

char* depuracion_sonido_modo_nombres[] = {
    "stereo",
    "head phone",
    "xxx",
    "monaural",
};

char* sonido_nombres_modo[MODOS_SONIDO_NUM] = { "STEREO", "HEADPHONE", "", "MONO" };

char* texto_perder_victoria[] = {
    "WINNER!",
    "LOSER!",
};

char* texto_tiempo_mejor[] = {
    "BEST RECORDS",
    "BEST LAP",
};

char* texto_tiempo_vuelta = "LAP TIME";

char* texto_tiempo_prefijo[] = {
    "LAP 1",
    "LAP 2",
    "LAP 3",
    "TOTAL",
};

char* dato_800E7744[] = {
    "1 ｓ", "2 ｎ", "3 ｒ", "4 ｔ", "5 ｔ", " ",
};

char* boton_pausa_texto[] = {
    "CONTINUE GAME", "RETRY", "COURSE CHANGE", "DRIVER CHANGE", "QUIT", "REPLAY", "SAVE GHOST",
};

char* dato_800E7778[] = {
    "VS MATCH RANKING",
    "BATTLE RANKING",
};

char texto_menu_anuncio_fantasma[] = "NOW-MEET THE COURSE GHOST!!!";

char* mando_sin_texto[] = { "CONNECT A CONTROLLER TO SOCKET 1,", "THEN POWER ON AGAIN" };

char* introduccion_batalla_texto[] = {
    "BATTLE GAME",
    "POP OPPOSING PLAYER'S BALLOONS",
    "WHEN ALL 3 ARE GONE,THEY ARE OUT!",
};

char datos_menu_texto[] = "a BUTTON*SEE DATA  B BUTTON*EXIT";

char distancia_texto[] = "distance";

char* longitudes_circuito[] = {
#include "recursos/pistas/metadatos/longitudes_circuito.inc.c"
};

char* opcion_menu_texto[] = {
    "return to menu",
    "erase records for this course",
    "erase ghost from this course",
};

char* dato_800E7840[] = {
    "quit",
    "erase",
};

char* borrar_mejor_fantasma_texto[] = {
    "THE BEST RECORDS AND BEST", "LAP FOR THIS COURSE WILL BE", "ERASED.  IS THIS OK?",

    "GHOST DATA FOR THIS",       "COURSE WILL BE ERASED.",      "IS THIS OK?",
};

char* dato_800E7860[] = {
    "UNABLE TO ERASE ",
    "GHOST DATA",
};

char* menu_opcion_texto[] = {
    "RETURN TO GAME SELECT",
    "SOUND MODE",
    "COPY N64 CONTROLLER PAK",
    "ERASE ALL DATA",
};

char* dato_800E7878[] = {
    "ALL SAVED DATA WILL BE",
    "PERMANENTLY ERASED.",
    "ARE YOU REALLY SURE?",
};

char* dato_800E7884[] = {
    "",
    "ALL SAVED DATA",
    "HAS BEEN NOW ERASED.",
};

char* dato_800E7890[] = {
    "CONTROLLER 1 DOES NOT HAVE ",
    "N64 CONTROLLER PAK",
    "",
    "",

    "UNABLE TO READ ",
    "N64 CONTROLLER PAK DATA ",
    "FROM CONTROLLER 1",
    "",

    "UNABLE TO CREATE GAME DATA ",
    "FROM CONTROLLER 1 ",
    "N64 CONTROLLER PAK",
    "",

    "UNABLE TO COPY GHOST ",
    "-- INSUFFICIENT FREE PAGES ",
    "IN CONTROLLER 1 ",
    "N64 CONTROLLER PAK",
};

char* dato_800E78D0[] = {
    "NO GHOST DATA ",         "IN CONTROLLER 2 ",         "N64 CONTROLLER PAK",

    "NO MARIO KART 64 DATA ", "PRESENT IN CONTROLLER 2 ", "N64 CONTROLLER PAK",

    "CONTROLLER 2 ",          "DOES NOT HAVE ",           "N64 CONTROLLER PAK SET",

    "UNABLE TO READ DATA ",   "FROM CONTROLLER 2 ",       "N64 CONTROLLER PAK",
};

char* dato_800E7900[] = {
    "UNABLE TO COPY DATA ", "FROM CONTROLLER 1 ", "N64 CONTROLLER PAK",

    "UNABLE TO READ DATA ", "FROM CONTROLLER 2 ", "N64 CONTROLLER PAK",
};

char* dato_800E7918[] = {
    "CONTROLLER 1",
    "CONTROLLER 2",
};

char* dato_800E7920[] = {
    "WHICH FILE DO YOU WANT TO MAKE A COPY OF?",
    "TO WHICH FILE DO YOU WANT TO COPY?",
};

char* dato_800E7928[] = {
    "CURRENT DATA WILL BE ERASED,",
    "IS THIS OK?",
};

char* dato_800E7930[] = {
    "QUIT",
    "COPY",
};

char* dato_800E7938[] = {
    "COPYING",
    "DATA COPY COMPLETED",
};

char* dato_800E7940[] = {
    "NO N64 CONTROLLER PAK DETECTED",
    "TO SAVE GHOST DATA, ",
    "INSERT N64 CONTROLLER PAK ",
    "INTO CONTROLLER 1",

    "UNABLE TO READ ",
    "N64 CONTROLLER PAK DATA",
    "",
    "",

    "",
    "",
    "",
    "",

    "INSUFFICIENT FREE PAGES AVAILABLE ",
    "IN N64 CONTROLLER PAK TO CREATE ",
    "GAME DATA, PLEASE FREE 121 PAGES.",
    "SEE INSTRUCTION BOOKLET FOR DETAILS.",
};

char* dato_800E7980[] = {
    "TO SAVE GHOST DATA, ",
    "INSERT N64 CONTROLLER PAK ",
    "INTO CONTROLLER 1",
};

char* dato_800E798C[] = {
    "N64 CONTROLLER PAK ",
    "NOT DETECTED. ",
    "IF YOU WANT TO SAVE ",
    "THE GHOST DATA, ",
    "PLEASE INSERT ",
    "N64 CONTROLLER PAK ",
    "INTO CONTROLLER 1",

    "",
    "UNABLE TO SAVE ",
    "     THE GHOST",
    "",
    "",
    "",
    "",

    "",
    "UNABLE TO SAVE ",
    "     THE GHOST",
    "",
    "",
    "",
    "",

    "INSUFFICIENT ",
    "FREE PAGES AVAILABLE ",
    "",
    "-- GHOST DATA ",
    "COULD NOT BE SAVED",
    "",
    "",

    "",
    "CANNOT CREATE ",
    "     GAME DATA",
    "",
    "",
    "",
    "",

    "",
    "THIS GHOST IS ",
    "     ALREADY SAVED",
    "",
    "",
    "",
    "",
};

char* dato_800E7A34[] = {
    "RACE DATA CANNOT ",
    "BE SAVED FOR GHOST",
};

char* dato_800E7A3C[] = {
    "SELECT THE FILE ",
    "YOU WANT TO SAVE",
};

char* dato_800E7A44 = "NO DATA";

char* dato_800E7A48[] = {
    "CREATING ",
    "MARIO KART 64 ",
    "GAME DATA",
};

char* dato_800E7A54[] = {
    "CANNOT CREATE GAME DATA",
    "",
    "",
};

char* dato_800E7A60[] = {
    "THE PREVIOUS DATA ",
    "WILL BE ERASED, ",
    "IS THIS OK?",
};

char* dato_800E7A6C[] = {
    "QUIT",
    "SAVE",
};

char* dato_800E7A74[] = {
    "SAVING GHOST DATA",
    "",
    "PLEASE WAIT",
};

char* dato_800E7A80[] = {
    "UNABLE TO SAVE ",
    "THE GHOST",
};

char* dato_800E7A88[] = {
    "YOU ARE AWARDED THE",
    "GOLD CUP",
    "SILVER CUP",
    "BRONZE CUP",
};

char* dato_800E7A98 = "MAYBE NEXT TIME!";

char* dato_800E7A9C[] = {
    "CONGRATULATIONS!",
    "WHAT A PITY!",
};

char* texto_lugar[] = {
    "YOU PLACED", "    st", "    nd", "    rd", "    th", "    th", "    th", "    th", "    th",
};

const s8 premios_punto_gp[] = { 9, 6, 3, 1 };
const s8 dato_800F0B1C[] = {
    0, 0, 1, 0, 1, 0, 1, 2, 0, 1, 2, 3,
};
const s8 dato_800F0B28[] = {
    0, 1, 2, 1, 2, 1, 2, 1, 2, 0, 0, 1, 2, 2, 1, 2, 2, 1, 2, 2,
    1, 2, 2, 1, 2, 2, 1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
};

const s8 dato_800F0B50[] = { 0x1f, 0x0b, 0x15, 0x29 };
const s8 dato_800F0B54[] = { 0x20, 0x0f, 0x18, 0x2c };

RGBA16 dato_800E7AC8[] = {
    { 0x00, 0x00, 0x00, 0x00 },
    { 0xff, 0xff, 0xff, 0xff },
    { 0x00, 0x00, 0x50, 0xff },
    { 0xff, 0xff, 0xff, 0xff },
};

RGBA16 dato_800E7AE8[] = {
    { 0x00, 0x00, 0x00, 0xff },
    { 0xff, 0xff, 0xff, 0xff },
};

TexturaMenu* dato_800E7AF8[] = {
    dato_02000000, dato_02000028, dato_02000050, dato_02000078, dato_020000A0, dato_020000C8, dato_020000F0, dato_02000118, dato_02000140,
    dato_02000168, dato_02000190, dato_020001B8, dato_020001E0, dato_02000208, dato_02000230, dato_02000258, dato_02000280, dato_020002A8,
    dato_020002D0, dato_020002F8, dato_02000320, dato_02000348, dato_02000370, dato_02000398, dato_020003C0, dato_020003E8, dato_02000410,
    dato_02000438, dato_02000460, dato_02000488, dato_020004B0, dato_020004D8, dato_02000500, dato_02000528, dato_02000550, dato_02000578,
    dato_020005A0, dato_020005C8, dato_020005F0, dato_02000618, dato_02000640, dato_02000668, dato_02000690, dato_020006B8, dato_020006E0,
    dato_02000708, dato_02000730, dato_02000758, dato_02000780, dato_020007A8, dato_020007D0, dato_020007F8, dato_02000820, dato_02000848,
    dato_02000870, dato_02000898, dato_020008C0, dato_020008E8, dato_02000910, dato_02000938, dato_02000960, dato_02000988, dato_020009B0,
    dato_020009D8, dato_02000A00, dato_02000A28, dato_02000A50, dato_02000A78, dato_02000AA0, dato_02000AC8, dato_02000AF0, dato_02000B18,
    dato_02000B40, dato_02000B68, dato_02000B90, dato_02000BB8, dato_02000BE0, dato_02000C08, dato_02000C30, dato_02000C58, dato_02000C80,
    dato_02000CA8, dato_02000CD0, dato_02000CF8, dato_02000D20, dato_02000D48, dato_02000D70, dato_02000D98, dato_02000DC0, dato_02000DE8,
    dato_02000E10, dato_02000E38, dato_02000E60, dato_02000E88, dato_02000EB0, dato_02000ED8, dato_02000F00, dato_02000F28, dato_02000F50,
    dato_02000F78, dato_02000FA0, dato_02000FC8, dato_02000FF0, dato_02001018, dato_02001040, dato_02001068, dato_02001090, dato_020010B8,
    dato_020010E0, dato_02001108, dato_02001130, dato_02001158, dato_02001180, dato_020011A8, dato_020011D0, dato_020011F8, dato_02001220,
    dato_02001248, dato_02001270, dato_02001298, dato_020012C0, dato_020012E8, dato_02001310, dato_02001338, dato_02001360, dato_02001388,
    dato_020013B0, dato_020013D8, dato_02001400, dato_02001428, dato_02001450, dato_02001478, dato_020014A0,
};

TexturaMenu* dato_800E7D0C[] = {
    dato_020016BC, dato_020016E4, dato_0200170C, dato_02001734, dato_0200175C,
    dato_02001784, dato_020017AC, dato_020017D4, dato_020017FC, dato_02001824,
};

AnimacionMk* dato_800E7D34[] = {
    dato_0200198C, dato_0200199C, dato_020019AC, dato_020019BC, dato_020019CC, dato_020019DC,
};

TexturaMenu* fondo_texturas_menu[] = {
    seg2_azul_cielo_fondo_textura,
    seg2_atardecer_fondo_textura,
};

TexturaMenu* dato_800E7D54[] = {
    dato_02001A8C, dato_02001A64, dato_02001AB4, dato_02001A14, dato_02001B04, dato_020019EC, dato_02001ADC, dato_02001A3C,
};

TexturaMenu* dato_800E7D74[] = {
    seg2_mario_raceway_textura_vista_previa,
    dato_02001B54,
    dato_02001B7C,
    dato_02001BA4,
    dato_02001BCC,
    dato_02001BF4,
    dato_02001C1C,
    dato_02001C44,
    dato_02001C6C,
    dato_02001C94,
    dato_02001CBC,
    dato_02001CE4,
    dato_02001D0C,
    dato_02001D34,
    dato_02001D5C,
    dato_02001D84,
    dato_02001DAC,
    dato_02001DD4,
    dato_02001DFC,
    dato_02001E24,
};

TexturaMenu* dato_800E7DC4[] = {
    seg2_mario_raceway_textura_titulo,
    seg2_choco_mountain_textura_titulo,
    dato_02004EF8,
    dato_02004F20,
    dato_02004F48,
    dato_02004F70,
    dato_02004F98,
    dato_02004FC0,
    dato_02004FE8,
    dato_02005010,
    dato_02005038,
    dato_02005060,
    dato_02005088,
    dato_020050B0,
    dato_020050D8,
    dato_02005100,
    dato_02005128,
    dato_02005150,
    dato_02005178,
    dato_020051A0,
};

#ifdef AVOID_UB
AnimacionMk* dato_800E7E14[] = {
    dato_020020BC, dato_020020CC, dato_020020DC, dato_020020DC, dato_020020EC, dato_020020FC, dato_0200210C, dato_0200210C,
};
#else
AnimacionMk* dato_800E7E14[] = {
    dato_020020BC,
    dato_020020CC,
    dato_020020DC,
};

AnimacionMk* dato_800E7E20[] = {
    dato_020020DC, dato_020020EC, dato_020020FC, dato_0200210C, dato_0200210C,
};
#endif

AnimacionMk* dato_800E7E34[] = {
    dato_02001E64, dato_02001E74, dato_02001E84, dato_02001E94, dato_02001EA4, dato_02001EB4, dato_02001EC4,
    dato_02001ED4, dato_02001EE4, dato_02001EF4, dato_02001F04, dato_02001F14, dato_02001F24, dato_02001F34,
    dato_02001F44, dato_02001F54, dato_02001F64, dato_02001F74, dato_02001F84, dato_02001F94,
};

TexturaMenu* lut_textura_glifo[] = {
    seg_2_textura_fuente_letra_a,
    seg_2_textura_fuente_letra_b,
    seg_2_textura_fuente_letra_c,
    seg_2_textura_fuente_letra_d,
    seg_2_textura_fuente_letra_e,
    seg_2_textura_fuente_letra_f,
    seg_2_textura_fuente_letra_g,
    seg_2_textura_fuente_letra_h,
    seg_2_textura_fuente_letra_i,
    seg_2_textura_fuente_letra_j,
    seg_2_textura_fuente_letra_k,
    seg_2_textura_fuente_letra_l,
    seg_2_textura_fuente_letra_m,
    seg_2_textura_fuente_letra_n,
    seg_2_textura_fuente_letra_o,
    seg_2_textura_fuente_letra_p,
    seg_2_textura_fuente_letra_q,
    seg_2_textura_fuente_letra_r,
    seg_2_textura_fuente_letra_s,
    seg_2_textura_fuente_letra_t,
    seg_2_textura_fuente_letra_u,
    seg_2_textura_fuente_letra_v,
    seg_2_textura_fuente_letra_w,
    seg_2_textura_fuente_letra_x,
    seg_2_textura_fuente_letra_y,
    seg_2_textura_fuente_letra_z,
    seg_2_textura_fuente_signo_exclamacion,
    seg_2_textura_fuente_menos,
    seg_2_textura_fuente_pregunta_marcar,
    seg_2_textura_fuente_simple_comilla,
    seg_2_textura_fuente_vacio,
    seg_2_textura_fuente_punto,
    seg_2_textura_fuente_numero_cero,
    seg_2_textura_fuente_numero_uno,
    seg_2_textura_fuente_numero_dos,
    seg_2_textura_fuente_numero_tres,
    seg_2_textura_fuente_numero_cuatro,
    seg_2_textura_fuente_numero_cinco,
    seg_2_textura_fuente_numero_seis,
    seg_2_textura_fuente_numero_siete,
    seg_2_textura_fuente_numero_ocho,
    seg_2_textura_fuente_numero_nueve,
    seg_2_textura_fuente_doble_comilla,
    seg_2_textura_fuente_cuatro_dote,
    seg_2_textura_fuente_mas,
    seg_2_textura_fuente_cc,
    seg_2_textura_fuente_coma,
    seg_2_textura_fuente_vacio,
    dato_02002824,
    dato_0200284C,
    dato_02002874,
    dato_0200289C,
    dato_020028C4,
    dato_020028EC,
    dato_02002F54,
    dato_02002914,
    dato_02002F7C,
    dato_0200293C,
    dato_02002FA4,
    dato_02002964,
    dato_02002FCC,
    dato_0200298C,
    dato_02002FF4,
    dato_020029B4,
    dato_0200301C,
    dato_020029DC,
    dato_02003044,
    dato_02002A04,
    dato_0200306C,
    dato_02002A2C,
    dato_02003094,
    dato_02002A54,
    dato_020030BC,
    dato_02002A7C,
    dato_020030E4,
    dato_02002AA4,
    dato_0200310C,
    dato_020033B4,
    dato_02002ACC,
    dato_02003134,
    dato_02002AF4,
    dato_0200315C,
    dato_02002B1C,
    dato_02003184,
    dato_02002B44,
    dato_02002B6C,
    dato_02002B94,
    dato_02002BBC,
    dato_02002BE4,
    dato_02002C0C,
    dato_020031AC,
};

TexturaMenu* dato_800E7FF0[] = {
    dato_02003274, dato_02002C34, dato_020031D4, dato_0200329C, dato_02002C5C, dato_020031FC, dato_020032C4, dato_02002C84, dato_02003224,
    dato_020032EC, dato_02002CAC, dato_0200324C, dato_02003314, dato_02002CD4, dato_02002CFC, dato_02002D24, dato_02002D4C, dato_02002D74,
    dato_0200333C, dato_02002D9C, dato_02003364, dato_02002DC4, dato_0200338C, dato_02002DEC, dato_02002E14, dato_02002E3C, dato_02002E64,
    dato_02002E8C, dato_02002EB4, dato_02002EDC, dato_02002F04, dato_02002F2C, dato_020033DC, dato_02003404, dato_0200342C, dato_02003454,
    dato_0200347C, dato_020034A4, dato_020034CC, dato_020034F4, dato_0200351C, dato_02003544, dato_0200356C, dato_02003BD4,
};

TexturaMenu* dato_800E80A0[] = {
    dato_02003594, dato_02003BFC, dato_020035BC, dato_02003C24, dato_020035E4, dato_02003C4C, dato_0200360C, dato_02003C74,
    dato_02003634, dato_02003C9C, dato_0200365C, dato_02003CC4, dato_02003684, dato_02003CEC, dato_020036AC, dato_02003D14,
    dato_020036D4, dato_02003D3C, dato_020036FC, dato_02003D64, dato_02003724, dato_02003D8C, dato_02004034, dato_0200374C,
    dato_02003DB4, dato_02003774, dato_02003DDC, dato_0200379C, dato_02003E04,
};

TexturaMenu* dato_800E8114[] = {
    dato_020037C4, dato_020037EC, dato_02003814, dato_0200383C, dato_02003864, dato_0200388C, dato_02003E2C, dato_02003EF4,
    dato_020038B4, dato_02003E54, dato_02003F1C, dato_020038DC, dato_02003E7C, dato_02003F44, dato_02003904, dato_02003EA4,
    dato_02003F6C, dato_0200392C, dato_02003ECC, dato_02003F94, dato_02003954, dato_0200397C, dato_020039A4, dato_020039CC,
};

TexturaMenu* dato_800E8174[] = {
    dato_020039F4,
    dato_02003FBC,
};

TexturaMenu* dato_800E817C[] = {
    dato_02003A1C, dato_02003FE4, dato_02003A44, dato_0200400C, dato_02003A6C, dato_02003A94, dato_02003ABC, dato_02003AE4, dato_02003B0C,
    dato_02003B34, dato_02003B5C, dato_02003B84, dato_02003BAC, dato_0200405C, dato_02004084, dato_020040AC, dato_020040D4, dato_020040FC,
    dato_020043CC, dato_02004444, dato_0200437C, dato_020043F4, dato_02004124, dato_0200414C, dato_02004174, dato_0200419C,
};

TexturaMenu* dato_800E81E4[] = {
    dato_020041C4, dato_020041EC, dato_02004214, dato_0200423C, dato_02004264, dato_0200428C, dato_020042B4, dato_020042DC, dato_02004354,
    dato_020043A4, dato_0200441C, dato_0200446C, dato_02004494, dato_020044BC, dato_02004304, dato_0200432C, dato_020044E4, dato_0200450C,
};
