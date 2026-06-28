# Build portable Windows x64 release (run on Windows with MinGW or MSVC in PATH)
param(
    [string]$Version = "0.1.0"
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$Build = Join-Path $Root "build-win-native"
$Name = "QuantumCircuitLab-$Version-win64"
$Dist = Join-Path $Root "dist\$Name"
$Archive = Join-Path $Root "dist\$Name.zip"

Write-Host "==> Configuring Release build..."
cmake -B $Build -DCMAKE_BUILD_TYPE=Release -DQSIM_BUILD_TESTS=OFF
cmake --build $Build --target quantum-lab -j

$Bin = Join-Path $Build "ui\quantum-lab.exe"
if (-not (Test-Path $Bin)) {
    throw "Build output not found: $Bin"
}

Write-Host "==> Assembling portable bundle..."
if (Test-Path $Dist) { Remove-Item -Recurse -Force $Dist }
New-Item -ItemType Directory -Path (Join-Path $Dist "presets") | Out-Null
Copy-Item $Bin (Join-Path $Dist "quantum-lab.exe")
Copy-Item -Recurse (Join-Path $Root "presets\*") (Join-Path $Dist "presets")
Copy-Item (Join-Path $Root "packaging\run-quantum-lab.bat") (Join-Path $Dist "run.bat")
Copy-Item (Join-Path $Root "packaging\README-portable-windows.txt") (Join-Path $Dist "README.txt")

New-Item -ItemType Directory -Force -Path (Join-Path $Root "dist") | Out-Null
if (Test-Path $Archive) { Remove-Item -Force $Archive }
Compress-Archive -Path "$Dist\*" -DestinationPath $Archive -Force

Write-Host ""
Write-Host "Done."
Write-Host "  Folder:  $Dist"
Write-Host "  Archive: $Archive"
