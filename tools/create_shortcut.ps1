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

$desktop = [Environment]::GetFolderPath([Environment+SpecialFolder]::DesktopDirectory)
if ([string]::IsNullOrWhiteSpace($desktop)) {
    throw "Windows could not resolve the desktop folder."
}

$shortcutPath = Join-Path $desktop ($Name + ".lnk")
$shell = New-Object -ComObject WScript.Shell
$shortcut = $shell.CreateShortcut($shortcutPath)
$shortcut.TargetPath = $targetPath
$shortcut.WorkingDirectory = $workingPath
$shortcut.IconLocation = "$targetPath,0"
$shortcut.Description = "Launch and update MemphisCat Minecraft"
$shortcut.Save()

Write-Host "Created shortcut: $shortcutPath"
