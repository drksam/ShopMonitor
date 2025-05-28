@echo off
:: Package the firmware for distribution
echo ShopMonitor Firmware Build Script (Windows version)

:: Create temporary directories
set TEMP_DIR_ESP32=%TEMP%\esp32_build
set TEMP_DIR_CYD=%TEMP%\esp32cyd_build
set ESP32_PACKAGE=esp32_shop_firmware
set CYD_PACKAGE=esp32_cyd_audio_firmware

:: Create or clear temp directories
if exist "%TEMP_DIR_ESP32%" rmdir /s /q "%TEMP_DIR_ESP32%"
if exist "%TEMP_DIR_CYD%" rmdir /s /q "%TEMP_DIR_CYD%"
mkdir "%TEMP_DIR_ESP32%"
mkdir "%TEMP_DIR_CYD%"

echo Building ESP32 Standard Firmware...
:: Build ESP32 standard firmware
cd esp32
call platformio run
if %ERRORLEVEL% neq 0 (
    echo Error building ESP32 standard firmware!
    exit /b %ERRORLEVEL%
)
copy .pio\build\esp32dev\firmware.bin "%TEMP_DIR_ESP32%\"
cd ..

echo Building ESP32-CYD Audio Firmware...
:: Build ESP32-CYD firmware with audio
cd esp32-cyd
call platformio run
if %ERRORLEVEL% neq 0 (
    echo Error building ESP32-CYD firmware!
    exit /b %ERRORLEVEL%
)
copy .pio\build\esp32-cyd\firmware.bin "%TEMP_DIR_CYD%\"
cd ..

echo Packaging ESP32 Standard Firmware...
:: Copy all necessary files for ESP32
mkdir "%TEMP_DIR_ESP32%\include"
mkdir "%TEMP_DIR_ESP32%\lib"
mkdir "%TEMP_DIR_ESP32%\src"
xcopy /E /Y "include" "%TEMP_DIR_ESP32%\include\"
xcopy /E /Y "lib" "%TEMP_DIR_ESP32%\lib\"
xcopy /E /Y "esp32\src" "%TEMP_DIR_ESP32%\src\"
copy "esp32\platformio.ini" "%TEMP_DIR_ESP32%\"
copy "README.md" "%TEMP_DIR_ESP32%\"

echo Packaging ESP32-CYD Audio Firmware...
:: Copy all necessary files for ESP32-CYD
mkdir "%TEMP_DIR_CYD%\include"
mkdir "%TEMP_DIR_CYD%\lib"
mkdir "%TEMP_DIR_CYD%\src"
xcopy /E /Y "include" "%TEMP_DIR_CYD%\include\"
xcopy /E /Y "lib" "%TEMP_DIR_CYD%\lib\"
xcopy /E /Y "esp32-cyd\src" "%TEMP_DIR_CYD%\src\"
copy "esp32-cyd\platformio.ini" "%TEMP_DIR_CYD%\"
copy "README.md" "%TEMP_DIR_CYD%\"

:: Create zip files (using PowerShell for compression)
echo Creating ZIP packages...
powershell -Command "Compress-Archive -Path '%TEMP_DIR_ESP32%\*' -DestinationPath '%ESP32_PACKAGE%.zip' -Force"
powershell -Command "Compress-Archive -Path '%TEMP_DIR_CYD%\*' -DestinationPath '%CYD_PACKAGE%.zip' -Force"

:: Clean up
rmdir /s /q "%TEMP_DIR_ESP32%"
rmdir /s /q "%TEMP_DIR_CYD%"

echo Packages created:
echo - %ESP32_PACKAGE%.zip (Standard ESP32)
echo - %CYD_PACKAGE%.zip (ESP32-CYD with Audio)