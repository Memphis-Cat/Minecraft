@echo off
setlocal EnableExtensions

rem Normalize %~dp0 so no quoted argument ends with a trailing backslash.
for %%I in ("%~dp0.") do set "ROOT=%%~fI"
set "BIN=%ROOT%\bin"
set "LOG_DIR=%ROOT%\logs"
set "BUILD_LOG=%LOG_DIR%\build.log"

if not exist "%LOG_DIR%" mkdir "%LOG_DIR%"
> "%BUILD_LOG%" echo ============================================================
>>"%BUILD_LOG%" echo Build started: %DATE% %TIME%
>>"%BUILD_LOG%" echo Root: %ROOT%

rem Remove the folder produced by older launchers that cloned inside bin.
if exist "%BIN%\source" (
    echo Removing legacy bin\source folder...
    >>"%BUILD_LOG%" echo Removing legacy folder: %BIN%\source
    rmdir /S /Q "%BIN%\source" >>"%BUILD_LOG%" 2>&1
)

echo [1/4] Building launcher...
call "%ROOT%\build_launcher.bat" >>"%BUILD_LOG%" 2>&1
if errorlevel 1 goto :error

echo [2/4] Building Minecraft...
call "%ROOT%\build_game.bat" "%ROOT%" "%BIN%" >>"%BUILD_LOG%" 2>&1
if errorlevel 1 goto :error

rem Remove every temporary CMake build folder and the old output layout.
if exist "%ROOT%\build-launcher" rmdir /S /Q "%ROOT%\build-launcher" >>"%BUILD_LOG%" 2>&1
if exist "%ROOT%\build-game" rmdir /S /Q "%ROOT%\build-game" >>"%BUILD_LOG%" 2>&1
if exist "%ROOT%\dist" rmdir /S /Q "%ROOT%\dist" >>"%BUILD_LOG%" 2>&1

echo [3/4] Creating desktop shortcut...
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%ROOT%\tools\create_shortcut.ps1" -Target "%BIN%\Launcher.exe" -WorkingDirectory "%BIN%" -Name "Minecraft Launcher" >>"%BUILD_LOG%" 2>&1
if errorlevel 1 goto :error

echo [4/4] Recording local release information...
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%ROOT%\tools\install_local_manifest.ps1" -SourceDirectory "%ROOT%" -GameExecutable "%BIN%\Minecraft.exe" -OutputManifest "%BIN%\local_manifest.json" >>"%BUILD_LOG%" 2>&1
if errorlevel 1 (
    echo WARNING: Could not create local_manifest.json. The launcher will verify and update on first start.
    >>"%BUILD_LOG%" echo WARNING: Local manifest stamping failed with exit code %ERRORLEVEL%.
    if exist "%BIN%\local_manifest.json" del /F /Q "%BIN%\local_manifest.json" >>"%BUILD_LOG%" 2>&1
)

>>"%BUILD_LOG%" echo Build completed successfully: %DATE% %TIME%

echo.
echo Build complete.
echo Launcher: %BIN%\Launcher.exe
echo Game:     %BIN%\Minecraft.exe
echo Build log: %BUILD_LOG%
echo.
echo A desktop shortcut named "Minecraft Launcher" was created.
echo Always start the game through that shortcut or bin\Launcher.exe.
exit /b 0

:error
set "EXIT_CODE=%ERRORLEVEL%"
if "%EXIT_CODE%"=="0" set "EXIT_CODE=1"
if exist "%ROOT%\build-launcher" rmdir /S /Q "%ROOT%\build-launcher" >>"%BUILD_LOG%" 2>&1
if exist "%ROOT%\build-game" rmdir /S /Q "%ROOT%\build-game" >>"%BUILD_LOG%" 2>&1
>>"%BUILD_LOG%" echo ERROR: Full build failed with exit code %EXIT_CODE% at %DATE% %TIME%.
echo.
echo ERROR: Full build failed.
echo Log: %BUILD_LOG%
echo.
powershell.exe -NoLogo -NoProfile -Command "if (Test-Path -LiteralPath '%BUILD_LOG%') { Get-Content -LiteralPath '%BUILD_LOG%' -Tail 40 }"
exit /b %EXIT_CODE%
