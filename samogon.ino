// ============================================================================
//  Samogon — Головний файл
//  ESP8266 Serial ↔ MQTT Bridge з веб-інтерфейсом налаштувань
// ============================================================================
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <EEPROM.h>
#include <ESPAsyncWebServer.h>
#include <PubSubClient.h>
#include "config.h"

// ─── Глобальні змінні WiFi ──────────────────────────────────────────────────
String savedSSID, savedPass;
bool hotspotMode = false;
int8_t wifiRSSI = 0;
String localIP = "";

// ─── Глобальні змінні MQTT ──────────────────────────────────────────────────
String mqttServer, mqttUser, mqttPass, mqttClientId;
String mqttPubTopic, mqttSubTopic;
String userToken;                    // Номер телефону (USER_TOKEN)
String deviceType;                   // Тип пристрою (редагується через веб)
String deviceVersion;                // Версія протоколу (редагується через веб)
uint16_t mqttPort = DEFAULT_MQTT_PORT;
bool mqttConnected = false;
volatile bool mqttNeedsReconnect = false;  // Прапорець відкладеного реконнекту
volatile bool settingsNeedSave = false;    // Прапорець відкладеного збереження

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

// ─── Веб-сервер ─────────────────────────────────────────────────────────────
AsyncWebServer server(80);

// ─── Serial Bridge ──────────────────────────────────────────────────────────
String serialKeys[MAX_SERIAL_KEYS];
String serialValues[MAX_SERIAL_KEYS];
uint8_t serialKeyCount = 0;
volatile bool newSerialData = false;
String lastRawSerial = "";

// ─── Допоміжні функції EEPROM ───────────────────────────────────────────────
void writeEEPROMString(int addr, String data, byte limit) {
  for (uint16_t i = 0; i < data.length() && i < limit; i++) {
    EEPROM.write(addr + i, data[i]);
  }
  EEPROM.write(addr + min((int)data.length(), (int)limit), 0);
}

String readEEPROMString(int addr, byte limit) {
  String data;
  char ch;
  int length = 0;
  while ((ch = EEPROM.read(addr++)) && length < limit) {
    data += ch;
    length++;
  }
  return data;
}

// ─── Побудова MQTT топіків з userToken ───────────────────────────────────────
// {USER_TOKEN}/{VERSION}/{DEVICE_TYPE}/data  та  .../cmd
void buildTopicsFromToken() {
  if (userToken.length() > 0 && deviceType.length() > 0) {
    String base = userToken + "/" + deviceVersion + "/" + deviceType;
    mqttPubTopic = base + "/data";
    mqttSubTopic = base + "/cmd";
  }
}

// ─── Завантаження всіх налаштувань з EEPROM ─────────────────────────────────
void loadSettings() {
  EEPROM.begin(EEPROM_SIZE);

  uint8_t flag = EEPROM.read(ADDR_SAVED_FLAG);

  if (flag == SETTINGS_SAVED_FLAG) {
    // WiFi
    savedSSID = readEEPROMString(ADDR_WIFI_SSID, 32);
    savedPass = readEEPROMString(ADDR_WIFI_PASS, 32);
    // MQTT
    mqttServer    = readEEPROMString(ADDR_MQTT_SERVER, 40);
    EEPROM.get(ADDR_MQTT_PORT, mqttPort);
    mqttUser      = readEEPROMString(ADDR_MQTT_USER, 32);
    mqttPass      = readEEPROMString(ADDR_MQTT_PASS, 32);
    mqttClientId  = readEEPROMString(ADDR_MQTT_CLIENT_ID, 32);
    userToken     = readEEPROMString(ADDR_USER_TOKEN, 20);
    deviceType    = readEEPROMString(ADDR_DEVICE_TYPE, 16);
    deviceVersion = readEEPROMString(ADDR_DEVICE_VERSION, 8);
    // Топіки: спочатку з EEPROM, потім перезаписуємо якщо є userToken
    mqttPubTopic  = readEEPROMString(ADDR_MQTT_PUB_TOPIC, 64);
    mqttSubTopic  = readEEPROMString(ADDR_MQTT_SUB_TOPIC, 64);
  } else {
    // Значення за замовчуванням
    savedSSID     = DEFAULT_WIFI_SSID;
    savedPass     = DEFAULT_WIFI_PASS;
    mqttServer    = DEFAULT_MQTT_SERVER;
    mqttPort      = DEFAULT_MQTT_PORT;
    mqttUser      = DEFAULT_MQTT_USER;
    mqttPass      = DEFAULT_MQTT_PASS;
    mqttPubTopic  = DEFAULT_MQTT_PUB_TOPIC;
    mqttSubTopic  = DEFAULT_MQTT_SUB_TOPIC;
    mqttClientId  = DEFAULT_MQTT_CLIENT_ID;
    userToken     = DEFAULT_USER_TOKEN;
    deviceType    = DEVICE_TYPE;
    deviceVersion = DEVICE_VERSION;
  }

  // Якщо є userToken — автоматично побудувати топіки
  buildTopicsFromToken();

  // Захист від порожніх значень
  if (mqttPubTopic.length() == 0) mqttPubTopic = DEFAULT_MQTT_PUB_TOPIC;
  if (mqttSubTopic.length() == 0) mqttSubTopic = DEFAULT_MQTT_SUB_TOPIC;
  if (mqttPort == 0) mqttPort = DEFAULT_MQTT_PORT;
  if (deviceType.length() == 0) deviceType = DEVICE_TYPE;
  if (deviceVersion.length() == 0) deviceVersion = DEVICE_VERSION;

  // Client ID: {deviceType}_{ChipID}
  mqttClientId = deviceType + "_" + String(ESP.getChipId(), HEX);

  EEPROM.end();

  Serial1.println("Налаштування завантажено.");
  Serial1.println("WiFi SSID: " + savedSSID);
  Serial1.println("MQTT Server: " + mqttServer + ":" + String(mqttPort));
  Serial1.println("User Token: " + userToken);
  Serial1.println("Pub Topic: " + mqttPubTopic);
  Serial1.println("Sub Topic: " + mqttSubTopic);
}

// ─── Збереження всіх налаштувань у EEPROM ───────────────────────────────────
void saveSettings() {
  EEPROM.begin(EEPROM_SIZE);

  EEPROM.write(ADDR_SAVED_FLAG, SETTINGS_SAVED_FLAG);
  // WiFi
  writeEEPROMString(ADDR_WIFI_SSID, savedSSID, 32);
  writeEEPROMString(ADDR_WIFI_PASS, savedPass, 32);
  // MQTT
  writeEEPROMString(ADDR_MQTT_SERVER, mqttServer, 40);
  EEPROM.put(ADDR_MQTT_PORT, mqttPort);
  writeEEPROMString(ADDR_MQTT_USER, mqttUser, 32);
  writeEEPROMString(ADDR_MQTT_PASS, mqttPass, 32);
  writeEEPROMString(ADDR_MQTT_PUB_TOPIC, mqttPubTopic, 64);
  writeEEPROMString(ADDR_MQTT_SUB_TOPIC, mqttSubTopic, 64);
  writeEEPROMString(ADDR_MQTT_CLIENT_ID, mqttClientId, 32);
  writeEEPROMString(ADDR_USER_TOKEN, userToken, 20);
  writeEEPROMString(ADDR_DEVICE_TYPE, deviceType, 16);
  writeEEPROMString(ADDR_DEVICE_VERSION, deviceVersion, 8);

  EEPROM.commit();
  EEPROM.end();

  Serial1.println("Налаштування збережено в EEPROM.");
}

// ─── Прототипи функцій з інших файлів ───────────────────────────────────────
void initWiFi();
void loopWIFI();
void setupWebServer();
void initMqttOTA();
void mqttOtaLoop();
void mqttInit();
void mqttLoop();
void mqttReconnectWithNewSettings();
void serialLoop();
void serialSendCommand(String cmd);
void setArduinoCommand(String key, String value);
void sendCommandToArduino();
void buildTopicsFromToken();

// ═══════════════════════════════════════════════════════════════════════════
//  SETUP
// ═══════════════════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(SERIAL_BAUD);     // UART до Arduino (115200) — тільки дані!
  Serial1.begin(115200);          // Debug UART (GPIO2, TX-only)
  delay(100);
  Serial1.println("\n\n=== Samogon Starting ===");

  loadSettings();       // Завантаження налаштувань з EEPROM
  initWiFi();           // Ініціалізація WiFi (STA + AP fallback)
  setupWebServer();     // Запуск веб-сервера налаштувань
  mqttInit();           // Ініціалізація MQTT клієнта
  initMqttOTA();        // Ініціалізація OTA через MQTT

  Serial1.println("=== Samogon Ready ===\n");
}

// ═══════════════════════════════════════════════════════════════════════════
//  LOOP
// ═══════════════════════════════════════════════════════════════════════════
void loop() {
  serialLoop();               // Прийом та парсинг даних з Serial
  mqttLoop();                 // Обробка MQTT (реконнект + відправка)
  mqttOtaLoop();              // Обробка OTA через MQTT

  // Відкладене збереження налаштувань (після веб-форми)
  if (settingsNeedSave) {
    settingsNeedSave = false;
    saveSettings();
    Serial1.println("[LOOP] Налаштування збережено (deferred).");
  }

  // Відкладений реконнект MQTT (після збереження налаштувань з веб)
  if (mqttNeedsReconnect) {
    mqttNeedsReconnect = false;
    mqttReconnectWithNewSettings();
  }

  yield();

  // Перевірка WiFi кожні 20 секунд
  static unsigned long wifiTimer = millis();
  if (millis() - wifiTimer >= WIFI_RECONNECT_INTERVAL) {
    wifiTimer = millis();
    loopWIFI();
  }
}

