// ============================================================================
//  HomeSamogon_ua2 — Оптимізована версія HomeSamogon_ua1
//  Вся логіка збережена 1:1, але String замінені на char[] буфери
//  для усунення фрагментації heap на ATmega328P (2KB RAM)
// ============================================================================
//  Піни:
//  2  — ТЕМПЕРАТУРА (OneWire DS18B20)
//  3  — БУЗЕР (активний)
//  4  — ENCODER DT
//  5  — ENCODER SW
//  6  — OneWire шина
//  7  — РЕЛЕ КЛАПАНА (пряме)
//  9  — ДАТЧИК ПРОТІЧКИ (вхід)
//  11 — SoftSerial RX
//  12 — SoftSerial TX
//  A0 — ПЕРИФЕРІЯ
//  A1 — РОЗГІН
//  A2 — РОБОТА (VODA)
//  A3 — АВАРІЯ
//  A4 — SDA (I2C)
//  A5 — SCL (I2C)
// ============================================================================

#include <SoftwareSerial.h>
#include <Wire.h>
#include <OneWire.h>
#include <LiquidCrystal_I2C.h>
#include <SFE_BMP180.h>
#include <EEPROM.h>
#include <EncButton.h>
#include <avr/pgmspace.h>

// ─── Піни ────────────────────────────────────────────────────────────────────
#define BUZZER_PIN              3
#define ONE_WIRE_BUS_PIN        6
#define VALVE_RELAY_DIRECT_PIN  A3
#define PERIPHERY_OUTPUT_PIN    A2
#define RAZGON_OUTPUT_PIN       A1
#define AVARIA_OUTPUT_PIN       A0
#define SOFT_SERIAL_TX_PIN      11
#define SOFT_SERIAL_RX_PIN      12
#define RAIN_LEAK_INPUT_PIN     9
#define ENCODER_CLK             2
#define ENCODER_DT              4
#define ENCODER_SW              5

// ─── Константи ──────────────────────────────────────────────────────────────
#define SENSOR_COUNT 3
#define UART_BUF_SIZE 64

// ─── Периферія ──────────────────────────────────────────────────────────────
SFE_BMP180 bmeSensor;
long bmpPressure = 0;
SoftwareSerial BtSerial(SOFT_SERIAL_RX_PIN, SOFT_SERIAL_TX_PIN);
OneWire oneWireBus(ONE_WIRE_BUS_PIN);
LiquidCrystal_I2C mainDisplay(0x27, 16, 2);
EncButton enc(ENCODER_CLK, ENCODER_DT, ENCODER_SW);

// ─── Адреси датчиків ────────────────────────────────────────────────────────
uint8_t tempSensorAddresses[SENSOR_COUNT][8];
const char* tempSensorNames[SENSOR_COUNT] = {"Куб", "Колона", "ТСА"};
byte columnSensorAddr[8];
byte cubeSensorAddr[8];
byte alarmSensorAddr[8];

// ─── Налаштування структури UB ──────────────────────────────────────────────
struct UB_185384122 {
  float uboValue = 0.00;
  bool uboFlag = 0;
  float gtv1Value = 1.00;
  float gtv2Value = 105.00;
};
UB_185384122 ubDataInstance2, ubDataInstance3, ubDataInstance4;
float ubDataUbi = 0.00;

// ─── Режим керування ────────────────────────────────────────────────────────
int controlMode = 0;

// ─── UART буфери (замість String — char[]) ──────────────────────────────────
char uartBuf0[UART_BUF_SIZE];    // Буфер Serial (port 0)
uint8_t uartBuf0Len = 0;
char uartBuf100[UART_BUF_SIZE];  // Буфер BtSerial (port 100)
uint8_t uartBuf100Len = 0;

// ─── Буфери для дисплея та UART передачі ────────────────────────────────────
char displayLowerBuf[20];         // Контрольна сума (нижній рядок)
char tempStr11Buf[20];            // 2-й рядок LCD ("END DISTILLATION", "!STOP AVAR STOP!", тощо)

// ─── Основні змінні ─────────────────────────────────────────────────────────
int dispTempLength = 0;
bool needClearDisplay;
int avlDfu0 = 0;
int avlDfu100 = 0;
bool freeLogicInputArr[2];
bool freeLogicQ1StateArr[] = { 0, 1 };
volatile int powerDownCount = 0;
bool columnSensorFlag = 1;
bool cubeSensorFlag = 1;
float columnTemp;
float cubeTemp;
float alarmTemp;
float atmPressure = 760;
float cubeTempCorrection = 0;
bool pressureCorrectionEnabled = 1;
float vaporSensorTriggerTemp = 65;
int pwmFinishValue = 82;
float cubeFinishTemp = 98;
int finishDelayMs = 30000;
int alarmStopDelayMs = 30000;
float columnPeripheralSwitchTemp = 40;
long peripheralOffDelayMs = 300000;
bool invertPeripheralOutput = 1;
bool invertRazgonOutput = 1;
bool invertWorkOutput = 1;
bool invertAlarmOutput = 0;
bool displayMiddleMode = 1;
int pwmPeriodMs = 4000;
bool alarmFlag = 1;
bool alarmFlag2 = 0;
float alarmTempLimit = 110;
float pressureValue = 0;
float pwmValue1 = 777;
float pwmValue2 = 777;
float pressureSensorValue;
bool tempFlag33 = 0;
int tempInt2 = 0;
bool pwmCoarseFlag = 0;
bool pwmFineFlag = 0;
bool tempFlag12 = 0;
bool finishFlag = 1;
bool finishFlag2;
bool tempFlag28 = 0;
bool tempFlag20 = 1;
bool tempFlag41 = 0;
bool tempFlag29 = 0;
bool tempFlag30 = 0;
bool tempFlag5 = 1;
bool tempFlag31 = 1;
int dispOldLength = 0;
int dispOldLength3 = 0;

// ─── UART прийом/таймери ────────────────────────────────────────────────────
bool rvfu3Reset = 1;
bool rvfu1Reset = 1;
bool triggerFlag2b = 0;
bool triggerFlag2 = 0;
bool triggerFlag3 = 0;
bool triggerFlag3b = 0;
bool pzs2OES = 0;
bool d1b2 = 0;
bool freeLogicQ1 = 0;
float switchValue11;
bool switchFlag4;
bool switchFlag9;
bool switchFlag10;

// ─── Таймери ────────────────────────────────────────────────────────────────
bool timer1Active = 0, timer1Output = 0;
unsigned long timer1PrevMillis = 0UL;
bool timer2Active = 0, timer2Output = 0;
unsigned long timer2PrevMillis = 0UL;
bool timer3Active = 0, timer3Output = 0;
unsigned long timer3PrevMillis = 0UL;
bool timer4Active = 0, timer4Output = 0;
unsigned long timer4PrevMillis = 0UL;
bool timer5Active = 0, timer5Output = 0;
unsigned long timer5PrevMillis = 0UL;
bool timer6Active = 0, timer6Output = 0;
unsigned long timer6PrevMillis = 0UL;
bool timer7Active = 0, timer7Output = 0;
unsigned long timer7PrevMillis = 0UL;
bool timer8Active = 0, timer8Output = 0;
unsigned long timer8PrevMillis = 0UL;
bool timer10Active = 0, timer10Output = 0;
unsigned long timer10PrevMillis = 0UL;

// ─── Генератори ─────────────────────────────────────────────────────────────
bool generator1Active = 0, generator1Output = 0;
unsigned long generator1PrevMillis = 0UL;
bool generator3Active = 0, generator3Output = 0;
unsigned long generator3PrevMillis = 0UL;

// ─── Контроль зміни значень ─────────────────────────────────────────────────
bool changeNumber1Output = 0; int changeNumber1Value;
bool changeNumber2cubeTemp = 0; float changeNumber2Value;
bool changeNumber4Output = 0; float changeNumber4Value;
bool changeNumber5Output = 0; float changeNumber5Value;
bool changeNumber6columnTemp = 0; float changeNumber6Value;
bool changeNumber7atmPressure = 0; float changeNumber7Value;

// ─── Зміна бітів ────────────────────────────────────────────────────────────
bool bitChange1OldState = 0, bitChange1Output = 0;
bool bitChange2OldState = 0, bitChange2Output = 0;
bool bitChange4OldState = 0, bitChange4Output = 0;

// ─── Датчики: час читання ───────────────────────────────────────────────────
unsigned long columnSensorReadTime = 0UL;
float columnSensorValue = 0.00;
unsigned long cubeSensorReadTime = 0UL;
float cubeSensorValue = 0.00;
unsigned long alarmSensorReadTime = 0UL;
float alarmSensorValue = 0.00;
unsigned long bmpSensorReadTime2 = 0UL;
unsigned long stou1 = 0UL, stou2 = 0UL;

// ─── Тимчасові змінні ───────────────────────────────────────────────────────
byte tempByte;
int tempInt;
float tempFloat;

// ─── ШІМ клапана ────────────────────────────────────────────────────────────
unsigned long valvePwmLastTime = 0;
bool valvePwmState = false;

// ═══════════════════════════════════════════════════════════════════════════
//  УТИЛІТИ (оголошення вперед)
// ═══════════════════════════════════════════════════════════════════════════

// ─── Enum'и оголошені тут, ДО функцій, щоб Arduino-препроцесор бачив їх
//     при автогенерації прототипів функцій ─────────────────────────────────
enum MenuValueType : uint8_t { TYPE_INT, TYPE_FLOAT, TYPE_BOOL };
enum MenuSection : uint8_t { MENU_SETUP, MENU_WORK };
enum MainMenuLevel : uint8_t { MAIN_MENU, SUB_MENU };

bool isTimer(unsigned long startTime, unsigned long period);
float readDS18TempOW2(byte addr[8], byte type_s);
float convertDS18x2xData(byte type_s, byte data[12]);
bool checkFreeLogicBlockOutput(bool inArray[], int inArraySize, bool stArray[], int stArraySize);
void funcUB_185384122(struct UB_185384122 *ubInstance, float inputValue);
bool isEEPROMInitialized();
void loadAddresses();
void saveAddresses();
void scanAndSaveSensors();
bool isValidAddress(uint8_t* addr);
void softwarePWM(uint8_t pin, uint8_t value, uint16_t freq_hz, unsigned long &lastTime, bool &state);

// ─── Helper: друк float через Print (без String!) ───────────────────────────
void printFloat(Print &out, float val, uint8_t dec) {
  char buf[12];
  dtostrf(val, 0, dec, buf);
  out.print(buf);
}

// ─── Helper: float округлений до N знаків ───────────────────────────────────
float roundFloat(float val, uint8_t dec) {
  float mult = 1;
  for (uint8_t i = 0; i < dec; i++) mult *= 10;
  return ((int)(val * mult)) / mult;
}

// ═══════════════════════════════════════════════════════════════════════════
//  МЕНЮ (PROGMEM оптимізоване)
// ═══════════════════════════════════════════════════════════════════════════


const char title_klapan[] PROGMEM = "Klapan/motor";
const char title_pwmkonec[] PROGMEM = "PwmKonec";
const char title_kubkonec[] PROGMEM = "KubKonec";
const char title_vodavkl[] PROGMEM = "VodaVkl temp";
const char title_alarm[] PROGMEM = "Alarm Temp Limit";
const char title_water[] PROGMEM = "Water/temp";
const char title_pwmperiod[] PROGMEM = "pwmPeriodMs";
const char title_waterflag[] PROGMEM = "Water Flag";
const char title_shimklapam[] PROGMEM = "shim klapam";

const char* const menuTitles[] PROGMEM = {
  title_klapan, title_pwmkonec, title_kubkonec, title_vodavkl,
  title_alarm, title_water, title_pwmperiod, title_waterflag, title_shimklapam
};
#define MENU_ITEMS_COUNT 9

struct MenuItem {
  uint8_t titleIdx;
  void* valuePtr;
  MenuValueType valueType;
  int16_t step;
  int16_t minVal;
  int16_t maxVal;
  void (*onConfirm)();
  MenuSection section;
};

MenuItem menuItems[MENU_ITEMS_COUNT] = {
  { 0, &controlMode, TYPE_INT, 1, 0, 1, nullptr, MENU_SETUP },
  { 1, &pwmFinishValue, TYPE_INT, 1, 0, 100, nullptr, MENU_SETUP },
  { 2, &cubeFinishTemp, TYPE_FLOAT, 1, 90, 100, nullptr, MENU_SETUP },
  { 3, &columnPeripheralSwitchTemp, TYPE_FLOAT, 1, 30, 100, nullptr, MENU_SETUP },
  { 4, &alarmTempLimit, TYPE_FLOAT, 1, 60, 120, nullptr, MENU_SETUP },
  { 5, &displayMiddleMode, TYPE_BOOL, 1, 0, 1, nullptr, MENU_SETUP },
  { 6, &pwmPeriodMs, TYPE_INT, 250, 1000, 8000, nullptr, MENU_SETUP },
  { 7, &tempFlag29, TYPE_BOOL, 1, 0, 1, nullptr, MENU_WORK },
  { 8, &tempInt2, TYPE_INT, 1, 0, 1000, nullptr, MENU_WORK }
};

const char menuTitleSetup[] PROGMEM = "Setup";
const char menuTitleWork[] PROGMEM = "Work";
const char* const mainMenuTitles[] PROGMEM = { menuTitleSetup, menuTitleWork };

bool encoderMenuActive = false;
MainMenuLevel menuLevel = MAIN_MENU;
uint8_t mainMenuIndex = 0;
uint8_t subMenuIndex = 0;
bool editMode = false;

uint8_t getSectionCount(MenuSection section) {
  uint8_t count = 0;
  for (uint8_t i = 0; i < MENU_ITEMS_COUNT; ++i)
    if (menuItems[i].section == section) count++;
  return count;
}

int8_t getMenuItemIndex(MenuSection section, uint8_t sectionItemIdx) {
  uint8_t idx = 0;
  for (uint8_t i = 0; i < MENU_ITEMS_COUNT; ++i) {
    if (menuItems[i].section == section) {
      if (idx == sectionItemIdx) return i;
      idx++;
    }
  }
  return -1;
}

void getMenuTitle(uint8_t titleIdx, char* buf, uint8_t bufSize) {
  strncpy_P(buf, (PGM_P)pgm_read_ptr(&menuTitles[titleIdx]), bufSize - 1);
  buf[bufSize - 1] = 0;
}

void getMainMenuTitle(uint8_t mainMenuIdx, char* buf, uint8_t bufSize) {
  strncpy_P(buf, (PGM_P)pgm_read_ptr(&mainMenuTitles[mainMenuIdx]), bufSize - 1);
  buf[bufSize - 1] = 0;
}

void displayMenu() {
  mainDisplay.clear();
  char buf[17];
  if (menuLevel == MAIN_MENU) {
    mainDisplay.setCursor(0, 0);
    mainDisplay.print(F("Menu:"));
    mainDisplay.setCursor(0, 1);
    getMainMenuTitle(mainMenuIndex, buf, sizeof(buf));
    mainDisplay.print(buf);
    return;
  }
  int8_t menuItemIndex = getMenuItemIndex(
    mainMenuIndex == 0 ? MENU_SETUP : MENU_WORK, subMenuIndex);
  if (menuItemIndex < 0 || menuItemIndex >= MENU_ITEMS_COUNT) {
    mainDisplay.setCursor(0, 0);
    mainDisplay.print(F("No items"));
    return;
  }
  MenuItem &item = menuItems[menuItemIndex];
  getMenuTitle(item.titleIdx, buf, sizeof(buf));
  mainDisplay.setCursor(0, 0);
  mainDisplay.print(buf);
  if (editMode) mainDisplay.print(F(" [E]"));
  mainDisplay.setCursor(0, 1);
  switch (item.valueType) {
    case TYPE_INT:   mainDisplay.print(*(int*)item.valuePtr); break;
    case TYPE_FLOAT: mainDisplay.print(*(float*)item.valuePtr, 1); break;
    case TYPE_BOOL:  mainDisplay.print(*(bool*)item.valuePtr ? "On" : "Off"); break;
  }
}

void handleEncoderMenu() {
  if (enc.hasClicks(2)) {
    encoderMenuActive = !encoderMenuActive;
    menuLevel = MAIN_MENU;
    editMode = false;
    mainMenuIndex = 0;
    subMenuIndex = 0;
    displayMenu();
    return;
  }
  if (!encoderMenuActive) return;

  if (menuLevel == MAIN_MENU) {
    uint8_t count = 2;
    if (enc.right()) { mainMenuIndex = (mainMenuIndex + 1) % count; displayMenu(); }
    if (enc.left())  { mainMenuIndex = (mainMenuIndex - 1 + count) % count; displayMenu(); }
    if (enc.click()) { menuLevel = SUB_MENU; editMode = false; subMenuIndex = 0; displayMenu(); }
    return;
  }

  MenuSection currentSection = (mainMenuIndex == 0) ? MENU_SETUP : MENU_WORK;
  uint8_t sectionCount = getSectionCount(currentSection);

  if (enc.hold()) { editMode = !editMode; displayMenu(); return; }

  if (!editMode) {
    if (enc.right()) { subMenuIndex = (subMenuIndex + 1) % sectionCount; displayMenu(); }
    if (enc.left())  { subMenuIndex = (subMenuIndex - 1 + sectionCount) % sectionCount; displayMenu(); }
    if (enc.click()) { menuLevel = MAIN_MENU; editMode = false; displayMenu(); }
    return;
  }

  int8_t menuItemIndex = getMenuItemIndex(currentSection, subMenuIndex);
  if (menuItemIndex < 0 || menuItemIndex >= MENU_ITEMS_COUNT) return;
  MenuItem &item = menuItems[menuItemIndex];
  bool changed = false;

  if (enc.right()) {
    switch (item.valueType) {
      case TYPE_INT:
        *(int*)item.valuePtr += (int)item.step;
        if (*(int*)item.valuePtr > (int)item.maxVal) *(int*)item.valuePtr = (int)item.maxVal;
        changed = true; break;
      case TYPE_FLOAT:
        *(float*)item.valuePtr += item.step;
        if (*(float*)item.valuePtr > item.maxVal) *(float*)item.valuePtr = item.maxVal;
        changed = true; break;
      case TYPE_BOOL:
        *(bool*)item.valuePtr = !*(bool*)item.valuePtr;
        changed = true; break;
    }
  }
  if (enc.left()) {
    switch (item.valueType) {
      case TYPE_INT:
        *(int*)item.valuePtr -= (int)item.step;
        if (*(int*)item.valuePtr < (int)item.minVal) *(int*)item.valuePtr = (int)item.minVal;
        changed = true; break;
      case TYPE_FLOAT:
        *(float*)item.valuePtr -= item.step;
        if (*(float*)item.valuePtr < item.minVal) *(float*)item.valuePtr = item.minVal;
        changed = true; break;
    }
  }
  if (changed) displayMenu();
  if (enc.hold()) {
    editMode = false;
    displayMenu();
    if (item.onConfirm) item.onConfirm();
  }
}

// ═══════════════════════════════════════════════════════════════════════════
//  UART ПРИЙОМ (оптимізований — char буфер замість String)
// ═══════════════════════════════════════════════════════════════════════════

void readByteFromUART(byte data, int port) {
  if (port == 0) {
    if (uartBuf0Len < UART_BUF_SIZE - 1) {
      uartBuf0[uartBuf0Len++] = (char)data;
      uartBuf0[uartBuf0Len] = '\0';
    }
  } else if (port == 100) {
    if (uartBuf100Len < UART_BUF_SIZE - 1) {
      uartBuf100[uartBuf100Len++] = (char)data;
      uartBuf100[uartBuf100Len] = '\0';
    }
  }
}

// ═══════════════════════════════════════════════════════════════════════════
//  ДЕКОДУВАННЯ UART КОМАНД (char[] замість String.indexOf/substring)
// ═══════════════════════════════════════════════════════════════════════════

void decodeUartCommand(const char* cmd) {
  const char* p;

  // #value! → tempInt2 (ШІМ клапана)
  p = strchr(cmd, '#');
  if (p) {
    tempInt2 = atoi(p + 1);
  }

  // @value! → alarmTempLimit (температура сигналізації)
  p = strchr(cmd, '@');
  if (p) {
    alarmTempLimit = (int)atof(p + 1);
  }

  // *value% → pwmValue2
  p = strchr(cmd, '*');
  if (p) {
    pressureSensorValue = atmPressure;
    pwmValue2 = atof(p + 1);
  }

  // &value* → pwmValue1
  p = strchr(cmd, '&');
  if (p) {
    pwmValue1 = atof(p + 1);
  }

  // $value& → tempFlag33 (авто режим)
  p = strchr(cmd, '$');
  if (p) {
    char val = *(p + 1);
    if (val == '0') triggerFlag3b = 0;
    if (val == '1') triggerFlag3b = 1;
    tempFlag33 = triggerFlag3b;
  }

  // ^value$ → tempFlag29 (клапан води)
  p = strchr(cmd, '^');
  if (p) {
    char val = *(p + 1);
    if (val == '0') triggerFlag2 = 0;
    if (val == '1') triggerFlag2 = 1;
    tempFlag29 = triggerFlag2;
  }

  // %value~ → displayMiddleMode (центральна позиція дисплея)
  // Оригінал не обробляє %, але зберігаємо на майбутнє
}

// ═══════════════════════════════════════════════════════════════════════════
//  ПЕРЕДАЧА UART ПАКЕТУ (Print напряму, без String конкатенації)
// ═══════════════════════════════════════════════════════════════════════════

void sendDataPacket(Print &out) {
  // Заголовок + температури
  out.print(F("HomeSamogon.ru/4.8,"));
  printFloat(out, columnTemp, 1); out.print(',');
  printFloat(out, atmPressure, 1); out.print(',');
  printFloat(out, cubeTemp, 1); out.print(',');
  // switchString6 (авто режим)
  out.print(tempFlag33 ? '1' : '0'); out.print(',');
  // switchString3 (старт/стоп)
  out.print(tempFlag12 ? '1' : '0'); out.print(',');
  // pwmValue1 - pressureValue
  printFloat(out, pwmValue1 - pressureValue, 1); out.print(',');
  // pwmValue2 - pressureValue
  printFloat(out, pwmValue2 - pressureValue, 1); out.print(',');
  // tempInt2 (ШІМ)
  out.print(tempInt2, DEC); out.print(',');
  // alarmTempLimit
  printFloat(out, alarmTempLimit, 2); out.print(',');
  // displayLowerText (контрольна сума)
  out.print(displayLowerBuf); out.print(',');
  // alarmTemp
  printFloat(out, alarmTemp, 1); out.print(',');
  // switchString2 (протікання/аварія)
  out.print(alarmFlag2 ? '1' : '0'); out.print(',');
  // switchString8 (периферія)
  out.print(tempFlag30 ? '1' : '0');
  // Маркер кінця
  out.print(F(",%,"));
}

// ═══════════════════════════════════════════════════════════════════════════
//  SETUP
// ═══════════════════════════════════════════════════════════════════════════

void setup() {
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(RAIN_LEAK_INPUT_PIN, INPUT_PULLUP);
  pinMode(VALVE_RELAY_DIRECT_PIN, OUTPUT);
  digitalWrite(VALVE_RELAY_DIRECT_PIN, 1);
  pinMode(PERIPHERY_OUTPUT_PIN, OUTPUT);
  digitalWrite(PERIPHERY_OUTPUT_PIN, 0);
  pinMode(RAZGON_OUTPUT_PIN, OUTPUT);
  digitalWrite(RAZGON_OUTPUT_PIN, 0);
  pinMode(AVARIA_OUTPUT_PIN, OUTPUT);
  digitalWrite(AVARIA_OUTPUT_PIN, 0);

  Wire.begin();
  delay(10);
  analogReference(EXTERNAL);
  bmeSensor.begin();  // SFE_BMP180 ініціалізація
  BtSerial.begin(9600);
  Serial.begin(115200);
  mainDisplay.init();
  mainDisplay.noBacklight();
  stou1 = millis();
  stou2 = millis();

  // Ініціалізація буферів
  uartBuf0[0] = '\0';
  uartBuf100[0] = '\0';
  displayLowerBuf[0] = '\0';
  tempStr11Buf[0] = '\0';

  // EEPROM — датчики температури
  if (!isEEPROMInitialized()) {
    scanAndSaveSensors();
  } else {
    loadAddresses();

    bool foundFlags[SENSOR_COUNT] = {false};
    oneWireBus.reset_search();
    uint8_t addr[8];
    while (oneWireBus.search(addr)) {
      if (!isValidAddress(addr)) continue;
      bool matched = false;
      for (byte i = 0; i < SENSOR_COUNT; i++) {
        if (memcmp(addr, tempSensorAddresses[i], 8) == 0) {
          foundFlags[i] = true;
          matched = true;
          break;
        }
      }
      if (!matched) {
        for (byte i = 0; i < SENSOR_COUNT; i++) {
          if (!foundFlags[i]) {
            memcpy(tempSensorAddresses[i], addr, 8);
            foundFlags[i] = true;
            break;
          }
        }
      }
    }
    saveAddresses();
  }

  memcpy(columnSensorAddr, tempSensorAddresses[0], 8);
  memcpy(cubeSensorAddr, tempSensorAddresses[1], 8);
  memcpy(alarmSensorAddr, tempSensorAddresses[2], 8);
}

// ═══════════════════════════════════════════════════════════════════════════
//  LOOP
// ═══════════════════════════════════════════════════════════════════════════

void loop() {
  enc.tick();

  if (needClearDisplay) {
    mainDisplay.clear();
    needClearDisplay = 0;
  }
  powerDownCount = 0;

  // ─── Прийом UART ────────────────────────────────────────────────────────
  if (avlDfu0) {
    avlDfu0 = 0;
  } else {
    if (Serial.available()) {
      avlDfu0 = 1;
      readByteFromUART(Serial.read(), 0);
    }
  }
  if (avlDfu100) {
    avlDfu100 = 0;
  } else {
    if (BtSerial.available()) {
      avlDfu100 = 1;
      readByteFromUART(BtSerial.read(), 100);
    }
  }

  // ─── BMP180 тиск (кожні 5 сек) ─────────────────────────────────────────
  if (isTimer(bmpSensorReadTime2, 5000)) {
    bmpSensorReadTime2 = millis();
    double bmpTempData, bmpPressData;
    tempByte = bmeSensor.startTemperature();
    if (tempByte != 0) {
      delay(tempByte);
      tempByte = bmeSensor.getTemperature(bmpTempData);
      if (tempByte != 0) {
        tempByte = bmeSensor.startPressure(3);
        if (tempByte != 0) {
          delay(tempByte);
          tempByte = bmeSensor.getPressure(bmpPressData, bmpTempData);
          if (tempByte != 0) {
            bmpPressure = (long)(bmpPressData * 100.0); // мбар → Па
          }
        }
      }
    }
  }

  // ─── Скидання UART буферів ──────────────────────────────────────────────
  if (columnSensorFlag) {
    if (!rvfu3Reset) {
      uartBuf0[0] = '\0'; uartBuf0Len = 0;
      rvfu3Reset = 1;
    }
  } else {
    rvfu3Reset = 0;
  }
  if (columnSensorFlag) {
    if (!rvfu1Reset) {
      uartBuf100[0] = '\0'; uartBuf100Len = 0;
      rvfu1Reset = 1;
    }
  } else {
    rvfu1Reset = 0;
  }

  if (avlDfu100 || avlDfu0) {
    tempFlag5 = 1;
  }
  cubeSensorFlag = (avlDfu100 || avlDfu0);

  // ─── Таймер 10 (UART таймаут) ──────────────────────────────────────────
  if (avlDfu100 || avlDfu0) {
    timer10Output = 1;
    timer10Active = 1;
  } else {
    if (timer10Active) {
      timer10Active = 0;
      timer10PrevMillis = millis();
    } else {
      if (timer10Output) {
        if (isTimer(timer10PrevMillis, 1000)) timer10Output = 0;
      }
    }
  }
  columnSensorFlag = !(timer10Output);

  if (avlDfu0) triggerFlag2b = 1;
  if (avlDfu100) triggerFlag2b = 0;

  // ─── Вибір UART буфера та декодування ──────────────────────────────────
  const char* cmdBuf = triggerFlag2b ? uartBuf0 : uartBuf100;

  if (cubeSensorFlag == 1) {
    decodeUartCommand(cmdBuf);
    cubeSensorFlag = 0;
  }

  // ─── Датчики температури ───────────────────────────────────────────────
  // Колона (кожні 5 сек)
  if (isTimer(columnSensorReadTime, 5000)) {
    columnSensorReadTime = millis();
    tempFloat = readDS18TempOW2(columnSensorAddr, 0);
    if (tempFloat < 500) columnSensorValue = tempFloat;
  }
  ubDataUbi = columnSensorValue;
  funcUB_185384122(&ubDataInstance2, ubDataUbi);
  if (ubDataInstance2.uboFlag && !(columnSensorValue == 85)) {
    columnTemp = (int(10 * ubDataInstance2.uboValue)) / 10.00;
  }

  // Контроль зміни columnTemp
  if (changeNumber6columnTemp) {
    changeNumber6columnTemp = 0;
  } else {
    tempFloat = columnTemp;
    if (tempFloat != changeNumber6Value) {
      changeNumber6Value = tempFloat;
      changeNumber6columnTemp = 1;
    }
  }

  // Контроль зміни atmPressure
  if (changeNumber7atmPressure) {
    changeNumber7atmPressure = 0;
  } else {
    tempFloat = atmPressure;
    if (tempFloat != changeNumber7Value) {
      changeNumber7Value = tempFloat;
      changeNumber7atmPressure = 1;
    }
  }

  // Контроль зміни cubeTemp
  if (changeNumber2cubeTemp) {
    changeNumber2cubeTemp = 0;
  } else {
    tempFloat = cubeTemp;
    if (tempFloat != changeNumber2Value) {
      changeNumber2Value = tempFloat;
      changeNumber2cubeTemp = 1;
    }
  }

  if (changeNumber6columnTemp || changeNumber7atmPressure || changeNumber2cubeTemp) {
    tempFlag5 = 1;
  }

  // Куб (кожні 5 сек)
  if (isTimer(cubeSensorReadTime, 5000)) {
    cubeSensorReadTime = millis();
    tempFloat = readDS18TempOW2(cubeSensorAddr, 0);
    if (tempFloat < 500) cubeSensorValue = tempFloat;
  }
  ubDataUbi = cubeSensorValue;
  funcUB_185384122(&ubDataInstance3, ubDataUbi);
  if (ubDataInstance3.uboFlag && !(cubeSensorValue == 85)) {
    cubeTemp = cubeTempCorrection + ubDataInstance3.uboValue;
  }

  // Аварійний (кожні 5 сек)
  if (isTimer(alarmSensorReadTime, 5000)) {
    alarmSensorReadTime = millis();
    tempFloat = readDS18TempOW2(alarmSensorAddr, 0);
    if (tempFloat < 500) alarmSensorValue = tempFloat;
  }
  ubDataUbi = alarmSensorValue;
  funcUB_185384122(&ubDataInstance4, ubDataUbi);
  if (ubDataInstance4.uboFlag && !(alarmSensorValue == 85)) {
    alarmTemp = ubDataInstance4.uboValue;
  }

  // Тиск
  atmPressure = bmpPressure / 133.3;

  // ─── Формування displayLowerBuf (контрольна сума) ──────────────────────
  if (tempFlag5 == 1) {
    // Оригінал: (round(cubeT,1)+3.14) * (round(colT,1)+round(atm,1))
    float ct1 = roundFloat(cubeTemp, 1);
    float colt1 = roundFloat(columnTemp, 1);
    float atm1 = roundFloat(atmPressure, 1);
    float lowerVal = (ct1 + 3.14f) * (colt1 + atm1);
    dtostrf(lowerVal, 0, 1, displayLowerBuf);
  }

  // ─── Аварійні прапорці ─────────────────────────────────────────────────
  alarmFlag2 = ((!digitalRead(RAIN_LEAK_INPUT_PIN)) || (alarmTemp > vaporSensorTriggerTemp));

  // ─── Передача в BtSerial (кожні 2 сек) ────────────────────────────────
  if (columnSensorFlag) {
    if (isTimer(stou1, 2000)) {
      sendDataPacket(BtSerial);
      stou1 = millis();
    }
  } else {
    stou1 = millis();
  }

  // ─── Передача в Serial (кожні 2 сек) ──────────────────────────────────
  if (columnSensorFlag) {
    if (isTimer(stou2, 2000)) {
      sendDataPacket(Serial);
      stou2 = millis();
    }
  } else {
    stou2 = millis();
  }

  // ─── Зміщення температури при зміні тиску ─────────────────────────────
  if (pressureCorrectionEnabled == 1) {
    if (changeNumber4Output) {
      changeNumber4Output = 0;
    } else {
      tempFloat = atmPressure;
      if (tempFloat != changeNumber4Value) {
        changeNumber4Value = tempFloat;
        changeNumber4Output = 1;
      }
    }
    if (changeNumber4Output) {
      pressureValue = (pressureSensorValue - atmPressure) * 0.035;
    }
  }

  // ─── ШІМ ──────────────────────────────────────────────────────────────
  freeLogicInputArr[0] = tempInt2 <= 0;
  freeLogicInputArr[1] = tempInt2 >= 1023;
  freeLogicQ1 = checkFreeLogicBlockOutput(freeLogicInputArr, 2, freeLogicQ1StateArr, 2);

  if (!(tempInt2 <= 0 || tempInt2 >= 1023)) {
    if (!generator3Active) {
      generator3Active = 1;
      generator3Output = 1;
      generator3PrevMillis = millis();
    }
  } else {
    generator3Active = 0;
    generator3Output = 0;
  }

  if (generator3Active) {
    if (generator3Output) {
      if (isTimer(generator3PrevMillis, map(tempInt2, 0, 1023, 0, pwmPeriodMs))) {
        generator3PrevMillis = millis();
        generator3Output = 0;
      }
    } else {
      if (isTimer(generator3PrevMillis, pwmPeriodMs - map(tempInt2, 0, 1023, 0, pwmPeriodMs))) {
        generator3PrevMillis = millis();
        generator3Output = 1;
      }
    }
  }

  if ((tempInt2 <= 0) || (tempInt2 >= 1023)) {
    switchFlag4 = freeLogicQ1;
  } else {
    switchFlag4 = generator3Output;
  }
  pwmCoarseFlag = switchFlag4;
  pwmFineFlag = ((tempInt2 <= 0) || (tempInt2 >= 1023));

  // ─── Динамічний ШІМ / Клапан ──────────────────────────────────────────
  if (bitChange4Output) {
    bitChange4Output = 0;
  } else {
    if (!(bitChange4OldState == tempFlag12)) {
      bitChange4OldState = tempFlag12;
      bitChange4Output = 1;
    }
  }
  if (bitChange4Output) {
    tempInt2 = tempInt2 * 19 / 20; // Оптимізовано: було map(x, 0, x, 0, x - x/20)
  }

  if (columnTemp >= pwmValue2 - (pwmValue2 - pwmValue1)) triggerFlag3 = 1;
  if (columnTemp <= pwmValue2) triggerFlag3 = 0;

  if (triggerFlag3) {
    if (timer1Active) {
      if (isTimer(timer1PrevMillis, 5000)) timer1Output = 1;
    } else {
      timer1Active = 1;
      timer1PrevMillis = millis();
    }
  } else {
    timer1Output = 0;
    timer1Active = 0;
  }
  tempFlag12 = !timer1Output;

  if (pwmFineFlag) {
    switchFlag9 = pwmCoarseFlag;
  } else {
    switchFlag9 = ((!timer1Output) && pwmCoarseFlag);
  }

  // Керування клапаном
  uint8_t pwmValue = map(tempInt2, 0, 1023, 0, 255);
  if (controlMode == 1) {
    softwarePWM(VALVE_RELAY_DIRECT_PIN, pwmValue, 100, valvePwmLastTime, valvePwmState);
  } else {
    digitalWrite(VALVE_RELAY_DIRECT_PIN, !(switchFlag9));
    valvePwmState = false;
  }

  // ─── Зумер ────────────────────────────────────────────────────────────
  if (((cubeTemp >= alarmTempLimit) || (alarmTemp > vaporSensorTriggerTemp))
      || ((!digitalRead(RAIN_LEAK_INPUT_PIN)) || (!alarmFlag))) {
    if (timer5Active) {
      if (isTimer(timer5PrevMillis, 5000)) timer5Output = 1;
    } else {
      timer5Active = 1;
      timer5PrevMillis = millis();
    }
  } else {
    timer5Output = 0;
    timer5Active = 0;
  }

  if (timer5Output) {
    if (!generator1Active) {
      generator1Active = 1;
      generator1Output = 1;
      generator1PrevMillis = millis();
    }
  } else {
    generator1Active = 0;
    generator1Output = 0;
  }

  if (generator1Active) {
    if (generator1Output) {
      if (isTimer(generator1PrevMillis, 200)) {
        generator1PrevMillis = millis();
        generator1Output = 0;
      }
    } else {
      if (isTimer(generator1PrevMillis, 400)) {
        generator1PrevMillis = millis();
        generator1Output = 1;
      }
    }
  }

  if (generator1Output) {
    if (!pzs2OES) { digitalWrite(BUZZER_PIN, HIGH); pzs2OES = 1; }
  } else {
    if (pzs2OES) { digitalWrite(BUZZER_PIN, LOW); pzs2OES = 0; }
  }

  if (!generator1Output) {
    if (!d1b2) { mainDisplay.backlight(); d1b2 = 1; }
  } else {
    if (d1b2) { mainDisplay.noBacklight(); d1b2 = 0; }
  }

  // ─── Розгін, Робота, Периферія, Аварія ────────────────────────────────
  // Таймер 3 (розгін)
  if ((!finishFlag2) && finishFlag) {
    timer3Output = 1; timer3Active = 1;
  } else {
    if (timer3Active) {
      timer3Active = 0; timer3PrevMillis = millis();
    } else {
      if (timer3Output) {
        if (isTimer(timer3PrevMillis, finishDelayMs)) timer3Output = 0;
      }
    }
  }
  if (finishFlag) finishFlag = timer3Output;

  // Таймер 7 (робота)
  if ((!finishFlag2) && finishFlag) {
    if (timer7Active) {
      if (isTimer(timer7PrevMillis, 10000)) timer7Output = 1;
    } else {
      timer7Active = 1; timer7PrevMillis = millis();
    }
  } else {
    timer7Output = 0; timer7Active = 0;
  }
  tempFlag28 = (timer7Output && (!alarmFlag2));

  // Таймер 4 (периферія — аварійна зупинка)
  if ((!alarmFlag2) && alarmFlag) {
    timer4Output = 1; timer4Active = 1;
  } else {
    if (timer4Active) {
      timer4Active = 0; timer4PrevMillis = millis();
    } else {
      if (timer4Output) {
        if (isTimer(timer4PrevMillis, alarmStopDelayMs)) timer4Output = 0;
      }
    }
  }
  if (alarmFlag) alarmFlag = timer4Output;

  // Таймер 2 (аварія по температурі колони)
  if ((!alarmFlag2) && (!(columnTemp >= columnPeripheralSwitchTemp))) {
    if (timer2Active) {
      if (isTimer(timer2PrevMillis, 20000)) timer2Output = 1;
    } else {
      timer2Active = 1; timer2PrevMillis = millis();
    }
  } else {
    timer2Output = 0; timer2Active = 0;
  }
  tempFlag41 = (timer2Output && alarmFlag);

  if (columnTemp >= columnPeripheralSwitchTemp) {
    switchFlag10 = true;
  } else {
    switchFlag10 = tempFlag29;
  }

  // Таймер 8 (затримка периферії)
  if (finishFlag) {
    timer8Output = 1; timer8Active = 1;
  } else {
    if (timer8Active) {
      timer8Active = 0; timer8PrevMillis = millis();
    } else {
      if (timer8Output) {
        if (isTimer(timer8PrevMillis, peripheralOffDelayMs)) timer8Output = 0;
      }
    }
  }

  tempFlag30 = (switchFlag10 && alarmFlag && timer8Output);

  // Виходи
  digitalWrite(PERIPHERY_OUTPUT_PIN, (tempFlag30 ^ invertPeripheralOutput));
  digitalWrite(RAZGON_OUTPUT_PIN, (tempFlag41 ^ invertRazgonOutput));
  digitalWrite(AVARIA_OUTPUT_PIN, (alarmFlag ^ invertAlarmOutput));

  // ─── Дисплей: рядок 1 (температури) ───────────────────────────────────
  if (tempFlag5 == 1) {
    if (displayMiddleMode) {
      switchValue11 = alarmTemp;
    } else {
      switchValue11 = atmPressure;
    }

    char t1[7], t2[7], t3[7], dispLineBuf[20];
    dtostrf(columnTemp, 0, 1, t1);
    dtostrf(switchValue11, 0, 1, t2);
    dtostrf(cubeTemp, 0, 1, t3);
    snprintf(dispLineBuf, sizeof(dispLineBuf), "%s %s %s", t1, t2, t3);
    dispTempLength = strlen(dispLineBuf);

    if (dispOldLength3 > dispTempLength) needClearDisplay = 1;
    dispOldLength3 = dispTempLength;

    mainDisplay.setCursor((16 - dispTempLength) / 2, 0);
    if (!encoderMenuActive) {
      mainDisplay.print(dispLineBuf);
    }
    tempFlag5 = 0;
  }

  // ─── Контроль оновлення значень для рядка 2 ───────────────────────────
  if (changeNumber5Output) {
    changeNumber5Output = 0;
  } else {
    tempFloat = alarmTemp;
    if (tempFloat != changeNumber5Value) { changeNumber5Value = tempFloat; changeNumber5Output = 1; }
  }
  if (changeNumber1Output) {
    changeNumber1Output = 0;
  } else {
    tempInt = tempInt2;
    if (tempInt != changeNumber1Value) { changeNumber1Value = tempInt; changeNumber1Output = 1; }
  }
  if (bitChange1Output) {
    bitChange1Output = 0;
  } else {
    if (!(bitChange1OldState == tempFlag12)) { bitChange1OldState = tempFlag12; bitChange1Output = 1; }
  }
  if (bitChange2Output) {
    bitChange2Output = 0;
  } else {
    if (!(bitChange2OldState == tempFlag33)) { bitChange2OldState = tempFlag33; bitChange2Output = 1; }
  }

  if ((changeNumber5Output || changeNumber1Output) || (bitChange1Output || bitChange2Output)) {
    timer6Output = 1; timer6Active = 1;
  } else {
    if (timer6Active) {
      timer6Active = 0; timer6PrevMillis = millis();
    } else {
      if (timer6Output) {
        if (isTimer(timer6PrevMillis, 100)) timer6Output = 0;
      }
    }
  }
  if (timer6Output) tempFlag31 = 1;

  // ─── Рядок 2: Автоматичний режим ──────────────────────────────────────
  if (tempFlag33 == 1) {
    const char* modeStr = tempFlag12 ? "START" : "STOP ";
    if (alarmFlag) {
      char t1[8], t2[6];
      dtostrf(tempInt2 / 10.23, 0, 1, t1);
      dtostrf(alarmTemp, 0, 0, t2);
      snprintf(tempStr11Buf, sizeof(tempStr11Buf), "%s%% %s %s", t1, modeStr, t2);
    }
    finishFlag2 = ((tempInt2 <= pwmFinishValue) && (tempInt2 >= 16));
  }

  // ─── Рядок 2: Ручний режим ───────────────────────────────────────────
  if (tempFlag33 == 0) {
    if (tempInt2 >= 2) {
      if (alarmFlag) {
        char t1[8], t2[6];
        dtostrf(tempInt2 / 10.23, 0, 1, t1);
        dtostrf(alarmTemp, 0, 0, t2);
        snprintf(tempStr11Buf, sizeof(tempStr11Buf), "%s%% Manual %s", t1, t2);
      }
    } else {
      if (alarmFlag) {
        strncpy(tempStr11Buf, "realKraft.ua", sizeof(tempStr11Buf) - 1);
        tempStr11Buf[sizeof(tempStr11Buf) - 1] = '\0';
      }
    }
    finishFlag2 = (cubeTemp >= cubeFinishTemp);
  }

  // ─── Перегонка завершена ──────────────────────────────────────────────
  if (finishFlag == 0) {
    if (strcmp(tempStr11Buf, "END DISTILLATION") != 0) {
      tempFlag31 = 1;
      strncpy(tempStr11Buf, "END DISTILLATION", sizeof(tempStr11Buf) - 1);
      tempStr11Buf[sizeof(tempStr11Buf) - 1] = '\0';
    }
    tempFlag33 = 0;
    alarmTempLimit = 110.00;
    tempInt2 = 0;
    pwmCoarseFlag = 0;
    pwmFineFlag = 0;
  }

  // ─── Аварійна зупинка ─────────────────────────────────────────────────
  if (alarmFlag == 0) {
    if (strcmp(tempStr11Buf, "!STOP AVAR STOP!") != 0) {
      tempFlag31 = 1;
      strncpy(tempStr11Buf, "!STOP AVAR STOP!", sizeof(tempStr11Buf) - 1);
      tempStr11Buf[sizeof(tempStr11Buf) - 1] = '\0';
    }
    tempFlag33 = 0;
    alarmTempLimit = 110.00;
    tempInt2 = 0;
    pwmCoarseFlag = 0;
    pwmFineFlag = 0;
    tempFlag20 = 0;
    tempFlag30 = 0;
    finishFlag = 0;
  }

  // ─── Дисплей: рядок 2 ─────────────────────────────────────────────────
  if (tempFlag31 == 1) {
    dispTempLength = strlen(tempStr11Buf);
    if (dispOldLength > dispTempLength) needClearDisplay = 1;
    dispOldLength = dispTempLength;
    mainDisplay.setCursor((16 - dispTempLength) / 2, 1);
    if (!encoderMenuActive) {
      mainDisplay.print(tempStr11Buf);
    }
    tempFlag31 = 0;
  }

  handleEncoderMenu();
}

// ═══════════════════════════════════════════════════════════════════════════
//  УТИЛІТИ
// ═══════════════════════════════════════════════════════════════════════════

// Виправлений таймер (коректний overflow millis())
bool isTimer(unsigned long startTime, unsigned long period) {
  return (millis() - startTime) >= period;
}

ISR(TIMER2_OVF_vect) {
  TCNT2 = 100;
  if (powerDownCount >= 1000) {
    asm volatile("jmp 0x0000");
  } else {
    powerDownCount++;
  }
}

float convertDS18x2xData(byte type_s, byte data[12]) {
  int16_t raw = (data[1] << 8) | data[0];
  if (type_s) {
    raw = raw << 3;
    if (data[7] == 0x10) {
      raw = (raw & 0xFFF0) + 12 - data[6];
    }
  } else {
    byte cfg = (data[4] & 0x60);
    if (cfg == 0x00) raw = raw & ~7;
    else if (cfg == 0x20) raw = raw & ~3;
    else if (cfg == 0x40) raw = raw & ~1;
  }
  return (float)raw / 16.0;
}

float readDS18TempOW2(byte addr[8], byte type_s) {
  byte data[12];
  oneWireBus.reset();
  oneWireBus.select(addr);
  oneWireBus.write(0xBE);
  for (byte i = 0; i < 9; i++) {
    data[i] = oneWireBus.read();
  }
  oneWireBus.reset();
  oneWireBus.select(addr);
  oneWireBus.write(0x44);
  if (oneWireBus.crc8(data, 8) != data[8]) return 501;
  return convertDS18x2xData(type_s, data);
}

bool checkFreeLogicBlockOutput(bool inArray[], int inArraySize, bool stArray[], int stArraySize) {
  int inIndex = 0;
  bool result = 1;
  for (int i = 0; i < stArraySize; i++) {
    if (!(inArray[inIndex] == stArray[i])) result = 0;
    inIndex++;
    if (inIndex == inArraySize) {
      if (result) return 1;
      result = 1;
      inIndex = 0;
    }
  }
  return 0;
}

void funcUB_185384122(struct UB_185384122 *ubInstance, float inputValue) {
  ubInstance->uboValue = inputValue;
  ubInstance->uboFlag = ((ubInstance->gtv1Value < inputValue) && (inputValue < ubInstance->gtv2Value));
}

// ─── EEPROM ─────────────────────────────────────────────────────────────────

bool isEEPROMInitialized() {
  return EEPROM.read(0) == 1;
}

void loadAddresses() {
  for (byte i = 0; i < SENSOR_COUNT; i++) {
    for (byte j = 0; j < 8; j++) {
      tempSensorAddresses[i][j] = EEPROM.read(1 + i * 8 + j);
    }
  }
}

void saveAddresses() {
  EEPROM.write(0, 1);
  for (byte i = 0; i < SENSOR_COUNT; i++) {
    for (byte j = 0; j < 8; j++) {
      EEPROM.write(1 + i * 8 + j, tempSensorAddresses[i][j]);
    }
  }
}

void scanAndSaveSensors() {
  oneWireBus.reset_search();
  uint8_t addr[8];
  byte i = 0;
  while (oneWireBus.search(addr) && i < SENSOR_COUNT) {
    if (isValidAddress(addr)) {
      memcpy(tempSensorAddresses[i], addr, 8);
      i++;
    }
  }
  saveAddresses();
}

bool isValidAddress(uint8_t* addr) {
  return (OneWire::crc8(addr, 7) == addr[7]) && (addr[0] == 0x28);
}

void softwarePWM(uint8_t pin, uint8_t value, uint16_t freq_hz, unsigned long &lastTime, bool &state) {
  unsigned long period = 1000000UL / freq_hz;
  unsigned long highTime = (period * value) / 255;
  unsigned long lowTime = period - highTime;
  unsigned long now = micros();
  if (state) {
    if (now - lastTime >= highTime) {
      digitalWrite(pin, LOW);
      lastTime = now;
      state = false;
    }
  } else {
    if (now - lastTime >= lowTime) {
      digitalWrite(pin, HIGH);
      lastTime = now;
      state = true;
    }
  }
}

