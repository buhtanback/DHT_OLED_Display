#include "Config.h"
#include "painlessMesh.h"
#include <DHT.h>
#include <U8g2lib.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP085_U.h>



#define MESH_PREFIX "buhtan"
#define MESH_PASSWORD "buhtan123"
#define MESH_PORT 5555

painlessMesh mesh;



Adafruit_BMP085_Unified bmp = Adafruit_BMP085_Unified(10085);

#define DHTPIN 4
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

#define TRIGGER_PIN 14
#define ECHO_PIN 27

#define skeletor_width 64
#define skeletor_height 64

#define OLED_RESET -1
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, OLED_RESET);

unsigned long welcomeScreenStartTime = 0;
bool isWelcomeScreenVisible = false;

unsigned long lastUpdateTime = 0; // Time of the last update
const long updateInterval = 2000; // Update interval (2000 milliseconds = 2 seconds)

unsigned long lastTempSendTime = 0; // Time of the last temperature sending
unsigned long lastHumiSendTime = 0; // Time of the last humidity sending
const long sendInterval = 500; // Minimum interval between sendings (500 milliseconds)

bool isStopwatchActive = false; // Logical variable for the state of the stopwatch
unsigned long stopwatchStartTime = 0; // Stopwatch start time
unsigned long stopwatchElapsedTime = 0; // Elapsed time of the stopwatch
bool lastSensorActive = false; // Last state of the sensor

unsigned long lastSerialUpdateTime = 0; // Last update time for the serial monitor (stopwatch)
const long serialUpdateInterval = 1000; // Interval for updating the serial monitor (1000 milliseconds = 1 second)

unsigned long lastAnimationTime = 0; // Last update time for the animation

String currentTime = "Loading...";
String currentDate = "Loading..."; // Змінна для зберігання дати
unsigned long lastTimeUpdate = 0;
const long timeUpdateInterval = 60000; // 60 секунд
unsigned long lastMillis = 0;
int currentSecond = 0;
int currentMinute = 0;
int currentHour = 0;

int lastPrintedSecond = -1;

float lastTemperature = 0.0;
float lastHumidity = 0.0;

String weatherDescription;
float weatherTemp;

bool isWelcomeScreenShown = false; // Додана змінна для відстеження стану привітання

unsigned long lastDistanceMeasureTime = 0; // Last time distance was measured
const unsigned long distanceMeasureInterval = 500; // Interval between distance measurements (500 milliseconds)

unsigned long lastWeatherUpdate = 0; // Time of the last weather update

// Змінні для обробки серійного вводу
String lastSerialInput = "";
unsigned long lastSerialInputTime = 0;
const long serialDisplayDuration = 5000; // 5 секунд

// Змінні для таймера
bool isTimerActive = false;
unsigned long timerStartTime = 0;
unsigned long timerDuration = 0;

// Прототипи функцій
void connectToWiFi();
void updateTime();
void updateWeather();
void showWeather();
void showTemperatureAndHumidity();
void showStopwatch();
void showImage();
void sendTemperatureAndHumidityData(float temperature, float humidity);
float measureDistance();
void receivedCallback(uint32_t from, String &msg);
void newConnectionCallback(uint32_t nodeId);
void handleSerialInput();
void checkSerialDisplayTimeout();
void showTimer();
void startTimer(int durationInSeconds);
void stopTimer();


void connectToWiFi() {
    Serial.println("Attempting to connect to WiFi...");
    WiFi.begin(ssid, password);
    int attempt = 0;
    while (WiFi.status() != WL_CONNECTED && attempt < 30) { // Максимум 30 спроб
        delay(1000);
        Serial.print(".");
        attempt++;
    }
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Connected to WiFi");
    } else {
        Serial.println("Failed to connect to WiFi");
    }
}

void updateTime() {
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Attempting to update time...");
        HTTPClient http;
        http.begin("http://worldtimeapi.org/api/timezone/Europe/Kiev");
        int httpCode = http.GET();

        if (httpCode > 0) {
            String payload = http.getString();
            Serial.println("Payload: " + payload);  // Додано для налагодження
            DynamicJsonDocument doc(1024);
            deserializeJson(doc, payload);
            String dateTime = doc["datetime"];
            currentDate = dateTime.substring(2, 4) + "-" + dateTime.substring(5, 7).toInt() + "-" + dateTime.substring(8, 10); // Витягнути дату в потрібному форматі
            currentTime = dateTime.substring(11, 19); // Витягнути час з datetime
            currentHour = currentTime.substring(0, 2).toInt();
            currentMinute = currentTime.substring(3, 5).toInt();
            currentSecond = currentTime.substring(6, 8).toInt();
            lastMillis = millis();
            Serial.println("Time updated: " + currentTime);
            Serial.println("Date updated: " + currentDate);
        } else {
            Serial.println("HTTP GET failed: " + String(httpCode));
        }
        http.end();
    } else {
        Serial.println("Not connected to WiFi");
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
            Serial.println("Weather Payload: " + payload);  // Додано для налагодження
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
    u8g2.clearBuffer();
    u8g2.enableUTF8Print();
    u8g2.setFont(u8g2_font_cu12_t_cyrillic);
    u8g2.setCursor(0, 15);
    u8g2.printf(" %s", weatherDescription.c_str());
    u8g2.setCursor(0, 30);
    u8g2.printf("Вулиця: %.2f C", weatherTemp);
    u8g2.setCursor(0, 45);
    u8g2.printf("%s %02d:%02d:%02d", currentDate.c_str(), currentHour, currentMinute, currentSecond);
    u8g2.sendBuffer();
}

void showTemperatureAndHumidity() {
    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();

    u8g2.clearBuffer();
    u8g2.enableUTF8Print();
    u8g2.setFont(u8g2_font_cu12_t_cyrillic);
    u8g2.setCursor(0, 15);
    u8g2.printf("Кімната: %.2f C", temperature);
    u8g2.setCursor(0, 30);
    u8g2.printf("Волога: %.2f %%", humidity);
    u8g2.setCursor(0, 45);
    u8g2.printf("%s %02d:%02d:%02d", currentDate.c_str(), currentHour, currentMinute, currentSecond);
    u8g2.sendBuffer();

    // Only send data to the mesh and update serial monitor if there's a significant change
    if (abs(temperature - lastTemperature) >= 0.1 || abs(humidity - lastHumidity) >= 1.0) {
        Serial.printf("Temperature: %.2f °C, Humidity: %.2f %%\n", temperature, humidity);
        Serial.printf("Current Date and Time: %s %02d:%02d:%02d\n", currentDate.c_str(), currentHour, currentMinute, currentSecond);
        sendTemperatureAndHumidityData(temperature, humidity); // Send temperature and humidity data
        lastTemperature = temperature;
        lastHumidity = humidity;
    }
}

void readAndDisplayPressure() {
    sensors_event_t event;
    bmp.getEvent(&event);

    if (event.pressure) {
        // Виведення тиску
        Serial.print("Тиск: ");
        Serial.print(event.pressure);
        Serial.println(" hPa");
        
        // Відправка даних тиску в mesh-мережу
        sendPressureData(event.pressure);
    }
}



void sendPressureData(float pressure) {
    char pressureMsg[20];
    sprintf(pressureMsg, "07%.2f", pressure);
    mesh.sendBroadcast(pressureMsg);
    Serial.printf("Sent to mesh: Pressure: %.2f hPa\n", pressure); // Debugging output
}

void showStopwatch() {
    unsigned long elapsed = millis() - stopwatchStartTime;
    int seconds = (elapsed / 1000) % 60;
    int minutes = (elapsed / 60000);

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_cu12_t_cyrillic);
    u8g2.setCursor(0, 15);
    u8g2.printf("Секундомір: %02d:%02d", minutes, seconds);
    u8g2.sendBuffer();

    // Update serial monitor if the interval has passed
    if (millis() - lastSerialUpdateTime >= serialUpdateInterval && seconds != lastPrintedSecond) {
        Serial.printf("Stopwatch time: %02d:%02d\n", minutes, seconds);
        lastSerialUpdateTime = millis();
        lastPrintedSecond = seconds;
    }
}

void showImage() {
    u8g2.clearBuffer();
    u8g2.drawBitmap(0, 0, 16, 128, image);
    u8g2.sendBuffer();
}

void setup() {
    Serial.begin(115200);
    dht.begin();
    connectToWiFi();
    updateTime(); // Initial time update
    updateWeather(); // Initial weather update

    // Ініціалізація BMP180
    if (!bmp.begin()) {
        Serial.println("Не вдалося знайти BMP180! Перевірте з'єднання.");
        while (1);
    }

    pinMode(TRIGGER_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);

    u8g2.begin();
    mesh.init(MESH_PREFIX, MESH_PASSWORD, MESH_PORT);
    mesh.onReceive(&receivedCallback);
    mesh.onNewConnection(&newConnectionCallback);
    
    showImage(); // Показати картинку привітання при запуску
    welcomeScreenStartTime = millis();
    isWelcomeScreenVisible = true;
}

void loop() {
    mesh.update(); // Updating mesh network state

    unsigned long currentMillis = millis();

    // Оновлення температури і вологості при зміні значень
    if (currentMillis - lastTempSendTime >= sendInterval) {
        float currentTemperature = dht.readTemperature();
        float currentHumidity = dht.readHumidity();

        // Перевірка чи змінились значення температури або вологості
        if (abs(currentTemperature - lastTemperature) >= 0.1 || abs(currentHumidity - lastHumidity) >= 1.0) {
            sendTemperatureAndHumidityData(currentTemperature, currentHumidity);
            lastTemperature = currentTemperature;
            lastHumidity = currentHumidity;
        }

        lastTempSendTime = currentMillis;
    }

    // Показати привітання протягом 5 секунд при запуску
    if (isWelcomeScreenVisible && (currentMillis - welcomeScreenStartTime >= 5000)) {
        isWelcomeScreenVisible = false; // Закінчити показ привітання
        isWelcomeScreenShown = true; // Встановити прапорець, що привітання показано
    }

    // Вимірювання відстані
    if (currentMillis - lastDistanceMeasureTime >= distanceMeasureInterval) {
        float distance = measureDistance();
        bool sensorActive = (distance <= 25);
        if (sensorActive && !lastSensorActive) {
            if (!isStopwatchActive && !isTimerActive) {
                isStopwatchActive = true;
                stopwatchStartTime = millis();
                Serial.println("Stopwatch started.");
            } else if (isStopwatchActive) {
                isStopwatchActive = false;
                stopwatchElapsedTime = millis() - stopwatchStartTime;
                Serial.println("Stopwatch stopped.");
            }
        }
        lastSensorActive = sensorActive;

        if (!isWelcomeScreenShown) {
            // Показ привітання
        } else if (distance < 5) {
            showWeather(); // Показати погоду замість анімації
        } else if (isStopwatchActive) {
            showStopwatch();
        } else if (isTimerActive) {
            showTimer(); // Show timer if active
        } else {
            showTemperatureAndHumidity();
        }

        lastDistanceMeasureTime = currentMillis;
    }

    // Оновлення часу кожні 60 секунд
    if (currentMillis - lastTimeUpdate >= timeUpdateInterval) {
        Serial.println("Updating time...");
        updateTime();
        lastTimeUpdate = currentMillis;
    }

    // Оновлення погоди кожні 10 хвилин
    if (currentMillis - lastWeatherUpdate >= 600000) { // 600000 milliseconds = 10 minutes
        Serial.println("Updating weather...");
        updateWeather();
        lastWeatherUpdate = currentMillis;
    }

    // Оновлення секунд на основі millis
    if (currentMillis - lastMillis >= 1000) {
        currentSecond++;
        if (currentSecond >= 60) {
            currentSecond = 0;
            currentMinute++;
            if (currentMinute >= 60) {
                currentMinute = 0;
                currentHour++;
                if (currentHour >= 24) {
                    currentHour = 0;
                }
            }
        }
        lastMillis = currentMillis;
    }

    // Обробка серійного вводу
    handleSerialInput();

    // Перевірка чи закінчився тайм-аут для серійного дисплея
    checkSerialDisplayTimeout();

    // Оновлення таймера
    if (isTimerActive) {
        if (currentMillis - timerStartTime >= timerDuration) {
            stopTimer();
        } else {
            showTimer();
        }
    }

    // Читання і виведення тиску кожні 2 секунди
    static unsigned long lastPressureReadTime = 0;
    if (currentMillis - lastPressureReadTime >= 2000) {
        readAndDisplayPressure();
        lastPressureReadTime = currentMillis;
    }
}

void receivedCallback(uint32_t from, String &msg) {
    Serial.printf("Received from %u: %s\n", from, msg.c_str());

    // Обробка команди для запуску таймера
    if (msg.startsWith("start=")) {
        int minutes = msg.substring(6).toInt();
        startTimer(minutes * 60); // Convert minutes to seconds
    } else if (msg == "stop") {
        stopTimer();
    }
}

void newConnectionCallback(uint32_t nodeId) {
    Serial.printf("New Connection, nodeId = %u\n", nodeId);
}

float measureDistance() {
    digitalWrite(TRIGGER_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIGGER_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIGGER_PIN, LOW);

    float duration = pulseIn(ECHO_PIN, HIGH);
    return duration * 0.034 / 2;
}

void sendTemperatureAndHumidityData(float temperature, float humidity) {
    char tempMsg[20], humiMsg[20];
    sprintf(tempMsg, "05%.2f", temperature);
    sprintf(humiMsg, "06%.2f", humidity);
    mesh.sendBroadcast(tempMsg);
    mesh.sendBroadcast(humiMsg);
    Serial.printf("Sent to mesh: Temperature: %.2f °C, Humidity: %.2f %%\n", temperature, humidity); // Debugging output
}

void handleSerialInput() {
    if (Serial.available() > 0) {
        String input = Serial.readStringUntil('\n');
        input.trim();

        if (input.startsWith("start=")) {
            int minutes = input.substring(6).toInt();
            startTimer(minutes * 60); // Convert minutes to seconds
        } else if (input == "stop") {
            stopTimer();
        } else {
            lastSerialInput = input;
            lastSerialInputTime = millis();
        }
    }
}

void checkSerialDisplayTimeout() {
    if (lastSerialInput != "" && millis() - lastSerialInputTime >= serialDisplayDuration) {
        lastSerialInput = "";
        showTemperatureAndHumidity();
    }
}

void showTimer() {
    unsigned long elapsed = millis() - timerStartTime;
    unsigned long remaining = timerDuration - elapsed;

    int seconds = (remaining / 1000) % 60;
    int minutes = (remaining / 60000) % 60;
    int hours = (remaining / 3600000);

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_cu12_t_cyrillic);
    u8g2.setCursor(0, 15);
    u8g2.printf("Таймер: %02d:%02d:%02d", hours, minutes, seconds);
    u8g2.sendBuffer();
}

void startTimer(int durationInSeconds) {
    isTimerActive = true;
    timerStartTime = millis();
    timerDuration = durationInSeconds * 1000;
    Serial.printf("Timer started for %d seconds\n", durationInSeconds);
}

void stopTimer() {
    isTimerActive = false;
    Serial.println("Timer stopped.");
    showTemperatureAndHumidity(); // Return to room information after timer stops
}