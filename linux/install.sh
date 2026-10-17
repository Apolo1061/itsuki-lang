#!/usr/bin/env bash
# ===========================================================
#        ITSUKI PROGRAMMING LANGUAGE - LINUX INSTALLER
# ===========================================================

set -e

# Colores para la terminal
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

echo -e "${CYAN}===========================================================${NC}"
echo -e "${CYAN}       ITSUKI PROGRAMMING LANGUAGE - INSTALADOR LINUX      ${NC}"
echo -e "${CYAN}===========================================================${NC}"
echo ""

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# 1. Localizar binario y biblioteca estandar
BIN_SRC=""
for cand in "$SCRIPT_DIR/itsuki" "$SCRIPT_DIR/build/itsuki" "$SCRIPT_DIR/build/linux/itsuki" "$SCRIPT_DIR/linux/itsuki"; do
    if [ -f "$cand" ]; then
        BIN_SRC="$cand"
        break
    fi
done

if [ -z "$BIN_SRC" ]; then
    echo -e "${RED}[ERROR] No se pudo encontrar el binario 'itsuki'.${NC}"
    echo "Asegurate de compilar el proyecto antes de ejecutar el instalador."
    exit 1
fi

LIB_SRC=""
for cand in "$SCRIPT_DIR/lib" "$SCRIPT_DIR/linux/lib" "$SCRIPT_DIR/build/linux/lib"; do
    if [ -d "$cand" ]; then
        LIB_SRC="$cand"
        break
    fi
done

echo -e "${YELLOW}[1/5] Localizando archivos:${NC}"
echo "      Binario: $BIN_SRC"
[ -n "$LIB_SRC" ] && echo "      Librerias: $LIB_SRC"

# 2. Determinar modo de instalacion (Root o Usuario normal)
if [ "$EUID" -eq 0 ]; then
    INSTALL_MODE="system"
    INSTALL_DIR="/usr/local"
    BIN_DIR="/usr/local/bin"
    LIB_DIR="/usr/local/share/itsuki/lib"
    PKG_DIR="/var/lib/itsuki/packages"
    echo -e "${YELLOW}[2/5] Modo de instalacion: Sistema global (/usr/local)${NC}"
else
    INSTALL_MODE="user"
    INSTALL_DIR="$HOME/.itsuki"
    BIN_DIR="$HOME/.itsuki/bin"
    LIB_DIR="$HOME/.itsuki/lib"
    PKG_DIR="$HOME/.itsuki/packages"
    echo -e "${YELLOW}[2/5] Modo de instalacion: Usuario actual ($INSTALL_DIR)${NC}"
fi

mkdir -p "$BIN_DIR" "$LIB_DIR" "$PKG_DIR"

# 3. Copiar binario y librerias
echo -e "${YELLOW}[3/5] Instalando ejecutables y bibliotecas...${NC}"
cp -f "$BIN_SRC" "$BIN_DIR/itsuki"
chmod +x "$BIN_DIR/itsuki"

if [ -n "$LIB_SRC" ]; then
    cp -rf "$LIB_SRC"/* "$LIB_DIR/"
fi

# 4. Configurar PATH si es instalacion de usuario
echo -e "${YELLOW}[4/5] Verificando variable de entorno PATH...${NC}"
if [ "$INSTALL_MODE" = "user" ]; then
    PATH_LINE="export PATH=\"\$HOME/.itsuki/bin:\$PATH\""
    ADDED=0
    for rc in "$HOME/.bashrc" "$HOME/.zshrc" "$HOME/.profile"; do
        if [ -f "$rc" ]; then
            if ! grep -q ".itsuki/bin" "$rc"; then
                echo "" >> "$rc"
                echo "# Itsuki Programming Language" >> "$rc"
                echo "$PATH_LINE" >> "$rc"
                ADDED=1
                echo -e "      ${GREEN}[OK] Agregado a $rc${NC}"
            fi
        fi
    done
    if [ "$ADDED" -eq 0 ]; then
        echo -e "      ${GREEN}[INFO] PATH ya estaba configurado.${NC}"
    fi
    export PATH="$BIN_DIR:$PATH"
else
    echo -e "      ${GREEN}[OK] Instalado en /usr/local/bin (disponible en PATH para todos los usuarios).${NC}"
fi

# 5. Generar script de desinstalacion
UNINSTALL_PATH="$INSTALL_DIR/uninstall.sh"
if [ "$INSTALL_MODE" = "system" ]; then
    UNINSTALL_PATH="/usr/local/bin/itsuki-uninstall"
fi

cat << 'EOF' > "$UNINSTALL_PATH"
#!/usr/bin/env bash
RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'

echo -e "${RED}===========================================================${NC}"
echo -e "${RED}       DESINSTALADOR DE ITSUKI PROGRAMMING LANGUAGE        ${NC}"
echo -e "${RED}===========================================================${NC}"
read -p "¿Deseas desinstalar Itsuki de tu sistema? (s/N): " CONFIRM
if [[ ! "$CONFIRM" =~ ^[sS]$ ]]; then
    echo "Desinstalacion cancelada."
    exit 0
fi

if [ -f "/usr/local/bin/itsuki" ]; then
    sudo rm -f "/usr/local/bin/itsuki" "/usr/local/bin/itsuki-uninstall"
    sudo rm -rf "/usr/local/share/itsuki"
    echo -e "${GREEN}[OK] Itsuki eliminado de /usr/local.${NC}"
fi

if [ -d "$HOME/.itsuki" ]; then
    rm -rf "$HOME/.itsuki/bin" "$HOME/.itsuki/lib"
    echo -e "${GREEN}[OK] Itsuki eliminado de $HOME/.itsuki.${NC}"
fi

echo -e "${GREEN}[OK] Itsuki ha sido desinstalado correctamente.${NC}"
EOF
chmod +x "$UNINSTALL_PATH"

# Comprobar dependencias (GCC y Git)
echo ""
echo -e "${YELLOW}[5/5] Comprobando dependencias:${NC}"
if command -v gcc >/dev/null 2>&1; then
    echo -e "      ${GREEN}[OK] GCC detectado: $(command -v gcc)${NC}"
else
    echo -e "      ${RED}[ADVERTENCIA] GCC no encontrado. Instala gcc con tu gestor (ej: apt install gcc gcc-multilib)${NC}"
fi

if command -v git >/dev/null 2>&1; then
    echo -e "      ${GREEN}[OK] Git detectado: $(command -v git)${NC}"
else
    echo -e "      ${RED}[ADVERTENCIA] Git no encontrado. Instala git para usar 'itsuki install'.${NC}"
fi

echo ""
echo -e "${GREEN}===========================================================${NC}"
echo -e "${GREEN}       ¡INSTALACION COMPLETADA EXITOSAMENTE!               ${NC}"
echo -e "${GREEN}===========================================================${NC}"
echo "  Ejecutable: $BIN_DIR/itsuki"
echo "  Runtime   : $LIB_DIR"
echo "  Paquetes  : $PKG_DIR"
echo ""
"$BIN_DIR/itsuki" --version || true
echo ""
echo -e "${CYAN}Para empezar a programar en Itsuki:${NC}"
echo "  1. Abre una nueva terminal o ejecuta:"
if [ "$INSTALL_MODE" = "user" ]; then
    echo "     export PATH=\"\$HOME/.itsuki/bin:\$PATH\""
fi
echo "  2. Comprueba tu instalacion:"
echo "     itsuki --help"
echo "     itsuki list"
echo "     itsuki run programa.suki"
echo -e "${GREEN}===========================================================${NC}"
