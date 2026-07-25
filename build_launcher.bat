@echo off
setlocal EnableExtensions

set "ROOT=%~dp0"
set "BUILD=%ROOT%build-launcher"
set "BIN=%ROOT%bin"

where cmake >nul 2>nul || (
    echo ERROR: CMake was not found in PATH.
    exit /b 1
)

if exist "%BUILD%" rmdir /S /Q "%BUILD%"

cmake -S "%ROOT%" -B "%BUILD%" -A x64 -DMC_BUILD_LAUNCHER=ON -DMC_BUILD_GAME=OFF
if errorlevel 1 goto :error

cmake --build "%BUILD%" --config Release --target Launcher
if errorlevel 1 goto :error

if not exist "%BIN%" mkdir "%BIN%"
copy /Y "%BUILD%\bin\Release\Launcher.exe" "%BIN%\Launcher.exe" >nul
if errorlevel 1 goto :error

if exist "%BUILD%" rmdir /S /Q "%BUILD%"

echo Built: %BIN%\Launcher.exe
exit /b 0

:error
set "EXIT_CODE=%ERRORLEVEL%"
if "%EXIT_CODE%"=="0" set "EXIT_CODE=1"
if exist "%BUILD%" rmdir /S /Q "%BUILD%"
echo ERROR: Launcher build failed.
exit /b %EXIT_CODE%
