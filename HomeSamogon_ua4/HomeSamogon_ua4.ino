// ============================================================================
//  HomeSamogon_ua3 — Оптимізована версія з виправленою логікою виходів
//
//  ЛОГІКА ВИХОДІВ:
//  ТЕН увімкнений  → AVARIA (A0) = LOW  (0)
//  ТЕН вимкнений  → AVARIA (A0) = HIGH (1)
//  Аварія активна → AVARIA (A0) = HIGH (1)  (незалежно від ТЕН)
//  Норма (ТЕН ON + немає аварії) → AVARIA (A0) = LOW (0)
//
//  РОЗГІН (A1):
//  Працює ТІЛЬКИ якщо tenEnabled=true
//  При tenEnabled=false → розгін примусово вимкнений
//
//  ПЕРИФЕРІЯ (A2): інвертована (invertPeripheralOutput=1)
//    tempFlag30=1 → A2=LOW  (увімкнена)
//    tempFlag30=0 → A2=HIGH (вимкнена)
//
//  РОЗГІН (A1): інвертований (invertRazgonOutput=1)
//    tempFlag41=1 → A1=LOW  (увімкнений)
//    tempFlag41=0 → A1=HIGH (вимкнений)
//
//  КЛАПАН (A3): HIGH=закритий, LOW=відкритий
//    При tenEnabled=false → завжди HIGH (закритий)
//
//  Рядок String замінені на char[] буфери
//  для усунення фрагментації heap на ATmega328P (2KB RAM)
// ============================================================================
//  Піни:
//  2  — ENCODER CLK
//  3  — БУЗЕР (активний, HIGH=звук)
//  4  — ENCODER DT
//  5  — ENCODER SW (кнопка)
//  6  — OneWire шина (DS18B20)
//  9  — ДАТЧИК ПРОТІЧКИ (INPUT_PULLUP, LOW=протічка)
//  11 — SoftSerial TX → Bluetooth
//  12 — SoftSerial RX ← Bluetooth
//  A0 — АВАРІЯ    (HIGH=аварія або ТЕН OFF, LOW=норма і ТЕН ON)
//  A1 — РОЗГІН    (інверт: LOW=увімк, HIGH=вимк)
//  A2 — ПЕРИФЕРІЯ (інверт: LOW=увімк, HIGH=вимк)
//  A3 — КЛАПАН    (HIGH=закритий, LOW=відкритий)
//  A4 — SDA I2C
//  A5 — SCL I2C
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
#define BUZZER_PIN              3    // Бузер: HIGH=звук ON
#define ONE_WIRE_BUS_PIN        6    // OneWire шина датчиків DS18B20
#define VALVE_RELAY_DIRECT_PIN  A3   // Клапан: HIGH=закритий, LOW=відкритий
#define PERIPHERY_OUTPUT_PIN    A2   // Периферія (вода): LOW=увімк (інверт)
#define RAZGON_OUTPUT_PIN       A1   // Розгін: LOW=увімк (інверт)
#define AVARIA_OUTPUT_PIN       A0   // Аварія: HIGH=проблема, LOW=норма
#define SOFT_SERIAL_TX_PIN      11   // Bluetooth TX
#define SOFT_SERIAL_RX_PIN      12   // Bluetooth RX
#define RAIN_LEAK_INPUT_PIN     9    // Датчик протічки: LOW=протічка (PULLUP)
#define ENCODER_CLK             2    // Енкодер CLK
#define ENCODER_DT              4    // Енкодер DT
#define ENCODER_SW              5    // Енкодер кнопка SW

// ─── Константи ───────────────────────────────────────────────────────────────
#define SENSOR_COUNT  3              // Кількість датчиків DS18B20
#define UART_BUF_SIZE 64             // Розмір UART буфера (байт)

// ─── Периферійні об'єкти ─────────────────────────────────────────────────────
SFE_BMP180 bmeSensor;                // Датчик атмосферного тиску BMP180
long bmpPressure = 0;                // Тиск з BMP180 у Па*100 (сирий)
SoftwareSerial BtSerial(SOFT_SERIAL_RX_PIN, SOFT_SERIAL_TX_PIN); // Bluetooth UART
OneWire oneWireBus(ONE_WIRE_BUS_PIN);              // OneWire шина
LiquidCrystal_I2C mainDisplay(0x27, 16, 2);        // LCD 16x2 по I2C адресі 0x27
EncButton enc(ENCODER_CLK, ENCODER_DT, ENCODER_SW);// Енкодер з кнопкою

// ─── Адреси датчиків температури ─────────────────────────────────────────────
uint8_t tempSensorAddresses[SENSOR_COUNT][8];       // Масив 8-байтних адрес DS18B20
const char* tempSensorNames[SENSOR_COUNT] = {"Куб", "Колона", "ТСА"}; // Назви датчиків
byte columnSensorAddr[8];   // Адреса датчика колони  (tempSensorAddresses[0])
byte cubeSensorAddr[8];     // Адреса датчика куба    (tempSensorAddresses[1])
byte alarmSensorAddr[8];    // Адреса датчика аварії  (tempSensorAddresses[2])

// ─── Структура UB: фільтр вхідного значення (діапазон gtv1..gtv2) ────────────
// Якщо inputValue потрапляє в діапазон (gtv1, gtv2) — uboFlag=true, uboValue=inputValue
struct UB_185384122 {
  float uboValue   = 0.00;   // Відфільтроване вихідне значення
  bool  uboFlag    = 0;      // true = значення в допустимому діапазоні
  float gtv1Value  = 1.00;   // Нижня межа діапазону (не включно)
  float gtv2Value  = 105.00; // Верхня межа діапазону (не включно)
};
UB_185384122 ubDataInstance2;  // Фільтр для датчика колони
UB_185384122 ubDataInstance3;  // Фільтр для датчика куба
UB_185384122 ubDataInstance4;  // Фільтр для датчика аварії
float ubDataUbi = 0.00;        // Тимчасовий вхід для funcUB_185384122

// ─── Режим керування клапаном ────────────────────────────────────────────────
// 0 = дискретний (реле ON/OFF)
// 1 = програмний ШІМ (softwarePWM)
int controlMode = 0;

// ─── UART буфери (замість String — char[] для економії heap) ─────────────────
char    uartBuf0[UART_BUF_SIZE];    // Буфер прийому Serial (USB, port 0)
uint8_t uartBuf0Len = 0;            // Поточна довжина буфера Serial
char    uartBuf100[UART_BUF_SIZE];  // Буфер прийому BtSerial (Bluetooth, port 100)
uint8_t uartBuf100Len = 0;          // Поточна довжина буфера BtSerial

// ─── Буфери для дисплея та UART передачі ─────────────────────────────────────
char displayLowerBuf[20]; // Контрольна сума/хеш для нижнього рядка LCD та пакету
char tempStr11Buf[20];    // Рядок 2 LCD: "TEN OFF", "END DISTILLATION", "!STOP AVAR STOP!" тощо

// ─── Змінні дисплея ──────────────────────────────────────────────────────────
int  dispTempLength  = 0;   // Поточна довжина рядка що виводиться (для центрування)
bool needClearDisplay = 0;  // Прапор: потрібно очистити LCD перед наступним виводом
int  dispOldLength   = 0;   // Попередня довжина рядка 2 (для визначення потреби clear)
int  dispOldLength3  = 0;   // Попередня довжина рядка 1

// ─── UART: прапори прийому байту ─────────────────────────────────────────────
int avlDfu0   = 0;  // 1 = щойно прийнятий байт з Serial (port 0), чекає обробки
int avlDfu100 = 0;  // 1 = щойно прийнятий байт з BtSerial (port 100), чекає обробки

// ─── Логіка вільних блоків ───────────────────────────────────────────────────
bool freeLogicInputArr[2];              // Вхідний масив для checkFreeLogicBlockOutput
bool freeLogicQ1StateArr[] = { 0, 1 };  // Еталонний масив станів (tempInt2<=0, tempInt2>=1023)
bool freeLogicQ1 = 0;                   // Вихід блоку вільної логіки (стан клапана при крайніх значеннях)

// ─── Watchdog лічильник ──────────────────────────────────────────────────────
volatile int powerDownCount = 0; // Лічильник ISR TIMER2: якщо >=1000 → перезапуск МК

// ─── Прапори наявності датчиків ──────────────────────────────────────────────
// columnSensorFlag: true = датчик колони (DS18B20) НЕ підключений (немає UART від ESP)
// cubeSensorFlag:   тимчасовий — true = є новий UART байт для декодування
bool columnSensorFlag = 1; // true = UART відсутній (локальний режим DS18B20)
bool cubeSensorFlag   = 1; // true = є новий UART пакет для обробки

// ─── Температури ─────────────────────────────────────────────────────────────
float columnTemp = 0; // Температура колони (°C), з DS18B20 або UART
float cubeTemp   = 0; // Температура куба   (°C), з DS18B20 або UART
float alarmTemp  = 0; // Температура аварійного датчика (°C), DS18B20 [3-й]

// ─── Тиск ────────────────────────────────────────────────────────────────────
float atmPressure = 760;          // Атмосферний тиск (мм рт.ст.), з BMP180
float cubeTempCorrection = 0;     // Корекція температури куба (°C, додається до cubeTemp)
bool  pressureCorrectionEnabled = 1; // true = враховувати поправку тиску на температуру кипіння
float pressureValue = 0;          // Поправка температури від тиску (°C)
float pressureSensorValue = 0;    // Тиск при ініціалізації (еталон для розрахунку поправки)
bool  pressureSensorInitialized = false; // true = еталонний тиск вже записаний (по команді *)

// ─── Параметри спрацювання датчиків ─────────────────────────────────────────
float vaporSensorTriggerTemp = 65; // Температура (°C) спрацювання датчика пари (alarmTemp)
                                   // Якщо alarmTemp > цього → alarmFlag2=true → аварія

// ─── Параметри завершення перегонки ─────────────────────────────────────────
int   pwmFinishValue  = 82;    // ШІМ клапана (%) при якому вважається "кінець" (авто режим)
float cubeFinishTemp  = 98;    // Температура куба (°C) при якій кінець (ручний режим)
int   finishDelayMs   = 30000; // Затримка (мс) після finishFlag2 перед finishFlag=0

// ─── Параметри аварійної зупинки ─────────────────────────────────────────────
int   alarmStopDelayMs = 30000; // Затримка (мс) після alarmFlag2=true перед alarmFlag=0

// ─── Параметри периферії (вода/охолодження) ─────────────────────────────────
float columnPeripheralSwitchTemp = 40; // Температура колони (°C) при якій авто вмикається вода
long  peripheralOffDelayMs = 300000;   // Затримка (мс=5хв) після кінця перегонки перед вимк. води
bool  invertPeripheralOutput = 0;      // true = інверсія виходу A2 (LOW=вода увімкнена)
bool  invertRazgonOutput     = 0;      // true = інверсія виходу A1 (LOW=розгін увімкнений)
bool  invertWorkOutput       = 0;      // true = інверсія виходу роботи (не використовується)
bool  invertAlarmOutput      = 0;      // false = НЕ інвертуємо AVARIA (HIGH=аварія)

// ─── Параметри дисплея ───────────────────────────────────────────────────────
bool displayMiddleMode = 1; // true = у середині рядка 1 показувати alarmTemp
                            // false = показувати atmPressure

// ─── Параметри ШІМ клапана ───────────────────────────────────────────────────
int pwmPeriodMs = 4000; // Період ШІМ клапана (мс), регулюється меню/UART

// ─── Аварійні прапори ────────────────────────────────────────────────────────
bool alarmFlag  = 1; // ГОЛОВНИЙ ПРАПОР АВАРІЇ:
                     //   1 = норма (система працює штатно)
                     //   0 = аварійна зупинка (протікання або пара або температура)
bool alarmFlag2 = 0; // ДЖЕРЕЛО АВАРІЇ (поточний стан):
                     //   0 = аварійних умов немає
                     //   1 = є аварія (протічка OR alarmTemp > vaporSensorTriggerTemp)
                     // Після 30 сек з alarmFlag2=1 → alarmFlag стає 0

// ─── Температурна сигналізація ───────────────────────────────────────────────
float alarmTempLimit = 110; // Ліміт температури куба (°C):
                            // якщо cubeTemp >= alarmTempLimit → зумер через 5 сек

// ─── Параметри автоматичного регулювання (ESP/Bluetooth керування) ────────��──
float pwmValue1 = 777; // Нижня межа температури колони для авто-ШІМ (СТОП)
                       // Встановлюється командою & по UART. 777 = не задано
float pwmValue2 = 777; // Верхня межа температури колони для авто-ШІМ (СТАРТ)
                       // Встановлюється командою * по UART. 777 = не задано

// ─── ТЕН (нагрівальний елемент) ──────────────────────────────────────────────
bool tenEnabled = false; // СТАН ТЕН:
                         //   false = вимкнений (за замовчуванням при старті)
                         //   true  = увімкнений
                         // Керується командою UART: !1=увімк, !0=вимк
                         // При аварії автоматично → false
                         // ВПЛИВ:
                         //   false → клапан закритий, ШІМ не працює
                         //   false → розгін (A1) примусово вимкнений
                         //   false → AVARIA (A0) = HIGH

// ─── ШІМ клапана ─────────────────────────────────────────────────────────────
bool tempFlag33 = 0;   // РЕЖИМ АВТО ШІМ:
                       //   0 = ручний режим (tempInt2 керується вручну)
                       //   1 = авто режим (ESP керує клапаном автоматично)
                       // Встановлюється командою $ по UART (тільки якщо tenEnabled=true)
int  tempInt2   = 0;   // ЗНАЧЕННЯ ШІМ КЛАПАНА (0..1023):
                       //   0    = клапан повністю закритий
                       //   1023 = клапан повністю відкритий
                       //   Між  = ШІМ регулювання
                       // Встановлюється командою # по UART або меню
bool pwmCoarseFlag = 0; // Грубий ШІМ сигнал (від генератора3 або freeLogicQ1)
bool pwmFineFlag   = 0; // true = крайнє значення (0 або 1023), без ШІМ генератора

// ─── Прапори стану перегонки ─────────────────────────────────────────────────
bool tempFlag12  = 0; // СТАН КЛАПАНА в авто режимі:
                      //   1 = клапан ВІДКРИТИЙ (START — колона в нормі)
                      //   0 = клапан ЗАКРИТИЙ  (STOP  — колона перегріта)
bool finishFlag  = 1; // ПРАПОР АКТИВНОСТІ ПЕРЕГОНКИ:
                      //   1 = перегонка іде (норма)
                      //   0 = перегонка завершена (END DISTILLATION)
bool finishFlag2 = 0; // УМОВА ЗАВЕРШЕННЯ:
                      //   true = виконано умову кінця (ШІМ <= pwmFinishValue або cubeTemp >= cubeFinishTemp)
                      //   Через finishDelayMs → finishFlag=0

// ─── Допоміжні прапори стану ─────────────────────────────────────────────────
bool tempFlag28 = 0; // Прапор: timer7 спрацював І немає аварії (не використовується активно)
bool tempFlag20 = 1; // Прапор дозволу роботи (скидається при аварії)
bool tempFlag41 = 0; // РОЗГІН: true = умови розгону виконані (колона холодна + норма + ТЕН ON)
                     //   tempFlag41=1 → A1=LOW (розгін увімкнений, з інверсією)
                     //   tempFlag41=0 → A1=HIGH (розгін вимкнений)
bool tempFlag29 = 0; // РУЧНЕ КЕРУВАННЯ ВОДОЮ:
                     //   0 = вода вимкнена (ручний режим, якщо колона < switchTemp)
                     //   1 = вода увімкнена вручну
                     // Встановлюється командою ^ по UART або меню
bool tempFlag30 = 0; // ПЕРИФЕРІЯ (вода/охолодження):
                     //   true = всі умови виконані → вода увімкнена
                     //   false = вода вимкнена
bool tempFlag5  = 1; // true = потрібно оновити рядок 1 LCD (температури змінились)
bool tempFlag31 = 1; // true = потрібно оновити рядок 2 LCD (стан змінився)

// ─── UART: прапори скидання буферів ─────────────────────────────────────────
bool rvfu3Reset  = 1;  // true = буфер Serial вже скинутий (чекає нового циклу)
bool rvfu1Reset  = 1;  // true = буфер BtSerial вже скинутий
bool triggerFlag2b = 0; // true = активний порт Serial (USB), false = BtSerial (Bluetooth)
bool triggerFlag2  = 0; // Проміжний: стан команди ^ (вода ручна)
bool triggerFlag3  = 0; // true = колона перевищила pwmValue1 (починаємо рахувати таймер1)
bool triggerFlag3b = 0; // Проміжний: стан команди $ (авто режим)

// ─── Стан бузера та підсвічування ────────────────────────────────────────────
bool pzs2OES = 0; // true = бузер зараз HIGH (захист від зайвих digitalWrite)
bool d1b2    = 0; // true = підсвічування LCD зараз увімкнене

// ─── Перемикачі ──────────────────────────────────────────────────────────────
float switchValue11 = 0; // Значення для середини рядка 1 LCD (alarmTemp або atmPressure)
bool  switchFlag4   = 0; // Вихід мультиплексора ШІМ (freeLogicQ1 або generator3Output)
bool  switchFlag9   = 0; // Фінальний сигнал керування клапаном (після timer1)
bool  switchFlag10  = 0; // Дозвіл подачі води:
                         //   true = колона >= switchTemp (авто) або tempFlag29=1 (ручна)
                         //   false = колона холодна і ручне вимкнено

// ─── Таймери (загальна структура кожного) ────────────────────────────────────
// timerXActive:     true = таймер зараз відраховує
// timerXOutput:     вихід таймера (1 поки активний або затримка після)
// timerXPrevMillis: мітка часу початку затримки

// Таймер 1: затримка 5 сек між triggerFlag3 і зупинкою клапана (tempFlag12=0)
bool          timer1Active = 0, timer1Output = 0;
unsigned long timer1PrevMillis = 0UL;

// Таймер 2: затримка 20 сек перед увімкненням розгону (колона холодна + норма)
bool          timer2Active = 0, timer2Output = 0;
unsigned long timer2PrevMillis = 0UL;

// Таймер 3: затримка finishDelayMs (30 сек) після finishFlag2 → finishFlag=0
bool          timer3Active = 0, timer3Output = 0;
unsigned long timer3PrevMillis = 0UL;

// Таймер 4: затримка alarmStopDelayMs (30 сек) після alarmFlag2=1 → alarmFlag=0
bool          timer4Active = 0, timer4Output = 0;
unsigned long timer4PrevMillis = 0UL;

// Таймер 5: затримка 5 сек перед увімкненням зумера (щоб не було хибних спрацювань)
bool          timer5Active = 0, timer5Output = 0;
unsigned long timer5PrevMillis = 0UL;

// Таймер 6: затримка 100 мс після останньої зміни рядка 2 LCD
bool          timer6Active = 0, timer6Output = 0;
unsigned long timer6PrevMillis = 0UL;

// Таймер 7: затримка 10 сек після початку нормальної роботи (для tempFlag28)
bool          timer7Active = 0, timer7Output = 0;
unsigned long timer7PrevMillis = 0UL;

// Таймер 8: peripheralOffDelayMs (5 хв) затримка вимкнення води після кінця перегонки
bool          timer8Active = 0, timer8Output = 0;
unsigned long timer8PrevMillis = 0UL;

// Таймер 10: UART таймаут 1 сек — якщо немає байтів → columnSensorFlag=true (локальний режим)
bool          timer10Active = 0, timer10Output = 0;
unsigned long timer10PrevMillis = 0UL;

// ─── Генератори (меандри) ────────────────────────────────────────────────────
// generator1: меандр бузера (200 мс HIGH / 400 мс LOW)
bool          generator1Active = 0, generator1Output = 0;
unsigned long generator1PrevMillis = 0UL;

// generator3: ШІМ клапана (HIGH=tempInt2/1023*period, LOW=залишок)
bool          generator3Active = 0, generator3Output = 0;
unsigned long generator3PrevMillis = 0UL;

// ─── Детектори зміни значень (аналог блоку "зміна числа") ───────────────────
// changeNumberXOutput: true = значення змінилось у цьому циклі (імпульс на 1 цикл)
// changeNumberXValue:  попереднє значення для порівняння
bool  changeNumber1Output = 0;  int   changeNumber1Value = 0; // tempInt2 (ШІМ)
bool  changeNumber2cubeTemp = 0; float changeNumber2Value = 0; // cubeTemp
bool  changeNumber4Output = 0;  float changeNumber4Value = 0; // atmPressure (для поправки тиску)
bool  changeNumber5Output = 0;  float changeNumber5Value = 0; // alarmTemp
bool  changeNumber6columnTemp = 0; float changeNumber6Value = 0; // columnTemp
bool  changeNumber7atmPressure = 0; float changeNumber7Value = 0; // atmPressure (для LCD)

// ─── Детектори зміни бітів (аналог блоку "зміна біта") ─────────────────────
// bitChangeXOutput:   true = значення змінилось (імпульс на 1 цикл)
// bitChangeXOldState: попередній стан
bool bitChange1OldState = 0, bitChange1Output = 0; // tempFlag12 (стан клапана)
bool bitChange2OldState = 0, bitChange2Output = 0; // tempFlag33 (авто режим)
bool bitChange4OldState = 0, bitChange4Output = 0; // tempFlag12 (зменшення ШІМ при зміні)

// ─── Датчики DS18B20: час останнього читання та значення ────────────────────
unsigned long columnSensorReadTime = 0UL;  // Час останнього читання колони (мс)
float         columnSensorValue    = 0.00; // Сире значення з датчика колони (до фільтра)
unsigned long cubeSensorReadTime   = 0UL;  // Час останнього читання куба (мс)
float         cubeSensorValue      = 0.00; // Сире значення з датчика куба (до фільтра)
unsigned long alarmSensorReadTime  = 0UL;  // Час останнього читанн�� аварії (мс)
float         alarmSensorValue     = 0.00; // Сире значення аварійного датчика (до фільтра)
unsigned long bmpSensorReadTime2   = 0UL;  // Час останнього читання BMP180 (мс)

// ─── Таймери надсилання телеметрії ───────────────────────────────────────────
unsigned long stou1 = 0UL; // Час останнього пакету до BtSerial (Bluetooth)
unsigned long stou2 = 0UL; // Час останнього пакету до Serial (WiFi/USB)

// ─── Тимчасові змінні (для уникнення повторних обчислень) ───────────────────
byte  tempByte  = 0; // Тимчасовий байт (для BMP180 та інших)
int   tempInt   = 0; // Тимчасове ціле
float tempFloat = 0; // Тимчасовий float

// ─── Програмний ШІМ клапана (для controlMode=1) ─────────────────────────────
unsigned long valvePwmLastTime = 0;    // Час останнього перемикання softwarePWM (мкс)
bool          valvePwmState    = false;// Поточний стан виходу softwarePWM (true=HIGH)

// ═══════════════════════════════════════════════════════════════════════════
//  ТИПИ ТА ОГОЛОШЕННЯ ВПЕРЕД
// ═══════════════════════════════════════════════════════════════════════════

// Тип значення пункту меню
enum MenuValueType : uint8_t {
  TYPE_INT,   // Ціле число (int)
  TYPE_FLOAT, // Дробове число (float)
  TYPE_BOOL   // Булеве (bool): показується як "On"/"Off"
};

// Секція меню
enum MenuSection : uint8_t {
  MENU_SETUP, // Секція налаштувань (Setup)
  MENU_WORK   // Секція роботи (Work)
};

// Рівень головного меню
enum MainMenuLevel : uint8_t {
  MAIN_MENU,  // Головне меню (вибір секції)
  SUB_MENU    // Підменю (пункти секції)
};

// ─── Оголошення функцій вперед ───────────────────────────────────────────────
bool  isTimer(unsigned long startTime, unsigned long period);
float readDS18TempOW2(byte addr[8], byte type_s);
float convertDS18x2xData(byte type_s, byte data[12]);
bool  checkFreeLogicBlockOutput(bool inArray[], int inArraySize, bool stArray[], int stArraySize);
void  funcUB_185384122(struct UB_185384122 *ubInstance, float inputValue);
bool  isEEPROMInitialized();
void  loadAddresses();
void  saveAddresses();
void  scanAndSaveSensors();
bool  isValidAddress(uint8_t* addr);
void  softwarePWM(uint8_t pin, uint8_t value, uint16_t freq_hz, unsigned long &lastTime, bool &state);

// ─── Helper: друк float через Print (без String, без heap) ───────────────────
// Використовується для надсилання даних по UART без динамічної пам'яті
void printFloat(Print &out, float val, uint8_t dec) {
  char buf[12];
  dtostrf(val, 0, dec, buf); // float → рядок з dec знаками після крапки
  out.print(buf);
}

// ─── Helper: округлення float до N знаків після крапки ──────────────────────
// Використовується для обчислення контрольної суми пакету
float roundFloat(float val, uint8_t dec) {
  float mult = 1;
  for (uint8_t i = 0; i < dec; i++) mult *= 10;
  long rounded = (long)(val * mult + (val >= 0 ? 0.5f : -0.5f));
  return rounded / mult;
}

// ═══════════════════════════════════════════════════════════════════════════
//  РОЗРАХУНОК ВМІСТУ СПИРТУ (таблиці у PROGMEM — не займають RAM)
// ══════════════════════════════════════════════��════════════════════════════

// ─── Таблиця куба (рідина): температура → % спирту, діапазон 0..65% ─────────
// Формат: { температура °C, % спирту }
// Температура СПАДАЄ зі зростанням % спирту
// Для уточнення — просто змінюй значення або додавай рядки
static const float cubeAlcTable[][2] PROGMEM = {
  { 100.0,  0.0 },
  {  97.0,  5.0 },
  {  94.3, 10.0 },
  {  93.5, 15.0 },
  {  91.8, 20.0 },
  {  90.0, 25.0 },
  {  88.1, 30.0 },
  {  86.2, 35.0 },
  {  84.2, 40.0 },
  {  82.6, 45.0 },
  {  81.3, 50.0 },
  {  80.2, 55.0 },
  {  79.3, 60.0 },
  {  78.7, 65.0 },
};
#define CUBE_ALC_TABLE_SIZE (sizeof(cubeAlcTable) / sizeof(cubeAlcTable[0]))

// ─── Таблиця колони (пара): температура → % спирту, діапазон 77..100% ───────
// Формат: { температура °C, % спирту }
// Температура СПАДАЄ зі зростанням % спирту
// Для уточнення — просто змінюй значення або додавай рядки
static const float columnAlcTable[][2] PROGMEM = {
  { 100.00, 77.0 },
  {  99.55, 78.0 },
  {  99.08, 79.0 },
  {  98.60, 80.0 },
  {  98.09, 81.0 },
  {  97.57, 82.0 },
  {  97.02, 83.0 },
  {  96.45, 84.0 },
  {  95.85, 85.0 },
  {  95.22, 86.0 },
  {  94.55, 87.0 },
  {  93.84, 88.0 },
  {  93.09, 89.0 },
  {  92.28, 90.0 },
  {  91.42, 91.0 },
  {  90.50, 92.0 },
  {  89.50, 93.0 },
  {  88.43, 94.0 },
  {  87.27, 95.0 },
  {  85.99, 96.0 },
  {  84.57, 97.0 },
  {  82.97, 98.0 },
  {  81.11, 99.0 },
  {  78.15, 100.0 },
};
#define COLUMN_ALC_TABLE_SIZE (sizeof(columnAlcTable) / sizeof(columnAlcTable[0]))

// ─── Загальна функція лінійної інтерполяції по таблиці ───────────────────────
// table[][0] = температура (спадає по рядках)
// table[][1] = % спирту   (росте по рядках)
// Повертає % спирту для заданої температури temp
float interpAlcTable(float temp, const float table[][2], uint8_t size) {
  // Якщо температура вище першого рядка (занадто гаряча) — повертаємо мін. %
  float t0 = pgm_read_float(&table[0][0]);
  float a0 = pgm_read_float(&table[0][1]);
  if (temp >= t0) return a0;

  // Якщо температура нижче останнього рядка (занадто холодна) — повертаємо макс. %
  float tLast = pgm_read_float(&table[size - 1][0]);
  float aLast = pgm_read_float(&table[size - 1][1]);
  if (temp <= tLast) return aLast;

  // Шукаємо два сусідні рядки між якими лежить temp
  for (uint8_t i = 0; i < size - 1; i++) {
    float t1 = pgm_read_float(&table[i][0]);
    float t2 = pgm_read_float(&table[i + 1][0]);
    if (temp <= t1 && temp >= t2) {
      float a1 = pgm_read_float(&table[i][1]);
      float a2 = pgm_read_float(&table[i + 1][1]);
      // Лінійна інтерполяція між двома точками
      return a1 + (a2 - a1) * (t1 - temp) / (t1 - t2);
    }
  }
  return 0.0;
}

// ─── Куб: % спирту по температурі рідини ────────────────────────────────────
float calcCubeAlcohol(float temp) {
  return interpAlcTable(temp, cubeAlcTable, CUBE_ALC_TABLE_SIZE);
}

// ─── Колона: % спирту по температурі пари ───────────────────────────────────
float calcColumnAlcohol(float temp) {
  return interpAlcTable(temp, columnAlcTable, COLUMN_ALC_TABLE_SIZE);
}
// ═══════════════════════════════════════════════════════════════════════════
//  МЕНЮ (PROGMEM оптимізоване — рядки у флеш, не в RAM)
// ═══════════════════════════════════════════════════════════════════════════

// ─── Рядки назв пунктів меню у PROGMEM ──────────────────────────────────────
const char title_klapan[]    PROGMEM = "Klapan/motor";   // Режим клапана (0=реле, 1=ШІМ)
const char title_pwmkonec[]  PROGMEM = "PwmKonec";       // ШІМ при якому кінець перегонки
const char title_kubkonec[]  PROGMEM = "KubKonec";       // Температура куба кінець
const char title_vodavkl[]   PROGMEM = "VodaVkl temp";   // Температура вмикання води
const char title_alarm[]     PROGMEM = "Alarm Temp Limit";// Ліміт температури куба (зумер)
const char title_water[]     PROGMEM = "Water/temp";     // Режим відображення середини LCD
const char title_pwmperiod[] PROGMEM = "pwmPeriodMs";    // Період ШІМ клапана (мс)
const char title_waterflag[] PROGMEM = "Water Flag";     // Ручне керування водою (0/1)
const char title_shimklapam[]PROGMEM = "shim klapam";    // Ручне значення ШІМ клапана

// ─── Таблиця покажчиків на рядки меню (у PROGMEM) ───────────────────────────
const char* const menuTitles[] PROGMEM = {
  title_klapan,    // індекс 0
  title_pwmkonec,  // індекс 1
  title_kubkonec,  // індекс 2
  title_vodavkl,   // індекс 3
  title_alarm,     // індекс 4
  title_water,     // індекс 5
  title_pwmperiod, // індекс 6
  title_waterflag, // індекс 7
  title_shimklapam // індекс 8
};
#define MENU_ITEMS_COUNT 9 // Загальна кількість пунктів меню

// ─── Структура одного пункту меню ────────────────────────────────────────────
struct MenuItem {
  uint8_t       titleIdx;   // Індекс у menuTitles[] (рядок назви у PROGMEM)
  void*         valuePtr;   // Покажчик на змінну що редагується
  MenuValueType valueType;  // Тип змінної: TYPE_INT, TYPE_FLOAT, TYPE_BOOL
  int16_t       step;       // Крок зміни при обертанні енкодера
  int16_t       minVal;     // Мінімальне допустиме значення
  int16_t       maxVal;     // Максимальне допустиме значення
  void (*onConfirm)();      // Callback після підтвердження (hold) — nullptr якщо не потрібен
  MenuSection   section;    // Секція: MENU_SETUP або MENU_WORK
};

// ─── Масив пунктів меню ──────────────────────────────────────────────────────
MenuItem menuItems[MENU_ITEMS_COUNT] = {
  // titleIdx, valuePtr,                    type,        step, min,  max,    onConfirm, section
  { 0, &controlMode,                TYPE_INT,   1,    0,    1,    nullptr, MENU_SETUP }, // Klapan/motor: 0=реле, 1=ШІМ
  { 1, &pwmFinishValue,             TYPE_INT,   1,    0,    100,  nullptr, MENU_SETUP }, // PwmKonec: ШІМ% кінець авто
  { 2, &cubeFinishTemp,             TYPE_FLOAT, 1,    90,   100,  nullptr, MENU_SETUP }, // KubKonec: °C кінець ручний
  { 3, &columnPeripheralSwitchTemp, TYPE_FLOAT, 1,    30,   100,  nullptr, MENU_SETUP }, // VodaVkl temp: °C авто вода
  { 4, &alarmTempLimit,             TYPE_FLOAT, 1,    60,   120,  nullptr, MENU_SETUP }, // Alarm Temp Limit: °C зумер
  { 5, &displayMiddleMode,          TYPE_BOOL,  1,    0,    1,    nullptr, MENU_SETUP }, // Water/temp: 0=тиск, 1=аварТемп
  { 6, &pwmPeriodMs,                TYPE_INT,   250,  1000, 8000, nullptr, MENU_SETUP }, // pwmPeriodMs: мс
  { 7, &tempFlag29,                 TYPE_BOOL,  1,    0,    1,    nullptr, MENU_WORK  }, // Water Flag: ручна вода
  { 8, &tempInt2,                   TYPE_INT,   1,    0,    1000, nullptr, MENU_WORK  }, // shim klapam: ШІМ вручну
};

// ─── Рядки назв секцій головного меню у PROGMEM ─────────────────────────────
const char menuTitleSetup[] PROGMEM = "Setup"; // Секція налаштувань
const char menuTitleWork[]  PROGMEM = "Work";  // Секція роботи
const char* const mainMenuTitles[] PROGMEM = { menuTitleSetup, menuTitleWork };

// ─── Стан меню ───────────────────────────────────────────────────────────────
bool          encoderMenuActive = false;    // true = меню активне (відображається на LCD)
MainMenuLevel menuLevel         = MAIN_MENU;// Поточний рівень: головне або підменю
uint8_t       mainMenuIndex     = 0;        // Індекс вибраної секції (0=Setup, 1=Work)
uint8_t       subMenuIndex      = 0;        // Індекс вибраного пункту всередині секції
bool          editMode          = false;    // true = режим редагування значення (hold)

// ─── Підрахунок кількості пунктів у секції ───────────────────────────────────
uint8_t getSectionCount(MenuSection section) {
  uint8_t count = 0;
  for (uint8_t i = 0; i < MENU_ITEMS_COUNT; ++i)
    if (menuItems[i].section == section) count++;
  return count;
}

// ─── Отримання глобального індексу пункту за секцією та позицією ─────────────
// Повертає індекс у menuItems[] або -1 якщо не знайдено
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

// ─── Читання назви пункту меню з PROGMEM у RAM буфер ────────────────────────
void getMenuTitle(uint8_t titleIdx, char* buf, uint8_t bufSize) {
  strncpy_P(buf, (PGM_P)pgm_read_ptr(&menuTitles[titleIdx]), bufSize - 1);
  buf[bufSize - 1] = 0;
}

// ─── Читання назви секції головного меню з PROGMEM у RAM буфер ──────────────
void getMainMenuTitle(uint8_t mainMenuIdx, char* buf, uint8_t bufSize) {
  strncpy_P(buf, (PGM_P)pgm_read_ptr(&mainMenuTitles[mainMenuIdx]), bufSize - 1);
  buf[bufSize - 1] = 0;
}

// ─── Відображення поточного стану меню на LCD ────────────────────────────────
void displayMenu() {
  mainDisplay.clear();
  char buf[17]; // Буфер для рядка LCD (16 символів + \0)

  if (menuLevel == MAIN_MENU) {
    // Головне меню: рядок 1 = "Menu:", рядок 2 = назва секції
    mainDisplay.setCursor(0, 0);
    mainDisplay.print(F("Menu:"));
    mainDisplay.setCursor(0, 1);
    getMainMenuTitle(mainMenuIndex, buf, sizeof(buf));
    mainDisplay.print(buf);
    return;
  }

  // Підменю: шукаємо пункт
  int8_t menuItemIndex = getMenuItemIndex(
    mainMenuIndex == 0 ? MENU_SETUP : MENU_WORK, subMenuIndex);
  if (menuItemIndex < 0 || menuItemIndex >= MENU_ITEMS_COUNT) {
    mainDisplay.setCursor(0, 0);
    mainDisplay.print(F("No items"));
    return;
  }

  MenuItem &item = menuItems[menuItemIndex];

  // Рядок 1: назва пункту + "[E]" якщо в режимі редагування
  getMenuTitle(item.titleIdx, buf, sizeof(buf));
  mainDisplay.setCursor(0, 0);
  mainDisplay.print(buf);
  if (editMode) mainDisplay.print(F(" [E]"));

  // Рядок 2: поточне значення
  mainDisplay.setCursor(0, 1);
  switch (item.valueType) {
    case TYPE_INT:   mainDisplay.print(*(int*)item.valuePtr); break;
    case TYPE_FLOAT: mainDisplay.print(*(float*)item.valuePtr, 1); break;
    case TYPE_BOOL:  mainDisplay.print(*(bool*)item.valuePtr ? "On" : "Off"); break;
  }
}

// ─── Обробка енкодера: навігація по меню та редагування значень ──────────────
void handleEncoderMenu() {
  // ── Подвійний клік — увімкнути/вимкнути меню ──────────────────────────
  if (enc.hasClicks(2)) {
    encoderMenuActive = !encoderMenuActive;
    menuLevel     = MAIN_MENU;
    editMode      = false;
    mainMenuIndex = 0;
    subMenuIndex  = 0;

    if (encoderMenuActive) {
      // Входимо в меню → показуємо меню
      displayMenu();
    } else {
      // ВИХОДИМО з меню → очищаємо і одразу перемальовуємо обидва рядки
      mainDisplay.clear();
      tempFlag5  = 1; // Примусово оновити рядок 1 (температури)
      tempFlag31 = 1; // Примусово оновити рядок 2 (стан)
      // Скидаємо старі довжини щоб не було помилкового needClearDisplay
      dispOldLength  = 0;
      dispOldLength3 = 0;
    }
    return;
  }

  // ── 1 клік поза меню — перемикання ТЕН ───────────────────────────────
  if (!encoderMenuActive && enc.click()) {
    tenEnabled = !tenEnabled;
    if (!tenEnabled) {
      tempInt2   = 0;
      tempFlag33 = 0;
    }
    tempFlag31 = 1;
    return;
  }

  if (!encoderMenuActive) return;

  // ── Головне меню ──────────────────────────────────────────────────────
  if (menuLevel == MAIN_MENU) {
    uint8_t count = 2;
    if (enc.right()) { mainMenuIndex = (mainMenuIndex + 1) % count; displayMenu(); }
    if (enc.left())  { mainMenuIndex = (mainMenuIndex - 1 + count) % count; displayMenu(); }
    if (enc.click()) {
      menuLevel    = SUB_MENU;
      editMode     = false;
      subMenuIndex = 0;
      displayMenu();
    }
    return;
  }

  // ── Підменю ───────────────────────────────────────────────────────────
  MenuSection currentSection = (mainMenuIndex == 0) ? MENU_SETUP : MENU_WORK;
  uint8_t sectionCount = getSectionCount(currentSection);

  if (enc.hold()) { editMode = !editMode; displayMenu(); return; }

  if (!editMode) {
    if (enc.right()) { subMenuIndex = (subMenuIndex + 1) % sectionCount; displayMenu(); }
    if (enc.left())  { subMenuIndex = (subMenuIndex - 1 + sectionCount) % sectionCount; displayMenu(); }
    if (enc.click()) {
      menuLevel = MAIN_MENU;
      editMode  = false;
      displayMenu();
    }
    return;
  }

  // ── Режим редагування ─────────────────────────────────────────────────
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
}//  UART ПРИЙОМ
// ═══════════════════════════════════════════════════════════════════════════

// ─── Запис одного байту у відповідний буфер ──────────────────────────────────
// port=0   → Serial (USB/WiFi)
// port=100 → BtSerial (Bluetooth)
// Захист від переповнення: записуємо тільки якщо є місце (< UART_BUF_SIZE-1)
void readByteFromUART(byte data, int port) {
  if (port == 0) {
    if (uartBuf0Len < UART_BUF_SIZE - 1) {
      uartBuf0[uartBuf0Len++] = (char)data;
      uartBuf0[uartBuf0Len]   = '\0'; // Завжди тримаємо null-термінатор
    }
  } else if (port == 100) {
    if (uartBuf100Len < UART_BUF_SIZE - 1) {
      uartBuf100[uartBuf100Len++] = (char)data;
      uartBuf100[uartBuf100Len]   = '\0';
    }
  }
}

// ═══════════════════════════════════════════════════════════════════════════
//  ДЕКОДУВАННЯ UART КОМАНД
// ═══════════════════════════════════════════════════════════════════════════
//
//  Протокол: рядок може містити кілька команд одночасно
//  Кожна команда = символ-префікс + значення
//
//  Таблиця команд:
//  #value  → tempInt2 (ШІМ клапана 0..1023)     — тільки якщо tenEnabled=true
//  @value  → alarmTempLimit (ліміт темп. зумера)  — завжди
//  *value  → pwmValue2 (верхня межа авто-ШІМ)     — завжди, + фіксує еталонний тиск
//  &value  → pwmValue1 (нижня межа авто-ШІМ)      — завжди
//  $value  → tempFlag33 (авто режим 0/1)           — тільки якщо tenEnabled=true
//  ^value  → tempFlag29 (ручна вода 0/1)           — завжди
//  %value  → displayMiddleMode (середина LCD 0/1)  — завжди
//  :value  → pwmPeriodMs (період ШІМ мс)           — завжди
//  ;value  → pwmFinishValue (ШІМ% кінець авто)     — завжди
//  |value  → cubeFinishTemp (°C кінець ручний)     — завжди
//  !value  → tenEnabled (1=увімк, 0=вимк ТЕН)      — завжди
// ═══════════════════════════════════════════════════════════════════════════

void decodeUartCommand(const char* cmd) {
  const char* p;

  // ── # → tempInt2 (ШІМ клапана 0..1023) ──────────────────────────────────
  // Ігнорується якщо ТЕН вимкнений (безпека: не можна керувати клапаном без ТЕН)
  p = strchr(cmd, '#');
  if (p && tenEnabled) {
    tempInt2 = atoi(p + 1);
  }

  // ── @ → alarmTempLimit (температура спрацювання зумера, °C) ─────────────
  // Приймається завжди (незалежно від стану ТЕН)
  p = strchr(cmd, '@');
  if (p) {
    alarmTempLimit = (int)atof(p + 1);
  }

  // ── * → pwmValue2 (верхня межа авто-ШІМ) + фіксація еталонного тиску ───
  // При першому отриманні — зберігає поточний тиск як еталон для поправки
  p = strchr(cmd, '*');
  if (p) {
    pressureSensorValue     = atmPressure; // Зберігаємо поточний тиск як еталон
    pressureSensorInitialized = true;      // Дозволяємо розрахунок поправки тиску
    pwmValue2 = atof(p + 1);
  }

  // ── & → pwmValue1 (нижня межа авто-ШІМ, температура СТОП колони) ────────
  p = strchr(cmd, '&');
  if (p) {
    pwmValue1 = atof(p + 1);
  }

  // ── $ → tempFlag33 (авто режим ШІМ: 0=ручний, 1=авто) ──────────────────
  // Ігнорується якщо ТЕН вимкнений
  p = strchr(cmd, '$');
  if (p && tenEnabled) {
    char val = *(p + 1);
    if (val == '0') triggerFlag3b = 0;
    if (val == '1') triggerFlag3b = 1;
    tempFlag33 = triggerFlag3b;
  }

  // ── ^ → tempFlag29 (ручне керування водою: 0=вимк, 1=увімк) ────────────
  // Працює незалежно від ТЕН (воду можна вмикати і без нагріву)
  p = strchr(cmd, '^');
  if (p) {
    char val = *(p + 1);
    if (val == '0') triggerFlag2 = 0;
    if (val == '1') triggerFlag2 = 1;
    tempFlag29 = triggerFlag2;
  }

  // ── % → displayMiddleMode (що показувати в середині рядка 1 LCD) ────────
  // 0 = атмосферний тиск, 1 = температура аварійного датчика
  p = strchr(cmd, '%');
  if (p) {
    displayMiddleMode = atoi(p + 1);
  }

  // ── : → pwmPeriodMs (повний період ШІМ клапана в мілісекундах) ──────────
  // Наприклад 4000 = 4 сек цикл (HIGH+LOW разом)
  p = strchr(cmd, ':');
  if (p) {
    pwmPeriodMs = atoi(p + 1);
  }

  // ── ; → pwmFinishValue (ШІМ% при якому авто режим вважає кінець) ────────
  // Якщо tempInt2 <= pwmFinishValue і tempInt2 >= 16 → finishFlag2=true
  p = strchr(cmd, ';');
  if (p) {
    pwmFinishValue = atoi(p + 1);
  }

  // ── | → cubeFinishTemp (температура куба °C кінець у ручному режимі) ────
  // Якщо cubeTemp >= cubeFinishTemp → finishFlag2=true (ручний режим)
  p = strchr(cmd, '|');
  if (p) {
    cubeFinishTemp = atof(p + 1);
  }

  // ── ! → tenEnabled (увімкнення/вимкнення ТЕН дистанційно) ───────────────
  // !1 = увімкнути ТЕН (дозволити нагрів і керування клапаном)
  // !0 = вимкнути ТЕН:
  //       - клапан закривається
  //       - ШІМ скидається в 0
  //       - авто режим вимикається
  //       - AVARIA (A0) → HIGH
  //       - розгін (A1) → вимкнений
  //       - алармові таймери продовжують роботу штатно
  p = strchr(cmd, '!');
  if (p) {
    char val = *(p + 1);
    if (val == '1') {
      tenEnabled = true;  // Дозволити роботу ТЕН
    }
    if (val == '0') {
      tenEnabled = false; // Заборонити роботу ТЕН
      tempInt2   = 0;     // Скинути ШІМ → клапан закриється
      tempFlag33 = 0;     // Вимкнути авто режим
      // Увага: alarmFlag, таймери аварії та периферії продовжують роботу
    }
  }
}

// ═══════════════════════════════════════════════════════════════════════════
//  ПЕРЕДАЧА UART ПАКЕТУ (короткий — для Bluetooth)
// ═══════════════════════════════════════════════════════════════════════════
//
//  Формат: "HomeSamogon.ru/4.8,col,atm,cube,auto,valve,p1,p2,shim,alLim,hash,alT,alF2,water,%,"
//
//  Поля:
//  col   — температура колони (°C, 1 знак)
//  atm   — атмосферний тиск (мм рт.ст., 1 знак)
//  cube  — температура куба (°C, 1 знак)
//  auto  — tempFlag33: 1=авто режим, 0=ручний
//  valve — tempFlag12: 1=клапан відкритий (START), 0=закритий (STOP)
//  p1    — pwmValue1 - pressureValue (нижня межа з поправкою тиску)
//  p2    — pwmValue2 - pressureValue (верхня межа з поправкою тиску)
//  shim  — tempInt2 (поточне значення ШІМ 0..1023)
//  alLim — alarmTempLimit (ліміт зумера, 2 знаки)
//  hash  — контрольна сума: (cube+3.14)*(col+atm)
//  alT   — alarmTemp (температура аварійного датчика, 1 знак)
//  alF2  — alarmFlag2: 1=є аварія, 0=норма
//  water — tempFlag30: 1=вода увімкнена, 0=вимкнена
// ═══════════════════════════════════════════════════════════════════════════

void sendDataPacket(Print &out) {
  // Обчислюємо контрольну суму (хеш для перевірки цілісності пакету)
  float ct1    = roundFloat(cubeTemp, 1);
  float colt1  = roundFloat(columnTemp, 1);
  float atm1   = roundFloat(atmPressure, 1);
  float lowerVal = (ct1 + 3.14f) * (colt1 + atm1);
  dtostrf(lowerVal, 0, 1, displayLowerBuf); // Зберігаємо у глобальний буфер

  out.print(F("HomeSamogon.ru/4.8,"));
  printFloat(out, columnTemp, 1);                    out.print(','); // col
  printFloat(out, atmPressure, 1);                   out.print(','); // atm
  printFloat(out, cubeTemp, 1);                      out.print(','); // cube
  out.print(tempFlag33 ? '1' : '0');                 out.print(','); // auto
  out.print(tempFlag12 ? '1' : '0');                 out.print(','); // valve
  printFloat(out, pwmValue1 - pressureValue, 1);     out.print(','); // p1
  printFloat(out, pwmValue2 - pressureValue, 1);     out.print(','); // p2
  out.print(tempInt2, DEC);                          out.print(','); // shim
  printFloat(out, alarmTempLimit, 2);                out.print(','); // alLim
  out.print(displayLowerBuf);                        out.print(','); // hash
  printFloat(out, alarmTemp, 1);                     out.print(','); // alT
  out.print(alarmFlag2 ? '1' : '0');                 out.print(','); // alF2
  out.print(tempFlag30 ? '1' : '0');                 out.print(','); // water
  out.print(F("%,"));                                               // кінець пакету
}

// ═══════════════════════════════════════════════════════════════════════════
//  ПЕРЕДАЧА UART ПАКЕТУ (розширений — для WiFi/Serial)
// ═══════════════════════════════════════════════════════════════════════════
//
//  Додаткові поля порівняно з sendDataPacket:
//  cubeFinishTemp  — температура куба для кінця (ручний режим)
//  pwmFinishValue  — ШІМ% для кінця (авто режим)
//  pwmPeriodMs     — період ШІМ (мс)
//  tenEnabled      — 1=ТЕН увімкнений, 0=вимкнений
//  finishFlag      — 1=перегонка іде, 0=завершена
//  cubeAlc         — % спирту в кубі (за таблицею)
//  columnAlc       — % спирту в колоні (за таблицею)
// ═══════════════════════════════════════════════════════════════════════════

void sendDataPacketwifi(Print &out) {
  // Обчислюємо контрольну суму (хеш для перевірки цілісності пакету)
  float ct1    = roundFloat(cubeTemp, 1);
  float colt1  = roundFloat(columnTemp, 1);
  float atm1   = roundFloat(atmPressure, 1);
  float lowerVal = (ct1 + 3.14f) * (colt1 + atm1);
  dtostrf(lowerVal, 0, 1, displayLowerBuf);

  out.print(F("HomeSamogon.ru/4.8,"));
  printFloat(out, columnTemp, 1);                    out.print(','); // col
  printFloat(out, atmPressure, 1);                   out.print(','); // atm
  printFloat(out, cubeTemp, 1);                      out.print(','); // cube
  out.print(tempFlag33 ? '1' : '0');                 out.print(','); // auto
  out.print(tempFlag12 ? '1' : '0');                 out.print(','); // valve
  printFloat(out, pwmValue1 - pressureValue, 1);     out.print(','); // p1
  printFloat(out, pwmValue2 - pressureValue, 1);     out.print(','); // p2
  out.print(tempInt2, DEC);                          out.print(','); // shim
  printFloat(out, alarmTempLimit, 2);                out.print(','); // alLim
  out.print(displayLowerBuf);                        out.print(','); // hash
  printFloat(out, alarmTemp, 1);                     out.print(','); // alT
  out.print(alarmFlag2 ? '1' : '0');                 out.print(','); // alF2
  out.print(tempFlag30 ? '1' : '0');                 out.print(','); // water
  printFloat(out, cubeFinishTemp, 1);                out.print(','); // cubeFinishTemp
  out.print(pwmFinishValue, DEC);                    out.print(','); // pwmFinishValue
  out.print(pwmPeriodMs, DEC);                       out.print(','); // pwmPeriodMs
  out.print(tenEnabled ? '1' : '0');                 out.print(','); // tenEnabled: 1=ON, 0=OFF
  out.print(finishFlag ? '1' : '0');                 out.print(','); // finishFlag: 1=іде, 0=кінець
  printFloat(out, calcCubeAlcohol(cubeTemp), 1);     out.print(','); // % спирту куб
  printFloat(out, calcColumnAlcohol(columnTemp), 1); out.print(','); // % спирту колона
  out.println(F("%,"));                                              // кінець пакету
}



// ═══════════════════════════════════════════════════════════════════════════
//  SETUP — ініціалізація при старті
// ═══════════════════════════════════════════════════════════════════════════

void setup() {
  // ─── Ініціалізація виходів ───────────────────────────────────────────────

  // Бузер: початковий стан — вимкнений
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // Датчик протічки: вхід з підтяжкою (LOW = пр��тічка)
  pinMode(RAIN_LEAK_INPUT_PIN, INPUT_PULLUP);

  // Клапан: вихід, при старті — ЗАКРИТИЙ (HIGH)
  // ТЕН вимкнений → клапан завжди закритий до команди !1
  pinMode(VALVE_RELAY_DIRECT_PIN, OUTPUT);
  digitalWrite(VALVE_RELAY_DIRECT_PIN, HIGH);

  // СТАНЕ:
// A2 ПЕРИФЕРІЯ: HIGH = реле не світиться = вода вимкнена
pinMode(PERIPHERY_OUTPUT_PIN, OUTPUT);
digitalWrite(PERIPHERY_OUTPUT_PIN, HIGH);

// A1 РОЗГІН: HIGH = реле не світиться = розгін вимкнений
pinMode(RAZGON_OUTPUT_PIN, OUTPUT);
digitalWrite(RAZGON_OUTPUT_PIN, HIGH);

// A0 AVARIA/ТЕН: LOW = реле світиться = ТЕН відключений (tenEnabled=false при старті)
pinMode(AVARIA_OUTPUT_PIN, OUTPUT);
digitalWrite(AVARIA_OUTPUT_PIN, LOW);
  // ─── Ініціалізація шин та периферії ─────────────────────────────────────

  // I2C шина (для LCD та BMP180)
  Wire.begin();
  delay(10); // Пауза для стабілізації I2C

  // Зовнішній опорний сигнал для АЦП (для точніших аналогових вимірювань)
  analogReference(EXTERNAL);

  // BMP180 — датчик атмосферного тиску
  bmeSensor.begin();

  // Bluetooth UART (9600 бод)
  BtSerial.begin(9600);

  // USB/WiFi UART (115200 бод)
  Serial.begin(115200);

  // LCD 16x2 — ініціалізація, підсвічування вимкнене при старті
  mainDisplay.init();
  mainDisplay.noBacklight(); // Підсвічування вимкнене (увімкнеться коли немає зумера)

  // ─── Ініціалізація таймерів телеметрії ──────────────────────────────────
  stou1 = millis(); // Таймер пакету Bluetooth
  stou2 = millis(); // Таймер пакету WiFi/Serial

  // ─── Очищення UART буферів ───────────────────────────────────────────────
  uartBuf0[0]        = '\0';
  uartBuf100[0]      = '\0';
  displayLowerBuf[0] = '\0';
  tempStr11Buf[0]    = '\0';

  // ─── Ініціалізація адрес датчиків DS18B20 ───────────────────────────────
  // Перевіряємо чи EEPROM вже містить збережені адреси (байт 0 == 1)
  if (!isEEPROMInitialized()) {
    // EEPROM порожній — сканувати шину і зберегти знайдені датчики
    // Порядок збереження: [0]=колона, [1]=куб, [2]=аварія
    scanAndSaveSensors();
  } else {
    // EEPROM має дані — завантажити збережені адреси
    loadAddresses();

    // Додатково: перевіряємо які датчики є на шині прямо зараз
    // Якщо знайдено новий датчик якого немає в EEPROM — додаємо його
    // на перше вільне місце (для автоматичного додавання нових датчиків)
    bool foundFlags[SENSOR_COUNT] = {false}; // Прапори: який датчик знайдено на шині
    oneWireBus.reset_search();
    uint8_t addr[8];
    while (oneWireBus.search(addr)) {
      if (!isValidAddress(addr)) continue; // Пропускаємо датчики з невірним CRC або не 0x28

      // Перевіряємо чи ця адреса вже є в збережених
      bool matched = false;
      for (byte i = 0; i < SENSOR_COUNT; i++) {
        if (memcmp(addr, tempSensorAddresses[i], 8) == 0) {
          foundFlags[i] = true; // Датчик знайдений — позначаємо
          matched = true;
          break;
        }
      }

      // Якщо адреса нова (не збережена) — додаємо на перше вільне місце
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

    // Зберігаємо оновлений список у EEPROM
    saveAddresses();
  }

  // ─── Копіюємо адреси у робочі змінні ────────────────────────────────────
  // tempSensorAddresses[0] → колона
  // tempSensorAddresses[1] → куб
  // tempSensorAddresses[2] → аварія (пара)
  memcpy(columnSensorAddr, tempSensorAddresses[0], 8);
  memcpy(cubeSensorAddr,   tempSensorAddresses[1], 8);
  memcpy(alarmSensorAddr,  tempSensorAddresses[2], 8);
}
// ═══════════════════════════════════════════════════════════════════════════
//  LOOP — головний цикл програми
// ═══════════════════════════════════════════════════════════════════════════

void loop() {
  // ─── Опитування енкодера (має бути першим у циклі для чіткої реакції) ───
  enc.tick();

  // ─── Очищення LCD якщо попередній рядок був довший за поточний ──────────
  // Потрібно щоб не залишались "хвости" від старих довших рядків
  if (needClearDisplay) {
    mainDisplay.clear();
    needClearDisplay = 0;
  }

  // ─── Скидання watchdog лічильника ────────────────────────────────────────
  // ISR TIMER2 збільшує powerDownCount кожен тік
  // Якщо loop() не скидає його — через ~1 сек МК перезапускається
  powerDownCount = 0;

  // ═══════════════════════════════════════════════════════════════════════
  //  ПРИЙОМ UART (по одному байту за цикл з кожного порту)
  // ═══════════════════════════════════════════════════════════════════════
  // Логіка: avlDfuX=1 означає що байт щойно прийнятий і чекає обробки
  // Наступний цикл: скидаємо прапор і читаємо наступний байт
  // Це забезпечує рівномірний прийом без блокування loop()

  // Serial (USB/WiFi, port 0)
  if (avlDfu0) {
    avlDfu0 = 0; // Скидаємо прапор — байт вже оброблений
  } else {
    if (Serial.available()) {
      avlDfu0 = 1;                          // Позначаємо: є новий байт
      readByteFromUART(Serial.read(), 0);   // Додаємо до буфера uartBuf0
    }
  }

  // BtSerial (Bluetooth, port 100)
  if (avlDfu100) {
    avlDfu100 = 0; // Скидаємо прапор
  } else {
    if (BtSerial.available()) {
      avlDfu100 = 1;                            // Позначаємо: є новий байт
      readByteFromUART(BtSerial.read(), 100);   // Додаємо до буфера uartBuf100
    }
  }

  // ═══════════════════════════════════════════════════════════════════════
  //  BMP180 — зчитування атмосферного тиску (кожні 5 секунд)
  // ═══════════════════════════════════════════════════════════════════════
  // Результат: bmpPressure = тиск у Па*100 (ціле для уникнення float помилок)
  // Конвертація в мм рт.ст.: atmPressure = bmpPressure / 133.3
  if (isTimer(bmpSensorReadTime2, 5000)) {
    bmpSensorReadTime2 = millis();
    double bmpTempData, bmpPressData;

    // Крок 1: запускаємо вимірювання температури (потрібна для компенсації тиску)
    tempByte = bmeSensor.startTemperature();
    if (tempByte != 0) {
      delay(tempByte); // Чекаємо завершення вимірювання (мс)

      // Крок 2: читаємо температуру
      tempByte = bmeSensor.getTemperature(bmpTempData);
      if (tempByte != 0) {

        // Крок 3: запускаємо вимірювання тиску (режим 3 = найвища точність)
        tempByte = bmeSensor.startPressure(3);
        if (tempByte != 0) {
          delay(tempByte); // Чекаємо завершення

          // Крок 4: читаємо тиск (з температурною компенсацією)
          tempByte = bmeSensor.getPressure(bmpPressData, bmpTempData);
          if (tempByte != 0) {
            // Зберігаємо як ціле (Па * 100) для уникнення float drift
            bmpPressure = (long)(bmpPressData * 100.0);
          }
        }
      }
    }
  }

  // ═══════════════════════════════════════════════════════════════════════
  //  СКИДАННЯ UART БУФЕРІВ
  // ═══════════════════════════════════════════════════════════════════════
  // Буфери скидаються коли columnSensorFlag=true (немає UART → локальний режим)
  // rvfu3Reset/rvfu1Reset: захист від повторного скидання у тому ж стані

  // Скидання буфера Serial (port 0)
  if (columnSensorFlag) {
    if (!rvfu3Reset) {
      uartBuf0[0]  = '\0';
      uartBuf0Len  = 0;
      rvfu3Reset   = 1; // Позначаємо що вже скинули
    }
  } else {
    rvfu3Reset = 0; // Дозволяємо наступне скидання
  }

  // Скидання буфера BtSerial (port 100)
  if (columnSensorFlag) {
    if (!rvfu1Reset) {
      uartBuf100[0]  = '\0';
      uartBuf100Len  = 0;
      rvfu1Reset     = 1;
    }
  } else {
    rvfu1Reset = 0;
  }

  // ─── Якщо є новий байт — оновити LCD рядок 1 ────────────────────────────
  if (avlDfu100 || avlDfu0) {
    tempFlag5 = 1; // Потрібно оновити рядок 1 LCD
  }

  // cubeSensorFlag: true = є новий UART байт для декодування в цьому циклі
  cubeSensorFlag = (avlDfu100 || avlDfu0);

  // ═══════════════════════════════════════════════════════════════════════
  //  ТАЙМЕР 10 — UART ТАЙМАУТ (визначення режиму: UART або локальний DS18B20)
  // ═══════════════════════════════════════════════════════════════════════
  // Якщо немає UART байтів протягом 1 секунди → columnSensorFlag=true
  // columnSensorFlag=true  → читаємо температури локально з DS18B20
  // columnSensorFlag=false → температури приходять по UART від ESP

  if (avlDfu100 || avlDfu0) {
    // Є UART дані → тримаємо таймер активним
    timer10Output = 1;
    timer10Active = 1;
  } else {
    if (timer10Active) {
      // UART щойно зник → запускаємо відлік таймауту
      timer10Active      = 0;
      timer10PrevMillis  = millis();
    } else {
      // Таймер вже запущений → чекаємо 1 сек
      if (timer10Output) {
        if (isTimer(timer10PrevMillis, 1000)) timer10Output = 0; // Таймаут → вимкнути
      }
    }
  }
  // Інверсія: немає UART → columnSensorFlag=true (локальний режим DS18B20)
  columnSensorFlag = !(timer10Output);

  // ─── Визначення активного UART порту ─────────────────────────────────────
  // triggerFlag2b=true  → активний Serial (USB/WiFi)
  // triggerFlag2b=false → активний BtSerial (Bluetooth)
  if (avlDfu0)   triggerFlag2b = 1;
  if (avlDfu100) triggerFlag2b = 0;

  // ─── Вибір буфера та декодування UART команди ────────────────────────────
  const char* cmdBuf = triggerFlag2b ? uartBuf0 : uartBuf100;

  // Декодуємо тільки коли є новий байт (cubeSensorFlag=true на 1 цикл)
  if (cubeSensorFlag == 1) {
    decodeUartCommand(cmdBuf); // Розбираємо всі команди у рядку
    cubeSensorFlag = 0;        // Скидаємо прапор до наступного байту
  }

  // ═══════════════════════════════════════════════════════════════════════
  //  ЗЧИТУВАННЯ ДАТЧИКІВ DS18B20 (кожні 5 секунд)
  // ═══════════════════════════════════════════════════════════════════════
  // Датчик колони (tempSensorAddresses[0])
  // columnSensorValue → через фільтр UB → columnTemp
  if (isTimer(columnSensorReadTime, 5000)) {
    columnSensorReadTime = millis();
    tempFloat = readDS18TempOW2(columnSensorAddr, 0);
    if (tempFloat < 500) columnSensorValue = tempFloat; // 501 = помилка CRC → ігноруємо
  }
  // Фільтр UB: приймаємо тільки значення в діапазоні (1..105)°C
  ubDataUbi = columnSensorValue;
  funcUB_185384122(&ubDataInstance2, ubDataUbi);
  if (ubDataInstance2.uboFlag && !(columnSensorValue == 85)) {
    // 85°C = типовий артефакт DS18B20 при відключенні → ігноруємо
    // Округлення до 1 знака після крапки через int арифметику (економія RAM)
    columnTemp = (int(10 * ubDataInstance2.uboValue)) / 10.00;
  }

  // ─── Детектори зміни значень (для оновлення LCD та телеметрії) ───────────

  // Зміна columnTemp → оновити рядок 1 LCD
  if (changeNumber6columnTemp) {
    changeNumber6columnTemp = 0;
  } else {
    tempFloat = columnTemp;
    if (tempFloat != changeNumber6Value) {
      changeNumber6Value      = tempFloat;
      changeNumber6columnTemp = 1; // Імпульс: значення змінилось
    }
  }

  // Зміна atmPressure → оновити рядок 1 LCD
  if (changeNumber7atmPressure) {
    changeNumber7atmPressure = 0;
  } else {
    tempFloat = atmPressure;
    if (tempFloat != changeNumber7Value) {
      changeNumber7Value       = tempFloat;
      changeNumber7atmPressure = 1;
    }
  }

  // Зміна cubeTemp → оновити рядок 1 LCD
  if (changeNumber2cubeTemp) {
    changeNumber2cubeTemp = 0;
  } else {
    tempFloat = cubeTemp;
    if (tempFloat != changeNumber2Value) {
      changeNumber2Value    = tempFloat;
      changeNumber2cubeTemp = 1;
    }
  }

  // Якщо будь-яка температура або тиск змінились → оновити рядок 1
  if (changeNumber6columnTemp || changeNumber7atmPressure || changeNumber2cubeTemp) {
    tempFlag5 = 1;
  }

  // Датчик куба (tempSensorAddresses[1])
  // cubeSensorValue → через фільтр UB → cubeTemp
  if (isTimer(cubeSensorReadTime, 5000)) {
    cubeSensorReadTime = millis();
    tempFloat = readDS18TempOW2(cubeSensorAddr, 0);
    if (tempFloat < 500) cubeSensorValue = tempFloat;
  }
  ubDataUbi = cubeSensorValue;
  funcUB_185384122(&ubDataInstance3, ubDataUbi);
  if (ubDataInstance3.uboFlag && !(cubeSensorValue == 85)) {
    // cubeTempCorrection — ручна поправка температури куба (за замовчуванням 0)
    cubeTemp = cubeTempCorrection + ubDataInstance3.uboValue;
  }

  // Датчик аварії/пари (tempSensorAddresses[2])
  // alarmSensorValue → через фільтр UB → alarmTemp
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

  // ─── Конвертація тиску BMP180 → мм рт.ст. ───────────────────────────────
  // bmpPressure зберігається як Па*100 (long) → ділимо на 133.3 для мм рт.ст.
  atmPressure = bmpPressure / 133.3;

  // ═══════════════════════════════════════════════════════════════════════
  //  АВАРІЙНІ ПРАПОРИ
  // ═══════════════════════════════════════════════════════════════════════
  // alarmFlag2 = true якщо:
  //   - датчик протічки спрацював (пін LOW через INPUT_PULLUP)
  //   - АБО температура аварійного датчика перевищила поріг (пара потрапила на датчик)
  alarmFlag2 = ((!digitalRead(RAIN_LEAK_INPUT_PIN)) || (alarmTemp > vaporSensorTriggerTemp));

  // ═══════════════════════════════════════════════════════════════════════
  //  ПЕРЕДАЧА ТЕЛЕМЕТРІЇ (кожні 2 секунди)
  // ═══════════════════════════════════════════════════════════════════════
  // Передається ЗАВЖДИ незалежно від стану ТЕН
  // columnSensorFlag=true → локальний режим → відправляємо дані
  // columnSensorFlag=false → є UART від ESP → не відправляємо (ESP сам опитує)

  // Пакет до Bluetooth (короткий)
  if (columnSensorFlag) {
    if (isTimer(stou1, 2000)) {
      sendDataPacket(BtSerial);
      stou1 = millis();
    }
  } else {
    stou1 = millis(); // Скидаємо таймер поки є UART
  }

  // Пакет до Serial/WiFi (розширений)
  if (columnSensorFlag) {
    if (isTimer(stou2, 2000)) {
      sendDataPacketwifi(Serial);
      stou2 = millis();
    }
  } else {
    stou2 = millis();
  }

  // ═══════════════════════════════════════════════════════════════════════
  //  ПОПРАВКА ТЕМПЕРАТУРИ ВІД ЗМІНИ ТИСКУ
  // ═══════════════════════════════════════════════════════════════════════
  // Температура кипіння залежить від тиску: ~0.035°C на 1 мм рт.ст.
  // При зміні тиску відносно еталонного → коригуємо pwmValue1/pwmValue2
  // pressureValue = (еталон - поточний) * 0.035
  // Якщо тиск впав на 10 мм → pressureValue = +0.35°C (точка кипіння нижча)

  if (pressureCorrectionEnabled == 1 && pressureSensorInitialized) {
    if (changeNumber4Output) {
      changeNumber4Output = 0;
    } else {
      tempFloat = atmPressure;
      if (tempFloat != changeNumber4Value) {
        changeNumber4Value  = tempFloat;
        changeNumber4Output = 1; // Тиск змінився → перерахувати поправку
      }
    }
    if (changeNumber4Output) {
      // pressureSensorValue = тиск при старті (еталон)
      // pressureValue додається до pwmValue1/pwmValue2 при передачі
      pressureValue = (pressureSensorValue - atmPressure) * 0.035;
    }
  }

  // ═══════════════════════════════════════════════════════════════════════
  //  ШІМ ТА КЕРУВАННЯ КЛАПАНОМ
  // ═══════════════════════════════════════════════════════════════════════

  if (!tenEnabled) {
    // ── ТЕН ВИМКНЕНИЙ ──────────────────────────────────────────────────
    // Клапан примусово закритий (HIGH)
    // ШІМ ге��ератор зупинений
    // Всі ШІМ прапори скинуті
    digitalWrite(VALVE_RELAY_DIRECT_PIN, HIGH); // Клапан ЗАКРИТИЙ
    valvePwmState  = false;
    pwmCoarseFlag  = 0;
    pwmFineFlag    = 0;

  } else {
    // ── ТЕН УВІМКНЕНИЙ — стандартна логіка ШІМ ─────────────────────────

    // Визначаємо стан freeLogicQ1:
    // freeLogicQ1StateArr = {0, 1} означає шаблон: [tempInt2<=0]=0, [tempInt2>=1023]=1
    // Якщо tempInt2=0 → Q1=false (клапан закритий при 0)
    // Якщо tempInt2=1023 → Q1=true (клапан відкритий при максимумі)
    freeLogicInputArr[0] = tempInt2 <= 0;
    freeLogicInputArr[1] = tempInt2 >= 1023;
    freeLogicQ1 = checkFreeLogicBlockOutput(freeLogicInputArr, 2, freeLogicQ1StateArr, 2);

    // ── Generator3: ШІМ генератор клапана ─────────────────────────────
    // Активний тільки коли tempInt2 в діапазоні (0..1023) — не крайні значення
    if (!(tempInt2 <= 0 || tempInt2 >= 1023)) {
      if (!generator3Active) {
        generator3Active      = 1;
        generator3Output      = 1; // Починаємо з HIGH (клапан відкритий)
        generator3PrevMillis  = millis();
      }
    } else {
      // Крайнє значення → зупиняємо генератор
      generator3Active = 0;
      generator3Output = 0;
    }

    if (generator3Active) {
      if (generator3Output) {
        // HIGH фаза: тривалість = tempInt2/1023 * pwmPeriodMs
        if (isTimer(generator3PrevMillis, map(tempInt2, 0, 1023, 0, pwmPeriodMs))) {
          generator3PrevMillis = millis();
          generator3Output     = 0; // Переходимо в LOW фазу
        }
      } else {
        // LOW фаза: тривалість = (1 - tempInt2/1023) * pwmPeriodMs
        if (isTimer(generator3PrevMillis, pwmPeriodMs - map(tempInt2, 0, 1023, 0, pwmPeriodMs))) {
          generator3PrevMillis = millis();
          generator3Output     = 1; // Переходимо в HIGH фазу
        }
      }
    }

    // ── Вибір сигналу керування клапаном ──────────────────────────────
    // При крайніх значеннях (0 або 1023) → використовуємо freeLogicQ1 (пряме ON/OFF)
    // При проміжних значеннях → використовуємо generator3Output (ШІМ)
    if ((tempInt2 <= 0) || (tempInt2 >= 1023)) {
      switchFlag4 = freeLogicQ1;
    } else {
      switchFlag4 = generator3Output;
    }
    pwmCoarseFlag = switchFlag4; // Грубий ШІМ сигнал
    // pwmFineFlag=true означає крайнє значення (без ШІМ)
    pwmFineFlag   = ((tempInt2 <= 0) || (tempInt2 >= 1023));

    // ── Зменшення ШІМ при перемиканні tempFlag12 ──────────────────────
    // При зміні стану клапана (START↔STOP) → зменшуємо ШІМ на 5%
    // Це запобігає різким стрибкам при автоматичному регулюванні
    if (bitChange4Output) {
      bitChange4Output = 0;
    } else {
      if (!(bitChange4OldState == tempFlag12)) {
        bitChange4OldState = tempFlag12;
        bitChange4Output   = 1; // Імпульс: стан змінився
      }
    }
    if (bitChange4Output) {
      tempInt2 = tempInt2 * 19 / 20; // Зменшення на ~5%
    }

    // ── Timer1: затримка перед закриттям клапана (triggerFlag3) ───────
    // triggerFlag3=true коли columnTemp досягла зони регулювання
    // Після 5 сек → timer1Output=1 → tempFlag12=0 (STOP)
    if (columnTemp >= pwmValue2 - (pwmValue2 - pwmValue1)) triggerFlag3 = 1;
    if (columnTemp <= pwmValue2) triggerFlag3 = 0;

    if (triggerFlag3) {
      if (timer1Active) {
        if (isTimer(timer1PrevMillis, 5000)) timer1Output = 1; // Через 5 сек → STOP
      } else {
        timer1Active      = 1;
        timer1PrevMillis  = millis();
      }
    } else {
      timer1Output = 0; // Скидаємо якщо умова зникла
      timer1Active = 0;
    }
    // tempFlag12: 1=клапан START (норма), 0=клапан STOP (перегрів колони)
    tempFlag12 = !timer1Output;

    // ── Фінальний сигнал клапана ───────────────────────────────────────
    // pwmFineFlag=true (крайні значення) → switchFlag9 = pwmCoarseFlag (пряме)
    // pwmFineFlag=false (ШІМ режим)      → switchFlag9 = !timer1Output && pwmCoarseFlag
    if (pwmFineFlag) {
      switchFlag9 = pwmCoarseFlag;
    } else {
      switchFlag9 = ((!timer1Output) && pwmCoarseFlag);
    }

    // ── Запис у вихід клапана ──────────────────────────────────────────
    uint8_t pwmValue = map(tempInt2, 0, 1023, 0, 255); // Масштаб для softwarePWM
    if (controlMode == 1) {
      // Режим 1: програмний ШІМ (висока частота ~100 Гц, для мотора/симістора)
      softwarePWM(VALVE_RELAY_DIRECT_PIN, pwmValue, 100, valvePwmLastTime, valvePwmState);
    } else {
      // Режим 0: дискретне реле (LOW=відкритий, HIGH=закритий)
      // Інверсія: switchFlag9=true → LOW (відкритий)
      digitalWrite(VALVE_RELAY_DIRECT_PIN, !(switchFlag9));
      valvePwmState = false;
    }
  }

  // ═══════════════════════════════════════════════════════════════════════
  //  ЗУМЕР ТА ПІДСВІЧУВАННЯ LCD
  // ═══════════════════════════════════════════════════════════════════════
  // Зумер спрацьовує якщо будь-яка з умов:
  //   1. cubeTemp >= alarmTempLimit    (куб перегрітий)
  //   2. alarmTemp > vaporSensorTriggerTemp (пара на датчику)
  //   3. RAIN_LEAK_INPUT_PIN = LOW     (протічка)
  //   4. alarmFlag = 0                 (аварійна зупинка відбулась)
  // Затримка 5 сек перед активацією (щоб уникнути хибних спрацювань)

  if (((cubeTemp >= alarmTempLimit) || (alarmTemp > vaporSensorTriggerTemp))
      || ((!digitalRead(RAIN_LEAK_INPUT_PIN)) || (!alarmFlag))) {
    // Умова зумера виконана → запускаємо timer5
    if (timer5Active) {
      if (isTimer(timer5PrevMillis, 5000)) timer5Output = 1; // Через 5 сек → зумер ON
    } else {
      timer5Active     = 1;
      timer5PrevMillis = millis();
    }
  } else {
    // Умова зникла → скидаємо таймер і зумер
    timer5Output = 0;
    timer5Active = 0;
  }

  // Generator1: меандр зумера (200 мс HIGH / 400 мс LOW)
  if (timer5Output) {
    if (!generator1Active) {
      generator1Active     = 1;
      generator1Output     = 1; // Починаємо з HIGH (звук)
      generator1PrevMillis = millis();
    }
  } else {
    // timer5 вимкнений → зупиняємо генератор
    generator1Active = 0;
    generator1Output = 0;
  }

  if (generator1Active) {
    if (generator1Output) {
      // HIGH фаза: 200 мс (звук)
      if (isTimer(generator1PrevMillis, 200)) {
        generator1PrevMillis = millis();
        generator1Output     = 0;
      }
    } else {
      // LOW фаза: 400 мс (пауза)
      if (isTimer(generator1PrevMillis, 400)) {
        generator1PrevMillis = millis();
        generator1Output     = 1;
      }
    }
  }

  // Запис у бузер (з захистом від зайвих digitalWrite через pzs2OES)
  if (generator1Output) {
    if (!pzs2OES) { digitalWrite(BUZZER_PIN, HIGH); pzs2OES = 1; }
  } else {
    if (pzs2OES)  { digitalWrite(BUZZER_PIN, LOW);  pzs2OES = 0; }
  }

  // Підсвічування LCD: вимикається під час звучання зумера
  // Візуальна індикація аварії (мигання підсвічування синхронно з зумером)
  if (!generator1Output) {
    if (!d1b2) { mainDisplay.backlight();   d1b2 = 1; } // Підсвічування ON
  } else {
    if (d1b2)  { mainDisplay.noBacklight(); d1b2 = 0; } // Підсвічування OFF
  }

  // ═══════════════════════════════════════════════════════════════════════
  //  ЛОГІКА ЗАВЕРШЕННЯ ПЕРЕГОНКИ (finishFlag)
  // ═══════════════════════════════════════════════════════════════════════
  // finishFlag2=true → умова кінця виконана (ШІМ або температура)
  // Timer3: якщо finishFlag2=true і finishFlag=true → через 30 сек finishFlag=0

  if ((!finishFlag2) && finishFlag) {
    // Перегонка іде нормально → тримаємо timer3 активним (output=1)
    timer3Output = 1;
    timer3Active = 1;
  } else {
    if (timer3Active) {
      // Умова кінця щойно з'явилась → запускаємо відлік 30 сек
      timer3Active     = 0;
      timer3PrevMillis = millis();
    } else {
      if (timer3Output) {
        // Чекаємо 30 сек → потім finishFlag=0 (кінець перегонки)
        if (isTimer(timer3PrevMillis, finishDelayMs)) timer3Output = 0;
      }
    }
  }
  // finishFlag слідує за timer3Output (може тільки стати 0, не 1)
  if (finishFlag) finishFlag = timer3Output;

  // Timer7: затримка 10 сек після початку нормальної роботи → tempFlag28
  // (tempFlag28 не використовується активно в поточній версії)
  if ((!finishFlag2) && finishFlag) {
    if (timer7Active) {
      if (isTimer(timer7PrevMillis, 10000)) timer7Output = 1;
    } else {
      timer7Active     = 1;
      timer7PrevMillis = millis();
    }
  } else {
    timer7Output = 0;
    timer7Active = 0;
  }
  tempFlag28 = (timer7Output && (!alarmFlag2));

  // ═══════════════════════════════════════════════════════════════════════
  //  ТАЙМЕР 4 — АВАРІЙНА ЗУПИНКА (alarmFlag)
  // ═══════════════════════════════════════════════════════════════════════
  // Працює НЕЗАЛЕЖНО від tenEnabled (аварія моніториться завжди)
  //
  // alarmFlag2=false (немає аварії) → timer4Output=1 → alarmFlag=1 (норма)
  // alarmFlag2=true  (є аварія)     → через 30 сек timer4Output=0 → alarmFlag=0
  //
  // alarmFlag=0 → аварійна зупинка всього:
  //   tenEnabled=false, tempInt2=0, tempFlag33=0
  //   tempFlag30=0 (вода вимкнена), finishFlag=0

  if ((!alarmFlag2) && alarmFlag) {
    // Норма → тримаємо timer4 активним
    timer4Output = 1;
    timer4Active = 1;
  } else {
    if (timer4Active) {
      // Аварія щойно з'явилась → запускаємо відлік 30 сек
      timer4Active     = 0;
      timer4PrevMillis = millis();
    } else {
      if (timer4Output) {
        // Через 30 сек аварії → alarmFlag=0 (повна зупинка)
        if (isTimer(timer4PrevMillis, alarmStopDelayMs)) timer4Output = 0;
      }
    }
  }
  if (alarmFlag) alarmFlag = timer4Output;

  // ═══════════════════════════════════════════════════════════════════════
  //  РОЗГІН (tempFlag41 → RAZGON_OUTPUT_PIN A1)
  // ═══════════════════════════════════════════════════════════════════════
  // Розгін активний ТІЛЬКИ якщо:
  //   1. tenEnabled=true   (ТЕН увімкнений — є нагрів)
  //   2. alarmFlag2=false  (немає аварії)
  //   3. columnTemp < columnPeripheralSwitchTemp (колона ще холодна = фаза розгону)
  //   4. Пройшло 20 сек у цьому стані (timer2)
  //   5. alarmFlag=1       (загальна норма)
  //
  // Як тільки колона досягає columnPeripheralSwitchTemp (40°C) →
  //   розгін вимикається (перейшли до основної перегонки)

  // СТАНЕ — додаємо умову tenEnabled:
// СТАНЕ — додаємо умову tenEnabled:
if ((!alarmFlag2) && (!(columnTemp >= columnPeripheralSwitchTemp)) && tenEnabled) {
  // Розгін тільки якщо ТЕН увімкнений + немає аварії + колона холодна
  if (timer2Active) {
    if (isTimer(timer2PrevMillis, 20000)) timer2Output = 1;
  } else {
    timer2Active     = 1;
    timer2PrevMillis = millis();
  }
} else {
  // ТЕН вимкнений АБО аварія АБО колона тепла → розгін вимкнений
  timer2Output = 0;
  timer2Active = 0;
}
// tempFlag41=true тільки якщо розгін дозволений І норма І ТЕН увімкнений
tempFlag41 = (timer2Output && alarmFlag);
  // ═══════════════════════════════════════════════════════════════════════
  //  ПЕРИФЕРІЯ / ВОДА (tempFlag30 → PERIPHERY_OUTPUT_PIN A2)
  // ═══════════════════════════════════════════════════════════════════════

  // switchFlag10: дозвіл подачі води
  // Якщо колона >= 40°C → авто режим (вода завжди ON)
  // Якщо колона < 40°C  → ручне керування (tempFlag29: 0=вимк, 1=увімк)
  if (columnTemp >= columnPeripheralSwitchTemp) {
    switchFlag10 = true;  // Авто: колона гаряча → вода потрібна
  } else {
    switchFlag10 = tempFlag29; // Ручне керування водою (команда ^ або меню)
  }

  // Timer8: затримка peripheralOffDelayMs (5 хв) після кінця перегонки
  // Поки finishFlag=1 (перегонка іде) → timer8Output=1 (вода дозволена)
  // Після finishFlag=0 → ще 5 хв тримаємо воду (охолодження після перегонки)
  if (finishFlag) {
    timer8Output = 1;
    timer8Active = 1;
  } else {
    if (timer8Active) {
      timer8Active     = 0;
      timer8PrevMillis = millis();
    } else {
      if (timer8Output) {
        // 5 хв після кінця → вимикаємо воду
        if (isTimer(timer8PrevMillis, peripheralOffDelayMs)) timer8Output = 0;
      }
    }
  }

  // tempFlag30=true = всі умови виконані → вода увімкнена
  // Умови: switchFlag10 (авто або ручна) AND alarmFlag (норма) AND timer8Output (дозвіл)
  tempFlag30 = (switchFlag10 && alarmFlag && timer8Output);

  // ═══════════════════════════════════════════════════════════════════════
  //  ВИХОДИ
  // ═══════════════════════════════════════════════════════════════════════

// A2 — ПЕРИФЕРІЯ (вода)
// tempFlag30=true  → LOW  → реле світиться → вода тече ✅
// tempFlag30=false → HIGH → реле не світиться → вода вимкнена ✅
digitalWrite(PERIPHERY_OUTPUT_PIN, tempFlag30 ? LOW : HIGH);

// A1 — РОЗГІН
// tempFlag41=true + tenEnabled=true → LOW  → реле світиться → розгін активний ✅
// tempFlag41=false або tenEnabled=false → HIGH → реле не світиться → розгін вимкнений ✅
digitalWrite(RAZGON_OUTPUT_PIN, (tempFlag41 && tenEnabled) ? LOW : HIGH);

// A0 — AVARIA/ТЕН
// tenEnabled=true  + alarmFlag=1 → HIGH → реле не світиться → ТЕН працює ✅
// tenEnabled=false або аварія    → LOW  → реле світиться → ТЕН відключений ✅
digitalWrite(AVARIA_OUTPUT_PIN, (tenEnabled && alarmFlag) ? HIGH : LOW);
  
  
 //частина7 
  
  
  
  
    // ═══════════════════════════════════════════════════════════════════════
  //  ДИСПЛЕЙ: РЯДОК 1 (температури)
  // ═══════════════════════════════════════════════════════════════════════
  // Формат: "колона середина куб" — вирівняно по центру 16-символьного LCD
  // Середина = alarmTemp (якщо displayMiddleMode=1) або atmPressure (якщо =0)
  // Оновлюється тільки якщо tempFlag5=1 (щось змінилось)

  if (tempFlag5 == 1) {
    // Визначаємо що показувати в середині рядка
    if (displayMiddleMode) {
      switchValue11 = alarmTemp;   // Температура аварійного датчика (пара)
    } else {
      switchValue11 = atmPressure; // Атмосферний тиск (мм рт.ст.)
    }

    // Формуємо рядок з трьох значень через пробіл
    char t1[7], t2[7], t3[7], dispLineBuf[20];
    dtostrf(columnTemp,   0, 1, t1); // Температура колони (1 знак після крапки)
    dtostrf(switchValue11, 0, 1, t2); // Середнє значення
    dtostrf(cubeTemp,     0, 1, t3); // Температура куба
    snprintf(dispLineBuf, sizeof(dispLineBuf), "%s %s %s", t1, t2, t3);
    dispTempLength = strlen(dispLineBuf);

    // Якщо новий рядок коротший за попередній → очистити LCD
    // (щоб не залишались "хвости" від старого довшого рядка)
    if (dispOldLength3 > dispTempLength) needClearDisplay = 1;
    dispOldLength3 = dispTempLength;

    // Виводимо по центру рядка 0 (верхній рядок LCD)
    // Центрування: відступ = (16 - довжина) / 2
    mainDisplay.setCursor((16 - dispTempLength) / 2, 0);
    if (!encoderMenuActive) {
      // Не виводимо якщо активне меню (меню займає весь дисплей)
      mainDisplay.print(dispLineBuf);
    }
    tempFlag5 = 0; // Скидаємо прапор оновлення
  }

  // ═══════════════════════════════════════════════════════════════════════
  //  ДЕТЕКТОРИ ЗМІНИ ДЛЯ РЯДКА 2 LCD
  // ═══════════════════════════════════════════════════════════════════════
  // Рядок 2 оновлюється тільки якщо змінилось щось з:
  //   - alarmTemp        (температура аварійного датчика)
  //   - tempInt2         (значення ШІМ)
  //   - tempFlag12       (стан клапана START/STOP)
  //   - tempFlag33       (режим авто/ручний)
  // Через timer6 (100 мс затримка) → tempFlag31=1 → оновлення LCD рядка 2

  // Зміна alarmTemp
  if (changeNumber5Output) {
    changeNumber5Output = 0;
  } else {
    tempFloat = alarmTemp;
    if (tempFloat != changeNumber5Value) {
      changeNumber5Value  = tempFloat;
      changeNumber5Output = 1; // Імпульс: значення змінилось
    }
  }

  // Зміна tempInt2 (ШІМ клапана)
  if (changeNumber1Output) {
    changeNumber1Output = 0;
  } else {
    tempInt = tempInt2;
    if (tempInt != changeNumber1Value) {
      changeNumber1Value  = tempInt;
      changeNumber1Output = 1;
    }
  }

  // Зміна tempFlag12 (стан клапана: START/STOP)
  if (bitChange1Output) {
    bitChange1Output = 0;
  } else {
    if (!(bitChange1OldState == tempFlag12)) {
      bitChange1OldState = tempFlag12;
      bitChange1Output   = 1;
    }
  }

  // Зміна tempFlag33 (режим: авто/ручний)
  if (bitChange2Output) {
    bitChange2Output = 0;
  } else {
    if (!(bitChange2OldState == tempFlag33)) {
      bitChange2OldState = tempFlag33;
      bitChange2Output   = 1;
    }
  }

  // Timer6: 100 мс затримка після зміни → щоб не оновлювати LCD на кожен байт
  if ((changeNumber5Output || changeNumber1Output) || (bitChange1Output || bitChange2Output)) {
    // Є зміна → тримаємо timer6 активним
    timer6Output = 1;
    timer6Active = 1;
  } else {
    if (timer6Active) {
      timer6Active     = 0;
      timer6PrevMillis = millis();
    } else {
      if (timer6Output) {
        // 100 мс після останньої зміни → скидаємо
        if (isTimer(timer6PrevMillis, 100)) timer6Output = 0;
      }
    }
  }
  // timer6Output=1 → потрібно оновити рядок 2
  if (timer6Output) tempFlag31 = 1;

  // ═══════════════════════════════════════════════════════════════════════
  //  РЯДОК 2 LCD: ВІДОБРАЖЕННЯ СТАНУ
  // ═══════════════════════════════════════════════════════════════════════
  // Пріоритет відображення (від вищого до нижчого):
  //   1. ТЕН вимкнений              → "TEN OFF"
  //   2. Авто режим (tempFlag33=1)   → "XX.X% START/STOP YY"
  //   3. Ручний режим (tempFlag33=0) → "XX.X% Manual YY" або "realKraft.ua"
  //
  // Примітка: стани END DISTILLATION та !STOP AVAR STOP!
  // перезаписують рядок нижче після цього блоку

  if (!tenEnabled) {
    // ── ТЕН ВИМКНЕНИЙ ────────────────────────────────────────────────────
    // Показуємо "TEN OFF" (тільки якщо ще не показано)
    if (strcmp(tempStr11Buf, "TEN OFF") != 0) {
      tempFlag31 = 1; // Потрібно оновити LCD
      strncpy(tempStr11Buf, "TEN OFF", sizeof(tempStr11Buf) - 1);
      tempStr11Buf[sizeof(tempStr11Buf) - 1] = '\0';
    }

  } else if (tempFlag33 == 1) {
    // ── АВТО РЕЖИМ (ESP керує ШІМ автоматично) ───────────────────────────
    // Формат: "XX.X% START YY" або "XX.X% STOP  YY"
    //   XX.X = відсоток відкриття клапана (tempInt2 / 10.23)
    //   START/STOP = стан клапана (tempFlag12)
    //   YY = температура аварійного датчика (alarmTemp, ціле)
    const char* modeStr = tempFlag12 ? "START" : "STOP ";
    if (alarmFlag) {
      char t1[8], t2[6];
      dtostrf(tempInt2 / 10.23, 0, 1, t1); // % відкриття клапана
      dtostrf(alarmTemp, 0, 0, t2);         // Температура аварійного датчика
      snprintf(tempStr11Buf, sizeof(tempStr11Buf), "%s%% %s %s", t1, modeStr, t2);
    }
    // Умова завершення авто режиму:
    // Якщо ШІМ впав до pwmFinishValue і ще не нульовий (>=16) → кінець
    finishFlag2 = ((tempInt2 <= pwmFinishValue) && (tempInt2 >= 16));

  } else {
    // ── РУЧНИЙ РЕЖИМ (tempFlag33=0, tempInt2 керується вручну) ───────────
    if (tempInt2 >= 2) {
      // Є ненульовий ШІМ → показуємо відсоток і "Manual"
      // Формат: "XX.X% Manual YY"
      if (alarmFlag) {
        char t1[8], t2[6];
        dtostrf(tempInt2 / 10.23, 0, 1, t1);
        dtostrf(alarmTemp, 0, 0, t2);
        snprintf(tempStr11Buf, sizeof(tempStr11Buf), "%s%% Manual %s", t1, t2);
      }
    } else {
      // ШІМ = 0 або 1 → клапан закритий, показуємо логотип
      if (alarmFlag) {
        strncpy(tempStr11Buf, "realKraft.ua", sizeof(tempStr11Buf) - 1);
        tempStr11Buf[sizeof(tempStr11Buf) - 1] = '\0';
      }
    }
    // Умова завершення ручного режиму:
    // Якщо температура куба досягла cubeFinishTemp → кінець
    finishFlag2 = (cubeTemp >= cubeFinishTemp);
  }

  // ═══════════════════════════════════════════════════════════════════════
  //  ПЕРЕГОНКА ЗАВЕРШЕНА (finishFlag=0)
  // ═══════════════════════════════════════════════════════════════════════
  // Перезаписує рядок 2 рядком "END DISTILLATION"
  // Скидає всі параметри роботи (ШІМ, авто режим, ліміт)
  // ТЕН продовжує бути в тому стані що був (не вимикається тут)

  if (finishFlag == 0) {
    if (strcmp(tempStr11Buf, "END DISTILLATION") != 0) {
      tempFlag31 = 1;
      strncpy(tempStr11Buf, "END DISTILLATION", sizeof(tempStr11Buf) - 1);
      tempStr11Buf[sizeof(tempStr11Buf) - 1] = '\0';
    }
    tempFlag33    = 0;      // Вимкнути авто режим
    alarmTempLimit = 110.00;// Скинути ліміт зумера до максимального
    tempInt2      = 0;      // ШІМ = 0 (клапан закриється)
    pwmCoarseFlag = 0;
    pwmFineFlag   = 0;
  }

  // ═══════════════════════════════════════════════════════════════════════
  //  АВАРІЙНА ЗУПИНКА (alarmFlag=0)
  // ═══════════════════════════════════════════════════════════════════════
  // Перезаписує рядок 2 рядком "!STOP AVAR STOP!"
  // Повністю зупиняє систему:
  //   - ТЕН вимкнений (tenEnabled=false) → AVARIA=HIGH
  //   - ШІМ=0 → клапан закритий
  //   - Авто режим вимкнений
  //   - Вода вимкнена (tempFlag30=0)
  //   - Перегонка завершена (finishFlag=0)
  // Зумер продовжує звучати (аварія є)

// СТАНЕ — те саме, але додаємо явне скидання виходів:
if (alarmFlag == 0) {
  if (strcmp(tempStr11Buf, "!STOP AVAR STOP!") != 0) {
    tempFlag31 = 1;
    strncpy(tempStr11Buf, "!STOP AVAR STOP!", sizeof(tempStr11Buf) - 1);
    tempStr11Buf[sizeof(tempStr11Buf) - 1] = '\0';
  }
  tempFlag33    = 0;
  alarmTempLimit = 110.00;
  tempInt2      = 0;
  pwmCoarseFlag = 0;
  pwmFineFlag   = 0;
  tempFlag20    = 0;
  tempFlag30    = 0;
  finishFlag    = 0;
  tenEnabled    = false; // → A0=LOW → реле світиться → ТЕН відключений ✅
                         // → A1=HIGH → реле не світиться → розгін вимкнений ✅
}
  // ═══════════════════════════════════════════════════════════════════════
  //  ДИСПЛЕЙ: РЯДОК 2 (стан системи)
  // ═══════════════════════════════════════════════════════════════════════
  // Оновлюється тільки якщо tempFlag31=1
  // Вирівнювання по центру (як і рядок 1)

  if (tempFlag31 == 1) {
    dispTempLength = strlen(tempStr11Buf);

    // Якщо новий рядок коротший за попередній → очистити LCD
    if (dispOldLength > dispTempLength) needClearDisplay = 1;
    dispOldLength = dispTempLength;

    // Виводимо по центру рядка 1 (нижній рядок LCD)
    mainDisplay.setCursor((16 - dispTempLength) / 2, 1);
    if (!encoderMenuActive) {
      // Не виводимо якщо активне меню
      mainDisplay.print(tempStr11Buf);
    }
    tempFlag31 = 0; // Скидаємо прапор оновлення
  }

  // ─── Обробка меню енкодера (останнє у циклі) ─────────────────────────────
  // Перевіряє події енкодера і оновлює меню якщо воно активне
  handleEncoderMenu();

} // ════════════════════════════ кінець loop() ════════════════════════════════
// ═══════════════════════════════════════════════════════════════════════════
//  УТИЛІТИ
// ═══════════════════════════════════════════════════════════════════════════

// ─── isTimer: перевірка чи пройшов заданий час ───────────────────────────────
// startTime — мітка часу початку (millis() у момент старту)
// period    — необхідний інтервал у мілісекундах
// Повертає true якщо з моменту startTime пройшло >= period мс
// Безпечно при переповненні millis() (через беззнакове віднімання)
bool isTimer(unsigned long startTime, unsigned long period) {
  return (millis() - startTime) >= period;
}

// ─── ISR TIMER2: апаратний watchdog ──────────────────────────────────────────
// Викликається апаратним перериванням таймера 2 при кожному переповненні
// TCNT2=100: встановлюємо лічильник щоб переповнення було частіше (~1 кГц)
// powerDownCount збільшується кожен тік
// Якщо loop() не скидає powerDownCount → через ~1 сек МК перезапускається
// Це захист від зависання програми (програмний watchdog)
ISR(TIMER2_OVF_vect) {
  TCNT2 = 100; // Скидаємо лічильник таймера (прискорюємо переповнення)
  if (powerDownCount >= 1000) {
    // Loop() завис → примусовий перезапуск через перехід на адресу 0x0000
    asm volatile("jmp 0x0000");
  } else {
    powerDownCount++; // Збільшуємо лічильник (loop() скидає його в 0)
  }
}

// ─── convertDS18x2xData: конвертація сирих байт DS18B20 у температуру ────────
// type_s — тип датчика:
//   0 = DS18B20 (12-бітний, найпоширеніший)
//   1 = DS18S20 або DS1820 (9-бітний, старіший)
// data[12] — 9 байт скратчпада прочитаних з датчика
// Повертає температуру у °C як float
float convertDS18x2xData(byte type_s, byte data[12]) {
  // Збираємо 16-бітне сире значення з двох байт (LSB + MSB)
  int16_t raw = (data[1] << 8) | data[0];

  if (type_s) {
    // DS18S20 / DS1820: 9-бітна точність
    // Зсуваємо вліво для отримання 12-бітного формату
    raw = raw << 3;
    if (data[7] == 0x10) {
      // Використовуємо залишок лічильника для підвищення точності
      raw = (raw & 0xFFF0) + 12 - data[6];
    }
  } else {
    // DS18B20: точність залежить від біт конфігурації (data[4], біти 5-6)
    byte cfg = (data[4] & 0x60);
    if (cfg == 0x00)      raw = raw & ~7;  // 9-бітна: маскуємо 3 молодших біти
    else if (cfg == 0x20) raw = raw & ~3;  // 10-бітна: маскуємо 2 молодших біти
    else if (cfg == 0x40) raw = raw & ~1;  // 11-бітна: маскуємо 1 молодший біт
    // 0x60 = 12-бітна: без маскування (повна точність 0.0625°C)
  }

  // Конвертація: одиниця = 1/16 °C → ділимо на 16
  return (float)raw / 16.0;
}

// ─── readDS18TempOW2: читання температури з DS18B20 ──────────────────────────
// addr[8] — 8-байтна адреса датчика (ROM code)
// type_s  — тип датчика (0=DS18B20, 1=DS18S20)
// Повертає температуру °C або 501 якщо помилка CRC
//
// Протокол OneWire для читання температури:
//   1. reset → select(addr) → write(0xBE) → read 9 байт скратчпада
//   2. reset → select(addr) → write(0x44) → запускаємо нове вимірювання
//   3. Перевіряємо CRC (байт 8 = CRC від байт 0..7)
//   4. Якщо CRC ОК → конвертуємо і повертаємо температуру
//
// Увага: значення що повертається — це результат ПОПЕРЕДНЬОГО вимірювання
// (нове вимірювання запускається наприкінці і буде готове через ~750 мс)
float readDS18TempOW2(byte addr[8], byte type_s) {
  byte data[12]; // Буфер для скратчпада (нам потрібно 9 байт)

  // Крок 1: читаємо скратчпад (результат попереднього вимірювання)
  oneWireBus.reset();          // Ініціалізація шини
  oneWireBus.select(addr);     // Вибираємо конкретний датчик за адресою
  oneWireBus.write(0xBE);      // Команда READ SCRATCHPAD
  for (byte i = 0; i < 9; i++) {
    data[i] = oneWireBus.read(); // Читаємо 9 байт скратчпада
  }

  // Крок 2: запускаємо нове вимірювання температури
  oneWireBus.reset();
  oneWireBus.select(addr);
  oneWireBus.write(0x44);      // Команда CONVERT T (запуск вимірювання)
  // Не чекаємо завершення (750 мс) — воно буде готове до наступного виклику

  // Крок 3: перевірка CRC
  // CRC обчислюється від байт 0..7 і порівнюється з байтом 8
  if (oneWireBus.crc8(data, 8) != data[8]) {
    return 501; // Помилка CRC → повертаємо неможливе значення (>500)
                // У loop(): if (tempFloat < 500) → ігноруємо хибне читання
  }

  // Крок 4: конвертація сирих даних у температуру °C
  return convertDS18x2xData(type_s, data);
}

// ─── checkFreeLogicBlockOutput: блок вільної логіки ──────────────────────────
// Перевіряє чи масив вхідних значень inArray[] співпадає з будь-яким
// підмасивом довжиною inArraySize у стаціонарному масиві stArray[]
//
// Використовується для визначення стану клапана при крайніх значеннях ШІМ:
//   inArray = {tempInt2<=0, tempInt2>=1023}
//   stArray = {0, 1} (шаблон: не нуль І максимум)
//
// Якщо inArray={false, true} відповідає шаблону {0,1} → повертає true
// (tempInt2=1023 → клапан повністю відкритий)
//
// Якщо inArray={true, false} → не відповідає → повертає false
// (tempInt2=0 → клапан закритий)
bool checkFreeLogicBlockOutput(bool inArray[], int inArraySize, bool stArray[], int stArraySize) {
  int  inIndex = 0;
  bool result  = 1; // Поточне співпадіння підмасиву

  for (int i = 0; i < stArraySize; i++) {
    // Порівнюємо кожен елемент
    if (!(inArray[inIndex] == stArray[i])) result = 0; // Не співпадає → скидаємо
    inIndex++;

    if (inIndex == inArraySize) {
      // Перевірили повний підмасив
      if (result) return 1; // Знайшли співпадіння → true
      result  = 1;          // Скидаємо для наступного підмасиву
      inIndex = 0;
    }
  }
  return 0; // Жодного співпадіння не знайдено
}

// ─── funcUB_185384122: фільтр значення у діапазоні ───────────────────────────
// Оновлює структуру UB:
//   uboValue = inputValue (завжди зберігаємо останнє значення)
//   uboFlag  = true якщо inputValue потрапляє у відкритий діапазон (gtv1, gtv2)
//
// Використовується для фільтрації показів DS18B20:
//   gtv1Value=1.00   → ігноруємо значення <= 1°C (датчик не підключений або 0)
//   gtv2Value=105.00 → ігноруємо значення >= 105°C (помилкові показники)
//   85°C додатково ігнорується у loop() (типовий артефакт DS18B20)
void funcUB_185384122(struct UB_185384122 *ubInstance, float inputValue) {
  ubInstance->uboValue = inputValue; // Зберігаємо вхідне значення
  // uboFlag=true тільки якщо значення суворо всередині діапазону
  ubInstance->uboFlag = ((ubInstance->gtv1Value < inputValue) &&
                         (inputValue < ubInstance->gtv2Value));
}

// ═══════════════════════════════════════════════════════════════════════════
//  EEPROM — збереження та завантаження адрес датчиків DS18B20
// ═══════════════════════════════════════════════════════════════════════════
//
//  Розкладка EEPROM:
//  Байт 0:      маркер ініціалізації (1 = EEPROM містить дійсні дані)
//  Байти 1..8:  адреса датчика 0 (колона), 8 байт
//  Байти 9..16: адреса датчика 1 (куб),    8 байт
//  Байти 17..24:адреса датчика 2 (аварія),  8 байт
//  Разом: 25 байт з 1024 доступних на ATmega328P

// ─── isEEPROMInitialized: перевірка чи EEPROM містить збережені адреси ───────
// Повертає true якщо байт 0 == 1 (маркер ініціалізації)
bool isEEPROMInitialized() {
  return EEPROM.read(0) == 1;
}

// ─── loadAddresses: завантаження адрес датчиків з EEPROM у RAM ───────────────
// Читає 3 × 8 = 24 байти адрес починаючи з байту 1
void loadAddresses() {
  for (byte i = 0; i < SENSOR_COUNT; i++) {
    for (byte j = 0; j < 8; j++) {
      // Адреса датчика i, байт j = EEPROM[1 + i*8 + j]
      tempSensorAddresses[i][j] = EEPROM.read(1 + i * 8 + j);
    }
  }
}

// ─── saveAddresses: збереження адрес датчиків з RAM у EEPROM ─────────────────
// Записує маркер ініціалізації (байт 0 = 1) та 3 × 8 = 24 байти адрес
// Увага: EEPROM витримує ~100 000 циклів запису
// Ця функція викликається тільки при старті або виявленні нового датчика
void saveAddresses() {
  EEPROM.write(0, 1); // Маркер: EEPROM ініціалізований
  for (byte i = 0; i < SENSOR_COUNT; i++) {
    for (byte j = 0; j < 8; j++) {
      EEPROM.write(1 + i * 8 + j, tempSensorAddresses[i][j]);
    }
  }
}

// ─── scanAndSaveSensors: сканування шини OneWire та збереження датчиків ──────
// Виконується при першому старті (EEPROM порожній)
// Знаходить до SENSOR_COUNT датчиків типу 0x28 (DS18B20)
// Порядок знайдених датчиків залежить від їх адрес (не фіксований)
// Після сканування зберігає у EEPROM
void scanAndSaveSensors() {
  oneWireBus.reset_search(); // Скидаємо стан пошуку
  uint8_t addr[8];
  byte i = 0;

  // Шукаємо датчики поки не знайдемо SENSOR_COUNT або шина закінчиться
  while (oneWireBus.search(addr) && i < SENSOR_COUNT) {
    if (isValidAddress(addr)) {
      // Валідний датчик DS18B20 → зберігаємо адресу
      memcpy(tempSensorAddresses[i], addr, 8);
      i++;
    }
  }
  // Зберігаємо знайдені адреси у EEPROM
  saveAddresses();
}

// ─── isValidAddress: перевірка валідності адреси DS18B20 ─────────────────────
// Повертає true якщо:
//   1. CRC адреси (байти 0..6) співпадає з байтом 7 (захист від помилок шини)
//   2. Перший байт = 0x28 (Family Code для DS18B20)
//      (0x10 = DS18S20, 0x22 = DS1822 — ми підтримуємо тільки 0x28)
bool isValidAddress(uint8_t* addr) {
  return (OneWire::crc8(addr, 7) == addr[7]) && (addr[0] == 0x28);
}

// ─── softwarePWM: програмний ШІМ через digitalWrite ──────────────────────────
// Використовується при controlMode=1 для плавного керування мотором/симістором
// Частота: freq_hz (наприклад 100 Гц)
// Скважність: value/255 (0=0%, 255=100%)
//
// Параметри:
//   pin      — пін виходу
//   value    — скважність 0..255
//   freq_hz  — частота ШІМ у Гц
//   lastTime — посилання на змінну часу останнього перемикання (мкс)
//   state    — посилання на поточний стан виходу (true=HIGH, false=LOW)
//
// Використовує micros() для точності (не millis())
// Не блокує loop() — перевіряє час і перемикає якщо потрібно
void softwarePWM(uint8_t pin, uint8_t value, uint16_t freq_hz,
                 unsigned long &lastTime, bool &state) {
  // Розраховуємо тривалість одного повного циклу (мкс)
  unsigned long period   = 1000000UL / freq_hz;
  // Тривалість HIGH фази (пропорційно value)
  unsigned long highTime = (period * value) / 255;
  // Тривалість LOW фази
  unsigned long lowTime  = period - highTime;

  unsigned long now = micros(); // Поточний час у мікросекундах

  if (state) {
    // Зараз HIGH → чекаємо закінчення highTime
    if (now - lastTime >= highTime) {
      digitalWrite(pin, LOW); // Перемикаємо в LOW
      lastTime = now;
      state    = false;
    }
  } else {
    // Зараз LOW → чекаємо закінчення lowTime
    if (now - lastTime >= lowTime) {
      digitalWrite(pin, HIGH); // Перемикаємо в HIGH
      lastTime = now;
      state    = true;
    }
  }
}