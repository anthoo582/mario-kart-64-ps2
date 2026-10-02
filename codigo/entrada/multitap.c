#include <kernel.h>
#include <libmtap.h>

#include <ultra64.h>

#include "sistema/sistema_ps2.h"
#include "entrada/multitap.h"

static int familia_x;
static int mtap_cargado; /* sin XMTAPMAN, mtapInit() esperaria su servidor RPC para siempre */
static int mtap_abierto[PUERTOS_MANDO_PS2];
static int mtap_conectado[PUERTOS_MANDO_PS2];
static u32 ultima_vigilancia;

int cargar_modulos_mando_ps2(void)
{
    /* La familia X va junta: XPADMAN y XMTAPMAN necesitan XSIO2MAN, y no se mezcla con SIO2MAN. */
    if (cargar_modulo_iop("rom0:XSIO2MAN")) {
        familia_x = 1;
        if (!cargar_modulo_iop("rom0:XPADMAN")) {
            return 0;
        }
        mtap_cargado = cargar_modulo_iop("rom0:XMTAPMAN");
        if (!mtap_cargado) {
            registrar("mandos: sin XMTAPMAN, solo un mando por puerto");
        }
        return 1;
    }
    familia_x = 0;
    registrar("mandos: la BIOS no tiene XSIO2MAN, sin multitap");
    return cargar_modulo_iop("rom0:SIO2MAN") && cargar_modulo_iop("rom0:PADMAN");
}

int modulos_mando_x_ps2(void)
{
    return familia_x;
}

void inicializar_multitap_ps2(void)
{
    int puerto;

    if (!mtap_cargado || mtapInit() != 1) {
        return;
    }
    for (puerto = 0; puerto < PUERTOS_MANDO_PS2; puerto++) {
        mtap_abierto[puerto] = mtapPortOpen(puerto) == 1;
        mtap_conectado[puerto] = mtap_abierto[puerto] && mtapGetConnection(puerto) == 1;
        registrar("multitap en el puerto %d: %s", puerto + 1,
                  mtap_conectado[puerto] ? "conectado" : (mtap_abierto[puerto] ? "no" : "sin servicio"));
    }
    ultima_vigilancia = contador_vblank();
}

int vigilar_multitap_ps2(void)
{
    int puerto, cambio = 0;
    u32 ahora = contador_vblank();

    if (ahora - ultima_vigilancia < 60) {
        return 0;
    }
    ultima_vigilancia = ahora;
    for (puerto = 0; puerto < PUERTOS_MANDO_PS2; puerto++) {
        int c;

        if (!mtap_abierto[puerto]) {
            continue;
        }
        c = mtapGetConnection(puerto) == 1;
        if (c != mtap_conectado[puerto]) {
            mtap_conectado[puerto] = c;
            cambio = 1;
            registrar("multitap en el puerto %d: %s", puerto + 1, c ? "conectado" : "quitado");
        }
    }
    return cambio;
}

int multitap_conectado_ps2(int puerto)
{
    return mtap_conectado[puerto];
}

/* Asignacion fija por enchufe, como en los juegos de PS2 con multitap: cada jugador conserva su
   conector aunque otro mando se desconecte.
     multitap en el puerto 1:  J1-J4 = 1A, 1B, 1C, 1D
     multitap solo en el 2:    J1 = 1,  J2-J4 = 2A, 2B, 2C
     sin multitap:             J1 = 1,  J2 = 2 */
int conector_jugador_ps2(int jugador, int *puerto, int *ranura)
{
    if (jugador < 0 || jugador >= RANURAS_MULTITAP) {
        return 0;
    }
    if (mtap_conectado[0]) {
        *puerto = 0;
        *ranura = jugador;
        return 1;
    }
    if (jugador == 0) {
        *puerto = 0;
        *ranura = 0;
        return 1;
    }
    if (mtap_conectado[1]) {
        *puerto = 1;
        *ranura = jugador - 1;
        return 1;
    }
    if (jugador == 1) {
        *puerto = 1;
        *ranura = 0;
        return 1;
    }
    return 0;
}
