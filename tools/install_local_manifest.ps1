param(
    [Parameter(Mandatory = $true)]
    [string]$SourceDirectory,

    [Parameter(Mandatory = $true)]
    [string]$GameExecutable,

    [Parameter(Mandatory = $true)]
    [string]$OutputManifest,

    [string]$ManifestUrl = "https://raw.githubusercontent.com/Memphis-Cat/Minecraft/main/manifest.json"
)

$ErrorActionPreference = "Stop"

$sourcePath = [System.IO.Path]::GetFullPath($SourceDirectory)
$gamePath = [System.IO.Path]::GetFullPath($GameExecutable)
$outputPath = [System.IO.Path]::GetFullPath($OutputManifest)

if (-not (Test-Path -LiteralPath $gamePath -PathType Leaf)) {
    throw "Minecraft executable was not found: $gamePath"
}

$git = Get-Command git.exe -ErrorAction Stop
$head = (& $git.Source -C $sourcePath rev-parse HEAD 2>$null).Trim()
if ($LASTEXITCODE -ne 0 -or $head -notmatch '^[0-9a-fA-F]{40}$') {
    throw "Could not determine the checked-out Git commit."
}

$response = Invoke-WebRequest -UseBasicParsing -Uri $ManifestUrl
$remote = $response.Content | ConvertFrom-Json

if ($remote.source_commit -ne $head.ToLowerInvariant()) {
    if (Test-Path -LiteralPath $outputPath) {
        Remove-Item -LiteralPath $outputPath -Force
    }
    Write-Warning "The checked-out source commit is not the signed release. The launcher will update the game on first start."
    exit 0
}

$gameHash = (Get-FileHash -LiteralPath $gamePath -Algorithm SHA256).Hash.ToLowerInvariant()
$local = [ordered]@{
    schema = [int]$remote.schema
    version = [string]$remote.version
    source_commit = [string]$remote.source_commit
    channel_key_hash = [string]$remote.channel_key_hash
    minimum_launcher_version = [string]$remote.minimum_launcher_version
    signature = [string]$remote.signature
    game_exe_sha256 = $gameHash
}

$parent = Split-Path -Parent $outputPath
if (-not (Test-Path -LiteralPath $parent)) {
    New-Item -ItemType Directory -Path $parent -Force | Out-Null
}

$json = $local | ConvertTo-Json -Depth 5
[System.IO.File]::WriteAllText($outputPath, $json + [Environment]::NewLine, [System.Text.UTF8Encoding]::new($false))
Write-Host "Installed local release manifest: $outputPath"
