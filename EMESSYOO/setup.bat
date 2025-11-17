@echo off
REM MSU-1 Prep Studio - Setup Script for Windows
REM This script downloads JUCE and prepares the build environment

echo MSU-1 Prep Studio - Setup
echo ==========================
echo.

REM Check for CMake
where cmake >nul 2>nul
if %errorlevel% neq 0 (
    echo Error: CMake is not installed!
    echo Please install CMake 3.22 or higher
    pause
    exit /b 1
)

echo [ OK ] CMake found

REM Check for Git
where git >nul 2>nul
if %errorlevel% neq 0 (
    echo Error: Git is not installed!
    echo Please install Git
    pause
    exit /b 1
)

echo [ OK ] Git found

REM Clone JUCE if not present
if not exist "JUCE" (
    echo.
    echo Downloading JUCE Framework...
    git clone --depth 1 --branch 8.0.0 https://github.com/juce-framework/JUCE.git
    
    if %errorlevel% equ 0 (
        echo [ OK ] JUCE downloaded successfully
    ) else (
        echo Error: Failed to download JUCE
        pause
        exit /b 1
    )
) else (
    echo [ OK ] JUCE already present
)

REM Create build directory
echo.
echo Creating build directory...
if not exist "build" mkdir build

echo.
echo Setup complete!
echo.
echo To build the project:
echo   cd build
echo   cmake ..
echo   cmake --build . --config Release
echo.

pause
