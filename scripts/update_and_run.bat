@echo off
set "URL=https://github.com/dflat/physics-integration-test/releases/download/latest-preview/physics-integration-test-windows.zip"
set "ZIP_FILE=game_update.zip"
set "EXE_NAME=demo.exe"

echo [1/3] Downloading latest build...
curl -L %URL% -o %ZIP_FILE%

echo [2/3] Extracting files...
:: Windows has a built-in tar command that handles zips perfectly
tar -xf %ZIP_FILE%

echo [3/3] Launching %EXE_NAME%...
del %ZIP_FILE%
start "" "%EXE_NAME%"

exit
