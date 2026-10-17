#!/bin/bash
set -e

echo "============================================="
echo "   Compilando Itsuki Compiler para Linux     "
echo "============================================="

# Compilar ejecutable nativo optimizado en tamano y velocidad (-Os -s)
gcc -Wall -Wextra -Wpedantic -Os -s -fno-ident -fno-asynchronous-unwind-tables -Iheaders src/*.c -o itsuki

echo ""
echo "[OK] Compilador Itsuki generado con exito: ./itsuki"
./itsuki --version
