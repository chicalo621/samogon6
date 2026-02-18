// ============================================================================
//  MQTT Bridge — Serial комунікація
//  Прийом даних з UART, парсинг key=value, відправка команд
// ============================================================================
#include "config.h"

// ─── Парсинг рядка з key=value парами ───────────────────────────────────────
// Формат вхідних даних: "key1=value1|key2=value2|key3=value3\r\n"
// або просто довільний рядок для прямої пересилки
void parseSerialLine(String line) {
  line.trim();
  if (line.length() == 0) return;

  lastRawSerial = line;
  serialKeyCount = 0;

  // Спроба парсингу як key=value пар
  int startPos = 0;
  while (startPos < (int)line.length() && serialKeyCount < MAX_SERIAL_KEYS) {
    int delimPos = line.indexOf(SERIAL_DELIMITER, startPos);
    if (delimPos == -1) delimPos = line.length();

    String pair = line.substring(startPos, delimPos);
    pair.trim();

    int eqPos = pair.indexOf(SERIAL_KV_SEPARATOR);
    if (eqPos > 0) {
      serialKeys[serialKeyCount] = pair.substring(0, eqPos);
      serialValues[serialKeyCount] = pair.substring(eqPos + 1);
      serialKeys[serialKeyCount].trim();
      serialValues[serialKeyCount].trim();
      serialKeyCount++;
    } else if (pair.length() > 0) {
      // Якщо немає '=', зберігаємо весь фрагмент як значення з індексним ключем
      serialKeys[serialKeyCount] = "d" + String(serialKeyCount);
      serialValues[serialKeyCount] = pair;
      serialKeyCount++;
    }

    startPos = delimPos + 1;
  }

  if (serialKeyCount > 0) {
    newSerialData = true;
  }
}

// ─── Основний цикл прийому Serial ───────────────────────────────────────────
void serialLoop() {
#ifdef ENABLE_SERIAL_BRIDGE
  static String inputBuffer;
  static uint16_t bufIndex = 0;

  while (Serial.available() > 0) {
    char inChar = (char)Serial.read();

    if (inChar == '\r' || inChar == '\n') {
      if (inputBuffer.length() > 0) {
        parseSerialLine(inputBuffer);
        inputBuffer = "";
        bufIndex = 0;
      }
    } else if (bufIndex < SERIAL_BUFFER_SIZE) {
      inputBuffer += inChar;
      bufIndex++;
    } else {
      // Буфер переповнений — скидаємо
      inputBuffer = "";
      bufIndex = 0;
    }
  }
#endif
}

// ─── Відправка команди в Serial ─────────────────────────────────────────────
void serialSendCommand(String cmd) {
#ifdef ENABLE_SERIAL_BRIDGE
  Serial.println(cmd);
#endif
}

