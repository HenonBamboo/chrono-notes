[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string[]]$Files,
    [switch]$RequireSignature
)

$ErrorActionPreference = 'Stop'
$pfx = $env:CHRONONOTES_SIGN_PFX
$password = $env:CHRONONOTES_SIGN_PASSWORD
if ([string]::IsNullOrWhiteSpace($pfx)) {
    if ($RequireSignature) { throw 'CHRONONOTES_SIGN_PFX is required for a signed release.' }
    Write-Warning 'No signing certificate configured. Artifacts remain unsigned candidate builds.'
    return 'candidate'
}
if (-not (Test-Path -LiteralPath $pfx)) { throw "Signing certificate not found: $pfx" }

$signtool = Get-Command signtool.exe -ErrorAction SilentlyContinue
if (-not $signtool) {
    $signtool = Get-ChildItem 'C:\Program Files (x86)\Windows Kits\10\bin' -Filter signtool.exe -File -Recurse -ErrorAction SilentlyContinue |
        Where-Object FullName -Match '\\x64\\signtool\.exe$' |
        Sort-Object FullName -Descending |
        Select-Object -First 1
}
if (-not $signtool) { throw 'signtool.exe was not found.' }
$signtoolPath = if ($signtool.Source) { $signtool.Source } else { $signtool.FullName }

foreach ($file in $Files) {
    $resolved = (Resolve-Path -LiteralPath $file).Path
    $arguments = @('sign', '/fd', 'SHA256', '/td', 'SHA256', '/tr', 'https://timestamp.digicert.com', '/f', $pfx)
    if (-not [string]::IsNullOrEmpty($password)) { $arguments += @('/p', $password) }
    $arguments += $resolved
    & $signtoolPath @arguments
    if ($LASTEXITCODE -ne 0) { throw "Signing failed: $resolved" }
    & $signtoolPath verify /pa /all $resolved
    if ($LASTEXITCODE -ne 0) { throw "Signature verification failed: $resolved" }
}
return 'signed'
