@echo off
setlocal enabledelayedexpansion

echo ====================================================================
echo  ESP32 Auto Build and Upload Script for WS_3D_Footprint_Scanner_ESP32
echo ====================================================================
echo.

:: 1. Setup PATH for bundled arduino-cli
set "ARDUINO_CLI_PATH=C:\Users\lkdhr\AppData\Local\Programs\Arduino IDE\resources\app\lib\backend\resources"
set "PATH=%PATH%;%ARDUINO_CLI_PATH%"

:: 2. Verify arduino-cli
arduino-cli version >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] arduino-cli not found.
    echo Searched here: %ARDUINO_CLI_PATH%
    echo Is the Arduino IDE installed?
    pause
    exit /b 1
)
echo [OK] Found arduino-cli bundled in Arduino IDE!

:: 3. Initialise config if missing
if not exist "%USERPROFILE%\AppData\Local\Arduino15\arduino-cli.yaml" (
    echo [INFO] Initializing arduino-cli configuration...
    arduino-cli config init >nul 2>&1
    arduino-cli config add board_manager.additional_urls https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json >nul 2>&1
)

:: 4. Check/Install ESP32 Core
echo.
echo [1/3] Checking ESP32 Board support (Espressif core)...
arduino-cli core list | findstr "esp32:esp32" >nul
if %errorlevel% neq 0 (
    echo   [INFO] ESP32 core not found. Installing now - this might take a minute...
    arduino-cli core update-index
    arduino-cli core install esp32:esp32
    if !errorlevel! neq 0 (
        echo   [ERROR] Failed to install ESP32 core!
        pause
        exit /b 1
    )
    echo   [OK] ESP32 core installed successfully.
) else (
    echo   [OK] ESP32 core is already installed.
)

:: 5. Check/Install Libraries
echo.
echo [2/3] Checking required libraries...

call :CheckLib "WebSockets"
call :CheckLib "ArduinoJson"
goto :Compile

:CheckLib
arduino-cli lib list | find /i %1 >nul
if %errorlevel% neq 0 (
    echo   [INFO] Installing missing library: %~1
    arduino-cli lib install %1
) else (
    echo   [OK] Library %~1 is already installed.
)
exit /b

:: 6. Compile Code
:Compile
echo.
echo [3/3] Ready to compile!
arduino-cli compile --fqbn esp32:esp32:esp32 WS_3D_Footprint_Scanner_ESP32.ino
if %errorlevel% neq 0 (
    echo.
    echo [ERROR] Compilation failed! See logs above.
    pause
    exit /b 1
)
echo [OK] Compilation successful!

:: 7. Auto-Detect COM port and Upload
echo.
echo ====================================================================
echo Upload Phase
echo ====================================================================
echo Here are your current connected boards:
arduino-cli board list
echo.

:: Auto-detect the COM port for a USB Serial device
set "COMPORT="
for /f "tokens=1" %%i in ('arduino-cli board list ^| findstr /i "serial"') do (
    set "COMPORT=%%i"
)

if "!COMPORT!"=="" (
    echo [INFO] No COM port detected automatically.
    set /p COMPORT="Enter the COM port for ESP32 (e.g. COM6) OR leave blank to skip upload: "
) else (
    echo [INFO] Auto-detected ESP32 on port: !COMPORT!
    set /p CONFIRM="Press Enter to use !COMPORT!, or type a different port, or type 'skip' to skip upload: "
    if /I "!CONFIRM!"=="skip" (
        set "COMPORT="
    ) else if not "!CONFIRM!"=="" (
        set "COMPORT=!CONFIRM!"
    )
)

if "!COMPORT!"=="" (
    echo.
    echo [INFO] No COM port provided. Skipping upload...
) else (
    echo.
    echo Uploading to %COMPORT%...
    echo Remember: You may need to hold the BOOT button on the ESP32 while it connects
    arduino-cli upload --fqbn esp32:esp32:esp32 --port %COMPORT% WS_3D_Footprint_Scanner_ESP32.ino
    if !errorlevel! neq 0 (
        echo.
        echo [ERROR] Upload failed!
        pause
        exit /b 1
    )
    echo [OK] Upload successful!
)

echo.
echo All done! Press any key to exit.
pause
exit /b 0
