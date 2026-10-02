#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <ultra64.h>
#include <PR/gbi.h>
#include <gsKit.h>

#include "graficos/combinador_color.h"
#include "graficos/interprete_f3dex.h"
#include "graficos/sintetizador_gs.h"
#include "sistema/sistema_ps2.h"
#include "graficos/memoria_texturas.h"
#include "graficos/pantallas_gigantes.h"
#include "sistema/perfilado.h"
#include "sistema/cronometro_fases.h"
#include <juego/definiciones.h>
#include "sistema/bucle_principal.h" /* struct GfxPool: direcciones del pool de graficos */
#include "depuracion/guiones_prueba.h"
#ifdef SMK64_MEDIDOR
#include "depuracion/medidor_rendimiento.h"
#endif
/* Diagnostico del combinador (diagnostico_cc.inc.c) */
#ifdef SMK64_DIAG_CC
struct VtxColor;
static u32 diag_timg_seg;
static int diag_es_rect, diag_blanco;
static void diag_combinador(const u8 sh[4], const struct VtxColor *o);
#define DIAG_CC(sh, o) diag_combinador(sh, o)
#define DIAG_CC_ESTADO(rect, blanco) (diag_es_rect = (rect), diag_blanco = (blanco))
#define DIAG_CC_TIMG(w1) (diag_timg_seg = (w1))
#else
#define DIAG_CC(sh, o) ((void) 0)
#define DIAG_CC_ESTADO(rect, blanco) ((void) 0)
#define DIAG_CC_TIMG(w1) ((void) 0)
#endif

#include "interprete_f3dex/matrices_y_vertices.inc.c"
#include "interprete_f3dex/combinador_y_recorte.inc.c"
#include "interprete_f3dex/cache_matrices_y_ordenes.inc.c"
#include "interprete_f3dex/listas_dibujo.inc.c"
#include "interprete_f3dex/tarea_graficos.inc.c"
#include "interprete_f3dex/diagnostico_cc.inc.c"