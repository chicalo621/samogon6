@echo off
chcp 65001 >nul
echo ============================================
echo  MQTT Bridge - Встановлення бібліотек
echo ============================================
echo.
echo Цей скрипт допоможе встановити необхідні
echo бібліотеки для роботи проекту.
echo.

set ARDUINO_LIB=%USERPROFILE%\Documents\Arduino\libraries

echo Перевірка папки Arduino libraries...
if not exist "%ARDUINO_LIB%" (
    echo [ПОМИЛКА] Папку Arduino libraries не знайдено!
    echo Очікувана папка: %ARDUINO_LIB%
    echo.
    echo Встановіть Arduino IDE або створіть папку вручну.
    pause
    exit /b 1
)
echo [OK] Папка знайдена: %ARDUINO_LIB%
echo.

echo ============================================
echo Необхідні бібліотеки:
echo ============================================
echo.
echo 1. PubSubClient (через Library Manager)
echo 2. ESPAsyncWebServer (вручну з GitHub)
echo 3. ESPAsyncTCP (вручну з GitHub)
echo.
echo ІНСТРУКЦІЯ:
echo.
echo Крок 1: Встановіть PubSubClient
echo   - Відкрийте Arduino IDE
echo   - Sketch → Include Library → Manage Libraries
echo   - Знайдіть "PubSubClient" від Nick O'Leary
echo   - Натисніть Install
echo.
echo Крок 2-3: Завантажте вручну з GitHub:
echo   ESPAsyncWebServer:
echo   https://github.com/me-no-dev/ESPAsyncWebServer/archive/master.zip
echo.
echo   ESPAsyncTCP:
echo   https://github.com/me-no-dev/ESPAsyncTCP/archive/master.zip
echo.
echo Після завантаження:
echo   1. Розпакуйте архіви
echo   2. Перейменуйте папки:
echo      ESPAsyncWebServer-master → ESPAsyncWebServer
echo      ESPAsyncTCP-master → ESPAsyncTCP
echo   3. Скопіюйте папки в: %ARDUINO_LIB%
echo   4. Перезапустіть Arduino IDE
echo.
echo ESP8266 Board Package:
echo   File → Preferences → Additional Board Manager URLs:
echo   https://arduino.esp8266.com/stable/package_esp8266com_index.json
echo.
echo   Tools → Board → Boards Manager → встановіть "esp8266"
echo.
echo ============================================
pause

