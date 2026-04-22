#include <Arduino.h>  // Додано для гарантії включення стандартних визначень

// ============================================================================
//  Distillation Modes: Rectification and BZK Control
//  Мігрований код з Virtuino скриптів для автономної роботи ESP8266
// ============================================================================

// Прототипи функцій (щоб уникнути помилок компіляції, якщо викликаються раніше)
void selectRectificationMode(String newMode);
void updateRectificationState();
void updateBZKControl();
void setBZKEnabled(bool enabled);
void initDistillationModes();
void updateDistillationModes();

// ─── Глобальні змінні для режимів ──────────────────────────────────────────
String currentMode = "стоп";  // Поточний режим ректифікації
float v56_speed = 0.0;        // Швидкість режиму (v56)
float v58_target = 0.0;       // Цільовий об'єм (v58)
unsigned long v54_timeEst = 0; // Прогнозований час (сек)
unsigned long v63_endTime = 0; // Час завершення (timestamp)
unsigned long v55_elapsed = 0;  // Час, що минув
float v57_volume = 0.0;       // Накопичений об'єм
unsigned long v60_startTime = 0; // Час старту
unsigned long v61_lastUpdate = 0; // Останнє оновлення
float v66_lastSpeed = 0.0;    // Остання швидкість
bool v80_completed = 0;       // Флаг завершення (замінено false на 0)

// Швидкості для режимів (v40-v44)
float modeSpeeds[5] = {0.0, 0.0, 0.0, 0.0, 0.0};  // 0=голови, 1=підголови, 2=тіло, 3=підхвости, 4=хвости
// Цілі для режимів (v22-v26)
float modeTargets[5] = {0.0, 0.0, 0.0, 0.0, 0.0};

// ─── Змінні для ББК ────────────────────────────────────────────────────
enum BZKStage { BZK_OFF, WARMUP, STABILIZE, WORK };  // OFF перейменовано на BZK_OFF для уникнення конфлікту з #define OFF
BZKStage bzkStage = BZK_OFF;  // OFF перейменовано на BZK_OFF
unsigned long bzkTimer = 0;     // Таймер для етапів
float bzkV91_baseTemp = 0.0;    // Базова температура барди
float bzkV97_targetTemp = 0.0;  // Цільова температура
float bzkIntegral = 0.0;        // Інтегральна складова ПІД
float bzkLastError = 0.0;       // Попередня помилка
bool bzkEnabled = 0;            // Вмикання системи (замінено false на 0)

// Параметри ББК (можна налаштувати)
float WARMUP_TARGET = 70.0;
float DELTA = 1.0;
float MIN_PWM = 100.0;
float MAX_PWM = 800.0;
float KP = 1.5;
float KI = 0.05;
float MAX_TOP_TEMP = 95.0;
float MIN_BOTTOM_TEMP = 80.0;

// ─── Допоміжна функція для отримання значень з Serial ──────────────────────
float getSerialValue(String key) {
    for (uint8_t i = 0; i < serialKeyCount; i++) {
        if (serialKeys[i] == key) {
            return serialValues[i].toFloat();
        }
    }
    return 0.0;
}

// ─── Функція вибору режиму ректифікації (міграція скрипта 1) ───────────────
void selectRectificationMode(String newMode) {
    if (newMode == currentMode) return;
    
    unsigned long now = millis() / 1000;
    
    if (newMode == "стоп") {
        v56_speed = 0.0;
        v58_target = 0.0;
        v54_timeEst = 0;
        v63_endTime = 0;
        v55_elapsed = 0;
        v57_volume = 0.0;
        v61_lastUpdate = 0;
        v66_lastSpeed = 0.0;
        setArduinoCommand("v18", "0");
    } else {
        int idx = -1;
        if (newMode == "голови") idx = 0;
        else if (newMode == "підголови") idx = 1;
        else if (newMode == "тіло") idx = 2;
        else if (newMode == "підхвости") idx = 3;
        else if (newMode == "хвости") idx = 4;
        
        if (idx >= 0) {
            v56_speed = modeSpeeds[idx];
            v58_target = modeTargets[idx];
            v57_volume = 0.0;
            v55_elapsed = 0;
            v61_lastUpdate = 0;
            v66_lastSpeed = v56_speed;
            v60_startTime = now;
            
            if (v56_speed > 0.0) {
                v54_timeEst = (unsigned long)((v58_target / v56_speed) * 3600.0);
                v63_endTime = now + v54_timeEst;
            } else {
                v54_timeEst = 0;
                v63_endTime = 0;
            }
            
            setArduinoCommand("v18", String(v56_speed));
        }
    }
    
    currentMode = newMode;
    Serial1.println("Режим змінено на: " + newMode);
}

// ─── Функція оновлення стану ректифікації (міграція скрипта 2) ─────────────
void updateRectificationState() {
    if (currentMode == "стоп") {
        v55_elapsed = 0;
        v61_lastUpdate = 0;
        v57_volume = 0.0;
        v80_completed = 0;  // Замінено false на 0
        return;
    }
    
    unsigned long now = millis() / 1000;
    float currentSpeed = getSerialValue("v0");
    
    if (currentSpeed == 0.0) return;
    
    v55_elapsed = now - v60_startTime;
    
    if (v57_volume >= v58_target && v58_target > 0.0) {
        v80_completed = 1;  // true
        currentMode = "стоп";
        setArduinoCommand("v18", "0");
        Serial1.println("Режим завершено!");
        return;
    }
    
    unsigned long delta = now - v61_lastUpdate;
    if (delta == 0) return;
    
    float missedVolume = (currentSpeed / 3600.0) * (float)delta;
    v57_volume += missedVolume;
    if (v57_volume > v58_target) v57_volume = v58_target;
    
    v61_lastUpdate = now;
    v66_lastSpeed = currentSpeed;
    
    unsigned long v59_remaining = 0;
    if (currentSpeed > 0.0 && v57_volume < v58_target) {
        v59_remaining = (unsigned long)(((v58_target - v57_volume) / currentSpeed) * 3600.0);
        v63_endTime = now + v59_remaining;
    }
    
    int completedFlag = v80_completed ? 1 : 0;
    String payload = "v57=" + String(v57_volume, 3) + "|v55=" + String(v55_elapsed) + "|v59=" + String(v59_remaining) + "|v80=" + String(completedFlag);
    mqttClient.publish(mqttPubTopic.c_str(), payload.c_str());
}

// ─── Функція керування ББК (міграція скрипта ББК) ───────────────────────────
void updateBZKControl() {
    if (!bzkEnabled) {
        setArduinoCommand("v31", "0");
        bzkStage = BZK_OFF;  // OFF перейменовано на BZK_OFF
        return;
    }
    
    float tempTop = getSerialValue("v1");
    float tempBottom = getSerialValue("v3");
    unsigned long now = millis() / 1000;
    
    if (tempTop > MAX_TOP_TEMP) {
        setArduinoCommand("v31", "0");
        Serial1.println("АВАРІЯ: Перегрів верху!");
        return;
    }
    if (bzkStage > BZK_OFF && tempBottom < MIN_BOTTOM_TEMP) {  // OFF перейменовано на BZK_OFF
        setArduinoCommand("v31", "0");
        Serial1.println("Пауза: Низ охолонув!");
        return;
    }
    
    switch (bzkStage) {
        case BZK_OFF:  // OFF перейменовано на BZK_OFF
            setArduinoCommand("v31", "0");
            if (tempTop >= WARMUP_TARGET) {
                if (fabs(tempTop - bzkLastError) < 0.5) {
                    if (bzkTimer == 0) bzkTimer = now;
                    if (now - bzkTimer >= 60) {
                        bzkStage = STABILIZE;
                        bzkTimer = 0;
                    }
                } else {
                    bzkTimer = 0;
                }
            }
            bzkLastError = tempTop;
            break;
            
        case STABILIZE:
            setArduinoCommand("v31", String((int)MIN_PWM));
            if (fabs(tempBottom - bzkLastError) < 0.2) {
                if (bzkTimer == 0) bzkTimer = now;
                if (now - bzkTimer >= 60) {
                    bzkV91_baseTemp = tempBottom;
                    bzkV97_targetTemp = tempBottom - DELTA;
                    bzkIntegral = MIN_PWM;
                    bzkStage = WORK;
                    bzkTimer = 0;
                }
            } else {
                bzkTimer = 0;
            }
            bzkLastError = tempBottom;
            break;
            
        case WORK: {
            float error = tempBottom - bzkV97_targetTemp;
            bzkIntegral += error * KI;
            if (bzkIntegral > MAX_PWM) bzkIntegral = MAX_PWM;
            if (bzkIntegral < MIN_PWM) bzkIntegral = MIN_PWM;
            
            float pwmCalc = (error * KP) + bzkIntegral;
            if (pwmCalc > MAX_PWM) pwmCalc = MAX_PWM;
            if (pwmCalc < MIN_PWM) pwmCalc = MIN_PWM;
            
            int pwm = (int)round(pwmCalc);
            setArduinoCommand("v31", String(pwm));
            Serial1.println("ББК: Робота, PWM=" + String(pwm) + ", ціль=" + String(bzkV97_targetTemp, 1));
            break;
        }
    }
    
    String status = (bzkStage == BZK_OFF) ? "ВИМКНЕНО" : (bzkStage == WARMUP) ? "ПРОГРІВ" : (bzkStage == STABILIZE) ? "СТАБІЛІЗАЦІЯ" : "РОБОТА";  // OFF перейменовано на BZK_OFF
    String payload = "v86=" + status + "|v90=" + String((int)bzkStage) + "|v82=" + String(bzkLastError, 2);
    mqttClient.publish(mqttPubTopic.c_str(), payload.c_str());
}

// ─── Функція вмикання/вимикання ББК ──────────────────────────────────────────
void setBZKEnabled(bool enabled) {
    bzkEnabled = enabled;
    if (!enabled) {
        bzkStage = BZK_OFF;  // OFF перейменовано на BZK_OFF
        bzkTimer = 0;
    }
}

// ─── Функція ініціалізації режимів (викликати в setup) ──────────────────────
void initDistillationModes() {
    // Завантаження налаштувань з EEPROM або за замовчуванням
    // Наприклад, зчитати modeSpeeds та modeTargets
    Serial1.println("Режими дистилляції ініціалізовані.");
}

// ─── Функція щосекундного оновлення режимів (викликати в loop) ───────────────
void updateDistillationModes() {
    static unsigned long lastUpdate = 0;
    unsigned long now = millis() / 1000;
    if (now - lastUpdate >= 1) {
        lastUpdate = now;
        updateRectificationState();
        updateBZKControl();
    }
}