#ifndef GRAFICOS_PANTALLAS_GIGANTES_H
#define GRAFICOS_PANTALLAS_GIGANTES_H

#include "memoria_texturas.h"

void copiar_pantalla_gigante(int x, int y, int w, int h, const void *objetivo);
void reiniciar_pantallas_gigantes(void);

/* Renderizador: al empezar cada frame, antes de dibujar. */
void empezar_frame_pantallas_gigantes(void);
/* Renderizador: al terminar de dibujar el frame (en hires las copias van aqui). */
void terminar_frame_pantallas_gigantes(void);
int buscar_pantalla_gigante(const void *orig_, InfoTextura *salida);

#define BYTES_VRAM_PANTALLAS_GIGANTES (128 * 1024)

#endif
