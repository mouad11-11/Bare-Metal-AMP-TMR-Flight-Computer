$ErrorActionPreference = "Stop"
$toolsDir = "D:\tools"

if (-not (Test-Path $toolsDir)) {
    New-Item -ItemType Directory -Path $toolsDir -Force | Out-Null
}

# 1. ARM GNU Toolchain
$armZipUrl = "https://armkeil.blob.core.windows.net/developer/Files/downloads/gnu/14.2.rel1/binrel/arm-gnu-toolchain-14.2.rel1-mingw-w64-i686-arm-none-eabi.zip"
$armZipPath = Join-Path $toolsDir "arm-toolchain.zip"
$armExtractDir = Join-Path $toolsDir "arm-toolchain"

if (-not (Test-Path "$armExtractDir\bin\arm-none-eabi-gcc.exe")) {
    Write-Host "Downloading ARM GNU Toolchain..."
    curl.exe -k -L -o $armZipPath $armZipUrl
    Write-Host "Extracting ARM GNU Toolchain..."
    
    python -c "
import zipfile, os
zip_path = r'$armZipPath'
extract_to = r'$toolsDir'
with zipfile.ZipFile(zip_path, 'r') as zip_ref:
    zip_ref.extractall(extract_to)
"
    $extractedFolder = Get-ChildItem -Path $toolsDir -Directory -Filter "arm-gnu-toolchain-*" | Select-Object -First 1
    if ($extractedFolder) {
        if (Test-Path $armExtractDir) { Remove-Item -Recurse -Force $armExtractDir }
        Rename-Item -Path $extractedFolder.FullName -NewName "arm-toolchain"
    }
    if (Test-Path $armZipPath) { Remove-Item -Force $armZipPath }
    Write-Host "ARM GNU Toolchain installed successfully."
} else {
    Write-Host "ARM GNU Toolchain already installed."
}

# 2. QEMU for Windows
$qemuExeUrl = "https://qemu.weilnetz.de/w64/2026/qemu-w64-setup-20260811.exe"
$qemuInstallerPath = Join-Path $toolsDir "qemu-setup.exe"
$qemuDir = Join-Path $toolsDir "qemu"

if (-not (Test-Path "$qemuDir\qemu-system-arm.exe")) {
    Write-Host "Downloading QEMU for Windows..."
    curl.exe -k -L -o $qemuInstallerPath $qemuExeUrl
    Write-Host "Installing QEMU silently to $qemuDir..."
    Start-Process -FilePath $qemuInstallerPath -ArgumentList "/S", "/D=$qemuDir" -Wait
    if (Test-Path $qemuInstallerPath) { Remove-Item -Force $qemuInstallerPath }
    Write-Host "QEMU installed successfully."
} else {
    Write-Host "QEMU already installed."
}

Write-Host "Tool setup complete."
