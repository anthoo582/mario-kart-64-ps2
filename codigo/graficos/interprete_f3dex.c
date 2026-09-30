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
#include "sistema/bucle_principal.h" /* struct GfxPool: direcciones del pool de graficos */
#include "depuracion/guiones_prueba.h"
#ifdef SMK64_MEDIDOR
#include "depuracion/medidor_rendimiento.h"
#endif
#include "interprete_f3dex/matrices_y_vertices.inc.c"
#include "interprete_f3dex/combinador_y_recorte.inc.c"
#include "interprete_f3dex/cache_matrices_y_ordenes.inc.c"
#include "interprete_f3dex/listas_dibujo.inc.c"
#include "interprete_f3dex/tarea_graficos.inc.c"