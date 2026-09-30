#ifndef SISTEMA_SISTEMA_PS2_H
#define SISTEMA_SISTEMA_PS2_H

#include <ultra64.h>

void inicializar_hilos(void);
int prioridad_ee(OSPri prio);
OSThread *hilo_actual(void);
void enviar_evento_sistema(OSEvent e);
s32 id_hilo_ee_de(OSId id);

/* muestreo de la CPU (1 kHz, en todas las builds */
#define HILOS_MUESTREO 32
typedef struct {
    u32 total, inactivo;
    u32 thread[HILOS_MUESTREO];
} MuestrasCpu;
void registrar_bucle_inactivo(u32 empezar, u32 largo);
void leer_muestras_cpu(MuestrasCpu *salida);
void cantidades_muestreador_ps2(u32 *total, u32 *inactivo);

/* retrazo_vertical.c: VBlank, temporizadores y presentacion de frames */
void inicializar_retrazo(void);
u32 contador_vblank(void);
void avanzar_temporizadores(void);
void fijar_evento_audio_vi_os_ps2(OSMesgQueue *mq, OSMesg mens);

void inicializar_hardware_libultra(void);
void guardar_segmentos_iniciales(void);
u64 tiempo_actual(void);

void inicializar_mandos_ps2(void);
void leer_mandos_ps2(void);

/* arranque_ps2.c: 1 si el modulo quedo residente en el IOP. Sin el, la libreria
   del EE que lo usa se queda esperando su servidor RPC para siempre. */
int cargar_modulo_iop(const char *camino);
int ejecutar_modulo_iop(const char *nombre, void *irx, u32 tamanio);

#ifndef PS2_BUILD_ID
#define PS2_BUILD_ID "sin-version"
#endif

/* depuracion.c: consola en pantalla y registro */
void registrar(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
/* Lineas periodicas de perfilado */
void rend_registro_ps2(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
/* 1 desde que se muestra la pantalla de fallo */
extern volatile int en_panico_ps2;
int en_panico_depuracion_ps2(void);
void detener_por_error(const char *mens) __attribute__((noreturn));
void inicializar_depuracion(void);
void marcar_punto_control(const char *where);
void volcar_frame(const char *nombre, u32 direccion_vram, u32 ancho, u32 altura, u32 psm);
/* E/S de host */
void bloquear_host(void);
void desbloquear_host(void);
/* El DMA del GIF no termina */
void detener_por_gif_trabado(const char *por_que) __attribute__((noreturn));

/* ROM embebida (generada por el build, ver tools/ps2/) */
extern u8 __rom_start[];
extern u8 __rom_end[];

int inicializar_rom_ps2(const char *camino_arranque);
void esperar_rom_ps2(const void *direccion, u32 size);
void progreso_rom_ps2(u32 *cargado_2, u32 *total);
extern volatile int esperando_rom_ps2;

#endif
