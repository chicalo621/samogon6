// // ============================================================================
// //  Samogon — Serial комунікація
// //  Прийом даних з UART, парсинг key=value, відправка команд
// // ============================================================================
// #include "config.h"

// // ─── Парсинг рядка з key=value парами ───────────────────────────────────────
// // Формат вхідних даних: "key1=value1|key2=value2|key3=value3\r\n"
// // або просто довільний рядок для прямої пересилки
// void parseSerialLine(String line) {
//   line.trim();
//   if (line.length() == 0) return;

//   lastRawSerial = line;
//   serialKeyCount = 0;

//   // Спроба парсингу як key=value пар
//   int startPos = 0;
//   while (startPos < (int)line.length() && serialKeyCount < MAX_SERIAL_KEYS) {
//     int delimPos = line.indexOf(SERIAL_DELIMITER, startPos);
//     if (delimPos == -1) delimPos = line.length();

//     String pair = line.substring(startPos, delimPos);
//     pair.trim();

//     int eqPos = pair.indexOf(SERIAL_KV_SEPARATOR);
//     if (eqPos > 0) {
//       serialKeys[serialKeyCount] = pair.substring(0, eqPos);
//       serialValues[serialKeyCount] = pair.substring(eqPos + 1);
//       serialKeys[serialKeyCount].trim();
//       serialValues[serialKeyCount].trim();
//       serialKeyCount++;
//     } else if (pair.length() > 0) {
//       // Якщо немає '=', зберігаємо весь фрагмент як значення з індексним ключем
//       serialKeys[serialKeyCount] = "d" + String(serialKeyCount);
//       serialValues[serialKeyCount] = pair;
//       serialKeyCount++;
//     }

//     startPos = delimPos + 1;
//   }

//   if (serialKeyCount > 0) {
//     newSerialData = true;
//   }
// }

// // ─── Основний цикл прийому Serial ───────────────────────────────────────────
// void serialLoop() {
// #ifdef ENABLE_SERIAL_BRIDGE
//   static String inputBuffer;
//   static uint16_t bufIndex = 0;

//   while (Serial.available() > 0) {
//     char inChar = (char)Serial.read();

//     if (inChar == '\r' || inChar == '\n') {
//       if (inputBuffer.length() > 0) {
//         parseSerialLine(inputBuffer);
//         inputBuffer = "";
//         bufIndex = 0;
//       }
//     } else if (bufIndex < SERIAL_BUFFER_SIZE) {
//       inputBuffer += inChar;
//       bufIndex++;
//     } else {
//       // Буфер переповнений — скидаємо
//       inputBuffer = "";
//       bufIndex = 0;
//     }
//   }
// #endif
// }

// // ─── Відправка команди в Serial ─────────────────────────────────────────────
// void serialSendCommand(String cmd) {
// #ifdef ENABLE_SERIAL_BRIDGE
//   Serial.println(cmd);
// #endif
// }
// ============================================================================
//  Samogon — Serial комунікація (адаптована під пакет з комами HomeSamogon.ru/...)
//  Прийом даних з UART, розбиття по комі та іменований масив для MQTT
// ============================================================================
// ============================================================================
//  Samogon — Serial комунікація (оригінальний: тільки пакет з комами)
// ============================================================================

#include "config.h"

// Масив імен для полів (без дублювання, для MAX_SERIAL_KEYS!)
const char* field_names[MAX_SERIAL_KEYS] = {
  "displayUpperText",     // 0: HomeSamogon.ru/4.8
  "columnTemp",           // 1
  "atmPressure",          // 2
  "cubeTemp",             // 3
  "switchString6",        // 4
  "switchString3",        // 5
  "pwmValue1Pressure",    // 6
  "pwmValue2Pressure",    // 7
  "tempInt2",             // 8
  "alarmTempLimit",       // 9
  "displayLowerText",     // 10
  "alarmTemp",            // 11
  "switchString2",        // 12
  "switchString8",        // 13
  "endMark"               // 14, "%"
};

// Функція парсить пакет з комами, розкладає значення по serialKeys/serialValues
void parseSerialLineComma(String line) {
  line.trim();
  if (line.length() == 0) return;

  lastRawSerial = line;
  serialKeyCount = 0;

  int startPos = 0, colIdx = 0;
  while (startPos < line.length() && colIdx < MAX_SERIAL_KEYS) {
    int delimPos = line.indexOf(',', startPos);
    if (delimPos == -1) delimPos = line.length();

    serialKeys[colIdx] = field_names[colIdx];
    serialValues[colIdx] = line.substring(startPos, delimPos);
    serialValues[colIdx].trim();
    colIdx++;
    startPos = delimPos + 1;
  }
  serialKeyCount = colIdx;
  newSerialData = true;
}

// Основний serial loop: приймає рядок, викликає парсер
void serialLoop() {
  static String inputBuffer = "";
  static uint16_t bufIndex = 0;

  while (Serial.available() > 0) {
    char inChar = (char)Serial.read();

    if (inChar == '\r' || inChar == '\n') {
      if (inputBuffer.length() > 0) {
        parseSerialLineComma(inputBuffer);
        inputBuffer = "";
        bufIndex = 0;
      }
    } else if (bufIndex < SERIAL_BUFFER_SIZE) {
      inputBuffer += inChar;
      bufIndex++;
    } else {
      // Якщо переповнився буфер — скидаємо
      inputBuffer = "";
      bufIndex = 0;
    }
  }
}

// Відправка команди в Serial (залежить від ENABLE_SERIAL_BRIDGE)
void serialSendCommand(String cmd) {
#ifdef ENABLE_SERIAL_BRIDGE
  Serial.println(cmd);
#endif
}
