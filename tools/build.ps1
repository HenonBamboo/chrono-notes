[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',
    [string]$BuildDir,
    [switch]$Clean,
    [switch]$RunTests
)

$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if ([string]::IsNullOrWhiteSpace($BuildDir)) {
    $BuildDir = Join-Path $repo (if ($Configuration -eq 'Release') { 'cmake-build-release' } else { 'cmake-build-qt-debug' })
}
$BuildDir = [IO.Path]::GetFullPath($BuildDir)
$repoPrefix = $repo.TrimEnd('\') + '\'
if ($BuildDir.Equals($repo, [StringComparison]::OrdinalIgnoreCase) -or
    -not $BuildDir.StartsWith($repoPrefix, [StringComparison]::OrdinalIgnoreCase)) {
    throw "Build directory must remain inside the repository: $BuildDir"
}

function Resolve-CMakeExecutable {
    $command = Get-Command cmake.exe -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }
    if ($env:CMAKE_EXE -and (Test-Path -LiteralPath $env:CMAKE_EXE)) { return $env:CMAKE_EXE }
    $candidate = Get-ChildItem 'C:\Program Files\JetBrains' -Filter cmake.exe -File -Recurse -ErrorAction SilentlyContinue |
        Where-Object FullName -Match '\\bin\\cmake\\win\\x64\\bin\\cmake\.exe$' |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1
    if (-not $candidate) { throw 'CMake was not found. Install CMake or set CMAKE_EXE.' }
    return $candidate.FullName
}

function Resolve-MinGWBin {
    $cache = Join-Path $BuildDir 'CMakeCache.txt'
    if (Test-Path -LiteralPath $cache) {
        $line = Select-String -LiteralPath $cache -Pattern '^CMAKE_CXX_COMPILER:FILEPATH=(.+)$' | Select-Object -First 1
        if ($line) { return Split-Path $line.Matches[0].Groups[1].Value -Parent }
    }
    if ($env:MINGW_BIN -and (Test-Path -LiteralPath (Join-Path $env:MINGW_BIN 'g++.exe'))) {
        return $env:MINGW_BIN
    }
    $candidate = Get-ChildItem 'C:\Program Files\JetBrains' -Filter g++.exe -File -Recurse -ErrorAction SilentlyContinue |
        Where-Object FullName -Match '\\bin\\mingw\\bin\\g\+\+\.exe$' |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1
    if (-not $candidate) { throw 'MinGW g++ was not found. Set MINGW_BIN.' }
    return $candidate.DirectoryName
}

$cmake = Resolve-CMakeExecutable
$mingwBin = Resolve-MinGWBin
$qtBin = Join-Path $repo 'third_party\Qt\6.8.3\mingw_64\bin'
if (-not (Test-Path -LiteralPath (Join-Path $qtBin 'qmake.exe'))) {
    throw "Qt 6.8.3 was not found at $qtBin"
}

if ($Clean -and (Test-Path -LiteralPath $BuildDir)) {
    Remove-Item -LiteralPath $BuildDir -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
$compilerTemp = Join-Path $BuildDir '.compiler-tmp'
New-Item -ItemType Directory -Force -Path $compilerTemp | Out-Null
$env:TEMP = $compilerTemp
$env:TMP = $compilerTemp
$env:PATH = "$mingwBin;$qtBin;$env:PATH"

$cc1plus = (& (Join-Path $mingwBin 'g++.exe') -print-prog-name=cc1plus).Trim()
if (-not [IO.Path]::IsPathRooted($cc1plus)) {
    $cc1plus = Join-Path $mingwBin $cc1plus
}
& $cc1plus --version 2>$null
if ($LASTEXITCODE -ne 0) {
    throw 'MinGW cc1plus could not start. Verify that the compiler bin directory is on PATH.'
}

& $cmake -S $repo -B $BuildDir -G 'MinGW Makefiles' "-DCMAKE_BUILD_TYPE=$Configuration" "-DCMAKE_CXX_COMPILER=$(Join-Path $mingwBin 'g++.exe')" "-DCHRONONOTES_MINGW_RUNTIME_BIN=$mingwBin"
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed with exit code $LASTEXITCODE" }
& $cmake --build $BuildDir -j 6
if ($LASTEXITCODE -ne 0) { throw "Build failed with exit code $LASTEXITCODE" }

if ($RunTests) {
    $ctest = Join-Path (Split-Path $cmake -Parent) 'ctest.exe'
    & $ctest --test-dir $BuildDir --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw "Tests failed with exit code $LASTEXITCODE" }
}

[PSCustomObject]@{
    Configuration = $Configuration
    BuildDirectory = $BuildDir
    CMake = $cmake
    CompilerBin = $mingwBin
}
