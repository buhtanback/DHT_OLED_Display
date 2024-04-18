#include "Config.h"
#include <DHT.h>
#include <U8g2lib.h> 
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TimeLib.h>
#include <Wire.h>

#define DHTPIN 4
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);




#define JOYSTICK_X_PIN 34 
#define JOYSTICK_Y_PIN 35
#define BUTTON_PIN 32



U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, /* clock=*/ SCL, /* data=*/ SDA, /* reset=*/ U8X8_PIN_NONE);  

enum ScreenMode {
    OUTSIDE_WEATHER,
    INSIDE_TEMPERATURE_HUMIDITY
};

ScreenMode currentScreen = OUTSIDE_WEATHER;
;

int joystickXValue = 0;
int joystickYValue = 0;




void showImage() {
    u8g2.clearBuffer();
    u8g2.drawXBMP(0, 0, image_widht, image_height, image);
    u8g2.sendBuffer();
}


void setup() {
    Serial.begin(9600);
    dht.begin();
    u8g2.begin();  
    u8g2.clearBuffer();  
    u8g2.enableUTF8Print();
    u8g2.setFont(u8g2_font_cu12_t_cyrillic); 
    u8g2.drawStr(40, 35, "Welcome");  
    u8g2.sendBuffer();  
    delay(2000);
    u8g2.clearBuffer();  

    WiFi.begin(ssid, password);
    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("Connected");

    setTimeFromAPI();

    pinMode(BUTTON_PIN, INPUT_PULLUP); 
    showImage();
}


void loop() {
    readButton();

    if (currentScreen == INSIDE_TEMPERATURE_HUMIDITY) {
        showInsideTemperatureHumidity();
    } else {
        showOutsideWeather();
    }

    delay(100);
}

void readButton() {
    if (digitalRead(BUTTON_PIN) == LOW) {
        
        switchScreen();
        delay(100); 
    }
}

void switchScreen() {
    if (currentScreen == INSIDE_TEMPERATURE_HUMIDITY) {
        currentScreen = OUTSIDE_WEATHER;
    } else {
        currentScreen = INSIDE_TEMPERATURE_HUMIDITY;
    }
}



void setTimeFromAPI() {
    HTTPClient http;

    http.begin(url1);
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();

        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, payload);

        if (!error) {
            long unixTime = doc["unixtime"];
            setTime(unixTime + 3 * 3600);
            Serial.println("Time set from API");
        } else {
            Serial.println("Failed to parse JSON");
        }
    } else {
        Serial.println("Failed to get time from API");
    }

    http.end();
}



String getCurrentTime() {
    int currentHour = hour();
    int currentMinute = minute();
    int currentSecond = second();
    int currentDay = day();
    int currentMonth = month();
    int currentYear = year() % 100;  

    String currentTime = String("");
    currentTime += currentDay < 10 ? "0" + String(currentDay) : String(currentDay);
    currentTime += ".";
    currentTime += currentMonth < 10 ? "0" + String(currentMonth) : String(currentMonth);
    currentTime += ".";
    currentTime += currentYear < 10 ? "0" + String(currentYear) : String(currentYear);
    currentTime += " ";
    currentTime += currentHour < 10 ? "0" + String(currentHour) : String(currentHour);
    currentTime += ":";
    currentTime += currentMinute < 10 ? "0" + String(currentMinute) : String(currentMinute);
    currentTime += ":";
    currentTime += currentSecond < 10 ? "0" + String(currentSecond) : String(currentSecond);

    return currentTime;
}

void getTemperatureFromWeb(float& temperatureWeb, int& precipitationProbability) {
    HTTPClient http;
    http.begin(url);
    int httpCode = http.GET();

    temperatureWeb = -999.0;
    precipitationProbability = -1;

    if (httpCode > 0) {
        String payload = http.getString();

        int startIndex = payload.indexOf("class=\"today-temp\">") + 20;
        int endIndex = payload.indexOf("</span>", startIndex);
        String temperatureStr = payload.substring(startIndex, endIndex);
        temperatureWeb = temperatureStr.toFloat();

        startIndex = payload.indexOf("class=\"gray\">") + 13;
        endIndex = payload.indexOf("</span>", startIndex);
        String precipitationStr = payload.substring(startIndex, endIndex);
        precipitationProbability = precipitationStr.toInt();
    }

    http.end();
}

void showOutsideWeather() {
    float temperatureWeb;
    int precipitationProbability;
    getTemperatureFromWeb(temperatureWeb, precipitationProbability);

    String currentTime = getCurrentTime();

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_cu12_t_cyrillic);
    u8g2.setCursor(0, 15);
    u8g2.print("Outside: ");
    u8g2.print(temperatureWeb);
    u8g2.print(" C");
    u8g2.setCursor(0, 30);
    u8g2.print("Osadki: ");
    u8g2.print(precipitationProbability);
    u8g2.print(" %");
    u8g2.setCursor(0, 45);
    u8g2.print(currentTime);
    u8g2.sendBuffer();

    Serial.print("Street: ");
    Serial.print(temperatureWeb);
    Serial.print(" C, Osadki: ");
    Serial.print(precipitationProbability);
    Serial.print(" %, Time:  ");
    Serial.println(currentTime);
}

void readDHTSensor(float& temperature, float& humidity) {
    delay(2000);
    temperature = dht.readTemperature();
    humidity = dht.readHumidity();
}

void showInsideTemperatureHumidity() {
    float temperatureDHT, humidityDHT;
    readDHTSensor(temperatureDHT, humidityDHT);

    String currentTime = getCurrentTime();

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_cu12_t_cyrillic);
    u8g2.setCursor(0, 15);
    u8g2.print("Room: ");
    u8g2.print(temperatureDHT);
    u8g2.print(" C");
    u8g2.setCursor(0, 30);
    u8g2.print("Vlaga: ");
    u8g2.print(humidityDHT);
    u8g2.print(" %");
    u8g2.setCursor(0, 45);
    u8g2.print(currentTime);
    u8g2.sendBuffer();

    Serial.print("Room: ");
    Serial.print(temperatureDHT);
    Serial.print(" C, Vlaga: ");
    Serial.print(humidityDHT);
    Serial.print(" %, Time:  ");
    Serial.println(currentTime);
}