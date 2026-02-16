@echo off
setlocal enabledelayedexpansion

:: Default values
set BUILD_TYPE=Debug
set RUN_TESTS=0
set CLEAN=0

:parse_args
if "%~1"=="" goto run_build
if "%~1"=="-r" (set BUILD_TYPE=Release)
if "%~1"=="--release" (set BUILD_TYPE=Release)
if "%~1"=="-t" (set RUN_TESTS=1)
if "%~1"=="--test" (set RUN_TESTS=1)
if "%~1"=="-c" (set CLEAN=1)
if "%~1"=="--clean" (set CLEAN=1)
if "%~1"=="-h" (goto usage)
if "%~1"=="--help" (goto usage)
shift
goto parse_args

:usage
echo Usage: build.bat [options]
echo Options:
echo   -r, --release    Build in Release mode (default: Debug)
echo   -t, --test       Run unit tests after build
echo   -c, --clean      Remove build directory before building
goto :eof

:run_build
set BUILD_DIR=build
if "%BUILD_TYPE%"=="Release" (set BUILD_DIR=build_release)

if %CLEAN%==1 (
    echo Cleaning build directory: %BUILD_DIR%
    if exist %BUILD_DIR% rd /s /q %BUILD_DIR%
)

echo --- Configuring (%BUILD_TYPE%) ---
cmake -B %BUILD_DIR%

echo --- Building ---
:: MSVC is a multi-config generator, so we specify --config here
cmake --build %BUILD_DIR% --config %BUILD_TYPE%

if %RUN_TESTS%==1 (
    echo --- Running Tests ---
    cd %BUILD_DIR%
    ctest -C %BUILD_TYPE% --output-on-failure
    cd ..
)

echo --- Build Complete ---
echo Executable: .\%BUILD_DIR%\%BUILD_TYPE%\demo.exe
goto :eof
