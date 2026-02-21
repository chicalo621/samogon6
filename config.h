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
#define AP_SSID                "MQTT_Bridge"    // Назва точки доступу
#define AP_PASS                "12345678"        // Пароль точки доступу
#define WIFI_CONNECT_TIMEOUT   10000             // Таймаут підключення (мс)
#define WIFI_RECONNECT_INTERVAL 20000            // Інтервал перепідключення (мс)

// ─── Налаштування MQTT за замовчуванням ─────────────────────────────────────
#define DEFAULT_MQTT_SERVER    ""                // MQTT сервер
#define DEFAULT_MQTT_PORT      1883              // MQTT порт
#define DEFAULT_MQTT_USER      ""                // MQTT користувач
#define DEFAULT_MQTT_PASS      ""                // MQTT пароль
#define DEFAULT_MQTT_CLIENT_ID "mqtt_bridge"     // MQTT Client ID
#define DEFAULT_MQTT_PUB_TOPIC "bridge/data"     // Топік для публікації даних з Serial
#define DEFAULT_MQTT_SUB_TOPIC "bridge/cmd"      // Топік для підписки (команди → Serial)
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

// Адреси EEPROM
#define ADDR_SAVED_FLAG        0    // 1 байт  — маркер збереження
#define ADDR_WIFI_SSID         1    // 33 байти (32 + null)
#define ADDR_WIFI_PASS         34   // 33 байти (32 + null)
#define ADDR_MQTT_SERVER       67   // 41 байт  (40 + null)
#define ADDR_MQTT_PORT         108  // 2 байти  (uint16_t)
#define ADDR_MQTT_USER         110  // 33 байти (32 + null)
#define ADDR_MQTT_PASS         143  // 33 байти (32 + null)
#define ADDR_MQTT_PUB_TOPIC    176  // 65 байт  (64 + null)
#define ADDR_MQTT_SUB_TOPIC    241  // 65 байт  (64 + null)
#define ADDR_MQTT_CLIENT_ID    306  // 33 байти (32 + null)
// 339–511 — резерв

// ─── MQTT OTA ───────────────────────────────────────────────────────────────
#define MQTT_OTA_CHUNK_SIZE    4096              // Максимальний розмір чанка прошивки (байт)
#define MQTT_OTA_TIMEOUT       30000             // Таймаут очікування чанка (мс)

// ─── Інше ───────────────────────────────────────────────────────────────────
#define ON  true
#define OFF false

