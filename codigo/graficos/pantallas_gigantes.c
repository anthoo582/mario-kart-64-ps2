#include <string.h>

#include <ultra64.h>
#include <gsKit.h>

#include "graficos/sintetizador_gs.h"
#include "graficos/pantallas_gigantes.h"

#define RANURAS_MAX  8
#define BYTES_RANURA 8192 /* una pagina CT32: 64x32 */

typedef struct {
    const u8 *target;
    u32 bytes;     /* tamano de la textura original (RGBA16) */
    int x, y, w, h;
    int pendiente;
    int valido;
} Ranura;

static Ranura ranuras[RANURAS_MAX];

void reiniciar_pantallas_gigantes(void)
{
    memset(ranuras, 0, sizeof(ranuras));
}

void copiar_pantalla_gigante(int x, int y, int w, int h, const void *objetivo)
{
    int i, liberar_ranura = -1;

    for (i = 0; i < RANURAS_MAX; i++) {
        if (ranuras[i].target == objetivo) {
            break;
        }
        if (ranuras[i].target == NULL && liberar_ranura < 0) {
            liberar_ranura = i;
        }
    }
    if (i == RANURAS_MAX) {
        if (liberar_ranura < 0) {
            return;
        }
        i = liberar_ranura;
    }
    /* La ranura es de 64x32 texels: un trozo mayor no se copia. */
    if (w > 64 || h > 32 || w <= 0 || h <= 0) {
        return;
    }
    ranuras[i].target = (const u8 *) objetivo;
    ranuras[i].bytes = (u32) (w * h * 2);
    ranuras[i].x = x;
    ranuras[i].y = y;
    ranuras[i].w = w;
    ranuras[i].h = h;
    ranuras[i].pendiente = 1;
}

static u32 vram_ranura(int i)
{
    return gs_vram_texture_end() + (u32) i * BYTES_RANURA;
}

static float a_fb_y(int y)
{
    return (float) y * GS_HEIGHT / 240.0f;
}

void empezar_frame_pantallas_gigantes(void)
{
    int i;

    for (i = 0; i < RANURAS_MAX; i++) {
        Ranura *s = &ranuras[i];

        if (!s->pendiente) {
            continue;
        }
        s->pendiente = 0;
        s->valido = 1;
        gs_copy_from_screen((float) (s->x * 2), a_fb_y(s->y), (float) (s->w * 2), a_fb_y(s->y + s->h) - a_fb_y(s->y),
                               vram_ranura(i), s->w, s->h);
    }
}

int buscar_pantalla_gigante(const void *orig_, InfoTextura *salida)
{
    const u8 *p = (const u8 *) orig_;
    int i;

    if (p == NULL) {
        return 0;
    }
    for (i = 0; i < RANURAS_MAX; i++) {
        Ranura *s = &ranuras[i];

        if (!s->valido || p < s->target || p >= s->target + s->bytes) {
            continue;
        }
        /* 64x32 CT32 (TW 6, TH 5), 1:1 con los texels del N64. */
        salida->tex0 = GS_SETREG_TEX0(vram_ranura(i) / 256, 1, GS_PSM_CT32, 6, 5, 1, 0, 0, 0, 0, 0, 0);
        salida->clamp = GS_SETREG_CLAMP(2, 2, 0, s->w - 1, 0, s->h - 1);
        salida->width = (u32) s->w;
        salida->height = (u32) s->h;
        salida->maceta_w = 64.0f;
        salida->maceta_h = 32.0f;
        salida->maceta_w_inv = 1.0f / 64.0f;
        salida->maceta_h_inv = 1.0f / 32.0f;
        salida->origen_s = 0.0f;
        salida->origen_t = 0.0f;
        salida->envoltura_s = 0;
        salida->envoltura_t = 0;
        return 1;
    }
    return 0;
}
