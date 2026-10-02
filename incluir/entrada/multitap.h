#ifndef ENTRADA_MULTITAP_H
#define ENTRADA_MULTITAP_H

/* Conectores fisicos de la PS2: 2 puertos con 4 ranuras cada uno si hay multitap. */
#define PUERTOS_MANDO_PS2 2
#define RANURAS_MULTITAP 4

/* Carga los modulos de mandos del IOP. Prueba primero la familia X de la BIOS (XSIO2MAN, XPADMAN,
   XMTAPMAN), que es la que maneja el multitap; si no esta (BIOS muy antiguas) usa SIO2MAN y PADMAN.
   Devuelve 1 si PADMAN o XPADMAN quedaron residentes. */
int cargar_modulos_mando_ps2(void);

/* 1 si se cargo la familia X: la Memory Card tiene que usar XMCMAN y XMCSERV. */
int modulos_mando_x_ps2(void);

/* Abre el multitap de los dos puertos y lee si esta conectado. */
void inicializar_multitap_ps2(void);

/* Vuelve a mirar si se conecto o se quito un multitap (cada segundo). Devuelve 1 si cambio algo. */
int vigilar_multitap_ps2(void);

int multitap_conectado_ps2(int puerto);

/* Conector fisico que maneja al jugador (0-3). 0 si ese jugador no tiene conector. */
int conector_jugador_ps2(int jugador, int *puerto, int *ranura);

#endif
