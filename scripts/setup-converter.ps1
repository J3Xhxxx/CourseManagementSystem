param([string]$Python = 'python')
$ErrorActionPreference = 'Stop'
$converterRoot = Split-Path -Parent $PSScriptRoot
$converterEnvironment = Join-Path $converterRoot '.tools\timetable-converter'
& $Python -m venv $converterEnvironment
if ($LASTEXITCODE -ne 0) { throw 'Python 3.10+ is needed to create the converter environment.' }
$converterPython = Join-Path $converterEnvironment 'Scripts\python.exe'
& $converterPython -m pip install -r (Join-Path $converterRoot 'tools\timetable_converter\requirements.txt')
if ($LASTEXITCODE -ne 0) { throw 'Converter dependency installation failed.' }
Write-Output 'Timetable converter is ready.'
