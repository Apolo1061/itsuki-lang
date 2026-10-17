#!/usr/bin/env bash
# ===========================================================
#        ITSUKI COMPILER - BUILD (LINUX)
# ===========================================================
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Colores ANSI
CYAN='\033[0;36m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

echo ""
echo -e "${CYAN}=======================================================${NC}"
echo -e "${CYAN}         ITSUKI COMPILER - BUILD (LINUX)               ${NC}"
echo -e "${CYAN}=======================================================${NC}"
echo ""

# 25%
echo -e "${YELLOW} [====----------------]  25%  Detectando arquitectura y toolchain...${NC}"
HOST_ARCH=$(uname -m)
GCC_ARCH=$(gcc -dumpmachine 2>/dev/null || echo "desconocido")

# 50%
echo -e "${YELLOW} [========------------]  50%  Verificando archivos fuente...${NC}"
if [ ! -f "src/main.c" ]; then
    echo -e "${RED}[ERROR] No se encontraron los archivos fuente en src/${NC}"
    exit 1
fi

# 75%
echo -e "${YELLOW} [============--------]  75%  Compilando codigo C nativo con GCC...${NC}"
gcc -Wall -Wextra -Wpedantic -Os -s -fno-ident -fno-asynchronous-unwind-tables -Iheaders src/*.c -o itsuki

# 100%
echo -e "${GREEN} [====================] 100%  Compilacion exitosa.${NC}"
echo ""
echo -e "${GREEN}-------------------------------------------------------${NC}"
echo -e "${GREEN} [OK] ./itsuki generado exitosamente ($HOST_ARCH | GCC: $GCC_ARCH)${NC}"
echo -e "${GREEN}-------------------------------------------------------${NC}"
echo ""
echo "  Uso: ./itsuki run archivo.suki  o  ./itsuki --help"
echo ""
