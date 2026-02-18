// ============================================================================
//  MQTT Bridge — Веб-сервер
//  Маршрути: головна, налаштування, API статусу, збереження WiFi/MQTT
// ============================================================================
#include "web_pages.h"

void setupWebServer() {
#ifdef ENABLE_WEB_SERVER

  // ─── Головна сторінка ─────────────────────────────────────────────────────
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", MAIN_PAGE);
  });

  // ─── Сторінка відправки команд ────────────────────────────────────────────
  server.on("/send", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", SEND_PAGE);
  });

  // ─── API: Відправити Serial команду ───────────────────────────────────────
  server.on("/api/serial_send", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("cmd")) {
      String cmd = request->getParam("cmd")->value();
      serialSendCommand(cmd);
      request->send(200, "text/plain", "OK");
    } else {
      request->send(400, "text/plain", "Missing cmd");
    }
  });

  // ─── API: Публікація в MQTT ───────────────────────────────────────────────
  server.on("/api/mqtt_pub", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("topic") && request->hasParam("payload")) {
      String topic = request->getParam("topic")->value();
      String payload = request->getParam("payload")->value();
  #ifdef ENABLE_MQTT
      if (mqttClient.connected()) {
        mqttClient.publish(topic.c_str(), payload.c_str());
        request->send(200, "text/plain", "OK");
      } else {
        request->send(503, "text/plain", "MQTT not connected");
      }
  #else
      request->send(503, "text/plain", "MQTT disabled");
  #endif
    } else {
      request->send(400, "text/plain", "Missing topic/payload");
    }
  });

  // ─── API: Статус (JSON) ───────────────────────────────────────────────────
  server.on("/get_status", HTTP_GET, [](AsyncWebServerRequest *request) {
    String json = "{";
    json += "\"wifi_connected\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false") + ",";
    json += "\"wifi_ip\":\"" + (WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : WiFi.softAPIP().toString()) + "\",";
    json += "\"wifi_rssi\":" + String(WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0) + ",";
    json += "\"wifi_ssid\":\"" + savedSSID + "\",";
    json += "\"hotspot\":" + String(hotspotMode ? "true" : "false") + ",";
    json += "\"ap_ip\":\"" + WiFi.softAPIP().toString() + "\",";
  #ifdef ENABLE_MQTT
    json += "\"mqtt_connected\":" + String(mqttClient.connected() ? "true" : "false") + ",";
  #else
    json += "\"mqtt_connected\":false,";
  #endif
    json += "\"mqtt_server\":\"" + mqttServer + ":" + String(mqttPort) + "\",";
    json += "\"serial_last\":\"" + lastRawSerial + "\",";

    // Масив ключів
    json += "\"serial_keys\":[";
    for (uint8_t i = 0; i < serialKeyCount; i++) {
      if (i > 0) json += ",";
      json += "\"" + serialKeys[i] + "\"";
    }
    json += "],";

    // Масив значень
    json += "\"serial_values\":[";
    for (uint8_t i = 0; i < serialKeyCount; i++) {
      if (i > 0) json += ",";
      json += "\"" + serialValues[i] + "\"";
    }
    json += "],";

    json += "\"uptime\":" + String(millis() / 1000);
    json += "}";

    request->send(200, "application/json", json);
  });

  // ─── Сторінка налаштувань ─────────────────────────────────────────────────
  server.on("/settings", HTTP_GET, [](AsyncWebServerRequest *request) {
    String html = FPSTR(SETTINGS_PAGE);

    // WiFi placeholders
    html.replace("%SSID%", savedSSID);
    html.replace("%PASS%", savedPass);
    html.replace("%s_router%", !hotspotMode ? "checked" : "");
    html.replace("%s_hotspot%", hotspotMode ? "checked" : "");

    // MQTT placeholders
    html.replace("%MQTT_SRV%", mqttServer);
    html.replace("%MQTT_PORT%", String(mqttPort));
    html.replace("%MQTT_USER%", mqttUser);
    html.replace("%MQTT_PASS%", mqttPass);
    html.replace("%MQTT_CID%", mqttClientId);
    html.replace("%MQTT_PUB%", mqttPubTopic);
    html.replace("%MQTT_SUB%", mqttSubTopic);

    request->send(200, "text/html", html);
  });

  // ─── Збереження WiFi ──────────────────────────────────────────────────────
  server.on("/save_wifi", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("mode", true)) {
      String mode = request->getParam("mode", true)->value();
      if (mode == "hotspot") {
        hotspotSetup();
      } else {
        String newSSID = request->hasParam("ssid", true) ? request->getParam("ssid", true)->value() : "";
        String newPass = request->hasParam("pass", true) ? request->getParam("pass", true)->value() : "";
        savedSSID = newSSID;
        savedPass = newPass;
        saveSettings();
        // Підключаємося до нової мережі
        ConnectWIFI(savedSSID, savedPass);
        if (WiFi.status() == WL_CONNECTED) {
          hotspotMode = false;
          localIP = WiFi.localIP().toString();
        }
      }
    }
    request->send(200, "text/plain", "OK");
  });

  // ─── Збереження MQTT ──────────────────────────────────────────────────────
  server.on("/save_mqtt", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("mqtt_server", true))
      mqttServer = request->getParam("mqtt_server", true)->value();
    if (request->hasParam("mqtt_port", true))
      mqttPort = request->getParam("mqtt_port", true)->value().toInt();
    if (request->hasParam("mqtt_user", true))
      mqttUser = request->getParam("mqtt_user", true)->value();
    if (request->hasParam("mqtt_pass", true))
      mqttPass = request->getParam("mqtt_pass", true)->value();
    if (request->hasParam("mqtt_client_id", true))
      mqttClientId = request->getParam("mqtt_client_id", true)->value();
    if (request->hasParam("mqtt_pub_topic", true))
      mqttPubTopic = request->getParam("mqtt_pub_topic", true)->value();
    if (request->hasParam("mqtt_sub_topic", true))
      mqttSubTopic = request->getParam("mqtt_sub_topic", true)->value();

    saveSettings();

    // Переініціалізація MQTT з новими налаштуваннями
    mqttReconnectWithNewSettings();

    request->send(200, "text/plain", "OK");
  });

  // ─── Сканування WiFi мереж ────────────────────────────────────────────────
  server.on("/scan_wifi", HTTP_GET, [](AsyncWebServerRequest *request) {
    int n = WiFi.scanComplete();
    if (n == WIFI_SCAN_FAILED) {
      WiFi.scanNetworks(true);  // Асинхронне сканування
      request->send(200, "application/json", "[]");
      return;
    }
    if (n == WIFI_SCAN_RUNNING) {
      request->send(200, "application/json", "[]");
      return;
    }
    // Сканування завершене
    String json = "[";
    for (int i = 0; i < n; i++) {
      if (i > 0) json += ",";
      json += "{\"ssid\":\"" + WiFi.SSID(i) + "\",\"rssi\":" + String(WiFi.RSSI(i)) + "}";
    }
    json += "]";
    WiFi.scanDelete();
    // Запускаємо нове сканування для наступного запиту
    WiFi.scanNetworks(true);
    request->send(200, "application/json", json);
  });

  // ─── Перезавантаження ─────────────────────────────────────────────────────
  server.on("/restart", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "Restarting...");
    delay(500);
    ESP.restart();
  });

  // ─── Скидання до заводських налаштувань ────────────────────────────────────
  server.on("/factory_reset", HTTP_GET, [](AsyncWebServerRequest *request) {
    EEPROM.begin(EEPROM_SIZE);
    for (int i = 0; i < EEPROM_SIZE; i++) {
      EEPROM.write(i, 0xFF);
    }
    EEPROM.commit();
    EEPROM.end();
    request->send(200, "text/plain", "Factory reset done. Restarting...");
    delay(500);
    ESP.restart();
  });

  // ─── Запуск сервера ───────────────────────────────────────────────────────
  server.begin();
  Serial.println("[WEB] Сервер запущено.");

#endif // ENABLE_WEB_SERVER
}

