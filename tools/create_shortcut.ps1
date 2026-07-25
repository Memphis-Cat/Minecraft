param(
    [Parameter(Mandatory = $true)]
    [string]$Target,

    [Parameter(Mandatory = $true)]
    [string]$WorkingDirectory,

    [string]$Name = "Minecraft Launcher"
)

$ErrorActionPreference = "Stop"

$targetPath = [System.IO.Path]::GetFullPath($Target)
$workingPath = [System.IO.Path]::GetFullPath($WorkingDirectory)

if (-not (Test-Path -LiteralPath $targetPath -PathType Leaf)) {
    throw "Launcher executable was not found: $targetPath"
}
if (-not (Test-Path -LiteralPath $workingPath -PathType Container)) {
    throw "Launcher working directory was not found: $workingPath"
}

$desktop = [Environment]::GetFolderPath([Environment+SpecialFolder]::DesktopDirectory)
if ([string]::IsNullOrWhiteSpace($desktop)) {
    $rawDesktop = (Get-ItemProperty -LiteralPath "HKCU:\Software\Microsoft\Windows\CurrentVersion\Explorer\User Shell Folders" -Name Desktop -ErrorAction SilentlyContinue).Desktop
    if (-not [string]::IsNullOrWhiteSpace($rawDesktop)) {
        $desktop = [Environment]::ExpandEnvironmentVariables([string]$rawDesktop)
    }
}
if ([string]::IsNullOrWhiteSpace($desktop)) {
    throw "Windows could not resolve the current user's desktop folder."
}
if (-not (Test-Path -LiteralPath $desktop -PathType Container)) {
    New-Item -ItemType Directory -Path $desktop -Force | Out-Null
}

$shortcutPath = Join-Path $desktop ($Name + ".lnk")
if (Test-Path -LiteralPath $shortcutPath) {
    Remove-Item -LiteralPath $shortcutPath -Force
}

$shell = New-Object -ComObject WScript.Shell
$shortcut = $shell.CreateShortcut($shortcutPath)
$shortcut.TargetPath = $targetPath
$shortcut.WorkingDirectory = $workingPath
$shortcut.IconLocation = "$targetPath,0"
$shortcut.Description = "Launch and update MemphisCat Minecraft"
$shortcut.Save()

if (-not (Test-Path -LiteralPath $shortcutPath -PathType Leaf)) {
    throw "Windows did not create the shortcut: $shortcutPath"
}

Write-Host "Created shortcut: $shortcutPath"
Write-Host "Shortcut target: $targetPath"
Write-Host "Shortcut working directory: $workingPath"
