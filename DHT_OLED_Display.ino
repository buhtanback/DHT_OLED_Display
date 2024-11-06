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

DHT dht(DHTPIN, DHTTYPE);
Adafruit_BMP085_Unified bmp = Adafruit_BMP085_Unified(10085);

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, OLED_RESET);

int menuOption = 0;
int buttonState = 0;
int lastButtonState = 0;
unsigned long lastButtonDebounceTime = 0;
unsigned long lastJoystickDebounceTime = 0;
unsigned long buttonDebounceDelay = 50;
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

long utcOffsetInSecondsSummer = 7200; // Літній час
long utcOffsetInSecondsWinter = 3600; // Зимовий час

bool isBMPAvailable = true;

static unsigned long lastButtonPressTime = 0;

long getCurrentUtcOffset(unsigned long epochTime) {
    struct tm * timeInfo = gmtime((time_t *)&epochTime);

    int month = timeInfo->tm_mon + 1; // Місяці від 0 до 11
    int day = timeInfo->tm_mday;

    if ((month > 3 && month < 10) || (month == 3 && day >= 25) || (month == 10 && day < 25)) {
        return 7200; // Літній час (UTC+2)
    } else {
        return 3600; // Зимовий час (UTC+1)
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


int totalMenuOptions = 6; // Загальна кількість пунктів меню
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




void setup() {
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    Serial.begin(115200);

    u8g2.begin();
    u8g2.clearBuffer();
    u8g2.sendBuffer();

    dht.begin();
    if (!bmp.begin()) {
        Serial.print("Не вдалося знайти датчик BMP085.");
        isBMPAvailable = false; // Встановлюємо змінну в false, якщо датчик не знайдено
    }

    // Інші ініціалізації залишаємо без змін
    pinMode(JOYSTICK_X_PIN, INPUT);
    pinMode(JOYSTICK_Y_PIN, INPUT);
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



void loop() {
    // Оновлюємо Mesh в кожному циклі, щоб підтримувати зв'язок у сітці
    mesh.update();
  



    // Обробка натискань кнопок
    handleButtonPress();



    // Перевірка на режим заставки
    if (screensaverMode) {
        showImage();
        readAndSendData();  // Навіть у режимі заставки надсилаємо дані
        return;
    }

    // Перевірка на вітальну заставку
    if (isWelcomeScreenVisible) {
        if (millis() - welcomeScreenStartTime > 3000) {
            isWelcomeScreenVisible = false;
        } else {
            readAndSendData();
            return;
        }
    }

    // Зчитування стану джойстика
    u8g2.clearBuffer();
    int joystickY = analogRead(JOYSTICK_Y_PIN);

    // Якщо ми не в підменю, обробляємо джойстик для вибору пунктів меню
    if (!inSubMenu) {
        if (millis() - lastJoystickDebounceTime > joystickDebounceDelay) {
            if (joystickY < 1000) {
                menuOption--;
                if (menuOption < 0) {
                    menuOption = totalMenuOptions - 1;  // Перехід до останнього пункту
                }
                lastJoystickDebounceTime = millis();
                Serial.print("Joystick moved up. New menuOption: ");
                Serial.println(menuOption);
            } else if (joystickY > 3000) {
                menuOption++;
                if (menuOption >= totalMenuOptions) {
                    menuOption = 0;  // Перехід до першого пункту
                }
                lastJoystickDebounceTime = millis();
                Serial.print("Joystick moved down. New menuOption: ");
                Serial.println(menuOption);
            }
        }

        // Відображаємо меню
        showMenu();
    } else {
        // В залежності від обраного пункту меню викликаємо відповідну функцію
        if (menuOption == 0) {
            showTemperatureAndHumidity();  // Перший пункт меню
        } else if (menuOption == 1) {
            showWeather();  // Другий пункт меню
        } else if (menuOption == 2) {
            showStopwatch();  // Третій пункт меню
        } else if (menuOption == 3) {
            showPressure();  // Четвертий пункт меню (Тиск)
        } else if (menuOption == 4) {
            showTimer();  // П'ятий пункт меню (Таймер)
        } else if (menuOption == 5) {
            showGame();  // Шостий пункт меню (Гра)
        }
    }

    // Оновлення даних з датчиків та відправка через Mesh з певним інтервалом
    if (millis() - lastSerialUpdateTime >= serialUpdateInterval) {
        readAndSendData();
        lastSerialUpdateTime = millis();
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
        } else if (buttonPressCount == 2 || buttonPressCount == 3) {
            if (!inSubMenu) {
                screensaverMode = !screensaverMode;
                Serial.println("Button pressed 2 or 3 times. Toggling screensaver mode.");
                if (screensaverMode) {
                    isWelcomeScreenVisible = true;
                } else {
                    isWelcomeScreenVisible = false;
                }
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
    u8g2.drawBitmap(0, 0, 16, 128, image);
    u8g2.sendBuffer();
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
                    (optionIndex == 5) ? "Гра" : "";

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
                isSelectingTime = true;  // Повертаємося до вибору часу
            }
            lastJoystickMove = currentMillis;
        } else if (joystickX > 3000) {  // Праворуч
            if (isSelectingTime) {
                isSelectingTime = false;  // Переходимо до вибору кнопок
                selectedButton = 0;       // Починаємо з кнопки "Старт"
            } else {
                selectedButton = (selectedButton == 0) ? 1 : 0;  // Перемикаємо між "Старт" і "Скинути"
            }
            lastJoystickMove = currentMillis;
        }
    }

    // Обробка вертикального переміщення джойстика
    if (currentMillis - lastJoystickMove > debounceDelay) {
        if (joystickY < 1000) {  // Вгору
            if (isSelectingTime) {
                // Збільшуємо значення хвилин або секунд
                if (selectedDigit == 0) {
                    setMinutes = (setMinutes + 1) % 100;  // Максимум 99 хвилин
                } else {
                    setSeconds = (setSeconds + 1) % 60;   // Максимум 59 секунд
                }
            } else {
                // Перемикаємо вибір кнопки
                selectedButton = 0;  // "Старт"
            }
            lastJoystickMove = currentMillis;
        } else if (joystickY > 3000) {  // Вниз
            if (isSelectingTime) {
                // Зменшуємо значення хвилин або секунд
                if (selectedDigit == 0) {
                    setMinutes = (setMinutes == 0) ? 99 : setMinutes - 1;
                } else {
                    setSeconds = (setSeconds == 0) ? 59 : setSeconds - 1;
                }
            } else {
                // Перемикаємо вибір кнопки
                selectedButton = 1;  // "Скинути"
            }
            lastJoystickMove = currentMillis;
        }
    }

    // Відображаємо інтерфейс на екрані
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
        // Якщо таймер працює, відображаємо лише поточний стан відліку
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
                    previousMillis = currentMillis;  // Ініціалізуємо таймер
                    Serial.println("Таймер запущено");
                } else if (selectedButton == 1) {
                    // Скидаємо таймер
                    resetTimer();
                    Serial.println("Таймер скинуто");
                }
            }
        } else {
            // Якщо таймер вже працює, натискання кнопки зупиняє його
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
        if (buttonPressDuration == 0) {  // Початок натискання
            buttonPressDuration = currentMillis;
        }

        if (currentMillis - buttonPressDuration > 1000) {  // Якщо кнопка утримується більше 1 секунди
            inSubMenu = false;
            timerRunning = false;
            Serial.println("Вихід з підменю");
            buttonPressDuration = 0;  // Скидаємо час натискання
        }
    } else {
        buttonPressDuration = 0;  // Скидаємо час утримання
    }

    // Оновлюємо Mesh та інші функції
    mesh.update();
    readAndSendData();
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
        if (ballY <= 0) {
            ballDirY *= -1;
        } else if (ballY >= 61 - paddleHeight && ballX >= bottomPaddleX && ballX <= bottomPaddleX + paddleWidth) {
            // Відбивання м'ячика від нижньої панелі
            if (currentMillis - lastBounceTime > bounceDelay && ballAbovePaddle) {
                ballDirY *= -1;
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


void handleTimerButtonPress() {
    static unsigned long lastButtonPressTime = 0;
    const unsigned long debounceDelay = 200;  // 200 мс для запобігання повторним натисканням
    unsigned long currentMillis = millis();

    if (digitalRead(BUTTON_PIN) == LOW && currentMillis - lastButtonPressTime > debounceDelay) {
        if (selectedButton == 0 && !timerRunning) {
            // Якщо вибрано "Старт" і таймер не працює
            timerRunning = true;
            countdownTimeInMillis = (setMinutes * 60000) + (setSeconds * 1000);  // Встановлюємо час
            Serial.println("Таймер запущено");
        } else if (selectedButton == 1) {
            // Якщо вибрано "Скинути"
            resetTimer();  // Скидаємо таймер
        }
        lastButtonPressTime = currentMillis;  // Оновлюємо час останнього натискання
    }
}

void handleReturnButtonPress() {
    static unsigned long buttonPressTime = 0;
    unsigned long currentMillis = millis();
    int buttonState = digitalRead(BUTTON_PIN);

    if (buttonState == LOW) {
        if (buttonPressTime == 0) {
            buttonPressTime = currentMillis;  // Початок натискання
        }

        if (currentMillis - buttonPressTime > 1000) {  // Якщо натискання триває більше 1 секунди
            inSubMenu = false;
            timerRunning = false;
            Serial.println("Вихід з меню");
            buttonPressTime = 0;  // Скидаємо час натискання
        }
    } else {
        buttonPressTime = 0;  // Скидаємо час натискання, якщо кнопка відпущена
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