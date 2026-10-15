# 构建和启动共用工具链查找、Qt 运行库部署。
# 本机 Qt 位于桌面应用 LocalCache，普通终端也需要查找这个物理目录。
function Find-CoursePlannerDirectory {
    param([string]$RelativePath, [string]$Marker)
    $roots = @()
    $packages = Join-Path $env:LOCALAPPDATA 'Packages'
    if (Test-Path -LiteralPath $packages) {
        foreach ($package in (Get-ChildItem -LiteralPath $packages -Directory -Filter 'OpenAI.Codex_*' -ErrorAction SilentlyContinue | Sort-Object FullName)) {
            $roots += Join-Path $package.FullName 'LocalCache\Local\CoursePlanner'
        }
    }
    $roots += Join-Path $env:LOCALAPPDATA 'CoursePlanner'
    foreach ($root in $roots) {
        $directory = Join-Path $root $RelativePath
        if (Test-Path -LiteralPath (Join-Path $directory $Marker) -PathType Leaf) {
            return $directory
        }
    }
    return $null
}

function Get-CoursePlannerQtPrefix {
    param([string]$Requested = '')
    if ($Requested) {
        $prefix = [IO.Path]::GetFullPath($Requested)
        if (-not (Test-Path -LiteralPath (Join-Path $prefix 'bin\Qt6Core.dll') -PathType Leaf)) {
            throw "Qt6Core.dll was not found under QtPrefix: $prefix"
        }
        return $prefix
    }
    $prefix = Find-CoursePlannerDirectory 'Qt\6.8.3\mingw_64' 'bin\Qt6Core.dll'
    if (-not $prefix) { throw 'Qt 6 was not found. Pass -QtPrefix with the installed Qt directory.' }
    return $prefix
}

function Get-CoursePlannerCompilerBin {
    return Find-CoursePlannerDirectory 'Tools\mingw1310_64\bin' 'g++.exe'
}

# 将 DLL 和平台插件放在程序旁，直接双击 exe 也能启动。
function Install-CoursePlannerRuntime {
    param([string]$AppPath, [string]$QtPrefix = '')
    $qtBin = Join-Path (Get-CoursePlannerQtPrefix $QtPrefix) 'bin'
    $deployTool = Join-Path $qtBin 'windeployqt.exe'
    if (-not (Test-Path -LiteralPath $deployTool)) {
        throw "Qt deployment tool not found: $deployTool"
    }
    $compilerBin = Get-CoursePlannerCompilerBin
    $previousPath = $env:PATH
    try {
        $env:PATH = "$qtBin;$compilerBin;$previousPath"
        & $deployTool --no-translations --no-opengl-sw --compiler-runtime $AppPath
        if ($LASTEXITCODE -ne 0) { throw "Qt runtime deployment failed with code $LASTEXITCODE" }
    } finally {
        $env:PATH = $previousPath
    }
}
