@echo off
setlocal EnableExtensions
set "SOURCE=%~1"
set "OUTPUT=%~2"
if not defined SOURCE set "SOURCE=%~dp0"
if not defined OUTPUT set "OUTPUT=%~dp0dist\game"
for %%I in ("%SOURCE%") do set "SOURCE=%%~fI"
for %%I in ("%OUTPUT%") do set "OUTPUT=%%~fI"
set "BUILD=%SOURCE%\build-game"

where cmake >nul 2>nul || (
  echo ERROR: CMake was not found in PATH.
  exit /b 1
)

cmake -S "%SOURCE%" -B "%BUILD%" -A x64 -DMC_BUILD_LAUNCHER=OFF -DMC_BUILD_GAME=ON || exit /b 1
cmake --build "%BUILD%" --config Release --target Minecraft || exit /b 1
if not exist "%OUTPUT%" mkdir "%OUTPUT%"
copy /Y "%BUILD%\bin\Release\Minecraft.exe" "%OUTPUT%\Minecraft.exe" >nul || exit /b 1

echo Built: %OUTPUT%\Minecraft.exe
exit /b 0
