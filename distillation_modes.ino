#include <Arduino.h>

// ============================================================================
//  Distillation Modes — Дистиляція та ББК
//
//  Головний режим вибирається через MQTT:
//    .../cmd/mainMode  payload: "stop" | "distillation" | "bbk"
//
//  Стан дистиляції вибирається через MQTT:
//    .../cmd/distState  payload: "stop" | "heads" | "pre_heads" | "body" |
//                                "pre_tails" | "tails" | "stabilize" | "collect"
//
//  Стан ББК вибирається через MQTT:
//    .../cmd/bzkState   payload: "off" | "warmup" | "stabilize" | "work"
//
//  Єдиний вихід до Arduino — ШІМ насоса:
//    setArduinoCommand("shim", String(pwm))   →   Serial: #pwm!
//
//  Читання температур з Serial-пакету Arduino:
//    "columnTemp"  — температура колони (верх)
//    "cubeTemp"    — температура куба   (низ)
// ============================================================================

// ─── Enum: головний режим ────────────────────────────────────────────────────
enum MainMode {
    MAIN_STOP,         // нічого не активно, насос = 0
    MAIN_DISTILLATION, // дистиляція / ректифікація
    MAIN_BBK           // безперервна бражна колона
};

// ─── Enum: стани дистиляції ───────────────────────────────────────────────────
enum DistState {
    DIST_STOP,       // зупинка
    DIST_HEADS,      // відбір голів
    DIST_PRE_HEADS,  // підголови
    DIST_BODY,       // тіло
    DIST_PRE_TAILS,  // підхвости
    DIST_TAILS,      // хвости
    DIST_STABILIZE,  // стабілізація
    DIST_COLLECT     // відбір
};

// ─── Enum: стани ББК ─────────────────────────────────────────────────────────
enum BZKState {
    BZK_OFF,       // система вимкнена
    BZK_WARMUP,    // прогрів
    BZK_STABILIZE, // стабілізація
    BZK_WORK       // робота (ПІ-регулятор)
};

// ─── Поточні режим та стани ───────────────────────────────────────────────────
MainMode currentMainMode  = MAIN_STOP;
DistState currentDistState = DIST_STOP;
BZKState  currentBZKState  = BZK_OFF;

// ─── Змінні ББК ───────────────────────────────────────────────────────────────
unsigned long bzkTimer     = 0;
float bzkBaseTemp          = 0.0;  // базова temp низу (фіксується в кінці STABILIZE)
float bzkTargetTemp        = 0.0;  // цільова temp = bzkBaseTemp - DELTA
float bzkIntegral          = 0.0;  // інтегральна складова ПІ
float bzkLastTemp          = 0.0;  // запам'ятована temp для перевірки стабільності

// ─── Параметри ББК (налаштовуються через MQTT або вручну) ─────────────────────
float WARMUP_TARGET   = 70.0;   // мінімальна temp верху для переходу зі WARMUP
float DELTA           = 1.0;    // відступ цільової від базової температури
float MIN_PWM         = 100.0;  // мінімальний ШІМ насоса (0–1023)
float MAX_PWM         = 800.0;  // максимальний ШІМ насоса
float KP              = 1.5;    // пропорційний коефіцієнт ПІ
float KI              = 0.05;   // інтегральний коефіцієнт ПІ
float MAX_TOP_TEMP    = 95.0;   // аварійний поріг температури верху
float MIN_BOTTOM_TEMP = 80.0;   // мінімальна робоча temp низу

// ─── Допоміжна: читання поточного значення по імені поля Serial-пакету ────────
float getSerialValue(String key) {
    for (uint8_t i = 0; i < serialKeyCount; i++) {
        if (serialKeys[i] == key) {
            return serialValues[i].toFloat();
        }
    }
    return 0.0;
}

// ─── Публікація поточного стану в MQTT ───────────────────────────────────────
static void publishModeStatus(int pwm) {
    static const char* mainModeNames[]  = { "stop", "distillation", "bbk" };
    static const char* distStateNames[] = { "stop", "heads", "pre_heads", "body",
                                            "pre_tails", "tails", "stabilize", "collect" };
    static const char* bzkStateNames[]  = { "off", "warmup", "stabilize", "work" };

    String payload = "mainMode=";
    payload += mainModeNames[currentMainMode];
    payload += "|distState=";
    payload += distStateNames[currentDistState];
    payload += "|bzkState=";
    payload += bzkStateNames[currentBZKState];
    payload += "|pwm=";
    payload += String(pwm);

    mqttClient.publish((mqttPubTopic + "/mode").c_str(), payload.c_str());
}

// ─── Зміна головного режиму ───────────────────────────────────────────────────
void setMainMode(String value) {
    MainMode prev = currentMainMode;
    value.trim();
    if      (value == "distillation") currentMainMode = MAIN_DISTILLATION;
    else if (value == "bbk")          currentMainMode = MAIN_BBK;
    else                              currentMainMode = MAIN_STOP;

    if (currentMainMode != prev) {
        // При зміні режиму — зупиняємо насос і скидаємо стани
        setArduinoCommand("shim", "0");
        currentDistState = DIST_STOP;
        currentBZKState  = BZK_OFF;
        bzkTimer    = 0;
        bzkIntegral = 0.0;
        Serial1.println("[MODE] Режим змінено: " + value);
    }
}

// ─── Зміна стану дистиляції ───────────────────────────────────────────────────
void setDistState(String value) {
    if (currentMainMode != MAIN_DISTILLATION) {
        Serial1.println("[DIST] Ігнорую: режим дистиляції не активний");
        return;
    }
    value.trim();
    DistState prev = currentDistState;
    if      (value == "stop")       currentDistState = DIST_STOP;
    else if (value == "heads")      currentDistState = DIST_HEADS;
    else if (value == "pre_heads")  currentDistState = DIST_PRE_HEADS;
    else if (value == "body")       currentDistState = DIST_BODY;
    else if (value == "pre_tails")  currentDistState = DIST_PRE_TAILS;
    else if (value == "tails")      currentDistState = DIST_TAILS;
    else if (value == "stabilize")  currentDistState = DIST_STABILIZE;
    else if (value == "collect")    currentDistState = DIST_COLLECT;
    else {
        Serial1.println("[DIST] Невідомий стан: " + value);
        return;
    }
    if (currentDistState != prev) {
        setArduinoCommand("shim", "0");  // зупиняємо насос при зміні стану
        Serial1.println("[DIST] Стан змінено: " + value);
    }
}

// ─── Зміна стану ББК ─────────────────────────────────────────────────────────
void setBZKState(String value) {
    if (currentMainMode != MAIN_BBK) {
        Serial1.println("[ББК] Ігнорую: режим ББК не активний");
        return;
    }
    value.trim();
    BZKState prev = currentBZKState;
    if      (value == "off")       currentBZKState = BZK_OFF;
    else if (value == "warmup")    currentBZKState = BZK_WARMUP;
    else if (value == "stabilize") currentBZKState = BZK_STABILIZE;
    else if (value == "work")      currentBZKState = BZK_WORK;
    else {
        Serial1.println("[ББК] Невідомий стан: " + value);
        return;
    }
    if (currentBZKState != prev) {
        setArduinoCommand("shim", "0");
        bzkTimer    = 0;
        bzkIntegral = 0.0;
        Serial1.println("[ББК] Стан змінено: " + value);
    }
}

// ─── Алгоритм дистиляції — повертає розрахований ШІМ ──────────────────────────
static int computeDistillationPWM() {
    // float tempTop    = getSerialValue("columnTemp");  // температура верху
    // float tempBottom = getSerialValue("cubeTemp");    // температура низу

    switch (currentDistState) {
        case DIST_STOP:
            return 0;

        case DIST_HEADS:
            // TODO: алгоритм відбору голів
            return 0;

        case DIST_PRE_HEADS:
            // TODO: алгоритм підголовів
            return 0;

        case DIST_BODY:
            // TODO: алгоритм відбору тіла
            return 0;

        case DIST_PRE_TAILS:
            // TODO: алгоритм підхвостів
            return 0;

        case DIST_TAILS:
            // TODO: алгоритм відбору хвостів
            return 0;

        case DIST_STABILIZE:
            // TODO: алгоритм стабілізації
            return 0;

        case DIST_COLLECT:
            // TODO: алгоритм відбору
            return 0;

        default:
            return 0;
    }
}

// ─── Алгоритм ББК — повертає розрахований ШІМ ────────────────────────────────
static int computeBZKPWM() {
    float tempTop    = getSerialValue("columnTemp");  // температура верху колони
    float tempBottom = getSerialValue("cubeTemp");    // температура куба / низу
    unsigned long now = millis() / 1000;

    // Аварія: перегрів верху — зупиняємо незалежно від стану
    if (tempTop > MAX_TOP_TEMP) {
        Serial1.println("[ББК] АВАРІЯ: перегрів верху " + String(tempTop, 1));
        currentBZKState = BZK_OFF;
        return 0;
    }

    switch (currentBZKState) {

        case BZK_OFF:
            // TODO: алгоритм стану "система вимкнена"
            return 0;

        case BZK_WARMUP:
            // TODO: алгоритм прогріву
            return 0;

        case BZK_STABILIZE: {
            // Тримаємо мінімальний ШІМ, чекаємо стабілізації температури низу.
            // Коли низ стабільний ≥60 с — фіксуємо базову temp і переходимо в WORK.
            if (tempBottom < MIN_BOTTOM_TEMP) {
                bzkTimer = 0;  // скидаємо таймер, щоб відлік почався заново
                bzkLastTemp = tempBottom;
                return (int)MIN_PWM;
            }
            if (fabs(tempBottom - bzkLastTemp) < 0.2) {
                if (bzkTimer == 0) bzkTimer = now;
                if (now - bzkTimer >= 60) {
                    bzkBaseTemp   = tempBottom;
                    bzkTargetTemp = tempBottom - DELTA;
                    bzkIntegral   = MIN_PWM;
                    currentBZKState = BZK_WORK;
                    bzkTimer = 0;
                    Serial1.println("[ББК] → РОБОТА, ціль=" + String(bzkTargetTemp, 1));
                }
            } else {
                bzkTimer = 0;  // температура нестабільна — скидаємо відлік
            }
            bzkLastTemp = tempBottom;
            return (int)MIN_PWM;
        }

        case BZK_WORK: {
            // Пауза якщо низ охолонув нижче мінімуму
            if (tempBottom < MIN_BOTTOM_TEMP) {
                Serial1.println("[ББК] Пауза: низ " + String(tempBottom, 1));
                return (int)MIN_PWM;
            }
            // ПІ-регулятор
            float error = tempBottom - bzkTargetTemp;
            bzkIntegral += error * KI;
            if (bzkIntegral > MAX_PWM) bzkIntegral = MAX_PWM;
            if (bzkIntegral < MIN_PWM) bzkIntegral = MIN_PWM;

            float pwmCalc = (error * KP) + bzkIntegral;
            if (pwmCalc > MAX_PWM) pwmCalc = MAX_PWM;
            if (pwmCalc < MIN_PWM) pwmCalc = MIN_PWM;

            int pwm = (int)round(pwmCalc);
            Serial1.println("[ББК] PWM=" + String(pwm) +
                            " err=" + String(error, 2) +
                            " ціль=" + String(bzkTargetTemp, 1));
            return pwm;
        }

        default:
            return 0;
    }
}

// ─── Ініціалізація (викликати в setup) ────────────────────────────────────────
void initDistillationModes() {
    currentMainMode  = MAIN_STOP;
    currentDistState = DIST_STOP;
    currentBZKState  = BZK_OFF;
    Serial1.println("[MODES] Ініціалізовано.");
}

// ─── Головне оновлення (викликати в loop кожну секунду) ───────────────────────
void updateDistillationModes() {
    static unsigned long lastUpdate = 0;
    unsigned long now = millis();
    if (now - lastUpdate < 1000) return;
    lastUpdate = now;

    int pwm = 0;
    switch (currentMainMode) {
        case MAIN_STOP:
            pwm = 0;
            break;
        case MAIN_DISTILLATION:
            pwm = computeDistillationPWM();
            break;
        case MAIN_BBK:
            pwm = computeBZKPWM();
            break;
    }

    setArduinoCommand("shim", String(pwm));
    publishModeStatus(pwm);
}