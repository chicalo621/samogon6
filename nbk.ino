// Файл: nbk_mode.cpp
// Одна реалізація режиму НБК (один C++ файл) адаптована з realKraft6.26.
// Усі пояснення і коментарі українською мовою.
//
// Інтеграція:
// - Помістіть цей файл у проєкт chicalo621/samogon6 (наприклад у src/ або в корінь).
// - Впевніться, що в проєкті є реалізації зовнішніх символів (див. секцію "ПОТРІБНІ ЗОВНІШНІ СИМВОЛИ").
// - Викликайте nbk_proc() періодично з головного циклу або з таски, коли активовано режим NBK.
// - Можна додати веб-UI і збереження налаштувань пізніше — зараз реалізовано лише ядро алгоритму.
//
// Автор: портовано і спрощено з realKraft6.26 (ви надали дозвіл на використання).

#include <Arduino.h>

// -------------------- КОНФІГУРАЦІЯ ТА ДЕФОЛТИ --------------------

// Кількість інерцій-пауз після захльобу (множник)
#ifndef NBK_MULT_PAUSE_OVERFLOW
#define NBK_MULT_PAUSE_OVERFLOW 2
#endif

// Дефолтні параметри NBK (можна змінювати при інтеграції)
#ifndef NBK_COLUMN_INERTIA_DEFAULT
#define NBK_COLUMN_INERTIA_DEFAULT 180    // інерція колони в секундах
#endif
#ifndef NBK_OVERFLOW_PRESSURE_DEFAULT
#define NBK_OVERFLOW_PRESSURE_DEFAULT 40  // тиск захльобу (мм рт.ст.)
#endif
#ifndef NBK_TN_DEFAULT
#define NBK_TN_DEFAULT 98.5f
#endif
#ifndef NBK_DT_DEFAULT
#define NBK_DT_DEFAULT 0.5f
#endif
#ifndef NBK_DM_DEFAULT
#define NBK_DM_DEFAULT 100.0f
#endif
#ifndef NBK_DP_DEFAULT
#define NBK_DP_DEFAULT 0.5f
#endif
#ifndef NBK_TP_DEFAULT
#define NBK_TP_DEFAULT 81.0f
#endif
#ifndef NBK_OPERATING_RANGE
#define NBK_OPERATING_RANGE 100.0f
#endif
#ifndef NBK_OPT_ITER_LIMIT
#define NBK_OPT_ITER_LIMIT 300
#endif

// Якщо у Вас нема визначення максимальної температури води, використовуємо дефолт
#ifndef NBK_MAX_WATER_TEMP_DEFAULT
#define NBK_MAX_WATER_TEMP_DEFAULT 70.0f
#endif

// -------------------- ПОТРІБНІ ЗОВНІШНІ СИМВОЛИ --------------------
// Ці змінні/функції має надати ваш проєкт samogon6.
// Якщо їхні імена в проєкті інші — додайте невелику прокладку (wrapper) і змініть тут позначення.

extern bool PowerOn;                 // чи увімкнений нагрів/процес
extern float target_power_volt;      // цільовий сигнал регулятора (вольти або інше представлення)
extern float current_power_volt;     // (опціонально) поточний потік/значення для індикації

// Простий опис структури сенсора: поле avgTemp потрібно використовувати
struct Sensor_t { float avgTemp; float PrevTemp; };
extern Sensor_t SteamSensor;         // датчик пари
extern Sensor_t TankSensor;          // датчик барди/куба
extern Sensor_t WaterSensor;         // датчик води охолодження
extern Sensor_t ACPSensor;           // датчик ТСА (або інший нагрівальний вузол)

// Тиск, якщо вимірюється; якщо немає датчика — встановіть -1 у вашій інтеграції
extern float pressure_value;

// Функції керування насосом/кроковим приводом — проєкт повинен реалізувати
extern float i2c_get_liquid_rate_by_step(int StepperSpeed);
extern float i2c_get_speed_from_rate(float volume_per_hour);
extern uint32_t get_stepper_speed(void);
extern bool set_stepper_target(uint32_t spd, uint8_t direction, uint32_t target);

// Функції керування потужністю/харчуванням — проєкт повинен реалізувати
extern void set_current_power(float Volt); // встановити потужність/напряження на регуляторі
extern void set_power(bool On);            // вмик/викл нагріву

// Функція відправки повідомлень/логів — проєкт може мати власний тип повідомлень
extern void SendMsg(const String& m, int msg_type);

// Якщо у вашому проєкті інші імена/сигнатури — додайте адаптери і підключіть їх тут.

// -------------------- ЛОКАЛЬНІ КОНСТАНТИ ПОВІДОЛЕНЬ --------------------
// Якщо ваш проєкт має свої типи повідомлень — замініть ці значення або використайте свої.
#ifndef NBK_MSG_NOTIFY
#define NBK_MSG_NOTIFY 0
#endif
#ifndef NBK_MSG_WARNING
#define NBK_MSG_WARNING 1
#endif
#ifndef NBK_MSG_ALARM
#define NBK_MSG_ALARM 2
#endif

// -------------------- ВНУТРІШНІ ЗМІННІ НБК --------------------

struct NBKProgramLine {
  String WType;   // "H","S","O","W"
  float Speed;    // л/год
  float Power;    // задана потужність/напряження у програмі
};

static NBKProgramLine program[8]; // буфер програми; збільште при потребі
static uint8_t ProgramLen = 0;
static uint8_t ProgramNum = 0;

// Параметри та стан NBK
static uint16_t nbk_column_inertia = NBK_COLUMN_INERTIA_DEFAULT;
static float nbk_overflow_pressure = NBK_OVERFLOW_PRESSURE_DEFAULT;

static float nbk_M = 0.0f;     // поточна "мощність" (Вт або умовна одиниця)
static float nbk_M_max = 3200.0f; // гранична потужність (за потреби підрахуйте за опором ТЕНа)
static float nbk_Mo = 0.0f;    // оптимальна потужність (Mo)
static float nbk_dM = NBK_DM_DEFAULT; // крок регулювання потужності

static float nbk_P = 0.0f;     // поточна подача (л/год)
static float nbk_Po = 0.0f;    // оптимальна подача (Po)
static float nbk_dP = NBK_DP_DEFAULT; // крок регулювання подачі

static float nbk_Tb = 0.0f;    // температура барди (куба)
static float nbk_Tn = NBK_TN_DEFAULT; // цільова нижня температура барди (Тн)
static float nbk_Tp = 0.0f;    // температура пару
static float nbk_Tvody = 0.0f; // те��пература води

static float nbk_dD = 0.0f;    // поправка до Тн по тиску (якщо застосовується)
static float nbk_dT = NBK_DT_DEFAULT; // дозволена просадка Т барди
static float nbk_Tp_lim = NBK_TP_DEFAULT; // мінімальна Тп для роботи

// Для оптимізації
static uint8_t nbk_opt_iter = 0;
static uint32_t nbk_opt_next_time = 0;
static uint32_t time_speed = 0;
static bool nbk_opt_in_progress = false;

// Для роботи
static uint32_t nbk_work_next_time = 0;
static bool nbk_work_in_pause = false;
static bool workrun = true;
static uint8_t nbk_work_pause_stage = 0;
static float nbk_Mo_temp = 0.0f, nbk_Po_temp = 0.0f;
static bool manual_overflow = false;
static bool noDZ_message_sent = false;

// Статистика
struct NBKStats {
  float avgSpeed;
  float totalVolume;
  uint32_t startTime;
  uint32_t lastVolumeUpdate;
} stats;

// Допоміжні змінні
static uint32_t begintime = 0;
static bool msgfl = false;

// -------------------- ДОПОМОЖНІ ФУНКЦІЇ (адаптуйте за потреби) --------------------

// Перетворення між "цікавими" одиницями потужності.
// У проєкті realKraft використовували toPower/fromPower з урахуванням опору ТЕНа.
// Тут залишаємо прості функції-заглушки — при інтеграції підставте коректні формули
// або викличте наявні в проєкті перетворення.
static float toPower(float value) {
  // value може бути або в В, або у Вт залежно від того, як задаються програми.
  // За замовчуванням повертаємо value (не конвертуємо).
  return value;
}
static float fromPower(float value) {
  // зворотне перетворення (для встановлення регулятора)
  return value;
}

// Встановити швидкість насоса/крокового двигуна у л/год
static void SetSpeed(float Speed) {
  // Якщо Speed < 1 — зупиняємо
  if (Speed < 1.0f) {
    if (set_stepper_target) set_stepper_target(0, 0, 0);
    return;
  }
  // Конвертація у швидкість крокового двигуна та застосування
  uint32_t stp = (uint32_t)i2c_get_speed_from_rate(Speed);
  set_stepper_target(stp, 0, 2147483640UL);
}

// Перевірка захльобу колони (overflow)
// Використовуємо тиск (pressure_value) — якщо немає датчика, set pressure_value = -1
static bool overflow() {
  if (!PowerOn) return false;
  if (pressure_value != -1.0f) {
    if (pressure_value >= nbk_overflow_pressure) {
      SendMsg("Захльоб по тиску", NBK_MSG_WARNING);
      return true;
    }
  }
  // Якщо немає тиску і у вашому проєкті є датчик рівня — додайте тут перевірку.
  return false;
}

// -------------------- ОСНОВНІ ФУНКЦІЇ СТАНІВ --------------------

// Обробка паузи/захльобу: коригує стан/завершує процес або ставить у паузу
static void handle_overflow(const String& msg, bool finish = true, uint32_t pause_ms = 0) {
  // Зменшуємо потужність і подачу тимчасово
  nbk_M = nbk_M / 2.0f;
  nbk_P = nbk_P / 3.0f;
  SetSpeed(nbk_P);
  SendMsg(msg, NBK_MSG_ALARM);
  if (finish) {
    // Повне завершення: вимикаємо подачу та нагрів
    SetSpeed(0.0f);
    set_current_power(0.0f);
    // Перекладаємо програму у стан завершення:
    ProgramNum = 255; // маркер завершення (інтегратор може обробити по-своєму)
  } else if (pause_ms > 0) {
    // Часткове відновлення: ставимо в паузу та плануємо відновлення
    set_current_power(fromPower(nbk_Mo / 2.0f));
    nbk_work_in_pause = true;
    nbk_work_pause_stage = 1;
    nbk_work_next_time = millis() + pause_ms;
  }
}

// Перевірка критичних аварій: перегрів, нестача охолодження, кінець браги
static bool check_nbk_critical_alarms() {
  // Якщо в проєкті існує глобальна аварійна подія — додайте перевірку тут.

  // Якщо не у ручній настройці — якщо пара > 98 => "закінчилась брага"
  if (program[ProgramNum].WType != "S") {
    if (SteamSensor.avgTemp > 98.0f) {
      SendMsg("Закінчилась брага! Зупинка.", NBK_MSG_ALARM);
      nbk_M = 0.0f;
      nbk_P = 0.0f;
      SetSpeed(0.0f);
      ProgramNum = 255; // маркер завершення
      return true;
    }
  }

  // Перевірка недостатнього охолодження: Т ТСА або Т води вище ліміту стабільно
  static uint32_t overheat_start_time = 0;
  float max_water_temp = NBK_MAX_WATER_TEMP_DEFAULT;
  // Якщо у вашому проєкті є глобальна константа — використайте її (наприклад MAX_WATER_TEMP)
  // else використаємо дефолт.

  if (ACPSensor.avgTemp > 60.0f || WaterSensor.avgTemp > max_water_temp) {
    if (overheat_start_time == 0) overheat_start_time = millis();
    if ((millis() - overheat_start_time) > 60000UL) { // 60 с
      SendMsg("Недостатнє охолодження! Зупинка.", NBK_MSG_ALARM);
      nbk_M = 0.0f;
      nbk_P = 0.0f;
      set_power(false);
      SetSpeed(0.0f);
      ProgramNum = 255;
      return true;
    }
  } else {
    overheat_start_time = 0;
  }

  return false;
}

// Завершення роботи NBK: зупинка, підрахунок статистики, повідомлення
static void nbk_finish() {
  SendMsg("Робота НБК завершена", NBK_MSG_NOTIFY);
  SetSpeed(0.0f);
  uint32_t totalTime = 0;
  if (stats.startTime > 0) totalTime = (millis() - stats.startTime) / 1000UL; // сек
  if (totalTime > 0) {
    stats.avgSpeed = (stats.totalVolume * 3600.0f) / (float)totalTime;
  } else {
    stats.avgSpeed = 0.0f;
  }
  String summary = "";
  summary += "Пропущено браги " + String(stats.totalVolume, 2) + " л, ";
  summary += "середня швидкість " + String(stats.avgSpeed, 2) + " л/год, ";
  summary += "тривалість " + String(totalTime / 3600.0f, 2) + " год.";
  SendMsg(summary, NBK_MSG_NOTIFY);
  delay(1000);
  set_power(false);
  // Закривайте будь-які файли або ресурси тут при потребі.
}

// -------------------- РЕАЛІЗАЦІЯ ЕТАПІВ --------------------

// 1) Етап "H" — прогрів парогенератора до мінімальної температури пари (~75°C)
static void handle_nbk_stage_heatup() {
  nbk_Tp = SteamSensor.avgTemp;
  if (ProgramNum == 0) {
    // Початок сесії: ініціалізація статистики
    time_speed = 0;
    stats.startTime = millis();
    stats.avgSpeed = 0.0f;
    stats.totalVolume = 0.0f;
    SendMsg("Запуск програми НБК. Прогрів", NBK_MSG_NOTIFY);
    // Якщо потрібно — створюйте файл логів або ініціалізуйте запис
  }

  // Розгін до Тп >= 75°C
  if (nbk_Tp >= 75.0f) {
    // Перехід на наступний рядок програми
    ProgramNum++;
    if (ProgramNum >= ProgramLen) ProgramNum = 255;
    return;
  }

  // Якщо виявлено захльоб — зупинка або пауза, залеж��о від налаштувань
  if (overflow()) {
    handle_overflow("На прогріві задані занадто великі потужність або подача. Зупинка програми.", true, 0);
  }

  delay(200);
}

// 2) Етап "S" — ручне налаштування (коригування Ін, Тн, Мо, По оператором)
static void handle_nbk_stage_manual() {
  // Якщо сталося захльоблення під час ручної настройки — автоматично зменшуємо параметри
  if (overflow() && !manual_overflow) {
    manual_overflow = true;
    nbk_P = nbk_P / 3.0f;
    nbk_M = nbk_M / 2.0f;
    set_current_power(fromPower(nbk_M));
    SetSpeed(nbk_P);
    SendMsg("Подача 1/3, потужність 1/2 (реакція на захльоб).", NBK_MSG_WARNING);
    delay(200);
    return;
  } else if (get_stepper_speed() > 0) {
    manual_overflow = false;
  }
  delay(200);
}

// 3) Етап "O" — автоматична оптимізація (пошук Mo і Po)
static void handle_nbk_stage_optimization() {
  if (!nbk_opt_in_progress) {
    // Невелика пауза (30 с) — щоб користувач міг пропустити оптимізацію
    if ((begintime + 30000UL) > millis()) {
      delay(200);
      return;
    }

    // Якщо відсутній датчик захльобу (в проєкті) — ніщо не гарантує коректну оптимізацію.
    // Тут ми повідомляємо і після таймауту переходимо в режим Робота використовуючи ручні настройки.
    if (pressure_value == -1.0f) {
      if (!noDZ_message_sent) {
        SendMsg("Оптимізація неможлива - відсутній датчик захльобу. Встановіть параметри вручну.", NBK_MSG_WARNING);
      }
      noDZ_message_sent = true;
      if ((begintime + 600000UL) > millis()) { // даємо час оператору
        delay(200);
        return;
      }
      // Пропускаємо оптимізацію
      ProgramNum++;
      return;
    }

    // Ініціалізація оптимізації
    nbk_opt_in_progress = true;
    begintime = 0;
    nbk_Mo_temp = 0.0f;
    nbk_Po_temp = 0.0f;
    nbk_Mo = 0.0f;
    nbk_Po = 0.0f;

    // Ініціалізація поточних M і P (з поточного стану або з рядка програми)
    nbk_M = toPower(target_power_volt) > 100.0f ? toPower(target_power_volt) : 0.3f * nbk_M_max;
    nbk_P = get_stepper_speed() > 0 ? i2c_get_liquid_rate_by_step(get_stepper_speed()) : 10.0f;
    if (program[ProgramNum].Power > 0.0f) nbk_M = toPower(program[ProgramNum].Power);
    if (program[ProgramNum].Speed > 0.0f) nbk_P = program[ProgramNum].Speed;

    set_current_power(fromPower(nbk_M));
    SetSpeed(nbk_P);

    nbk_opt_next_time = millis() + (uint32_t)(nbk_column_inertia * (NBK_MULT_PAUSE_OVERFLOW / 3.0f) * 1000.0f);
    SendMsg("Оптимізація розпочата: " + String(fromPower(nbk_M), 0) + ", " + String(nbk_P, 1) + " л/год", NBK_MSG_NOTIFY);
  }

  // Цикл оптимізації
  if (nbk_opt_in_progress) {
    // Якщо стався захльоб після деяких ітерацій
    if (overflow() && !workrun) {
      if (nbk_Mo == 0.0f && nbk_Po == 0.0f) {
        // Захльоб на перших ітераціях — оптимізація неможлива
        handle_overflow("Задані параметри занадто великі — оптимізація неможлива. Зупинка.", true, 0);
        return;
      } else {
        // Якщо оптимум знайдений раніше — закінчуємо оптимізацію та переходимо в Роботу після паузи
        nbk_Po *= NBK_OPERATING_RANGE / 100.0f;
        nbk_Mo *= NBK_OPERATING_RANGE / 100.0f;
        SendMsg("Оптимум: " + String(fromPower(nbk_Mo), 0) + ", " + String(nbk_Po, 1) + " л/год", NBK_MSG_WARNING);
        // Перехід на наступний рядок (робота) і пауза MULT*Ін
        ProgramNum++;
        handle_overflow("Оптимізація завершена.", false, (uint32_t)(NBK_MULT_PAUSE_OVERFLOW * nbk_column_inertia * 1000UL));
        return;
      }
    }

    // Якщо настав час перевірки (вийшла інерція)
    if ((int32_t)(millis() - nbk_opt_next_time) >= 0) {
      nbk_Tb = TankSensor.avgTemp;
      // Розрахунок поправки по тиску (як у оригіналі) — при бажанні підставте власну формулу
      if (pressure_value != -1.0f) {
        nbk_dD = 0.00001913f * pressure_value * pressure_value + 0.03694f * pressure_value;
      } else {
        nbk_dD = 0.0f;
      }

      nbk_M = toPower(target_power_volt);
      nbk_P = i2c_get_liquid_rate_by_step(get_stepper_speed());
      nbk_Tp = SteamSensor.avgTemp;

      // Ядро оптимізації:
      if ((nbk_Tb >= nbk_Tn + nbk_dD) && (nbk_Tp >= nbk_Tp_lim)) {
        // Якщо барда тримає температуру — запам'ятовуємо Po/Mo та пробуємо збільшити подачу
        nbk_Po = nbk_P;
        nbk_Mo = nbk_M;
        nbk_P += nbk_dP;
        if (nbk_P > 9999.0f) {
          SendMsg("Досягнута гранична подача.", NBK_MSG_WARNING);
          ProgramNum++;
          return;
        }
        SendMsg("Оптимізація: Тб >= Тн, збільшуємо подачу. Ітерація " + String(nbk_opt_iter + 1), NBK_MSG_NOTIFY);
      } else {
        // Якщо барда охолоджується — зменшуємо подачу і підвищуємо потужність
        if ((nbk_M + nbk_dM) > nbk_M_max) {
          SendMsg("Досягнута гранична потужність. Результат: " + String(nbk_Po, 1) + " л/год", NBK_MSG_WARNING);
          ProgramNum++;
          return;
        }
        nbk_P *= 0.9f;
        nbk_M += nbk_dM;
        if (nbk_Tp < nbk_Tp_lim) {
          SendMsg("Оптимізація: Тп < мінімум, збільшуємо потужність. Ітерація " + String(nbk_opt_iter + 1), NBK_MSG_NOTIFY);
        } else {
          SendMsg("Оптимізація: Тб < Тн, збільшуємо потужність. Ітерація " + String(nbk_opt_iter + 1), NBK_MSG_NOTIFY);
        }
      }

      set_current_power(fromPower(nbk_M));
      SetSpeed(nbk_P);
      nbk_opt_iter++;
      if (nbk_opt_iter >= NBK_OPT_ITER_LIMIT) {
        SendMsg("Досягнуто ліміт ітерацій оптимізації. Результат: " + String(fromPower(nbk_Mo), 0) + ", " + String(nbk_Po, 1) + " л/год", NBK_MSG_WARNING);
        ProgramNum++;
        return;
      }
      nbk_opt_next_time = millis() + (uint32_t)(nbk_column_inertia * 1000UL);
    }
  }

  delay(200);
}

// 4) Етап "W" — основний режим роботи: підтримка подачі і потужності
static void handle_nbk_stage_work() {
  // Якщо не в паузі по захльобу
  if (!nbk_work_in_pause) {
    if (overflow()) {
      // Невелике зниження подачі і потужності і пауза MULT*Ін
      handle_overflow("Тимчасове зниження подачі і нагріву через захльоб.", false, (uint32_t)(NBK_MULT_PAUSE_OVERFLOW * nbk_column_inertia * 1000UL));
      return;
    }

    if ((int32_t)(millis() - nbk_work_next_time) >= 0) {
      nbk_Tp = SteamSensor.avgTemp;
      nbk_Tb = TankSensor.avgTemp;
      if (pressure_value != -1.0f) {
        nbk_dD = 0.00001913f * pressure_value * pressure_value + 0.03694f * pressure_value;
      } else {
        nbk_dD = 0.0f;
      }
      nbk_M = toPower(target_power_volt);
      nbk_P = i2c_get_liquid_rate_by_step(get_stepper_speed());

      // Правило: якщо Тб < Тн - dT + dD або Тп < порог => знижуємо подачу
      if ((nbk_Tb < nbk_Tn - nbk_dT + nbk_dD) || (nbk_Tp < nbk_Tp_lim)) {
        if ((nbk_P > nbk_Po - 0.1f) && (nbk_P < nbk_Po + 0.1f) && (nbk_M > nbk_Mo - 5.0f) && (nbk_M < nbk_Mo + 5.0f)) {
          nbk_Po -= nbk_dP / 10.0f;
        }
        nbk_P = nbk_Po;
        nbk_M = nbk_Mo;
        set_current_power(fromPower(nbk_M));
        if (nbk_P < 0.0f) nbk_P = 0.0f;
        SetSpeed(nbk_P);
      }

      if (nbk_Tb < nbk_Tn - nbk_dT + nbk_dD) {
        SendMsg("Робота: Тб < Тн-dT, знижуємо подачу на " + String(nbk_dP / 10.0f, 1) + " до " + String(nbk_P, 1) + " л/год", NBK_MSG_NOTIFY);
      } else if (nbk_Tp < nbk_Tp_lim) {
        SendMsg("Робота: Тп нижча межі, знижуємо подачу на " + String(nbk_dP / 10.0f, 1) + " до " + String(nbk_P, 1) + " л/год", NBK_MSG_NOTIFY);
      }

      nbk_work_next_time = millis() + (uint32_t)(nbk_column_inertia * 1000UL);
    }
  }

  // Обробка поетапного відновлення після захльобу
  if (nbk_work_in_pause && (int32_t)(millis() - nbk_work_next_time) >= 0) {
    if (nbk_work_pause_stage == 1) {
      // Після першої паузи: зменшуємо оптимальні Mo/Po на 1/10 кроку (якщо не було ручних втручань)
      if (workrun) {
        if ((nbk_P > nbk_Po - 0.1f) && (nbk_P < nbk_Po + 0.1f) && (nbk_M == nbk_Mo)) {
          nbk_Mo -= nbk_dM / 10.0f;
          nbk_Po -= nbk_dP / 10.0f;
        }
      }
      if (!workrun) workrun = true;
      if (nbk_Mo < 0.0f) nbk_Mo = 0.0f;
      if (nbk_Po < 0.0f) nbk_Po = 0.0f;
      nbk_M = nbk_Mo;
      nbk_P = nbk_Po;
      set_current_power(fromPower(nbk_M));
      SetSpeed(nbk_P);

      SendMsg("Робота: відновлення після захльобу, скоректовані параметри: " + String(fromPower(nbk_Mo), 0) + ", " + String(nbk_Po, 1) + " л/год", NBK_MSG_NOTIFY);

      nbk_work_pause_stage = 2;
      nbk_work_next_time = millis() + (uint32_t)(2UL * (NBK_MULT_PAUSE_OVERFLOW / 3.0f) * nbk_column_inertia * 1000UL);
    } else if (nbk_work_pause_stage == 2) {
      // Після другої паузи — продовжуємо роботу
      nbk_work_in_pause = false;
      nbk_work_pause_stage = 0;
      nbk_work_next_time = millis() + (uint32_t)(nbk_column_inertia * 1000UL);
      SendMsg("Робота: продовжуємо цикл після паузи.", NBK_MSG_NOTIFY);
    }
  }
  delay(200);
}

// -------------------- УПРАВЛІННЯ ПРОГРАМОЮ NBK --------------------

// Перехід на рядок програми (ініціалізація при вході в рядок)
static void run_nbk_program(uint8_t num) {
  // Якщо номер виходить за межі — завершуємо
  if (num >= ProgramLen) {
    nbk_finish();
    return;
  }
  ProgramNum = num;
  // Очищаємо лічильники, відкриття/запуск
  begintime = 0;
  msgfl = true;

  // Повідомлення про перехід
  SendMsg("Перехід до рядка №" + String((int)num + 1) + ". Тип: " + program[num].WType, NBK_MSG_NOTIFY);

  // При переході на розгон H
  if (program[ProgramNum].WType == "H") {
    workrun = false;
    begintime = 0;
    set_power(true);
    delay(2500);
    if (program[ProgramNum].Power > 0.0f) {
      nbk_M = toPower(program[ProgramNum].Power);
      set_current_power(fromPower(nbk_M));
    } else {
      nbk_M = nbk_M_max;
    }
    nbk_P = (program[ProgramNum].Speed > 0.0f) ? program[ProgramNum].Speed : 1.0f;
    SetSpeed(nbk_P);
  }

  // При переході на ручну настройку S
  if (program[ProgramNum].WType == "S") {
    begintime = 0;
    nbk_M = (program[ProgramNum].Power > 0.0f) ? toPower(program[ProgramNum].Power) : 500.0f;
    nbk_P = (program[ProgramNum].Speed > 0.0f) ? program[ProgramNum].Speed : 1.0f;
    set_current_power(fromPower(nbk_M));
    SetSpeed(nbk_P);
  }

  // При переході на оптимізацію O
  if (program[ProgramNum].WType == "O") {
    nbk_opt_iter = 0;
    nbk_opt_in_progress = false;
    begintime = millis();
    nbk_Mo_temp = toPower(target_power_volt);
    nbk_Po_temp = i2c_get_liquid_rate_by_step(get_stepper_speed());
    noDZ_message_sent = false;
  }

  // При переході на роботу W
  if (program[ProgramNum].WType == "W") {
    if (nbk_Mo_temp > 0.0f && nbk_Po_temp > 0.0f) {
      nbk_Mo = nbk_Mo_temp;
      nbk_Po = nbk_Po_temp;
      SendMsg("Оптимізація пропущена, використовуємо ручні налаштування.", NBK_MSG_WARNING);
    }
    nbk_M = (program[ProgramNum].Power > 0.0f) ? toPower(program[ProgramNum].Power) : (nbk_Mo > 0.0f ? nbk_Mo : 500.0f);
    nbk_P = (program[ProgramNum].Speed > 0.0f) ? program[ProgramNum].Speed : (nbk_Po > 0.0f ? nbk_Po : 1.0f);
    nbk_Mo = nbk_M;
    nbk_Po = nbk_P;
    set_current_power(fromPower(nbk_M));
    SetSpeed(nbk_P);
    SendMsg("Робота: M= " + String(nbk_M, 0) + ", P=" + String(nbk_P, 1) + " л/год", NBK_MSG_NOTIFY);
    nbk_work_in_pause = false;
  }
}

// Парсинг текстової програми формату: "H;1;0\nS;10;167\nO;0;0\nW;0;0\n"
static void set_nbk_program(const String& WProgram) {
  if (WProgram.length() == 0) return;
  // Копіюємо у тимчасовий чар-буфер
  char c[1024];
  WProgram.toCharArray(c, sizeof(c));
  char* pair = strtok(c, ";");
  int i = 0;
  while (pair != NULL && i < (int)(sizeof(program)/sizeof(program[0]))) {
    program[i].WType = String(pair);
    pair = strtok(NULL, ";");
    if (!pair) break;
    program[i].Speed = atof(pair);
    pair = strtok(NULL, "\n");
    if (!pair) break;
    program[i].Power = atof(pair);
    i++;
    pair = strtok(NULL, ";");
  }
  ProgramLen = i;
  if (ProgramLen == 0) {
    // Якщо програм немає — створюємо дефолтну: H,S,O,W
    program[0].WType = "H"; program[0].Speed = 1; program[0].Power = 0;
    program[1].WType = "S"; program[1].Speed = 10; program[1].Power = 167;
    program[2].WType = "O"; program[2].Speed = 0; program[2].Power = 0;
    program[3].WType = "W"; program[3].Speed = 0; program[3].Power = 0;
    ProgramLen = 4;
  }
}

// Повернути програму у текстовому форматі
static String get_nbk_program() {
  String Str = "";
  for (uint8_t i = 0; i < ProgramLen; i++) {
    Str += program[i].WType + ";" + String(program[i].Speed) + ";" + String((int)program[i].Power) + "\n";
  }
  return Str;
}

// -------------------- ГОЛОВНЕ: nbk_proc --------------------

// Головний цикл НБК — викликайте періодично (наприклад у loop())
void nbk_proc() {
  // Оновлюємо параметри з налаштувань проєкту при потребі (в інтеграції)
  nbk_column_inertia = NBK_COLUMN_INERTIA_DEFAULT; // замініть на читання з налаштувань SamSetup.NbkIn якщо є
  nbk_dT = NBK_DT_DEFAULT;                         // аналогічно
  nbk_Tn = NBK_TN_DEFAULT;
  nbk_overflow_pressure = NBK_OVERFLOW_PRESSURE_DEFAULT;
  nbk_dM = NBK_DM_DEFAULT;
  nbk_dP = NBK_DP_DEFAULT;
  nbk_Tp_lim = NBK_TP_DEFAULT;
  // nbk_M_max можна обчислити на підставі опору ТЕНа, якщо у вас є SamSetup.HeaterResistant

  // Якщо програма не встановлена — встановити дефолт
  if (ProgramLen == 0) {
    set_nbk_program("H;1;0\nS;10;167\nO;0;0\nW;0;0\n");
  }

  // Вибір типу рядка та виклик відповідної обробки
  if (ProgramNum >= ProgramLen) {
    // нічог�� не робимо
    delay(10);
    return;
  }

  String wtype = program[ProgramNum].WType;
  if (wtype == "H") {
    handle_nbk_stage_heatup();
    return;
  } else if (wtype == "S") {
    handle_nbk_stage_manual();
    return;
  } else if (wtype == "O") {
    handle_nbk_stage_optimization();
    return;
  } else if (wtype == "W") {
    handle_nbk_stage_work();
    return;
  }

  delay(10);
}

// -------------------- ДОПОМІЖНІ ФУНКЦІЇ ДЛЯ ІНТЕГРАЦІЇ --------------------

// Отримати рядок програми (зовнішнє API)
String nbk_get_program() {
  return get_nbk_program();
}

// Встановити рядок програми (зовнішнє API)
void nbk_set_program(const String& prog) {
  set_nbk_program(prog);
  // При бажанні, після встановлення програми починаємо з першого рядка
  ProgramNum = 0;
}

// Запустити NBK (зовнішній виклик) — вмикає харчування і запускає програму з початку
void nbk_start() {
  ProgramNum = 0;
  stats.startTime = millis();
  set_power(true);
  run_nbk_program(0);
}

// Зупинити NBK (зовнішній виклик) — безпечна зупинка
void nbk_stop() {
  nbk_finish();
}

// -------------------- КІНЕЦЬ ФАЙЛУ --------------------
// Після додавання цього файлу у проєкт:
// - Перевірте зв'язування extern-символів з реальними функціями/змінними проєкту.
// - За потреби додайте адаптери (wrapper-и), які підлаштують імена/сигнатури до існуючих у samogon6.
// - Пізніше можна додати UI (data/nbk.htm) і збереження параметрів у EEPROM/NVS аналогічно модулю SamSetup.