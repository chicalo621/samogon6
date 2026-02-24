// ============================================================================
//  Samogon — Serial комунікація
//  Прийом пакетів від Arduino HomeSamogon_ua1 (кома-розділений, маркер кінця %)
//  Відправка команд в Arduino — ПООДИНЦІ, у рідному форматі ^$&*%#@!~
// ============================================================================
//
//  === ПРОТОКОЛ Arduino → ESP (дані, кожні 2 сек) ===
//  Формат: HomeSamogon.ru/4.8,colT,atmP,cubeT,sw6,sw3,pwm1,pwm2,shim,alarm,lower,alarmT,sw2,sw8,%,
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
//
// ============================================================================

#include "config.h"

// ─── Буфери для складених команд (кожен топік приходить окремо) ──────────────
// auto: $mode&endT*startT  →  autoEnd і autoStart зберігаються, autoMode — тригер
String buf_autoEnd   = "";
String buf_autoStart = "";
// startStop: &start*stop  →  два окремих топіки start, stop
String buf_start = "";
String buf_stop  = "";

// ─── Імена полів для розпарсених даних (індекс = позиція в пакеті) ───────────
// Ці імена стають суб-топіками MQTT: .../data/columnTemp тощо
const char* field_names[MAX_SERIAL_KEYS] = {
  "header",              // 0: "HomeSamogon.ru/4.8"
  "columnTemp",          // 1: температура колони
  "atmPressure",         // 2: атмосферний тиск (мм рт.ст.)
  "cubeTemp",            // 3: температура куба
  "switchString6",       // 4: автоматичний режим (0/1)
  "switchString3",       // 5: стан старт/стоп (0/1)
  "pwmValue1Pressure",   // 6: pwmValue1 - pressureValue
  "pwmValue2Pressure",   // 7: pwmValue2 - pressureValue
  "tempInt2",            // 8: значення ШІМ (0-1023)
  "alarmTempLimit",      // 9: межа аварійної температури
  "displayLowerText",    // 10: контрольна сума (нижній рядок)
  "alarmTemp",           // 11: температура аварійного датчика (ТСА)
  "switchString2",       // 12: протікання/перегрів (0/1)
  "switchString8",       // 13: стан периферії (0/1)
  "endMark"              // 14: "%" — маркер кінця пакету
};

// ─── Парсинг пакету від Arduino (кома-розділений) ────────────────────────────
void parseSerialPacket(String line) {
  line.trim();
  if (line.length() == 0) return;

  // Валідація: пакет повинен починатися з заголовка HomeSamogon
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

// ─── Основний цикл прийому Serial ───────────────────────────────────────────
// Формат потоку (БЕЗ \n):
//   ...sw8,%,HomeSamogon.ru/4.8,...sw8,%,HomeSamogon.ru/4.8,...
// Маркер кінця пакету: ,%,
void serialLoop() {
  static String inputBuffer = "";

  while (Serial.available() > 0) {
    char inChar = (char)Serial.read();

    // Ігноруємо \r \n якщо раптом є
    if (inChar == '\r' || inChar == '\n') continue;

    inputBuffer += inChar;

    // Шукаємо повний маркер кінця пакету: ,%,
    int markerPos = inputBuffer.indexOf(",%,");
    if (markerPos > 0) {
      // Витягуємо пакет до маркера (включно з ,%)
      String packet = inputBuffer.substring(0, markerPos + 2); // до ",%"
      parseSerialPacket(packet);

      // Все що після ,%,  — це початок наступного пакету
      inputBuffer = inputBuffer.substring(markerPos + 3);
      continue;
    }

    // Захист від переповнення
    if ((int)inputBuffer.length() > SERIAL_BUFFER_SIZE) {
      // Спробуємо знайти початок пакету в буфері
      int headerPos = inputBuffer.indexOf("HomeSamogon");
      if (headerPos > 0) {
        inputBuffer = inputBuffer.substring(headerPos);
      } else {
        inputBuffer = "";
      }
    }
  }
}

// ─── Відправка одиночної команди до Arduino ─────────────────────────────────
// Команди відправляються ПО ОДНІЙ у рідному форматі.
// MQTT топік .../cmd/{key} з payload → формує відповідну команду
void setArduinoCommand(String key, String value) {
  value.trim();
  String cmd = "";

  // ── Вода (клапан) ──
  // MQTT: .../cmd/water   payload: "1" або "0"
  // Serial: ^1$ або ^0$
  if (key == "water") {
    cmd = "^" + value + "$";
  }

  // ── ШІМ клапана ──
  // MQTT: .../cmd/shim   payload: "440"
  // Serial: #440!
  else if (key == "shim") {
    cmd = "#" + value + "!";
  }

  // ── Температура сигналізації ──
  // MQTT: .../cmd/alarmLimit   payload: "110.00"
  // Serial: @110.00!
  else if (key == "alarmLimit") {
    cmd = "@" + value + "!";
  }

  // ── Авто режим (3 окремих топіки) ──
  // MQTT: .../cmd/autoMode   payload: "1" або "0"  ← ТРИГЕР відправки
  // MQTT: .../cmd/autoEnd    payload: "32.5"        ← просто зберігаємо
  // MQTT: .../cmd/autoStart  payload: "22.5"        ← просто зберігаємо
  // Serial: $1&32.5*22.5  або  $0&777*777
  else if (key == "autoEnd") {
    buf_autoEnd = value;   // тільки зберігаємо, не відправляємо
  }
  else if (key == "autoStart") {
    buf_autoStart = value; // тільки зберігаємо, не відправляємо
  }
  else if (key == "autoMode") {
    if (value == "0") {
      cmd = "$0&777*777";  // вимкнути авто режим
    } else {
      cmd = "$1&" + buf_autoEnd + "*" + buf_autoStart;  // увімкнути з температурами
    }
  }

  // ── Температура старт-стоп (2 окремих топіки → збираємо і відправляємо) ──
  // MQTT: .../cmd/start   payload: "90"
  // MQTT: .../cmd/stop    payload: "80"
  // Serial: &90*80
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

  // ── Дисплей центральна позиція ──
  // MQTT: .../cmd/display   payload: "0", "1", або "2"
  // Serial: %0~  %1~  %2~
  else if (key == "display") {
    cmd = "%" + value + "~";
  }

  // ── Raw команда (прямий формат Arduino) ──
  // MQTT: .../cmd/raw   payload: "^1$" або будь-яка інша
  else if (key == "raw") {
    cmd = value;
  }

  // Невідомий ключ — ігноруємо
  else {
    Serial1.println("[CMD] Невідомий ключ: " + key);
    return;
  }

  // Відправка
  if (cmd.length() > 0) {
    Serial.print(cmd);
    Serial1.println("[CMD] TX → " + cmd);
  }
}

// ─── Відправка команди (виклик з mqtt_client / web_server) ──────────────────
// Формат "key=value" → розбирає і відправляє окрему команду
// Інакше → raw відправка
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
    // Raw — відправляємо як є
    Serial.print(cmd);
    Serial1.println("[CMD] TX raw → " + cmd);
  }
#endif
}

// ─── Заглушка для сумісності ────────────────────────────────────────────────
void sendCommandToArduino() {
  // Команди тепер відправляються одразу в setArduinoCommand()
}
