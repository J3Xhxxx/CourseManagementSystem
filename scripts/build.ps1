param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',
    [string]$QtPrefix = '',
    [switch]$CoreOnly
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'toolchain.ps1')
$projectRoot = Split-Path -Parent $PSScriptRoot
$localCmake = Join-Path $projectRoot '.tools\cmake\data\bin\cmake.exe'
$cmakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
if ($cmakeCommand) {
    $cmakePath = $cmakeCommand.Source
} elseif (Test-Path -LiteralPath $localCmake) {
    $cmakePath = $localCmake
} else {
    throw 'CMake is missing. Install CMake 3.24+ or run: python -m pip install --target .tools cmake'
}
$ninjaCommand = Get-Command ninja -ErrorAction SilentlyContinue
$ninjaPath = if ($ninjaCommand) { $ninjaCommand.Source } else { Join-Path $projectRoot '.tools\bin\ninja.exe' }
if (-not (Test-Path -LiteralPath $ninjaPath)) {
    throw 'Ninja is missing. Run: python -m pip install --target .tools ninja'
}
# 使用支持中文仓库路径的 Ninja 构建。
$buildKind = if ($CoreOnly) { 'core' } else { 'qt' }
$sourcePath = $projectRoot
# MinGW 版 moc 不能正确处理中文路径，通过 ASCII 目录联接访问同一份源码。
if ($projectRoot -match '[^\x00-\x7F]') {
    $hash = [System.Security.Cryptography.SHA256]::Create()
    try { $key = ([BitConverter]::ToString($hash.ComputeHash([Text.Encoding]::UTF8.GetBytes($projectRoot)))).Replace('-', '').Substring(0, 12) }
    finally { $hash.Dispose() }
    $aliasRoot = Join-Path $env:LOCALAPPDATA 'CoursePlanner\Workspaces'
    New-Item -ItemType Directory -Force -Path $aliasRoot | Out-Null
    $sourcePath = Join-Path $aliasRoot $key
    if (Test-Path -LiteralPath $sourcePath) {
        $alias = Get-Item -LiteralPath $sourcePath
        if ($alias.LinkType -ne 'Junction' -or [IO.Path]::GetFullPath($alias.Target) -ne $projectRoot) {
            throw 'Build alias exists but does not point to this project.'
        }
    } else {
        New-Item -ItemType Junction -Path $sourcePath -Target $projectRoot | Out-Null
    }
}
$buildPath = Join-Path $sourcePath "build-$buildKind-$Configuration"
$options = @("-DCMAKE_MAKE_PROGRAM=$ninjaPath", "-DCMAKE_BUILD_TYPE=$Configuration")
$compilerBin = Get-CoursePlannerCompilerBin
if ($compilerBin) {
    $options += "-DCMAKE_CXX_COMPILER=$compilerBin\g++.exe"
}
if ($CoreOnly) {
    $options += '-DCOURSE_PLANNER_WITH_QT=OFF'
} else {
    $QtPrefix = Get-CoursePlannerQtPrefix $QtPrefix
    $options += '-DCOURSE_PLANNER_WITH_QT=ON'
    $options += @('-UQt6*_DIR', "-DCMAKE_PREFIX_PATH=$QtPrefix")
}
$previousPath = $env:PATH
try {
    if ($compilerBin) { $env:PATH = "$compilerBin;$env:PATH" }
    # 源码路径或编译器变化时重新配置，保留其他情况下的增量构建。
    $cache = Join-Path $buildPath 'CMakeCache.txt'
    $configureOptions = @()
    if (Test-Path -LiteralPath $cache) {
        $cachedSource = (Select-String -LiteralPath $cache -Pattern '^CMAKE_HOME_DIRECTORY:INTERNAL=(.*)$').Matches.Groups[1].Value
        if ($cachedSource -and $cachedSource.Replace('/', '\') -ne $sourcePath) { $configureOptions += '--fresh' }
        $compilerMatch = Select-String -LiteralPath $cache -Pattern '^CMAKE_CXX_COMPILER:[^=]+=(.*)$'
        if ($compilerBin -and $compilerMatch) {
            $cachedCompiler = $compilerMatch.Matches.Groups[1].Value.Replace('/', '\')
            if ($cachedCompiler -ne (Join-Path $compilerBin 'g++.exe') -and $configureOptions -notcontains '--fresh') {
                $configureOptions += '--fresh'
            }
        }
    }
    & $cmakePath @configureOptions -S $sourcePath -B $buildPath -G Ninja @options
    if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed' }
    & $cmakePath --build $buildPath --parallel 2
    if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
    if (-not $CoreOnly) {
        Install-CoursePlannerRuntime -AppPath (Join-Path $buildPath 'course_app.exe') -QtPrefix $QtPrefix
    }
} finally {
    $env:PATH = $previousPath
}
