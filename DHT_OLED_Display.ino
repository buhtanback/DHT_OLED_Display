#include "Config.h"
#include "painlessMesh.h"
#include <DHT.h>
#include <U8g2lib.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#define MESH_PREFIX "buhtan"
#define MESH_PASSWORD "buhtan123"
#define MESH_PORT 5555

painlessMesh mesh;

#define DHTPIN 4
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

#define TRIGGER_PIN 14
#define ECHO_PIN 27

#define skeletor_width 64
#define skeletor_height 64

#define OLED_RESET -1
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, OLED_RESET);

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
            currentTime = dateTime.substring(11, 19); // Витягнути час з datetime
            currentHour = currentTime.substring(0, 2).toInt();
            currentMinute = currentTime.substring(3, 5).toInt();
            currentSecond = currentTime.substring(6, 8).toInt();
            lastMillis = millis();
            Serial.println("Time updated: " + currentTime);
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
    u8g2.printf("Час: %02d:%02d:%02d", currentHour, currentMinute, currentSecond);
    u8g2.sendBuffer();

    // Update serial monitor if the interval has passed
    if (millis() - lastSerialUpdateTime >= serialUpdateInterval && currentSecond != lastPrintedSecond) {
        Serial.printf("Temperature: %.2f °C, Humidity: %.2f %%\n", temperature, humidity);
        Serial.printf("Current Time: %02d:%02d:%02d\n", currentHour, currentMinute, currentSecond);
        lastSerialUpdateTime = millis();
        lastPrintedSecond = currentSecond;
    }

    // Only send data to the mesh if there's a significant change
    if (abs(temperature - lastTemperature) >= 0.1 || abs(humidity - lastHumidity) >= 1.0) {
        lastTemperature = temperature;
        lastHumidity = humidity;
        sendTemperatureAndHumidityData(temperature, humidity); // передаємо температуру та вологість
    }
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

    pinMode(TRIGGER_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);

    u8g2.begin();
    mesh.init(MESH_PREFIX, MESH_PASSWORD, MESH_PORT);
    mesh.onReceive(&receivedCallback);
    mesh.onNewConnection(&newConnectionCallback);
    
    showImage(); // Показати картинку привітання при запуску
    delay(5000); // Затримка для показу привітання (5 секунд)
    isWelcomeScreenShown = true; // Встановити прапорець, що привітання показано
}

void loop() {
    mesh.update(); // Updating mesh network state

    unsigned long currentMillis = millis();

    // Measure distance at a defined interval
    if (currentMillis - lastDistanceMeasureTime >= distanceMeasureInterval) {
        float distance = measureDistance();

        // Determine if the sensor is active
        bool sensorActive = (distance <= 25);

        // Check for state transition
        if (sensorActive && !lastSensorActive) {
            // Sensor just became active
            if (!isStopwatchActive) {
                isStopwatchActive = true;
                stopwatchStartTime = millis();
                Serial.println("Stopwatch started.");
            } else {
                // If the stopwatch was already active, stop it
                isStopwatchActive = false;
                stopwatchElapsedTime = millis() - stopwatchStartTime;
                Serial.println("Stopwatch stopped.");
            }
        }

        // Update last sensor state
        lastSensorActive = sensorActive;

        // Display appropriate screen
        if (!isWelcomeScreenShown) {
            showImage();
            delay(5000);
            isWelcomeScreenShown = true;
        } else if (distance < 5) {
            showWeather(); // Показати погоду замість анімації
        } else if (isStopwatchActive) {
            showStopwatch();
        } else {
            showTemperatureAndHumidity();
        }

        lastDistanceMeasureTime = currentMillis;
    }

    // Update time every 60 seconds
    if (currentMillis - lastTimeUpdate >= timeUpdateInterval) {
        Serial.println("Updating time...");
        updateTime();
        lastTimeUpdate = currentMillis;
    }

    // Update weather every 10 minutes
    if (currentMillis - lastTimeUpdate >= 600000) { // 600000 milliseconds = 10 minutes
        Serial.println("Updating weather...");
        updateWeather();
    }

    // Update seconds based on millis
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

    // Send temperature and humidity data to mesh network
    if (currentMillis - lastTempSendTime >= updateInterval) {
        lastTempSendTime = currentMillis;
        float temperature = dht.readTemperature();
        float humidity = dht.readHumidity();
        Serial.println("Sending temperature and humidity data to mesh network...");
        sendTemperatureAndHumidityData(temperature, humidity); // передаємо температуру та вологість
    }
}

void receivedCallback(uint32_t from, String &msg) {
    Serial.printf("Received from %u: %s\n", from, msg.c_str());
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
