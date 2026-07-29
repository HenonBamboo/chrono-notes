[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$InputDirectory,
    [Parameter(Mandatory)][string]$OutputDirectory,
    [string]$Version = '1.0.0'
)

$ErrorActionPreference = 'Stop'
$input = (Resolve-Path -LiteralPath $InputDirectory).Path
$output = (New-Item -ItemType Directory -Force -Path $OutputDirectory).FullName
$files = Get-ChildItem -LiteralPath $input -File -Recurse | Sort-Object FullName
$serial = "urn:uuid:$([Guid]::NewGuid())"
$timestamp = (Get-Date).ToUniversalTime().ToString('yyyy-MM-ddTHH:mm:ssZ')

function Get-StableSpdxId([string]$Value) {
    $hasher = [Security.Cryptography.SHA256]::Create()
    try {
        $bytes = [Text.Encoding]::UTF8.GetBytes($Value)
        $digest = $hasher.ComputeHash($bytes)
        $hex = -join ($digest | ForEach-Object { $_.ToString('x2') })
        return "SPDXRef-File-$($hex.Substring(0, 24))"
    } finally {
        $hasher.Dispose()
    }
}

$components = foreach ($file in $files) {
    $relative = $file.FullName.Substring($input.Length).TrimStart('\').Replace('\', '/')
    $hash = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    [ordered]@{
        type = 'file'
        name = $relative
        version = $Version
        hashes = @([ordered]@{ alg = 'SHA-256'; content = $hash })
    }
}
$cyclone = [ordered]@{
    bomFormat = 'CycloneDX'; specVersion = '1.5'; serialNumber = $serial; version = 1
    metadata = [ordered]@{
        timestamp = $timestamp
        component = [ordered]@{ type = 'application'; name = 'ChronoNotes'; version = $Version }
        tools = @([ordered]@{ vendor = 'ChronoNotes'; name = 'generate_sbom.ps1'; version = $Version })
    }
    components = @($components)
}
$cyclone | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath (Join-Path $output 'ChronoNotes.cdx.json') -Encoding UTF8

$spdxFiles = foreach ($file in $files) {
    $relative = $file.FullName.Substring($input.Length).TrimStart('\').Replace('\', '/')
    $hash = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    [ordered]@{
        fileName = "./$relative"
        SPDXID = Get-StableSpdxId $relative
        checksums = @([ordered]@{ algorithm = 'SHA256'; checksumValue = $hash })
        licenseConcluded = 'NOASSERTION'; copyrightText = 'NOASSERTION'
    }
}
$spdx = [ordered]@{
    spdxVersion = 'SPDX-2.3'; dataLicense = 'CC0-1.0'; SPDXID = 'SPDXRef-DOCUMENT'
    name = "ChronoNotes-$Version"; documentNamespace = "https://chrononotes.local/spdx/$([Guid]::NewGuid())"
    creationInfo = [ordered]@{ created = $timestamp; creators = @('Tool: ChronoNotes-generate_sbom.ps1') }
    packages = @([ordered]@{
        name = 'ChronoNotes'; SPDXID = 'SPDXRef-Package-ChronoNotes'; versionInfo = $Version
        downloadLocation = 'NOASSERTION'; filesAnalyzed = $true; licenseConcluded = 'NOASSERTION'; copyrightText = 'NOASSERTION'
    })
    files = @($spdxFiles)
    relationships = @([ordered]@{ spdxElementId = 'SPDXRef-DOCUMENT'; relationshipType = 'DESCRIBES'; relatedSpdxElement = 'SPDXRef-Package-ChronoNotes' })
}
$spdx | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath (Join-Path $output 'ChronoNotes.spdx.json') -Encoding UTF8
