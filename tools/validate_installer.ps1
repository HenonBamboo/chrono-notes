[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$Installer,
    [string]$InstallDirectory,
    [string]$DataDirectory
)

$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$installerPath = (Resolve-Path -LiteralPath $Installer).Path
if ([string]::IsNullOrWhiteSpace($InstallDirectory)) {
    $InstallDirectory = Join-Path $repo 'cmake-build-installer-validation'
}
if ([string]::IsNullOrWhiteSpace($DataDirectory)) {
    $DataDirectory = Join-Path $repo 'cmake-build-installer-user-data'
}
$install = [IO.Path]::GetFullPath($InstallDirectory)
$data = [IO.Path]::GetFullPath($DataDirectory)
$repoPrefix = $repo.TrimEnd('\') + '\'
foreach ($path in @($install, $data)) {
    if ($path.Equals($repo, [StringComparison]::OrdinalIgnoreCase) -or
        -not $path.StartsWith($repoPrefix, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Validation directories must remain inside the repository: $path"
    }
}

if (Test-Path -LiteralPath $install) {
    Remove-Item -LiteralPath $install -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $data | Out-Null

function Invoke-SilentInstaller {
    $process = Start-Process -FilePath $installerPath `
        -ArgumentList @('/S', "/D=$install") -PassThru -Wait
    if ($process.ExitCode -ne 0) {
        throw "Installer exited with code $($process.ExitCode)."
    }
}

Invoke-SilentInstaller
$application = Join-Path $install 'ChronoNotes.exe'
if (-not (Test-Path -LiteralPath $application)) {
    throw "Installed application is missing: $application"
}
if (Get-ChildItem -LiteralPath $install -Filter portable.flag -File -Recurse `
        -ErrorAction SilentlyContinue) {
    throw 'Installed application unexpectedly contains portable.flag.'
}

# A second install to the same directory exercises the upgrade path.
Invoke-SilentInstaller
if (-not (Test-Path -LiteralPath $application)) {
    throw 'Application is missing after upgrade validation.'
}

$previousDataDir = $env:STICKY_NOTES_DATA_DIR
$env:STICKY_NOTES_DATA_DIR = $data
try {
    $applicationProcess = Start-Process -FilePath $application -PassThru
    try {
        if (-not $applicationProcess.WaitForInputIdle(5000)) {
            throw 'Installed application did not become input-ready.'
        }
    } finally {
        if (-not $applicationProcess.HasExited) {
            $null = $applicationProcess.CloseMainWindow()
            if (-not $applicationProcess.WaitForExit(3000)) {
                Stop-Process -Id $applicationProcess.Id -Force
                $applicationProcess.WaitForExit()
            }
        }
    }
} finally {
    $env:STICKY_NOTES_DATA_DIR = $previousDataDir
}

$database = Join-Path $data 'notes.sqlite'
if (-not (Test-Path -LiteralPath $database)) {
    throw 'Installed application did not create its isolated user database.'
}

$uninstaller = Join-Path $install 'Uninstall.exe'
if (-not (Test-Path -LiteralPath $uninstaller)) {
    throw "Uninstaller is missing: $uninstaller"
}
$uninstallProcess = Start-Process -FilePath $uninstaller `
    -ArgumentList '/S' -PassThru -Wait
if ($uninstallProcess.ExitCode -ne 0) {
    throw "Uninstaller exited with code $($uninstallProcess.ExitCode)."
}
if (-not (Test-Path -LiteralPath $database)) {
    throw 'Uninstall removed user data.'
}

[PSCustomObject]@{
    Installer = $installerPath
    InstalledTwice = $true
    PortableMarkerExcluded = $true
    UserDataPreservedAfterUninstall = $true
    UserDatabase = $database
}
