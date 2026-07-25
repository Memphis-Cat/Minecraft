@echo off
setlocal EnableExtensions

set "PROJECT_DIR=D:\Minecraft"

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

echo Staging all changed, new, and deleted files...
git add -A
if errorlevel 1 goto :error

git diff --cached --quiet
if errorlevel 1 goto :commit_changes

echo No local file changes need a new commit.
goto :after_commit

:commit_changes
set "MESSAGE="
set /p "MESSAGE=Commit message [Update project]: "
if not defined MESSAGE set "MESSAGE=Update project"

git commit -m "%MESSAGE%"
if errorlevel 1 goto :error

:after_commit
git ls-remote --exit-code --heads origin main >nul 2>&1
if not errorlevel 1 (
    echo Downloading remote commits before upload...
    git pull --rebase origin main
    if errorlevel 1 (
        echo.
        echo ERROR: Git could not combine your work with GitHub.
        echo Resolve the conflict, then run this file again.
        pause
        exit /b 1
    )
)

echo Uploading to GitHub...
git push -u origin main
if errorlevel 1 goto :error

echo.
echo Upload complete.
pause
exit /b 0

:error
echo.
echo ERROR: Upload failed. Read the Git message above.
pause
exit /b 1
