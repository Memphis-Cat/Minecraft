@echo off
setlocal EnableExtensions

set "SOURCE=%~1"
set "OUTPUT=%~2"
if not defined SOURCE set "SOURCE=%~dp0"
if not defined OUTPUT set "OUTPUT=%~dp0bin"

for %%I in ("%SOURCE%") do set "SOURCE=%%~fI"
for %%I in ("%OUTPUT%") do set "OUTPUT=%%~fI"
set "BUILD=%SOURCE%\build-game"

where cmake >nul 2>nul || (
    echo ERROR: CMake was not found in PATH.
    exit /b 1
)

if exist "%BUILD%" rmdir /S /Q "%BUILD%"

cmake -S "%SOURCE%" -B "%BUILD%" -A x64 -DMC_BUILD_LAUNCHER=OFF -DMC_BUILD_GAME=ON
if errorlevel 1 goto :error

cmake --build "%BUILD%" --config Release --target Minecraft
if errorlevel 1 goto :error

if not exist "%OUTPUT%" mkdir "%OUTPUT%"
copy /Y "%BUILD%\bin\Release\Minecraft.exe" "%OUTPUT%\Minecraft.exe" >nul
if errorlevel 1 goto :error

if exist "%BUILD%" rmdir /S /Q "%BUILD%"

echo Built: %OUTPUT%\Minecraft.exe
exit /b 0

:error
set "EXIT_CODE=%ERRORLEVEL%"
if "%EXIT_CODE%"=="0" set "EXIT_CODE=1"
if exist "%BUILD%" rmdir /S /Q "%BUILD%"
echo ERROR: Minecraft build failed.
exit /b %EXIT_CODE%
