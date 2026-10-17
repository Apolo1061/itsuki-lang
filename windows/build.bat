@echo off
setlocal EnableDelayedExpansion
pushd "%~dp0"

echo.
echo  =======================================================
echo          ITSUKI COMPILER - BUILD (WINDOWS)
echo  =======================================================
echo.

echo  [====----------------]  25%%  Detectando arquitectura y toolchain...
set "ARCH_NAME=x86"
set "ARCH_BITS=32"
if "%PROCESSOR_ARCHITECTURE%"=="AMD64" set "ARCH_NAME=x86_64" & set "ARCH_BITS=64"
if "%PROCESSOR_ARCHITEW6432%"=="AMD64" set "ARCH_NAME=x86_64" & set "ARCH_BITS=64"
if "%PROCESSOR_ARCHITECTURE%"=="ARM64" set "ARCH_NAME=arm64" & set "ARCH_BITS=64"
set "GCC_ARCH=desconocido"
for /f "tokens=*" %%g in ('gcc -dumpmachine 2^>nul') do set "GCC_ARCH=%%g"

echo  [========------------]  50%%  Verificando archivos fuente...
if not exist "src\main.c" (
    echo.
    echo  [ERROR] No se encontraron los archivos fuente en src\
    popd
    exit /b 1
)

echo  [============--------]  75%%  Compilando codigo C nativo con GCC...
gcc -Wall -Wextra -Os -s -fno-ident -fno-asynchronous-unwind-tables -Iheaders src\*.c -o itsuki.exe
if !ERRORLEVEL! neq 0 (
    echo.
    echo  [ERROR] Fallo en la compilacion de itsuki.exe.
    popd
    exit /b 1
)

echo  [====================] 100%%  Compilacion exitosa.
echo.
echo  -------------------------------------------------------
echo   [OK] itsuki.exe generado exitosamente (%ARCH_NAME% %ARCH_BITS%-bit ^| GCC: %GCC_ARCH%)
echo  -------------------------------------------------------
echo.

if not "%~1"=="" (
    itsuki.exe run %*
) else (
    echo   Uso: itsuki run archivo.suki  o  itsuki --help
    echo.
)

popd
