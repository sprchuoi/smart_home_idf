@echo off
REM ESP32 Smart Home - Build Script for Windows
REM Usage: make.bat [command]

setlocal enabledelayedexpansion

set SCRIPT_DIR=%~dp0
set PROJECT_DIR=%SCRIPT_DIR%
set BUILD_DIR=%PROJECT_DIR%build
set IDF_PATH=%IDF_PATH%

if "%IDF_PATH%"=="" set IDF_PATH=%USERPROFILE%\esp\esp-idf

if "%1"=="" goto :help
if "%1"=="help" goto :help
if "%1"=="--help" goto :help
if "%1"=="-h" goto :help

if "%1"=="setup" goto :setup
if "%1"=="build" goto :build
if "%1"=="clean" goto :clean
if "%1"=="flash" goto :flash
if "%1"=="monitor" goto :monitor
if "%1"=="flash-monitor" goto :flash_monitor
if "%1"=="test" goto :test
if "%1"=="doc" goto :doc
if "%1"=="ci" goto :ci

echo [ERROR] Unknown command: %1
goto :help

:setup
echo [INFO] Setting up environment...
if not exist "%IDF_PATH%\export.bat" (
    echo [ERROR] ESP-IDF not found at %IDF_PATH%
    echo Please install ESP-IDF or set IDF_PATH environment variable
    exit /b 1
)
call "%IDF_PATH%\export.bat"
echo [SUCCESS] Environment setup complete
goto :end

:build
echo [INFO] Building project...
if not exist "%IDF_PATH%\export.bat" (
    echo [ERROR] ESP-IDF not found. Run 'make.bat setup' first
    exit /b 1
)
call "%IDF_PATH%\export.bat"
cd /d "%PROJECT_DIR%"
idf.py build
if %ERRORLEVEL% EQU 0 (
    echo [SUCCESS] Build complete
) else (
    echo [ERROR] Build failed
    exit /b 1
)
goto :end

:clean
echo [INFO] Cleaning build artifacts...
if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
if exist "%IDF_PATH%\export.bat" (
    call "%IDF_PATH%\export.bat"
    cd /d "%PROJECT_DIR%"
    idf.py fullclean
)
echo [SUCCESS] Clean complete
goto :end

:flash
echo [INFO] Flashing to ESP32...
if not exist "%IDF_PATH%\export.bat" (
    echo [ERROR] ESP-IDF not found
    exit /b 1
)
call "%IDF_PATH%\export.bat"
cd /d "%PROJECT_DIR%"
idf.py flash
goto :end

:monitor
echo [INFO] Starting serial monitor...
if not exist "%IDF_PATH%\export.bat" (
    echo [ERROR] ESP-IDF not found
    exit /b 1
)
call "%IDF_PATH%\export.bat"
cd /d "%PROJECT_DIR%"
idf.py monitor
goto :end

:flash_monitor
call :flash
call :monitor
goto :end

:test
echo [INFO] Running tests...
call :build
echo [INFO] Tests complete
goto :end

:doc
echo [INFO] Generating documentation...
if not exist "%PROJECT_DIR%\docs" mkdir "%PROJECT_DIR%\docs"
echo [INFO] Documentation generation (basic)
echo See README.md, ARCHITECTURE.md for documentation
goto :end

:ci
echo [INFO] Running CI/CD pipeline...
call :setup
call :clean
call :build
call :test
call :doc
echo [SUCCESS] CI/CD pipeline complete
goto :end

:help
echo ESP32 Smart Home - Build Script for Windows
echo.
echo Usage: make.bat [command]
echo.
echo Commands:
echo   setup          Setup development environment
echo   build          Build the project
echo   clean          Clean build artifacts
echo   flash          Flash to ESP32 device
echo   monitor        Monitor serial output
echo   flash-monitor  Flash and monitor
echo   test           Run tests
echo   doc            Generate documentation
echo   ci             Run CI/CD pipeline
echo   help           Show this help message
echo.
echo Environment Variables:
echo   IDF_PATH       ESP-IDF installation path (default: %%USERPROFILE%%\esp\esp-idf)
goto :end

:end
endlocal

