[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$Executable,
    [int]$Runs = 5,
    [int]$StartupLimitMs = 1500,
    [int]$RssLimitMiB = 120,
    [int]$IdleWaitMs = 2000,
    [string]$ReportPath
)

$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$exe = (Resolve-Path -LiteralPath $Executable).Path
if ($Runs -lt 1) { throw 'Runs must be at least 1.' }
if ([string]::IsNullOrWhiteSpace($ReportPath)) {
    $ReportPath = Join-Path (Split-Path $exe -Parent) 'performance-report.json'
}

$dataRoot = Join-Path $repo 'cmake-build-performance-data'
New-Item -ItemType Directory -Force -Path $dataRoot | Out-Null
$previousDataDir = $env:STICKY_NOTES_DATA_DIR
$previousStyle = $env:QT_QUICK_CONTROLS_STYLE
$startupSamples = @()
$rssSamples = @()

try {
    $env:QT_QUICK_CONTROLS_STYLE = 'Basic'
    for ($index = 0; $index -lt $Runs; $index++) {
        $runData = Join-Path $dataRoot "run-$PID-$index"
        New-Item -ItemType Directory -Force -Path $runData | Out-Null
        $env:STICKY_NOTES_DATA_DIR = $runData

        $stopwatch = [Diagnostics.Stopwatch]::StartNew()
        $process = Start-Process -FilePath $exe -PassThru
        try {
            if (-not $process.WaitForInputIdle(5000)) {
                throw "ChronoNotes did not become input-ready in run $($index + 1)."
            }
            $stopwatch.Stop()
            $startupSamples += [int64]$stopwatch.ElapsedMilliseconds

            Start-Sleep -Milliseconds $IdleWaitMs
            $process.Refresh()
            if ($process.HasExited) {
                throw "ChronoNotes exited unexpectedly in run $($index + 1)."
            }
            $rssSamples += [int64]$process.WorkingSet64
        } finally {
            if ($process -and -not $process.HasExited) {
                $null = $process.CloseMainWindow()
                if (-not $process.WaitForExit(3000)) {
                    Stop-Process -Id $process.Id -Force
                    $process.WaitForExit()
                }
            }
        }
    }
} finally {
    $env:STICKY_NOTES_DATA_DIR = $previousDataDir
    $env:QT_QUICK_CONTROLS_STYLE = $previousStyle
}

function Get-P95([array]$Values) {
    $sorted = @($Values | Sort-Object)
    $index = [Math]::Ceiling($sorted.Count * 0.95) - 1
    return [int64]$sorted[$index]
}

$startupP95 = Get-P95 $startupSamples
$rssP95Bytes = Get-P95 $rssSamples
$rssP95MiB = [Math]::Round($rssP95Bytes / 1MB, 2)
$report = [ordered]@{
    executable = $exe
    runs = $Runs
    startupMs = @($startupSamples)
    startupP95Ms = $startupP95
    idleRssMiB = @($rssSamples | ForEach-Object { [Math]::Round($_ / 1MB, 2) })
    idleRssP95MiB = $rssP95MiB
    idleWaitMs = $IdleWaitMs
    thresholds = [ordered]@{
        startupP95Ms = $StartupLimitMs
        idleRssP95MiB = $RssLimitMiB
    }
}
$report | ConvertTo-Json -Depth 4 |
    Set-Content -LiteralPath $ReportPath -Encoding utf8

if ($startupP95 -gt $StartupLimitMs) {
    throw "Cold startup p95 is $startupP95 ms; limit is $StartupLimitMs ms."
}
if ($rssP95Bytes -gt ($RssLimitMiB * 1MB)) {
    throw "Idle RSS p95 is $rssP95MiB MiB; limit is $RssLimitMiB MiB."
}

[PSCustomObject]$report
