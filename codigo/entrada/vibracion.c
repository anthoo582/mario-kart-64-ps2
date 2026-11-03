#include <ultra64.h>
#include <string.h>
#include <juego/macros.h>
#include <juego/mk64.h>
#include <juego/definiciones.h>
#include "sistema/bucle_principal.h"
#include "carrera/preparacion_carrera.h"
#include "entrada/vibracion.h"

/* Un golpe de vibracion: al activarse alguno de los efectos, los motores se encienden unos frames. */
typedef struct {
    u32 efectos;
    u8 grande;
    u8 pequeno;
    u8 frames;
} GolpeVibracion;

/* De mas fuerte a mas flojo: un golpe nuevo solo sustituye a otro si es al menos igual de fuerte. */
static const GolpeVibracion golpes[] = {
    { EFECTO_APLASTAMIENTO, 255, 1, 30 },
    { EFECTO_VUELCO_TERRENO | GOLPE_POR_EFECTO_ESTRELLA | EFECTO_ERROR_EXPLOSION, 255, 0, 28 },
    { EFECTO_GOLPE_RAYO, 200, 1, 24 },
    { EFECTO_TROMPO_BANANA | EFECTO_TROMPO_CONDUCIENDO | TEMPRANO_INICIO_TROMPO_EFECTO, 150, 0, 18 },
    { EFECTO_HONGO | IMPULSO_RAMPA_ASFALTO_EFECTO | IMPULSO_RAMPA_MADERA_EFECTO, 0, 1, 15 },
    { MINI_EFECTO_TURBO, 0, 1, 8 },
};

/* Aterrizaje tras un salto */
#define ATERRIZAJE_GRANDE 90
#define ATERRIZAJE_FRAMES 5

/* Choque con otro kart */
#define CHOQUE_GRANDE 170
#define CHOQUE_FRAMES 10

/* Fuera de la pista (pasto, arena o tierra de fuera, nieve blanda): vibracion suave mientras dure.
   Base mas un poco por cada rueda fuera. Por debajo de ~60 el motor grande apenas gira. */
#define TERRENO_GRANDE_BASE 50
#define TERRENO_GRANDE_RUEDA 10
#define TERRENO_RAPIDEZ_MIN 1.0f

static u32 efectos_ant[MAXCONTROLLERS];
static u8 fuerza_grande[MAXCONTROLLERS];
static u8 fuerza_pequeno[MAXCONTROLLERS];
static u8 frames_restantes[MAXCONTROLLERS];
static u8 choque_pendiente[MAXCONTROLLERS];

void vibracion_choque_jugador(Jugador *jugador)
{
    s32 indice = jugador - jugadores;

    if (indice >= 0 && indice < MAXCONTROLLERS) {
        choque_pendiente[indice] = 1;
    }
}

static void empezar_golpe(int jugador, u8 grande, u8 pequeno, u8 frames)
{
    if (frames_restantes[jugador] != 0 && grande < fuerza_grande[jugador]) {
        return;
    }
    fuerza_grande[jugador] = grande;
    fuerza_pequeno[jugador] = pequeno;
    frames_restantes[jugador] = frames;
}

static int es_fuera_pista(u8 superficie)
{
    switch (superficie) {
        case PASTO:
        case FUERA_PISTA_ARENA:
        case FUERA_PISTA_NIEVE:
        case FUERA_PISTA_TIERRA:
            return 1;
        default:
            return 0;
    }
}

static u8 vibracion_terreno(Jugador *j)
{
    s32 ruedas_fuera = 0;
    s32 i;

    if ((j->efectos & EFECTO_EN_EL_AIRE) || j->speed < TERRENO_RAPIDEZ_MIN) {
        return 0;
    }
    for (i = 0; i < 4; i++) {
        ruedas_fuera += es_fuera_pista(j->ruedas[i].tipo_superficie);
    }
    if (ruedas_fuera == 0) {
        return 0;
    }
    return TERRENO_GRANDE_BASE + ruedas_fuera * TERRENO_GRANDE_RUEDA;
}

static int vibracion_activa(void)
{
    /* Tras la carrera (resultados, repeticion de la meta) no vibra */
    return estado_juego == CARRERA && estado_carrera < HECHO_CARRERA && !juego_en_pausa && !modo_demo;
}

void calcular_vibracion(u8 pequeno[MAXCONTROLLERS], u8 grande[MAXCONTROLLERS])
{
    int jugador;

    memset(pequeno, 0, MAXCONTROLLERS);
    memset(grande, 0, MAXCONTROLLERS);

    if (!vibracion_activa()) {
        memset(efectos_ant, 0, sizeof(efectos_ant));
        memset(frames_restantes, 0, sizeof(frames_restantes));
        memset(choque_pendiente, 0, sizeof(choque_pendiente));
        return;
    }

    /* Los humanos son siempre los primeros jugadores y el jugador N usa el mando N.
       Al cruzar la meta el juego pasa al humano a CPU_JUGADOR: desde ahi ya no conduce el. */
    for (jugador = 0; jugador < MAXCONTROLLERS; jugador++) {
        Jugador *j = &jugadores[jugador];
        u32 nuevos;
        s32 i;

        if (jugador >= seleccion_cantidad_jugador_1 ||
            (j->type & (EXISTE_JUGADOR | HUMANO_JUGADOR | CPU_JUGADOR)) != (EXISTE_JUGADOR | HUMANO_JUGADOR)) {
            efectos_ant[jugador] = 0;
            frames_restantes[jugador] = 0;
            choque_pendiente[jugador] = 0;
            continue;
        }

        nuevos = j->efectos & ~efectos_ant[jugador];
        for (i = 0; i < CANTIDAD_ARREGLO(golpes); i++) {
            if (nuevos & golpes[i].efectos) {
                empezar_golpe(jugador, golpes[i].grande, golpes[i].pequeno, golpes[i].frames);
                break;
            }
        }
        if ((efectos_ant[jugador] & EFECTO_EN_EL_AIRE) && !(j->efectos & EFECTO_EN_EL_AIRE)) {
            empezar_golpe(jugador, ATERRIZAJE_GRANDE, 0, ATERRIZAJE_FRAMES);
        }
        /* El choque se repite cada frame mientras los karts se tocan: solo cuenta si no vibra ya */
        if (choque_pendiente[jugador] && frames_restantes[jugador] == 0) {
            empezar_golpe(jugador, CHOQUE_GRANDE, 0, CHOQUE_FRAMES);
        }
        choque_pendiente[jugador] = 0;
        efectos_ant[jugador] = j->efectos;

        grande[jugador] = vibracion_terreno(j);
        if (frames_restantes[jugador] != 0) {
            frames_restantes[jugador]--;
            if (fuerza_grande[jugador] > grande[jugador]) {
                grande[jugador] = fuerza_grande[jugador];
            }
            pequeno[jugador] = fuerza_pequeno[jugador];
        }
    }
}
