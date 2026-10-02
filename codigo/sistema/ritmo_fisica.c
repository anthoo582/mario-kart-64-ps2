#include <ultra64.h>

#include "sistema/sistema_ps2.h"

#define VBLANK_US   16683u
#define US_MARGEN   4000u
#define FRAMES_ABAJO 90
/* Un paso de fisica por retrazo: si la imagen tarda mas retrazos que pasos, el juego va en camara
   lenta. Se admite un paso mas que el original (hasta 4, lo que usa DK Jungle en 4 jugadores) para
   que la carrera siga a velocidad real aunque baje la cantidad de imagenes. */
#define PASOS_MAX 4

static s32 pasos;        /* pasos elegidos (0: aun sin elegir) */
static s32 s_original;     /* los del juego para esta pista y modo */
static u32 good_frames;   /* imagenes seguidas en que cabria un paso menos */
static u8 late_hist;      /* 1 bit por imagen: duro mas de lo previsto */
static u32 samp_total, inactivo_samp;

s32 ps2_ritmo_pasos(s32 original)
{
    if (original != s_original || pasos == 0) {
        /* Otra pista o modo */
        s_original = original;
        pasos = original;
        good_frames = 0;
        late_hist = 0;
    }
    return pasos;
}

/* Al terminar cada imagen de la carrera: retrazos que duro. */
void ps2_ritmo_imagen(u32 periodo)
{
    u32 total, inactivo, dt, di, us_ocupado;

    cantidades_muestreador_ps2(&total, &inactivo);
    dt = total - samp_total;
    di = inactivo - inactivo_samp;
    samp_total = total;
    inactivo_samp = inactivo;
    if (pasos == 0 || periodo == 0) {
        return;
    }
    late_hist = (u8) ((late_hist << 1) | (periodo > (u32) pasos ? 1 : 0));
    if (__builtin_popcount(late_hist) >= 2 && pasos < (s_original < PASOS_MAX ? s_original + 1 : PASOS_MAX)) {
        pasos++;
        late_hist = 0;
        good_frames = 0;
        rend_registro_ps2("ritmo 3-4P: %d pasos por imagen (no llegaba)", (int) pasos);
        return;
    }
    if (pasos <= 2 || dt < 4 || di > dt) {
        good_frames = 0;
        return;
    }
    us_ocupado = (u32) ((u64) periodo * VBLANK_US * (dt - di) / dt);
    if (us_ocupado + US_MARGEN <= (u32) (pasos - 1) * VBLANK_US) {
        if (++good_frames >= FRAMES_ABAJO) {
            pasos--;
            good_frames = 0;
            late_hist = 0;
            rend_registro_ps2("ritmo 3-4P: %d pasos por imagen (ocupado %u us)", (int) pasos, (unsigned) us_ocupado);
        }
    } else {
        good_frames = 0;
    }
}
