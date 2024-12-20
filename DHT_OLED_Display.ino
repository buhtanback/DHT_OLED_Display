#include "Config.h"
#include <Wire.h>
#include <U8g2lib.h>
#include <DHT.h>
#include "painlessMesh.h"
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP085_U.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

#define MESH_PREFIX "buhtan"
#define MESH_PASSWORD "buhtan123"
#define MESH_PORT 5555

painlessMesh mesh;

#define OLED_RESET      -1  
#define JOYSTICK_X_PIN  35
#define JOYSTICK_Y_PIN  34
#define BUTTON_PIN      5   

#define DHTPIN          4   
#define DHTTYPE         DHT22   

#define DEBOUNCE_DELAY 50 // Затримка в мілісекундах

DHT dht(DHTPIN, DHTTYPE);
Adafruit_BMP085_Unified bmp = Adafruit_BMP085_Unified(10085);

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, OLED_RESET);


int botShotCounter = 0; // Лічильник пострілів бота
int playerHP = 5;
int botHP = 5;
int botAngle = 0;
bool botAngleSet = false;
int playerAngle = 0;
bool playerTurn = true; // черга гравця
float catapultBulletX, catapultBulletY;
float catapultBulletSpeedX, catapultBulletSpeedY;
bool catapultBulletActive = false;
bool bulletFromPlayer = true;
unsigned long lastShotTime = 0; // Для відстеження часу між пострілами
const unsigned long botDelay = random(1000, 3000);  // 1 секунда для паузи бота
unsigned long endGameDisplayTime = 0; // Час відображення кінця гри
bool gameEnded = false; // Статус кінця гри

const float GRAVITY = 0.1;
bool buttonLongPress = false;



static unsigned char pig_bits[] = {
  0x21, 0x72, 0xb4, 0xfe, 0x7f, 0x3f, 0x22, 0x22 };

int calculateBotAngle() {
    // Відстань між ботом і гравцем
    float dx = abs(20 - 108); // Модуль горизонтальної відстані
    float dy = 50 - 50;       // Вертикальна різниця (обидві катапульти на одній висоті)

    // Початкова швидкість
    float speed = 3.0;
    float g = GRAVITY;

    // Лічильник пострілів
    static int botShotCounter = 0; // Робимо змінну статичною
    botShotCounter++;

    // Розрахунок параметра для перевірки досяжності
    float delta = pow(speed, 4) - g * (g * pow(dx, 2) + 2 * dy * pow(speed, 2));

    // Якщо траєкторія фізично можлива
    if (delta >= 0) {
        // Кожен третій постріл бот стріляє точно
        if (botShotCounter % 3 == 0) {
            // Розрахунок точного кута
            float angleRadians = atan((pow(speed, 2) - sqrt(delta)) / (g * dx));
            return degrees(angleRadians); // Конвертуємо в градуси
        } else {
            // Для інших пострілів бот спеціально "промахується"
            int randomOffset = random(-10, 10); // Додаємо випадковий зсув
            float angleRadians = atan((pow(speed, 2) - sqrt(delta)) / (g * dx));
            return constrain(degrees(angleRadians) + randomOffset, 20, 70); // Обмеження кутів
        }
    } else {
        // Якщо траєкторія неможлива, повертаємо випадковий кут
        return random(30, 60); // Універсальний промах
    }
}



int menuOption = 0;
int buttonState = 0;
int lastButtonState = 0;
unsigned long lastButtonDebounceTime = 0;
unsigned long lastJoystickDebounceTime = 0;
unsigned long buttonDebounceDelay = 75;
unsigned long joystickDebounceDelay = 300;
bool inSubMenu = false;
bool screensaverMode = false;

int buttonPressCount = 0;
unsigned long firstButtonPressTime = 0;
const unsigned long multiClickInterval = 500;

float lastSentTemperature = -999.0;
float lastSentHumidity = -999.0;
float lastSentPressure = -999.0;

unsigned long welcomeScreenStartTime = 0;
bool isWelcomeScreenVisible = false;

unsigned long stopwatchStartTime = 0;
bool stopwatchRunning = false;
unsigned long lastSerialUpdateTime = 0;
unsigned long serialUpdateInterval = 1000;
int lastPrintedSecond = -1;

String weatherDescription;
float weatherTemp;

long utcOffsetInSecondsWinter = 7200;  // Зимовий час (GMT+2 для вашого UTC+3)
long utcOffsetInSecondsSummer = 10800; // Літній час (GMT+3 для вашого UTC+3)

bool isBMPAvailable = true;

static unsigned long lastButtonPressTime = 0;
const int joystickDeadZone = 300; // Налаштуй це значення за потреби
const int smoothingFactor = 10;
int joystickYReadings[smoothingFactor];
int currentReadingIndex = 0;
int joystickCenterY = 0;
int joystickCenterX = 0;
unsigned long lastMenuChangeTime = 0; // Час останньої зміни пункту меню
const unsigned long menuChangeDelay = 300; // 300 мс затримка між переходами
const int deltaBufferSize = 5;

int deltaBuffer[deltaBufferSize]; // Масив для збереження змін `deltaY`
int deltaBufferIndex = 0; // Індекс для запису значень у буфер
unsigned long joystickTiltStartTime = 0;
const unsigned long joystickTiltDuration = 200;



int getStableDelta() {
    long total = 0;
    for (int i = 0; i < deltaBufferSize; i++) {
        total += deltaBuffer[i];
    }
    return total / deltaBufferSize;
}

int getSmoothedJoystickY() {
    long total = 0;
    for (int i = 0; i < smoothingFactor; i++) {
        total += joystickYReadings[i];
    }
    return total / smoothingFactor;
}

int getMedianJoystickY() {
    int sortedReadings[smoothingFactor];
    memcpy(sortedReadings, joystickYReadings, sizeof(joystickYReadings));
    // Сортування масиву
    for (int i = 0; i < smoothingFactor - 1; i++) {
        for (int j = i + 1; j < smoothingFactor; j++) {
            if (sortedReadings[i] > sortedReadings[j]) {
                int temp = sortedReadings[i];
                sortedReadings[i] = sortedReadings[j];
                sortedReadings[j] = temp;
            }
        }
    }
    // Повертаємо середнє значення
    return sortedReadings[smoothingFactor / 2];
}

bool isButtonPressed() {
    static unsigned long lastDebounceTime = 0;
    static bool lastButtonState = HIGH; // Припустимо, кнопка нормально розімкнута

    bool currentButtonState = digitalRead(BUTTON_PIN);

    // Якщо стан кнопки змінився
    if (currentButtonState != lastButtonState) {
        lastDebounceTime = millis(); // Оновлюємо час останньої зміни
    }

    // Якщо стан кнопки стабільний понад 50 мс
    if ((millis() - lastDebounceTime) > 50) {
        if (currentButtonState == LOW && lastButtonState == HIGH) {
            lastButtonState = currentButtonState;
            return true; // Кнопка натиснута
        }
    }

    lastButtonState = currentButtonState;
    return false; // Кнопка не натиснута або дребезг
}


bool isShootButtonPressed() {
    static unsigned long lastDebounceTime = 0;
    static bool lastButtonState = HIGH; // Припускаємо, що кнопка нормально розімкнута

    bool currentButtonState = digitalRead(BUTTON_PIN);

    if (currentButtonState != lastButtonState) {
        lastDebounceTime = millis();
    }

    if ((millis() - lastDebounceTime) > 50) { // Дебаунсинг (50 мс)
        if (currentButtonState == LOW && lastButtonState == HIGH) {
            lastButtonState = currentButtonState;
            return true; // Кнопка натиснута
        }
    }

    lastButtonState = currentButtonState;
    return false; // Кнопка не натиснута
}




long getCurrentUtcOffset(unsigned long epochTime) {
    struct tm * timeInfo = gmtime((time_t *)&epochTime);

    int month = timeInfo->tm_mon + 1; // Місяці від 0 до 11
    int day = timeInfo->tm_mday;

    if ((month > 3 && month < 10) || (month == 3 && day >= 25) || (month == 10 && day < 25)) {
        return 10800; // Літній час (UTC+2)
    } else {
        return 7200; // Зимовий час (UTC+1)
    }
}

// Ініціалізуємо NTP-клієнт без встановлення зміщення часу
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "time.nist.gov", 0);



unsigned long previousMillis = 0;
const long interval = 10000;

unsigned long lastWeatherUpdateTime = 0;
const unsigned long weatherUpdateInterval = 500; 

unsigned long lastPressureUpdateTime = 0;  // Час останнього оновлення тиску
const unsigned long pressureUpdateInterval = 60000;  // 


int totalMenuOptions = 11; // Загальна кількість пунктів меню
int maxDisplayOptions = 3;



int setMinutes = 0;
int setSeconds = 0;
unsigned long countdownTimeInMillis = 0;
bool timerRunning = false;
bool isTimerStarted = false;
int selectedButton = 0;    // 0: Старт, 1: Скинути
int selectedDigit = 0;     // 0: хвилини, 1: секунди

// Параметри гри
int paddleWidth = 20;
int paddleHeight = 3;
float bottomPaddleX = (128 - paddleWidth) / 2.0;
int topPaddleX = (128 - paddleWidth) / 2;
float ballX = 64.0;
float ballY = 32.0;
float ballDirX = 1.0;
float ballDirY = 1.0;
int score = 0;
int joystickCenter = 2048;
const int joystickThreshold = 500;
const long gameInterval = 10; 

// Змінні для гри Space Invaders
const int screenWidth = 128;
const int screenHeight = 64;
const int playerWidth = 10;
const int playerHeight = 3;
int playerX = screenWidth / 2 - playerWidth / 2;

const int bulletWidth = 2;
const int bulletHeight = 3;

const int enemyWidth = 8;
const int enemyHeight = 6;
const int maxEnemies = 5;
int enemyX[maxEnemies];
int enemyY[maxEnemies];
bool enemyActive[maxEnemies];
int enemyMoveDelay = 500;
int enemyDirection = 1;

int gameScore = 0;  // Перейменовано, щоб уникнути конфлікту зі змінною score
bool gameOver = false;

bool bulletActive = false;
int bulletX, bulletY;

unsigned long lastMoveTime = 0;
unsigned long lastEnemyMoveTime = 0;
unsigned long lastBulletMoveTime = 0;
const unsigned long bulletMoveInterval = 50;


int playerY = 32;              // Початкова позиція "пташки" по вертикалі
int obstacleX = 128;           // Початкова позиція перешкоди по горизонталі
int gapY = random(0, 64 - 20); // Початкова позиція зазору в перешкоді
int gapHeight = 20;            // Висота зазору
bool passedObstacle = false;   // Прапорець для відстеження, чи пройшла "пташка" перешкоду
unsigned long lastDrawTime = 0; // Час останнього оновлення кадру
const unsigned long drawInterval = 50; // Інтервал оновлення зображення

 unsigned long gameOverTime = 0; // Час, коли гра закінчилась
const unsigned long gameOverDelay = 1000; // Затримка в 1 секунду
   
int deadZone = 100; // Збільшено зону нечутливості

bool buttonPressed = false;         // Стан кнопки
bool exitInProgress = false;        // Чи триває процес виходу
unsigned long buttonPressStartTime = 0; // Час початку натискання кнопки
unsigned long lastActionTime = 0;       // Час останньої дії для debounce
const unsigned long longPressDuration = 1000; // Тривалість довгого натискання
const unsigned long debounceDelay = 300;      // Затримка для debounce
bool blockInputAfterLongPress = false; // Блокування повторного входу після виходу
bool ignoreButtonUntilRelease = false; // Ігнорувати натискання кнопки до її відпускання


bool drawingTrajectory = false;
float posX, posY, velX, velY;
int previousX, previousY, trajectoryStep;
bool isPlayerTrajectory;


bool waitingForRelease = false;
unsigned long lastExitTime = 0; // Змінна для збереження часу останнього виходу
// Глобальні змінні
bool exitButtonPressed = false; // Стан кнопки виходу


void calibrateJoystick() {
    long total = 0;
    const int numReadings = 100;
    for (int i = 0; i < numReadings; i++) {
        total += analogRead(JOYSTICK_Y_PIN);
        delay(5); // Невелика затримка між зчитуваннями
    }
    joystickCenterY = total / numReadings;
    Serial.print("Joystick Center Y Calibrated: ");
    Serial.println(joystickCenterY);
}


void setup() {
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    Serial.begin(115200);

    u8g2.begin();
    u8g2.clearBuffer();
    u8g2.sendBuffer();
    calibrateJoystick();
    dht.begin();
    if (!bmp.begin()) {
        Serial.print("Не вдалося знайти датчик BMP085.");
        isBMPAvailable = false; // Встановлюємо змінну в false, якщо датчик не знайдено
    }

    // Ініціалізація джойстика
    pinMode(JOYSTICK_X_PIN, INPUT);
    pinMode(JOYSTICK_Y_PIN, INPUT);

    // Калібруємо осі джойстика
    joystickCenterY = analogRead(JOYSTICK_Y_PIN);
    Serial.print("Joystick Center Y Calibrated: ");
    Serial.println(joystickCenterY);

    // Інші ініціалізації залишаємо без змін
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    joystickCenter = analogRead(JOYSTICK_X_PIN);

    if (isBMPAvailable) {
        sensors_event_t event;
        bmp.getEvent(&event);
        lastSentPressure = event.pressure;
    }

    lastSentTemperature = dht.readTemperature();
    lastSentHumidity = dht.readHumidity();

    mesh.onNewConnection([](size_t nodeId) {
        // Код для обробки нових з'єднань
    });

    mesh.setDebugMsgTypes(ERROR | STARTUP | CONNECTION);
    mesh.init(MESH_PREFIX, MESH_PASSWORD, MESH_PORT);
    mesh.onReceive([](uint32_t from, String &msg) {
        Serial.printf("Received from %u msg=%s\n", from, msg.c_str());
    });

    showImage();
    welcomeScreenStartTime = millis();
    isWelcomeScreenVisible = true;
}

void exitSubMenu() {
    inSubMenu = false;
    lastExitTime = millis(); // Фіксуємо час виходу
}

void waitForButtonRelease() {
    while (digitalRead(BUTTON_PIN) == LOW) {
        yield(); // Дозволяємо обробляти інші задачі
    }
}


void checkButtonRelease() {
    if (waitingForRelease) {
        if (digitalRead(BUTTON_PIN) == HIGH) {
            waitingForRelease = false; // Кнопка відпущена
            // Дії після відпускання кнопки (можна додати, якщо потрібно)
        }
    }
}

void loop() {
    // Зчитування джойстика
    joystickYReadings[currentReadingIndex] = analogRead(JOYSTICK_Y_PIN);
    currentReadingIndex = (currentReadingIndex + 1) % smoothingFactor;

    int medianJoystickY = getMedianJoystickY();
    int deltaY = medianJoystickY - joystickCenterY;

    // Стабілізація значень
    deltaBuffer[deltaBufferIndex] = deltaY;
    deltaBufferIndex = (deltaBufferIndex + 1) % deltaBufferSize;
    int stableDeltaY = getStableDelta();

    // Вивід для діагностики
    Serial.print("Median Joystick Y: ");
    Serial.print(medianJoystickY);
    Serial.print(" | Stable Delta Y: ");
    Serial.println(stableDeltaY);

    // Вихід у підменю
    if (!inSubMenu && (millis() - lastExitTime > 500)) {
        if (isButtonPressed()) {
            inSubMenu = true;
            waitForButtonRelease();
        }
    }

    // Перевіряємо, чи кнопка була відпущена після входу в підменю
    if (inSubMenu) {
        checkButtonRelease();
    }

    // Перевірка заставок
    if (screensaverMode) {
        showImage();
        readAndSendData();
        return;
    }

    if (isWelcomeScreenVisible) {
        if (millis() - welcomeScreenStartTime > 3000) {
            isWelcomeScreenVisible = false;
        } else {
            readAndSendData();
            return;
        }
    }

    // Оновлення Mesh для підтримки зв’язку
    mesh.update();

    // Дебаунс кнопки
    if (millis() - lastButtonPressTime > buttonDebounceDelay) {
        handleButtonPress();
        lastButtonPressTime = millis();
    }

    if (!inSubMenu) {
        if (millis() - lastMenuChangeTime > menuChangeDelay) {
            if (stableDeltaY < -joystickDeadZone) {  // Рух вгору
                if (joystickTiltStartTime == 0) {
                    joystickTiltStartTime = millis();
                } else if (millis() - joystickTiltStartTime > joystickTiltDuration) {
                    menuOption--;
                    if (menuOption < 0) {
                        menuOption = totalMenuOptions - 1;
                    }
                    lastMenuChangeTime = millis();
                    Serial.print("Menu Option Up: ");
                    Serial.println(menuOption);
                    joystickTiltStartTime = 0;
                }
            } else if (stableDeltaY > joystickDeadZone) {  // Рух вниз
                if (joystickTiltStartTime == 0) {
                    joystickTiltStartTime = millis();
                } else if (millis() - joystickTiltStartTime > joystickTiltDuration) {
                    menuOption++;
                    if (menuOption >= totalMenuOptions) {
                        menuOption = 0;
                    }
                    lastMenuChangeTime = millis();
                    Serial.print("Menu Option Down: ");
                    Serial.println(menuOption);
                    joystickTiltStartTime = 0;
                }
            } else {
                joystickTiltStartTime = 0;
            }
        }

        // Відображення меню
        showMenu();
    } else {
        // Вхід у підменю: виконуємо вибраний пункт
        handleMenuOption(menuOption);
    }

    // Оновлення даних з датчиків і Mesh із певним інтервалом
    if (millis() - lastSerialUpdateTime >= serialUpdateInterval) {
        readAndSendData();
        lastSerialUpdateTime = millis();
    }
}




// Функція для обробки пунктів меню
void handleMenuOption(int menuOption) {
    switch (menuOption) {
        case 0:
            showTemperatureAndHumidity();
            break;
        case 1:
            showWeather();
            break;
        case 2:
            showStopwatch();
            break;
        case 3:
            showPressure();
            break;
        case 4:
            showTimer();
            break;
        case 5:
            showGame();
            break;
        case 6:
            showSpaceInvaders();
            break;
        case 7:
            showFlappyBird();
            break;
        case 8:
            showCatapultGame();
            break;
        case 9:
            showImage();
            break;
        case 10:
            showScreensaver();  // Новий пункт меню "Лого"
            break;
        default:
            Serial.println("Невідомий пункт меню!");
            break;
    }
}


void handleButtonPress() {
    int reading = digitalRead(BUTTON_PIN);

    if (reading != lastButtonState) {
        lastButtonDebounceTime = millis();
    }

    if ((millis() - lastButtonDebounceTime) > buttonDebounceDelay) {
        if (reading != buttonState) {
            buttonState = reading;
            if (buttonState == LOW) {
                buttonPressCount++;
                if (buttonPressCount == 1) {
                    firstButtonPressTime = millis();
                }
            }
        }
    }

    lastButtonState = reading;

    if (buttonPressCount > 0 && (millis() - firstButtonPressTime) > multiClickInterval) {
        if (buttonPressCount == 1) {
            if (!inSubMenu && !screensaverMode) {
                inSubMenu = true;
                if (menuOption == 2) {
                    stopwatchStartTime = millis();
                    stopwatchRunning = true;
                }
                Serial.print("Button pressed once. Entering subMenu: ");
                Serial.println(menuOption);
            }
        }
        buttonPressCount = 0;
    }
}

void showTemperatureAndHumidity() {
    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();
    float pressure = -1.0;

    if (isBMPAvailable) {
        sensors_event_t event;
        bmp.getEvent(&event);
        pressure = event.pressure;
    }

    u8g2.clearBuffer();
    u8g2.enableUTF8Print();
    u8g2.setFont(u8g2_font_cu12_t_cyrillic);
    u8g2.setCursor(0, 15);
    u8g2.printf("Кімната: %.2f C", temperature);
    u8g2.setCursor(0, 30);
    u8g2.printf("Волога: %.2f %%", humidity);

    if (isBMPAvailable) {
        u8g2.setCursor(0, 45);
        u8g2.printf("Тиск: %.2f hPa", pressure);
    } else {
        u8g2.setCursor(0, 45);
        u8g2.print("Тиск: недоступний");
    }

    u8g2.sendBuffer();

    if (handleReturnButton()) {
        inSubMenu = false;
        Serial.println("Button pressed. Returning to main menu.");
    }
}


void sendPressureData(float pressure) {
    if (isBMPAvailable && pressure != -999) {
        char msg[30];
        sprintf(msg, "Pressure: %.2f hPa", pressure);
        mesh.sendBroadcast(msg);
        Serial.printf("Sent pressure to mesh: %.2f hPa\n", pressure);
    }
}

void readAndSendData() {
    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();
    float pressure = -1.0;

    if (isBMPAvailable) {
        sensors_event_t event;
        bmp.getEvent(&event);
        pressure = event.pressure;
    }

    char tempMsg[20];
    sprintf(tempMsg, "Temp: %.2f C", temperature);
    mesh.sendBroadcast(tempMsg);
    Serial.printf("Sent temperature: %.2f C\n", temperature);
    lastSentTemperature = temperature;

    char humMsg[20];
    sprintf(humMsg, "Humidity: %.2f %%", humidity);
    mesh.sendBroadcast(humMsg);
    Serial.printf("Sent humidity: %.2f %%\n", humidity);
    lastSentHumidity = humidity;

    if (isBMPAvailable) {
        char pressMsg[30];
        sprintf(pressMsg, "Pressure: %.2f hPa", pressure);
        mesh.sendBroadcast(pressMsg);
        Serial.printf("Sent pressure: %.2f hPa\n", pressure);
        lastSentPressure = pressure;
    }
}



void sendTemperatureAndHumidityData(float temperature, float humidity) {
    char tempMsg[20];
    char humMsg[20];

    // Формуємо і відправляємо дані про температуру і вологість через Mesh
    sprintf(tempMsg, "Temp: %.2f C", temperature);
    sprintf(humMsg, "Humidity: %.2f %%", humidity);

    mesh.sendBroadcast(tempMsg);
    mesh.sendBroadcast(humMsg);

    Serial.printf("Sent temperature: %.2f C, humidity: %.2f %%\n", temperature, humidity);
}

void showStopwatch() {
    unsigned long previousMillis = 0;
    const unsigned long interval = 1000;  // Оновлюємо екран кожну секунду

    while (inSubMenu) {
        unsigned long currentMillis = millis();

        // Оновлюємо дисплей кожну секунду
        if (currentMillis - previousMillis >= interval) {
            previousMillis = currentMillis;

            unsigned long elapsed = millis() - stopwatchStartTime;
            int seconds = (elapsed / 1000) % 60;
            int minutes = (elapsed / 60000);

            u8g2.clearBuffer();
            u8g2.setFont(u8g2_font_cu12_t_cyrillic);
            u8g2.setCursor(0, 15);
            u8g2.printf("Секундомір: %02d:%02d", minutes, seconds);
            u8g2.sendBuffer();

            Serial.printf("Stopwatch time: %02d:%02d\n", minutes, seconds);
        }

        // Оновлення Mesh-сітки для підтримання зв'язку
        mesh.update();

        // Викликаємо функцію readAndSendData() для постійного надсилання даних із сенсорів
        readAndSendData();

        // Перевіряємо на натискання кнопки для виходу з підменю
        if (handleReturnButton()) {
            inSubMenu = false;
            stopwatchRunning = false;
            Serial.println("Button pressed. Returning to main menu.");
        }
    }
}

void showImage() {
    u8g2.clearBuffer();
    u8g2.drawBitmap(0, 0, 16, 128, image); // Приклад малювання зображення (перевірте параметри drawBitmap)
    u8g2.sendBuffer();

    // Логіка виходу з екрану "Лого" при тривалому натисканні кнопки
    static unsigned long buttonPressTime = 0;
    if (digitalRead(BUTTON_PIN) == LOW) {
        if (buttonPressTime == 0) {
            buttonPressTime = millis(); // Початок натискання
        } else if (millis() - buttonPressTime > 1000) { // Тривале натискання понад 1 секунду
            inSubMenu = false; // Повернення до меню
            buttonPressTime = 0;
        }
    } else {
        buttonPressTime = 0; // Скидання часу натискання, якщо кнопка відпущена
    }
}


void showMenu() {
    u8g2.clearBuffer();
    u8g2.enableUTF8Print();
    u8g2.setFont(u8g2_font_cu12_t_cyrillic);

    int startOption = menuOption / maxDisplayOptions * maxDisplayOptions;

    for (int i = 0; i < maxDisplayOptions; i++) {
        int optionIndex = startOption + i;
        if (optionIndex >= totalMenuOptions) break; // Якщо перевищуємо кількість опцій

        int y = 20 + i * 20;
        u8g2.setCursor(10, y);

        // Оновлені назви для нових пунктів меню
        String optionText = (optionIndex == 0) ? "Кімната" : 
                            (optionIndex == 1) ? "Вулиця" :
                            (optionIndex == 2) ? "Секундомір" : 
                            (optionIndex == 3) ? "Тиск" : 
                            (optionIndex == 4) ? "Таймер" :
                            (optionIndex == 5) ? "Гра" :
                            (optionIndex == 6) ? "Space Invaders" : 
                            (optionIndex == 7) ? "Flappy Bird" :
                            (optionIndex == 8) ? "Catapult" :
                            (optionIndex == 9) ? "Лого" :
                            (optionIndex == 10) ? "ScreenSaver" : "";  

        int textWidth = u8g2.getStrWidth(optionText.c_str());

        if (menuOption == optionIndex) {
            u8g2.drawRBox(10 - 4, y - 15, textWidth + 8, 15, 4);
            u8g2.print("> ");
        } else {
            u8g2.print("  ");
        }

        u8g2.print(optionText);
    }

    u8g2.sendBuffer();
}



void connectToWiFi() {
    Serial.println("Attempting to connect to WiFi...");
    WiFi.begin(ssid, password);
    int attempt = 0;
    unsigned long startAttemptTime = millis();
    while (WiFi.status() != WL_CONNECTED && attempt < 30) {
        if (millis() - startAttemptTime >= 1000) {
            Serial.print(".");
            startAttemptTime = millis();
            attempt++;
        }
    }
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Connected to WiFi");
    } else {
        Serial.println("Failed to connect to WiFi");
    }
}

void updateWeather() {
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Attempting to update weather...");
        HTTPClient http;
        http.begin("http://api.openweathermap.org/data/2.5/weather?q=Khmelnytskyi&lang=ua&appid=89b5c4878e84804573dae7a6c3628e94&units=metric");
        int httpCode = http.GET();

        if (httpCode > 0) {
            String payload = http.getString();
            Serial.println("Weather Payload: " + payload);
            DynamicJsonDocument doc(2048);
            deserializeJson(doc, payload);
            weatherDescription = doc["weather"][0]["description"].as<String>();
            weatherTemp = doc["main"]["temp"];
            Serial.printf("Weather updated: %s, %.2f °C\n", weatherDescription.c_str(), weatherTemp);
        } else {
            Serial.println("HTTP GET failed: " + String(httpCode));
        }
        http.end();
    } else {
        Serial.println("Not connected to WiFi");
    }
}

void showWeather() {
    mesh.stop();
    connectToWiFi();
    updateWeather();  // Оновлюємо погоду

    // Оновлюємо час лише раз перед входом у цикл
    timeClient.update();  // Отримуємо час з NTP-сервера
    unsigned long epochTime = timeClient.getEpochTime();
    long utcOffset = getCurrentUtcOffset(epochTime);
    timeClient.setTimeOffset(utcOffset);

    unsigned long lastTimeUpdate = 0;
    const unsigned long timeUpdateInterval = 600000; // 10 хвилин

    while (inSubMenu) {
        // Оновлюємо час раз на 10 хвилин
        if (millis() - lastTimeUpdate >= timeUpdateInterval) {
            timeClient.update();
            lastTimeUpdate = millis();
        }

        int currentHour = timeClient.getHours();
        int currentMinute = timeClient.getMinutes();
        int currentSecond = timeClient.getSeconds();

        struct tm *ptm = gmtime((time_t *)&epochTime);
        int currentDay = ptm->tm_mday;
        int currentMonth = ptm->tm_mon + 1;
        int currentYear = (ptm->tm_year + 1900) % 100;

        char dateString[9];
        sprintf(dateString, "%d.%d.%02d", currentDay, currentMonth, currentYear);

        if (millis() - lastWeatherUpdateTime >= weatherUpdateInterval) {
            lastWeatherUpdateTime = millis();

            u8g2.clearBuffer();
            u8g2.enableUTF8Print();
            u8g2.setFont(u8g2_font_cu12_t_cyrillic);
            u8g2.setCursor(0, 15);
            u8g2.printf(" %s", weatherDescription.c_str());
            u8g2.setCursor(0, 30);
            u8g2.printf("Вулиця: %.2f C", weatherTemp);
            u8g2.setCursor(0, 45);
            u8g2.printf("%s %02d:%02d:%02d", dateString, currentHour, currentMinute, currentSecond);
            u8g2.sendBuffer();
        }

        if (handleReturnButton()) {
            inSubMenu = false;
            Serial.println("Button pressed. Returning to main menu.");
            WiFi.disconnect();
            mesh.init(MESH_PREFIX, MESH_PASSWORD, MESH_PORT);
            timeClient.end();
        }
    }
}


void showTimer() {
    unsigned long currentMillis = millis();
    const unsigned long debounceDelay = 200;
    static unsigned long lastJoystickMove = 0;
    static unsigned long lastButtonPressTime = 0;
    static unsigned long buttonPressDuration = 0;  // Час утримання кнопки для виходу
    static bool isSelectingTime = true;            // Перемикання між вибором часу і кнопок
    static int selectedDigit = 0;                  // 0: хвилини, 1: секунди
    static int selectedButton = 0;                 // 0: Старт, 1: Скинути

    // Зчитуємо стан джойстика
    int joystickX = analogRead(JOYSTICK_X_PIN);  // Горизонтальне переміщення
    int joystickY = analogRead(JOYSTICK_Y_PIN);  // Вертикальне переміщення

    // Обробка горизонтального переміщення джойстика
    if (currentMillis - lastJoystickMove > debounceDelay) {
        if (joystickX < 1000) {  // Ліворуч
            if (isSelectingTime) {
                selectedDigit = (selectedDigit == 0) ? 1 : 0;  // Перемикаємо між хвилинами і секундами
            } else {
                selectedButton = (selectedButton == 0) ? 1 : 0;  // Перемикаємо між "Старт" і "Скинути"
            }
            lastJoystickMove = currentMillis;
        } else if (joystickX > 3000) {  // Праворуч
            if (isSelectingTime) {
                selectedDigit = (selectedDigit == 0) ? 1 : 0;  // Перемикаємо між хвилинами і секундами
            } else {
                selectedButton = (selectedButton == 0) ? 1 : 0;  // Перемикаємо між "Старт" і "Скинути"
            }
            lastJoystickMove = currentMillis;
        }
    }

    // Обробка вертикального переміщення джойстика
    if (currentMillis - lastJoystickMove > debounceDelay) {
        if (joystickY < 1000) {  // Вгору
            if (!isSelectingTime) {
                isSelectingTime = true;  // Повертаємося до вибору часу
            } else {
                // Збільшуємо значення хвилин або секунд
                if (selectedDigit == 0) {
                    setMinutes = (setMinutes + 1) % 100;  // Максимум 99 хвилин
                } else {
                    setSeconds = (setSeconds + 1) % 60;   // Максимум 59 секунд
                }
            }
            lastJoystickMove = currentMillis;
        } else if (joystickY > 3000) {  // Вниз
            if (isSelectingTime) {
                isSelectingTime = false;  // Переходимо до вибору кнопок
            } else {
                // Зменшуємо значення хвилин або секунд
                if (selectedButton == 0) {
                    setMinutes = (setMinutes == 0) ? 99 : setMinutes - 1;
                } else {
                    setSeconds = (setSeconds == 0) ? 59 : setSeconds - 1;
                }
            }
            lastJoystickMove = currentMillis;
        }
    }

    // Відображення інтерфейсу
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_cu12_t_cyrillic);

    // Відображаємо хвилини і секунди (тільки якщо таймер ще не запущений)
    if (!timerRunning) {
        u8g2.setCursor(0, 15);
        u8g2.print("Таймер: ");
        if (isSelectingTime && selectedDigit == 0) {
            u8g2.print(">");
        } else {
            u8g2.print(" ");
        }
        u8g2.printf("%02d:", setMinutes);
        if (isSelectingTime && selectedDigit == 1) {
            u8g2.print(">");
        } else {
            u8g2.print(" ");
        }
        u8g2.printf("%02d", setSeconds);
    } else {
        int seconds = (countdownTimeInMillis / 1000) % 60;
        int minutes = (countdownTimeInMillis / 60000);

        u8g2.setCursor(0, 15);
        u8g2.printf("Таймер: %02d:%02d", minutes, seconds);
    }

    // Відображаємо кнопки "Старт" і "Скинути"
    u8g2.setCursor(0, 40);
    if (!isSelectingTime && selectedButton == 0) {
        u8g2.print("> Старт  ");
    } else {
        u8g2.print("  Старт  ");
    }
    if (!isSelectingTime && selectedButton == 1) {
        u8g2.print("> Скинути");
    } else {
        u8g2.print("  Скинути");
    }
    u8g2.sendBuffer();

    // Обробка натискання кнопки для запуску або скидання таймера
    if (digitalRead(BUTTON_PIN) == LOW && currentMillis - lastButtonPressTime > debounceDelay) {
        lastButtonPressTime = currentMillis;
        if (!timerRunning) {
            if (!isSelectingTime) {
                if (selectedButton == 0) {
                    // Запускаємо таймер
                    timerRunning = true;
                    countdownTimeInMillis = (setMinutes * 60000UL) + (setSeconds * 1000UL);
                    previousMillis = currentMillis;
                    Serial.println("Таймер запущено");
                } else if (selectedButton == 1) {
                    // Скидаємо таймер
                    resetTimer();
                    Serial.println("Таймер скинуто");
                }
            }
        } else {
            timerRunning = false;
            Serial.println("Таймер зупинено");
        }
    }

    // Логіка відліку таймера
    if (timerRunning && countdownTimeInMillis > 0) {
        if (currentMillis - previousMillis >= 1000) {
            countdownTimeInMillis -= 1000;
            previousMillis = currentMillis;

            // Якщо таймер завершено
            if (countdownTimeInMillis <= 0) {
                timerRunning = false;
                countdownTimeInMillis = 0;
                Serial.println("Таймер завершено!");
            }
        }
    }

    // Логіка для тривалого натискання кнопки для виходу з підменю
    if (digitalRead(BUTTON_PIN) == LOW) {
        if (buttonPressDuration == 0) {
            buttonPressDuration = currentMillis;
        }
        if (currentMillis - buttonPressDuration > 1000) {
            inSubMenu = false;
            timerRunning = false;
            Serial.println("Вихід з підменю");
            buttonPressDuration = 0;
        }
    } else {
        buttonPressDuration = 0;
    }

    // Оновлення Mesh та інших функцій
    mesh.update();
    readAndSendData();
}


void showFlappyBird() {
    if (digitalRead(BUTTON_PIN) == LOW) { 
        inSubMenu = false;
        return;
    }

    unsigned long currentMillis = millis();

    // Зчитування значення з джойстика
    int joystickY = analogRead(JOYSTICK_Y_PIN);
    int centerValue = 2048; // Центральне значення для 12-бітного джойстика
    int deadZone = 100; // Зона нечутливості

    // Перевіряємо, чи джойстик в зоні нечутливості
    int deltaY = 0;
    if (joystickY < centerValue - deadZone) {
        deltaY = -1; // Повільно рухаємось вгору
    } else if (joystickY > centerValue + deadZone) {
        deltaY = 1;  // Повільно рухаємось вниз
    }

    // Оновлення позиції гравця тільки кожні 50 мс для більш повільного руху
    static unsigned long lastMoveTime = 0;
    if (currentMillis - lastMoveTime > 50) {
        playerY += deltaY;
        playerY = constrain(playerY, 0, 63 - 12); // Враховуємо висоту свинки (12 пікселів)
        lastMoveTime = currentMillis;
    }

    // Оновлення екрану
    if (currentMillis - lastDrawTime >= drawInterval) {
        lastDrawTime = currentMillis;

        obstacleX -= 2;

        if (obstacleX < -5) {
            obstacleX = 128;
            gapY = random(0, 64 - gapHeight);
            passedObstacle = false;
        }

        u8g2.clearBuffer();
        
        // Замість квадрата малюємо свинку
        u8g2.drawXBMP(5, playerY, 8, 8, pig_bits); // 8x8 пікселів

        // Малюємо перешкоди
        u8g2.drawBox(obstacleX, 0, 5, gapY); 
        u8g2.drawBox(obstacleX, gapY + gapHeight, 5, 64 - (gapY + gapHeight)); 

        // Перевірка на зіткнення
        if (obstacleX < 10 && (playerY < gapY || playerY > gapY + gapHeight)) {
            playerY = 32;
            obstacleX = 128;
            score = 0;
            passedObstacle = false;
        } else if (obstacleX < 5 && !passedObstacle) {
            score++;
            passedObstacle = true;
        }

        // Відображення рахунку
        u8g2.setFont(u8g2_font_6x10_tr);
        u8g2.setCursor(0, 10);
        u8g2.print("Score: ");
        u8g2.print(score);

        u8g2.sendBuffer();
    }
}





void resetTimer() {
    setMinutes = 0;
    setSeconds = 0;
    countdownTimeInMillis = 0;
    timerRunning = false;
    isTimerStarted = false;  // Скидаємо статус таймера
    Serial.println("Таймер скинуто");
}

void showGame() {
    static unsigned long gamePreviousMillis = 0;
    static unsigned long lastBounceTime = 0;        // Час останнього відскоку від платформи
    static unsigned long lastButtonPressTime = 0;   // Час останнього натискання кнопки
    static bool ballAbovePaddle = true;             // Стан м'ячика відносно платформи
    const long gameInterval = 10;                   // Інтервал для оновлення гри
    const long bounceDelay = 100;                   // Мінімальний інтервал між відскоками
    const long buttonDebounceDelay = 200;           // Інтервал для обробки "дребезгу" кнопки
    unsigned long currentMillis = millis();

    // Оновлюємо логіку гри з заданим інтервалом
    if (currentMillis - gamePreviousMillis >= gameInterval) {
        gamePreviousMillis = currentMillis;

        // Читання положення джойстика
        int joystickX = analogRead(JOYSTICK_X_PIN);
        int joystickOffset = joystickCenter - joystickX;

        // Рух панелі на основі зміщення джойстика
        if (joystickOffset > joystickThreshold) { // Рух праворуч
            bottomPaddleX += 1.0;
        } else if (joystickOffset < -joystickThreshold) { // Рух ліворуч
            bottomPaddleX -= 1.0;
        }

        // Обмеження положення панелі в межах екрану
        if (bottomPaddleX < 0) bottomPaddleX = 0;
        if (bottomPaddleX > 128 - paddleWidth) bottomPaddleX = 128 - paddleWidth;

        // Оновлення позиції м'ячика
        ballX += ballDirX;
        ballY += ballDirY;

        // Відбивання м'ячика від стінок
        if (ballX <= 0 || ballX >= 128) ballDirX *= -1;

        // Перевірка зіткнення з верхньою платформою
        if (ballY <= paddleHeight && ballX >= topPaddleX && ballX <= topPaddleX + paddleWidth) {
            // Відбивання м'ячика від верхньої панелі
            if (currentMillis - lastBounceTime > bounceDelay) {
                // Зміна напряму руху м'ячика після відбивання від верхньої платформи
                ballDirY *= -1; // Змінюємо вертикальний напрям

                // Додаємо випадковий кут для горизонтального напряму
                float angle = random(-45, 45); // Випадковий кут від -45 до 45 градусів
                float radians = angle * (PI / 180.0); // Конвертація в радіани

                // Поточна швидкість м'ячика
                float speed = sqrt(ballDirX * ballDirX + ballDirY * ballDirY);

                // Оновлення напрямів руху м'ячика з урахуванням випадкового кута
                ballDirX = speed * sin(radians);
                ballDirY = speed * cos(radians);

                lastBounceTime = currentMillis;  // Оновлення часу останнього відскоку
            }
        }
        // Відбивання м'ячика від нижньої платформи
        else if (ballY >= 61 - paddleHeight && ballX >= bottomPaddleX && ballX <= bottomPaddleX + paddleWidth) {
            if (currentMillis - lastBounceTime > bounceDelay && ballAbovePaddle) {
                // Розрахунок напряму до верхньої панелі
                float targetX = topPaddleX + paddleWidth / 2.0;
                float targetY = 0; // Верхній край екрана
                float dx = targetX - ballX;
                float dy = targetY - ballY;
                float length = sqrt(dx * dx + dy * dy);
                // Уникнення ділення на нуль
                if (length == 0) length = 1;
                // Встановлення нового напряму руху м'ячика
                float speed = sqrt(ballDirX * ballDirX + ballDirY * ballDirY); // Поточна швидкість м'ячика
                ballDirX = (dx / length) * speed;
                ballDirY = (dy / length) * speed;

                score++;  // Збільшення очок
                lastBounceTime = currentMillis;  // Оновлення часу останнього відскоку
                ballAbovePaddle = false;         // М'ячик тепер не над платформою
            }
        } else if (ballY < 61 - paddleHeight) {
            ballAbovePaddle = true; // Встановлюємо, що м'ячик над платформою, коли він вище
        }

        // Перевірка на програш (м'ячик пішов за нижній край)
        if (ballY > 64) {
            // Скидання положень
            ballX = 64;
            ballY = 32;
            ballDirY = 1;
            ballDirX = 0; // Додано для скидання горизонтальної швидкості
            score = 0; // Обнулення рахунку
            ballAbovePaddle = true; // Скидаємо стан
        }

        // Малювання елементів
        u8g2.clearBuffer();
        u8g2.drawBox((int)bottomPaddleX, 61, paddleWidth, paddleHeight);  // Нижня панель
        u8g2.drawBox(topPaddleX, 0, paddleWidth, paddleHeight);           // Верхня панель
        u8g2.drawDisc((int)ballX, (int)ballY, 2, U8G2_DRAW_ALL);          // М'ячик

        // Вивід рахунку на екран
        u8g2.setFont(u8g2_font_ncenB08_tr);  // Встановлення шрифту
        u8g2.setCursor(0, 10);
        u8g2.print("Score: ");
        u8g2.print(score);

        u8g2.sendBuffer();
    }

    // Перевірка на натискання кнопки для виходу з гри з обробкою "дребезгу"
    if (digitalRead(BUTTON_PIN) == LOW && currentMillis - lastButtonPressTime > buttonDebounceDelay) {
        lastButtonPressTime = currentMillis;  // Оновлення часу останнього натискання
        inSubMenu = false;
    }
}




void readJoystick() {
    int joystickX = analogRead(JOYSTICK_X_PIN);
    Serial.println(joystickX);
    int center = 1500;   // Оновіть це значення на основі ваших вимірювань
    int threshold = 50; // Налаштуйте поріг чутливості

    int deviation = joystickX - center;

    if (abs(deviation) > threshold) {
        if (deviation > 0) {
            // Рух вліво
            playerX -= 1;
        } else {
            // Рух вправо
            playerX += 1;
        }
    }

    // Обмежуємо позицію гравця в межах екрану
    playerX = constrain(playerX, 0, screenWidth - playerWidth);
}


void moveBullet() {
    bulletY -= 4;
    if (bulletY < 0) {
        bulletActive = false;
    }
}

void moveEnemies() {
    int activeEnemies = 0;  // Лічильник активних ворогів

    // Переміщення ворогів вліво або вправо
    for (int i = 0; i < maxEnemies; i++) {
        if (enemyActive[i]) {
            activeEnemies++;
            enemyX[i] += enemyDirection * 2;
            if (enemyX[i] <= 0 || enemyX[i] >= screenWidth - enemyWidth) {
                enemyDirection *= -1;  // Зміна напряму
                for (int j = 0; j < maxEnemies; j++) {
                    enemyY[j] += enemyHeight;
                }
                break;
            }

            // Перевірка на досягнення ворогами нижньої частини екрану
            if (enemyY[i] >= screenHeight - playerHeight) {
                gameOver = true;
                return;
            }
        }
    }

    // Якщо всі вороги знищені, перезапустити хвилю ворогів
    if (activeEnemies == 0) {
        resetEnemies();
    }
}

void checkCollisions() {
    if (bulletActive) {
        for (int i = 0; i < maxEnemies; i++) {
            if (enemyActive[i] &&
                bulletX + bulletWidth > enemyX[i] && bulletX < enemyX[i] + enemyWidth &&
                bulletY + bulletHeight > enemyY[i] && bulletY < enemyY[i] + enemyHeight) {
                
                enemyActive[i] = false;
                bulletActive = false;
                score += 10;
                break;
            }
        }
    }
}

void shootBullet() {
    if (!bulletActive) { // Якщо куля не активна, дозволяємо новий постріл
        bulletActive = true;
        bulletX = playerX + playerWidth / 2 - bulletWidth / 2; // Позиція кулі по X посередині гравця
        bulletY = screenHeight - playerHeight - bulletHeight; // Початкова позиція кулі по Y
    }
}

void showSpaceInvaders() {
    unsigned long currentTime = millis();

    // Перевірка на програш і автоматичний перезапуск гри з затримкою
    if (gameOver) {
        if (gameOverTime == 0) {
            gameOverTime = currentTime; // Задаємо час початку затримки
        }

        // Якщо минуло більше 1 секунди з моменту програшу, скидаємо гру
        if (currentTime - gameOverTime >= gameOverDelay) {
            resetGame();
            // Не потрібно встановлювати gameOver = false тут, оскільки це робиться в resetGame()
            // Після скидання гри дозволяємо виконання коду продовжитися
        } else {
            // Відображаємо повідомлення "Game Over"
            u8g2.clearBuffer();
            u8g2.setFont(u8g2_font_ncenB08_tr);
            u8g2.setCursor(20, 30);
            u8g2.print("Game Over");
            u8g2.setCursor(10, 50);
            u8g2.print("Score: ");
            u8g2.print(score);
            u8g2.sendBuffer();
            return; // Уникаємо подальшого виконання, поки не мине затримка
        }
    }

    // Решта коду гри
    readJoystick();
    checkCollisions();

    // Перевірка на натискання кнопки стрільби
    if (digitalRead(BUTTON_PIN) == LOW) {
        shootBullet();
    }

    // Оновлення кулі, якщо вона активна
    if (bulletActive && currentTime - lastBulletMoveTime > bulletMoveInterval) {
        lastBulletMoveTime = currentTime;
        moveBullet();
    }

    // Оновлення ворогів із затримкою
    if (currentTime - lastEnemyMoveTime > enemyMoveDelay) {
        lastEnemyMoveTime = currentTime;
        moveEnemies();
    }

    // Очищення буфера екрану перед оновленням
    u8g2.clearBuffer();

    // Малюємо гравця
    u8g2.drawBox(playerX, screenHeight - playerHeight, playerWidth, playerHeight);

    // Малюємо кулю
    if (bulletActive) {
        u8g2.drawBox(bulletX, bulletY, bulletWidth, bulletHeight);
    }

    // Малюємо ворогів
    for (int i = 0; i < maxEnemies; i++) {
        if (enemyActive[i]) {
            drawEnemy(enemyX[i], enemyY[i]);
        }
    }

    // Відображаємо рахунок
    u8g2.setCursor(0, 10);
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.print("Score: ");
    u8g2.print(score);

    // Надсилаємо все на дисплей
    u8g2.sendBuffer();

    // Логіка виходу з гри при тривалому натисканні кнопки
    static unsigned long buttonPressTime = 0;
    if (digitalRead(BUTTON_PIN) == LOW) {
        if (buttonPressTime == 0) {
            buttonPressTime = millis();
        } else if (millis() - buttonPressTime > 1000) {
            inSubMenu = false;
            buttonPressTime = 0;
            resetGame();
        }
    } else {
        buttonPressTime = 0;
    }
}


void drawEnemy(int x, int y) {
    u8g2.drawPixel(x + 2, y + 1);
    u8g2.drawPixel(x + 5, y + 1);
    u8g2.drawBox(x + 1, y + 2, 6, 1);
    u8g2.drawPixel(x, y + 3);
    u8g2.drawBox(x + 2, y + 3, 4, 1);
    u8g2.drawPixel(x + 7, y + 3);
}

void resetGame() {
    gameOver = false;
    bulletActive = false;
    bulletX = playerX + playerWidth / 2 - bulletWidth / 2;
    bulletY = screenHeight - playerHeight - bulletHeight;
    score = 0;
    playerX = screenWidth / 2 - playerWidth / 2;  // Центрування гравця
    enemyDirection = 1;
    lastEnemyMoveTime = millis();
    lastBulletMoveTime = millis();
    resetEnemies();
    gameOverTime = 0;
}



void resetEnemies() {
    int enemiesPerRow = 5; // Кількість ворогів у ряду
    int spacingX = (screenWidth - (enemiesPerRow * enemyWidth)) / (enemiesPerRow + 1); // Розрахунок проміжків між ворогами по X
    int spacingY = 5; // Проміжок між рядами ворогів по Y
    int rows = maxEnemies / enemiesPerRow;

    for (int i = 0; i < maxEnemies; i++) {
        int row = i / enemiesPerRow;
        int col = i % enemiesPerRow;
        enemyX[i] = spacingX + col * (enemyWidth + spacingX);
        enemyY[i] = spacingY + row * (enemyHeight + spacingY);
        enemyActive[i] = true;
    }
}




void showPressure() {
    if (isBMPAvailable) {
        sensors_event_t event;
        if (bmp.getEvent(&event)) {
            float pressure = event.pressure;

            u8g2.clearBuffer();
            u8g2.enableUTF8Print();
            u8g2.setFont(u8g2_font_cu12_t_cyrillic);
            u8g2.setCursor(0, 15);
            u8g2.printf("Тиск: %.2f hPa", pressure);
            u8g2.sendBuffer();
        } else {
            u8g2.clearBuffer();
            u8g2.enableUTF8Print();
            u8g2.setFont(u8g2_font_cu12_t_cyrillic);
            u8g2.setCursor(0, 15);
            u8g2.print("Помилка датчика");
            u8g2.sendBuffer();
            Serial.println("Не вдалося отримати дані з датчика тиску.");
        }
    } else {
        u8g2.clearBuffer();
        u8g2.enableUTF8Print();
        u8g2.setFont(u8g2_font_cu12_t_cyrillic);
        u8g2.setCursor(0, 15);
        u8g2.print("Тиск: недоступний");
        u8g2.sendBuffer();
    }

    if (handleReturnButton()) {
        inSubMenu = false;
        Serial.println("Button pressed. Returning to main menu.");
    }
}




void updateButtonState() {
    int buttonState = digitalRead(BUTTON_PIN);
    static bool buttonWasReleased = true;

    if (ignoreButtonUntilRelease) {
        if (buttonState == HIGH) {
            // Кнопка відпущена, можна знову реагувати на натискання
            ignoreButtonUntilRelease = false;
            buttonWasReleased = true;
        }
        // Якщо кнопка все ще натиснута, нічого не робимо
        return;
    }

    if (buttonState == LOW && buttonWasReleased) {
        // Кнопка щойно натиснута
        buttonPressed = true;
        buttonPressStartTime = millis();
        buttonWasReleased = false;
    } else if (buttonState == LOW && buttonPressed) {
        // Кнопка тримається натиснутою
        if (!buttonLongPress && millis() - buttonPressStartTime > longPressDuration) {
            buttonLongPress = true;
            handleLongPress();
            ignoreButtonUntilRelease = true; // Ігноруємо подальші натискання до відпускання кнопки
        }
    } else if (buttonState == HIGH && buttonPressed) {
        // Кнопка щойно відпущена
        if (!buttonLongPress) {
            handleShortPress();
        }
        // Скидаємо стан кнопки
        buttonPressed = false;
        buttonLongPress = false;
        buttonPressStartTime = 0;
        buttonWasReleased = true;
    }
}



// Обробка короткого натискання
void handleShortPress() {
    if (!inSubMenu && !screensaverMode) {
        inSubMenu = true;
        Serial.print("Коротке натискання. Вхід у підменю: ");
        Serial.println(menuOption);
    } else if (inSubMenu) {
        handleMenuOption(menuOption); // Виклик функції обробки меню
    } else if (screensaverMode) {
        screensaverMode = false; // Вихід із заставки
        Serial.println("Вихід із режиму заставки");
    }
}

void handleLongPress() {
    if (inSubMenu) {
        // Довге натискання для виходу з підменю
        inSubMenu = false;
        timerRunning = false;
        Serial.println("Довге натискання. Вихід з підменю");
        ignoreButtonUntilRelease = true; // Додаємо цю лінію
    } else {
        // Довге натискання в головному меню (можна додати дію)
        Serial.println("Довге натискання в головному меню");
        ignoreButtonUntilRelease = true; // Додаємо цю лінію
    }
}


void handleReturnButtonPress() {
    unsigned long currentMillis = millis();
    int buttonState = digitalRead(BUTTON_PIN);

    // Якщо кнопка натиснута
    if (buttonState == LOW && !buttonPressed && !exitInProgress && !blockInputAfterLongPress) {
        buttonPressed = true;  // Фіксуємо натискання кнопки
        buttonPressStartTime = currentMillis;
    }

    // Якщо кнопка відпущена
    if (buttonState == HIGH) {
        blockInputAfterLongPress = false;  // Розблокування після відпускання кнопки
        if (buttonPressed) {
            buttonPressed = false;  // Скидаємо стан кнопки
            if (!exitInProgress) {
                // Виконуємо дію тільки після завершення натискання
                if (inSubMenu) {
                    Serial.println("Вихід з підменю");
                    inSubMenu = false;
                    exitInProgress = true;  // Фіксуємо процес виходу
                    lastActionTime = currentMillis;
                }
            }
        }
    }

    // Перевірка debounce
    if (exitInProgress && (currentMillis - lastActionTime > debounceDelay)) {
        exitInProgress = false;  // Скидаємо блокування
        blockInputAfterLongPress = true; // Блокуємо повторний вхід до відпускання кнопки
    }
}

// Обробка натискання в таймері
void handleTimerButtonPress() {
    if (selectedButton == 0 && !timerRunning) {
        // Якщо вибрано "Старт" і таймер не працює
        timerRunning = true;
        countdownTimeInMillis = (setMinutes * 60000) + (setSeconds * 1000); // Встановлюємо час
        Serial.println("Таймер запущено");
    } else if (selectedButton == 1) {
        // Якщо вибрано "Скинути"
        resetTimer(); // Скидаємо таймер
    }
}

bool handleReturnButton() {
    int reading = digitalRead(BUTTON_PIN);
    if (reading != lastButtonState) {
        lastButtonDebounceTime = millis();
    }

    if (reading != buttonState) {
        buttonState = reading;
        if (buttonState == LOW) {
            lastButtonState = reading;
            return true;
        }
    }
    lastButtonState = reading;
    return false;
}


bool checkExitButton() {
    static unsigned long buttonPressStart = 0;

    if (digitalRead(BUTTON_PIN) == LOW) { // Кнопка натиснута
        if (buttonPressStart == 0) {
            buttonPressStart = millis(); // Початок тривалого натискання
        } else if (millis() - buttonPressStart > 1000) { // Тривале натискання > 1 секунди
            return true; // Повертаємо сигнал про вихід
        }
    } else {
        buttonPressStart = 0; // Скидаємо таймер, якщо кнопка відпущена
    }

    return false; // Кнопка не натиснута або натискання коротке
}


void showCatapultGame() {
    gameEnded = false;

    while (!gameEnded) {
        // Перевірка на вихід
        if (checkExitButton()) {
            inSubMenu = false;    // Повернення в меню
            gameEnded = true;     // Завершуємо цикл гри
            resetCatapultGame();  // Скидаємо параметри гри

            // Очікуємо відпускання кнопки перед поверненням у меню
            while (digitalRead(BUTTON_PIN) == LOW) {
                delay(10); // Невелика затримка для стабільності
            }
            return;
        }

        u8g2.clearBuffer();

        // Основна логіка гри (стрільба, траєкторії, оновлення)
        if (playerTurn && !catapultBulletActive) {
            playerAngle = map(analogRead(JOYSTICK_X_PIN), 0, 4095, 0, 90);

            // Перевірка натискання кнопки для пострілу
            if (digitalRead(BUTTON_PIN) == LOW) {
                shoot(playerAngle, true); // Гравець стріляє
                playerTurn = false;
                catapultBulletActive = true;
                bulletFromPlayer = true;
                lastShotTime = millis(); // Фіксуємо час пострілу
            }
        }

        // Логіка для бота
        if (!playerTurn && !catapultBulletActive) {
            if (!botAngleSet) {
                botAngle = calculateBotAngle();
                botAngleSet = true;
                lastShotTime = millis();
            }

            // Малювання траєкторії або стрільба
            if (millis() - lastShotTime < botDelay) {
                drawTrajectory(botAngle, false);
            } else {
                shoot(botAngle, false);
                botAngleSet = false;
                playerTurn = true;
                catapultBulletActive = true;
                bulletFromPlayer = false;
            }
        }

        // Оновлення гри
        if (catapultBulletActive) {
            updateCatapultBullet();
            checkCatapultCollisions();
        }

        // Перевірка завершення гри
        if (playerHP <= 0 || botHP <= 0) {
            endCatapultGame();
        }

        drawCatapultGame();
        u8g2.sendBuffer();
    }

    resetCatapultGame();
}







void drawTrajectory(int angle, bool isPlayer) {
    float speed = 3.0; // Початкова швидкість
    float posX = isPlayer ? 20 : 118; // Початкова позиція
    float posY = 50;
    float velX = speed * cos(radians(angle));
    float velY = -speed * sin(radians(angle));

    // Якщо бот, то швидкість по X повинна бути негативною
    if (!isPlayer) {
        velX = -velX;
    }

    int previousX = posX;
    int previousY = posY;

    // Ліміт кроків для малювання траєкторії
    int maxSteps = 50;

    for (int i = 0; i < maxSteps; i++) {
        posX += velX;
        posY += velY;
        velY += GRAVITY;

        // Вихід за межі екрану
        if (posX < 0 || posX > 128 || posY > 64) {
            break;
        }

        u8g2.drawLine(previousX, previousY, (int)posX, (int)posY);
        previousX = posX;
        previousY = posY;

        // Дозволяємо обробляти інші задачі
        yield();
    }
}


void updateTrajectory() {
    if (!drawingTrajectory) return;

    posX += velX;
    posY += velY;
    velY += GRAVITY;

    if (posX < 0 || posX > 128 || posY > 64 || trajectoryStep >= 50) {
        drawingTrajectory = false; // Завершуємо малювання
        return;
    }

    u8g2.drawLine(previousX, previousY, (int)posX, (int)posY);
    previousX = posX;
    previousY = posY;

    trajectoryStep++;
    yield(); // Дозволяємо обробляти інші задачі
}

void drawCatapultGame() {
    // Відображення HP
    u8g2.setCursor(10, 10);
    u8g2.print("P: ");
    u8g2.print(playerHP);

    int botHPCursorX = (botHP >= 100) ? 88 : (botHP >= 10 ? 94 : 100);
    u8g2.setCursor(botHPCursorX, 10);
    u8g2.print("B: ");
    u8g2.print(botHP);

    // Катапульти
    u8g2.drawBox(10, 50, 10, 5); // Гравець
    u8g2.drawBox(108, 50, 10, 5); // Бот

    // Траєкторія гравця
    if (playerTurn && !catapultBulletActive) {
        drawTrajectory(playerAngle, true);
    }

    // Снаряд
    if (catapultBulletActive) {
        u8g2.drawDisc(catapultBulletX, catapultBulletY, 2);
    }
}


void startDrawingTrajectory(int angle, bool isPlayer) {
    posX = isPlayer ? 20 : 118;
    posY = 50;
    velX = 3.0 * cos(radians(angle));
    velY = -3.0 * sin(radians(angle));

    if (!isPlayer) {
        velX = -velX;
    }

    previousX = posX;
    previousY = posY;
    trajectoryStep = 0;
    drawingTrajectory = true;
    isPlayerTrajectory = isPlayer;
}


void shoot(int angle, bool isPlayer) {
    // Ініціалізація снаряда з початкової позиції
    if (isPlayer) {
        catapultBulletX = 20;
        catapultBulletY = 50;
        catapultBulletSpeedX = 3.0 * cos(radians(angle));  // Вправо
    } else {
        catapultBulletX = 108;
        catapultBulletY = 50;
        catapultBulletSpeedX = -3.0 * cos(radians(angle)); // Вліво
    }

    catapultBulletSpeedY = -3.0 * sin(radians(angle)); // Вгору (негативне значення)
}




void updateCatapultBullet() {
  // Оновлюємо положення снаряда з урахуванням гравітації
  catapultBulletX += catapultBulletSpeedX;
  catapultBulletY += catapultBulletSpeedY;
  catapultBulletSpeedY += GRAVITY; // Ефект гравітації

  // Якщо снаряд виходить за межі екрану
  if (catapultBulletX < 0 || catapultBulletX > 128 || catapultBulletY > 64) {
    catapultBulletActive = false;
  }
}

void checkCatapultCollisions() {
  // Перевірка на попадання у катапульту
  if (bulletFromPlayer && catapultBulletX >= 108 && catapultBulletX <= 118 && catapultBulletY >= 50 && catapultBulletY <= 55) {
    botHP--;
    catapultBulletActive = false;
  } else if (!bulletFromPlayer && catapultBulletX >= 10 && catapultBulletX <= 20 && catapultBulletY >= 50 && catapultBulletY <= 55) {
    playerHP--;
    catapultBulletActive = false;
  }
}



void endCatapultGame() {
  u8g2.clearBuffer();
  u8g2.setCursor(30, 30);
  if (playerHP <= 0) {
    u8g2.print("Bot Wins!");
  } else {
    u8g2.print("Player Wins!");
  }
  u8g2.sendBuffer();
  endGameDisplayTime = millis(); // Записуємо час кінця гри
  gameEnded = true;
}

void resetCatapultGame() {
    playerHP = 5;
    botHP = 5;
    playerTurn = true;
    gameEnded = false;
}


void showScreensaver() {
    // Зупиняємо Mesh-сітку та підключаємося до Wi-Fi
    mesh.stop();
    connectToWiFi();

    // Ініціалізація NTP-клієнта
    timeClient.begin();
    timeClient.update();
    unsigned long epochTime = timeClient.getEpochTime();
    long utcOffset = getCurrentUtcOffset(epochTime);
    timeClient.setTimeOffset(utcOffset);

    // Таймери
    unsigned long previousTimeMillis = 0; // Таймер для оновлення часу
    const unsigned long timeInterval = 1000; // Інтервал оновлення часу (1 секунда)
    unsigned long previousBallMillis = 0; // Таймер для оновлення м'ячика
    const unsigned long ballInterval = 30; // Інтервал оновлення м'ячика (30 мс)

    // Параметри м'ячика
    float ballX = 64, ballY = 32;           // Початкова позиція м'ячика
    float ballSpeedX = 1.5, ballSpeedY = 1.2; // Швидкість м'ячика
    const int ballRadius = 3;              // Радіус м'ячика

    bool screensaverActive = true;
    bool manualControl = false; // Режим керування джойстиком

    while (screensaverActive) {
        unsigned long currentMillis = millis();

        // Оновлення часу через заданий інтервал
        if (currentMillis - previousTimeMillis >= timeInterval) {
            previousTimeMillis = currentMillis;
            timeClient.update();
        }

        // Зчитування джойстика
        int joystickX = analogRead(JOYSTICK_X_PIN);
        int joystickY = analogRead(JOYSTICK_Y_PIN);

        // Перевірка, чи джойстик у нейтральній позиції
        if (joystickX > 2000 && joystickX < 3000 && joystickY > 2000 && joystickY < 3000) {
            manualControl = false; // Повертаємо керування м'ячиком у заставку
        } else {
            manualControl = true; // Активуємо ручне керування м'ячиком
        }

        // Оновлення позиції м'ячика через заданий інтервал
        if (currentMillis - previousBallMillis >= ballInterval) {
            previousBallMillis = currentMillis;

            if (manualControl) {
                // Керування м'ячиком за допомогою джойстика
                if (joystickX > 3000 && ballX - ballRadius > 0) {
                    ballX -= 2; // Рух ліворуч
                } else if (joystickX < 2000 && ballX + ballRadius < 128) {
                    ballX += 2; // Рух праворуч
                }

                if (joystickY < 2000 && ballY - ballRadius > 0) {
                    ballY -= 2; // Рух вгору
                } else if (joystickY > 3000 && ballY + ballRadius < 64) {
                    ballY += 2; // Рух вниз
                }
            } else {
                // Автоматичний рух м'ячика
                ballX += ballSpeedX;
                ballY += ballSpeedY;

                // Відбивання м'ячика від стінок
                if (ballX - ballRadius <= 0 || ballX + ballRadius >= 128) {
                    ballSpeedX *= -1;
                }
                if (ballY - ballRadius <= 0 || ballY + ballRadius >= 64) {
                    ballSpeedY *= -1;
                }
            }

            // Малюємо м'ячик і час на екрані
            u8g2.clearBuffer();

            // Малюємо м'ячик
            u8g2.drawDisc(ballX, ballY, ballRadius);

            // Малюємо час
            String currentTime = timeClient.getFormattedTime();
            u8g2.setFont(u8g2_font_courB18_tr);
            int textWidth = u8g2.getStrWidth(currentTime.c_str());
            int x = (128 - textWidth) / 2;
            int y = 40;
            u8g2.setCursor(x, y);
            u8g2.print(currentTime);

            // Відправка даних на екран
            u8g2.sendBuffer();
        }

        // Вихід із заставки при натисканні кнопки
        if (digitalRead(BUTTON_PIN) == LOW) {
            screensaverActive = false;
            inSubMenu = false; // Повернення в меню
        }
    }

    // Відключаємося від Wi-Fi та відновлюємо Mesh-сітку
    WiFi.disconnect();
    mesh.init(MESH_PREFIX, MESH_PASSWORD, MESH_PORT);
    timeClient.end();
}