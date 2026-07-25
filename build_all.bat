@echo off
setlocal EnableExtensions

set "ROOT=%~dp0"
set "BIN=%ROOT%bin"

call "%ROOT%build_launcher.bat"
if errorlevel 1 goto :error

call "%ROOT%build_game.bat" "%ROOT%" "%BIN%"
if errorlevel 1 goto :error

rem Remove every temporary CMake build folder and the old output layout.
if exist "%ROOT%build-launcher" rmdir /S /Q "%ROOT%build-launcher"
if exist "%ROOT%build-game" rmdir /S /Q "%ROOT%build-game"
if exist "%ROOT%dist" rmdir /S /Q "%ROOT%dist"

powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%ROOT%tools\create_shortcut.ps1" -Target "%BIN%\Launcher.exe" -WorkingDirectory "%BIN%" -Name "Minecraft Launcher"
if errorlevel 1 goto :error

echo.
echo Build complete.
echo Launcher: %BIN%\Launcher.exe
echo Game:     %BIN%\Minecraft.exe
echo.
echo A desktop shortcut named "Minecraft Launcher" was created.
echo Always start the game through that shortcut or bin\Launcher.exe.
exit /b 0

:error
set "EXIT_CODE=%ERRORLEVEL%"
if "%EXIT_CODE%"=="0" set "EXIT_CODE=1"
if exist "%ROOT%build-launcher" rmdir /S /Q "%ROOT%build-launcher"
if exist "%ROOT%build-game" rmdir /S /Q "%ROOT%build-game"
echo.
echo ERROR: Full build failed.
exit /b %EXIT_CODE%
