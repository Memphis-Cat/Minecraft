@echo off
setlocal EnableExtensions
set "ROOT=%~dp0"
set "BUILD=%ROOT%build-launcher"
set "DIST=%ROOT%dist"

where cmake >nul 2>nul || (
  echo ERROR: CMake was not found in PATH.
  exit /b 1
)

cmake -S "%ROOT%" -B "%BUILD%" -A x64 -DMC_BUILD_LAUNCHER=ON -DMC_BUILD_GAME=OFF || exit /b 1
cmake --build "%BUILD%" --config Release --target Launcher || exit /b 1
if not exist "%DIST%" mkdir "%DIST%"
copy /Y "%BUILD%\bin\Release\Launcher.exe" "%DIST%\Launcher.exe" >nul || exit /b 1

echo Built: %DIST%\Launcher.exe
exit /b 0
