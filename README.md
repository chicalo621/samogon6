# 🥃 Samogon6 / Самогон6

<div align="center">

![Platform](https://img.shields.io/badge/platform-ESP8266-0A7EA4?style=for-the-badge&logo=espressif)
![Language](https://img.shields.io/badge/language-Arduino%20%2F%20C%2B%2B-00979D?style=for-the-badge&logo=arduino)
![Protocol](https://img.shields.io/badge/protocol-MQTT-660066?style=for-the-badge&logo=eclipsemosquitto)
![Web UI](https://img.shields.io/badge/web-ui%20built--in-2ea44f?style=for-the-badge&logo=googlechrome)
![OTA](https://img.shields.io/badge/update-OTA-orange?style=for-the-badge)
![Status](https://img.shields.io/badge/status-in%20active%20development-success?style=for-the-badge)

**ESP8266-based automation gateway for a home distillation / rectification setup**  
**ESP8266-шлюз для автоматизації самогонного апарата, ректифікації та віддаленого керування**

</div>

---

## 🇺🇦 Українською

### Що це за проєкт

**Samogon6** — це прошивка для **ESP8266**, яка працює як мережевий шлюз і панель керування для автоматики самогонного апарата.

Проєкт з'єднує:
- **Arduino / контролер автоматики** по **Serial**,
- **MQTT брокер** для віддаленого моніторингу та команд,
- **вбудований веб-інтерфейс** для налаштування та локального керування,
- **OTA-оновлення** через web або MQTT.

По суті, це міст між "залізом" дистиляції та сучасним мережевим керуванням.

---

### Основні можливості

- 📶 **Wi‑Fi режими:**
  - підключення до роутера (**STA**)
  - власна точка доступу (**AP**) для первинного налаштування
- 🌐 **Вбудований web UI**:
  - сторінка статусу
  - сторінка налаштувань Wi‑Fi / MQTT / system
  - сторінка ручної відправки команд
  - сторінка OTA-оновлення
- 📡 **MQTT інтеграція**:
  - публікація телеметрії
  - прийом команд керування
  - автоматичне формування топіків за `userToken / version / deviceType`
- 🔌 **Serial ↔ MQTT шлюз** між ESP8266 та контролером автоматики
- 💾 **Збереження налаштувань у EEPROM**
- 🔄 **OTA оновлення**:
  - через браузер
  - через MQTT чанками
- 🧠 **Основи автоматизації процесу дистиляції / ректифікації**
- 🧪 окремі заготовки/модулі для **ректифікації, БЗК та НБК**

---

### Для чого саме цей проєкт

Цей репозиторій орієнтований саме на **автоматизацію самогонного апарата**, а не просто на абстрактний MQTT gateway.

З коду видно, що система вміє:
- приймати дані від автоматики типу **HomeSamogon**;
- відображати температури, стани, тиск, ШІМ та службові параметри;
- передавати команди на керування:
  - водою,
  - ТЕНом,
  - клапаном / ШІМ,
  - аварійною температурою,
  - режимами авто/ручного керування,
  - параметрами старт/стоп,
  - лімітами відбору та продуктивності;
- готувати основу для режимів **ректифікації**, **БЗК** та **НБК**.

---

### Архітектура

```text
[Контролер автоматики / Arduino]
            ⇅ Serial
          [ESP8266]
      ⇅ Wi‑Fi / Web / MQTT
[Веб-інтерфейс]   [MQTT брокер]   [віддалений клієнт]
```

ESP8266 тут виконує роль:
- мережевого шлюзу,
- локального web-сервера,
- точки інтеграції з MQTT,
- OTA-вузла для оновлення прошивки.

---

### Web-інтерфейс

Вбудований інтерфейс надає:

- **`/`** — головна сторінка статусу
- **`/settings`** — налаштування Wi‑Fi, MQTT, system
- **`/send`** — ручна відправка Serial / MQTT команд
- **`/update`** — OTA оновлення прошивки
- **`/automation`** — сторінка/заготовка для автоматизації

API endpoints:

- **`/get_status`** — JSON статус пристрою
- **`/save_wifi`** — збереження Wi‑Fi налаштувань
- **`/save_mqtt`** — збереження MQTT налаштувань
- **`/scan_wifi`** — сканування Wi‑Fi мереж
- **`/api/serial_send?cmd=...`** — надсилання Serial команди
- **`/api/mqtt_pub?topic=...&payload=...`** — MQTT publish
- **`/restart`** — перезапуск ESP
- **`/factory_reset`** — скидання налаштувань
- **`/api/ota_upload`** — web OTA upload

---

### Приклад MQTT топіків

Якщо задано:
- `userToken = 380991234567`
- `deviceVersion = 1`
- `deviceType = sam`

тоді формуються топіки:

- publish: `380991234567/1/sam/data`
- subscribe: `380991234567/1/sam/cmd`

Телеметрія публікується у підтемах типу:
- `.../data/<key>`
- `.../data/raw`
- `.../data/status`
- `.../data/ota/status`
- `.../data/ota/progress`

Команди приймаються через:
- `.../cmd/<key>`
- а також OTA-команди через `.../cmd/ota/...`

---

### Serial протокол та керування апаратом

ESP приймає пакети від Arduino/автоматики та розбирає їх у структуровані поля.

Підтримуються команди керування, зокрема:
- `water` — керування водою
- `shim` — значення ШІМ клапана
- `PUBalarmLimit` — температура сигналізації
- `autoEnd`, `autoStart`, `autoMode` — авто режим
- `start`, `stop` — температури старт/стоп
- `display` — центральне поле дисплея
- `Periodkl` — період клапана
- `pwmFinish` — завершення по ШІМ
- `cubeFinish` — завершення по температурі куба
- `tenControl` — керування ТЕН
- `targetVolume` — цільовий об'єм відбору
- `maxFlowMlH` — максимальна продуктивність
- `raw` — raw команда без трансляції

Це дає змогу будувати як простий моніторинг, так і повноцінне віддалене керування процесом.

---

### Режими автоматизації

У репозиторії є окремі модулі, пов'язані з автоматизацією дистиляції:

- **`distillation_modes.ino`** — логіка режимів ректифікації та БЗК
- **`nbk.ino`** — адаптований модуль режиму НБК

Ці модулі виглядають як напрямок розвитку проєкту в сторону:
- автоматичного відбору по режимах,
- стабілізації,
- обмежень по температурі / тиску,
- напівавтоматичної або автоматичної роботи колони.

---

### Структура проєкту

```text
samogon6/
├── samogon.ino            # головний файл, setup/loop, EEPROM, ініціалізація
├── config.h               # основні константи та конфігурація
├── wifi_utils.ino         # Wi‑Fi STA/AP, reconnect, hotspot logic
├── web_server.ino         # маршрути веб-сервера та API
├── web_pages.h            # HTML сторінки, вбудовані у PROGMEM
├── mqtt_client.ino        # MQTT логіка, subscribe/publish, callback
├── serial_comm.ino        # міст між Serial та командами автоматики
├── OTA.ino                # OTA через MQTT
├── distillation_modes.ino # режими ректифікації / БЗК
├── nbk.ino                # заготовка/ядро НБК
├── install_libraries.bat  # допоміжний скрипт встановлення бібліотек
└── libraries/             # локальні бібліотеки проєкту
```

---

### Швидкий старт

#### 1. Встановити Arduino IDE та ESP8266 board package

Додайте в **Additional Board Manager URLs**:

```text
https://arduino.esp8266.com/stable/package_esp8266com_index.json
```

Потім у **Boards Manager** встановіть пакет **esp8266**.

#### 2. Встановити бібліотеки

Потрібні бібліотеки:
- **PubSubClient**
- **ESPAsyncWebServer**
- **ESPAsyncTCP**

У репозиторії є файл:
- `install_libraries.bat`

який підказує порядок встановлення.

#### 3. Відкрити проєкт

В Arduino IDE відкрийте:

```text
samogon.ino
```

#### 4. Обрати плату

Підійде одна з ESP8266-плат, наприклад:
- **NodeMCU 1.0 (ESP-12E Module)**
- **Wemos D1 mini**
- або **Generic ESP8266 Module**

#### 5. Прошити ESP8266

Після прошивки:
- якщо Wi‑Fi не налаштований — пристрій підніме **AP**;
- якщо Wi‑Fi збережений — підключиться до роутера і дасть доступ до web UI.

---

### Налаштування за замовчуванням

#### Wi‑Fi
- AP SSID: `Samogon_Setup_<ChipID>`
- AP password: `12345678`
- якщо `savedSSID` порожній — старт у AP режимі

#### MQTT
- server: `vmi516392.contaboserver.net`
- port: `9001`
- user: `samovar`
- fallback pub topic: `sam/data`
- fallback sub topic: `sam/cmd`

> **Увага:** перед реальним використанням рекомендовано змінити мережеві та MQTT параметри під свою інфраструктуру.

---

### OTA оновлення

#### Через web
Відкрий:

```text
http://<device-ip>/update
```

та завантаж `.bin` файл прошивки.

#### Через MQTT
Використовуються топіки:
- `{sub_topic}/ota/begin`
- `{sub_topic}/ota/data`
- `{sub_topic}/ota/end`
- `{sub_topic}/ota/abort`

Статус і прогрес:
- `{pub_topic}/ota/status`
- `{pub_topic}/ota/progress`

---

### Стан проєкту

Проєкт уже має робочі частини для:
- web UI,
- Wi‑Fi конфігурації,
- MQTT інтеграції,
- Serial bridge,
- OTA,
- базової автоматизації.

Також видно активний розвиток у бік **спеціалізованої автоматики для дистиляції / ректифікації**.

---

### Для кого цей репозиторій

Проєкт може бути корисний, якщо ти хочеш:
- підключити автоматику самогонного апарата до Wi‑Fi;
- моніторити параметри через MQTT;
- керувати апаратом з телефона або web-інтерфейсу;
- інтегрувати систему з Home Assistant, Node-RED або власним мобільним застосунком;
- побудувати більш просунуту автоматику ректифікації / НБК.

---

## 🇬🇧 English

### What this project is

**Samogon6** is an **ESP8266 firmware** that acts as a network gateway and control panel for a **home distillation / rectification setup**.

It connects:
- an **Arduino / automation controller** over **Serial**,
- an **MQTT broker** for telemetry and remote commands,
- a built-in **web interface** for local configuration,
- **OTA updates** via web or MQTT.

In practice, it is a bridge between the distillation hardware and modern network-based control.

---

### Key features

- 📶 **Wi‑Fi modes**:
  - STA client mode
  - AP hotspot mode for setup
- 🌐 **Built-in web UI**:
  - status page
  - Wi‑Fi / MQTT / system settings
  - manual command page
  - OTA update page
- 📡 **MQTT integration**:
  - telemetry publishing
  - remote command receiving
  - automatic topic generation from `userToken / version / deviceType`
- 🔌 **Serial ↔ MQTT bridge** between ESP8266 and automation controller
- 💾 **EEPROM-based settings storage**
- 🔄 **OTA updates**:
  - web upload
  - MQTT chunked firmware transfer
- 🧠 building blocks for **distillation / rectification automation**
- 🧪 separate modules for **rectification modes, BZK, and NBK**

---

### Why this repository is special

This is not just a generic MQTT bridge.

The repository is clearly focused on **moonshine still / distillation apparatus automation**. From the codebase, the system is designed to:
- parse **HomeSamogon-like** telemetry packets,
- expose temperatures, pressure, PWM, status flags and service values,
- send commands for:
  - water control,
  - heater (TEN) control,
  - valve / PWM control,
  - alarm temperature,
  - automatic/manual mode,
  - start/stop parameters,
  - volume / flow limits,
- evolve toward advanced rectification and process control.

---

### Project architecture

```text
[Automation controller / Arduino]
              ⇅ Serial
            [ESP8266]
       ⇅ Wi‑Fi / Web / MQTT
[Web UI]     [MQTT broker]     [Remote client]
```

ESP8266 acts as:
- a network gateway,
- a local web server,
- an MQTT integration layer,
- an OTA update node.

---

### Web interface

Built-in routes include:

- **`/`** — status dashboard
- **`/settings`** — Wi‑Fi, MQTT and system settings
- **`/send`** — manual Serial / MQTT command sending
- **`/update`** — OTA update page
- **`/automation`** — automation UI prototype page

API endpoints:

- **`/get_status`** — device JSON status
- **`/save_wifi`** — save Wi‑Fi settings
- **`/save_mqtt`** — save MQTT settings
- **`/scan_wifi`** — scan Wi‑Fi networks
- **`/api/serial_send?cmd=...`** — send Serial command
- **`/api/mqtt_pub?topic=...&payload=...`** — publish to MQTT
- **`/restart`** — reboot ESP
- **`/factory_reset`** — reset saved settings
- **`/api/ota_upload`** — web OTA firmware upload

---

### Example MQTT topics

If:
- `userToken = 380991234567`
- `deviceVersion = 1`
- `deviceType = sam`

then the generated topics are:

- publish: `380991234567/1/sam/data`
- subscribe: `380991234567/1/sam/cmd`

Telemetry can be published under:
- `.../data/<key>`
- `.../data/raw`
- `.../data/status`
- `.../data/ota/status`
- `.../data/ota/progress`

Commands are received through:
- `.../cmd/<key>`
- OTA topics under `.../cmd/ota/...`

---

### Serial protocol and device control

ESP parses incoming automation packets and maps them into named fields.

Supported command keys include:
- `water`
- `shim`
- `PUBalarmLimit`
- `autoEnd`
- `autoStart`
- `autoMode`
- `start`
- `stop`
- `display`
- `Periodkl`
- `pwmFinish`
- `cubeFinish`
- `tenControl`
- `targetVolume`
- `maxFlowMlH`
- `raw`

This makes the firmware suitable for both monitoring and remote process control.

---

### Automation modes

The repository already contains modules related to distillation automation:

- **`distillation_modes.ino`** — rectification and BZK logic
- **`nbk.ino`** — adapted NBK mode core/module

These indicate a roadmap toward:
- automatic takeoff modes,
- stabilization logic,
- pressure/temperature safety handling,
- semi-automatic or fully automatic column operation.

---

### Project structure

```text
samogon6/
├── samogon.ino
├── config.h
├── wifi_utils.ino
├── web_server.ino
├── web_pages.h
├── mqtt_client.ino
├── serial_comm.ino
├── OTA.ino
├── distillation_modes.ino
├── nbk.ino
├── install_libraries.bat
└── libraries/
```

---

### Quick start

#### 1. Install Arduino IDE and ESP8266 board package

Add to **Additional Board Manager URLs**:

```text
https://arduino.esp8266.com/stable/package_esp8266com_index.json
```

Then install the **esp8266** board package.

#### 2. Install dependencies

Required libraries:
- **PubSubClient**
- **ESPAsyncWebServer**
- **ESPAsyncTCP**

See:
- `install_libraries.bat`

#### 3. Open the project

Open:

```text
samogon.ino
```

#### 4. Select board

Typical boards:
- **NodeMCU 1.0 (ESP-12E Module)**
- **Wemos D1 mini**
- **Generic ESP8266 Module**

#### 5. Flash the firmware

After upload:
- if Wi‑Fi is not configured, the device starts in **AP mode**;
- if Wi‑Fi is already configured, it connects to the router and serves the web UI.

---

### Default settings

#### Wi‑Fi
- AP SSID: `Samogon_Setup_<ChipID>`
- AP password: `12345678`
- empty saved SSID means AP startup mode

#### MQTT
- server: `vmi516392.contaboserver.net`
- port: `9001`
- user: `samovar`
- fallback pub topic: `sam/data`
- fallback sub topic: `sam/cmd`

> **Warning:** before real-world usage, update Wi‑Fi and MQTT settings for your own infrastructure.

---

### OTA updates

#### Via web
Open:

```text
http://<device-ip>/update
```

and upload the compiled `.bin` firmware.

#### Via MQTT
Topics used:
- `{sub_topic}/ota/begin`
- `{sub_topic}/ota/data`
- `{sub_topic}/ota/end`
- `{sub_topic}/ota/abort`

Progress/status:
- `{pub_topic}/ota/status`
- `{pub_topic}/ota/progress`

---

### Repository status

The project already contains working parts for:
- web UI,
- Wi‑Fi setup,
- MQTT integration,
- Serial bridge,
- OTA,
- early automation logic.

The codebase also clearly points toward deeper **distillation / rectification automation**.

---

### Who this is for

This repository may be useful if you want to:
- connect a distillation controller to Wi‑Fi,
- monitor process values through MQTT,
- control the setup from a phone or browser,
- integrate with Home Assistant, Node-RED, or a custom mobile app,
- build more advanced rectification / NBK automation.

---

## ⚠️ Safety note / Зауваження щодо безпеки

**UA:** Автоматизація нагріву, клапанів, води та процесів дистиляції вимагає уважного тестування. Використовуйте систему тільки з розумінням ризиків, із захистом від перегріву, протікання та аварійних станів.  
**EN:** Automating heaters, valves, cooling water and distillation processes requires careful testing. Use this project only with a full understanding of the risks and with proper thermal, electrical and fail-safe protection.

---

## 👤 Author

- GitHub: **[@chicalo621](https://github.com/chicalo621)**
- Repository: **`chicalo621/samogon6`**

If you want, I can also prepare a next iteration with:
- screenshots section,
- wiring/pinout section,
- Home Assistant / Node-RED examples,
- a cleaner badges row and a table of supported commands.
