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

unsigned long previousMillis = 0;
const long interval = 10000;

unsigned long lastWeatherUpdateTime = 0;
const unsigned long weatherUpdateInterval = 500; 

unsigned long lastPressureUpdateTime = 0;  // Час останнього оновлення тиску
const unsigned long pressureUpdateInterval = 60000;  // 


WiFiUDP ntpUDP;
const long utcOffsetInSeconds = 10800;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);

void setup() {
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    Serial.begin(115200);

    u8g2.begin();
    u8g2.clearBuffer();
    u8g2.sendBuffer();

    dht.begin();
    if (!bmp.begin()) {
        Serial.print("Не вдалося знайти датчик BMP085.");
        while (1);
    }

    // Зчитуємо початкові дані з сенсорів
    sensors_event_t event;
    bmp.getEvent(&event);
    lastSentPressure = event.pressure;
    lastSentTemperature = dht.readTemperature();
    lastSentHumidity = dht.readHumidity();

    mesh.onNewConnection([](size_t nodeId) {
        Serial.printf("New connection, nodeId=%u\n", nodeId);
        
        // Відправляємо останні дані з сенсорів новому вузлу
        if (lastSentTemperature != -999.0) {
            char tempMsg[20];
            sprintf(tempMsg, "Temp: %.2f C", lastSentTemperature);
            mesh.sendSingle(nodeId, tempMsg);
        }

        if (lastSentHumidity != -999.0) {
            char humMsg[20];
            sprintf(humMsg, "Humidity: %.2f %%", lastSentHumidity);
            mesh.sendSingle(nodeId, humMsg);
        }

        if (lastSentPressure != -999.0) {
            char pressMsg[30];
            sprintf(pressMsg, "Pressure: %.2f hPa", lastSentPressure);
            mesh.sendSingle(nodeId, pressMsg);
        }
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
        // Навіть у режимі заставки ми хочемо передавати дані в Mesh
        readAndSendData();  // Надсилаємо дані з датчиків
        return;
    }

    // Перевірка на вітальну заставку
    if (isWelcomeScreenVisible) {
        if (millis() - welcomeScreenStartTime > 3000) {
            isWelcomeScreenVisible = false;
        } else {
            // Навіть під час відображення вітальної заставки надсилаємо дані
            readAndSendData();
            return;
        }
    }

    // Зчитування стану джойстика
    u8g2.clearBuffer();
    int joystickY = analogRead(JOYSTICK_Y_PIN);

    // Якщо ми не в підменю, то обробляємо джойстик для вибору пунктів меню
    if (!inSubMenu) {
        if (millis() - lastJoystickDebounceTime > joystickDebounceDelay) {
            if (joystickY < 1000) {
                menuOption--;
                if (menuOption < 0) {
                    menuOption = 2;  // Повертаємося до останнього пункту, якщо перевищено межі
                }
                lastJoystickDebounceTime = millis();
                Serial.print("Joystick moved up. New menuOption: ");
                Serial.println(menuOption);
            } else if (joystickY > 3000) {
                menuOption++;
                if (menuOption > 2) {
                    menuOption = 0;  // Повертаємося до першого пункту, якщо перевищено межі
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
            showTemperatureAndHumidity();
        } else if (menuOption == 1) {
            showWeather();
        } else if (menuOption == 2) {
            showStopwatch();
        }
    }

    // Оновлення даних з датчиків та відправка через Mesh з певним інтервалом
    if (millis() - lastSerialUpdateTime >= serialUpdateInterval) {
        readAndSendData();  // Функція, яка збирає дані з датчиків і відправляє їх у Mesh
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
    sensors_event_t event;
    bmp.getEvent(&event);
    float pressure = event.pressure;

    u8g2.clearBuffer();
    u8g2.enableUTF8Print();
    u8g2.setFont(u8g2_font_cu12_t_cyrillic);
    u8g2.setCursor(0, 15);
    u8g2.printf("Кімната: %.2f C", temperature);
    u8g2.setCursor(0, 30);
    u8g2.printf("Волога: %.2f %%", humidity);
    u8g2.setCursor(0, 45);
    u8g2.printf("Тиск: %.2f hPa", pressure);
    u8g2.sendBuffer();

    if (handleReturnButton()) {
        inSubMenu = false;
        Serial.println("Button pressed. Returning to main menu.");
    }
}

void sendPressureData(float pressure) {
    if (pressure != -999) {  // Переконуємося, що тиск отримано коректно
        // Формуємо і відправляємо повідомлення про тиск через Mesh
        char msg[30];
        sprintf(msg, "Pressure: %.2f hPa", pressure);
        mesh.sendBroadcast(msg);
        Serial.printf("Sent pressure to mesh: %.2f hPa\n", pressure);
    }
}

void readAndSendData() {
    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();
    sensors_event_t event;
    bmp.getEvent(&event);
    float pressure = event.pressure;

    // Відправляємо температуру
    char tempMsg[20];
    sprintf(tempMsg, "Temp: %.2f C", temperature);
    mesh.sendBroadcast(tempMsg);
    Serial.printf("Sent temperature: %.2f C\n", temperature);
    lastSentTemperature = temperature;

    // Відправляємо вологість
    char humMsg[20];
    sprintf(humMsg, "Humidity: %.2f %%", humidity);
    mesh.sendBroadcast(humMsg);
    Serial.printf("Sent humidity: %.2f %%\n", humidity);
    lastSentHumidity = humidity;

    // Відправляємо тиск
    char pressMsg[30];
    sprintf(pressMsg, "Pressure: %.2f hPa", pressure);
    mesh.sendBroadcast(pressMsg);
    Serial.printf("Sent pressure: %.2f hPa\n", pressure);
    lastSentPressure = pressure;
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

    for (int i = 0; i < 3; i++) {
        int y = 20 + i * 20;
        u8g2.setCursor(10, y);

        int textWidth = u8g2.getStrWidth((i == 0) ? "Кімната" : (i == 1) ? "Вулиця" : "Секундомір");

        if (menuOption == i) {
            u8g2.drawRBox(10 - 4, y - 15, textWidth + 8, 15, 4);
            u8g2.print("> ");
        } else {
            u8g2.print("  ");
        }

        if (i == 0) u8g2.print("Кімната");
        else if (i == 1) u8g2.print("Вулиця");
        else if (i == 2) u8g2.print("Секундомір");
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
    timeClient.begin();
    updateWeather();

    while (inSubMenu) {
        timeClient.update();

        int currentHour = timeClient.getHours();
        int currentMinute = timeClient.getMinutes();
        int currentSecond = timeClient.getSeconds();

        unsigned long epochTime = timeClient.getEpochTime();
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