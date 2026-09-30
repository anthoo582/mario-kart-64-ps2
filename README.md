# Mario Kart 64 PS2 Port

Port de Mario Kart 64 a PlayStation 2. Incluye el juego descompilado, una capa
que reemplaza el hardware de la N64 con el de la PS2 y todos los recursos
(texturas, pistas, karts, música y efectos) ya extraídos. No hace falta la ROM
para compilar.

Probado en PCSX2 (con BIOS real) y en Play!. Todavía sin probar en una PS2 real.

## Características

- Las 20 pistas, los 8 personajes y todos los modos: Gran Premio, Contrarreloj,
  VS de 2 a 4 jugadores y Batalla.
- Audio original: las muestras de la ROM mezcladas con la misma aritmética que
  la N64, con salida a 48 kHz.
- Modo 60 FPS opcional (R3). Agrega un cuadro intermedio solo si la CPU libre
  alcanza; la lógica sigue a 30 Hz como en el original.
- Guardado en Memory Card.
- Pantalla de fallo con el estado de cada hilo si el juego se cuelga.

## Requisitos

- Toolchain de PS2 ([ps2dev](https://github.com/ps2dev/ps2dev)): `mips64r5900el-ps2-elf-gcc`,
  ps2sdk y gsKit, instalados en `/usr/local/ps2dev` o en `$PS2DEV`.
- `gcc`, `make`, `python3` e `iconv` en el PC.
- `genisoimage`, solo para `make iso`.

## Compilar

```bash
. herramientas/entorno.sh
make -j8
make iso
```

`make` deja `build/ps2/SLUS_999.99` y `build/ps2/SMK64ROM.BIN`. `make iso` deja
`compilaciones/SLUS_999.99.SuperMarioKart64.iso`.

| Comando | Qué hace |
|---|---|
| `make DEBUG=1` | Panel de rendimiento en pantalla (L3 + R3) y registro por `printf` |
| `make DEV=1` | DEBUG + registro y guiones de prueba por `host:` (emuladores) |
| `make DEBUG=1 DEBUG_AUDIO=1` | DEBUG + página de audio en el panel |
| `make MONOLITICO=1` | Un solo ELF con toda la ROM adentro (uLaunchELF sin ISO) |
| `make EXTRA_DEFINES=-DFILTRADO_TEXTURAS=0` | Texturas sin suavizado (vecino más cercano) |
| `make test` | Pruebas en el PC (combinador de color, caminos del tren y del barco) |
| `make clean` | Borra `build/` |

## Ejecutar

- **PCSX2**: abrir la ISO de `compilaciones/`.
- **OPL**: copiar la ISO a la carpeta `CD/` del USB, HDD o SMB. `make iso` deja
  una copia con el nombre que espera OPL en `compilaciones/disco/OPL/CD/`.
  Lanzado desde OPL, el juego no reinicia el IOP (OPL ya lo dejó listo), lo que
  ahorra volver a montar el USB, el HDD o la red. A cambio no hay VMC, PADEMU ni
  IGR de OPL: la partida se guarda en la Memory Card física.
- **uLaunchELF**: `SLUS_999.99` necesita `SMK64ROM.BIN` al lado; si no, usar el
  ELF de `make MONOLITICO=1`.

### Controles

| N64 | DualShock 2 |
|---|---|
| A / B | Cruz / Cuadrado |
| Z | L1 o L2 |
| R | R1 o R2 |
| L | Select |
| Start | Start |
| Stick / cruceta | Stick izquierdo / cruceta |
| C arriba / abajo | Triángulo / Círculo |
| C izquierda / derecha | Stick derecho |
| — | R3: 60 FPS sí/no · L3 + R3: panel (solo DEBUG) |

## Estructura

| Carpeta | Contenido |
|---|---|
| `codigo/sistema/` | Arranque, bucle principal, hilos, retrazo, carga de la ROM, guardado, descompresión |
| `codigo/sistema/libultra/` | Parte portable de libultra (conserva sus nombres originales) |
| `codigo/graficos/` | Intérprete de listas de dibujo, memoria de texturas, combinador de color, salida al GS, dibujo de jugadores, objetos y pistas |
| `codigo/audio/` | Motor de sonido del juego, microcódigo de audio en C y salida por audsrv |
| `codigo/carrera/` | Lógica de carrera, física, colisiones, cámara, objetos, actores e IA |
| `codigo/menus/`, `codigo/ceremonia/` | Menús; podio y créditos |
| `codigo/entrada/` | Mandos |
| `codigo/memoria/` | Pools, buffers y tablas |
| `codigo/depuracion/` | Registro, guiones de prueba y panel de rendimiento (DEBUG/DEV) |
| `codigo/datos/` | Tablas del juego y listas de recursos binarios (`.s`) |
| `incluir/` | Cabeceras por área. `incluir/libultra/` es la API de la N64, sin traducir |
| `recursos/pistas/` | Las 20 pistas: modelos, listas de dibujo, vértices y metadatos |
| `recursos/texturas/` | Karts, personajes, Lakitu, objetos, menús, HUD y texturas de pistas |
| `recursos/sonido/` | Bancos de instrumentos, muestras y la música (`musica/*.m64`) |
| `recursos/comunes/`, `recursos/ceremonia/`, `recursos/logo_inicio/` | Bloques de datos que la ROM guarda comprimidos |
| `compilacion/` | Scripts del enlazador |
| `herramientas/` | Scripts del build (ROM, ISO, texturas, audio), compresor y pruebas |

El código se enlaza en RAM como en la N64. La "ROM" se arma aparte: cada bloque
de datos se enlaza en su dirección segmentada, se comprime con MIO0 y se guarda
en `SMK64ROM.BIN`, que el juego carga en segundo plano durante el logo.

## Contribuir

- Nombres en español y en `snake_case`. Los tipos van en `PascalCase` y las
  constantes en `MAYUSCULAS`.
- La API de libultra, el SDK de PS2 y los símbolos del enlazador conservan sus
  nombres originales.
- Funciones y datos sin identificar se llaman `funcion_<dirección>` y
  `dato_<dirección>`, con la dirección original de la N64.
- Comentarios cortos.
- Ningún archivo de código pasa de 1000 líneas. Si una parte crece, va a un
  archivo propio.
- Antes de enviar un cambio: `make clean && make` y `make test`.
