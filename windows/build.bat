@echo off
echo ===================================================
echo   Compilando el compilador Itsuki (C nativo)
echo ===================================================
gcc -Wall -Wextra -Os -s -fno-ident -fno-asynchronous-unwind-tables -Iheaders src\*.c -o itsuki.exe
if %ERRORLEVEL% equ 0 (
    echo [OK] Compilador Itsuki generado exitosamente: itsuki.exe
    echo.
    if not "%~1"=="" (
        itsuki.exe run %*
    ) else (
        itsuki.exe --version
    )
) else (
    echo [ERROR] Fallo al compilar el compilador Itsuki.
)
