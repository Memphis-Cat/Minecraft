param(
    [Parameter(Mandatory=$true)][string]$Manifest,
    [Parameter(Mandatory=$true)][string]$PrivateKey
)
$ErrorActionPreference = "Stop"
$json = Get-Content -Raw -LiteralPath $Manifest | ConvertFrom-Json
$canonical = "schema=$($json.schema)`nversion=$($json.version)`nsource_commit=$($json.source_commit)`nchannel_key_hash=$($json.channel_key_hash)`nminimum_launcher_version=$($json.minimum_launcher_version)`n"
$rsa = [System.Security.Cryptography.RSA]::Create()
$rsa.ImportFromPem((Get-Content -Raw -LiteralPath $PrivateKey))
$bytes = [Text.Encoding]::UTF8.GetBytes($canonical)
$signature = $rsa.SignData($bytes, [Security.Cryptography.HashAlgorithmName]::SHA256, [Security.Cryptography.RSASignaturePadding]::Pss)
$json.signature = [Convert]::ToBase64String($signature)
$json | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $Manifest -Encoding UTF8
Write-Host "Signed $Manifest"
