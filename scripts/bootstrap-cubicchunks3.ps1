[CmdletBinding()]
param(
    [string]$Destination = "",
    [switch]$SkipTests
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Split-Path -Parent $scriptDir
$patchDir = Join-Path $repoRoot "patches"

if ([string]::IsNullOrWhiteSpace($Destination)) {
    $Destination = Join-Path $repoRoot "work\CubicChunks3"
}
$Destination = [System.IO.Path]::GetFullPath($Destination)

function Require-Command {
    param([Parameter(Mandatory = $true)][string]$Name)
    if (-not (Get-Command $Name -ErrorAction SilentlyContinue)) {
        throw "Required command '$Name' was not found in PATH."
    }
}

Require-Command -Name "git"
Require-Command -Name "java"

$javaVersionText = (& java -version 2>&1 | Out-String)
if ($LASTEXITCODE -ne 0) {
    throw "Unable to execute Java."
}
if ($javaVersionText -notmatch 'version\s+"21(?:\.|\")') {
    throw "CubicChunks3 currently requires Java 21. Detected:`n$javaVersionText"
}

if (Test-Path $Destination) {
    throw "Destination already exists: $Destination`nRemove it or pass -Destination with a new empty path."
}

$destinationParent = Split-Path -Parent $Destination
New-Item -ItemType Directory -Force -Path $destinationParent | Out-Null

Write-Host "Cloning CubicChunks3 with submodules..."
& git clone --recursive --branch dev https://github.com/OpenCubicChunks/CubicChunks3.git $Destination
if ($LASTEXITCODE -ne 0) {
    throw "git clone failed."
}

Write-Host "Pinning audited upstream commit..."
& git -C $Destination checkout e23e0e42b550ed0ba653b5460350193cd84403c0
if ($LASTEXITCODE -ne 0) {
    throw "Unable to check out the audited upstream commit."
}
& git -C $Destination submodule update --init --recursive
if ($LASTEXITCODE -ne 0) {
    throw "Unable to initialize CubicChunksCore."
}

$patches = Get-ChildItem -Path $patchDir -Filter "*.patch" | Sort-Object Name
if ($patches.Count -eq 0) {
    throw "No patch files were found in $patchDir"
}

foreach ($patch in $patches) {
    Write-Host "Checking $($patch.Name)..."
    & git -C $Destination apply --check --whitespace=error-all $patch.FullName
    if ($LASTEXITCODE -ne 0) {
        throw "Patch check failed: $($patch.FullName)"
    }

    Write-Host "Applying $($patch.Name)..."
    & git -C $Destination apply --whitespace=fix $patch.FullName
    if ($LASTEXITCODE -ne 0) {
        throw "Patch apply failed: $($patch.FullName)"
    }
}

Push-Location $Destination
try {
    Write-Host "Validating patch whitespace..."
    & git diff --check
    if ($LASTEXITCODE -ne 0) {
        throw "git diff --check failed."
    }

    Write-Host "Gradle environment:"
    & .\gradlew.bat --version
    if ($LASTEXITCODE -ne 0) {
        throw "Gradle wrapper failed to start."
    }

    if (-not $SkipTests) {
        Write-Host "Running unit tests..."
        & .\gradlew.bat test
        if ($LASTEXITCODE -ne 0) {
            throw "Gradle tests failed."
        }

        Write-Host "Running complete checks..."
        & .\gradlew.bat check
        if ($LASTEXITCODE -ne 0) {
            throw "Gradle checks failed."
        }
    }
}
finally {
    Pop-Location
}

Write-Host ""
Write-Host "Patched CubicChunks3 checkout created at:"
Write-Host $Destination
Write-Host ""
Write-Host "Next useful commands:"
Write-Host "  cd `"$Destination`""
Write-Host "  .\gradlew.bat runClient"
Write-Host "  .\gradlew.bat runServer"
