#pragma once
// ============================================================================
//  Samogon — Конфигурація проекту
//  ESP8266 Serial ↔ MQTT Bridge з веб-інтерфейсом налаштувань
// ============================================================================

// ─── Функціональні модулі ───────────────────────────────────────────────────
#define ENABLE_WIFI            // WiFi (STA + AP)
#define ENABLE_WEB_SERVER      // Веб-сервер налаштувань
#define ENABLE_OTA             // OTA оновлення прошивки через MQTT
#define ENABLE_MQTT            // MQTT клієнт
#define ENABLE_SERIAL_BRIDGE   // Serial ↔ MQTT міст

// ─── Налаштування WiFi за замовчуванням ─────────────────────────────────────
#define DEFAULT_WIFI_SSID      ""               // SSID за замовчуванням (порожній = AP режим)
#define DEFAULT_WIFI_PASS      ""               // Пароль за замовчуванням
#define AP_SSID                "Samogon_Setup"  // Назва точки доступу
#define AP_PASS                "12345678"        // Пароль точки доступу
#define WIFI_CONNECT_TIMEOUT   10000             // Таймаут підключення (мс)
#define WIFI_RECONNECT_INTERVAL 20000            // Інтервал перепідключення (мс)

// ─── Ідентифікація пристрою (для побудови MQTT топіків) ──────────────────────
#define DEVICE_VERSION         "1"               // Версія протоколу
#define DEVICE_TYPE            "sam"             // Тип пристрою
#define DEFAULT_USER_TOKEN     ""                // Номер телефону (380991234567)

// ─── Налаштування MQTT за замовчуванням ─────────────────────────────────────
#define DEFAULT_MQTT_SERVER    ""                // MQTT сервер
#define DEFAULT_MQTT_PORT      1883              // MQTT порт
#define DEFAULT_MQTT_USER      ""                // MQTT користувач
#define DEFAULT_MQTT_PASS      ""                // MQTT пароль
#define DEFAULT_MQTT_CLIENT_ID "sam"             // MQTT Client ID
// Топіки будуються автоматично: {USER_TOKEN}/{VERSION}/{DEVICE_TYPE}/data|cmd
// Якщо USER_TOKEN порожній — використовуються значення нижче
#define DEFAULT_MQTT_PUB_TOPIC "sam/data"        // Топік публікації (fallback)
#define DEFAULT_MQTT_SUB_TOPIC "sam/cmd"         // Топік підписки (fallback)
#define MQTT_RECONNECT_INTERVAL 5000             // Інтервал перепідключення MQTT (мс)

// ─── Налаштування Serial ────────────────────────────────────────────────────
#define SERIAL_BAUD            115200            // Швидкість UART (збігається з Arduino HomeSamogon_ua1)
#define SERIAL_BUFFER_SIZE     256               // Розмір буфера прийому
#define SERIAL_DELIMITER       '|'               // Роздільник ключ=значення пар
#define SERIAL_KV_SEPARATOR    '='               // Роздільник ключ від значення
#define MAX_SERIAL_KEYS        16                // Максимальна кількість пар ключ=значення

// ─── EEPROM ─────────────────────────────────────────────────────────────────
#define EEPROM_SIZE            512
#define SETTINGS_SAVED_FLAG    0xAB              // Маркер збережених налаштувань

// Розміри полів (без null-термінатора)
#define LEN_WIFI_SSID          32                // SSID WiFi (макс 32 за стандартом)
#define LEN_WIFI_PASS          63                // Пароль WiFi (макс 63 за WPA2)
#define LEN_MQTT_SERVER        40                // MQTT сервер
#define LEN_MQTT_USER          32                // MQTT користувач
#define LEN_MQTT_PASS          32                // MQTT пароль
#define LEN_MQTT_TOPIC         64                // MQTT топік
#define LEN_MQTT_CLIENT_ID     32                // MQTT Client ID
#define LEN_USER_TOKEN         20                // Номер телефону
#define LEN_DEVICE_TYPE        16                // Тип пристрою
#define LEN_DEVICE_VERSION     8                 // Версія протоколу

// Адреси EEPROM (кожне поле = розмір + 1 байт null-термінатор)
#define ADDR_SAVED_FLAG        0    // 1 байт  — маркер збереження
#define ADDR_WIFI_SSID         1    // 33 байти (32 + null)
#define ADDR_WIFI_PASS         34   // 64 байти (63 + null) ← WPA2 макс 63 символи
#define ADDR_MQTT_SERVER       98   // 41 байт  (40 + null)
#define ADDR_MQTT_PORT         139  // 2 байти  (uint16_t)
#define ADDR_MQTT_USER         141  // 33 байти (32 + null)
#define ADDR_MQTT_PASS         174  // 33 байти (32 + null)
#define ADDR_MQTT_PUB_TOPIC    207  // 65 байт  (64 + null)
#define ADDR_MQTT_SUB_TOPIC    272  // 65 байт  (64 + null)
#define ADDR_MQTT_CLIENT_ID    337  // 33 байти (32 + null)
#define ADDR_USER_TOKEN        370  // 21 байт  (20 + null) — номер телефону
#define ADDR_DEVICE_TYPE       391  // 17 байт  (16 + null) — тип пристрою
#define ADDR_DEVICE_VERSION    408  // 9 байт   (8 + null)  — версія протоколу
// 417–511 — резерв (95 байт)

// ─── MQTT OTA ───────────────────────────────────────────────────────────────
#define MQTT_OTA_CHUNK_SIZE    4096              // Максимальний розмір чанка прошивки (байт)
#define MQTT_OTA_TIMEOUT       30000             // Таймаут очікування чанка (мс)

// ─── Інше ───────────────────────────────────────────────────────────────────
#define ON  true
#define OFF false

