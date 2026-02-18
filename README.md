# MQTT Bridge — ESP8266 Serial ↔ MQTT Gateway

Мост Serial ↔ MQTT на базі ESP8266 з веб-інтерфейсом налаштувань.

## Можливості

- **WiFi**: підключення до роутера (STA) + завжди активна точка доступу (AP) для налаштування
- **Веб-інтерфейс**: головна сторінка зі статусом, сторінка налаштувань WiFi/MQTT, сторінка ручної відправки
- **Serial → MQTT**: прийом даних з UART, парсинг `key=value` пар, публікація кожного значення в окремий MQTT топік
- **MQTT → Serial**: підписка на командний топік, пересилка отриманих повідомлень в Serial
- **OTA**: оновлення прошивки по мережі (ArduinoOTA)
- **EEPROM**: збереження всіх налаштувань

## Формат Serial

**Прийом (Serial → MQTT):**
```
key1=value1|key2=value2|key3=value3\r\n
```
Кожна пара публікується в `{pub_topic}/{key}` з payload `{value}`.  
Також весь рядок публікується в `{pub_topic}/raw`.

**Відправка (MQTT → Serial):**  
Повідомлення з топіка `{sub_topic}/power` з payload `500` → відправляється `power=500\r\n` в Serial.

## Встановлення

### 1. Встановити бібліотеки

В Arduino IDE: **Sketch → Include Library → Manage Libraries...**

| Бібліотека | Автор |
|---|---|
| **PubSubClient** | Nick O'Leary |
| **ESP Async WebServer** | — (або з libraries/ оригінального проекту) |
| **ESPAsyncTCP** | — (або з libraries/ оригінального проекту) |

> Бібліотеки **ESP Async WebServer** та **ESPAsyncTCP** можна скопіювати з папки `libraries/` оригінального проекту в папку `Arduino/libraries/` вашого комп'ютера.

### 2. Встановити Board Package

В Arduino IDE: **File → Preferences → Additional Board Manager URLs:**
```
http://arduino.esp8266.com/stable/package_esp8266com_index.json
```
Потім: **Tools → Board → Boards Manager** → встановити **esp8266**

### 3. Компіляція

1. Відкрити `mqtt_bridge.ino` в Arduino IDE
2. **Tools → Board:** Generic ESP8266 Module (або NodeMCU, Wemos D1 mini тощо)
3. **Tools → Flash Size:** 4MB (FS:2MB, OTA:~1019KB) або відповідно до вашої плати
4. Upload

## Веб-інтерфейс

| Сторінка | URL |
|---|---|
| Головна (статус) | `http://192.168.4.1/` (AP) або `http://<IP>/` (STA) |
| Налаштування | `http://<IP>/settings` |
| Відправка команд | `http://<IP>/send` |

## Налаштування за замовчуванням

| Параметр | Значення |
|---|---|
| AP SSID | `MQTT_Bridge` |
| AP Пароль | `12345678` |
| AP IP | `192.168.4.1` |
| Serial | 9600 бод |
| MQTT порт | 1883 |
| OTA пароль | `admin` |

## Структура файлів

```
mqtt_bridge/
├── mqtt_bridge.ino    — Головний файл, setup/loop, EEPROM
├── config.h           — Конфігурація, адреси EEPROM, дефайни
├── wifi_utils.ino     — WiFi STA+AP, підключення, реконнект
├── web_server.ino     — Веб-сервер, маршрути API
├── web_pages.h        — HTML сторінки (PROGMEM)
├── mqtt_client.ino    — MQTT клієнт, публікація, підписка
├── serial_comm.ino    — Serial прийом, парсинг, відправка
├── OTA.ino            — OTA оновлення
└── install_libraries.bat — Скрипт встановлення бібліотек
```

## API endpoints

| Endpoint | Метод | Опис |
|---|---|---|
| `/get_status` | GET | JSON зі статусом WiFi, MQTT, Serial |
| `/save_wifi` | POST | Збереження WiFi налаштувань |
| `/save_mqtt` | POST | Збереження MQTT налаштувань |
| `/scan_wifi` | GET | Сканування доступних WiFi мереж |
| `/api/serial_send?cmd=...` | GET | Відправити команду в Serial |
| `/api/mqtt_pub?topic=...&payload=...` | GET | Опублікувати в MQTT |
| `/restart` | GET | Перезавантаження |
| `/factory_reset` | GET | Скидання всіх налаштувань |

## Карта EEPROM (512 байт)

| Адреса | Розмір | Поле |
|---|---|---|
| 0 | 1 | Маркер збережених налаштувань (0xAB) |
| 1 | 33 | WiFi SSID |
| 34 | 33 | WiFi Password |
| 67 | 41 | MQTT Server |
| 108 | 2 | MQTT Port |
| 110 | 33 | MQTT User |
| 143 | 33 | MQTT Password |
| 176 | 65 | MQTT Publish Topic |
| 241 | 65 | MQTT Subscribe Topic |
| 306 | 33 | MQTT Client ID |
| 339–511 | — | Резерв |

