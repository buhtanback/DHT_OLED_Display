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

void updateTime() {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        http.begin("http://worldtimeapi.org/api/timezone/Europe/Kiev");
        int httpCode = http.GET();
        
        if (httpCode > 0) {
            String payload = http.getString();
            DynamicJsonDocument doc(1024);
            deserializeJson(doc, payload);
            String dateTime = doc["datetime"];
            currentTime = dateTime.substring(11, 19); // Витягнути час з datetime
            http.end();
        } else {
            currentTime = "Failed to get time";
            http.end();
        }
    } else {
        currentTime = "Not connected to WiFi";
    }
}

void animateTriangle() {
    if (millis() - lastAnimationTime >= 100) { // Animation interval 100 ms
        lastAnimationTime = millis();
        u8g2.clearBuffer();
        static uint8_t a = 0;

        switch (a % 4) { 
        case 0: u8g2.drawXBMP(32, 0, skeletor_width, skeletor_height, skeletor_bits[0]); break;
        case 1: u8g2.drawXBMP(32, 0, skeletor_width, skeletor_height, skeletor_bits[1]); break;
        case 2: u8g2.drawXBMP(32, 0, skeletor_width, skeletor_height, skeletor_bits[2]); break;
        case 3: u8g2.drawXBMP(32, 0, skeletor_width, skeletor_height, skeletor_bits[3]); break;
        }

        u8g2.sendBuffer();
        a++;
    }
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
    u8g2.printf("Час: %s", currentTime.c_str());
    u8g2.sendBuffer();

    // Update serial monitor if the interval has passed
    if (millis() - lastSerialUpdateTime >= serialUpdateInterval) {
        Serial.printf("Temperature: %.2f °C, Humidity: %.2f %%\n", temperature, humidity);
        Serial.printf("Time: %s\n", currentTime.c_str());
        lastSerialUpdateTime = millis();
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
    if (millis() - lastSerialUpdateTime >= serialUpdateInterval) {
        Serial.printf("Stopwatch time: %02d:%02d\n", minutes, seconds);
        lastSerialUpdateTime = millis();
    }
}

void setup() {
    Serial.begin(115200);
    dht.begin();

    pinMode(TRIGGER_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);

    u8g2.begin();

    // Connect to Wi-Fi
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Connecting to WiFi...");
    }
    Serial.println("Connected to WiFi");

    updateTime(); // Initial time update

    mesh.init(MESH_PREFIX, MESH_PASSWORD, MESH_PORT);
    mesh.onReceive(&receivedCallback);
    mesh.onNewConnection(&newConnectionCallback);
}

void loop() {
    mesh.update(); // Updating mesh network state

    // Update time every timeUpdateInterval
    if (millis() - lastTimeUpdate >= timeUpdateInterval) {
        updateTime();
        lastTimeUpdate = millis();
    }

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
    if (distance < 5) {
        animateTriangle();
    } else if (isStopwatchActive) {
        showStopwatch();
    } else {
        showTemperatureAndHumidity();
    }

    // Handle sending data to the mesh
    if (!isStopwatchActive && millis() - lastUpdateTime > updateInterval) {
        lastUpdateTime = millis();
        sendTemperatureAndHumidityData();
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

void sendTemperatureAndHumidityData() {
    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();
    char tempMsg[20], humiMsg[20];
    sprintf(tempMsg, "05%.2f", temperature);
    sprintf(humiMsg, "06%.2f", humidity);
    mesh.sendBroadcast(tempMsg);
    mesh.sendBroadcast(humiMsg);
}
