param(
    [string]$BuildDir = (Join-Path $PSScriptRoot '..\build'),
    [string]$OutputDir = (Join-Path $PSScriptRoot '..\dist'),
    [string]$PackageName = 'ChronoNotes'
)

$ErrorActionPreference = 'Stop'

$build = Resolve-Path -LiteralPath $BuildDir
$exe = Join-Path $build 'ChronoNotes.exe'
if (-not (Test-Path -LiteralPath $exe)) {
    throw "ChronoNotes.exe not found in $build. Build the ChronoNotes target first."
}

$output = (New-Item -ItemType Directory -Force -Path $OutputDir).FullName
$stage = Join-Path $output $PackageName
if (Test-Path -LiteralPath $stage) {
    Remove-Item -LiteralPath $stage -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $stage | Out-Null

foreach ($pattern in @('ChronoNotes.exe', '*.dll', '*.qm')) {
    Get-ChildItem -LiteralPath $build -Filter $pattern -File | ForEach-Object {
        Copy-Item -LiteralPath $_.FullName -Destination $stage -Force
    }
}

foreach ($dir in @(
    'generic',
    'iconengines',
    'imageformats',
    'networkinformation',
    'platforms',
    'qml',
    'qmltooling',
    'sqldrivers',
    'ChronoNotes',
    'tls',
    'translations'
)) {
    $source = Join-Path $build $dir
    if (Test-Path -LiteralPath $source) {
        Copy-Item -LiteralPath $source -Destination $stage -Recurse -Force
    }
}

$readme = Join-Path $PSScriptRoot '..\README.md'
if (Test-Path -LiteralPath $readme) {
    Copy-Item -LiteralPath $readme -Destination $stage -Force
}

$assets = Join-Path $PSScriptRoot '..\assets'
if (Test-Path -LiteralPath $assets) {
    Copy-Item -LiteralPath $assets -Destination $stage -Recurse -Force
}

$zip = Join-Path $output "$PackageName.zip"
if (Test-Path -LiteralPath $zip) {
    Remove-Item -LiteralPath $zip -Force
}
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip -Force
Write-Output "Package created: $zip"
