@echo off
setlocal

set "PROJECT_DIR=D:\VMAT"

cd /d "%PROJECT_DIR%"
if errorlevel 1 (
    echo ERROR: Could not open "%PROJECT_DIR%".
    pause
    exit /b 1
)

git rev-parse --is-inside-work-tree >nul 2>&1
if errorlevel 1 (
    echo ERROR: "%PROJECT_DIR%" is not a Git repository.
    echo Run 01_setup_project.bat first.
    pause
    exit /b 1
)

git ls-remote --exit-code --heads origin main >nul 2>&1
if errorlevel 1 (
    echo GitHub does not have a main branch yet.
    echo Upload your first commit with 02_upload_to_github.bat.
    pause
    exit /b 0
)

echo Downloading GitHub changes...
git pull --rebase --autostash origin main
if errorlevel 1 goto :error

echo Updating submodules...
git submodule sync --recursive
if errorlevel 1 goto :error

git submodule update --init --recursive
if errorlevel 1 goto :error

echo.
echo Download complete.
pause
exit /b 0

:error
echo.
echo ERROR: Download failed. Read the Git message above.
pause
exit /b 1
