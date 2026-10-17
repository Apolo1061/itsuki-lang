# ===========================================================
#        ITSUKI COMPILER - INSTALADOR (POWERSHELL)
# ===========================================================
[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"

Write-Host ""
Write-Host " =======================================================" -ForegroundColor Cyan
Write-Host "         ITSUKI COMPILER - INSTALADOR (POWERSHELL)      " -ForegroundColor Cyan
Write-Host " =======================================================" -ForegroundColor Cyan
Write-Host ""

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path

# 20%
Write-Host " [====----------------]  20%  Detectando arquitectura de la PC..." -ForegroundColor Yellow
$is64 = [Environment]::Is64BitOperatingSystem
$archBits = if ($is64) { 64 } else { 32 }
$archName = if ($env:PROCESSOR_ARCHITEW6432 -eq "AMD64" -or $env:PROCESSOR_ARCHITECTURE -eq "AMD64") {
    "x86_64"
} elseif ($env:PROCESSOR_ARCHITECTURE -eq "ARM64") {
    "arm64"
} else {
    "x86"
}

$gcc = Get-Command "gcc" -ErrorAction SilentlyContinue
$gccMachine = "desconocido"
$gccBits = $archBits
$gccFlags = ""
$multilibOk = 0

if ($gcc) {
    try {
        $gccMachine = (& gcc -dumpmachine 2>$null).Trim()
    } catch {
        $gccMachine = "desconocido"
    }

    if ($gccMachine -match "64") {
        $gccBits = 64
        $gccFlags = "-m32"
    } else {
        $gccBits = 32
        $gccFlags = ""
    }

    $probeSrc = Join-Path $env:TEMP "itsuki_probe_$([guid]::NewGuid().ToString('N')).c"
    $probeExe = Join-Path $env:TEMP "itsuki_probe_$([guid]::NewGuid().ToString('N')).exe"
    Set-Content -Path $probeSrc -Value "int main(void){return 0;}" -Encoding ascii

    if ($gccBits -eq 64) {
        & gcc -m32 $probeSrc -o $probeExe 2>$null
        $multilibOk = if ($LASTEXITCODE -eq 0) { 1 } else { 0 }
    } else {
        $multilibOk = 1
    }
    Remove-Item $probeSrc, $probeExe -ErrorAction SilentlyContinue
}

# 40%
Write-Host " [========------------]  40%  Localizando archivos del compilador..." -ForegroundColor Yellow
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
    Write-Host "`n [ERROR] No se encontro itsuki.exe. Compila primero con build.bat." -ForegroundColor Red
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

# 60%
Write-Host " [============--------]  60%  Preparando directorios del sistema..." -ForegroundColor Yellow
$installDir = Join-Path $env:USERPROFILE ".itsuki"
$binDir = Join-Path $installDir "bin"
$libDir = Join-Path $installDir "lib"
$pkgDir = Join-Path $installDir "packages"

foreach ($dir in @($installDir, $binDir, $libDir, $pkgDir)) {
    if (-not (Test-Path $dir)) {
        New-Item -ItemType Directory -Path $dir -Force | Out-Null
    }
}

# 80%
Write-Host " [================----]  80%  Instalando binario, librerias y arch.conf..." -ForegroundColor Yellow
$destBin = Join-Path $binDir "itsuki.exe"
Copy-Item -Path $binSrc -Destination $destBin -Force
if ($libSrc) {
    Copy-Item -Path "$libSrc\*" -Destination $libDir -Recurse -Force
}

$archConfPath = Join-Path $installDir "arch.conf"
$archContent = @"
# ===========================================================
#      ITSUKI PROGRAMMING LANGUAGE - ARCH CONFIG
#   Generado automaticamente durante la instalacion
# ===========================================================
os_name=windows
os_bits=$archBits
host_arch=$archName
gcc_path=gcc
gcc_arch=$gccMachine
gcc_bits=$gccBits
multilib_supported=$multilibOk
gcc_flags=$gccFlags
"@
Set-Content -Path $archConfPath -Value $archContent -Encoding ascii

$uninstallScript = @"
`$confirm = Read-Host "Desea desinstalar Itsuki de su sistema? (S/N)"
if (`$confirm -ne 'S' -and `$confirm -ne 's') { exit 0 }
`$bin = "$binDir"
`$cur = [Environment]::GetEnvironmentVariable("Path", "User")
if (`$cur) {
    `$parts = `$cur.Split(";") | Where-Object { `$_ -ne `$bin -and `$_ -ne "" }
    [Environment]::SetEnvironmentVariable("Path", (`$parts -join ";"), "User")
}
Remove-Item -Recurse -Force "$binDir", "$libDir", "$archConfPath" -ErrorAction SilentlyContinue
Write-Host "[OK] Itsuki ha sido desinstalado exitosamente." -ForegroundColor Green
"@
Set-Content -Path (Join-Path $installDir "uninstall.ps1") -Value $uninstallScript -Encoding utf8

# 100%
Write-Host " [====================] 100%  Configurando variable de entorno PATH..." -ForegroundColor Green
$userPath = [Environment]::GetEnvironmentVariable("Path", "User")
if ([string]::IsNullOrEmpty($userPath)) {
    [Environment]::SetEnvironmentVariable("Path", $binDir, "User")
} elseif ($userPath.Split(";") -notcontains $binDir) {
    $newUserPath = $userPath.TrimEnd(";") + ";" + $binDir
    [Environment]::SetEnvironmentVariable("Path", $newUserPath, "User")
}

Write-Host ""
Write-Host " =======================================================" -ForegroundColor Green
Write-Host "  [OK] ITSUKI INSTALADO EXITOSAMENTE ($archName $archBits-bit)" -ForegroundColor Green
Write-Host " =======================================================" -ForegroundColor Green
Write-Host "  Ejecutable : $destBin" -ForegroundColor Gray
Write-Host "  Runtime    : $libDir" -ForegroundColor Gray
Write-Host "  Config     : $archConfPath" -ForegroundColor Gray
Write-Host "  Toolchain  : $gccMachine" -ForegroundColor Gray
Write-Host " -------------------------------------------------------" -ForegroundColor DarkGray
Write-Host "  Comandos para comenzar:" -ForegroundColor White
Write-Host "    itsuki run programa.suki   Compilar y ejecutar" -ForegroundColor Cyan
Write-Host "    itsuki doctor             Ver estado del entorno" -ForegroundColor Cyan
Write-Host "    itsuki --help             Ver todas las opciones" -ForegroundColor Cyan
Write-Host " =======================================================" -ForegroundColor Green
Write-Host ""
