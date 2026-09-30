#ifdef SMK64_DEV
#include <kernel.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ultra64.h>
#include <PR/os.h>
#include <juego/estructuras_comunes.h>
#include <juego/definiciones.h>

#include "sistema/sistema_ps2.h"
#include "sistema/perfilado.h"
#include "audio/salida_audio.h"
#include "audio/microcodigo_audio.h"
#include "depuracion/guiones_prueba.h"
#include "graficos/interprete_f3dex.h"
#include "menus/elementos_menu.h"
#include "carrera/camara.h"

extern s32 estado_juego;
extern s32 siguiente_estado_juego;
extern s32 seleccion_menu;
extern s16 id_circuito_actual;
extern s32 seleccion_modo;
extern s32 estado_carrera;
extern u16 juego_en_pausa;
extern s32 seleccion_cantidad_jugador_1;
extern s8 seleccion_copa;
extern s32 seleccion_cc;
extern u16 modo_demo;
extern Jugador jugadores[];
extern s8 menu_principal_seleccion;
extern s8 seleccion_menu_sub;
extern u16 dato_8015F894; /* fase de la pantalla de resultados (logica_carrera.c) */
extern s8 cantidad_jugador;
extern s8 juego_modo_menu_columna[];
extern s8 juego_modo_sub_menu_columna[4][3];

#define PASOS_MAX 1024
#define TAMANIO_REGISTRO (192 * 1024)

enum Op { ESPERA_OP, PULSACION_OP, MANTENIDO_OP, PALANCA_OP, OP_HASTA, PULSACION_OP_HASTA, COMPROBACION_OP, AUTOPILOTO_OP, DISPARO_OP, NOTA_OP, VOLCADO_OP, RELLENO_OP, ERROR_OP, OP_AUDIO, OP_CAMS, OP_SI, OP_PROF, OP_ACMD, OP_ITEM, OP_STICKY, OP_INTERP, OP_ESTADO, OP_MUESTRA, FIN_OP };
enum Cmp { CMP_EQ, CMP_NE, CMP_LT, CMP_LE, CMP_GT, CMP_GE };

typedef struct {
    u8 op;
    u8 variable;
    u8 cmp;
    u16 buttons;
    s8 sx, sy;
    s32 count; /* frames, o el maximo en las esperas */
    s32 value;
    const char *text; /* dentro de guion (nota, captura) */
    u16 line;
} Paso;

static const char *const nombres_variable[] = { "estado", "menu",      "pista", "modo", "carrera", "pausa", "vuelta",
                                         "puesto", "jugadores", "copa",  "cc",   "demo",    "frame",
                                         "submenu", "seleccion", "menupausa", "resultados", "columna",
                                         "subcolumna", "menufinal", "efectos", "disparadores", "rapidez" };
#define VARIABLES_NUM ((int) (sizeof(nombres_variable) / sizeof(nombres_variable[0])))

static char *guion;
static Paso pasos[PASOS_MAX];
static int pasos_num;
static int activo;
static int act;
static s32 frame_paso; /* frames dentro del paso actual */
static u32 s_frame;
static int autopiloto;
static int s_relleno; /* mando que manejan las ordenes (0-3) */
static int fallos;
static int hecho;
static s32 ultimo_variables[VARIABLES_NUM];
static u32 frame_rend, vblank_rend;

static char registro[TAMANIO_REGISTRO];
static int largo_registro;

static void probar_registro(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void probar_vaciar_registro(void);
static u32 registro_vaciado; /* VBlank del ultimo volcado de smk64_test.txt */

static void probar_registro(const char *fmt, ...)
{
    va_list ap;
    int n;
    int inicio_linea = largo_registro;

    if (largo_registro > TAMANIO_REGISTRO - 256) {
        return; /* lleno: se conserva el principio, que es lo que importa */
    }
    n = snprintf(registro + largo_registro, TAMANIO_REGISTRO - largo_registro, "[%6u f%6u] ", (unsigned) contador_vblank(),
                 (unsigned) s_frame);
    largo_registro += n;
    va_start(ap, fmt);
    n = vsnprintf(registro + largo_registro, TAMANIO_REGISTRO - largo_registro - 1, fmt, ap);
    va_end(ap);
    if (n > 0) {
        largo_registro += (n < TAMANIO_REGISTRO - largo_registro - 1) ? n : TAMANIO_REGISTRO - largo_registro - 2;
    }
    registro[largo_registro++] = '\n';
    registro[largo_registro] = '\0'; /* para strstr de la linea; no se escribe en el fichero */
    /* El fichero se reescribe entero (host */
    if (hecho || strstr(registro + inicio_linea, "FALLO") != NULL || strstr(registro + inicio_linea, "captura") != NULL ||
        contador_vblank() - registro_vaciado >= 60) {
        probar_vaciar_registro();
    }
}

static void probar_vaciar_registro(void)
{
    FILE *f;

    bloquear_host();
    f = fopen("host:smk64_test.txt", "w");
    if (f != NULL) {
        fwrite(registro, 1, largo_registro, f);
        fclose(f);
    }
    desbloquear_host();
    registro_vaciado = contador_vblank();
}

static s32 leer_variable(int variable_)
{
    switch (variable_) {
        case 0: return estado_juego;
        case 1: return seleccion_menu;
        case 2: return id_circuito_actual;
        case 3: return seleccion_modo;
        case 4: return estado_carrera & 0xFFFF; /* u16 en bucle_principal.c, ver bucle_principal.h */
        case 5: return juego_en_pausa;
        case 6: return jugadores[0].cantidad_vuelta;
        case 7: return jugadores[0].puesto_actual + 1;
        case 8: return seleccion_cantidad_jugador_1;
        case 9: return seleccion_copa;
        case 10: return seleccion_cc;
        case 11: return modo_demo;
        case 12: return (s32) s_frame;
        case 13: return menu_principal_seleccion;
        case 14: return seleccion_menu_sub;
        case 15: {
            MenuItem *item = buscar_items_menu(0xC7); /* menu de pausa */

            return (item != NULL) ? item->state : -1;
        }
        case 16: return dato_8015F894;
        case 20: return (s32) jugadores[0].efectos;
        case 21: return jugadores[0].disparadores;
        case 22: return (s32) jugadores[0].actual_rapidez;
        case 19: {
            MenuItem *item = buscar_items_menu(MENU_ITEM_FIN_CIRCUITO_OPCION);

            return (item != NULL) ? item->state : -1;
        }
        case 17:
        case 18: {
            int n = (cantidad_jugador >= 1 && cantidad_jugador <= 4) ? cantidad_jugador - 1 : 0;
            int col = juego_modo_menu_columna[n];

            if (variable_ == 17) {
                return col;
            }
            return (col >= 0 && col < 3) ? juego_modo_sub_menu_columna[n][col] : -1;
        }
    }
    return 0;
}

static int comparar(s32 a, int cmp, s32 b)
{
    switch (cmp) {
        case CMP_EQ: return a == b;
        case CMP_NE: return a != b;
        case CMP_LT: return a < b;
        case CMP_LE: return a <= b;
        case CMP_GT: return a > b;
        case CMP_GE: return a >= b;
    }
    return 0;
}

static const char *const nombres_cmp[] = { "==", "!=", "<", "<=", ">", ">=" };

static char *palabra_siguiente(char **p)
{
    char *s = *p;
    char *w;

    while (*s == ' ' || *s == '\t') {
        s++;
    }
    if (*s == '\0') {
        *p = s;
        return NULL;
    }
    w = s;
    while (*s != '\0' && *s != ' ' && *s != '\t') {
        s++;
    }
    if (*s != '\0') {
        *s++ = '\0';
    }
    *p = s;
    return w;
}

static int analizar_botones(const char *w, u16 *salida)
{
    static const struct {
        const char *name;
        u16 bits;
    } nombres[] = { { "A", A_BUTTON },     { "B", B_BUTTON },     { "Z", Z_TRIG },         { "R", R_TRIG },
                  { "L", L_TRIG },       { "START", START_BUTTON }, { "CU", U_CBUTTONS }, { "CD", D_CBUTTONS },
                  { "CL", L_CBUTTONS },  { "CR", R_CBUTTONS },  { "ARRIBA", U_JPAD },    { "ABAJO", D_JPAD },
                  { "IZQ", L_JPAD },     { "DER", R_JPAD },     { "NADA", 0 } };
    char buf[64];
    char *s;
    char *tok;
    u16 bits = 0;

    strncpy(buf, w, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    s = buf;
    while ((tok = strsep(&s, "+")) != NULL) {
        unsigned i;

        for (i = 0; i < sizeof(nombres) / sizeof(nombres[0]); i++) {
            if (strcmp(tok, nombres[i].name) == 0) {
                bits |= nombres[i].bits;
                break;
            }
        }
        if (i == sizeof(nombres) / sizeof(nombres[0])) {
            return 0;
        }
    }
    *salida = bits;
    return 1;
}

static int analizar_cond(char **p, Paso *st)
{
    char *variable_ = palabra_siguiente(p);
    char *cmp = palabra_siguiente(p);
    char *val = palabra_siguiente(p);
    int i;

    if (variable_ == NULL || cmp == NULL || val == NULL) {
        return 0;
    }
    for (i = 0; i < VARIABLES_NUM && strcmp(variable_, nombres_variable[i]) != 0; i++) {
    }
    if (i == VARIABLES_NUM) {
        return 0;
    }
    st->variable = (u8) i;
    for (i = 0; i < 6 && strcmp(cmp, nombres_cmp[i]) != 0; i++) {
    }
    if (i == 6) {
        return 0;
    }
    st->cmp = (u8) i;
    st->value = (s32) strtol(val, NULL, 0);
    return 1;
}

static int analizar_linea(char *line, Paso *st)
{
    char *p = line;
    char *cmd = palabra_siguiente(&p);
    char *w;

    memset(st, 0, sizeof(*st));
    if (strcmp(cmd, "espera") == 0) {
        st->op = ESPERA_OP;
        w = palabra_siguiente(&p);
        st->count = w ? atoi(w) : 0;
        return w != NULL;
    }
    if (strcmp(cmd, "pulsa") == 0 || strcmp(cmd, "mantiene") == 0) {
        st->op = (cmd[0] == 'p') ? PULSACION_OP : MANTENIDO_OP;
        w = palabra_siguiente(&p);
        if (w == NULL || !analizar_botones(w, &st->buttons)) {
            return 0;
        }
        w = palabra_siguiente(&p);
        st->count = w ? atoi(w) : 2;
        return st->count > 0;
    }
    if (strcmp(cmd, "stick") == 0) {
        char *x = palabra_siguiente(&p);
        char *y = palabra_siguiente(&p);
        char *n = palabra_siguiente(&p);

        if (x == NULL || y == NULL || n == NULL) {
            return 0;
        }
        st->op = PALANCA_OP;
        st->sx = (s8) atoi(x);
        st->sy = (s8) atoi(y);
        st->count = atoi(n);
        w = palabra_siguiente(&p);
        return w == NULL || analizar_botones(w, &st->buttons);
    }
    if (strcmp(cmd, "hasta") == 0 || strcmp(cmd, "comprueba") == 0) {
        st->op = (cmd[0] == 'h') ? OP_HASTA : COMPROBACION_OP;
        if (!analizar_cond(&p, st)) {
            return 0;
        }
        w = palabra_siguiente(&p);
        st->count = w ? atoi(w) : 3000;
        return 1;
    }
    if (strcmp(cmd, "si") == 0) {
        st->op = OP_SI;
        if (!analizar_cond(&p, st)) {
            return 0;
        }
        w = palabra_siguiente(&p);
        st->count = w ? atoi(w) : 0;
        return st->count > 0;
    }
    if (strcmp(cmd, "pulsa_hasta") == 0) {
        st->op = PULSACION_OP_HASTA;
        w = palabra_siguiente(&p);
        if (w == NULL || !analizar_botones(w, &st->buttons) || !analizar_cond(&p, st)) {
            return 0;
        }
        w = palabra_siguiente(&p);
        st->count = w ? atoi(w) : 3000;
        return 1;
    }
    if (strcmp(cmd, "autopiloto") == 0) {
        st->op = AUTOPILOTO_OP;
        w = palabra_siguiente(&p);
        st->value = (w != NULL && strcmp(w, "si") == 0);
        return w != NULL;
    }
    if (strcmp(cmd, "captura") == 0 || strcmp(cmd, "nota") == 0) {
        st->op = (cmd[0] == 'c') ? DISPARO_OP : NOTA_OP;
        while (*p == ' ' || *p == '\t') {
            p++;
        }
        st->text = p;
        return 1;
    }
    if (strcmp(cmd, "mando") == 0) {
        st->op = RELLENO_OP;
        w = palabra_siguiente(&p);
        st->value = w ? atoi(w) : 0;
        return st->value >= 1 && st->value <= MAXCONTROLLERS;
    }
    if (strcmp(cmd, "fallo") == 0) {
        st->op = ERROR_OP;
        w = palabra_siguiente(&p);
        st->value = (w != NULL && strcmp(w, "div") == 0);
        return 1;
    }
    if (strcmp(cmd, "audio") == 0) {
        char *nombre = palabra_siguiente(&p);
        char *segs = palabra_siguiente(&p);

        if (nombre == NULL || segs == NULL) {
            return 0;
        }
        st->op = OP_AUDIO;
        st->text = nombre;
        st->value = atoi(segs);
        return st->value > 0;
    }
    if (strcmp(cmd, "tareas_audio") == 0) {
        char *nombre = palabra_siguiente(&p);
        char *n = palabra_siguiente(&p);

        if (nombre == NULL || n == NULL) {
            return 0;
        }
        st->op = OP_ACMD;
        st->text = nombre;
        st->value = atoi(n);
        return st->value > 0;
    }
    if (strcmp(cmd, "perfil") == 0) {
        w = palabra_siguiente(&p);
        if (w == NULL) {
            return 0;
        }
        st->op = OP_PROF;
        st->value = strcmp(w, "inicio") == 0;
        if (!st->value) {
            st->text = palabra_siguiente(&p);
            return strcmp(w, "fin") == 0 && st->text != NULL;
        }
        return 1;
    }
    if (strcmp(cmd, "intermedio") == 0) {
        st->op = OP_INTERP;
        w = palabra_siguiente(&p);
        st->value = (w != NULL && strcmp(w, "si") == 0);
        return w != NULL;
    }
    if (strcmp(cmd, "muestra") == 0) {
        char *variable_ = palabra_siguiente(&p);
        int i;

        st->op = OP_MUESTRA;
        for (i = 0; variable_ != NULL && i < VARIABLES_NUM && strcmp(variable_, nombres_variable[i]) != 0; i++) {
        }
        st->variable = (u8) i;
        return variable_ != NULL && i < VARIABLES_NUM;
    }
    if (strcmp(cmd, "siguiente_estado") == 0) {
        st->op = OP_ESTADO;
        w = palabra_siguiente(&p);
        st->value = w ? atoi(w) : -1;
        return st->value >= 0;
    }
    if (strcmp(cmd, "fija") == 0) {
        st->op = OP_STICKY;
        w = palabra_siguiente(&p);
        return w != NULL && analizar_botones(w, &st->buttons);
    }
    if (strcmp(cmd, "objeto") == 0) {
        char *n = palabra_siguiente(&p);
        char *j = palabra_siguiente(&p);

        st->op = OP_ITEM;
        st->value = n != NULL ? atoi(n) : 0;
        st->count = j != NULL ? atoi(j) : 0;
        return st->value >= 1 && st->value <= 15 && st->count >= 0 && st->count <= 4;
    }
    if (strcmp(cmd, "camaras") == 0) {
        st->op = OP_CAMS;
        return 1;
    }
    if (strcmp(cmd, "volcado") == 0) {
        st->op = VOLCADO_OP;
        return 1;
    }
    if (strcmp(cmd, "fin") == 0) {
        st->op = FIN_OP;
        return 1;
    }
    return 0;
}

void inicializar_guiones_prueba(void)
{
    FILE *f = fopen("host:smk64_input.txt", "r");
    long size;
    char *line;
    char *p;
    int no_linea = 0;

    if (f == NULL) {
        return;
    }
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);
    guion = malloc(size + 1);
    if (guion == NULL || fread(guion, 1, size, f) != (size_t) size) {
        fclose(f);
        registrar("autotest: no se pudo leer el guion");
        return;
    }
    fclose(f);
    guion[size] = '\0';

    p = guion;
    while ((line = strsep(&p, "\n")) != NULL) {
        char *hash = strchr(line, '#');
        char *end;

        no_linea++;
        if (hash != NULL) {
            *hash = '\0';
        }
        end = line + strlen(line);
        while (end > line && (end[-1] == '\r' || end[-1] == ' ' || end[-1] == '\t')) {
            *--end = '\0';
        }
        while (*line == ' ' || *line == '\t') {
            line++;
        }
        if (*line == '\0') {
            continue;
        }
        if (pasos_num == PASOS_MAX) {
            probar_registro("GUION DEMASIADO LARGO (max %d ordenes)", PASOS_MAX);
            break;
        }
        {
            char copiar[160];

            strncpy(copiar, line, sizeof(copiar) - 1);
            copiar[sizeof(copiar) - 1] = '\0';
            if (!analizar_linea(line, &pasos[pasos_num])) {
                probar_registro("ERROR DE SINTAXIS en la linea %d: %s", no_linea, copiar);
                fallos++;
                continue;
            }
        }
        pasos[pasos_num].line = (u16) no_linea;
        pasos_num++;
    }
    activo = 1;
    probar_registro("guion cargado: %d ordenes", pasos_num);
    registrar("autotest: guion de %d ordenes", pasos_num);
}

int guion_prueba_activo(void)
{
    return activo;
}

static void registrar_cambios(void)
{
    static const int vigilado[] = { 0, 1, 2, 3, 4, 5, 6, 7 };
    unsigned i;
    int cambiado = 0;

    for (i = 0; i < sizeof(vigilado) / sizeof(vigilado[0]); i++) {
        s32 v = leer_variable(vigilado[i]);

        if (v != ultimo_variables[vigilado[i]]) {
            cambiado = 1;
            ultimo_variables[vigilado[i]] = v;
        }
    }
    if (cambiado) {
        probar_registro("  estado %d menu %d pista %d modo %d carrera %d pausa %d vuelta %d puesto %d", (int) ultimo_variables[0],
                 (int) ultimo_variables[1], (int) ultimo_variables[2], (int) ultimo_variables[3], (int) ultimo_variables[4], (int) ultimo_variables[5],
                 (int) ultimo_variables[6], (int) leer_variable(7));
    }
}

static void aplicar_autopiloto(void)
{
    int i;

    if (!autopiloto || estado_juego != CARRERA) {
        return;
    }
    /* Lo mismo que hace el juego al cruzar la meta (logica_carrera.c) */
    for (i = 0; i < seleccion_cantidad_jugador_1 && i < MAXCONTROLLERS; i++) {
        Jugador *p = &jugadores[i];

        if ((p->type & EXISTE_JUGADOR) && (p->type & HUMANO_JUGADOR) && !(p->type & CPU_JUGADOR)) {
            p->type |= CPU_JUGADOR;
        }
    }
}

static const char *volatile captura_pedida;

const char *captura_pendiente_guion(void)
{
    return captura_pedida;
}

void captura_hecha_guion(void)
{
    captura_pedida = NULL;
}

/* Frames de juego y VBlanks */
static void registrar_rend(void)
{
    u32 vb = contador_vblank();

    if (frame_rend == 0) {
        frame_rend = s_frame;
        vblank_rend = vb;
        return;
    }
    if (s_frame - frame_rend >= 300) {
        u32 frames = s_frame - frame_rend;
        u32 vbl = vb - vblank_rend;

        probar_registro("  ritmo: %u frames en %u VBlanks = %u.%02u VBlanks/frame (estado %d)", (unsigned) frames,
                 (unsigned) vbl, (unsigned) (vbl / frames), (unsigned) (vbl * 100 / frames % 100), (int) estado_juego);
        frame_rend = s_frame;
        vblank_rend = vb;
    }
}

static void terminar(const char *por_que)
{
    extern s32 menu_texturas_salteado;

    hecho = 1;
    probar_registro("texturas de menu omitidas: %d", (int) menu_texturas_salteado);
    probar_registro("FIN DEL GUION (%s): %d fallos", por_que, fallos);
}

/* Devuelve los botones y el stick de este frame. */
static u16 s_sticky[MAXCONTROLLERS];

static void ejecutar_paso(OSContPad *relleno)
{
    for (;;) {
        Paso *st;

        if (hecho) {
            return;
        }
        if (act >= pasos_num) {
            terminar("ultima orden");
            return;
        }
        st = &pasos[act];
        switch (st->op) {
            case ESPERA_OP:
            case MANTENIDO_OP:
            case PALANCA_OP:
                if (frame_paso < st->count) {
                    relleno->button = st->buttons;
                    relleno->stick_x = st->sx;
                    relleno->stick_y = st->sy;
                    frame_paso++;
                    return;
                }
                break;
            case PULSACION_OP:
                if (frame_paso < st->count + 8) {
                    relleno->button = (frame_paso < st->count) ? st->buttons : 0;
                    frame_paso++;
                    return;
                }
                break;
            case OP_HASTA:
            case PULSACION_OP_HASTA:
                if (comparar(leer_variable(st->variable), st->cmp, st->value)) {
                    probar_registro("ok: %s %s %d (linea %d, %d frames)", nombres_variable[st->variable], nombres_cmp[st->cmp],
                             (int) st->value, st->line, (int) frame_paso);
                    break;
                }
                if (frame_paso >= st->count) {
                    fallos++;
                    probar_registro("FALLO: %s %s %d no se cumplio en %d frames (linea %d; vale %d)", nombres_variable[st->variable],
                             nombres_cmp[st->cmp], (int) st->value, (int) st->count, st->line,
                             (int) leer_variable(st->variable));
                    terminar("abortado");
                    return;
                }
                if (st->op == PULSACION_OP_HASTA) {
                    relleno->button = ((frame_paso % 12) < 2) ? st->buttons : 0;
                }
                frame_paso++;
                return;
            case COMPROBACION_OP:
                if (comparar(leer_variable(st->variable), st->cmp, st->value)) {
                    probar_registro("ok: %s %s %d (linea %d)", nombres_variable[st->variable], nombres_cmp[st->cmp], (int) st->value,
                             st->line);
                } else {
                    fallos++;
                    probar_registro("FALLO: %s %s %d (linea %d; vale %d)", nombres_variable[st->variable], nombres_cmp[st->cmp],
                             (int) st->value, st->line, (int) leer_variable(st->variable));
                }
                break;
            case AUTOPILOTO_OP:
                autopiloto = st->value;
                if (!autopiloto && estado_juego == CARRERA) {
                    int i;

                    for (i = 0; i < seleccion_cantidad_jugador_1 && i < MAXCONTROLLERS; i++) {
                        if ((jugadores[i].type & HUMANO_JUGADOR) && !(jugadores[i].type & MODO_CINEMATICA_JUGADOR)) {
                            jugadores[i].type &= ~CPU_JUGADOR;
                        }
                    }
                }
                probar_registro("autopiloto %s", autopiloto ? "si" : "no");
                break;
            case DISPARO_OP:
                if (frame_paso == 0) {
                    captura_pedida = st->text;
                    probar_registro("captura %s", st->text);
                }
                if (frame_paso < 900 && captura_pedida != NULL) {
                    frame_paso++;
                    return;
                }
                if (frame_paso >= 900) {
                    probar_registro("aviso: nadie hizo la captura %s", st->text);
                }
                break;
            case NOTA_OP:
                probar_registro("-- %s", st->text);
                break;
            case ERROR_OP:
                if (st->value == 0) {
                    probar_registro("provocando una excepcion (escritura en 0x1)");
                    *(volatile u32 *) 1 = 0;
                } else {
                    volatile s32 zero = 0;
                    volatile s32 r;

                    probar_registro("provocando una excepcion (division por cero)");
                    r = 100 / zero;
                    (void) r;
                }
                break;
            case RELLENO_OP:
                s_relleno = st->value - 1;
                probar_registro("mando %d", (int) st->value);
                break;
            case OP_SI:
                if (comparar(leer_variable(st->variable), st->cmp, st->value)) {
                    probar_registro("si %s %s %d: si (linea %d)", nombres_variable[st->variable], nombres_cmp[st->cmp], (int) st->value,
                             st->line);
                } else {
                    probar_registro("si %s %s %d: no (vale %d), se saltan %d ordenes (linea %d)", nombres_variable[st->variable],
                             nombres_cmp[st->cmp], (int) st->value, (int) leer_variable(st->variable), (int) st->count, st->line);
                    act += st->count;
                }
                break;
            case OP_PROF:
#ifdef SMK64_PROF
                if (st->value) {
                    iniciar_perfil_muestreo();
                    probar_registro("perfil: inicio");
                } else {
                    terminar_perfil_muestreo(st->text);
                    probar_registro("perfil: fin %s", st->text);
                }
#else
                probar_registro("perfil: hace falta -DSMK64_PROF");
#endif
                break;
            case OP_AUDIO:
                grabar_salida_audio(st->text, (int) st->value);
                probar_registro("audio: grabando %d s en host:%s", (int) st->value, st->text);
                break;
            case OP_ACMD:
                volcar_pedido_aspmain(st->text, (int) st->value);
                probar_registro("audio: volcando %d tareas en host:%s", (int) st->value, st->text);
                break;
            case OP_CAMS: {
                extern Camara camaras[];
                int i;

                for (i = 0; i < 4; i++) {
                    const Jugador *pl = &jugadores[i];
                    const Camara *c = &camaras[i];

                    probar_registro("  jugador %d tipo %04x pos %d %d %d | camara %d (sigue a %d) pos %d %d %d mira %d %d %d rot %d",
                             i, (unsigned) pl->type, (int) pl->pos[0], (int) pl->pos[1], (int) pl->pos[2], i,
                             (int) c->id_jugador, (int) c->pos[0], (int) c->pos[1], (int) c->pos[2], (int) c->mirar_a[0],
                             (int) c->mirar_a[1], (int) c->mirar_a[2], (int) c->rot[1]);
                }
                {
                    extern s16 dato_80164678[];
                    extern s16 dato_80164670[];
                    extern f32 acercar_camara[];

                    for (i = 0; i < 4; i++) {
                        probar_registro("  camara %d modo %d (guardado %d) campo de vision %d.%02d", i, (int) dato_80164678[i],
                                 (int) dato_80164670[i], (int) acercar_camara[i], (int) (acercar_camara[i] * 100) % 100);
                    }
                }
                break;
            }
            case OP_INTERP:
                if (ps2_gfx_interp_activado() != st->value) {
                    alternar_interp_gfx_ps2();
                }
                probar_registro("intermedio %s", st->value ? "si" : "no");
                break;
            case OP_MUESTRA:
                probar_registro("%s = %d (0x%x)", nombres_variable[st->variable], (int) leer_variable(st->variable),
                                (unsigned) leer_variable(st->variable));
                break;
            case OP_ESTADO:
                /* Como el atajo DVDL del original (L+R+Z+B lleva a la ceremonia). */
                siguiente_estado_juego = st->value;
                probar_registro("siguiente estado %d", (int) st->value);
                break;
            case OP_STICKY:
                s_sticky[s_relleno] = st->buttons;
                probar_registro("fija %04x en el mando %d", (unsigned) st->buttons, s_relleno + 1);
                break;
            case OP_ITEM: {
                extern void funcion_8007ABFC(s32 id_jugador, bool parametro1);
                int quien = st->count > 0 ? st->count - 1 : s_relleno;

                funcion_8007ABFC(quien, (bool) st->value);
                probar_registro("objeto %d para el jugador %d", (int) st->value, quien + 1);
                break;
            }
            case VOLCADO_OP:
                pedir_volcado_display_list();
                probar_registro("volcado de la display list pedido");
                break;
            case FIN_OP:
                terminar("fin");
                return;
        }
        act++;
        frame_paso = 0;
    }
}

void avanzar_guion_prueba(OSContPad *rellenos)
{
    int i;

    if (!activo) {
        return;
    }
    s_frame++;
    for (i = 0; i < MAXCONTROLLERS; i++) {
        rellenos[i].button = 0;
        rellenos[i].stick_x = 0;
        rellenos[i].stick_y = 0;
        rellenos[i].errno = 0;
    }
    registrar_cambios();
    registrar_rend();
    ejecutar_paso(&rellenos[s_relleno]);
    for (i = 0; i < MAXCONTROLLERS; i++) {
        rellenos[i].button |= s_sticky[i];
    }
    aplicar_autopiloto();
}
#endif
