@echo off
setlocal EnableDelayedExpansion
title Instalador Oficial de Itsuki
color 0B

echo
echo        ITSUKI PROGRAMMING LANGUAGE - INSTALADOR
echo

set "SCRIPT_DIR=%~dp0"
set "BIN_SRC="
set "LIB_SRC="

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
    color 0C
    echo [ERROR] No se encontro 'itsuki.exe'.
    echo Asegurate de compilar el proyecto o colocar el instalador junto a itsuki.exe.
    echo.
    pause
    exit /b 1
)

echo [1/5] Binario localizado: %BIN_SRC%
if not "%LIB_SRC%"=="" (
    echo [1/5] Biblioteca estandar localizada: %LIB_SRC%
)

set "INSTALL_DIR=%USERPROFILE%\.itsuki"
set "BIN_DIR=%INSTALL_DIR%\bin"
set "LIB_DIR=%INSTALL_DIR%\lib"
set "PKG_DIR=%INSTALL_DIR%\packages"

echo [2/5] Creando directorios en %INSTALL_DIR%...
if not exist "%INSTALL_DIR%" mkdir "%INSTALL_DIR%"
if not exist "%BIN_DIR%" mkdir "%BIN_DIR%"
if not exist "%LIB_DIR%" mkdir "%LIB_DIR%"
if not exist "%PKG_DIR%" mkdir "%PKG_DIR%"

echo [3/5] Copiando archivos del compilador y runtime...
copy /Y "%BIN_SRC%" "%BIN_DIR%\itsuki.exe" >nul
if errorlevel 1 (
    color 0C
    echo [ERROR] No se pudo copiar itsuki.exe en %BIN_DIR%
    pause
    exit /b 1
)

if not "%LIB_SRC%"=="" (
    xcopy /E /I /Y "%LIB_SRC%\*" "%LIB_DIR%\" >nul
)

echo [4/5] Configurando variable de entorno PATH...
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
    "$target = [System.IO.Path]::GetFullPath('%BIN_DIR%');" ^
    "$current = [Environment]::GetEnvironmentVariable('Path', 'User');" ^
    "if ($current -notlike ('*' + $target + '*')) {" ^
    "    $newPath = if ([string]::IsNullOrEmpty($current)) { $target } else { $current.TrimEnd(';') + ';' + $target };" ^
    "    [Environment]::SetEnvironmentVariable('Path', $newPath, 'User');" ^
    "    Write-Host '      [OK] Itsuki agregado al PATH de usuario.' -ForegroundColor Green;" ^
    "} else {" ^
    "    Write-Host '      [INFO] Itsuki ya estaba presente en el PATH.' -ForegroundColor Yellow;" ^
    "}"

(
    echo @echo off
    echo title Desinstalador de Itsuki
    echo color 0C
    echo
    echo       DESINSTALADOR DE ITSUKI PROGRAMMING LANGUAGE
    echo
    echo.
    echo Carpeta de instalacion: %INSTALL_DIR%
    echo.
    echo Desea desinstalar el compilador Itsuki de su sistema?
    echo Presione [S] para confirmar o cualquier otra tecla para cancelar.
    echo.
    echo set /p CONFIRM="Opcion: "
    echo if /i not "%%CONFIRM%%"=="S" exit /b 0
    echo.
    echo Removiendo de PATH de Windows...
    echo powershell -NoProfile -Command "$bin = [System.IO.Path]::GetFullPath('%BIN_DIR%'); $cur = [Environment]::GetEnvironmentVariable('Path', 'User'); if ($cur -like ('*' + $bin + '*')) { $p = $cur.Split(';') | Where-Object { $_ -ne $bin -and $_ -ne '' }; [Environment]::SetEnvironmentVariable('Path', ($p -join ';'), 'User'); Write-Host '[OK] Removido de PATH.' -ForegroundColor Green; }"
    echo.
    echo Eliminando archivos bin y lib...
    echo rmdir /s /q "%BIN_DIR%" 2^>nul
    echo rmdir /s /q "%LIB_DIR%" 2^>nul
    echo.
    echo [OK] Itsuki ha sido desinstalado.
    echo Los paquetes en %%USERPROFILE%%\.itsuki\packages se han conservado por seguridad.
    echo.
    echo pause
) > "%INSTALL_DIR%\uninstall.bat"

echo.
echo [5/5] Comprobando dependencias del sistema:
where gcc >nul 2>&1
if errorlevel 1 (
    echo   [ADVERTENCIA] 'gcc' no fue detectado en tu PATH.
    echo                 Itsuki utiliza GCC para enlazar ejecutables nativos x86.
    echo                 Te recomendamos instalar MinGW-w64 o w64devkit.
) else (
    echo   [OK] Compilador C / Enlazador 'gcc' detectado.
)

where git >nul 2>&1
if errorlevel 1 (
    echo   [ADVERTENCIA] 'git' no fue detectado en tu PATH.
    echo                 Lo necesitaras para instalar librerias con 'itsuki install'.
    echo                 Puedes descargarlo en: https://git-scm.com/
) else (
    echo   [OK] Gestor de versiones 'git' detectado.
)

echo.
echo
echo       INSTALACION COMPLETADA EXITOSAMENTE
echo
echo  Binario  : %BIN_DIR%\itsuki.exe
echo  Runtime  : %LIB_DIR%
echo  Paquetes : %PKG_DIR%
echo.
echo  Prueba de ejecucion:
"%BIN_DIR%\itsuki.exe" --version
echo.
echo  IMPORTANTE:
echo  Abre una NUEVA ventana de terminal (CMD o PowerShell)
echo  para comenzar a usar el comando 'itsuki' directamente:
echo.
echo    itsuki run programa.suki
echo    itsuki install apolo1061/libreria
echo    itsuki list
echo
echo.
pause
