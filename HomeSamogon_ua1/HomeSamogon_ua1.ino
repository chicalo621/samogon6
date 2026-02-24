//ось зкаінчений добрий файл який працює  запамятай
 /*піни
2 ТЕМПЕРАТУРА
4
6 БУЗЕР ПЕРЕНЕСЕНО З 4
7 РЕЛЕ КЛАПАНА ВІДБОРУ ПРЯМЕ
8 РЕЛЕ КЛАПАНА ВІДБОРУ ІНВЕРСНЕ
9 ДАТЧИК ДОЩУ ПРОТІЧКИ ВХЫДНИЙ ЦИФРА
10
11 НА ПЕРЕМИЧКУ 5 RXD
12 НА ПЕРЕМИЧКУ 4 TXD
14 A0 ПЕРЕФЕРіЯ
15 A1 РОЗГіН
16 А2 РОБОТА
17 А3 АВАРіЯ
18 a4 sda
19 a5 scl
20
*/

#include <SoftwareSerial.h>
#include <Wire.h>
#include <OneWire.h>
#include <LiquidCrystal_I2C.h>
//include <SFE_BMP180.h>
#include <GyverBME280.h>  // Підключення бібліотеки (_bmp085)

#include <EEPROM.h>
//#define ONE_WIRE_BUS 6
#define SENSOR_COUNT 3
GyverBME280 bmeSensor; // Сенсор тиску та температури (_bmp085)
//SFE_BMP180  _bmp085;
long bmpPressure = 0; // Поточний тиск з BMP датчика (_bmp085P)
SoftwareSerial BtSerial(12, 11); // Програмний UART для відлагодження (Serial100)
// --- Визначення пінів (на основі коду) ---
#define BUZZER_PIN              3//  5  // Пін для активного зумера
#define ONE_WIRE_BUS_PIN        6       // Пін для шини датчиків DS18B20
#define VALVE_RELAY_DIRECT_PIN  A3      // Пін реле клапана відбору (пряма логіка в коді)
#define PERIPHERY_OUTPUT_PIN    A2      // Пін виходу "Периферія" VODA
#define RAZGON_OUTPUT_PIN       A1      // Пін виходу "Розгін"
#define AVARIA_OUTPUT_PIN       A0      // Пін виходу "Аварія"
#define SOFT_SERIAL_TX_PIN      11      // Пін TX для SoftwareSerial
#define SOFT_SERIAL_RX_PIN      12      // Пін RX для SoftwareSerial
#define RAIN_LEAK_INPUT_PIN     9       // Вхід датчика протікання/дощу (A3)
#define RABOTA_OUTPUT_UNUSED_PIN 10     // Пін виходу "Робота" (A2) - логіка є, але digitalWrite закоментовано

#include <EncButton.h>
// Енкодер
#define ENCODER_CLK 2
#define ENCODER_DT 4
#define ENCODER_SW 5


//#define MODE_VALVE 0
int controlMode = 0; // або призначення режиму десь у логіці

OneWire oneWireBus(ONE_WIRE_BUS_PIN); // OneWire-шина для температурних датчиків (oneWire)
uint8_t tempSensorAddresses[SENSOR_COUNT][8]; // Масив адрес температурних датчиків (sensorAddresses)
const char* tempSensorNames[SENSOR_COUNT] = {"Куб", "Колона", "ТСА"}; // Імена температурних датчиків (sensorNames)

// --- Енкодер на EncButton ---

EncButton enc(ENCODER_CLK, ENCODER_DT, ENCODER_SW);
/*byte columnSensorAddr[8]={0x28, 0xFF, 0x64, 0xE, 0x71, 0xBE, 0xDF, 0xDF}; // Датчик Колони (_d18x2x1Addr)
byte cubeSensorAddr[8]={0x28, 0xFF, 0x64, 0xE, 0x6B, 0x31, 0xCA, 0x6A}; // Датчик Куба (_d18x2x2Addr)
byte alarmSensorAddr[8]={0x28, 0xFF, 0x64, 0xE, 0x6A, 0x14, 0x2C, 0xCB}; // Датчик Аварії (_d18x2x3Addr)
*///28 A5 5 81 E3 E1 3C A2
byte alarmSensorAddr[8]; // Адреса датчика аварії (_d18x2x3Addr)
byte columnSensorAddr[8]; // Адреса датчика колони (_d18x2x1Addr)
byte cubeSensorAddr[8];   // Адреса датчика куба (_d18x2x2Addr)
//byte alarmSensorAddr[8] = { 0x28, 0x6C, 0x16, 0xC5, 0x40, 0x21, 0x06, 0x11 };  // Датчик Аварії (_d18x2x3Addr)

LiquidCrystal_I2C mainDisplay(0x27, 16, 2); // Основний LCD дисплей (_lcd1)
int dispTempLength = 0; // Довжина рядка температури для відображення (_dispTempLength1)
boolean needClearDisplay; // Прапорець, чи потрібно очистити дисплей (_isNeedClearDisp1)
struct UB_185384122 { // Структура для збереження налаштувань (UB_185384122)
  float uboValue = 0.00; // Значення налаштувань (_ubo_120270494)
  bool uboFlag = 0; // Прапорець налаштувань (_ubo_13416262)
  float gtv1Value = 1.00; // Налаштування 1 (_gtv1)
  float gtv2Value = 105.00; // Налаштування 2 (_gtv2)
};
UB_185384122 ubDataInstance2; // Екземпляр налаштувань 2 (UB_185384122_Instance2)
UB_185384122 ubDataInstance3; // Екземпляр налаштувань 3 (UB_185384122_Instance3)
UB_185384122 ubDataInstance4; // Екземпляр налаштувань 4 (UB_185384122_Instance4)
float ubDataUbi = 0.00; // Додаткове налаштування (UB_185384122_ubi_117131038)
//OneWire oneWire(6);  // шина датчиов пин 2
int avlDfu0 = 0; // Стан DFU0 (_AvlDFU0)
int avlDfu100 = 0; // Стан DFU100 (_AvlDFU100)
bool freeLogicInputArr[2]; // Масив вхідних сигналів для блоку вільної логіки (_FreeLog1_IArr)
bool freeLogicQ1StateArr[] = { 0, 1 }; // Стан для вільної логіки (_FreeLog1_Q1_StArr)
volatile int powerDownCount = 0; // Лічильник вимкнень живлення (_PWDC)
bool columnSensorFlag = 1; // Прапорець датчика колони (_gtv1)
bool cubeSensorFlag = 1; // Прапорець датчика куба (_gtv32)
String tempString13; // Строка для тимчасових даних (_gtv13)
float columnTemp; // Температура колони (_gtv9)
float cubeTemp; // Температура куба (_gtv14)
float alarmTemp; // Температура аварійного датчика (_gtv15)
float atmPressure = 760; // Атмосферний тиск (_gtv17)
float cubeTempCorrection = 0; // Корекція температури куба (_gtv22)
bool pressureCorrectionEnabled = 1; // Корекція температури Старт-Стоп по зміні атмосферного тиску (_gtv23)
float vaporSensorTriggerTemp = 65; // Температура спрацювання датчика пара (_gtv21)
int pwmFinishValue = 82; // Значення ШИМ при завершенні перегонки (_gtv42)
float cubeFinishTemp = 98; // Температура куба при завершенні перегонки (_gtv43)
int finishDelayMs = 30000; // Затримка завершення перегонки (мс) (_gtv40)
int alarmStopDelayMs = 30000; // Затримка перед аварійною зупинкою (мс) (_gtv27)
float columnPeripheralSwitchTemp = 40; // Температура перемикання периферії (колона) (_gtv19)
long peripheralOffDelayMs = 300000; // Затримка вимкнення периферії після перегонки (мс) (_gtv38)
bool invertPeripheralOutput = 1; // Інверсія виходу "Периферія" (_gtv35)
bool invertRazgonOutput = 1; // Інверсія виходу "Розгін" (_gtv34)
bool invertWorkOutput = 1; // Інверсія виходу "Робота" (_gtv36)
bool invertAlarmOutput = 0; // Інверсія виходу "Аварія" (_gtv37)
bool displayMiddleMode = 1; // Що виводимо в середню позицію дисплея (_gtv46)
int pwmPeriodMs = 4000; // Період одного циклу ШИМ (мс) (_gtv45)
String displayLowerText; // Нижня частина екрану (_gtv16)
String displayUpperText; // Верхня частина екрану (_gtv10)
bool alarmFlag = 1; // Признак аварії (_gtv24)
bool alarmFlag2 = 0; // Додатковий признак аварії (_gtv25)
float alarmTempLimit = 110; // Межа аварійної температури (_gtv8)
float pressureValue = 0; // Поточний тиск (_gtv26)
float pwmValue1 = 777; // Значення ШИМ 1 (_gtv6)
float pwmValue2 = 777; // Значення ШИМ 2 (_gtv7)
float pressureSensorValue; // Значення з датчика тиску (_gtv18)
bool tempFlag33 = 0; // Прапорець 33 (_gtv33)
int tempInt2 = 0; // Цілочисельне значення 2 (_gtv2)
bool pwmCoarseFlag = 0; // Прапорець грубого ШИМу (_gtv3)
bool pwmFineFlag = 0; // Прапорець дрібного ШИМу (_gtv4)
bool tempFlag12 = 0; // Прапорець 12 (_gtv12)
bool finishFlag = 1; // Кінець перегонки (_gtv39)
bool finishFlag2; // Додатковий прапорець завершення (_gtv44)
bool tempFlag28 = 0; // Прапорець 28 (_gtv28)
bool tempFlag20 = 1; // Прапорець 20 (_gtv20)
bool tempFlag41 = 0; // Прапорець 41 (_gtv41)
bool tempFlag29 = 0; // Прапорець 29 (_gtv29)
bool tempFlag30 = 0; // Прапорець 30 (_gtv30)
bool tempFlag5 = 1; // Прапорець 5 (_gtv5)
bool tempFlag31 = 1; // Прапорець 31 (_gtv31)
String tempString11; // Додаткова строка (_gtv11)
String switchString1; // Тимчасова строка для перемикача (_swi1)
int dispOldLength = 0; // Стара довжина дисплея (_disp1oldLength)
String switchString6; // Тимчасова строка для перемикача 6 (_swi6)
int fsfs4io = 0; // IO параметр FSFS4 (_FSFS4IO)
bool fsfs4co = 0; // CO параметр FSFS4 (_FSFS4CO)
bool timer8Active = 0; // Прапорець активності таймера 8 (_tim8I)
bool timer8Output = 0; // Вихід таймера 8 (_tim8O)
unsigned long timer8PrevMillis = 0UL; // Попередній час таймера 8 (_tim8P)
bool changeNumber1Output = 0; // Вихід для зміни числа 1 (_changeNumber1_Out)
int changeNumber1Value; // Значення для зміни числа 1 (_changeNumber1_OLV)
bool timer10Active = 0; // Прапорець активності таймера 10 (_tim10I)
bool timer10Output = 0; // Вихід таймера 10 (_tim10O)
unsigned long timer10PrevMillis = 0UL; // Попередній час таймера 10 (_tim10P)
String gsfs8 = "0"; // Тимчасова строка GSFS8 (_GSFS8)
String rvfu3Data; // Дані RVFU3 (_RVFU3Data)
bool rvfu3Reset = 1; // Скидання RVFU3 (_RVFU3Reset)
bool freeLogicQ1 = 0; // Стан для вільної логіки Q1 (_FreeLog1_Q1)
bool timer2Active = 0; // Прапорець активності таймера 2 (_tim2I)
bool timer2Output = 0; // Вихід таймера 2 (_tim2O)
unsigned long timer2PrevMillis = 0UL; // Попередній час таймера 2 (_tim2P)
String gsfs17 = "0"; // Тимчасова строка GSFS17 (_GSFS17)
bool switchFlag10; // Прапорець перемикача 10 (_swi10)
unsigned long columnSensorReadTime = 0UL; // Час останнього читання датчика колони (_d18x2x1Tti)
float columnSensorValue = 0.00; // Значення датчика колони (_d18x2x1O)
bool triggerFlag3 = 0; // Тригер 3 (_trgs3)
int fsfs8io = 0; // IO параметр FSFS8 (_FSFS8IO)
bool fsfs8co = 0; // CO параметр FSFS8 (_FSFS8CO)
bool changeNumber2cubeTemp = 0; // Вихід для зміни числа 2 (_changeNumber2_Out)
float changeNumber2Value; // Значення для зміни числа 2 (_changeNumber2_OLV)
unsigned long alarmSensorReadTime = 0UL; // Час останнього читання датчика аварії (_d18x2x3Tti)
float alarmSensorValue = 0.00; // Значення датчика аварії (_d18x2x3O)
String gsfs1 = "0"; // Тимчасова строка GSFS1 (_GSFS1)
String gsfs5 = "0"; // Тимчасова строка GSFS5 (_GSFS5)
bool timer1Active = 0; // Прапорець активності таймера 1 (_tim1I)
bool timer1Output = 0; // Вихід таймера 1 (_tim1O)
unsigned long timer1PrevMillis = 0UL; // Попередній час таймера 1 (_tim1P)
String gsfs7 = "0"; // Тимчасова строка GSFS7 (_GSFS7)
String rvfu1Data; // Дані RVFU1 (_RVFU1Data)
bool rvfu1Reset = 1; // Скидання RVFU1 (_RVFU1Reset)
int fsfs5io = 0; // IO параметр FSFS5 (_FSFS5IO)
bool fsfs5co = 0; // CO параметр FSFS5 (_FSFS5CO)
bool d1b2 = 0; // Прапорець D1B2 (_D1B2)
int fsfs6io = 0; // IO параметр FSFS6 (_FSFS6IO)
bool changeNumber5Output = 0; // Вихід для зміни числа 5 (_changeNumber5_Out)
float changeNumber5Value; // Значення для зміни числа 5 (_changeNumber5_OLV)
bool timer6Active = 0; // Прапорець активності таймера 6 (_tim6I)
bool timer6Output = 0; // Вихід таймера 6 (_tim6O)
unsigned long timer6PrevMillis = 0UL; // Попередній час таймера 6 (_tim6P)
String switchString5; // Тимчасова строка для перемикача 5 (_swi5)
String gsfs6 = "0"; // Тимчасова строка GSFS6 (_GSFS6)
unsigned long stou2 = 0UL; // Час для STOU2 (_stou2)
bool timer5Active = 0; // Прапорець активності таймера 5 (_tim5I)
bool timer5Output = 0; // Вихід таймера 5 (_tim5O)
unsigned long timer5PrevMillis = 0UL; // Попередній час таймера 5 (_tim5P)
bool switchFlag4; // Прапорець перемикача 4 (_swi4)
int fsfs14io = 0; // IO параметр FSFS14 (_FSFS14IO)
bool fsfs14co = 0; // CO параметр FSFS14 (_FSFS14CO)
bool changeNumber7atmPressure = 0; // Вихід для зміни числа 7 (_changeNumber7_Out)
float changeNumber7Value; // Значення для зміни числа 7 (_changeNumber7_OLV)
bool timer4Active = 0; // Прапорець активності таймера 4 (_tim4I)
bool timer4Output = 0; // Вихід таймера 4 (_tim4O)
unsigned long timer4PrevMillis = 0UL; // Попередній час таймера 4 (_tim4P)
String gsfs13 = "0"; // Тимчасова строка GSFS13 (_GSFS13)
String gsfs4 = "0"; // Тимчасова строка GSFS4 (_GSFS4)
unsigned long bmpSensorReadTime2 = 0UL; // Час читання BMP датчика 2 (_bmp0852Tti)
String gsfs12 = "0"; // Тимчасова строка GSFS12 (_GSFS12)
int fsfs2io = 0; // IO параметр FSFS2 (_FSFS2IO)
bool fsfs2co = 0; // CO параметр FSFS2 (_FSFS2CO)
int fsfs3io = 0; // IO параметр FSFS3 (_FSFS3IO)
bool switchFlag9; // Прапорець перемикача 9 (_swi9)
String switchString8; // Тимчасова строка перемикача 8 (_swi8)
String switchString3; // Тимчасова строка перемикача 3 (_swi3)
unsigned long stou1 = 0UL; // Час для STOU1 (_stou1)
bool triggerFlag2 = 0; // Тригер 2 (_trgr2)
bool pzs2OES = 0; // Прапорець PZS2OES (_pzs2OES)
bool triggerFlag2b = 0; // Додатковий тригер 2 (_trgs2)
String gsfs16 = "0"; // Тимчасова строка GSFS16 (_GSFS16)
bool changeNumber6columnTemp = 0; // Вихід для зміни числа 6 (_changeNumber6_Out)
float changeNumber6Value; // Значення для зміни числа 6 (_changeNumber6_OLV)
bool timer7Active = 0; // Прапорець активності таймера 7 (_tim7I)
bool timer7Output = 0; // Вихід таймера 7 (_tim7O)
unsigned long timer7PrevMillis = 0UL; // Попередній час таймера 7 (_tim7P)
bool bitChange2OldState = 0; // Старий стан зміни біта 2 (_BitChange_2_OldSt)
bool bitChange2Output = 0; // Вихід біта зміни 2 (_BitChange_2_Out)
bool changeNumber4Output = 0; // Вихід для зміни числа 4 (_changeNumber4_Out)
float changeNumber4Value; // Значення для зміни числа 4 (_changeNumber4_OLV)
String gsfs9 = "0"; // Тимчасова строка GSFS9 (_GSFS9)
String switchString7; // Тимчасова строка перемикача 7 (_swi7)
bool generator3Active = 0; // Активність генератора 3 (_gen3I)
bool generator3Output = 0; // Вихід генератора 3 (_gen3O)
unsigned long generator3PrevMillis = 0UL; // Попередній час генератора 3 (_gen3P)
int fsfs13io = 0; // IO параметр FSFS13 (_FSFS13IO)
bool fsfs13co = 0; // CO параметр FSFS13 (_FSFS13CO)
String gsfs11 = "0"; // Тимчасова строка GSFS11 (_GSFS11)
String switchString2; // Тимчасова строка перемикача 2 (_swi2)
unsigned long cubeSensorReadTime = 0UL; // Час останнього читання датчика куба (_d18x2x2Tti)
float cubeSensorValue = 0.00; // Значення датчика куба (_d18x2x2O)
int fsfs9io = 0; // IO параметр FSFS9 (_FSFS9IO)
bool bitChange4OldState = 0; // Старий стан зміни біта 4 (_BitChange_4_OldSt)
bool bitChange4Output = 0; // Вихід біта зміни 4 (_BitChange_4_Out)
bool generator1Active = 0; // Активність генератора 1 (_gen1I)
bool generator1Output = 0; // Вихід генератора 1 (_gen1O)
unsigned long generator1PrevMillis = 0UL; // Попередній час генератора 1 (_gen1P)
bool triggerFlag3b = 0; // Додатковий тригер 3 (_trgr3)
bool timer3Active = 0; // Прапорець активності таймера 3 (_tim3I)
bool timer3Output = 0; // Вихід таймера 3 (_tim3O)
unsigned long timer3PrevMillis = 0UL; // Попередній час таймера 3 (_tim3P)
int dispOldLength3 = 0; // Стара довжина дисплея 3 (_disp3oldLength)
float switchValue11; // Значення перемикача 11 (_swi11)
bool bitChange1OldState = 0; // Старий стан зміни біта 1 (_BitChange_1_OldSt)
bool bitChange1Output = 0; // Вихід біта зміни 1 (_BitChange_1_Out)
byte tempByte; // Тимчасова змінна типу byte (_tempVariable_byte)
int tempInt; // Тимчасова змінна типу int (_tempVariable_int)
float tempFloat; // Тимчасова змінна типу float (_tempVariable_float)
//шим генератора
unsigned long valvePwmLastTime = 0;
bool valvePwmState = false;
#include <avr/pgmspace.h>
#include <EncButton.h> // твоя бібліотека енкодера

// --- Типи даних для пунктів меню ---
enum MenuValueType : uint8_t { TYPE_INT, TYPE_FLOAT, TYPE_BOOL };
enum MenuSection : uint8_t { MENU_SETUP, MENU_WORK };
enum MainMenuLevel : uint8_t { MAIN_MENU, SUB_MENU };

// --- Тексти назв пунктів у PROGMEM (тільки тут змінюєш) ---
const char title_klapan[] PROGMEM = "Klapan/motor";
const char title_pwmkonec[] PROGMEM = "PwmKonec";
const char title_kubkonec[] PROGMEM = "KubKonec";
const char title_vodavkl[] PROGMEM = "VodaVkl temp";
const char title_alarm[]   PROGMEM = "Alarm Temp Limit";
const char title_water[]   PROGMEM = "Water/temp";
const char title_pwmperiod[] PROGMEM = "pwmPeriodMs";
const char title_waterflag[] PROGMEM = "Water Flag";
const char title_shimklapam[] PROGMEM = "shim klapam";
// Якщо треба - додаєш ще тут

const char* const menuTitles[] PROGMEM = {
  title_klapan,
  title_pwmkonec,
  title_kubkonec,
  title_vodavkl,
  title_alarm,
  title_water,
  title_pwmperiod,
  title_waterflag,
  title_shimklapam,
  // додай далі тут
};
#define MENU_ITEMS_COUNT 9 // змінюй під фактичну кількість

// --- Структура пункту меню: titleIdx замість рядка! ---
struct MenuItem {
  uint8_t titleIdx;           // Індекс у menuTitles[]
  void* valuePtr;             // Вказівник на змінну
  MenuValueType valueType;    // Тип значення
  int16_t step;               // Крок Zміни (ціле!)
  int16_t minVal;             // Мін
  int16_t maxVal;             // Макс
  void (*onConfirm)();        // Функція після редагування (NULL якщо не треба)
  MenuSection section;        // Сетап/робота
};

// --- Масив пунктів меню (setup → work) ---
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
  // Додаєш тут!
};

// --- Назви головних пунктів меню (у PROGMEM!) ---
const char menuTitleSetup[] PROGMEM = "Setup";
const char menuTitleWork[]  PROGMEM = "Work";
const char* const mainMenuTitles[] PROGMEM = {menuTitleSetup, menuTitleWork};

// --- Глобальні змінні ---
bool encoderMenuActive = false;      // Чи відкрите меню (двійний клік)
MainMenuLevel menuLevel = MAIN_MENU; // Головне/підменю
uint8_t mainMenuIndex = 0;           // 0: Setup, 1: Work
uint8_t subMenuIndex = 0;            // Активний пункт у підменю
bool editMode = false;               // Редагування

// --- Підрахунок кількості пунктів у секції (setup/work) ---
uint8_t getSectionCount(MenuSection section) {
  uint8_t count = 0;
  for (uint8_t i = 0; i < MENU_ITEMS_COUNT; ++i)
    if (menuItems[i].section == section) count++;
  return count;
}

// --- Знаходження індекса пункту у menuItems за індексом у секції ---
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

// --- Витягуємо назву пункту з PROGMEM ---
void getMenuTitle(uint8_t titleIdx, char* buf, uint8_t bufSize) {
  strncpy_P(buf, (PGM_P)pgm_read_ptr(&menuTitles[titleIdx]), bufSize - 1);
  buf[bufSize-1] = 0;
}

// --- Витягуємо назву секції з PROGMEM ---
void getMainMenuTitle(uint8_t mainMenuIdx, char* buf, uint8_t bufSize) {
  strncpy_P(buf, (PGM_P)pgm_read_ptr(&mainMenuTitles[mainMenuIdx]), bufSize - 1);
  buf[bufSize-1] = 0;
}

// --- Відображення меню на дисплеї ---
void displayMenu() {
  mainDisplay.clear();
  char buf[17];

  // --- Головне меню ---
  if (menuLevel == MAIN_MENU) {
    mainDisplay.setCursor(0, 0);
    mainDisplay.print("Menu:");
    mainDisplay.setCursor(0, 1);
    getMainMenuTitle(mainMenuIndex, buf, sizeof(buf));
    mainDisplay.print(buf);
    return;
  }

  // --- Підменю: шукаємо пункт у секції ---
  int8_t menuItemIndex = getMenuItemIndex(
    mainMenuIndex == 0 ? MENU_SETUP : MENU_WORK, subMenuIndex);

  if (menuItemIndex < 0 || menuItemIndex >= MENU_ITEMS_COUNT) {
    mainDisplay.setCursor(0, 0);
    mainDisplay.print("Нет пунктов");
    return;
  }

  MenuItem &item = menuItems[menuItemIndex];
  getMenuTitle(item.titleIdx, buf, sizeof(buf));
  mainDisplay.setCursor(0, 0);
  mainDisplay.print(buf);
  if (editMode) mainDisplay.print(" [Edit]");

  mainDisplay.setCursor(0, 1);
  switch(item.valueType) {
    case TYPE_INT:
      mainDisplay.print(*(int*)item.valuePtr);
      break;
    case TYPE_FLOAT:
      mainDisplay.print(*(float*)item.valuePtr, 1);
      break;
    case TYPE_BOOL:
      mainDisplay.print(*(bool*)item.valuePtr ? "On" : "Off");
      break;
  }
}

// --- Основна обробка енкодера для меню ---
void handleEncoderMenu() {
  // --- Вхід/вихід у меню подвійним кліком ---
  if (enc.hasClicks(2)) {
    encoderMenuActive = !encoderMenuActive;
    // тут можеш додати свої флаги чи очистку
    menuLevel = MAIN_MENU;
    editMode = false;
    mainMenuIndex = 0;
    subMenuIndex = 0;
    displayMenu();
    return;
  }

  if (!encoderMenuActive) return;

  // --- ГОЛОВНЕ МЕНЮ: вибір секції (Setup/Work) поворотом, вхід у підменю кліком ---
  if (menuLevel == MAIN_MENU) {
    uint8_t count = 2; // Setup, Work
    if (enc.right()) {
      mainMenuIndex = (mainMenuIndex + 1) % count;
      displayMenu();
    }
    if (enc.left()) {
      mainMenuIndex = (mainMenuIndex - 1 + count) % count;
      displayMenu();
    }
    if (enc.click()) {
      menuLevel = SUB_MENU;
      editMode = false;
      subMenuIndex = 0;
      displayMenu();
    }
    return;
  }

  // --- ПІДМЕНЮ ---
  MenuSection currentSection = (mainMenuIndex == 0) ? MENU_SETUP : MENU_WORK;
  uint8_t sectionCount = getSectionCount(currentSection);

  // --- Вхід/вихід у режим редагування утриманням ---
  if (enc.hold()) {
    editMode = !editMode;
    displayMenu();
    return;
  }

  // --- Переміщення по пунктах підменю (циклічно) ---
  if (!editMode) {
    if (enc.right()) {
      subMenuIndex = (subMenuIndex + 1) % sectionCount;
      displayMenu();
    }
    if (enc.left()) {
      subMenuIndex = (subMenuIndex - 1 + sectionCount) % sectionCount;
      displayMenu();
    }
    // --- Вихід у головне меню по кліку ---
    if (enc.click()) {
      menuLevel = MAIN_MENU;
      editMode = false;
      displayMenu();
    }
    return;
  }

  // --- РЕДАГУВАННЯ значення пункту підменю ---
  int8_t menuItemIndex = getMenuItemIndex(currentSection, subMenuIndex);
  if (menuItemIndex < 0 || menuItemIndex >= MENU_ITEMS_COUNT) return;
  MenuItem &item = menuItems[menuItemIndex];
  bool changed = false;

  if (enc.right()) {
    switch(item.valueType) {
      case TYPE_INT:
        *(int*)item.valuePtr += (int)item.step;
        if (*(int*)item.valuePtr > (int)item.maxVal) *(int*)item.valuePtr = (int)item.maxVal;
        changed = true;
        break;
      case TYPE_FLOAT:
        *(float*)item.valuePtr += item.step;
        if (*(float*)item.valuePtr > item.maxVal) *(float*)item.valuePtr = item.maxVal;
        changed = true;
        break;
      case TYPE_BOOL:
        *(bool*)item.valuePtr = !*(bool*)item.valuePtr;
        changed = true;
        break;
    }
  }
  if (enc.left()) {
    switch(item.valueType) {
      case TYPE_INT:
        *(int*)item.valuePtr -= (int)item.step;
        if (*(int*)item.valuePtr < (int)item.minVal) *(int*)item.valuePtr = (int)item.minVal;
        changed = true;
        break;
      case TYPE_FLOAT:
        *(float*)item.valuePtr -= item.step;
        if (*(float*)item.valuePtr < item.minVal) *(float*)item.valuePtr = item.minVal;
        changed = true;
        break;
      // Для BOOL лівий поворот нічого не робить
    }
  }
  if (changed) displayMenu();

  // --- Вихід із режиму редагування утриманням ---
  if (enc.hold()) {
    editMode = false;
    displayMenu();
    if (item.onConfirm) item.onConfirm();
  }
}

/*
====================
ОСНОВНА ЛОГІКА МЕНЮ (пам'ять оптимізована)

- Усі назви menuItem зберігаються в PROGMEM (Flash, не RAM).
- MenuItem містить лише індекс назви, а не рядок.
- Циклічний перехід по пунктах меню та підменю.
- Відображення та логіка повністю відповідає твоїм правилам (див. вище в чаті).
====================
*/

void setup() {
	// displayMenu(menuItemIndex, editMode);
	pinMode(BUZZER_PIN, OUTPUT); // Пін для активного зумера (BUZZER_PIN)
    pinMode(RAIN_LEAK_INPUT_PIN, INPUT_PULLUP); // Вхід датчика протікання/дощу (17)
    pinMode(VALVE_RELAY_DIRECT_PIN, OUTPUT); // Пін реле клапана відбору (VALVE_RELAY_DIRECT_PIN)
    digitalWrite(VALVE_RELAY_DIRECT_PIN, 1); // Ініціалізація реле клапана відбору (VALVE_RELAY_DIRECT_PIN)
    //pinMode(8, OUTPUT); // Пін виходу "Периферія" (8)
    //digitalWrite(8, 0); // Ініціалізація периферії (8)
    pinMode(PERIPHERY_OUTPUT_PIN, OUTPUT); // Пін виходу "Периферія" (PERIPHERY_OUTPUT_PIN)
    digitalWrite(PERIPHERY_OUTPUT_PIN, 0); // Ініціалізація периферії (PERIPHERY_OUTPUT_PIN)
    pinMode(RAZGON_OUTPUT_PIN, OUTPUT); // Пін виходу "Розгін" (RAZGON_OUTPUT_PIN)
    digitalWrite(RAZGON_OUTPUT_PIN, 0); // Ініціалізація розгону (RAZGON_OUTPUT_PIN)
    //pinMode(RABOTA_OUTPUT_UNUSED_PIN, OUTPUT); // Пін виходу "Робота" (RABOTA_OUTPUT_UNUSED_PIN)
    //digitalWrite(RABOTA_OUTPUT_UNUSED_PIN, 0); // Ініціалізація "Робота" (RABOTA_OUTPUT_UNUSED_PIN)
    pinMode(AVARIA_OUTPUT_PIN, OUTPUT); // Пін виходу "Аварія" (AVARIA_OUTPUT_PIN)
    digitalWrite(AVARIA_OUTPUT_PIN, 0); // Ініціалізація аварії (AVARIA_OUTPUT_PIN)
    Wire.begin(); // Ініціалізація I2C (Wire.begin())
    delay(10); // Затримка для стабілізації (delay(10))
    // TCCR2A = 0x00; // Налаштування таймера (TCCR2A)
    // TCCR2B = 0x07; // Налаштування таймера (TCCR2B)
   // TIMSK2 = 0x01; // Налаштування таймера (TIMSK2)
    // TCNT2 = 100; // Початкове значення таймера (TCNT2)
    analogReference(EXTERNAL); // Зовнішня опорна напруга (analogReference)
    bmeSensor.begin(); // Ініціалізація сенсора тиску та температури (_bmp085.begin())
    BtSerial.begin(9600);// Ініціалізація Bluetooth UART (Serial100.begin(9600))
    Serial.begin(115200);// Ініціалізація апаратного UART (Serial.begin(115200))
    mainDisplay.begin(); // Ініціалізація LCD дисплея (_lcd1.init())
    mainDisplay.noBacklight(); // Вимкнення підсвітки дисплея (_lcd1.noBacklight())
    stou1 = millis(); // Час для STOU1 (_stou1 = millis())
    stou2 = millis(); // Час для STOU2 (_stou2 = millis())



    if (!isEEPROMInitialized()) {
        // Serial.println("EEPROM не ініціалізована. Зчитування нових адрес...") // Коментар для ініціалізації EEPROM
        scanAndSaveSensors(); // Сканування та збереження адрес датчиків (scanAndSaveSensors)
    } else {
        loadAddresses(); // Зчитування адрес з EEPROM (loadAddresses)

        // Перевірка: які з адрес з EEPROM присутні // Коментар до перевірки адрес
        bool foundFlags[SENSOR_COUNT] = {false}; // Прапорці присутності адрес (foundFlags)
        uint8_t newAddresses[SENSOR_COUNT][8] = {0}; // Масив нових адрес (newAddresses)

        oneWireBus.reset_search(); // Скидання пошуку OneWire (oneWire.reset_search())
        uint8_t addr[8]; // Буфер для адреси датчика (addr)
        while (oneWireBus.search(addr)) { // Пошук датчиків (oneWire.search(addr))
            if (!isValidAddress(addr)) continue;

            // Перевірити, чи вже є така адреса // Коментар до перевірки адреси
            bool matched = false;
            for (byte i = 0; i < SENSOR_COUNT; i++) {
                if (memcmp(addr, tempSensorAddresses[i], 8) == 0) { // Порівняння з відомими адресами (memcmp(addr, sensorAddresses[i], 8))
                    foundFlags[i] = true;
                    matched = true;
                    break;
                }
            }

            // Якщо нова — зберегти на місце відсутньої // Коментар до збереження нової адреси
            if (!matched) {
                for (byte i = 0; i < SENSOR_COUNT; i++) {
                    if (!foundFlags[i]) {
                        memcpy(tempSensorAddresses[i], addr, 8); // Збереження нової адреси (memcpy(sensorAddresses[i], addr, 8))
                        foundFlags[i] = true;
                        break;
                    }
                }
            }
        }

        saveAddresses();  // Оновити EEPROM (saveAddresses)
      //  Serial.println("Адреси після перевірки:"); // Вивід інформації (Serial.println)
    //   for (byte i = 0; i < SENSOR_COUNT; i++) {
     //       Serial.print(tempSensorNames[i]); Serial.print(": "); // Вивід імен датчиків (Serial.print(sensorNames[i]))
  //          printAddress(tempSensorAddresses[i]); // Вивід адрес датчиків (printAddress(sensorAddresses[i]))
       // }
    }

    memcpy(columnSensorAddr, tempSensorAddresses[0], 8);  // Колонна (memcpy(_d18x2x1Addr, sensorAddresses[0], 8))
    memcpy(cubeSensorAddr, tempSensorAddresses[1], 8);    // Куб (memcpy(_d18x2x2Addr, sensorAddresses[1], 8))
    memcpy(alarmSensorAddr, tempSensorAddresses[2], 8);   // Аварія (memcpy(_d18x2x3Addr, sensorAddresses[2], 8))


//displayMenu( menuItemIndex, editMode); // Початковий екран

}
void loop() {
//	Serial.print("Free memory: ");
//Serial.println(freeMemory());
	enc.tick();           // оновлення енкодера
	 

    if (needClearDisplay) { // Прапорець очищення дисплея (_isNeedClearDisp1)
        mainDisplay.clear(); // Очищення дисплея (_lcd1.clear())
        needClearDisplay = 0; // Скидання прапорця очищення (_isNeedClearDisp1)
    }
    powerDownCount = 0; // Лічильник вимкнень живлення (_PWDC)
    if (avlDfu0) { // Стан DFU0 (_AvlDFU0)
        avlDfu0 = 0; // Скидання DFU0 (_AvlDFU0)
    } else {
        if (Serial.available()) { // Чи є дані у апаратному UART (Serial.available())
			
            avlDfu0 = 1; // Встановити DFU0 (_AvlDFU0)
            readByteFromUART((Serial.read()), 0); // Зчитати байт з UART (_readByteFromUART)
        }
    }
    if (avlDfu100) { // Стан DFU100 (_AvlDFU100)
        avlDfu100 = 0; // Скидання DFU100 (_AvlDFU100)
    } else {
        if (BtSerial.available()) { // Чи є дані у Bluetooth UART (Serial100.available())
            avlDfu100 = 1; // Встановити DFU100 (_AvlDFU100)
            readByteFromUART((BtSerial.read()), 100); // Зчитати байт з Bluetooth UART (_readByteFromUART)
        }
    }
    if (isTimer(bmpSensorReadTime2, 5000)) { // Таймер для BMP сенсора (_isTimer(_bmp0852Tti, 5000))
        bmpSensorReadTime2 = millis(); // Оновити час читання BMP сенсора (_bmp0852Tti = millis())
        double tempBMP085TData; // Тимчасова змінна температури BMP (_tempBMP085TData)
        /* tempByte = bmeSensor.startTemperature();
            if (tempByte != 0) 
            {
                delay(tempByte);
                */
        tempByte = bmeSensor.readTemperature(); // Зчитати температуру BMP сенсора (_tempVariable_byte = _bmp085.readTemperature())
        if (tempByte != 0) {
            /* double tempBMP085PData;
                tempByte = bmeSensor.startPressure(3);*/
            if (tempByte != 0) {
                delay(10); // Затримка для стабілізації (_tempVariable_byte)
                tempByte = bmeSensor.readPressure(); // Зчитати тиск BMP сенсора (_tempVariable_byte = _bmp085.readPressure())
                //Serial.println(bmeSensor.readPressure()); // Вивід тиску BMP сенсора (Serial.println(_bmp085.readPressure()))
                //Serial.println(bmeSensor.readPressure()); // Вивід тиску BMP сенсора (Serial.println(_bmp085.readPressure()))

                //tempByte=pressureToMmHg(tempByte); // Переведення тиску у мм рт.ст. (_tempVariable_byte=pressureToMmHg(_tempVariable_byte))
                if (tempByte != 0) {
                    bmpPressure = bmeSensor.readPressure(); // Збереження тиску BMP сенсора (_bmp085P = _bmp085.readPressure())
                }
            }
        }
    }  
	
    //Плата:1
    //Наименование:Прием из UART
    if (columnSensorFlag) { // Ознака прийому із UART (_gtv1)
        if (!rvfu3Reset) { // Скидання прапорця прийому UART3 (!_RVFU3Reset)
            rvfu3Data = String(""); // Обнулення даних UART3 (_RVFU3Data)
            rvfu3Reset = 1; // Встановити прапорець прийому UART3 (_RVFU3Reset)
        }
    } else {
        rvfu3Reset = 0; // Скидання прапорця прийому UART3 (_RVFU3Reset)
    }
    if (columnSensorFlag) { // Ознака прийому із UART (_gtv1)
        if (!rvfu1Reset) { // Скидання прапорця прийому UART1 (!_RVFU1Reset)
            rvfu1Data = String(""); // Обнулення даних UART1 (_RVFU1Data)
            rvfu1Reset = 1; // Встановити прапорець прийому UART1 (_RVFU1Reset)
        }
    } else {
        rvfu1Reset = 0; // Скидання прапорця прийому UART1 (_RVFU1Reset)
    }
    if ((avlDfu100 || avlDfu0)) { // Ознака активності DFU (_AvlDFU100, _AvlDFU0)
        tempFlag5 = 1; // Ознака події DFU (_gtv5)
    }
    cubeSensorFlag = (avlDfu100 || avlDfu0); // Ознака активності DFU (_gtv32)
    if ((avlDfu100 || avlDfu0)) { // Активність DFU (_AvlDFU100, _AvlDFU0)
        timer10Output = 1; // Вихід таймера 10 (_tim10O)
        timer10Active = 1; // Активність таймера 10 (_tim10I)
    } else {
        if (timer10Active) { // Таймер 10 активний (_tim10I)
            timer10Active = 0; // Скидання активності таймера 10 (_tim10I)
            timer10PrevMillis = millis(); // Оновлення часу таймера 10 (_tim10P)
        } else {
            if (timer10Output) { // Вихід таймера 10 активний (_tim10O)
                if (isTimer(timer10PrevMillis, 1000)) timer10Output = 0; // Скидання виходу таймера 10 (_tim10O)
            }
        }
    }
    columnSensorFlag = !(timer10Output); // Ознака прийому із UART (_gtv1)
    if (avlDfu0) triggerFlag2b = 1; // Ознака активності DFU0 (_trgs2)
    if (avlDfu100) triggerFlag2b = 0; // Ознака активності DFU100 (_trgs2)
    if (triggerFlag2b) { // Вибір даних UART3 (_trgs2)
        switchString7 = rvfu3Data; // Дані UART3 (_swi7 = _RVFU3Data)
    } else {
        switchString7 = rvfu1Data; // Дані UART1 (_swi7 = _RVFU1Data)
    }
    tempString13 = switchString7; // Тимчасова строка (_gtv13 = _swi7)
 
	//Плата:2
    //Наименование:Декодирование UART
    if (cubeSensorFlag == 1) { // Ознака активності DFU (_gtv32 == 1)
        fsfs6io = (tempString13.indexOf(String("!"))); // Позиція символу ! (_FSFS6IO)
        fsfs5io = (tempString13.indexOf(String("#"))); // Позиція символу # (_FSFS5IO)
        fsfs5co = fsfs5io > -1; // Ознака наявності # (_FSFS5CO)
        gsfs8 = tempString13.substring(fsfs5io, fsfs6io); // Підрядок від # до ! (_GSFS8)
        gsfs9 = gsfs8.substring(1); // Підрядок після # (_GSFS9)
        if (fsfs5co) {
            tempInt2 = (int(stringToFloat(gsfs9))); // Дані після # (_gtv2)
        }
        fsfs3io = (tempString13.indexOf(String("!"))); // Позиція символу ! (_FSFS3IO)
        fsfs2io = (tempString13.indexOf(String("@"))); // Позиція символу @ (_FSFS2IO)
        fsfs2co = fsfs2io > -1; // Ознака наявності @ (_FSFS2CO)
        gsfs4 = tempString13.substring(fsfs2io, fsfs3io); // Підрядок від @ до ! (_GSFS4)
        gsfs5 = gsfs4.substring(1); // Підрядок після @ (_GSFS5)
        if (fsfs2co) {
            alarmTempLimit = (int(stringToFloat(gsfs5))); // Дані після @ (_gtv8)
        }
        fsfs9io = (tempString13.indexOf(String("%"))); // Позиція символу % (_FSFS9IO)
        fsfs8io = (tempString13.indexOf(String("*"))); // Позиція символу * (_FSFS8IO)
        fsfs8co = fsfs8io > -1; // Ознака наявності * (_FSFS8CO)
        if (fsfs8co) {
            pressureSensorValue = atmPressure; // Поточний тиск (_gtv18 = _gtv17)
        }
        gsfs11 = tempString13.substring(fsfs8io, fsfs9io); // Підрядок від * до % (_GSFS11)
        gsfs1 = gsfs11.substring(1); // Підрядок після * (_GSFS1)
        if (fsfs8co) {
            pwmValue2 = (stringToFloat(gsfs1)); // Дані після * (_gtv7)
        }
        fsfs14io = (tempString13.indexOf(String("&"))); // Позиція символу & (_FSFS14IO)
        fsfs14co = fsfs14io > -1; // Ознака наявності & (_FSFS14CO)
        gsfs17 = tempString13.substring(fsfs14io, fsfs8io); // Підрядок від & до * (_GSFS17)
        gsfs13 = gsfs17.substring(1); // Підрядок після & (_GSFS13)
        if (fsfs14co) {
            pwmValue1 = (stringToFloat(gsfs13)); // Дані після & (_gtv6)
        }
        fsfs13io = (tempString13.indexOf(String("$"))); // Позиція символу $ (_FSFS13IO)
        fsfs13co = fsfs13io > -1; // Ознака наявності $ (_FSFS13CO)
        gsfs16 = tempString13.substring(fsfs13io, fsfs14io); // Підрядок від $ до & (_GSFS16)
        gsfs12 = gsfs16.substring(1); // Підрядок після $ (_GSFS12)
        if ((String("0")).equals(gsfs12)) triggerFlag3b = 0; // Значення після $ (_trgr3 = 0)
        if ((String("1")).equals(gsf12)) triggerFlag3b = 1; // Значення після $ (_trgr3 = 1)
        if (fsfs13co) {
            tempFlag33 = triggerFlag3b; // Прапорець 33 (_gtv33)
        }
        fsfs4io = (tempString13.indexOf(String("^"))); // Позиція символу ^ (_FSFS4IO)
        fsfs4co = fsfs4io > -1; // Ознака наявності ^ (_FSFS4CO)
        gsfs6 = tempString13.substring(fsfs4io, fsfs13io); // Підрядок від ^ до $ (_GSFS6)
        gsfs7 = gsfs6.substring(1); // Підрядок після ^ (_GSFS7)
        if ((String("0")).equals(gsfs7)) triggerFlag2 = 0; // Значення після ^ (_trgr2 = 0)
        if ((String("1")).equals(gsfs7)) triggerFlag2 = 1; // Значення після ^ (_trgr2 = 1)
        if (fsfs4co) {
            tempFlag29 = triggerFlag2; // Прапорець 29 (_gtv29)
        }
        cubeSensorFlag = 0; // Скидання ознаки DFU (_gtv32 = 0)
    }
	   
 //Плата:3
//Наименование:Датчики і контроль зміни значень

// Читання датчика колони через таймер (5 сек)
if (isTimer(columnSensorReadTime, 5000)) {           // Стара назва: _isTimer(_d18x2x1Tti, 5000)
    columnSensorReadTime = millis();                  // Стара назва: _d18x2x1Tti = millis()
    tempFloat = readDS18TempOW2(columnSensorAddr, 0); // Стара назва: _tempVariable_float = _readDS18_ow2(_d18x2x1Addr, 0)
    if (tempFloat < 500) {                            // Стара назва: if (_tempVariable_float < 500)
        columnSensorValue = tempFloat;                // Стара назва: _d18x2x1O = _tempVariable_float
    }
}

// Оновлення структури UB_185384122 по колоні
ubDataUbi = columnSensorValue;                        // Стара назва: UB_185384122_ubi_117131038 = (_d18x2x1O)
funcUB_185384122(&ubDataInstance2, ubDataUbi);        // Стара назва: _func_UB_185384122(&UB_185384122_Instance2, UB_185384122_ubi_117131038)

// Якщо умови налаштування виконані і значення не 85 — зберігаємо температуру колони
if ((ubDataInstance2.uboFlag) && (!(columnSensorValue == 85))) { // Стара назва: UB_185384122_Instance2.ubo_13416262 && !((_d18x2x1O) == 85)
    columnTemp = (int(10 * ubDataInstance2.uboValue)) / 10.00;   // Стара назва: _gtv9 = ((int((10) * (UB_185384122_Instance2.ubo_120270494)))) / (10.00)
}

// Контроль зміни значення для columnTemp
if (changeNumber6columnTemp) {                            // Стара назва: _changeNumber6_Out
    changeNumber6columnTemp = 0;
} else {
    tempFloat = columnTemp;                           // Стара назва: _tempVariable_float = _gtv9
    if (tempFloat != changeNumber6Value) {            // Стара назва: if (_tempVariable_float != _changeNumber6_OLV)
        changeNumber6Value = tempFloat;               // Стара назва: _changeNumber6_OLV = _tempVariable_float
        changeNumber6columnTemp = 1;                      // Стара назва: _changeNumber6_Out = 1
    }
}

// Контроль зміни значення для atmPressure
if (changeNumber7atmPressure) {                            // Стара назва: _changeNumber7_Out
    changeNumber7atmPressure = 0;
} else {
    tempFloat = atmPressure;                          // Стара назва: _tempVariable_float = _gtv17
    if (tempFloat != changeNumber7Value) {            // Стара назва: if (_tempVariable_float != _changeNumber7_OLV)
        changeNumber7Value = tempFloat;               // Стара назва: _changeNumber7_OLV = _tempVariable_float
        changeNumber7atmPressure = 1;                      // Стара назва: _changeNumber7_Out = 1
    }
}

// Контроль зміни значення для cubeTemp
if (changeNumber2cubeTemp) {                            // Стара назва: _changeNumber2_Out
    changeNumber2cubeTemp = 0;
} else {
    tempFloat = cubeTemp;                             // Стара назва: _tempVariable_float = _gtv14
    if (tempFloat != changeNumber2Value) {            // Стара назва: if (_tempVariable_float != _changeNumber2_OLV)
        changeNumber2Value = tempFloat;               // Стара назва: _changeNumber2_OLV = _tempVariable_float
        changeNumber2cubeTemp = 1;                      // Стара назва: _changeNumber2_Out = 1
    }
}

// Оновлення прапорця зміни, якщо змінилась хоча б одна із трьох величин
if ((changeNumber6columnTemp || changeNumber7atmPressure) || changeNumber2cubeTemp) { // Стара назва: (((_changeNumber6_Out) || (_changeNumber7_Out))) || (_changeNumber2_Out)
    tempFlag5 = 1;                                       // Стара назва: _gtv5 = 1
}

// Читання датчика куба через таймер (5 сек)
if (isTimer(cubeSensorReadTime, 5000)) {                 // Стара назва: _isTimer(_d18x2x2Tti, 5000)
    cubeSensorReadTime = millis();                       // Стара назва: _d18x2x2Tti = millis()
    tempFloat = readDS18TempOW2(cubeSensorAddr, 0);      // Стара назва: _tempVariable_float = _readDS18_ow2(_d18x2x2Addr, 0)
    if (tempFloat < 500) {                               // Стара назва: if (_tempVariable_float < 500)
        cubeSensorValue = tempFloat;                     // Стара назва: _d18x2x2O = _tempVariable_float
    }
}

// Оновлення структури UB_185384122 по кубу
ubDataUbi = cubeSensorValue;                             // Стара назва: UB_185384122_ubi_117131038 = (_d18x2x2O)
funcUB_185384122(&ubDataInstance3, ubDataUbi);           // Стара назва: _func_UB_185384122(&UB_185384122_Instance3, UB_185384122_ubi_117131038)

// Якщо умови налаштування виконані і значення не 85 — зберігаємо температуру куба із корекцією
if ((ubDataInstance3.uboFlag) && (!(cubeSensorValue == 85))) { // Стара назва: UB_185384122_Instance3.ubo_13416262 && !((_d18x2x2O) == 85)
    cubeTemp = cubeTempCorrection + ubDataInstance3.uboValue;  // Стара назва: _gtv14 = (_gtv22) + (UB_185384122_Instance3.ubo_120270494)
}

// Читання аварійного датчика через таймер (5 сек)
if (isTimer(alarmSensorReadTime, 5000)) {                       // Стара назва: _isTimer(_d18x2x3Tti, 5000)
    alarmSensorReadTime = millis();                             // Стара назва: _d18x2x3Tti = millis()
    tempFloat = readDS18TempOW2(alarmSensorAddr, 0);            // Стара назва: _tempVariable_float = _readDS18_ow2(_d18x2x3Addr, 0)
    if (tempFloat < 500) {                                      // Стара назва: if (_tempVariable_float < 500)
        alarmSensorValue = tempFloat;                           // Стара назва: _d18x2x3O = _tempVariable_float
    }
}

// Оновлення структури UB_185384122 по аварійному датчику
ubDataUbi = alarmSensorValue;                                   // Стара назва: UB_185384122_ubi_117131038 = (_d18x2x3O)
funcUB_185384122(&ubDataInstance4, ubDataUbi);                  // Стара назва: _func_UB_185384122(&UB_185384122_Instance4, UB_185384122_ubi_117131038)

// Якщо умови налаштування виконані і значення не 85 — зберігаємо температуру аварійного датчика
if ((ubDataInstance4.uboFlag) && (!(alarmSensorValue == 85))) { // Стара назва: UB_185384122_Instance4.ubo_13416262 && !((_d18x2x3O) == 85)
    alarmTemp = ubDataInstance4.uboValue;                       // Стара назва: _gtv15 = UB_185384122_Instance4.ubo_120270494
}

// Оновлення значення атмосферного тиску
atmPressure = bmpPressure / 133.3;                              // Стара назва: _gtv17 = (_bmp085P) / (133.3)
//Плата:4
//Наименование:Блок датчиков и контрольной суммы для UART
//Serial.print("tempFlag5 - ");
//Serial.println(tempFlag5);

 //Плата:5
//Наименование:Передача в UART
// if (tempFlag5 == 1) { // _gtv5 == 1
// char upperBuf[64];
// char lowerBuf[32];

// Формуємо верхній рядок
// snprintf(
    // upperBuf, sizeof(upperBuf),
    // "HomeSamogon.ru/4.8,%.1f,%.1f,%.1f,",
    // columnTemp, atmPressure, cubeTemp
// );
// displayUpperText = upperBuf;

// // Обчислюємо проміжні значення для нижнього рядка
// float cubePlusPi = cubeTemp + 3.14f;
// float sumColumnAtm = columnTemp + atmPressure;
// float lowerVal = cubePlusPi * sumColumnAtm;

// // Формуємо нижній рядок
// snprintf(lowerBuf, sizeof(lowerBuf), "%.1f", lowerVal);
// displayLowerText = lowerBuf;
	// }
 if (tempFlag5 == 1) { // _gtv5 == 1
 displayUpperText = ((String("HomeSamogon.ru/4.8,")) + ((floatToStringWitRaz(columnTemp, 1))) + (String(",")) + ((floatToStringWitRaz(atmPressure, 1))) + (String(",")) + ((floatToStringWitRaz(cubeTemp, 1))) + (String(","))); // _gtv10 = ((String("HomeSamogon.ru/4.8,")) + ((_floatToStringWitRaz(_gtv9, 1))) + (String(",")) + ((_floatToStringWitRaz(_gtv17, 1))) + (String(",")) + ((_floatToStringWitRaz(_gtv14, 1))) + (String(",")));
 displayLowerText = (floatToStringWitRaz(((stringToFloat((floatToStringWitRaz(cubeTemp, 1)))) + (3.14)) * ((stringToFloat((floatToStringWitRaz(columnTemp, 1)))) + (stringToFloat((floatToStringWitRaz(atmPressure, 1))))), 1)); // _gtv16 = (_floatToStringWitRaz((((_stringToFloat((_floatToStringWitRaz(_gtv14, 1))))) + (3.14)) * (((_stringToFloat((_floatToStringWitRaz(_gtv9, 1))))) + ((_stringToFloat((_floatToStringWitRaz(_gtv17, 1)))))), 1));
 }

 if (tempFlag12) { // _gtv12
 switchString3 = String("1"); // _swi3 = String("1")
 } else {
 switchString3 = String("0"); // _swi3 = String("0")
 }

if (tempFlag33) { // _gtv33
  switchString6 = String("1"); // _swi6 = String("1")
} else {
  switchString6 = String("0"); // _swi6 = String("0")
}

alarmFlag2 = ((!digitalRead(RAIN_LEAK_INPUT_PIN)) || (alarmTemp > vaporSensorTriggerTemp)); // _gtv25 = ((!((digitalRead(RAIN_LEAK_INPUT_PIN)))) || ((_gtv15) > (_gtv21)))

if ((!digitalRead(RAIN_LEAK_INPUT_PIN)) || (alarmTemp > vaporSensorTriggerTemp)) { // ((!((digitalRead(RAIN_LEAK_INPUT_PIN)))) || ((_gtv15) > (_gtv21)))
  switchString2 = String("1"); // _swi2 = String("1")
} else {
  switchString2 = String("0"); // _swi2 = String("0")
}

if (tempFlag30) { // _gtv30
  switchString8 = String("1"); // _swi8 = String("1")
} else {
  switchString8 = String("0"); // _swi8 = String("0")
}

if (columnSensorFlag) { // _gtv1
  if (isTimer(stou1, 2000)) { // _isTimer(_stou1, 2000)
BtSerial.print(displayUpperText);
BtSerial.print(switchString6);
BtSerial.print(",");

BtSerial.print(switchString3);
BtSerial.print(",");

BtSerial.print(floatToStringWitRaz((pwmValue1 - pressureValue), 1));
BtSerial.print(",");

BtSerial.print(floatToStringWitRaz((pwmValue2 - pressureValue), 1));
BtSerial.print(",");

BtSerial.print(tempInt2, DEC);
BtSerial.print(",");

BtSerial.print(floatToStringWitRaz(alarmTempLimit, 2));
BtSerial.print(",");

BtSerial.print(displayLowerText);
BtSerial.print(",");

BtSerial.print(floatToStringWitRaz(alarmTemp, 1));
BtSerial.print(",");

BtSerial.print(switchString2);
BtSerial.print(",");

BtSerial.print(switchString8);
BtSerial.print(",%,");   


// String uartStr;

// uartStr  = displayUpperText + switchString6 + String(",") + switchString3 + String(",") +
      // floatToStringWitRaz((pwmValue1 - pressureValue), 1) + String(",") +
      // floatToStringWitRaz((pwmValue2 - pressureValue), 1) + String(",") +
      // String(tempInt2, DEC) + String(",") + floatToStringWitRaz(alarmTempLimit, 2) + String(",") +
      // displayLowerText + String(",") + floatToStringWitRaz(alarmTemp, 1) + String(",") +
      // switchString2 + String(",") + switchString8 + String(",%,");
// BtSerial.print(uartStr);
    stou1 = millis(); // _stou1 = millis()
  }
} else {
  stou1 = millis(); // _stou1 = millis()
}
// Serial.print("displayUpperText - ");
// Serial.println(displayUpperText);

// Serial.print("switchString6 - ");
// Serial.println(switchString6);

// Serial.print("switchString3 - ");
// Serial.println(switchString3);

// Serial.print("pwmValue1 - pressureValue - ");
// Serial.println(floatToStringWitRaz((pwmValue1 - pressureValue), 1));

// Serial.print("pwmValue2 - pressureValue - ");
// Serial.println(floatToStringWitRaz((pwmValue2 - pressureValue), 1));

// Serial.print("tempInt2 - ");
// Serial.println(tempInt2, DEC);

// Serial.print("alarmTempLimit - ");
// Serial.println(floatToStringWitRaz(alarmTempLimit, 2));

// Serial.print("displayLowerText - ");
// Serial.println(displayLowerText);

// Serial.print("alarmTemp - ");
// Serial.println(floatToStringWitRaz(alarmTemp, 1));

// Serial.print("switchString2 - ");
// Serial.println(switchString2);

// Serial.print("switchString8 - ");
// Serial.println(switchString8);

// Serial.println("%");

//delay(3000);
if (columnSensorFlag) { // _gtv1
  if (isTimer(stou2, 2000)) { // _isTimer(_stou2, 2000)
Serial.print(displayUpperText);

Serial.print(switchString6);
Serial.print(",");

Serial.print(switchString3);
Serial.print(",");

Serial.print(floatToStringWitRaz((pwmValue1 - pressureValue), 1));
Serial.print(",");

Serial.print(floatToStringWitRaz((pwmValue2 - pressureValue), 1));
Serial.print(",");

Serial.print(tempInt2, DEC);
Serial.print(",");

Serial.print(floatToStringWitRaz(alarmTempLimit, 2));
Serial.print(",");

Serial.print(displayLowerText);
Serial.print(",");

Serial.print(floatToStringWitRaz(alarmTemp, 1));
Serial.print(",");

Serial.print(switchString2);
Serial.print(",");

Serial.print(switchString8);
Serial.print(",%,");	



// String uartStr;

// uartStr  =  displayUpperText + switchString6 + String(",") + switchString3 + String(",") +
      // floatToStringWitRaz((pwmValue1 - pressureValue), 1) + String(",") +
      // floatToStringWitRaz((pwmValue2 - pressureValue), 1) + String(",") +
      // String(tempInt2, DEC) + String(",") + floatToStringWitRaz(alarmTempLimit, 2) + String(",") +
      // displayLowerText + String(",") + floatToStringWitRaz(alarmTemp, 1) + String(",") +
      // switchString2 + String(",") + switchString8 + String(",%,");
// Serial.print(uartStr);
    stou2 = millis(); // _stou2 = millis()
  }
} else {
  stou2 = millis(); // _stou2 = millis()
}
//Serial.println("Loop after Плата5");
//Плата:6
//Наименование:Смещение температуры при изменении давления

if (pressureCorrectionEnabled == 1) { // _gtv23
  if (changeNumber4Output) { // _changeNumber4_Out
    changeNumber4Output = 0;
  } else {
    tempFloat = atmPressure; // _tempVariable_float = _gtv17
    if (tempFloat != changeNumber4Value) { // _changeNumber4_OLV
      changeNumber4Value = tempFloat;
      changeNumber4Output = 1;
    }
  }
  if (changeNumber4Output) {
    pressureValue = (pressureSensorValue - atmPressure) * 0.035; // _gtv26 = ((_gtv18) - (_gtv17)) * (0.035)
  }
}
//Serial.println("Loop after Плата6");
 //Плата:7
//Наименование:ШИМ

freeLogicInputArr[0] = tempInt2 <= 0;        // _FreeLog1_IArr[0] = (_gtv2) <= (0);
freeLogicInputArr[1] = tempInt2 >= 1023;     // _FreeLog1_IArr[1] = (_gtv2) >= (1023);
freeLogicQ1 = checkFreeLogicBlockOutput(freeLogicInputArr, 2, freeLogicQ1StateArr, 2); // _FreeLog1_Q1 = _checkFreeLogicBlockOutput(_FreeLog1_IArr, 2, _FreeLog1_Q1_StArr, 2);

if (!(tempInt2 <= 0 || tempInt2 >= 1023)) {  // !((((_gtv2) <= (0)) || ((_gtv2) >= (1023))))
  if (!generator3Active) {                   // !_gen3I
    generator3Active = 1;                    // _gen3I = 1;
    generator3Output = 1;                    // _gen3O = 1;
    generator3PrevMillis = millis();         // _gen3P = millis();
  }
} else {
  generator3Active = 0;                      // _gen3I = 0;
  generator3Output = 0;                      // _gen3O = 0;
}

if (generator3Active) {                      // _gen3I
  if (generator3Output) {                     // _gen3O
    if (isTimer(generator3PrevMillis, map(tempInt2, 0, 1023, 0, pwmPeriodMs))) { // _isTimer(_gen3P, (map((_gtv2), (0), (1023), (0), ((_gtv45)))))
      generator3PrevMillis = millis();        // _gen3P = millis();
      generator3Output = 0;                   // _gen3O = 0;
    }
  } else {
    if (isTimer(generator3PrevMillis, pwmPeriodMs - map(tempInt2, 0, 1023, 0, pwmPeriodMs))) { // _isTimer(_gen3P, (_gtv45) - ((map((_gtv2), (0), (1023), (0), ((_gtv45))))))
      generator3PrevMillis = millis();        // _gen3P = millis();
      generator3Output = 1;                   // _gen3O = 1;
    }
  }
}

if ((tempInt2 <= 0) || (tempInt2 >= 1023)) { // (((_gtv2) <= (0)) || ((_gtv2) >= (1023)))
  switchFlag4 = freeLogicQ1;                 // _swi4 = _FreeLog1_Q1;
} else {
  switchFlag4 = generator3Output;            // _swi4 = _gen3O;
}
pwmCoarseFlag = switchFlag4;                 // _gtv3 = _swi4;
pwmFineFlag = ((tempInt2 <= 0) || (tempInt2 >= 1023)); // _gtv4 = (((_gtv2) <= (0)) || ((_gtv2) >= (1023)));

//Serial.println("Loop after Плата7");
//Плата:8
//Наименование:Динамический ШИМ / Клапан

if (bitChange4Output) {                      // _BitChange_4_Out
  bitChange4Output = 0;
} else {
  if (!(bitChange4OldState == tempFlag12)) { // !(_BitChange_4_OldSt == _gtv12)
    bitChange4OldState = tempFlag12;         // _BitChange_4_OldSt = _gtv12;
    bitChange4Output = 1;                    // _BitChange_4_Out = 1;
  }
}
if (bitChange4Output) {
  tempInt2 = map(tempInt2, 0, tempInt2, 0, tempInt2 - (tempInt2 / 20)); // _gtv2 = (map((_gtv2), (0), ((_gtv2)), (0), (((_gtv2) - ((_gtv2) / (20))))))
}

if (columnTemp >= pwmValue2 - (pwmValue2 - pwmValue1)) triggerFlag3 = 1;     // if ((_gtv9) >= ((_gtv7) - ((_gtv7) - (_gtv6)))) _trgs3 = 1;
if (columnTemp <= pwmValue2) triggerFlag3 = 0;                              // if ((_gtv9) <= (_gtv7)) _trgs3 = 0;

if (triggerFlag3) {                      // _trgs3
  if (timer1Active) {                    // _tim1I
    if (isTimer(timer1PrevMillis, 5000)) { // _isTimer(_tim1P, 5000)
      timer1Output = 1;                  // _tim1O = 1;
    }
  } else {
    timer1Active = 1;                    // _tim1I = 1;
    timer1PrevMillis = millis();         // _tim1P = millis();
  }
} else {
  timer1Output = 0;                      // _tim1O = 0;
  timer1Active = 0;                      // _tim1I = 0;
}

tempFlag12 = !timer1Output;              // _gtv12 = !(_tim1O);

if (pwmFineFlag) {                       // _gtv4
  switchFlag9 = pwmCoarseFlag;           // _swi9 = _gtv3;
} else {
  switchFlag9 = ((!timer1Output) && pwmCoarseFlag); // _swi9 = ((!(_tim1O)) && (_gtv3));
}

// Де керуєш клапаном:
// tempInt2 - основна ШІМ-змінна, діапазон 0-1023, треба привести до 0-255:

uint8_t pwmValue = map(tempInt2, 0, 1023, 0, 255);
if (controlMode == 1) {
  
    softwarePWM(VALVE_RELAY_DIRECT_PIN, pwmValue, 100, valvePwmLastTime, valvePwmState);
} else {
    digitalWrite(VALVE_RELAY_DIRECT_PIN, !(switchFlag9));
    valvePwmState = false;
}


// digitalWrite(VALVE_RELAY_DIRECT_PIN, switchFlag9);




//digitalWrite(VALVE_RELAY_DIRECT_PIN, !(switchFlag9));
//Serial.println("Loop after Плата8");
//Плата:9
//Наименование:Зуммер

// Перевірка аварійних умов та протікання
if (
    ((cubeTemp >= alarmTempLimit) ||       // _gtv14 >= _gtv8
     (alarmTemp > vaporSensorTriggerTemp)) // _gtv15 > _gtv21
    ||
    ((!digitalRead(RAIN_LEAK_INPUT_PIN))   // датчик дощу/протікання (pin 9)
     || (!alarmFlag))                      // _gtv24
) {
    // Таймер тривоги
    if (timer5Active) {                    // _tim5I
        if (isTimer(timer5PrevMillis, 5000)) { // _isTimer(_tim5P, 5000)
            timer5Output = 1;              // _tim5O = 1
        }
    } else {
        timer5Active = 1;                  // _tim5I = 1
        timer5PrevMillis = millis();       // _tim5P = millis()
    }
} else {
    timer5Output = 0;                      // _tim5O = 0
    timer5Active = 0;                      // _tim5I = 0
}

// Запуск генератора зумера при активному таймері
if (timer5Output) {
    if (!generator1Active) {               // !_gen1I
        generator1Active = 1;              // _gen1I = 1
        generator1Output = 1;              // _gen1O = 1
        generator1PrevMillis = millis();   // _gen1P = millis()
    }
} else {
    generator1Active = 0;                  // _gen1I = 0
    generator1Output = 0;                  // _gen1O = 0
}

// Генератор зумера: перемикає стан з затримкою HIGH/LOW
if (generator1Active) {                    // _gen1I
    if (generator1Output) {                 // _gen1O
        if (isTimer(generator1PrevMillis, 200)) { // _isTimer(_gen1P, 200)
            generator1PrevMillis = millis();      // _gen1P = millis()
            generator1Output = 0;                 // _gen1O = 0
        }
    } else {
        if (isTimer(generator1PrevMillis, 400)) { // _isTimer(_gen1P, 400)
            generator1PrevMillis = millis();      // _gen1P = millis()
            generator1Output = 1;                 // _gen1O = 1
        }
    }
}

// Включення/вимкнення активного зумера
if (generator1Output) {                     // _gen1O
    if (!pzs2OES) {                         // !_pzs2OES
        digitalWrite(BUZZER_PIN, HIGH);     // BUZZER_PIN (3), активний зумер
        pzs2OES = 1;                        // _pzs2OES = 1
    }
} else {
    if (pzs2OES) {                          // _pzs2OES
        digitalWrite(BUZZER_PIN, LOW);      // Вимикаємо зумер
        pzs2OES = 0;                        // _pzs2OES = 0
    }
}

// Перемикаємо підсвітку дисплея залежно від стану генератора
if (!generator1Output) {                    // !_gen1O
    if (!d1b2) {                            // !_D1B2
        mainDisplay.backlight();            // _lcd1.backlight()
        d1b2 = 1;                           // _D1B2 = 1
    }
} else {
    if (d1b2) {                             // _D1B2
        mainDisplay.noBacklight();          // _lcd1.noBacklight()
        d1b2 = 0;                           // _D1B2 = 0
    }
}
//Serial.println("Loop after Плата9");
//Плата:10
//Наименование:Разгон, Работа, Периферия, Авария
//Комментарии:Пины А0-А3

if ((!finishFlag2) && finishFlag) { // !_gtv44 && _gtv39; // Перевірка дозволу на розгін
  timer3Output = 1; // _tim3O; // Активувати таймер 3
  timer3Active = 1; // _tim3I; // Активувати прапорець таймера 3
} else {
  if (timer3Active) { // _tim3I; // Таймер 3 активний
    timer3Active = 0; // _tim3I; // Скинути таймер 3
    timer3PrevMillis = millis(); // _tim3P; // Зберегти час таймера 3
  } else {
    if (timer3Output) { // _tim3O; // Таймер 3 вихід активний
      if (isTimer(timer3PrevMillis, finishDelayMs)) timer3Output = 0; // _isTimer(_tim3P, _gtv40); // Перевірка затримки завершення розгону
    }
  }
}

if (finishFlag) { // _gtv39; // Перевірка прапорця розгону
  finishFlag = timer3Output; // _gtv39 = _tim3O; // Оновити прапорець розгону
}

if ((!finishFlag2) && finishFlag) { // !_gtv44 && _gtv39; // Перевірка дозволу на роботу
  if (timer7Active) { // _tim7I; // Таймер 7 активний
    if (isTimer(timer7PrevMillis, 10000)) { // _isTimer(_tim7P, 10000); // Таймер 7 — 10 сек
      timer7Output = 1; // _tim7O; // Активувати таймер 7 вихід
    }
  } else {
    timer7Active = 1; // _tim7I; // Активувати таймер 7
    timer7PrevMillis = millis(); // _tim7P; // Зберегти час таймера 7
  }
} else {
  timer7Output = 0; // _tim7O; // Скинути таймер 7 вихід
  timer7Active = 0; // _tim7I; // Скинути таймер 7
}

tempFlag28 = (timer7Output && (!alarmFlag2)); // _gtv28 = ((_tim7O) && (!(_gtv25))); // Прапорець роботи

if ((!alarmFlag2) && alarmFlag) { // !_gtv25 && _gtv24; // Перевірка дозволу на периферію
  timer4Output = 1; // _tim4O; // Активувати таймер 4 вихід
  timer4Active = 1; // _tim4I; // Активувати таймер 4
} else {
  if (timer4Active) { // _tim4I; // Таймер 4 активний
    timer4Active = 0; // _tim4I; // Скинути таймер 4
    timer4PrevMillis = millis(); // _tim4P; // Зберегти час таймера 4
  } else {
    if (timer4Output) { // _tim4O; // Таймер 4 вихід активний
      if (isTimer(timer4PrevMillis, alarmStopDelayMs)) timer4Output = 0; // _isTimer(_tim4P, _gtv27); // Перевірка затримки перед аварійною зупинкою
    }
  }
}

if (alarmFlag) { // _gtv24; // Периферія активна
  alarmFlag = timer4Output; // _gtv24 = _tim4O; // Оновити прапорець периферії
}

if ((!alarmFlag2) && (!(columnTemp >= columnPeripheralSwitchTemp))) { // !_gtv25 && !(_gtv9 >= _gtv19); // Перевірка аварійної ситуації
  if (timer2Active) { // _tim2I; // Таймер 2 активний
    if (isTimer(timer2PrevMillis, 20000)) { // _isTimer(_tim2P, 20000); // Таймер 2 — 20 сек
      timer2Output = 1; // _tim2O; // Активувати таймер 2 вихід
    }
  } else {
    timer2Active = 1; // _tim2I; // Активувати таймер 2
    timer2PrevMillis = millis(); // _tim2P; // Зберегти час таймера 2
  }
} else {
  timer2Output = 0; // _tim2O; // Скинути таймер 2 вихід
  timer2Active = 0; // _tim2I; // Скинути таймер 2
}

tempFlag41 = (timer2Output && alarmFlag); // _gtv41 = ((_tim2O) && (_gtv24)); // Прапорець аварії

if (columnTemp >= columnPeripheralSwitchTemp) { // _gtv9 >= _gtv19; // Перевірка порогу роботи
  switchFlag10 = (columnTemp >= columnPeripheralSwitchTemp); // _swi10 = (_gtv9) >= (_gtv19); // Прапорець для роботи
} else {
  switchFlag10 = tempFlag29; // _swi10 = _gtv29; // Запасний прапорець роботи
}

if (finishFlag) { // _gtv39; // Перевірка прапорця розгону
  timer8Output = 1; // _tim8O; // Активувати таймер 8 вихід
  timer8Active = 1; // _tim8I; // Активувати таймер 8
} else {
  if (timer8Active) { // _tim8I; // Таймер 8 активний
    timer8Active = 0; // _tim8I; // Скинути таймер 8
    timer8PrevMillis = millis(); // _tim8P; // Зберегти час таймера 8
  } else {
    if (timer8Output) { // _tim8O; // Таймер 8 вихід активний
      if (isTimer(timer8PrevMillis, peripheralOffDelayMs)) timer8Output = 0; // _isTimer(_tim8P, _gtv38); // Перевірка затримки вимкнення периферії
    }
  }
}

tempFlag30 = (switchFlag10 && alarmFlag && timer8Output); // _gtv30 = ((_swi10) && (_gtv24) && (_tim8O)); // Прапорець периферії

digitalWrite(PERIPHERY_OUTPUT_PIN, (tempFlag30 ^ invertPeripheralOutput)); // digitalWrite(PERIPHERY_OUTPUT_PIN, ((_gtv30) ^ (_gtv35))); // Керування периферією
digitalWrite(RAZGON_OUTPUT_PIN, (tempFlag41 ^ invertRazgonOutput)); // digitalWrite(RAZGON_OUTPUT_PIN, ((_gtv41) ^ (_gtv34))); // Керування розгоном
// digitalWrite(RABOTA_OUTPUT_UNUSED_PIN, (tempFlag28 ^ invertWorkOutput)); // digitalWrite(RABOTA_OUTPUT_UNUSED_PIN, ((_gtv28) ^ (_gtv36))); // Керування роботою (закоментовано)
digitalWrite(AVARIA_OUTPUT_PIN, (alarmFlag ^ invertAlarmOutput)); // digitalWrite(AVARIA_OUTPUT_PIN, ((_gtv24) ^ (_gtv37))); // Керування аварією
////Serial.println("Loop after Плата10");
//Плата:11
//Наименование:Вывод 1 строки дисплея

if ( tempFlag5 == 1) { // _gtv5; // Перевірка чи потрібно оновити рядок дисплея
  if (displayMiddleMode) { // _gtv46; // Чи відображати аварійну температуру
    switchValue11 = alarmTemp; // _swi11 = _gtv15; // Встановити аварійну температуру для відображення
  } else {
    switchValue11 = atmPressure; // _swi11 = _gtv17; // Встановити атмосферний тиск для відображення
  }
  if (1) {
    dispTempLength = ((floatToStringWitRaz(columnTemp, 1)) + String(" ") + (floatToStringWitRaz(switchValue11, 1)) + String(" ") + (floatToStringWitRaz(cubeTemp, 1))).length(); // _dispTempLength1 = (((((_floatToStringWitRaz(_gtv9, 1))) + (String(" ")) + ((_floatToStringWitRaz(_swi11, 1))) + (String(" ")) + ((_floatToStringWitRaz(_gtv14, 1)))))).length(); // Розрахунок довжини рядка для дисплея
    if (dispOldLength3 > dispTempLength) { // _disp3oldLength > _dispTempLength1; // Якщо попередня довжина більша — треба очистити дисплей
      needClearDisplay = 1; // _isNeedClearDisp1; // Прапорець очистки дисплея
    }
    dispOldLength3 = dispTempLength; // _disp3oldLength = _dispTempLength1; // Оновити попередню довжину рядка
    mainDisplay.setCursor(int((16 - dispTempLength) / 2), 0); // _lcd1.setCursor(int((16 - _dispTempLength1) / 2), 0); // Встановити курсор для центрування
 if (!encoderMenuActive) {
    mainDisplay.print(floatToStringWitRaz(columnTemp, 1));
    mainDisplay.print(" ");
    mainDisplay.print(floatToStringWitRaz(switchValue11, 1));
    mainDisplay.print(" ");
    mainDisplay.print(floatToStringWitRaz(cubeTemp, 1));
		}
  } else {
    if (dispOldLength3 > 0) { // _disp3oldLength > 0; // Якщо щось було — треба очистити дисплей
      needClearDisplay = 1; // _isNeedClearDisp1; // Прапорець очистки дисплея
      dispOldLength3 = 0; // _disp3oldLength = 0; // Скинути довжину
    }
  }
  tempFlag5 = 0; // _gtv5 = 0; // Скинути прапорець оновлення дисплея
}
//Serial.println("Loop after Плата11");
//Плата:12
//Наименование:Контроль оновлення значень для 2 строки дисплея

if (changeNumber5Output) { // _changeNumber5_Out; // Скидаємо прапорець зміни числа 5
  changeNumber5Output = 0; // _changeNumber5_Out = 0;
} else {
  tempFloat = alarmTemp; // _tempVariable_float = _gtv15; // Зберігаємо поточну аварійну температуру
  if (tempFloat != changeNumber5Value) { // _tempVariable_float != _changeNumber5_OLV; // Якщо значення змінилось
    changeNumber5Value = tempFloat; // _changeNumber5_OLV = _tempVariable_float; // Оновити значення
    changeNumber5Output = 1; // _changeNumber5_Out = 1; // Встановити прапорець зміни
  }
}

if (changeNumber1Output) { // _changeNumber1_Out; // Скидаємо прапорець зміни числа 1
  changeNumber1Output = 0; // _changeNumber1_Out = 0;
} else {
  tempInt = tempInt2; // _tempVariable_int = _gtv2; // Зберігаємо поточне цілочисельне значення
  if (tempInt != changeNumber1Value) { // _tempVariable_int != _changeNumber1_OLV; // Якщо значення змінилось
    changeNumber1Value = tempInt; // _changeNumber1_OLV = _tempVariable_int; // Оновити значення
    changeNumber1Output = 1; // _changeNumber1_Out = 1; // Встановити прапорець зміни
  }
}

if (bitChange1Output) { // _BitChange_1_Out; // Скидаємо прапорець біта зміни 1
  bitChange1Output = 0; // _BitChange_1_Out = 0;
} else {
  if (!(bitChange1OldState == tempFlag12)) { // !(_BitChange_1_OldSt == _gtv12); // Перевіряємо зміну стану
    bitChange1OldState = tempFlag12; // _BitChange_1_OldSt = _gtv12; // Оновити стан
    bitChange1Output = 1; // _BitChange_1_Out = 1; // Встановити прапорець зміни
  }
}

if (bitChange2Output) { // _BitChange_2_Out; // Скидаємо прапорець біта зміни 2
  bitChange2Output = 0; // _BitChange_2_Out = 0;
} else {
  if (!(bitChange2OldState == tempFlag33)) { // !(_BitChange_2_OldSt == _gtv33); // Перевіряємо зміну стану
    bitChange2OldState = tempFlag33; // _BitChange_2_OldSt = _gtv33; // Оновити стан
    bitChange2Output = 1; // _BitChange_2_Out = 1; // Встановити прапорець зміни
  }
}

if ((changeNumber5Output || changeNumber1Output) || (bitChange1Output || bitChange2Output)) { // (((((_changeNumber5_Out) || (_changeNumber1_Out))) || (((_BitChange_1_Out) || (_BitChange_2_Out))))); // Якщо змінилась хоча б одна змінна
  timer6Output = 1; // _tim6O = 1; // Активуємо таймер 6
  timer6Active = 1; // _tim6I = 1; // Прапорець таймера 6
} else {
  if (timer6Active) { // _tim6I; // Таймер 6 активний
    timer6Active = 0; // _tim6I = 0; // Скинути таймер 6
    timer6PrevMillis = millis(); // _tim6P = millis(); // Зберегти час таймера 6
  } else {
    if (timer6Output) { // _tim6O; // Таймер 6 вихід активний
      if (isTimer(timer6PrevMillis, 100)) timer6Output = 0; // _isTimer(_tim6P, 100); // Перевірка таймера 6 на 100 мс
    }
  }
}

if (timer6Output) { // _tim6O; // Таймер 6 вихід активний
  tempFlag31 = 1; // _gtv31 = 1; // Встановити прапорець для оновлення дисплея
}
//Serial.println("Loop after Плата12");
//Плата:13
//Наименование:2 строка "Автоматический режим"
if (tempFlag33 == 1) { // _gtv33; // Автоматичний режим
  if (tempFlag12) { // _gtv12; // Стан автоматичного режиму
    switchString5 = String("START"); // _swi5 = String("START"); // Вивід "СТАРТ"
  } else {
    switchString5 = String("STOP "); // _swi5 = String("STOP "); // Вивід "СТОП"
  }
  if (alarmFlag) { // _gtv24; // Якщо немає аварії
    tempString11 = (floatToStringWitRaz(tempInt2 / 10.23, 1)) + String("% ") + switchString5 + String(" ") + floatToStringWitRaz(alarmTemp, 0); // _gtv11 = (((_floatToStringWitRaz((_gtv2) / (10.23), 1))) + (String("% ")) + (_swi5) + (String(" ")) + ((_floatToStringWitRaz(_gtv15, 0)))); // Формуємо рядок для дисплея
  }
  finishFlag2 = ((tempInt2 <= pwmFinishValue) && (tempInt2 >= 16)); // _gtv44 = (((_gtv2) <= (_gtv42)) && ((_gtv2) >= (16))); // Перевірка завершення перегонки
}

//Плата:14
//Наименование:2 строка "Ручной режим"
if (tempFlag33 == 0) { // _gtv33; // Ручний режим
  if (tempInt2 >= 2) { // _gtv2 >= 2; // Якщо значення більше 2
    switchString1 = floatToStringWitRaz(tempInt2 / 10.23, 1) + String("% ") + String("Manual ") + floatToStringWitRaz(alarmTemp, 0); // _swi1 = (((_floatToStringWitRaz((_gtv2) / (10.23), 1))) + (String("% ")) + (String("Manual ")) + ((_floatToStringWitRaz(_gtv15, 0)))); // Формуємо рядок для ручного режиму
  } else {
    switchString1 = String("realKraft.ua"); // _swi1 = String("realKraft.ua"); // Вивід рекламного рядка
  }
  if (alarmFlag) { // _gtv24; // Якщо немає аварії
    tempString11 = switchString1; // _gtv11 = _swi1; // Встановити рядок на дисплей
  }
  finishFlag2 = (cubeTemp >= cubeFinishTemp); // _gtv44 = (_gtv14) >= (_gtv43); // Перевірка завершення перегонки (ручний режим)
}

//Плата:15
//Наименование:Перегонка завершена
if (finishFlag == 0) { // _gtv39; // Перегонка завершена
  if (!(String("END DISTILLATION").equals(tempString11))) { // !(((String("END DISTILLATION")).equals(_gtv11))); // Якщо рядок не "END DISTILLATION"
    tempFlag31 = 1; // _gtv31 = 1; // Прапорець оновлення
  }
  if (!(String("END DISTILLATION").equals(tempString11))) { // !(((String("END DISTILLATION")).equals(_gtv11))); // Якщо рядок не "END DISTILLATION"
    tempString11 = String("END DISTILLATION"); // _gtv11 = String("END DISTILLATION"); // Встановити завершальний рядок
  }
  tempFlag33 = 0; // _gtv33 = 0; // Скинути автоматичний режим
  alarmTempLimit = 110.00; // _gtv8 = 110.00; // Встановити межу аварійної температури
  tempInt2 = 0; // _gtv2 = 0; // Скинути значення
  pwmCoarseFlag = 0; // _gtv3 = 0; // Скинути прапорець грубого ШИМу
  pwmFineFlag = 0; // _gtv4 = 0; // Скинути прапорець дрібного ШИМу
}

//Плата:16
//Наименование:Аварийная остановка
if (alarmFlag == 0) { // _gtv24; // Аварійна зупинка
  if (!(String("!STOP AVAR STOP!").equals(tempString11))) { // !(((String("!STOP AVAR STOP!")).equals(_gtv11))); // Якщо рядок не "STOP AVAR STOP!"
    tempFlag31 = 1; // _gtv31 = 1; // Прапорець оновлення
  }
  if (!(String("!STOP AVAR STOP!").equals(tempString11))) { // !(((String("!STOP AVAR STOP!")).equals(_gtv11))); // Якщо рядок не "STOP AVAR STOP!"
    tempString11 = String("!STOP AVAR STOP!"); // _gtv11 = String("!STOP AVAR STOP!"); // Встановити аварійний рядок
  }
  tempFlag33 = 0; // _gtv33 = 0; // Скинути автоматичний режим
  alarmTempLimit = 110.00; // _gtv8 = 110.00; // Встановити межу аварійної температури
  tempInt2 = 0; // _gtv2 = 0; // Скинути значення
  pwmCoarseFlag = 0; // _gtv3 = 0; // Скинути прапорець грубого ШИМу
  pwmFineFlag = 0; // _gtv4 = 0; // Скинути прапорець дрібного ШИМу
  tempFlag20 = 0; // _gtv20 = 0; // Скинути прапорець 20
  tempFlag30 = 0; // _gtv30 = 0; // Скинути прапорець 30
  finishFlag = 0; // _gtv39 = 0; // Скинути прапорець завершення перегонки
}
//Плата:18
//Наименование:Вывод 2 строки дисплея

if ( tempFlag31 == 1){ // _gtv31; // Перевірка чи потрібно оновити 2 рядок дисплея
  if (1) {
    dispTempLength = (tempString11).length(); // _dispTempLength1 = ((_gtv11)).length(); // Визначення довжини 2 рядка
    if (dispOldLength > dispTempLength) { // _disp1oldLength > _dispTempLength1; // Якщо попередня довжина більша — треба очистити дисплей
      needClearDisplay = 1; // _isNeedClearDisp1 = 1; // Прапорець очистки дисплея
    }
    dispOldLength = dispTempLength; // _disp1oldLength = _dispTempLength1; // Оновити попередню довжину
    mainDisplay.setCursor(int((16 - dispTempLength) / 2), 1); // _lcd1.setCursor(int((16 - _dispTempLength1) / 2), 1); // Встановити курсор для центрування
if (!encoderMenuActive) {
    mainDisplay.print(tempString11); // _lcd1.print((_gtv11));
		}
  } else {
    if (dispOldLength > 0) { // _disp1oldLength > 0; // Якщо щось було — треба очистити дисплей
      needClearDisplay = 1; // _isNeedClearDisp1 = 1; // Прапорець очистки дисплея
      dispOldLength = 0; // _disp1oldLength = 0; // Скинути довжину
    }
  }
  tempFlag31 = 0; // _gtv31 = 0; // Скинути прапорець оновлення 2 рядка
}
//Serial.print (encoderMenuActive);
handleEncoderMenu();
}
// ------------------- УТИЛІТИ -------------------
float stringToFloat(String value) { // _stringToFloat; // Конвертація строки у float
  return value.toFloat();
}
String floatToStringWitRaz(float value, int raz) { // _floatToStringWitRaz; // Конвертація float у строку з заданою точністю
  return String(value, raz);
}
bool isTimer(unsigned long startTime, unsigned long period) { // _isTimer; // Перевірка періоду таймера
  unsigned long currentTime;
  currentTime = millis();
  if (currentTime >= startTime) {
    return (currentTime >= (startTime + period));
  } else {
    return (currentTime >= (4294967295 - startTime + period));
  }
}
ISR(TIMER2_OVF_vect) { // ISR(TIMER2_OVF_vect); // Обробник переповнення таймера 2
  TCNT2 = 100;
  if (powerDownCount >= 1000) { // _PWDC >= 1000; // Лічильник вимкнень живлення
    asm volatile("jmp 0x0000");
  } else {
    powerDownCount = powerDownCount + 1; // _PWDC = _PWDC + 1;
  };
}
void readByteFromUART(byte data, int port) { // _readByteFromUART; // Читання байта з UART
  if (port == 0) {
    rvfu3Data += char(data); // _RVFU3Data += char(data);
   
  }
  if (port == 100) {
    rvfu1Data += char(data); // _RVFU1Data += char(data);
  }
  Serial.println(rvfu3Data);
  Serial.println(rvfu1Data); // Serial.println(_RVFU1Data);
}
float convertDS18x2xData(byte type_s, byte data[12]) { // _convertDS18x2xData; // Конвертація даних з DS18B20
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
// _readDS18_ow2 — читання температури з DS18B20 через OneWire
// Нова назва: readDS18TempOW2
// Стара назва: _readDS18_ow2
// Читає дані з датчика, перевіряє CRC, повертає температуру або код помилки

float readDS18TempOW2(byte addr[8], byte type_s) { // Стара назва: _readDS18_ow2
  byte data[12];
  byte i;
  oneWireBus.reset();                // Стара назва: oneWire.reset()
  oneWireBus.select(addr);           // Стара назва: oneWire.select(addr)
  oneWireBus.write(0xBE);            // Стара назва: oneWire.write(0xBE)
  for (i = 0; i < 9; i++) {
    data[i] = oneWireBus.read();     // Стара назва: oneWire.read()
  }
  oneWireBus.reset();                // Стара назва: oneWire.reset()
  oneWireBus.select(addr);           // Стара назва: oneWire.select(addr)
  oneWireBus.write(0x44);            // Стара назва: oneWire.write(0x44)
  if (oneWireBus.crc8(data, 8) != data[8]) { // Стара назва: oneWire.crc8(data, 8)
    return 501;                      // Код помилки CRC
  }
  return convertDS18x2xData(type_s, data); // Стара назва: _convertDS18x2xData
}

// _checkFreeLogicBlockOutput — перевірка блоку вільної логіки
// Нова назва: checkFreeLogicBlockOutput
// Стара назва: _checkFreeLogicBlockOutput
// Перевіряє співпадіння масивів станів та входів, повертає результат

bool checkFreeLogicBlockOutput(bool inArray[], int inArraySize, bool stArray[], int stArraySize) { // Стара назва: _checkFreeLogicBlockOutput
  int inIndex = 0;
  bool result = 1;
  for (int i = 0; i < stArraySize; i = i + 1) {
    if (!(inArray[inIndex] == stArray[i])) {
      result = 0;
    }
    inIndex++;
    if (inIndex == inArraySize) {
      if (result) {
        return 1;
      } else {
        result = 1;
      }
      inIndex = 0;
    }
  }
  return 0;
}

// funcUB_185384122 — оновлення структури UB_185384122 згідно нового значення

void funcUB_185384122(struct UB_185384122 *ubInstance, float inputValue) {
  float uboValue = ubInstance->uboValue;     // Стара назва: ubo_120270494
  bool uboFlag = ubInstance->uboFlag;        // Стара назва: ubo_13416262
  float gtv1Value = ubInstance->gtv1Value;   // Стара назва: _gtv1
  float gtv2Value = ubInstance->gtv2Value;   // Стара назва: _gtv2

  uboValue = inputValue;                     // Оновлюємо значення структури
  uboFlag = ((gtv1Value < inputValue) && (inputValue < gtv2Value)); // Перевіряємо, чи входить у діапазон

  ubInstance->uboValue = uboValue;           // Записуємо нове значення (старе: _ubo_120270494)
  ubInstance->uboFlag = uboFlag;             // Записуємо прапорець діапазону (старе: _ubo_13416262)
  ubInstance->gtv1Value = gtv1Value;         // Зберігаємо межу діапазону 1 (старе: _gtv1)
  ubInstance->gtv2Value = gtv2Value;         // Зберігаємо межу діапазону 2 (старе: _gtv2)
  // Логіка: оновлення структури та перевірка, чи нове значення в заданому діапазоні
}
// ------------------- EEPROM -------------------

// isEEPROMInitialized — перевірка ініціалізації EEPROM
// Стара назва: isEEPROMInitialized
// Перевіряє прапорець ініціалізації

bool isEEPROMInitialized() {
  return EEPROM.read(0) == 1;
}

// loadAddresses — зчитування адрес датчиків з EEPROM
// Стара назва: loadAddresses
// Завантажує адреси датчиків у tempSensorAddresses

void loadAddresses() {
  for (byte i = 0; i < SENSOR_COUNT; i++) {
    for (byte j = 0; j < 8; j++) {
      tempSensorAddresses[i][j] = EEPROM.read(1 + i * 8 + j); // Стара назва: sensorAddresses
    }
  }
}

// saveAddresses — запис адрес датчиків у EEPROM
// Стара назва: saveAddresses
// Зберігає адреси датчиків з tempSensorAddresses

void saveAddresses() {
  EEPROM.write(0, 1);  // прапорець ініціалізації
  for (byte i = 0; i < SENSOR_COUNT; i++) {
    for (byte j = 0; j < 8; j++) {
      EEPROM.write(1 + i * 8 + j, tempSensorAddresses[i][j]); // Стара назва: sensorAddresses
    }
  }
}

// scanAndSaveSensors — сканування та збереження адрес датчиків температури
// Стара назва: scanAndSaveSensors
// Сканує OneWire, зберігає знайдені адреси

void scanAndSaveSensors() {
  oneWireBus.reset_search(); // Стара назва: oneWire.reset_search()
  uint8_t addr[8];           // Стара назва: addr
  byte i = 0;

  while (oneWireBus.search(addr) && i < SENSOR_COUNT) { // Стара назва: oneWire.search(addr)
    if (isValidAddress(addr)) { // Стара назва: isValidAddress
      memcpy(tempSensorAddresses[i], addr, 8); // Стара назва: sensorAddresses
      i++;
    }
  }

  saveAddresses(); // Збереження адрес у EEPROM

  for (byte j = 0; j < i; j++) {
   // Serial.print(tempSensorNames[j]); Serial.print(": "); // Стара назва: sensorNames
   // printAddress(tempSensorAddresses[j]); // Стара назва: sensorAddresses
  }

  if (i < SENSOR_COUNT) {
  //  Serial.println("УВАГА: знайдено менше 3-х датчиків!"); // Попередження якщо датчиків менше ніж треба
  }
}

// isValidAddress — перевірка чи адреса датчика коректна
// Стара назва: isValidAddress
// Перевірка CRC та типу датчика (DS18B20)

bool isValidAddress(uint8_t* addr) {
  return (OneWire::crc8(addr, 7) == addr[7]) && (addr[0] == 0x28);
}

// printAddress — вивід HEX-адреси датчика
// Стара назва: printAddress
// Виводить адресу датчика у HEX-форматі

// void printAddress(uint8_t* addr) {
  // for (byte i = 0; i < 8; i++) {
    // if (addr[i] < 16) Serial.print("0");
    // Serial.print(addr[i], HEX);
    // Serial.print(" ");
  // }
  // Serial.println();
// }
void softwarePWM(uint8_t pin, uint8_t value, uint16_t freq_hz, unsigned long &lastTime, bool &state) {
  unsigned long period = 1000000UL / freq_hz; // період у мкс
  unsigned long highTime = (period * value) / 255;
  unsigned long lowTime = period - highTime;
  unsigned long now = micros();
//Serial.println (value);
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