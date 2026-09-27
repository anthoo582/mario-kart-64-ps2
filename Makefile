# Mario Kart 64 PS2 Port
#
#   make                 ELF final (build/ps2/SLUS_999.99) + build/ps2/SMK64ROM.BIN
#   make iso             imagen ISO para OPL o PCSX2 (compilaciones/)
#   make DEBUG=1         panel de rendimiento y registro (build/ps2/debug)
#   make DEV=1           DEBUG + registro y guiones por host: (build/ps2/dev)
#   make MONOLITICO=1    un solo ELF con toda la ROM adentro
#   make test            pruebas en el PC
#   make clean           borra build/ps2

ifeq ($(PS2SDK),)
  $(error PS2SDK no definido: ejecuta '. herramientas/entorno.sh')
endif
PS2DEV ?= /usr/local/ps2dev

BUILD   := build/ps2
PREFIX  := mips64r5900el-ps2-elf-
CC      := $(PREFIX)gcc
AS      := $(PREFIX)as
LD      := $(PREFIX)ld
OBJCOPY := $(PREFIX)objcopy
NM      := $(PREFIX)nm
STRIP   := $(PREFIX)strip
PYTHON  ?= python3

HERRAMIENTAS := build/herramientas
MIO0TOOL     := $(HERRAMIENTAS)/mio0
DLPACKER     := $(HERRAMIENTAS)/empaquetador_listas

V ?= @
ifeq ($(V),1)
  V :=
endif

# --- Opciones -------------------------------------------------------------------

# ROM_STREAM=1: la ROM grande va en SMK64ROM.BIN y se carga en segundo plano.
DEV ?= 0
DEBUG ?= $(DEV)
MONOLITICO ?= 0
ifeq ($(MONOLITICO),1)
  ROM_STREAM := 0
endif
ROM_STREAM ?= $(if $(filter 1,$(DEV)),0,1)
BUILD_ID ?= 2026-09-25
DEFINES := -D_LANGUAGE_C -D_EE -DTARGET_PS2=1 -DVERSION_US=1 -DF3DEX_GBI=1 -DF3D_OLD=1 \
           -DNON_MATCHING=1 -DAVOID_UB=1 -DSMK64_ROM_STREAM=$(ROM_STREAM) $(EXTRA_DEFINES)
OBJDIR := $(BUILD)
ifeq ($(DEV),1)
  DEFINES += -DSMK64_DEV=1 -DSMK64_DEBUG=1 -DPS2_BUILD_ID=\"$(BUILD_ID)-dev\"
  OBJDIR  := $(BUILD)/dev
else ifeq ($(DEBUG),1)
  DEFINES += -DSMK64_DEBUG=1 -DPS2_BUILD_ID=\"$(BUILD_ID)-debug\"
  OBJDIR  := $(BUILD)/debug
else
  DEFINES += -DPS2_BUILD_ID=\"$(BUILD_ID)\"
endif
ifeq ($(MONOLITICO),1)
  OBJDIR  := $(OBJDIR)/monolitico
endif
# Pagina de audio en el panel (solo DEBUG o DEV)
DEBUG_AUDIO ?= 0
ifeq ($(DEBUG_AUDIO),1)
  ifneq ($(DEBUG),1)
    $(error DEBUG_AUDIO=1 necesita DEBUG=1 o DEV=1)
  endif
  DEFINES += -DSMK64_DEBUG_AUDIO=1
  OBJDIR  := $(OBJDIR)/debug_audio
endif
# Panel de rendimiento en pantalla (L3 + R3)
MEDIDOR ?= $(if $(filter 1,$(DEV)),0,$(DEBUG))
ifeq ($(MEDIDOR),1)
  EXTRA_SRC += codigo/depuracion/medidor_rendimiento.c codigo/depuracion/texto_pantalla.c \
               codigo/depuracion/estadisticas_memoria.c
  DEFINES += -DSMK64_MEDIDOR=1
endif

# build/ps2/be primero: texturas u16 con los bytes invertidos
INCLUDES := -I$(BUILD)/be -Iincluir -Iincluir/libultra -I$(BUILD) -I$(BUILD)/include -Icodigo -I. \
            -I$(PS2SDK)/ee/include -I$(PS2SDK)/common/include -I$(PS2DEV)/gsKit/include
ifneq ($(EXTRA_INCLUDES),)
  INCLUDES += $(EXTRA_INCLUDES)
endif

WARNINGS := -Wall -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-function \
            -Wno-missing-braces -Wno-unknown-pragmas -Wno-main -Wno-builtin-declaration-mismatch \
            -Wno-maybe-uninitialized -Wno-array-bounds -Wno-stringop-overflow -Wno-int-conversion \
            -Wno-incompatible-pointer-types -Wno-implicit-function-declaration -Wno-pointer-sign \
            -Wno-address-of-packed-member -Wno-format -Wno-parentheses

OPT      ?= -O2
CFLAGS   := $(OPT) -G0 -fno-strict-aliasing -fno-common -fwrapv -ffast-math -fno-reciprocal-math $(WARNINGS) $(DEFINES) $(INCLUDES)
DATAFLAGS := -O1 -G0 -fno-toplevel-reorder -fno-common -fno-zero-initialized-in-bss -w $(DEFINES) $(INCLUDES)
ASFLAGS  := -G0 -msingle-float -march=r5900 -I incluir/juego -I . -I $(BUILD) --defsym VERSION_US=1

LDFLAGS  := -T compilacion/ps2.ld -L$(BUILD) -L$(PS2SDK)/ee/lib -L$(PS2DEV)/gsKit/lib \
            -Wl,-zmax-page-size=128 -Wl,--gc-sections -Wl,-Map,$(OBJDIR)/smk64.map
# Doble precision por software: caminos rapidos primero
SOFTDOUBLE_RAPIDO := __adddf3 __subdf3 __muldf3 __extendsfdf2 __truncdfsf2 __floatsidf __fixdfsi \
                     __ltdf2 __gtdf2 __ledf2 __gedf2 __eqdf2 __nedf2
LDFLAGS += $(foreach f,$(SOFTDOUBLE_RAPIDO),-Wl,--wrap=$(f))
ifneq ($(findstring SMK64_PROF,$(EXTRA_DEFINES)),)
  LDFLAGS += -Wl,--wrap=memcpy -Wl,--wrap=memset -Wl,--wrap=__divdf3
endif
LIBS     := -lgskit_toolkit -lgskit -ldmakit -laudsrv -lpad -lmc -lcdvd -ldebug -leedebug -lpatches -lkernel -lm -lc

# --- Codigo (en este orden de enlace) ------------------------------------------------

JUEGO_SRC := \
  codigo/sistema/bucle_principal.c codigo/carrera/preparacion_carrera.c codigo/sistema/perfilador.c \
  codigo/sistema/pantalla_fallo.c codigo/carrera/animacion.c codigo/carrera/repeticiones.c \
  codigo/carrera/camara.c codigo/graficos/dibujar_jugador.c codigo/graficos/texturas_kart.c \
  codigo/carrera/control_jugador.c codigo/carrera/aparicion_jugadores.c codigo/carrera/fisica_superficie.c \
  codigo/graficos/macros_gbi.c codigo/sistema/matematicas_2.c codigo/graficos/dibujar_objetos.c \
  codigo/carrera/objetos_y_efectos.c codigo/carrera/variables_objetos.c codigo/carrera/inicio_hud_y_objetos.c \
  codigo/carrera/actualizar_objetos.c codigo/carrera/utilidades_objetos.c codigo/carrera/efectos.c \
  codigo/carrera/camara_espectador.c codigo/graficos/vertices_luces_800AF9B0.c codigo/menus/menus.c \
  codigo/sistema/guardado.c \
  codigo/audio/sintesis.c codigo/audio/monton.c codigo/audio/carga.c codigo/audio/reproduccion.c \
  codigo/audio/efectos.c codigo/audio/reproductor_secuencias.c codigo/audio/externo.c \
  codigo/audio/puerto_eu.c codigo/audio/datos.c codigo/audio/ajustes_sesion.c \
  codigo/datos/metadatos_caminos.c codigo/datos/atributos_kart.c codigo/datos/vertices_jugadores_y_listas.c \
  codigo/datos/luces_800E45C0.c codigo/datos/vertices_800E8700.c \
  codigo/memoria/pool_memoria.c codigo/memoria/aleatorio.c codigo/memoria/buffers.c \
  codigo/memoria/buffer_salida_graficos.c codigo/memoria/monton_audio.c codigo/memoria/tablas_trigonometricas.c \
  codigo/carrera/logica_carrera.c codigo/graficos/dibujar_pistas.c codigo/carrera/actores.c \
  codigo/graficos/cielo_y_pantalla_dividida.c codigo/memoria/memoria_carrera.c codigo/carrera/colision.c \
  codigo/carrera/actores_extendidos.c codigo/sistema/matematicas.c \
  codigo/datos/tabla_pistas.c \
  codigo/ceremonia/bucle_creditos.c codigo/ceremonia/actores_podio.c codigo/ceremonia/camara_ceremonia.c \
  codigo/ceremonia/carga_ceremonia.c codigo/ceremonia/dibujar_podio.c codigo/ceremonia/ceremonia_y_creditos.c \
  codigo/ceremonia/inicio_rdp_ceremonia.c

# Texto japones: el juego lo espera en EUC-JP (tambien sus partes .inc.c)
JP_SRC := codigo/carrera/ia/ia_vehiculos_y_camara.c codigo/menus/elementos_menu.c codigo/ceremonia/creditos.c
JP_PARTES := $(foreach f,$(JP_SRC),$(wildcard $(basename $(f))/*.inc.c))

# libultra portable (matematicas, printf, cadenas, tablas)
LIBULTRA_SRC := \
  codigo/sistema/libultra/guOrthoF.c codigo/sistema/libultra/guRotateF.c codigo/sistema/libultra/guScaleF.c \
  codigo/sistema/libultra/guPerspectiveF.c codigo/sistema/libultra/guLookAtF.c \
  codigo/sistema/libultra/guTranslateF.c codigo/sistema/libultra/guMtxCatL.c codigo/sistema/libultra/guMtxF2L.c \
  codigo/sistema/libultra/guNormalize.c codigo/sistema/libultra/guMtxCatF.c \
  codigo/sistema/libultra/math/sinf.c codigo/sistema/libultra/math/cosf.c codigo/sistema/libultra/_Printf.c \
  codigo/sistema/libultra/sprintf.c codigo/sistema/libultra/string.c codigo/sistema/libultra/_Litob.c \
  codigo/sistema/libultra/_Ldtob.c codigo/sistema/libultra/ldiv.c codigo/sistema/libultra/crc.c \
  codigo/sistema/libultra/osViTable.c codigo/sistema/libultra/osViData.c

# Capa de PS2: lo que en la N64 hacia el hardware
PS2_SRC := \
  codigo/audio/salida_audio.c codigo/audio/microcodigo_audio.c codigo/depuracion/depuracion_audio.c \
  codigo/depuracion/monitor_audio.c codigo/depuracion/guiones_prueba.c codigo/graficos/combinador_color.c \
  codigo/depuracion/depuracion.c codigo/sistema/descompresion.c codigo/graficos/interprete_f3dex.c \
  codigo/graficos/sintetizador_gs.c codigo/entrada/mandos.c codigo/sistema/arranque_ps2.c \
  codigo/sistema/memory_card.c codigo/sistema/hardware.c codigo/sistema/hilos.c \
  codigo/sistema/retrazo_vertical.c codigo/sistema/ritmo_fisica.c codigo/sistema/carga_rom.c \
  codigo/sistema/tareas_rsp.c codigo/depuracion/muestreo_cpu.c codigo/sistema/guardado_ps2.c \
  codigo/sistema/segmentos.c codigo/sistema/doble_precision.c codigo/sistema/cronometro_fases.c \
  codigo/graficos/memoria_texturas.c codigo/graficos/pantallas_gigantes.c \
  codigo/carrera/ia/caminos_vehiculos.c \
  codigo/sistema/descompresion_tkmk00.c codigo/sistema/descompresion_mio0.c $(EXTRA_SRC)

# Caminos 2D del tren y del barco, calculados al compilar
CAMINOS := $(BUILD)/tabla_caminos_vehiculos.h
CAMINOS_SRC := recursos/pistas/kalimari_desert/datos_pista.c recursos/pistas/dks_jungle_parkway/datos_pista.c

CODE_OBJS := $(addprefix $(OBJDIR)/,$(JUEGO_SRC:.c=.o) $(LIBULTRA_SRC:.c=.o) $(PS2_SRC:.c=.o)) \
             $(OBJDIR)/codigo/datos/datos_embebidos.o $(addprefix $(OBJDIR)/jp/,$(JP_SRC:.c=.o))

# --- Datos de la ROM --------------------------------------------------------------

PISTAS := mario_raceway choco_mountain bowsers_castle banshee_boardwalk yoshi_valley frappe_snowland \
          koopa_troopa_beach royal_raceway luigi_raceway moo_moo_farm toads_turnpike kalimari_desert \
          sherbet_land rainbow_road wario_stadium block_fort skyscraper double_deck dks_jungle_parkway big_donut

KARTS := luigi mario yoshi peach wario toad donkey_kong bowser

ROM_ASM := $(addprefix codigo/datos/karts/kart_,$(addsuffix .s,$(KARTS))) \
           codigo/datos/otras_texturas.s codigo/datos/texturas_seleccion.s codigo/datos/texturas_fuentes.s \
           codigo/datos/texturas_tkmk00.s codigo/datos/secuencias_musica.s codigo/datos/conjuntos_instrumentos.s
ROM_C    := codigo/datos/texturas.c codigo/datos/segmento_datos_2.c recursos/pistas/fantasmas_personal.c \
            $(foreach p,$(PISTAS),recursos/pistas/$(p)/desplazamientos.c)

MIO0_ELFS := $(BUILD)/recursos/comunes/datos_comunes.elf \
             $(BUILD)/recursos/ceremonia/datos_ceremonia.elf \
             $(BUILD)/recursos/logo_inicio/logo_inicio.elf \
             $(foreach p,$(PISTAS),$(BUILD)/recursos/pistas/$(p)/datos_pista.elf)

ROM_OBJS := $(addprefix $(BUILD)/,$(ROM_ASM:.s=.o) $(ROM_C:.c=.o)) \
            $(BUILD)/sonido/bancos_instrumentos.o $(BUILD)/sonido/muestras_audio.o \
            $(BUILD)/recursos/comunes/datos_comunes.mio0.o \
            $(BUILD)/recursos/ceremonia/datos_ceremonia.mio0.o \
            $(BUILD)/recursos/logo_inicio/logo_inicio.mio0.o \
            $(foreach p,$(PISTAS),$(BUILD)/recursos/pistas/$(p)/datos_pista.mio0.o \
                                  $(BUILD)/recursos/pistas/$(p)/geografia.mio0.o)

SWAP_STAMP := $(BUILD)/be/.stamp
# Cambiar las opciones recompila el codigo
FLAGS_STAMP := $(OBJDIR)/.cflags_$(shell echo '$(CFLAGS)' | md5sum | cut -c1-12)
ELF        := $(OBJDIR)/smk64.elf

# --- Objetivos --------------------------------------------------------------------

.PHONY: all elf iso clean herramientas test
.NOTINTERMEDIATE:

all: elf

elf: $(ELF)

ISO_NAME := compilaciones/SLUS_999.99.SuperMarioKart64$(if $(filter 1,$(DEBUG)),_DEBUG).iso
iso: $(ELF)
	$(V)ISO_ROM=$(if $(filter 1,$(ROM_STREAM)),$(OBJDIR)/SMK64ROM.BIN) \
	    ISO_NOMBRE_OPL=SLUS_999.99.SuperMarioKart64$(if $(filter 1,$(DEBUG)),_DEBUG).iso \
	    ISO_CONTENIDO=compilaciones/disco/contenido$(if $(filter 1,$(DEBUG)),_debug) \
	    sh herramientas/crear_iso.sh $(OBJDIR)/SLUS_999.99 $(ISO_NAME)

herramientas: $(MIO0TOOL) $(DLPACKER)

clean:
	rm -rf $(BUILD) $(HERRAMIENTAS)

# Pruebas en el PC: combinador de color y caminos del tren y del barco
PRUEBAS := $(BUILD)/pruebas
test: $(CAMINOS)
	@mkdir -p $(PRUEBAS)
	$(V)gcc -std=gnu99 -Wall -Wextra -O1 -D_LANGUAGE_C -DF3DEX_GBI -DTARGET_PS2 -Iincluir -Iincluir/libultra \
	    -o $(PRUEBAS)/prueba_combinador herramientas/pruebas/prueba_combinador.c codigo/graficos/combinador_color.c -lm
	$(V)$(PRUEBAS)/prueba_combinador
	$(V)gcc -std=gnu99 -Wall -O2 -ffp-contract=off -I$(BUILD) -DTABLA='"tabla_caminos_vehiculos.h"' \
	    -o $(PRUEBAS)/prueba_caminos_vehiculos herramientas/pruebas/prueba_caminos_vehiculos.c -lm
	$(V)$(PRUEBAS)/prueba_caminos_vehiculos

# --- Herramientas del PC ------------------------------------------------------------

HOST_CFLAGS := -Iincluir -Wall -Wextra -Wno-unused-parameter -pedantic -std=c99 -O2 -s

$(MIO0TOOL): codigo/sistema/descompresion_mio0.c incluir/sistema/descompresion_mio0.h
	@mkdir -p $(dir $@)
	$(V)gcc $(HOST_CFLAGS) -DMIO0_STANDALONE $< -o $@

$(DLPACKER): herramientas/empaquetador_listas.c
	@mkdir -p $(dir $@)
	$(V)gcc $(HOST_CFLAGS) -Wno-unused-result -Iincluir/libultra -DF3DEX_GBI=1 -D_LANGUAGE_C=1 $< -o $@

# --- Texturas u16 con los bytes invertidos -------------------------------------------

SWAP_SOURCES := $(shell find codigo recursos -name '*.c' 2>/dev/null)
$(SWAP_STAMP): herramientas/invertir_texturas.py $(SWAP_SOURCES)
	@mkdir -p $(dir $@)
	$(V)$(PYTHON) herramientas/invertir_texturas.py --stamp $@ $(BUILD)/be codigo recursos

# --- Compilacion ----------------------------------------------------------------------

$(BUILD)/jp/%.c: %.c
	@mkdir -p $(dir $@)
	$(V)iconv -f UTF-8 -t EUC-JP $< > $@

$(addprefix $(BUILD)/jp/,$(JP_PARTES)): $(BUILD)/jp/%: %
	@mkdir -p $(dir $@)
	$(V)iconv -f UTF-8 -t EUC-JP $< > $@

$(addprefix $(OBJDIR)/jp/,$(JP_SRC:.c=.o)): $(addprefix $(BUILD)/jp/,$(JP_PARTES))

$(OBJDIR)/jp/%.o: $(BUILD)/jp/%.c $(FLAGS_STAMP) $(SWAP_STAMP)
	@mkdir -p $(dir $@)
	@echo "  CC(jp)  $<"
	$(V)$(CC) $(CFLAGS) -iquote $(dir $(patsubst $(BUILD)/jp/%,%,$<)) -MMD -MP -c $< -o $@

$(FLAGS_STAMP):
	@mkdir -p $(dir $@)
	@rm -f $(OBJDIR)/.cflags_*
	@touch $@

ifneq ($(OBJDIR),$(BUILD))
$(OBJDIR)/%.o: %.c $(FLAGS_STAMP) $(SWAP_STAMP)
	@mkdir -p $(dir $@)
	@echo "  CC      $<"
	$(V)$(CC) $(CFLAGS) -MMD -MP -c $< -o $@
endif

$(BUILD)/%.o: %.c $(FLAGS_STAMP) $(SWAP_STAMP)
	@mkdir -p $(dir $@)
	@echo "  CC      $<"
	$(V)$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/icono_partida.ico: herramientas/crear_icono.py
	@mkdir -p $(dir $@)
	$(V)$(PYTHON) $< $@

$(OBJDIR)/codigo/datos/datos_embebidos.o: codigo/datos/datos_embebidos.s $(BUILD)/icono_partida.ico
	@mkdir -p $(dir $@)
	$(V)$(AS) $(ASFLAGS) -I$(PS2SDK)/iop/irx -I$(BUILD) --MD $(@:.o=.d) -o $@ $<

$(CAMINOS): herramientas/generar_caminos_vehiculos.py $(CAMINOS_SRC)
	@mkdir -p $(dir $@)
	@echo "  GEN     $@"
	$(V)$(PYTHON) herramientas/generar_caminos_vehiculos.py $@
$(OBJDIR)/codigo/carrera/ia/caminos_vehiculos.o: $(CAMINOS)

$(BUILD)/%.o: %.s
	@mkdir -p $(dir $@)
	@echo "  AS      $<"
	$(V)$(AS) $(ASFLAGS) --MD $(@:.o=.d) -o $@ $<

# Datos: todo a .data (ver DATAFLAGS)
$(addprefix $(BUILD)/,$(ROM_C:.c=.o)): $(BUILD)/%.o: %.c $(SWAP_STAMP)
	@mkdir -p $(dir $@)
	@echo "  CC(dat) $<"
	$(V)$(CC) $(DATAFLAGS) -MMD -MP -c $< -o $@

# --- Segmentos comprimidos con MIO0 -------------------------------------------------
# Cada bloque se enlaza en su direccion segmentada, se extrae y se comprime.

$(BUILD)/%.data.o: %.c $(SWAP_STAMP)
	@mkdir -p $(dir $@)
	@echo "  CC(dat) $<"
	$(V)$(CC) $(DATAFLAGS) -MMD -MP -c $< -o $@

define SEG_ELF
	$(V)$(LD) -e 0 -T compilacion/segmento_datos.ld --section-start=.data=$(1) $(2) -o $@ $< --no-check-sections
endef

$(BUILD)/recursos/comunes/datos_comunes.elf: $(BUILD)/recursos/comunes/datos_comunes.data.o
	$(call SEG_ELF,0x0D000000,)
$(BUILD)/recursos/ceremonia/datos_ceremonia.elf: $(BUILD)/recursos/ceremonia/datos_ceremonia.data.o
	$(call SEG_ELF,0x0B000000,)
$(BUILD)/recursos/logo_inicio/logo_inicio.elf: $(BUILD)/recursos/logo_inicio/logo_inicio.data.o
	$(call SEG_ELF,0x06000000,)

$(BUILD)/recursos/pistas/%/texturas.linkonly.elf: $(BUILD)/recursos/pistas/%/texturas.linkonly.data.o
	$(call SEG_ELF,0x05000000,)
$(BUILD)/recursos/pistas/%/listas_dibujo.inc.elf: $(BUILD)/recursos/pistas/%/listas_dibujo.inc.data.o \
                                                  $(BUILD)/recursos/pistas/%/texturas.linkonly.elf
	$(call SEG_ELF,0x07000000,-R $(BUILD)/recursos/pistas/$*/texturas.linkonly.elf)
$(BUILD)/recursos/pistas/%/vertices.inc.elf: $(BUILD)/recursos/pistas/%/vertices.inc.data.o
	$(call SEG_ELF,0x0F000000,)
$(BUILD)/recursos/pistas/%/datos_pista.elf: $(BUILD)/recursos/pistas/%/datos_pista.data.o \
                                            $(BUILD)/recursos/pistas/%/listas_dibujo.inc.elf
	$(call SEG_ELF,0x06000000,-R $(BUILD)/recursos/pistas/$*/listas_dibujo.inc.elf)

# Los .inc.c de cada pista se compilan como unidad propia
$(BUILD)/recursos/pistas/%.inc.data.o: recursos/pistas/%.inc.c $(SWAP_STAMP)
	@mkdir -p $(dir $@)
	@echo "  CC(dat) $<"
	$(V)$(CC) $(DATAFLAGS) -MMD -MP -c -x c $< -o $@

%.bin: %.elf
	$(V)$(OBJCOPY) -O binary --only-section=.data $< $@

%.mio0: %.bin $(MIO0TOOL)
	$(V)$(MIO0TOOL) -c $< $@ > /dev/null

# Listas de dibujo de las pistas: formato empaquetado de MK64
$(BUILD)/recursos/pistas/%/listas_empaquetadas.inc.bin: $(BUILD)/recursos/pistas/%/listas_dibujo.inc.bin $(DLPACKER)
	$(V)$(DLPACKER) -le $< $@ > /dev/null

$(BUILD)/recursos/pistas/%/geografia.mio0.s: $(BUILD)/recursos/pistas/%/vertices.inc.mio0 \
                                             $(BUILD)/recursos/pistas/%/listas_empaquetadas.inc.bin
	$(V)printf '.include "macros.inc"\n.section .data\n.balign 4\nglabel d_circuito_$*_vertice\n.incbin "$(BUILD)/recursos/pistas/$*/vertices.inc.mio0"\n.balign 4\nglabel d_circuito_$*_empaquetado\n.incbin "$(BUILD)/recursos/pistas/$*/listas_empaquetadas.inc.bin"\n.balign 0x10\n' > $@

$(BUILD)/recursos/pistas/%/datos_pista.mio0.s: $(BUILD)/recursos/pistas/%/datos_pista.mio0
	$(V)printf '.section .data\n.balign 4\n.incbin "$<"\n' > $@

$(BUILD)/recursos/ceremonia/datos_ceremonia.mio0.s: $(BUILD)/recursos/ceremonia/datos_ceremonia.mio0
	$(V)printf '.include "macros.inc"\n.data\n.balign 4\nglabel datos_ceremonia\n.incbin "$<"\n.balign 16\nglabel fin_datos_ceremonia\n' > $@

$(BUILD)/recursos/logo_inicio/logo_inicio.mio0.s: $(BUILD)/recursos/logo_inicio/logo_inicio.mio0
	$(V)printf '.include "macros.inc"\n.data\n.balign 4\nglabel logo_inicio\n.incbin "$<"\n.balign 16\nglabel fin_logo_inicio\n' > $@

$(BUILD)/recursos/comunes/datos_comunes.mio0.s: $(BUILD)/recursos/comunes/datos_comunes.mio0
	$(V)printf '.section .data\n.balign 4\n.incbin "$<"\n' > $@

$(BUILD)/%.mio0.o: $(BUILD)/%.mio0.s
	$(V)$(AS) $(ASFLAGS) -o $@ $<

# --- Audio: bancos pasados a little-endian --------------------------------------------

$(BUILD)/sonido/bancos_instrumentos.le.bin $(BUILD)/sonido/muestras_audio.le.bin &: \
        recursos/sonido/bancos_instrumentos.bin recursos/sonido/muestras_audio.bin herramientas/invertir_audio.py
	@mkdir -p $(BUILD)/sonido
	$(V)$(PYTHON) herramientas/invertir_audio.py recursos/sonido/bancos_instrumentos.bin recursos/sonido/muestras_audio.bin \
	    $(BUILD)/sonido/bancos_instrumentos.le.bin $(BUILD)/sonido/muestras_audio.le.bin

$(BUILD)/sonido/%.o: $(BUILD)/sonido/%.le.bin
	$(V)printf '.section .data\n.incbin "$<"\n' | $(AS) $(ASFLAGS) -o $@ -

# --- La ROM ------------------------------------------------------------------------------

$(BUILD)/rom.ld: herramientas/generar_rom_ld.py
	@mkdir -p $(dir $@)
	$(V)$(PYTHON) herramientas/generar_rom_ld.py $(BUILD) > $@

$(BUILD)/rom.elf: $(BUILD)/rom.ld $(ROM_OBJS) $(MIO0_ELFS)
	@echo "  LD      rom.elf"
	$(V)$(LD) -T $(BUILD)/rom.ld $(foreach e,$(MIO0_ELFS),-R $(e)) -Map $(BUILD)/rom.map -o $@ --no-check-sections

$(BUILD)/rom.bin: $(BUILD)/rom.elf
	$(V)$(OBJCOPY) -O binary $< $@
	@echo "  ROM     $$(stat -c %s $@) bytes"

# ROM entera (monolitico) o solo la cabecera (el resto en SMK64ROM.BIN)
$(BUILD)/rom_cabecera.bin $(BUILD)/SMK64ROM.BIN &: $(BUILD)/rom.bin $(BUILD)/rom.elf herramientas/partir_rom.py
	$(V)$(PYTHON) herramientas/partir_rom.py $(BUILD)/rom.bin \
	    0x$$($(NM) $(BUILD)/rom.elf | awk '/ __rom_stream_start$$/ {print $$1}') \
	    $(BUILD)/rom_cabecera.bin $(BUILD)/SMK64ROM.BIN

$(BUILD)/rom_blob_full.o: $(BUILD)/rom.bin
	$(V)printf '.section .rom_blob, "a"\n.balign 16\n.incbin "$<"\n' | $(AS) $(ASFLAGS) -o $@ -

$(BUILD)/rom_blob_head.o: $(BUILD)/rom_cabecera.bin
	$(V)printf '.section .rom_blob, "a"\n.balign 16\n.incbin "$<"\n' | $(AS) $(ASFLAGS) -o $@ -

ROM_BLOB := $(BUILD)/rom_blob_$(if $(filter 1,$(ROM_STREAM)),head,full).o

# _xxxSegmentRomStart = __rom_start + desplazamiento en la imagen
$(BUILD)/rom_syms.ld: $(BUILD)/rom.elf
	$(V)$(NM) $< | awk '/ __romseg_.*_start$$/ { n=$$3; sub(/^__romseg_/,"",n); sub(/_start$$/,"",n); \
	    printf "_%sSegmentRomStart = __rom_start + 0x%s;\n", n, $$1 } \
	    / __romseg_.*_end$$/ { n=$$3; sub(/^__romseg_/,"",n); sub(/_end$$/,"",n); \
	    printf "_%sSegmentRomEnd = __rom_start + 0x%s;\n", n, $$1 } \
	    / __rom_size$$/ { printf "__rom_total_size = 0x%s;\n", $$1 }' > $@

# --- Enlace final ------------------------------------------------------------------------

$(ELF): $(CODE_OBJS) $(ROM_BLOB) $(BUILD)/rom_syms.ld $(BUILD)/rom.elf compilacion/ps2.ld \
        $(if $(filter 1,$(ROM_STREAM)),$(BUILD)/SMK64ROM.BIN)
	@echo "  LD      $@"
	$(V)$(CC) $(LDFLAGS) -Wl,-R,$(BUILD)/rom.elf $(foreach e,$(MIO0_ELFS),-Wl,-R,$(e)) \
	    -o $@ $(CODE_OBJS) $(ROM_BLOB) $(LIBS)
	$(V)$(STRIP) -o $(OBJDIR)/SLUS_999.99 $@
	$(V)$(if $(filter-out $(BUILD),$(OBJDIR)),$(if $(filter 1,$(ROM_STREAM)),cp $(BUILD)/SMK64ROM.BIN $(OBJDIR)/SMK64ROM.BIN,rm -f $(OBJDIR)/SMK64ROM.BIN),true)
	@echo "  OK      $@ ($$(stat -c %s $(OBJDIR)/SLUS_999.99) bytes sin simbolos$(if $(filter 1,$(ROM_STREAM)), + SMK64ROM.BIN $$(stat -c %s $(OBJDIR)/SMK64ROM.BIN) bytes))"

-include $(shell find $(BUILD) -name '*.d' 2>/dev/null)

print-%:
	@echo $($*)
