param(
    [string]$BuildDir = (Join-Path $PSScriptRoot '..\cmake-build-qt-debug'),
    [string]$OutputDir = (Join-Path $PSScriptRoot '..\dist'),
    [string]$PackageName = 'StickyNotesC'
)

$ErrorActionPreference = 'Stop'

$build = Resolve-Path -LiteralPath $BuildDir
$exe = Join-Path $build 'StickyNotesC.exe'
if (-not (Test-Path -LiteralPath $exe)) {
    throw "StickyNotesC.exe not found in $build. Build the StickyNotesC target first."
}

$output = (New-Item -ItemType Directory -Force -Path $OutputDir).FullName
$stage = Join-Path $output $PackageName
if (Test-Path -LiteralPath $stage) {
    Remove-Item -LiteralPath $stage -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $stage | Out-Null

foreach ($pattern in @('StickyNotesC.exe', '*.dll', '*.qm')) {
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
    'StickyNotes',
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

$zip = Join-Path $output "$PackageName.zip"
if (Test-Path -LiteralPath $zip) {
    Remove-Item -LiteralPath $zip -Force
}
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip -Force
Write-Output "Package created: $zip"
