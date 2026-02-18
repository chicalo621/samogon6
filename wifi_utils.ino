// ============================================================================
//  MQTT Bridge — WiFi утиліти
//  Підключення до WiFi, точка доступу, реконнект
// ============================================================================
#include "config.h"

// ─── Запуск точки доступу (AP) ──────────────────────────────────────────────
void startHotspot() {
#ifdef ENABLE_WIFI
  hotspotMode = true;
  WiFi.mode(WIFI_AP_STA);  // AP + STA — щоб веб-інтерфейс працював завжди
  WiFi.softAP(AP_SSID, AP_PASS);
  Serial.println("Точка доступу запущена: " + String(AP_SSID));
  Serial.println("AP IP: " + WiFi.softAPIP().toString());
#endif
}

// ─── Підключення до WiFi мережі ────────────────────────────────────────────
void ConnectWIFI(String SSID, String Pass) {
#ifdef ENABLE_WIFI
  WiFi.mode(WIFI_AP_STA);  // Залишаємо AP активною для налаштувань
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);
  WiFi.begin(SSID, Pass);

  Serial.print("Підключення до WiFi: " + SSID);

  uint32_t startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < WIFI_CONNECT_TIMEOUT) {
    Serial.print(".");
    delay(200);
  }
  Serial.println();
#endif
}

// ─── Збереження WiFi налаштувань та підключення ─────────────────────────────
void WiFiSetup(String newSSID, String newPass) {
#ifdef ENABLE_WIFI
  hotspotMode = false;
  savedSSID = newSSID;
  savedPass = newPass;
  saveSettings();  // Зберігаємо все в EEPROM

  if (WiFi.status() != WL_CONNECTED) {
    ConnectWIFI(savedSSID, savedPass);
  }
#endif
}

// ─── Перехід у режим точки доступу ──────────────────────────────────────────
void hotspotSetup() {
#ifdef ENABLE_WIFI
  Serial.println("Перехід у режим точки доступу...");
  hotspotMode = true;
  startHotspot();
#endif
}

// ─── Ініціалізація WiFi при старті ──────────────────────────────────────────
void initWiFi() {
#ifdef ENABLE_WIFI
  // Завжди запускаємо AP, щоб можна було налаштувати
  WiFi.mode(WIFI_AP_STA);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFi.softAP(AP_SSID, AP_PASS);
  Serial.println("AP запущена: " + String(AP_SSID) + " IP: " + WiFi.softAPIP().toString());

  // Якщо є збережений SSID — підключаємося до роутера
  if (savedSSID.length() > 0) {
    ConnectWIFI(savedSSID, savedPass);

    if (WiFi.status() == WL_CONNECTED) {
      hotspotMode = false;
      localIP = WiFi.localIP().toString();
      Serial.println("Підключено до WiFi. IP: " + localIP);
      Serial.println("RSSI: " + String(WiFi.RSSI()) + " dBm");
    } else {
      Serial.println("Не вдалося підключитися до WiFi.");
      hotspotMode = true;
    }
  } else {
    hotspotMode = true;
    Serial.println("WiFi SSID не налаштований. Використовуйте AP для налаштування.");
  }
#endif
}

// ─── Перевірка WiFi з'єднання та реконнект ──────────────────────────────────
void loopWIFI() {
#ifdef ENABLE_WIFI
  if (savedSSID.length() > 0) {
    if (WiFi.status() != WL_CONNECTED) {
      WiFi.begin(savedSSID, savedPass);
      wifiRSSI = 0;
    } else {
      wifiRSSI = WiFi.RSSI();
      localIP = WiFi.localIP().toString();
    }
  }
#endif
}

