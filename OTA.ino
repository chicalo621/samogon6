// ============================================================================
//  Samogon — OTA оновлення прошивки через MQTT
//  Прийом прошивки бінарними чанками через MQTT топік
// ============================================================================
//
//  Протокол оновлення:
//    1. Відправити розмір прошивки:  {sub_topic}/ota/begin  → payload: розмір в байтах (текстом)
//    2. Відправити чанки:            {sub_topic}/ota/data   → payload: бінарні дані (до 4096 байт)
//    3. Завершити оновлення:         {sub_topic}/ota/end    → payload: будь-який (або порожній)
//
//  Статус публікується в:            {pub_topic}/ota/status
//  Прогрес публікується в:           {pub_topic}/ota/progress
//
// ============================================================================

#include <Updater.h>

// Стан OTA
static bool     otaInProgress   = false;
static uint32_t otaTotalSize    = 0;
static uint32_t otaReceived     = 0;
static uint32_t otaLastProgress = 0;
static unsigned long otaLastDataTime = 0;

// ─── Публікація статусу OTA ─────────────────────────────────────────────────
void otaPublishStatus(const char* status) {
#ifdef ENABLE_MQTT
  if (mqttClient.connected()) {
    String topic = mqttPubTopic + "/ota/status";
    mqttClient.publish(topic.c_str(), status, false);
  }
#endif
  Serial.println(String("[OTA] ") + status);
}

void otaPublishProgress(uint8_t percent) {
#ifdef ENABLE_MQTT
  if (mqttClient.connected()) {
    String topic = mqttPubTopic + "/ota/progress";
    mqttClient.publish(topic.c_str(), String(percent).c_str(), false);
  }
#endif
}

// ─── Обробка OTA команд з MQTT ──────────────────────────────────────────────
void mqttOtaCallback(String subTopic, byte* payload, unsigned int length) {

  // ── ota/begin — початок оновлення ──
  if (subTopic == "ota/begin") {
    String sizeStr;
    sizeStr.reserve(length);
    for (unsigned int i = 0; i < length; i++) {
      sizeStr += (char)payload[i];
    }
    otaTotalSize = sizeStr.toInt();

    if (otaTotalSize == 0) {
      otaPublishStatus("error: invalid size");
      return;
    }

    // Перевіряємо наявність вільного місця
    uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
    if (otaTotalSize > maxSketchSpace) {
      otaPublishStatus("error: firmware too large");
      Serial.printf("[OTA] Розмір %u > доступно %u\n", otaTotalSize, maxSketchSpace);
      return;
    }

    // Збільшуємо буфер MQTT для прийому чанків
    mqttClient.setBufferSize(MQTT_OTA_CHUNK_SIZE + 128);

    otaReceived = 0;
    otaLastProgress = 0;
    otaInProgress = true;
    otaLastDataTime = millis();

    if (Update.begin(otaTotalSize, U_FLASH)) {
      otaPublishStatus("started");
      Serial.printf("[OTA] Старт оновлення, розмір: %u байт\n", otaTotalSize);
    } else {
      otaInProgress = false;
      otaPublishStatus("error: update begin failed");
      Serial.println("[OTA] Update.begin() помилка");
    }
    return;
  }

  // ── ota/data — чанк прошивки ──
  if (subTopic == "ota/data") {
    if (!otaInProgress) {
      otaPublishStatus("error: no update in progress");
      return;
    }

    otaLastDataTime = millis();

    size_t written = Update.write(payload, length);
    if (written != length) {
      otaInProgress = false;
      Update.end(false);
      mqttClient.setBufferSize(512);
      otaPublishStatus("error: write failed");
      Serial.printf("[OTA] Помилка запису: записано %u з %u\n", written, length);
      return;
    }

    otaReceived += length;

    // Публікуємо прогрес кожні 5%
    uint8_t percent = (uint8_t)((otaReceived * 100) / otaTotalSize);
    if (percent >= otaLastProgress + 5 || percent == 100) {
      otaLastProgress = percent;
      otaPublishProgress(percent);
      Serial.printf("[OTA] Прогрес: %u%% (%u/%u)\n", percent, otaReceived, otaTotalSize);
    }
    return;
  }

  // ── ota/end — завершення оновлення ──
  if (subTopic == "ota/end") {
    if (!otaInProgress) {
      otaPublishStatus("error: no update in progress");
      return;
    }

    otaInProgress = false;
    mqttClient.setBufferSize(512);

    if (Update.end(true)) {
      otaPublishStatus("success, rebooting...");
      Serial.println("[OTA] Оновлення завершено успішно! Перезавантаження...");
      delay(1000);
      ESP.restart();
    } else {
      String errMsg = "error: update end failed, err=" + String(Update.getError());
      otaPublishStatus(errMsg.c_str());
      Serial.printf("[OTA] Помилка завершення: %d\n", Update.getError());
    }
    return;
  }

  // ── ota/abort — скасування оновлення ──
  if (subTopic == "ota/abort") {
    if (otaInProgress) {
      otaInProgress = false;
      Update.end(false);
      mqttClient.setBufferSize(512);
      otaPublishStatus("aborted");
      Serial.println("[OTA] Оновлення скасовано.");
    }
    return;
  }
}

// ─── Підписка на OTA топіки ─────────────────────────────────────────────────
void mqttOtaSubscribe() {
#ifdef ENABLE_OTA
  if (!mqttClient.connected()) return;

  String baseTopic = mqttSubTopic;
  if (baseTopic.endsWith("/#")) {
    baseTopic = baseTopic.substring(0, baseTopic.length() - 2);
  } else if (baseTopic.endsWith("#")) {
    baseTopic = baseTopic.substring(0, baseTopic.length() - 1);
  }

  mqttClient.subscribe((baseTopic + "/ota/begin").c_str(), 1);
  mqttClient.subscribe((baseTopic + "/ota/data").c_str(), 0);
  mqttClient.subscribe((baseTopic + "/ota/end").c_str(), 1);
  mqttClient.subscribe((baseTopic + "/ota/abort").c_str(), 1);

  Serial.println("[OTA] Підписка на MQTT OTA топіки");
#endif
}

// ─── Ініціалізація MQTT OTA ─────────────────────────────────────────────────
void initMqttOTA() {
#ifdef ENABLE_OTA
  otaInProgress = false;
  Serial.println("[OTA] MQTT OTA ініціалізовано.");
#endif
}

// ─── Цикл MQTT OTA (перевірка таймауту) ─────────────────────────────────────
void mqttOtaLoop() {
#ifdef ENABLE_OTA
  if (otaInProgress) {
    if (otaLastDataTime > 0 && millis() - otaLastDataTime > MQTT_OTA_TIMEOUT) {
      otaInProgress = false;
      Update.end(false);
      mqttClient.setBufferSize(512);
      otaPublishStatus("error: timeout");
      Serial.println("[OTA] Таймаут оновлення.");
      otaLastDataTime = 0;
    }
  }
#endif
}

// ─── Перевірка чи це OTA повідомлення ────────────────────────────────────────
bool isMqttOtaMessage(String subTopic) {
  return subTopic.startsWith("ota/");
}
