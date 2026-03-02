# Samogon — ESP8266 Serial ↔ MQTT Gateway

Міст Serial ↔ MQTT на базі ESP8266 із сучасним веб-інтерфейсом, автооновленням IP, надійним керуванням WiFi-режимами та оновленням прошивки через MQTT.

---

## 📚 Документація

- **[ARDUINO_IDE_SETUP.md](ARDUINO_IDE_SETUP.md)** — покрокова інструкція для Arduino IDE
- **[EXAMPLES.md](EXAMPLES.md)** — приклади інтеграції з Home Assistant, Node-RED
- **[README.md](README.md)** — цей огляд проекту

---

## Можливості

- **WiFi:**  
  - Підключення до роутера (STA)  
  - Перехід у точку доступу (AP/hotspot) для налаштування
  - **Захист:** перехід в AP можливий лише якщо поле SSID порожнє (уникає втрати доступу)
  - **Автоматичне оновлення IP:** На сторінці налаштувань WiFi IP ESP (STA) оновлюється “на льоту” без перезавантаження сторінки
  - **Скан WiFi:** Кнопка “Сканувати” з автоповтором до 3 разів, Unicode-friendly, вибір зі списку з автоматичним обрізанням пробілів
- **Веб-інтерфейс:**  
  - Головна сторінка статусу: відображає підключення, MQTT, серійні дані, аптайм  
  - Сторінка налаштувань із вкладками WiFi/MQTT/System  
  - Сторінка відправки MQTT та Serial команд  
  - OTA-сторінка для оновлення прошивки через браузер
- **Serial → MQTT:**  
  - Прийом key=value даних із UART, автоматичний парсинг та публікація кожної пари у свій топік MQTT  
  - Весь рядок також публікується як raw
- **MQTT → Serial:**  
  - Підписка на топік, надходження команд і пересилка у Serial
- **OTA через MQTT:**  
  - Оновлення прошивки бінарними чанками через MQTT-брокер, статус та прогрес у MQTT
- **EEPROM:**  
  - Збереження всіх налаштувань WiFi & MQTT
- **Захист від помилок користувача:**  
  - Обрізка пробілів у SSID  
  - Якщо у WiFi обрано режим “hotspot (AP)” та SSID не очищено, режим не вмикається − це захист від блокування пристрою
- **Стабільний reconnect WiFi STA:**  
  - Періодичний перепідключення до роутера за таймером

---

## Формат Serial

**Прийом (Serial → MQTT):**
```
key1=value1|key2=value2|key3=value3\r\n
```
Кожна пара публікується в `{pub_topic}/{key}` з payload `{value}`.  
Весь рядок - у `{pub_topic}/raw`.

**Відправка (MQTT → Serial):**  
З топіка `{sub_topic}/power` із payload `500` відправляється `power=500\r\n` в Serial.

---

## OTA оновлення через MQTT

| Крок         | Топік                  | Payload                   |
|--------------|------------------------|---------------------------|
| 1. Початок   | `{sub_topic}/ota/begin`| розмір прошивки (текстом) |
| 2. Дані      | `{sub_topic}/ota/data` | бінарний чанк (≤4096 байт)|
| 3. Завершення| `{sub_topic}/ota/end`  | будь-який/порожній        |
| Скасування   | `{sub_topic}/ota/abort`| будь-який/порожній        |

**Статус оновлення:**
- `{pub_topic}/ota/status` — `started`, `success, rebooting...`, `error: ...`, `aborted`
- `{pub_topic}/ota/progress` — % завершеності (0–100)

**Приклад (Python):**
```python
import paho.mqtt.client as mqtt
import time

broker = "192.168.1.100"
sub_topic = "bridge/cmd"
firmware_path = "firmware.bin"
chunk_size = 4096

client = mqtt.Client()
client.connect(broker)

with open(firmware_path, "rb") as f:
    data = f.read()

client.publish(f"{sub_topic}/ota/begin", str(len(data)))
time.sleep(1)

for i in range(0, len(data), chunk_size):
    chunk = data[i:i+chunk_size]
    client.publish(f"{sub_topic}/ota/data", chunk)
    time.sleep(0.1)

time.sleep(1)
client.publish(f"{sub_topic}/ota/end", "done")
```

---

## Встановлення

### 1. Встановити бібліотеки

**Через Arduino Library Manager:**

- В меню Arduino IDE:  
  **Sketch → Include Library → Manage Libraries...**  
  → встановити **PubSubClient** від Nick O'Leary

**Для ESP Async Web Server вручну:**
1. Завантажити:
   - [ESPAsyncWebServer](https://github.com/me-no-dev/ESPAsyncWebServer/archive/master.zip)
   - [ESPAsyncTCP](https://github.com/me-no-dev/ESPAsyncTCP/archive/master.zip)
2. Розпакувати у `.../Arduino/libraries/`
3. Перейменувати папки:
   - `ESPAsyncWebServer-master` → `ESPAsyncWebServer`
   - `ESPAsyncTCP-master` → `ESPAsyncTCP`
4. Перезапустити Arduino IDE

### 2. Встановити Board Package

**Arduino IDE → File → Preferences → Additional Board Manager URLs:**
```
https://arduino.esp8266.com/stable/package_esp8266com_index.json
```
**Tools → Board → Boards Manager** → встановити **esp8266** (версія 3.0.0+)

### 3. Компіляція

1. Відкрити `samogon.ino` в Arduino IDE
2. Tools → Board: Generic ESP8266 Module (або NodeMCU/Wemos D1 mini)
3. Tools → Flash Size: 4MB (FS:2MB, OTA:~1019KB) або під свою плату
4. Upload

---

## Веб-інтерфейс

| Сторінка         | URL                                      |
|------------------|------------------------------------------|
| Головна (статус) | `http://192.168.4.1/` (AP) / `http://<IP>/` (STA) |
| Налаштування     | `http://<IP>/settings`                   |
| Відправка команд | `http://<IP>/send`                       |
| OTA оновлення    | `http://<IP>/update`                     |

---

## Налаштування за замовчуванням

| Параметр   | Значення         |
|------------|------------------|
| AP SSID    | `MQTT_Bridge`    |
| AP Пароль  | `12345678`       |
| AP IP      | `192.168.4.1`    |
| Serial     | 9600 бод         |
| MQTT порт  | 1883             |

---

## Структура проєкту

```
samogon/
├── samogon.ino        — Головний файл, setup/loop, EEPROM
├── config.h           — Конфігурація, адреси EEPROM, дефайни
├── wifi_utils.ino     — WiFi STA+AP, підключення, реконнект
├── web_server.ino     — Веб-сервер, всі API-маршрути
├── web_pages.h        — HTML сторінки (PROGMEM)
├── mqtt_client.ino    — MQTT клієнт
├── serial_comm.ino    — Serial прийом, парсинг, відправка
├── OTA.ino            — OTA через MQTT
└── install_libraries.bat — Автоматичне встановлення бібліотек
```

---

## API endpoints

| Endpoint                    | Метод | Опис                                 |
|-----------------------------|-------|---------------------------------------|
| `/get_status`               | GET   | JSON: статус WiFi, MQTT, Serial       |
| `/save_wifi`                | POST  | Зберегти налаштування WiFi           |
| `/save_mqtt`                | POST  | Зберегти MQTT налаштування           |
| `/scan_wifi`                | GET   | Сканування усіх доступних WiFi мереж |
| `/api/serial_send?cmd=...`  | GET   | Надіслати команду у Serial           |
| `/api/mqtt_pub?topic=...&payload=...`| GET| Опублікувати у MQTT                  |
| `/restart`                  | GET   | Перезапуск ESP                       |
| `/factory_reset`            | GET   | Скидання всіх налаштувань            |

---

## Карта EEPROM (512 байт)

| Адреса | Розмір | Поле               |
|--------|--------|--------------------|
| 0      | 1      | Маркер (0xAB)      |
| 1      | 33     | WiFi SSID          |
| 34     | 33     | WiFi Password      |
| 67     | 41     | MQTT Server        |
| 108    | 2      | MQTT Port          |
| 110    | 33     | MQTT User          |
| 143    | 33     | MQTT Password      |
| 176    | 65     | MQTT Publish Topic |
| 241    | 65     | MQTT Subscribe Top.|
| 306    | 33     | MQTT Client ID     |
| 339–511| -      | Резерв             |

---

## Основні зміни та новації 2026

- **Захист від неправильного налаштування WiFi:** Перехід у AP лише за порожнього SSID
- **Оновлення IP на сторінці налаштувань STA** (автоматично через JS)
- **Стабільна обрізка SSID** (завжди trim і при виборі, і при сабміті, і в бекенді)
- **Покращене сканування WiFi:** Повтор сканування до 3 разів для reliability
- **Веб-розділення сторінок:** Всі функції винесені у окремі вкладки
- **OTA через MQTT та Web** працюють паралельно
- **Автоматичний reconnect STA із інтервалом** не впливає на роботу точок доступу
- Всі JS-фрагменти у web_pages.h виправлені, дублікати та баги прибрані

---

## TODO

- Автоматичне вимкнення AP через 10 хв (roadmap)
- Графіки, автоматизація, розширена інтеграція з Home Assistant
- Захист вебінтерфейсу паролем

---

## Ліцензія та авторство

Автор: [chicalo621](https://github.com/chicalo621)  
Зворотній зв'язок та баги — через [issues](https://github.com/chicalo621/samogon6/issues)

---

**Всі актуальні можливості, логіка та захист відповідають вихідному коду на березень 2026 року.**
