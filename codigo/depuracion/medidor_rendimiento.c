#include <stdio.h>
#include <string.h>

#include <ultra64.h>

#include "graficos/sintetizador_gs.h"
#include "sistema/sistema_ps2.h"
#include "sistema/perfilado.h"
#include "sistema/cronometro_fases.h"
#include "graficos/memoria_texturas.h"
#include "depuracion/estadisticas_memoria.h"
#include "depuracion/texto_pantalla.h"
#include "depuracion/medidor_rendimiento.h"

#define VBLANKS_POR_VENTANA 60
#define VBLANKS_POR_SEGUNDO_X100 5994u
#define BYTES_BUCLE_INACTIVO 64u

#define BOTONES_CAMBIO_PANEL 0x0006u

#define MEDIDOR_LINEAS 12

/* Salida de audsrv */
#define AUDIO_BYTES_POR_MS 192u

/* Con DEBUG_AUDIO hay una pagina mas */
enum { PANEL_OCULTO, PANEL_LINEA, PANEL_COMPLETO, PANEL_AUDIO };
#ifdef SMK64_DEBUG_AUDIO
#include "depuracion/monitor_audio.h"
#define PANEL_ULTIMO PANEL_AUDIO
#else
#define PANEL_ULTIMO PANEL_COMPLETO
#endif

typedef struct {
    u32 frames;
    u32 vblank_inicio;
    u32 ciclos_inicio;
    MuestrasCpu muestras_inicio;
    u32 render_ciclos;
    u32 ciclos_espera_gs;
    u32 ciclos_espera_dma;
    u32 ciclos_audio;
    u32 periodo_maximo;
    u32 envios;
    u32 bytes_paquete;
    u32 subidas;
    u32 bytes_subidas;
    u32 triangulos;
    u32 rectangulos;
    u32 cola_audio_minima;
    u32 secciones_inicio[CANTIDAD_PROF]; /* ciclos_prof al empezar */
} Ventana;

typedef struct {
    u32 frames_totales;
    u32 vblanks_totales;
    u32 fps_minimo;
    u32 fps_maximo;
    int ventanas_cerradas;
    u32 cortes_audio;
} Acumulado;

extern s32 estado_juego;

static int iniciado;
static int modo_panel = PANEL_COMPLETO;
static u32 botones_anteriores;
static s32 escena_anterior = -1;
static int reiniciar_pendiente;

static Ventana ventana;
static Acumulado acumulado;
static u32 inicio_frame;
static u32 anterior_inicio_frame;
static u32 espera_gs_frame;
static u32 espera_dma_frame;
static volatile u32 ciclos_audio_total;
static u32 audio_inicio_frame;

static TexturaTexto texto;
static int texto_rendimiento; /* la textura tiene el panel de rendimiento (no el de audio) */
#ifdef SMK64_DEV
static u32 ventanas_registro;
#endif

void medidor_sumar_espera_gs(u32 ciclos)
{
    espera_gs_frame += ciclos;
}

void medidor_sumar_espera_dma(u32 ciclos)
{
    espera_dma_frame += ciclos;
}

void medidor_contar_envio_dma(void)
{
    ventana.envios++;
}

void medidor_sumar_audio(u32 ciclos)
{
    ventana.ciclos_audio += ciclos;
    ciclos_audio_total += ciclos;
}

/* Lo llama el hilo de audio */
void medidor_cola_audio(u32 bytes_encolados)
{
    static int audio_empezado;

    audio_empezado |= bytes_encolados > 0;
    if (bytes_encolados < ventana.cola_audio_minima) {
        ventana.cola_audio_minima = bytes_encolados;
    }
    if (audio_empezado && bytes_encolados == 0) {
        acumulado.cortes_audio++;
    }
}

void medidor_entrada_mando(u32 pulsados)
{
    int combinacion = (pulsados & BOTONES_CAMBIO_PANEL) == BOTONES_CAMBIO_PANEL;
    int antes = (botones_anteriores & BOTONES_CAMBIO_PANEL) == BOTONES_CAMBIO_PANEL;

    if (combinacion && !antes) {
        modo_panel = (modo_panel == PANEL_OCULTO) ? PANEL_ULTIMO : modo_panel - 1;
        if (modo_panel == PANEL_COMPLETO) {
            reiniciar_pendiente = 1;
        }
    }
    botones_anteriores = pulsados;
}

void medidor_bucle_inactivo(void)
{
    registrar_bucle_inactivo((u32) medidor_bucle_inactivo, BYTES_BUCLE_INACTIVO);
    for (;;) {
    }
}

static void empezar_ventana(u32 ahora)
{
    memset(&ventana, 0, sizeof(ventana));
    ventana.cola_audio_minima = ~0u;
    ventana.vblank_inicio = contador_vblank();
    ventana.ciclos_inicio = ahora;
    leer_muestras_cpu(&ventana.muestras_inicio);
    memcpy(ventana.secciones_inicio, ciclos_prof, sizeof(ventana.secciones_inicio));
}

static u32 ciclos_seccion(int seccion)
{
    u32 ahora = ciclos_prof[seccion], antes = ventana.secciones_inicio[seccion];

    return ahora >= antes ? ahora - antes : ahora;
}

static void reiniciar_acumulado(void)
{
    memset(&acumulado, 0, sizeof(acumulado));
}

/* Milesimas de ms -> "1234" ms, o "-" si no hay medida. */
static void ms_o_guion(char *destino, u32 tam, int hay, u32 us)
{
    if (hay) {
        snprintf(destino, tam, "%u", (unsigned) ((us + 500) / 1000));
    } else {
        snprintf(destino, tam, "-");
    }
}

/* Decimas: 123 -> "12.3". */
static void decimas(char *destino, u32 tam, u32 valor)
{
    snprintf(destino, tam, "%u.%u", (unsigned) (valor / 10), (unsigned) (valor % 10));
}

static u32 ciclos_a_decimas_ms(u32 ciclos, u32 divisor)
{
    if (divisor == 0) {
        return 0;
    }
    return (u32) ((u64) ciclos * 10 / MEDIDOR_CICLOS_POR_MS / divisor);
}

static u32 porcentaje(u32 parte, u32 total)
{
    if (total == 0) {
        return 0;
    }
    return (u32) ((u64) parte * 100 / total);
}

static u32 porcentaje_hilo(const MuestrasCpu *ahora, const MuestrasCpu *antes, OSId id, u32 total)
{
    s32 hilo = id_hilo_ee_de(id);

    if (hilo <= 0 || hilo >= HILOS_MUESTREO) {
        return 0;
    }
    return porcentaje(ahora->thread[hilo] - antes->thread[hilo], total);
}

static u32 color_fps(u32 fps_x_100)
{
    /* La logica del juego va a 30 Hz como maximo */
    if (fps_x_100 >= 2950) {
        return TEXTO_VERDE;
    }
    return (fps_x_100 >= 2500) ? TEXTO_AMARILLO : TEXTO_ROJO;
}

static void cerrar_ventana(u32 ahora, u32 vblank_ahora)
{
    Ventana *v = &ventana;
    u32 vblanks = vblank_ahora - v->vblank_inicio;
    u32 transcurrido = ahora - v->ciclos_inicio;
    u32 fps = (vblanks != 0) ? v->frames * VBLANKS_POR_SEGUNDO_X100 / vblanks : 0;
    MuestrasCpu muestras;
    u32 total, pct_libre, pct_cpu, pct_video, pct_juego, pct_audio, pct_otros, suma_hilos;
    u32 rom_cargados, rom_total, arranque_us, carga_us, largo_us, largo_arranque_us;
    const char *paso_largo, *paso_largo_arranque;
    int hay_arranque, hay_carga;

    leer_muestras_cpu(&muestras);
    total = muestras.total - v->muestras_inicio.total;
    pct_libre = porcentaje(muestras.inactivo - v->muestras_inicio.inactivo, total);
    pct_cpu = (total != 0) ? 100 - pct_libre : 0;
    pct_video = porcentaje_hilo(&muestras, &v->muestras_inicio, 3, total);
    pct_audio = porcentaje_hilo(&muestras, &v->muestras_inicio, 4, total);
    pct_juego = porcentaje_hilo(&muestras, &v->muestras_inicio, 5, total);
    suma_hilos = pct_video + pct_audio + pct_juego + pct_libre;
    pct_otros = (total != 0 && suma_hilos < 100) ? 100 - suma_hilos : 0;
    u32 frames = v->frames ? v->frames : 1;
    Acumulado *a = &acumulado;
    EstadisticasMemoria mem;
    char linea[TEXTO_COLUMNAS + 8];
    char t1[16], t2[16], t3[16], t4[16];
    u32 fps_promedio;
    u32 cola_ms = (v->cola_audio_minima == ~0u) ? 0 : v->cola_audio_minima / AUDIO_BYTES_POR_MS;

    if (a->ventanas_cerradas > 0) {
        if (a->ventanas_cerradas == 1 || fps < a->fps_minimo) {
            a->fps_minimo = fps;
        }
        if (fps > a->fps_maximo) {
            a->fps_maximo = fps;
        }
        a->frames_totales += v->frames;
        a->vblanks_totales += vblanks;
    }
    a->ventanas_cerradas++;
    fps_promedio = (a->vblanks_totales != 0) ? a->frames_totales * VBLANKS_POR_SEGUNDO_X100 / a->vblanks_totales : fps;

    consultar_memoria(&mem);
    texto_rendimiento = 1;

    snprintf(t1, sizeof(t1), "%u.%02u", (unsigned) (fps / 100), (unsigned) (fps % 100));
    snprintf(t2, sizeof(t2), "%u.%u", (unsigned) (a->fps_minimo / 100), (unsigned) (a->fps_minimo % 100 / 10));
    snprintf(t3, sizeof(t3), "%u.%u", (unsigned) (a->fps_maximo / 100), (unsigned) (a->fps_maximo % 100 / 10));
    snprintf(t4, sizeof(t4), "%u.%u", (unsigned) (fps_promedio / 100), (unsigned) (fps_promedio % 100 / 10));
    if (a->ventanas_cerradas <= 1) {
        snprintf(linea, sizeof(linea), "FPS %s  MIN -  MAX -  PROM -", t1);
    } else {
        snprintf(linea, sizeof(linea), "FPS %s MIN %s MAX %s PROM %s", t1, t2, t3, t4);
    }
    texto_escribir_linea(&texto, 0, linea, color_fps(fps));

    decimas(t1, sizeof(t1), ciclos_a_decimas_ms(transcurrido, frames));
    decimas(t2, sizeof(t2), ciclos_a_decimas_ms(v->periodo_maximo, 1));
    snprintf(linea, sizeof(linea), "FRAME %s MS  PICO %s  CPU %u%%", t1, t2, (unsigned) pct_cpu);
    texto_escribir_linea(&texto, 1, linea, pct_cpu >= 90 ? TEXTO_AMARILLO : TEXTO_BLANCO);

    decimas(t1, sizeof(t1), ciclos_a_decimas_ms(v->render_ciclos, frames));
    decimas(t2, sizeof(t2), ciclos_a_decimas_ms(v->ciclos_audio, frames));
    decimas(t3, sizeof(t3), ciclos_a_decimas_ms(v->ciclos_espera_gs, frames));
    decimas(t4, sizeof(t4), ciclos_a_decimas_ms(v->ciclos_espera_dma, frames));
    snprintf(linea, sizeof(linea), "MS/F REND %s AUD %s GS %s DMA %s", t1, t2, t3, t4);
    texto_escribir_linea(&texto, 2, linea, TEXTO_BLANCO);

    snprintf(linea, sizeof(linea), "CPU%% VIDEO %u JUEGO %u AUDIO %u LIBRE %u", (unsigned) pct_video,
             (unsigned) pct_juego, (unsigned) pct_audio, (unsigned) pct_libre);
    texto_escribir_linea(&texto, 3, linea, pct_libre < 10 ? TEXTO_AMARILLO : TEXTO_BLANCO);

    snprintf(linea, sizeof(linea), "DMA %u/F %uKB/F SUBIDAS %u/F %uKB/F", (unsigned) (v->envios / frames),
             (unsigned) (v->bytes_paquete / frames / 1024), (unsigned) (v->subidas / frames),
             (unsigned) (v->bytes_subidas / frames / 1024));
    texto_escribir_linea(&texto, 4, linea, TEXTO_BLANCO);

    snprintf(linea, sizeof(linea), "TRIS %u RECT %u TEX %u %u/%uKB", (unsigned) (v->triangulos / frames),
             (unsigned) (v->rectangulos / frames), (unsigned) mem.texturas_entradas,
             (unsigned) (mem.texturas_bytes / 1024), (unsigned) (mem.texturas_capacidad / 1024));
    texto_escribir_linea(&texto, 5, linea, TEXTO_BLANCO);

    snprintf(linea, sizeof(linea), "LIBRE %uKB POOL %uKB AUD %u/%uKB", (unsigned) (mem.monton_libre / 1024),
             (unsigned) (mem.pool_juego_libre / 1024), (unsigned) (mem.audio_usado / 1024),
             (unsigned) (mem.audio_total / 1024));
    texto_escribir_linea(&texto, 6, linea, TEXTO_BLANCO);

    snprintf(linea, sizeof(linea), "AUDIO CORTES %u COLA MIN %u MS", (unsigned) a->cortes_audio, (unsigned) cola_ms);
    texto_escribir_linea(&texto, 7, linea, a->cortes_audio != 0 ? TEXTO_AMARILLO : TEXTO_BLANCO);

    progreso_rom_ps2(&rom_cargados, &rom_total);
    if (gs_hires()) {
        /* Cumulative: late passes mean the GS takes longer than a band to draw a frame */
        snprintf(linea, sizeof(linea), "HIRES PASS %u LATE %u FLIP %u DROP %u", (unsigned) gs_stats.band_passes,
                 (unsigned) gs_stats.band_overruns, (unsigned) gs_stats.flips, (unsigned) gs_stats.dropped);
        texto_escribir_linea(&texto, 8, linea, gs_stats.band_overruns != 0 || gs_stats.dropped != 0 ? TEXTO_AMARILLO : TEXTO_BLANCO);
    } else {
        snprintf(linea, sizeof(linea), "OTROS HILOS %u%%  ROM EN RAM %u%%", (unsigned) pct_otros,
                 (unsigned) (rom_total != 0 ? rom_cargados * 100 / rom_total : 100));
        texto_escribir_linea(&texto, 8, linea, (rom_total != 0 && rom_cargados < rom_total) ? TEXTO_AMARILLO : TEXTO_BLANCO);
    }

    hay_arranque = summary_tiempos_ps2("arranque", &arranque_us, &paso_largo_arranque, &largo_arranque_us);
    hay_carga = summary_tiempos_ps2("carga de pista", &carga_us, &paso_largo, &largo_us);
    ms_o_guion(t1, sizeof(t1), hay_arranque, arranque_us);
    ms_o_guion(t2, sizeof(t2), hay_carga, carga_us);
    snprintf(linea, sizeof(linea), "ARRANQUE %s MS  CARGA PISTA %s MS", t1, t2);
    texto_escribir_linea(&texto, 9, linea, TEXTO_BLANCO);

    if (!hay_carga) {
        paso_largo = hay_arranque ? paso_largo_arranque : NULL;
        largo_us = largo_arranque_us;
    }
    if (paso_largo != NULL) {
        snprintf(linea, sizeof(linea), "MAX %u MS %s", (unsigned) ((largo_us + 500) / 1000), paso_largo);
    } else {
        snprintf(linea, sizeof(linea), "MAX -");
    }
    texto_escribir_linea(&texto, 10, linea, TEXTO_BLANCO);

    {
        char t5[16];

        decimas(t1, sizeof(t1), ciclos_a_decimas_ms(ciclos_seccion(PROF_VTX), frames));
        decimas(t2, sizeof(t2), ciclos_a_decimas_ms(ciclos_seccion(PROF_TRI) + ciclos_seccion(PROF_RECT), frames));
        decimas(t3, sizeof(t3),
                ciclos_a_decimas_ms(ciclos_seccion(CARGA_PROF) + ciclos_seccion(PREPARAR_PROF) +
                                        ciclos_seccion(DECODIFICACION_PROF), frames));
        decimas(t4, sizeof(t4), ciclos_a_decimas_ms(ciclos_seccion(ESTADO_PROF), frames));
        decimas(t5, sizeof(t5), ciclos_a_decimas_ms(ciclos_seccion(ENVIO_PROF), frames));
        snprintf(linea, sizeof(linea), "VTX %s TRI %s TEX %s EST %s ENV %s", t1, t2, t3, t4, t5);
        texto_escribir_linea(&texto, 11, linea, TEXTO_BLANCO);
    }

#ifdef SMK64_DEV
    /* Al registro de host */
    if ((++ventanas_registro % 5) != 0) {
        return;
    }
    registrar("medidor: escena %d fps %u.%02u min %u max %u prom %u (x100) frame %u pico %u (0,1 ms)", (int) estado_juego,
            (unsigned) (fps / 100), (unsigned) (fps % 100), (unsigned) a->fps_minimo, (unsigned) a->fps_maximo,
            (unsigned) fps_promedio, (unsigned) ciclos_a_decimas_ms(transcurrido, frames),
            (unsigned) ciclos_a_decimas_ms(v->periodo_maximo, 1));
    registrar("medidor: cpu %u%% video %u%% juego %u%% audio %u%% otros %u%% libre %u%%; por frame (0,1 ms) render %u "
            "audio %u espgs %u dma %u; audio cortes %u cola min %u ms",
            (unsigned) pct_cpu, (unsigned) pct_video, (unsigned) pct_juego, (unsigned) pct_audio, (unsigned) pct_otros,
            (unsigned) pct_libre,
            (unsigned) ciclos_a_decimas_ms(v->render_ciclos, frames), (unsigned) ciclos_a_decimas_ms(v->ciclos_audio, frames),
            (unsigned) ciclos_a_decimas_ms(v->ciclos_espera_gs, frames),
            (unsigned) ciclos_a_decimas_ms(v->ciclos_espera_dma, frames), (unsigned) a->cortes_audio, (unsigned) cola_ms);
    registrar("medidor: por frame envios %u paquete %uKB subidas %u %uKB tris %u rect %u; tex %u %u/%uKB libre %uKB "
            "pool %uKB audio %u/%uKB",
            (unsigned) (v->envios / frames), (unsigned) (v->bytes_paquete / frames / 1024),
            (unsigned) (v->subidas / frames), (unsigned) (v->bytes_subidas / frames / 1024),
            (unsigned) (v->triangulos / frames), (unsigned) (v->rectangulos / frames),
            (unsigned) mem.texturas_entradas, (unsigned) (mem.texturas_bytes / 1024),
            (unsigned) (mem.texturas_capacidad / 1024), (unsigned) (mem.monton_libre / 1024),
            (unsigned) (mem.pool_juego_libre / 1024), (unsigned) (mem.audio_usado / 1024),
            (unsigned) (mem.audio_total / 1024));
#endif
}

void medidor_inicio_frame(void)
{
    u32 ahora = medidor_leer_ciclos();
    u32 vblank = contador_vblank();

    if (!iniciado) {
        iniciado = 1;
        texto.vram = gs_vram_meter();
        texto_limpiar(&texto);
        texto_escribir_linea(&texto, 0, "MIDIENDO...", TEXTO_BLANCO);
        reiniciar_acumulado();
        empezar_ventana(ahora);
        anterior_inicio_frame = ahora;
    }

    if (estado_juego != escena_anterior || reiniciar_pendiente) {
        escena_anterior = estado_juego;
        reiniciar_pendiente = 0;
        reiniciar_acumulado();
    }

    if (ahora - anterior_inicio_frame > ventana.periodo_maximo) {
        ventana.periodo_maximo = ahora - anterior_inicio_frame;
    }
    anterior_inicio_frame = ahora;

    if (vblank - ventana.vblank_inicio >= VBLANKS_POR_VENTANA) {
        cerrar_ventana(ahora, vblank);
        empezar_ventana(ahora);
    }

    inicio_frame = ahora;
    espera_gs_frame = 0;
    espera_dma_frame = 0;
    audio_inicio_frame = ciclos_audio_total;
}

void medidor_dibujar(void)
{
    if (!iniciado || modo_panel == PANEL_OCULTO) {
        return;
    }
#ifdef SMK64_DEBUG_AUDIO
    if (modo_panel == PANEL_AUDIO) {
        /* se reescribe cada 30 frames */
        static u32 cuenta;

        if (texto_rendimiento || ++cuenta >= 30) {
            texto_rendimiento = 0;
            cuenta = 0;
            char lineas[MEDIDOR_LINEAS][64];
            int i, n = ps2_audio_monitor_lineas(lineas, MEDIDOR_LINEAS);

            texto_limpiar(&texto);
            for (i = 0; i < n; i++) {
                texto_escribir_linea(&texto, i, lineas[i], TEXTO_BLANCO);
            }
        }
        texto_dibujar(&texto, 16.0f, 16.0f, MEDIDOR_LINEAS);
        return;
    }
#endif
    texto_dibujar(&texto, 16.0f, 16.0f, modo_panel == PANEL_COMPLETO ? MEDIDOR_LINEAS : 1);
}

void medidor_frame_intermedio(void)
{
    ventana.frames++;
}

void medidor_fin_frame(void)
{
    u32 total = medidor_leer_ciclos() - inicio_frame;
    u32 esperas = espera_gs_frame + espera_dma_frame + (ciclos_audio_total - audio_inicio_frame);

    ventana.frames++;
    ventana.render_ciclos += (total > esperas) ? total - esperas : 0;
    ventana.ciclos_espera_gs += espera_gs_frame;
    ventana.ciclos_espera_dma += espera_dma_frame;
    ventana.bytes_paquete += gs_stats.packet_bytes;
    ventana.subidas += gs_stats.uploads;
    ventana.bytes_subidas += gs_stats.upload_bytes;
    ventana.triangulos += gs_stats.triangles;
    ventana.rectangulos += gs_stats.sprites;
}
