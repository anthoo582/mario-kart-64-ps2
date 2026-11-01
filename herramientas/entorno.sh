# Carga el toolchain de PS2: . herramientas/entorno.sh
# : "${PS2DEV:=/usr/local/ps2dev}"
if [ ! -x "$PS2DEV/ee/bin/mips64r5900el-ps2-elf-gcc" ]; then
    echo "entorno.sh: no hay toolchain de PS2 en $PS2DEV" >&2
    return 1 2>/dev/null || exit 1
fi
export PS2DEV
export PS2SDK="$PS2DEV/ps2sdk"
export GSKIT="$PS2DEV/gsKit"
case ":$PATH:" in
    *":$PS2DEV/ee/bin:"*) ;;
    *) export PATH="$PS2DEV/bin:$PS2DEV/ee/bin:$PS2DEV/iop/bin:$PS2DEV/dvp/bin:$PS2SDK/bin:$PATH" ;;
esac
