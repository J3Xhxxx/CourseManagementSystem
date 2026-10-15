@echo off
setlocal
pushd "%~dp0"
set "CoursePlannerConfig=Release"
if not exist "build-qt-Release\course_app.exe" set "CoursePlannerConfig=Debug"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ".\scripts\run.ps1" -Configuration "%CoursePlannerConfig%" %*
set "CoursePlannerExitCode=%errorlevel%"
popd
if not "%CoursePlannerExitCode%"=="0" pause
exit /b %CoursePlannerExitCode%
