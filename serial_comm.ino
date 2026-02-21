// ============================================================================
//  Samogon — Serial комунікація
//  Прийом пакетів від Arduino HomeSamogon_ua1 (кома-розділений, маркер кінця %)
//  Формування команд для Arduino у форматі ^$&*%#@!
// ============================================================================
//
//  === ПРОТОКОЛ Arduino → ESP (дані, кожні 2 сек) ===
//  Формат: HomeSamogon.ru/4.8,colT,atmP,cubeT,sw6,sw3,pwm1,pwm2,shim,alarm,lower,alarmT,sw2,sw8,%,
//  Маркер кінця:  ,%   (символ % як останнє поле перед фінальною комою)
//  БЕЗ \r\n !
//
//  === ПРОТОКОЛ ESP → Arduino (команди керування) ===
//  Формат: ^<water>$<autoMode>&<pwm1>*<pwm2>%#<shim>@<alarmLimit>!
//  Приклад: ^0$1&777.0*777.0%#500@110!
//
// ============================================================================

#include "config.h"

// ─── Імена полів для розпарсених даних (індекс = позиція в пакеті) ───────────
// Ці імена стають суб-топіками MQTT: bridge/data/columnTemp тощо
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

// ─── Стан команд для відправки в Arduino (ESP → Arduino) ─────────────────────
static String cmdTempFlag29  = "0";      // ^..$ вода вкл/викл
static String cmdTempFlag33  = "0";      // $..& автоматичний режим
static String cmdPwmValue1   = "777.0";  // &..* поріг ШІМ 1
static String cmdPwmValue2   = "777.0";  // *..% поріг ШІМ 2
static String cmdTempInt2    = "0";      // #..@ значення ШІМ клапану
static String cmdAlarmLimit  = "110";    // @..! межа тривоги
static bool   cmdPending     = false;    // є зміни для відправки

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
// Arduino НЕ відправляє \r\n — пакет закінчується символом ,%,
// Тому шукаємо маркер ",%" як ознаку кінця пакету
void serialLoop() {
  static String inputBuffer = "";

  while (Serial.available() > 0) {
    char inChar = (char)Serial.read();

    // \r \n — це debug/echo від Arduino (readByteFromUART шле Serial.println)
    // Якщо в буфері вже є початок валідного пакету — ігноруємо \r\n
    // Інакше — скидаємо буфер (це сміття/debug)
    if (inChar == '\r' || inChar == '\n') {
      if (inputBuffer.length() > 0 && !inputBuffer.startsWith("HomeSamogon")) {
        inputBuffer = "";
      }
      continue;
    }

    inputBuffer += inChar;

    // Маркер кінця пакету: ",%" — Arduino шле ...switchString8,%,
    // Коли бачимо ",%" — пакет зібраний
    if (inputBuffer.endsWith(",%")) {
      parseSerialPacket(inputBuffer);
      inputBuffer = "";
      continue;
    }

    // Захист від переповнення
    if ((int)inputBuffer.length() > SERIAL_BUFFER_SIZE) {
      inputBuffer = "";
    }
  }

  // Відправка відкладених команд до Arduino
  if (cmdPending) {
    sendCommandToArduino();
    cmdPending = false;
  }
}

// ─── Формування та відправка команди до Arduino ─────────────────────────────
// Формат: ^<water>$<autoMode>&<pwm1>*<pwm2>%#<shim>@<alarmLimit>!
//
// Arduino декодує (HomeSamogon_ua1.ino рядки 717-770):
//   ^...$ → tempFlag29 (вода, "0"/"1")
//   $...& → tempFlag33 (авто, "0"/"1")
//   &...* → pwmValue1 (float)
//   *...% → pwmValue2 (float)
//   #...! → tempInt2 (int, ШІМ клапану)  [toFloat зупиняється на @]
//   @...! → alarmTempLimit (float)
void sendCommandToArduino() {
  String cmd = "^" + cmdTempFlag29 +
               "$" + cmdTempFlag33 +
               "&" + cmdPwmValue1 +
               "*" + cmdPwmValue2 +
               "%" +
               "#" + cmdTempInt2 +
               "@" + cmdAlarmLimit + "!";
  Serial.print(cmd);
}

// ─── Встановлення параметра команди (з MQTT або веб) ────────────────────────
// Наприклад: bridge/cmd/shim → key="shim", value="500"
void setArduinoCommand(String key, String value) {
  value.trim();

  if      (key == "tempFlag29" || key == "water")     cmdTempFlag29 = value;
  else if (key == "tempFlag33" || key == "autoMode")  cmdTempFlag33 = value;
  else if (key == "pwmValue1")                        cmdPwmValue1  = value;
  else if (key == "pwmValue2")                        cmdPwmValue2  = value;
  else if (key == "tempInt2"   || key == "shim")      cmdTempInt2   = value;
  else if (key == "alarmTempLimit" || key == "alarmLimit") cmdAlarmLimit = value;
  else if (key == "raw") {
    // Пряма відправка (наприклад: ^0$1&777*777%#500@110!)
    Serial.print(value);
    return;
  }
  else return; // невідомий ключ

  cmdPending = true;
}

// ─── Відправка команди в Serial (виклик з mqtt_client / web_server) ──────────
// "key=value" → розбирає і встановлює параметр
// інакше → raw відправка
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
  }
#endif
}
