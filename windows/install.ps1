# ===========================================================
#        ITSUKI PROGRAMMING LANGUAGE - POWERSHELL INSTALLER
# ===========================================================
[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"

Write-Host "===========================================================" -ForegroundColor Cyan
Write-Host "       ITSUKI PROGRAMMING LANGUAGE - INSTALADOR            " -ForegroundColor Cyan
Write-Host "===========================================================" -ForegroundColor Cyan
Write-Host ""

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path

# 1. Localizar binario y runtime
$binSrc = $null
$possibleBins = @(
    (Join-Path $scriptDir "itsuki.exe"),
    (Join-Path $scriptDir "windows\itsuki.exe"),
    (Join-Path $scriptDir "build\itsuki.exe"),
    (Join-Path $scriptDir "build\windows\itsuki.exe")
)
foreach ($cand in $possibleBins) {
    if (Test-Path $cand) {
        $binSrc = $cand
        break
    }
}

if (-not $binSrc) {
    Write-Host "[ERROR] No se pudo encontrar itsuki.exe en las rutas de compilacion." -ForegroundColor Red
    exit 1
}

$libSrc = $null
$possibleLibs = @(
    (Join-Path $scriptDir "lib"),
    (Join-Path $scriptDir "windows\lib"),
    (Join-Path $scriptDir "build\windows\lib")
)
foreach ($cand in $possibleLibs) {
    if (Test-Path $cand) {
        $libSrc = $cand
        break
    }
}

Write-Host "[1/5] Localizando archivos:" -ForegroundColor Yellow
Write-Host "      Binario: $binSrc" -ForegroundColor Gray
if ($libSrc) {
    Write-Host "      Runtime: $libSrc" -ForegroundColor Gray
}

# 2. Preparar directorios de destino en ~/.itsuki
$installDir = Join-Path $env:USERPROFILE ".itsuki"
$binDir = Join-Path $installDir "bin"
$libDir = Join-Path $installDir "lib"
$pkgDir = Join-Path $installDir "packages"

Write-Host "[2/5] Creando carpetas en $installDir..." -ForegroundColor Yellow
foreach ($dir in @($installDir, $binDir, $libDir, $pkgDir)) {
    if (-not (Test-Path $dir)) {
        New-Item -ItemType Directory -Path $dir -Force | Out-Null
    }
}

# 3. Copiar binario y librerias
Write-Host "[3/5] Instalando archivos del compilador..." -ForegroundColor Yellow
$destBin = Join-Path $binDir "itsuki.exe"
Copy-Item -Path $binSrc -Destination $destBin -Force
if ($libSrc) {
    Copy-Item -Path "$libSrc\*" -Destination $libDir -Recurse -Force
}

# 4. Actualizar PATH de Usuario
Write-Host "[4/5] Configurando variable de entorno PATH..." -ForegroundColor Yellow
$userPath = [Environment]::GetEnvironmentVariable("Path", "User")
if ([string]::IsNullOrEmpty($userPath)) {
    [Environment]::SetEnvironmentVariable("Path", $binDir, "User")
    Write-Host "      [OK] PATH de usuario inicializado con Itsuki." -ForegroundColor Green
} elseif ($userPath.Split(";") -notcontains $binDir) {
    $newUserPath = $userPath.TrimEnd(";") + ";" + $binDir
    [Environment]::SetEnvironmentVariable("Path", $newUserPath, "User")
    Write-Host "      [OK] Itsuki agregado al PATH permanentemente." -ForegroundColor Green
} else {
    Write-Host "      [INFO] Itsuki ya estaba presente en el PATH." -ForegroundColor Gray
}

# 5. Generar script de desinstalacion
$uninstallScript = @"
Write-Host "===========================================================" -ForegroundColor Red
Write-Host "       DESINSTALADOR DE ITSUKI PROGRAMMING LANGUAGE       " -ForegroundColor Red
Write-Host "===========================================================" -ForegroundColor Red
`$confirm = Read-Host "Desea desinstalar Itsuki de su sistema? (S/N)"
if (`$confirm -ne 'S' -and `$confirm -ne 's') {
    Write-Host "Desinstalacion cancelada." -ForegroundColor Yellow
    exit 0
}

`$bin = "$binDir"
`$cur = [Environment]::GetEnvironmentVariable("Path", "User")
if (`$cur) {
    `$parts = `$cur.Split(";") | Where-Object { `$_ -ne `$bin -and `$_ -ne "" }
    [Environment]::SetEnvironmentVariable("Path", (`$parts -join ";"), "User")
    Write-Host "[OK] Removido de PATH." -ForegroundColor Green
}

Remove-Item -Recurse -Force "$binDir" -ErrorAction SilentlyContinue
Remove-Item -Recurse -Force "$libDir" -ErrorAction SilentlyContinue
Write-Host "[OK] Itsuki ha sido desinstalado exitosamente." -ForegroundColor Green
"@
Set-Content -Path (Join-Path $installDir "uninstall.ps1") -Value $uninstallScript -Encoding utf8

# Comprobaciones
Write-Host "[5/5] Comprobando dependencias:" -ForegroundColor Yellow
$gcc = Get-Command "gcc" -ErrorAction SilentlyContinue
if ($gcc) {
    Write-Host "      [OK] GCC detectado: $($gcc.Source)" -ForegroundColor Green
} else {
    Write-Host "      [ADVERTENCIA] GCC no detectado en PATH. Se recomienda instalar MinGW o w64devkit." -ForegroundColor Yellow
}

$git = Get-Command "git" -ErrorAction SilentlyContinue
if ($git) {
    Write-Host "      [OK] Git detectado: $($git.Source)" -ForegroundColor Green
} else {
    Write-Host "      [ADVERTENCIA] Git no detectado. Lo necesitaras para 'itsuki install'." -ForegroundColor Yellow
}

Write-Host ""
Write-Host "===========================================================" -ForegroundColor Green
Write-Host "       INSTALACION COMPLETADA EXITOSAMENTE!                " -ForegroundColor Green
Write-Host "===========================================================" -ForegroundColor Green
Write-Host "  Binario  : $destBin"
Write-Host "  Librerias: $libDir"
Write-Host "  Paquetes : $pkgDir"
Write-Host ""
& $destBin --version
Write-Host ""
Write-Host "Para empezar, abre una NUEVA consola y ejecuta:" -ForegroundColor Cyan
Write-Host "  itsuki --help" -ForegroundColor White
Write-Host "  itsuki list" -ForegroundColor White
Write-Host "  itsuki run programa.suki" -ForegroundColor White
Write-Host "===========================================================" -ForegroundColor Green
