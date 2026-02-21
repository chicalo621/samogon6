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
    Serial1.println("[WiFi] AP запущена: " + String(AP_SSID) + " IP: " + WiFi.softAPIP().toString());
  }
#endif
}

// ─── Зупинка точки доступу ──────────────────────────────────────────────────
void stopAP() {
#ifdef ENABLE_WIFI
  if (hotspotMode) {
    hotspotMode = false;
    WiFi.softAPdisconnect(true);
    Serial1.println("[WiFi] AP вимкнена.");
  }
#endif
}

// ─── Підключення до WiFi мережі ────────────────────────────────────────────
void ConnectWIFI(String SSID, String Pass) {
#ifdef ENABLE_WIFI
  // Запускаємо ТІЛЬКИ STA — AP увімкнеться лише при невдачі
  WiFi.mode(WIFI_STA);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);
  WiFi.begin(SSID, Pass);

  Serial1.print("[WiFi] Підключення до: " + SSID);

  uint32_t startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < WIFI_CONNECT_TIMEOUT) {
    Serial1.print(".");
    delay(200);
  }
  Serial1.println();

  if (WiFi.status() == WL_CONNECTED) {
    // STA підключено — AP не потрібна
    stopAP();
    localIP = WiFi.localIP().toString();
    Serial1.println("[WiFi] Підключено. IP: " + localIP);
    Serial1.println("[WiFi] RSSI: " + String(WiFi.RSSI()) + " dBm");
  } else {
    // STA не підключено — вмикаємо AP для налаштування
    Serial1.println("[WiFi] Не вдалося підключитися.");
    WiFi.mode(WIFI_AP_STA);  // AP + STA (щоб STA продовжував спроби)
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
  Serial1.println("[WiFi] Примусовий перехід у режим AP...");
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
    // Є збережений SSID — підключаємось тільки по STA (AP при невдачі)
    ConnectWIFI(savedSSID, savedPass);
  } else {
    // SSID не налаштований — тільки AP
    WiFi.mode(WIFI_AP);
    startAP();
    Serial1.println("[WiFi] SSID не налаштований. AP активна для налаштування.");
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
        Serial1.println("[WiFi] З'єднання втрачено, вмикаю AP...");
        WiFi.mode(WIFI_AP_STA);
        startAP();
      }
      WiFi.begin(savedSSID, savedPass);
      wifiRSSI = 0;
    } else {
      // З'єднання є — вимикаємо AP якщо була активна
      if (hotspotMode) {
        stopAP();
        WiFi.mode(WIFI_STA);  // Повертаємось до чистого STA
      }
      wifiRSSI = WiFi.RSSI();
      localIP = WiFi.localIP().toString();
    }
  }
#endif
}
