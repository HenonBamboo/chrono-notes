[CmdletBinding()]
param(
    [string]$Destination
)

$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($Destination)) {
    $Destination = Join-Path $PSScriptRoot '..\.tools\nsis\3.12'
}
$version = '3.12'
$expectedSha256 = '56581F90DB321581C5381193D796FFFCF2D24B2F8FED2160A6C6A3BAA67F2C4F'
$root = Split-Path $Destination -Parent
$archive = Join-Path $root "nsis-$version.zip"
$makensis = Get-ChildItem $Destination -Filter makensis.exe -File -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
if ($makensis) {
    $makensis.DirectoryName
    exit 0
}

New-Item -ItemType Directory -Force -Path $root | Out-Null
if (Test-Path -LiteralPath $archive) {
    $cachedHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $archive).Hash
    if ($cachedHash -ne $expectedSha256) {
        Remove-Item -LiteralPath $archive -Force
    }
}
if (-not (Test-Path -LiteralPath $archive)) {
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    $partial = "$archive.partial"
    $sources = @(
        "https://sourceforge.net/projects/nsis/files/NSIS%203/$version/nsis-$version.zip/download",
        "https://mirrors.mit.edu/macports/distfiles/nsis/nsis-$version.zip"
    )
    foreach ($url in $sources) {
        try {
            Invoke-WebRequest -UseBasicParsing -Uri $url -OutFile $partial
            $downloadHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $partial).Hash
            if ($downloadHash -eq $expectedSha256) {
                Move-Item -LiteralPath $partial -Destination $archive -Force
                break
            }
        } catch {
            Write-Verbose "NSIS download failed from ${url}: $($_.Exception.Message)"
        } finally {
            if (Test-Path -LiteralPath $partial) {
                Remove-Item -LiteralPath $partial -Force
            }
        }
    }
    if (-not (Test-Path -LiteralPath $archive)) {
        throw "Unable to download the verified NSIS $version archive."
    }
}
$actual = (Get-FileHash -Algorithm SHA256 -LiteralPath $archive).Hash
if ($actual -ne $expectedSha256) {
    throw "NSIS archive checksum mismatch. Expected $expectedSha256, got $actual."
}
if (Test-Path -LiteralPath $Destination) {
    Remove-Item -LiteralPath $Destination -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $Destination | Out-Null
Expand-Archive -LiteralPath $archive -DestinationPath $Destination -Force
$makensis = Get-ChildItem $Destination -Filter makensis.exe -File -Recurse | Select-Object -First 1
if (-not $makensis) { throw 'makensis.exe was not found in the verified NSIS archive.' }
$makensis.DirectoryName
