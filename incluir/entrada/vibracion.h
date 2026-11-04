#ifndef ENTRADA_VIBRACION_H
#define ENTRADA_VIBRACION_H

#include <ultra64.h>
#include <juego/estructuras_comunes.h>

/* La N64 no tenia vibracion en MK64: se saca de los efectos de cada jugador humano.
   Se llama una vez por frame de juego y deja la fuerza de los dos motores del DualShock
   (pequeno: 0/1, grande: 0-255) de cada jugador. */
void calcular_vibracion(u8 pequeno[MAXCONTROLLERS], u8 grande[MAXCONTROLLERS]);

/* Choque entre karts: el efecto que marca el juego se borra en el mismo frame y no se puede sondear. */
void vibracion_choque_jugador(Jugador *jugador);

#endif
