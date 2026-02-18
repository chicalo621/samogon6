// ============================================================================
//  MQTT Bridge — MQTT клієнт
//  Підключення до MQTT брокера, публікація, підписка, обробка повідомлень
// ============================================================================

// ─── Callback при отриманні повідомлення з MQTT ─────────────────────────────
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message;
  message.reserve(length);
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  String topicStr = String(topic);
  Serial.println("[MQTT RX] " + topicStr + " : " + message);

  // Визначаємо суб-топік (після базового subscribe-топіка)
  // Наприклад: bridge/cmd/power → sub-topic = "power"
  String subTopic = "";
  String baseTopic = mqttSubTopic;
  // Видаляємо /# з кінця для порівняння
  if (baseTopic.endsWith("/#")) {
    baseTopic = baseTopic.substring(0, baseTopic.length() - 2);
  } else if (baseTopic.endsWith("#")) {
    baseTopic = baseTopic.substring(0, baseTopic.length() - 1);
  }

  if (topicStr.startsWith(baseTopic + "/")) {
    subTopic = topicStr.substring(baseTopic.length() + 1);
  }

  // Формуємо команду для Serial
  if (subTopic.length() > 0) {
    // bridge/cmd/power з payload "500" → "power=500"
    serialSendCommand(subTopic + "=" + message);
  } else {
    // Якщо прийшло на точний топік — пересилаємо payload як є
    serialSendCommand(message);
  }
}

// ─── Підключення до MQTT брокера ────────────────────────────────────────────
bool mqttConnect() {
#ifdef ENABLE_MQTT
  if (mqttServer.length() == 0) return false;
  if (WiFi.status() != WL_CONNECTED && !hotspotMode) return false;

  Serial.print("[MQTT] Підключення до " + mqttServer + ":" + String(mqttPort) + "... ");

  bool connected = false;
  if (mqttUser.length() > 0) {
    connected = mqttClient.connect(mqttClientId.c_str(), mqttUser.c_str(), mqttPass.c_str());
  } else {
    connected = mqttClient.connect(mqttClientId.c_str());
  }

  if (connected) {
    Serial.println("OK");
    mqttConnected = true;

    // Підписуємося на командний топік з wildcard
    String subWithWild = mqttSubTopic;
    if (!subWithWild.endsWith("#") && !subWithWild.endsWith("+")) {
      subWithWild += "/#";
    }
    mqttClient.subscribe(subWithWild.c_str());
    // Також підписуємось на точний топік
    mqttClient.subscribe(mqttSubTopic.c_str());

    Serial.println("[MQTT] Підписка на: " + subWithWild);
    Serial.println("[MQTT] Підписка на: " + mqttSubTopic);

    // Публікуємо статус online
    String statusTopic = mqttPubTopic + "/status";
    mqttClient.publish(statusTopic.c_str(), "online", true);
  } else {
    Serial.println("ПОМИЛКА, rc=" + String(mqttClient.state()));
    mqttConnected = false;
  }
  return connected;
#else
  return false;
#endif
}

// ─── Ініціалізація MQTT ─────────────────────────────────────────────────────
void mqttInit() {
#ifdef ENABLE_MQTT
  if (mqttServer.length() > 0) {
    mqttClient.setServer(mqttServer.c_str(), mqttPort);
    mqttClient.setCallback(mqttCallback);
    mqttClient.setBufferSize(512);
    Serial.println("[MQTT] Ініціалізовано: " + mqttServer + ":" + String(mqttPort));
  } else {
    Serial.println("[MQTT] Сервер не налаштований.");
  }
#endif
}

// ─── Публікація даних Serial → MQTT ─────────────────────────────────────────
void mqttPublishSerialData() {
#ifdef ENABLE_MQTT
  if (!mqttClient.connected()) return;
  if (serialKeyCount == 0) return;

  // Публікуємо кожну пару key=value в окремий суб-топік
  for (uint8_t i = 0; i < serialKeyCount; i++) {
    String topic = mqttPubTopic + "/" + serialKeys[i];
    mqttClient.publish(topic.c_str(), serialValues[i].c_str());
  }

  // Також публікуємо весь рядок як raw
  String rawTopic = mqttPubTopic + "/raw";
  mqttClient.publish(rawTopic.c_str(), lastRawSerial.c_str());
#endif
}

// ─── Основний цикл MQTT ────────────────────────────────────────────────────
void mqttLoop() {
#ifdef ENABLE_MQTT
  if (mqttServer.length() == 0) return;

  if (!mqttClient.connected()) {
    mqttConnected = false;
    // Неблокуючий реконнект з інтервалом
    static unsigned long lastReconnectAttempt = 0;
    if (millis() - lastReconnectAttempt >= MQTT_RECONNECT_INTERVAL) {
      lastReconnectAttempt = millis();
      mqttConnect();
    }
  } else {
    mqttClient.loop();
  }

  // Якщо є нові дані з Serial — публікуємо в MQTT
  if (newSerialData) {
    newSerialData = false;
    mqttPublishSerialData();
  }
#endif
}

// ─── Переініціалізація MQTT (після зміни налаштувань) ────────────────────────
void mqttReconnectWithNewSettings() {
#ifdef ENABLE_MQTT
  if (mqttClient.connected()) {
    String statusTopic = mqttPubTopic + "/status";
    mqttClient.publish(statusTopic.c_str(), "offline", true);
    mqttClient.disconnect();
  }
  mqttConnected = false;

  if (mqttServer.length() > 0) {
    mqttClient.setServer(mqttServer.c_str(), mqttPort);
    mqttConnect();
  }
#endif
}

