// ============================================================================
//  MQTT Bridge — Головний файл
//  ESP8266 Serial ↔ MQTT Bridge з веб-інтерфейсом налаштувань
// ============================================================================
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <EEPROM.h>
#include <ArduinoOTA.h>
#include <ESPAsyncWebServer.h>
#include <ESP8266mDNS.h>
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
uint16_t mqttPort = DEFAULT_MQTT_PORT;
bool mqttConnected = false;

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
    mqttPubTopic  = readEEPROMString(ADDR_MQTT_PUB_TOPIC, 64);
    mqttSubTopic  = readEEPROMString(ADDR_MQTT_SUB_TOPIC, 64);
    mqttClientId  = readEEPROMString(ADDR_MQTT_CLIENT_ID, 32);
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
  }

  // Захист від порожніх значень
  if (mqttPubTopic.length() == 0) mqttPubTopic = DEFAULT_MQTT_PUB_TOPIC;
  if (mqttSubTopic.length() == 0) mqttSubTopic = DEFAULT_MQTT_SUB_TOPIC;
  if (mqttClientId.length() == 0) mqttClientId = DEFAULT_MQTT_CLIENT_ID;
  if (mqttPort == 0) mqttPort = DEFAULT_MQTT_PORT;

  EEPROM.end();

  Serial.println("Налаштування завантажено.");
  Serial.println("WiFi SSID: " + savedSSID);
  Serial.println("MQTT Server: " + mqttServer + ":" + String(mqttPort));
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

  EEPROM.commit();
  EEPROM.end();

  Serial.println("Налаштування збережено в EEPROM.");
}

// ─── Прототипи функцій з інших файлів ───────────────────────────────────────
void initWiFi();
void loopWIFI();
void setupWebServer();
void initOTA();
void mqttInit();
void mqttLoop();
void serialLoop();
void serialSendCommand(String cmd);

// ═══════════════════════════════════════════════════════════════════════════
//  SETUP
// ═══════════════════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(100);
  Serial.println("\n\n=== MQTT Bridge Starting ===");

  loadSettings();       // Завантаження налаштувань з EEPROM
  initWiFi();           // Ініціалізація WiFi (STA + AP fallback)
  setupWebServer();     // Запуск веб-сервера налаштувань
  initOTA();            // Запуск OTA
  mqttInit();           // Ініціалізація MQTT клієнта

  Serial.println("=== MQTT Bridge Ready ===\n");
}

// ═══════════════════════════════════════════════════════════════════════════
//  LOOP
// ═══════════════════════════════════════════════════════════════════════════
void loop() {
  MDNS.update();
  ArduinoOTA.handle();        // Обробка OTA

  serialLoop();               // Прийом та парсинг даних з Serial
  mqttLoop();                 // Обробка MQTT (реконнект + відправка)

  yield();

  // Перевірка WiFi кожні 20 секунд
  static unsigned long wifiTimer = millis();
  if (millis() - wifiTimer >= WIFI_RECONNECT_INTERVAL) {
    wifiTimer = millis();
    loopWIFI();
  }
}

