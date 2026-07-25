@echo off
setlocal EnableExtensions
call "%~dp0build_launcher.bat" || exit /b 1
call "%~dp0build_game.bat" "%~dp0" "%~dp0dist\game" || exit /b 1

echo.
echo Build complete.
echo Launcher: %~dp0dist\Launcher.exe
echo Game:     %~dp0dist\game\Minecraft.exe
echo.
echo Start the game through Launcher.exe. Minecraft.exe rejects direct launches.
exit /b 0
