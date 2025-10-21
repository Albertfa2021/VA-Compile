@echo off
REM VA2022a Build Script
REM This script builds the VA project in Release mode

echo ========================================
echo VA2022a Build
echo ========================================
echo.

cd /d "%~dp0build"

if not exist CMakeCache.txt (
    echo ERROR: Project not configured!
    echo Please run configure.bat first.
    pause
    exit /b 1
)

echo Building in Release mode with 8 parallel jobs...
echo.

cmake --build . --config Release -j8

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ========================================
    echo Build FAILED!
    echo ========================================
    pause
    exit /b 1
)

echo.
echo ========================================
echo Build completed successfully!
echo ========================================
echo.
echo Build outputs are in: build\Release\
echo.
pause
