#!/usr/bin/env bash
# ===========================================================
#        ITSUKI COMPILER - INSTALADOR (LINUX)
#    100% Espacio de Usuario - Sin requerir 'sudo' o root
# ===========================================================
set -e

# Colores ANSI
CYAN='\033[0;36m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
GRAY='\033[0;90m'
NC='\033[0m'

echo ""
echo -e "${CYAN}=======================================================${NC}"
echo -e "${CYAN}         ITSUKI COMPILER - INSTALADOR (LINUX)          ${NC}"
echo -e "${CYAN}=======================================================${NC}"
echo ""

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# -----------------------------------------------------------
# [1/5] 20% Deteccion de arquitectura y GCC
# -----------------------------------------------------------
echo -e "${YELLOW} [====----------------]  20%  Detectando arquitectura de la PC...${NC}"
HOST_RAW=$(uname -m)
case "$HOST_RAW" in
    x86_64|amd64)
        OS_BITS=64
        HOST_ARCH_NAME="x86_64"
        ;;
    i386|i486|i586|i686)
        OS_BITS=32
        HOST_ARCH_NAME="x86"
        ;;
    aarch64|arm64)
        OS_BITS=64
        HOST_ARCH_NAME="arm64"
        ;;
    arm*)
        OS_BITS=32
        HOST_ARCH_NAME="arm"
        ;;
    *)
        OS_BITS=32
        HOST_ARCH_NAME="$HOST_RAW"
        ;;
esac

GCC_PATH="gcc"
GCC_MACHINE="desconocido"
GCC_BITS=$OS_BITS
GCC_FLAGS=""
MULTILIB_OK=0

if command -v gcc >/dev/null 2>&1; then
    GCC_MACHINE=$(gcc -dumpmachine 2>/dev/null || echo "desconocido")
    if [[ "$GCC_MACHINE" =~ 64 ]]; then
        GCC_BITS=64
    else
        GCC_BITS=32
    fi

    if [ "$OS_BITS" -eq 64 ]; then
        GCC_FLAGS="-m32 -no-pie"
        if echo "int main(void){return 0;}" | gcc -m32 -x c - -o /tmp/itsuki_probe >/dev/null 2>&1; then
            MULTILIB_OK=1
        else
            MULTILIB_OK=0
        fi
        rm -f /tmp/itsuki_probe 2>/dev/null || true
    else
        MULTILIB_OK=1
        GCC_FLAGS="-no-pie"
    fi
fi

# -----------------------------------------------------------
# [2/5] 40% Localizar archivos
# -----------------------------------------------------------
echo -e "${YELLOW} [========------------]  40%  Localizando archivos del compilador...${NC}"
BIN_SRC=""
for cand in "$SCRIPT_DIR/itsuki" "$SCRIPT_DIR/build/itsuki" "$SCRIPT_DIR/build/linux/itsuki" "$SCRIPT_DIR/linux/itsuki"; do
    if [ -f "$cand" ]; then
        BIN_SRC="$cand"
        break
    fi
done

if [ -z "$BIN_SRC" ]; then
    echo -e "${RED}[ERROR] No se pudo encontrar el binario 'itsuki'. Compila primero con ./build.sh${NC}"
    exit 1
fi

LIB_SRC=""
for cand in "$SCRIPT_DIR/lib" "$SCRIPT_DIR/linux/lib" "$SCRIPT_DIR/build/linux/lib"; do
    if [ -d "$cand" ]; then
        LIB_SRC="$cand"
        break
    fi
done

# -----------------------------------------------------------
# [3/5] 60% Preparar carpetas en espacio de usuario (~/.itsuki)
# Sin requerir 'sudo' ni permisos de administrador
# -----------------------------------------------------------
echo -e "${YELLOW} [============--------]  60%  Preparando directorios del sistema (~/.itsuki)...${NC}"
TARGET_HOME="${HOME:-/tmp}"
if [ -n "$SUDO_USER" ] && [ "$SUDO_USER" != "root" ]; then
    TARGET_HOME=$(getent passwd "$SUDO_USER" 2>/dev/null | cut -d: -f6 || echo "/home/$SUDO_USER")
fi

INSTALL_DIR="$TARGET_HOME/.itsuki"
BIN_DIR="$INSTALL_DIR/bin"
LIB_DIR="$INSTALL_DIR/lib"
CONFIG_DIR="$INSTALL_DIR"
PKG_DIR="$INSTALL_DIR/packages"

mkdir -p "$BIN_DIR" "$LIB_DIR" "$CONFIG_DIR" "$PKG_DIR"

# -----------------------------------------------------------
# [4/5] 80% Copiar binario y generar arch.conf
# -----------------------------------------------------------
echo -e "${YELLOW} [================----]  80%  Instalando binario, librerias y arch.conf...${NC}"
cp -f "$BIN_SRC" "$BIN_DIR/itsuki"
chmod +x "$BIN_DIR/itsuki"

if [ -n "$LIB_SRC" ]; then
    cp -rf "$LIB_SRC"/* "$LIB_DIR/"
fi

cat << EOF > "$CONFIG_DIR/arch.conf"
# ===========================================================
#      ITSUKI PROGRAMMING LANGUAGE - ARCH CONFIG
#   Generado automaticamente durante la instalacion Linux
# ===========================================================
os_name=linux
os_bits=$OS_BITS
host_arch=$HOST_ARCH_NAME
gcc_path=$GCC_PATH
gcc_arch=$GCC_MACHINE
gcc_bits=$GCC_BITS
multilib_supported=$MULTILIB_OK
gcc_flags=$GCC_FLAGS
EOF

# Desinstalador 100% en espacio de usuario
UNINSTALL_PATH="$CONFIG_DIR/uninstall.sh"
cat << EOF > "$UNINSTALL_PATH"
#!/usr/bin/env bash
read -p "¿Deseas desinstalar Itsuki de tu sistema? (s/N): " CONFIRM
if [[ ! "\$CONFIRM" =~ ^[sS]$ ]]; then exit 0; fi
rm -rf "$BIN_DIR" "$LIB_DIR" "$CONFIG_DIR/arch.conf" "$CONFIG_DIR/uninstall.sh" 2>/dev/null || true
echo "[OK] Itsuki ha sido desinstalado correctamente de tu usuario."
EOF
chmod +x "$UNINSTALL_PATH"

# -----------------------------------------------------------
# [5/5] 100% Configurar PATH
# -----------------------------------------------------------
echo -e "${GREEN} [====================] 100%  Configurando variable de entorno PATH...${NC}"
PATH_LINE="export PATH=\"\$HOME/.itsuki/bin:\$PATH\""
for rc in "$TARGET_HOME/.bashrc" "$TARGET_HOME/.zshrc" "$TARGET_HOME/.profile"; do
    if [ -f "$rc" ] && ! grep -q ".itsuki/bin" "$rc"; then
        echo "" >> "$rc"
        echo "# Itsuki Programming Language" >> "$rc"
        echo "$PATH_LINE" >> "$rc"
    fi
done
export PATH="$BIN_DIR:$PATH"

echo ""
echo -e "${GREEN}=======================================================${NC}"
echo -e "${GREEN} [OK] ITSUKI INSTALADO EXITOSAMENTE ($HOST_ARCH_NAME $OS_BITS-bit)${NC}"
echo -e "${GREEN}=======================================================${NC}"
echo -e "  Instalacion: Espacio de usuario (Sin sudo / root)"
echo -e "  Ejecutable : $BIN_DIR/itsuki"
echo -e "  Runtime    : $LIB_DIR"
echo -e "  Config     : $CONFIG_DIR/arch.conf"
echo -e "  Toolchain  : $GCC_MACHINE (flags: $GCC_FLAGS)"
echo -e "${GRAY}-------------------------------------------------------${NC}"
echo -e "  Comandos para comenzar:"
echo -e "    ${CYAN}itsuki run archivo.suki${NC}   Compilar y ejecutar"
echo -e "    ${CYAN}itsuki doctor${NC}             Ver estado del entorno"
echo -e "    ${CYAN}itsuki --help${NC}             Ver todas las opciones"
echo -e "${GREEN}=======================================================${NC}"
echo ""
