// ============================================================================
//  Samogon — WiFi утиліти
//  Підключення до WiFi, точка доступу (тільки при відсутності STA), реконнект
// ============================================================================
#include "config.h"

// ─── Запуск точки доступу (AP) ──────────────────────────────────────────────
void startAP() {
#ifdef ENABLE_WIFI
  if (!hotspotMode) {
    hotspotMode = true;
    WiFi.softAP(AP_SSID, AP_PASS);
    Serial.println("[WiFi] AP запущена: " + String(AP_SSID) + " IP: " + WiFi.softAPIP().toString());
  }
#endif
}

// ─── Зупинка точки доступу ──────────────────────────────────────────────────
void stopAP() {
#ifdef ENABLE_WIFI
  if (hotspotMode) {
    hotspotMode = false;
    WiFi.softAPdisconnect(true);
    Serial.println("[WiFi] AP вимкнена.");
  }
#endif
}

// ─── Підключення до WiFi мережі ────────────────────────────────────────────
void ConnectWIFI(String SSID, String Pass) {
#ifdef ENABLE_WIFI
  WiFi.mode(WIFI_AP_STA);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);
  WiFi.begin(SSID, Pass);

  Serial.print("[WiFi] Підключення до: " + SSID);

  uint32_t startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < WIFI_CONNECT_TIMEOUT) {
    Serial.print(".");
    delay(200);
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    // STA підключено — вимикаємо AP
    stopAP();
    localIP = WiFi.localIP().toString();
    Serial.println("[WiFi] Підключено. IP: " + localIP);
    Serial.println("[WiFi] RSSI: " + String(WiFi.RSSI()) + " dBm");
  } else {
    // STA не підключено — вмикаємо AP для налаштування
    Serial.println("[WiFi] Не вдалося підключитися.");
    startAP();
  }
#endif
}

// ─── Збереження WiFi налаштувань та підключення ─────────────────────────────
void WiFiSetup(String newSSID, String newPass) {
#ifdef ENABLE_WIFI
  savedSSID = newSSID;
  savedPass = newPass;
  saveSettings();
  ConnectWIFI(savedSSID, savedPass);
#endif
}

// ─── Примусовий перехід у режим точки доступу ───────────────────────────────
void hotspotSetup() {
#ifdef ENABLE_WIFI
  Serial.println("[WiFi] Примусовий перехід у режим AP...");
  WiFi.disconnect(false);
  WiFi.mode(WIFI_AP);
  startAP();
#endif
}

// ─── Ініціалізація WiFi при старті ──────────────────────────────────────────
void initWiFi() {
#ifdef ENABLE_WIFI
  WiFi.setSleepMode(WIFI_NONE_SLEEP);

  if (savedSSID.length() > 0) {
    // Є збережений SSID — пробуємо підключитися (AP буде тільки при неуспіху)
    WiFi.mode(WIFI_STA);
    ConnectWIFI(savedSSID, savedPass);
  } else {
    // SSID не налаштований — запускаємо тільки AP
    WiFi.mode(WIFI_AP);
    startAP();
    Serial.println("[WiFi] SSID не налаштований. AP активна для налаштування.");
  }
#endif
}

// ─── Перевірка WiFi з'єднання та реконнект ──────────────────────────────────
void loopWIFI() {
#ifdef ENABLE_WIFI
  if (savedSSID.length() > 0) {
    if (WiFi.status() != WL_CONNECTED) {
      // З'єднання втрачено — запускаємо AP і пробуємо реконнект
      if (!hotspotMode) {
        Serial.println("[WiFi] З'єднання втрачено, вмикаю AP...");
        WiFi.mode(WIFI_AP_STA);
        startAP();
      }
      WiFi.begin(savedSSID, savedPass);
      wifiRSSI = 0;
    } else {
      // З'єднання є — вимикаємо AP якщо була активна
      if (hotspotMode) {
        stopAP();
      }
      wifiRSSI = WiFi.RSSI();
      localIP = WiFi.localIP().toString();
    }
  }
#endif
}
