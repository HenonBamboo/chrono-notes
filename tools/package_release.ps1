[CmdletBinding()]
param(
    [string]$BuildDir,
    [string]$OutputDir,
    [switch]$SkipBuild,
    [switch]$RequireSignature
)

$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if ([string]::IsNullOrWhiteSpace($BuildDir)) {
    $BuildDir = Join-Path $repo 'cmake-build-release'
}
if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $OutputDir = Join-Path $repo 'dist'
}
if (-not $SkipBuild) {
    & (Join-Path $PSScriptRoot 'build.ps1') -Configuration Release -BuildDir $BuildDir -RunTests
}
$build = (Resolve-Path -LiteralPath $BuildDir).Path
$output = (New-Item -ItemType Directory -Force -Path $OutputDir).FullName
$cpackConfig = Join-Path $build 'CPackConfig.cmake'
if (-not (Test-Path -LiteralPath $cpackConfig)) { throw "CPack configuration not found: $cpackConfig" }

$cmakeLine = Select-String -LiteralPath (Join-Path $build 'CMakeCache.txt') -Pattern '^CMAKE_COMMAND:INTERNAL=(.+)$' | Select-Object -First 1
if (-not $cmakeLine) { throw 'CMAKE_COMMAND is missing from CMakeCache.txt.' }
$cpack = Join-Path (Split-Path $cmakeLine.Matches[0].Groups[1].Value -Parent) 'cpack.exe'
$nsisBin = & (Join-Path $PSScriptRoot 'bootstrap_nsis.ps1')
$env:PATH = "$nsisBin;$env:PATH"

$application = Join-Path $build 'ChronoNotes.exe'
if (-not (Test-Path -LiteralPath $application)) {
    throw "Application executable not found: $application"
}
$applicationStatus = @(& (Join-Path $PSScriptRoot 'sign_release.ps1') `
    -Files @($application) -RequireSignature:$RequireSignature)[-1]

Get-ChildItem -LiteralPath $output -File -ErrorAction SilentlyContinue |
    Where-Object Extension -In '.zip', '.exe', '.json', '.txt' |
    Remove-Item -Force
& $cpack --config $cpackConfig -G ZIP -B $output
if ($LASTEXITCODE -ne 0) { throw 'CPack ZIP generation failed.' }
& $cpack --config $cpackConfig -G NSIS -B $output
if ($LASTEXITCODE -ne 0) { throw 'CPack NSIS generation failed.' }
$nsisStage = Join-Path $build '_CPack_Packages\win64\NSIS'
if (Test-Path -LiteralPath $nsisStage) {
    $unexpectedPortableMarker = Get-ChildItem -LiteralPath $nsisStage `
        -Filter portable.flag -File -Recurse -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if ($unexpectedPortableMarker) {
        throw "Installer staging contains portable.flag: $($unexpectedPortableMarker.FullName)"
    }
}

$packages = Get-ChildItem -LiteralPath $output -File | Where-Object Extension -In '.zip', '.exe'
if (-not ($packages | Where-Object Extension -eq '.zip')) { throw 'Portable ZIP was not generated.' }
if (-not ($packages | Where-Object Extension -eq '.exe')) { throw 'NSIS installer was not generated.' }

Add-Type -AssemblyName System.IO.Compression.FileSystem
foreach ($zip in $packages | Where-Object Extension -eq '.zip') {
    $archive = [IO.Compression.ZipFile]::OpenRead($zip.FullName)
    try {
        if (-not ($archive.Entries | Where-Object FullName -Match '(^|/)portable\.flag$')) {
            throw "Portable marker missing from $($zip.Name)"
        }
    } finally { $archive.Dispose() }
    if ($zip.Length -gt 35MB) { throw "Portable ZIP exceeds 35 MiB: $($zip.Length) bytes" }
}

$installerStatus = @(& (Join-Path $PSScriptRoot 'sign_release.ps1') `
    -Files @($packages | Where-Object Extension -eq '.exe' | ForEach-Object FullName) `
    -RequireSignature:$RequireSignature)[-1]
$releaseStatus = if ($applicationStatus -eq 'signed' -and $installerStatus -eq 'signed') {
    'signed'
} else {
    'candidate'
}
$releaseStatus | Set-Content -LiteralPath (Join-Path $output 'RELEASE_STATUS.txt') -Encoding ascii

$hashLines = foreach ($package in $packages | Sort-Object Name) {
    $hash = (Get-FileHash -LiteralPath $package.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    "$hash  $($package.Name)"
}
$hashLines | Set-Content -LiteralPath (Join-Path $output 'SHA256SUMS.txt') -Encoding ascii

$portablePackage = $packages | Where-Object Extension -eq '.zip' | Select-Object -First 1
$sbomInput = Join-Path $build '.sbom-stage'
if (Test-Path -LiteralPath $sbomInput) {
    $resolvedSbom = [IO.Path]::GetFullPath($sbomInput)
    $buildPrefix = $build.TrimEnd('\') + '\'
    if (-not $resolvedSbom.StartsWith($buildPrefix, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Unsafe SBOM staging directory: $resolvedSbom"
    }
    Remove-Item -LiteralPath $resolvedSbom -Recurse -Force
}
[IO.Compression.ZipFile]::ExtractToDirectory($portablePackage.FullName, $sbomInput)
$installedBytes = (Get-ChildItem -LiteralPath $sbomInput -File -Recurse |
    Measure-Object -Property Length -Sum).Sum
if ($installedBytes -gt 80MB) {
    throw "Portable install footprint exceeds 80 MiB: $installedBytes bytes"
}
& (Join-Path $PSScriptRoot 'generate_sbom.ps1') `
    -InputDirectory $sbomInput -OutputDirectory $output -Version '1.0.0'

[PSCustomObject]@{
    Status = $releaseStatus
    Packages = @($packages | Select-Object Name, Length, FullName)
}
