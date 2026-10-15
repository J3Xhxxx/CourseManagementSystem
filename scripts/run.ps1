param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',
    [string]$QtPrefix = '',
    [string]$ImportCsv = ''
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'toolchain.ps1')
$projectRoot = Split-Path -Parent $PSScriptRoot
$appPath = Join-Path $projectRoot "build-qt-$Configuration\course_app.exe"
if (-not (Test-Path -LiteralPath $appPath)) {
    throw 'Application is not built. Run ./scripts/build.ps1 first.'
}
$appDirectory = Split-Path -Parent $appPath
$runtimeFiles = @('Qt6Core.dll', 'Qt6Gui.dll', 'Qt6Widgets.dll',
    'libgcc_s_seh-1.dll', 'libstdc++-6.dll', 'libwinpthread-1.dll', 'platforms\qwindows.dll')
$missing = @($runtimeFiles | Where-Object { -not (Test-Path -LiteralPath (Join-Path $appDirectory $_)) })
if ($QtPrefix -or $missing.Count) {
    Install-CoursePlannerRuntime -AppPath $appPath -QtPrefix $QtPrefix
}
$arguments = @()
if ($ImportCsv) { $arguments += @('--import', $ImportCsv) }
& $appPath @arguments
if ($LASTEXITCODE -eq -1073741515) {
    throw 'A runtime DLL is missing (0xC0000135). Run ./scripts/build.ps1 to restore the application runtime.'
}
if ($LASTEXITCODE -ne 0) { throw "Application failed with code $LASTEXITCODE" }
