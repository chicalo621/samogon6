// ============================================================================
//  Samogon — Serial комунікація
//  Прийом пакетів від Arduino HomeSamogon_ua2 (кома-розділений, маркер кінця %)
//  Відправка команд в Arduino — ПООДИНЦІ, у рідному форматі ^$&*%#@!~
// ============================================================================
//
//  === ПРОТОКОЛ Arduino → ESP (дані, кожні 2 сек) ===
//  Формат: HomeSamogon.ru/4.8,colT,atmP,cubeT,sw6,sw3,pwm1,pwm2,shim,alarm,
//          lower,alarmT,sw2,sw8,cubeFinishTemp,pwmFinishValue,pwmPeriodMs,
//          tenEnabled,finishFlag,cubeAlcohol,columnAlcohol,%,
//  Маркер кінця:  ,%   (символ % як останнє поле перед фінальною комою)
//  БЕЗ \r\n !
//
//  === ПРОТОКОЛ ESP → Arduino (команди керування, по одній) ===
//
//  ^1$           — включити воду
//  ^0$           — виключити воду
//  #440!         — встановити ШІМ клапана 440
//  @110.00!      — встановити температуру сигналізації 110
//  $0&777*777    — вимкнути зумер / вимкнути авто режим
//  $1&32.5*22.5  — авто режим: дельта 10, початкова 22.5, кінцева 32.5
//  &90*80        — змінити температуру старт-стоп 80 з дельтою 10
//  %0~           — дисплей центр: атмосферний тиск
//  %1~           — дисплей центр: температура аварії
//  %2~           — дисплей центр: температура потужності
//  !1            — увімкнути ТЕН
//  !0            — вимкнути ТЕН
//
// ============================================================================

#include "config.h"

// ─── Буфери для складених команд ─────────────────────────────────────────────
String buf_autoEnd   = "";
String buf_autoStart = "";
String buf_start = "";
String buf_stop  = "";

// ─── Імена полів для розпарсених даних ───────────────────────────────────────
const char* field_names[MAX_SERIAL_KEYS] = {
  "header",              // 0:  "HomeSamogon.ru/4.8"
  "columnTemp",          // 1:  температура колони
  "atmPressure",         // 2:  атмосферний тиск (мм рт.ст.)
  "cubeTemp",            // 3:  температура куба
  "switchString6",       // 4:  автоматичний режим (0/1)
  "switchString3",       // 5:  стан старт/стоп (0/1)
  "pwmValue1Pressure",   // 6:  температура старту
  "pwmValue2Pressure",   // 7:  температура стопу
  "tempInt2",            // 8:  значення ШІМ (0-1023)
  "alarmTempLimit",      // 9:  межа аварійної температури
  "displayLowerText",    // 10: контрольна сума (нижній рядок)
  "alarmTemp",           // 11: температура аварійного датчика (ТСА)
  "switchString2",       // 12: протікання/перегрів (0/1)
  "switchString8",       // 13: стан периферії (0/1)
  "cubeFinishTemp",      // 14: температура закінчення дистиляції
  "pwmFinishValue",      // 15: ШІМ закінчення дистиляції (0-100)
  "pwmPeriodMs",         // 16: період клапана (мс)
  "tenEnabled",          // 17: стан ТЕН (0/1)
  "finishFlag",          // 18: дистиляція: 1=іде, 0=завершена
  "cubeAlcohol",         // 19: % спирту по кубу (0-65%)
  "columnAlcohol",       // 20: % спирту по колоні (77-100%)
  "endMark"              // 21: "%" — маркер кінця пакету
};

// ─── Парсинг пакету від Arduino ─────────────��────────────────────────────────
void parseSerialPacket(String line) {
  line.trim();
  if (line.length() == 0) return;

  if (!line.startsWith("HomeSamogon")) return;

  lastRawSerial = line;
  serialKeyCount = 0;

  int startPos = 0;
  int colIdx = 0;

  while (startPos < (int)line.length() && colIdx < MAX_SERIAL_KEYS) {
    int delimPos = line.indexOf(',', startPos);
    if (delimPos == -1) delimPos = line.length();

    serialKeys[colIdx]   = field_names[colIdx];
    serialValues[colIdx] = line.substring(startPos, delimPos);
    serialValues[colIdx].trim();
    colIdx++;
    startPos = delimPos + 1;
  }

  serialKeyCount = colIdx;

  if (serialKeyCount > 1) {
    newSerialData = true;
  }
}

// ─── Основний цикл прийому Serial ────────────────────────────────────────────
void serialLoop() {
  static String inputBuffer = "";

  while (Serial.available() > 0) {
    char inChar = (char)Serial.read();

    if (inChar == '\r' || inChar == '\n') continue;

    inputBuffer += inChar;

    int markerPos = inputBuffer.indexOf(",%,");
    if (markerPos > 0) {
      String packet = inputBuffer.substring(0, markerPos + 2);
      parseSerialPacket(packet);
      inputBuffer = inputBuffer.substring(markerPos + 3);
      continue;
    }

    if ((int)inputBuffer.length() > SERIAL_BUFFER_SIZE) {
      int headerPos = inputBuffer.indexOf("HomeSamogon");
      if (headerPos > 0) {
        inputBuffer = inputBuffer.substring(headerPos);
      } else {
        inputBuffer = "";
      }
    }
  }
}

// ─── Відправка команди до Arduino ────────────────────────────────────────────
// MQTT топіки .../cmd/{key}:
//
//  water        30  стан периферії вода (0/1)
//  shim         31  значення ШІМ (0-1023)
//  PUBalarmLimit 32 сигналізація по кубі
//  autoEnd      33  дельта стоп
//  autoStart    34  дельта start
//  autoMode     35  режим авто/ручний
//  start        36  зміна дельта start
//  stop         37  зміна дельта стоп
//  display      38  дисплей центральна позиція
//  Periodkl     39  період клапана
//  pwmFinish    40  дистиляція до % відкриття клапана
//  cubeFinish   41  дистиляція до температури в кубі
//  tenControl   42  керування ТЕН (0/1)

void setArduinoCommand(String key, String value) {
  value.trim();
  String cmd = "";

  // ── Вода (клапан)
  if (key == "water") {
    cmd = "^" + value + "$";
  }

  // ── ШІМ клапана
  else if (key == "shim") {
    cmd = "#" + value + "!";
  }

  // ── Температура сигналізації
  else if (key == "PUBalarmLimit") {
    cmd = "@" + value + "!";
  }

  // ── Авто режим
  else if (key == "autoEnd") {
    buf_autoEnd = value;
  }
  else if (key == "autoStart") {
    buf_autoStart = value;
  }
  else if (key == "autoMode") {
    if (value == "0") {
      cmd = "$0&777*777";
    } else {
      cmd = "$1&" + buf_autoEnd + "*" + buf_autoStart;
    }
  }

  // ── Температура старт-стоп
  else if (key == "start") {
    buf_start = value;
    if (buf_stop.length() > 0) {
      cmd = "&" + buf_start + "*" + buf_stop;
    }
  }
  else if (key == "stop") {
    buf_stop = value;
    if (buf_start.length() > 0) {
      cmd = "&" + buf_start + "*" + buf_stop;
    }
  }

  // ── Дисплей центральна позиція
  else if (key == "display") {
    cmd = "%" + value + "~";
  }

  // ── Період клапана
  else if (key == "Periodkl") {
    cmd = ":" + value + "!";
  }

  // ── ШІМ закінчення дистиляції
  else if (key == "pwmFinish") {
    cmd = ";" + value + "!";
  }

  // ── Температура куба закінчення дистиляції
  else if (key == "cubeFinish") {
    cmd = "|" + value + "!";
  }

  // ── Керування ТЕН (НОВЕ)
  // MQTT: .../cmd/tenControl   payload: "1" або "0"
  // Serial: !1  або  !0
  else if (key == "tenControl") {
    cmd = "!" + value;
  }

  // ── Raw команда
  else if (key == "raw") {
    cmd = value;
  }

  // Невідомий ключ
  else {
    Serial1.println("[CMD] Невідомий ключ: " + key);
    return;
  }

  if (cmd.length() > 0) {
    Serial.print(cmd);
    Serial1.println("[CMD] TX → " + cmd);
  }
}

// ─── Відправка команди (виклик з mqtt_client / web_server) ──────────────────
void serialSendCommand(String cmd) {
#ifdef ENABLE_SERIAL_BRIDGE
  cmd.trim();
  if (cmd.length() == 0) return;

  int eqPos = cmd.indexOf('=');
  if (eqPos > 0) {
    String key = cmd.substring(0, eqPos);
    String val = cmd.substring(eqPos + 1);
    key.trim();
    val.trim();
    setArduinoCommand(key, val);
  } else {
    Serial.print(cmd);
    Serial1.println("[CMD] TX raw → " + cmd);
  }
#endif
}

// ─── Заглушка для сумісності ─────────────────────────────────────────────────
void sendCommandToArduino() {
  // Команди тепер відправляються одразу в setArduinoCommand()
}