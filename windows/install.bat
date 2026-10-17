@echo off
setlocal EnableDelayedExpansion
title Instalador Oficial de Itsuki
color 0B

echo.
echo  =======================================================
echo          ITSUKI COMPILER - INSTALADOR (WINDOWS)
echo  =======================================================
echo.

set "SCRIPT_DIR=%~dp0"
set "BIN_SRC="
set "LIB_SRC="

rem -----------------------------------------------------------
rem [1/5] 20% Deteccion de arquitectura y toolchain
rem -----------------------------------------------------------
echo  [====----------------]  20%%  Detectando arquitectura de la PC...
set "ARCH_BITS=32"
set "ARCH_NAME=x86"
if "%PROCESSOR_ARCHITECTURE%"=="AMD64" set "ARCH_BITS=64" & set "ARCH_NAME=x86_64"
if "%PROCESSOR_ARCHITEW6432%"=="AMD64" set "ARCH_BITS=64" & set "ARCH_NAME=x86_64"
if "%PROCESSOR_ARCHITECTURE%"=="ARM64" set "ARCH_BITS=64" & set "ARCH_NAME=arm64"

set "GCC_FOUND=0"
set "GCC_MACHINE=desconocido"
set "GCC_BITS=%ARCH_BITS%"
set "GCC_FLAGS="
set "MULTILIB_OK=0"

where gcc >nul 2>&1
if !ERRORLEVEL! equ 0 (
    set "GCC_FOUND=1"
    for /f "tokens=*" %%g in ('gcc -dumpmachine 2^>nul') do set "GCC_MACHINE=%%g"
    echo !GCC_MACHINE! | findstr /i "64" >nul
    if !ERRORLEVEL! equ 0 (
        set "GCC_BITS=64"
        set "GCC_FLAGS=-m32"
    ) else (
        set "GCC_BITS=32"
        set "GCC_FLAGS="
    )

    rem Test silencioso de multilib
    set "PROBE_SRC=%TEMP%\itsuki_probe_%RANDOM%.c"
    set "PROBE_EXE=%TEMP%\itsuki_probe_%RANDOM%.exe"
    echo int main^(void^){return 0;} > "!PROBE_SRC!"
    if "!GCC_BITS!"=="64" (
        gcc -m32 "!PROBE_SRC!" -o "!PROBE_EXE!" >nul 2>&1
        if !ERRORLEVEL! equ 0 (
            set "MULTILIB_OK=1"
        ) else (
            set "MULTILIB_OK=0"
        )
    ) else (
        set "MULTILIB_OK=1"
    )
    if exist "!PROBE_SRC!" del /f /q "!PROBE_SRC!" >nul 2>&1
    if exist "!PROBE_EXE!" del /f /q "!PROBE_EXE!" >nul 2>&1
)

rem -----------------------------------------------------------
rem [2/5] 40% Localizar binario y runtime
rem -----------------------------------------------------------
echo  [========------------]  40%%  Localizando archivos del compilador...
if exist "%SCRIPT_DIR%itsuki.exe" (
    set "BIN_SRC=%SCRIPT_DIR%itsuki.exe"
) else if exist "%SCRIPT_DIR%windows\itsuki.exe" (
    set "BIN_SRC=%SCRIPT_DIR%windows\itsuki.exe"
) else if exist "%SCRIPT_DIR%build\itsuki.exe" (
    set "BIN_SRC=%SCRIPT_DIR%build\itsuki.exe"
) else if exist "%SCRIPT_DIR%build\windows\itsuki.exe" (
    set "BIN_SRC=%SCRIPT_DIR%build\windows\itsuki.exe"
)

if exist "%SCRIPT_DIR%lib" (
    set "LIB_SRC=%SCRIPT_DIR%lib"
) else if exist "%SCRIPT_DIR%windows\lib" (
    set "LIB_SRC=%SCRIPT_DIR%windows\lib"
) else if exist "%SCRIPT_DIR%build\windows\lib" (
    set "LIB_SRC=%SCRIPT_DIR%build\windows\lib"
)

if "%BIN_SRC%"=="" (
    echo.
    echo  [ERROR] No se encontro 'itsuki.exe'. Compila primero con build.bat.
    echo.
    pause
    exit /b 1
)

rem -----------------------------------------------------------
rem [3/5] 60% Preparar carpetas en ~/.itsuki
rem -----------------------------------------------------------
echo  [============--------]  60%%  Preparando directorios del sistema...
set "INSTALL_DIR=%USERPROFILE%\.itsuki"
set "BIN_DIR=%INSTALL_DIR%\bin"
set "LIB_DIR=%INSTALL_DIR%\lib"
set "PKG_DIR=%INSTALL_DIR%\packages"

if not exist "%INSTALL_DIR%" mkdir "%INSTALL_DIR%" >nul 2>&1
if not exist "%BIN_DIR%" mkdir "%BIN_DIR%" >nul 2>&1
if not exist "%LIB_DIR%" mkdir "%LIB_DIR%" >nul 2>&1
if not exist "%PKG_DIR%" mkdir "%PKG_DIR%" >nul 2>&1

rem -----------------------------------------------------------
rem [4/5] 80% Copiar archivos y guardar arch.conf
rem -----------------------------------------------------------
echo  [================----]  80%%  Instalando binario, librerias y arch.conf...
copy /Y "%BIN_SRC%" "%BIN_DIR%\itsuki.exe" >nul
if errorlevel 1 (
    echo.
    echo  [ERROR] No se pudo copiar itsuki.exe a %BIN_DIR%
    pause
    exit /b 1
)

if not "%LIB_SRC%"=="" (
    xcopy /E /I /Y "%LIB_SRC%\*" "%LIB_DIR%\" >nul 2>&1
)

(
    echo # ===========================================================
    echo #      ITSUKI PROGRAMMING LANGUAGE - ARCH CONFIG
    echo #   Generado automaticamente durante la instalacion
    echo # ===========================================================
    echo os_name=windows
    echo os_bits=%ARCH_BITS%
    echo host_arch=%ARCH_NAME%
    echo gcc_path=gcc
    echo gcc_arch=%GCC_MACHINE%
    echo gcc_bits=%GCC_BITS%
    echo multilib_supported=%MULTILIB_OK%
    echo gcc_flags=%GCC_FLAGS%
) > "%INSTALL_DIR%\arch.conf"

rem Generar desinstalador silencioso
(
    echo @echo off
    echo title Desinstalador de Itsuki
    echo.
    echo Desea desinstalar el compilador Itsuki de su sistema?
    echo Presione [S] para confirmar o cualquier otra tecla para cancelar.
    echo set /p CONFIRM="Opcion: "
    echo if /i not "%%CONFIRM%%"=="S" exit /b 0
    echo powershell -NoProfile -Command "$bin = [System.IO.Path]::GetFullPath('%BIN_DIR%'); $cur = [Environment]::GetEnvironmentVariable('Path', 'User'); if ($cur -like ('*' + $bin + '*')) { $p = $cur.Split(';') | Where-Object { $_ -ne $bin -and $_ -ne '' }; [Environment]::SetEnvironmentVariable('Path', ($p -join ';'), 'User'); }"
    echo rmdir /s /q "%BIN_DIR%" 2^>nul
    echo rmdir /s /q "%LIB_DIR%" 2^>nul
    echo del /f /q "%INSTALL_DIR%\arch.conf" 2^>nul
    echo [OK] Itsuki desinstalado.
    echo pause
) > "%INSTALL_DIR%\uninstall.bat"

rem -----------------------------------------------------------
rem [5/5] 100% Configurar PATH
rem -----------------------------------------------------------
echo  [====================] 100%%  Configurando variable de entorno PATH...
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
    "$target = [System.IO.Path]::GetFullPath('%BIN_DIR%');" ^
    "$current = [Environment]::GetEnvironmentVariable('Path', 'User');" ^
    "if ($current -notlike ('*' + $target + '*')) {" ^
    "    $newPath = if ([string]::IsNullOrEmpty($current)) { $target } else { $current.TrimEnd(';') + ';' + $target };" ^
    "    [Environment]::SetEnvironmentVariable('Path', $newPath, 'User');" ^
    "}" >nul 2>&1

echo.
echo  =======================================================
echo   [OK] ITSUKI INSTALADO EXITOSAMENTE (%ARCH_NAME% %ARCH_BITS%-bit)
echo  =======================================================
echo   Ejecutable : %BIN_DIR%\itsuki.exe
echo   Runtime    : %LIB_DIR%
echo   Config     : %INSTALL_DIR%\arch.conf
echo   Toolchain  : %GCC_MACHINE% (flags: %GCC_FLAGS%)
echo  -------------------------------------------------------
echo   Comandos para comenzar:
echo     itsuki run archivo.suki   Compilar y ejecutar
echo     itsuki doctor             Ver estado del entorno
echo     itsuki --help             Ver todas las opciones
echo  =======================================================
echo.
pause
