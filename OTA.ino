// ============================================================================
//  MQTT Bridge — OTA оновлення прошивки
// ============================================================================

void initOTA() {
#ifdef ENABLE_OTA
  ArduinoOTA.setHostname("MQTTBridge");
  ArduinoOTA.setPassword("admin");

  ArduinoOTA.onStart([]() {
    Serial.println("[OTA] Старт оновлення...");
  });

  ArduinoOTA.onEnd([]() {
    Serial.println("\n[OTA] Прошивку отримано.");
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    static uint16_t count = 0;
    count++;
    if (count > 20) { count = 0; Serial.print("."); }
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("[OTA] Помилка[%u]: ", error);
    if (error == OTA_AUTH_ERROR)         Serial.println("Авторизація");
    else if (error == OTA_BEGIN_ERROR)   Serial.println("Старт");
    else if (error == OTA_CONNECT_ERROR) Serial.println("З'єднання");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Прийом");
    else if (error == OTA_END_ERROR)     Serial.println("Завершення");
  });

  ArduinoOTA.begin();
  Serial.println("[OTA] Сервер запущено.");
#endif
}

