@echo off
echo ============================================
echo  MQTT Bridge - Встановлення бібліотек
echo ============================================
echo.
echo Встановлюю бібліотеки через Arduino CLI...
echo.

where arduino-cli >nul 2>nul
if %ERRORLEVEL% NEQ 0 (
    echo [УВАГА] arduino-cli не знайдено в PATH.
    echo.
    echo Встановіть бібліотеки вручну через Arduino IDE:
    echo   Sketch ^> Include Library ^> Manage Libraries...
    echo.
    echo Необхідні бібліотеки:
    echo   1. PubSubClient ^(by Nick O'Leary^)
    echo   2. ESP Async WebServer ^(by ESP Async^)
    echo   3. ESPAsyncTCP ^(by ESP Async^)
    echo.
    echo Або скопіюйте папки з libraries/ оригінального проекту
    echo в папку Arduino/libraries/ вашого комп'ютера.
    echo.
    pause
    exit /b 1
)

arduino-cli lib install "PubSubClient"
arduino-cli lib install "ESP Async WebServer"
arduino-cli lib install "ESPAsyncTCP"

echo.
echo Також потрібен ESP8266 board package:
arduino-cli core install esp8266:esp8266 --additional-urls http://arduino.esp8266.com/stable/package_esp8266com_index.json

echo.
echo ============================================
echo  Готово! Відкрийте mqtt_bridge.ino в Arduino IDE
echo ============================================
pause

