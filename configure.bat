@echo off
REM VA2022a CMake Configuration Script
REM This script configures the VA build with all necessary options

echo ========================================
echo VA2022a CMake Configuration
echo ========================================
echo.

cd /d "%~dp0build"

REM Clean previous configuration if requested
if "%1"=="clean" (
    echo Cleaning previous build...
    if exist CMakeCache.txt del CMakeCache.txt
    if exist CMakeFiles rmdir /s /q CMakeFiles
    echo Done cleaning.
    echo.
)

echo Configuring CMake...
echo.

cmake ../source -G "Visual Studio 17 2022" -A x64 ^
  -DCMAKE_TOOLCHAIN_FILE="%~dp0va_toolchain.cmake" ^
  -DITA_VA_WITH_CORE=ON ^
  -DITA_VA_WITH_SERVER_APP=ON ^
  -DITA_VA_WITH_BINDING_MATLAB=ON ^
  -DITA_VA_WITH_BINDING_PYTHON=OFF ^
  -DITA_VACORE_WITH_RENDERER_BINAURAL_AIR_TRAFFIC_NOISE=OFF

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ========================================
    echo CMake configuration FAILED!
    echo ========================================
    pause
    exit /b 1
)

echo.
echo ========================================
echo CMake configuration completed successfully!
echo ========================================
echo.
echo Next steps:
echo   1. To build: cmake --build . --config Release -j8
echo   2. To install: cmake --build . --config Release --target INSTALL -j8
echo.
pause
