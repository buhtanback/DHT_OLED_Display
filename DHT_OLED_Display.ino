#include "Config.h"

#include <DHT.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TimeLib.h>

#define DHTPIN 4    
#define DHTTYPE DHT22   
DHT dht(DHTPIN, DHTTYPE);

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino resetpin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

enum ScreenMode {
    OUTSIDE_WEATHER,
    INSIDE_TEMPERATURE_HUMIDITY
};

ScreenMode currentScreen = OUTSIDE_WEATHER;

void setup() {
    Serial.begin(9600);
    dht.begin();
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println(F("SSD1306 allocation failed"));
        for (;;); 
    }
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.setCursor(0, 0);
    display.println("WELCOME");
    display.display();
    delay(2000);
    display.clearDisplay();

    WiFi.begin(ssid, password);
    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("Connected");

    
    setTimeFromAPI(); 
}

void loop() {
    switch (currentScreen) {
        case OUTSIDE_WEATHER:
            showOutsideWeather();
            delay(5000); 
            currentScreen = INSIDE_TEMPERATURE_HUMIDITY;
            break;
        case INSIDE_TEMPERATURE_HUMIDITY:
            showInsideTemperatureHumidity();
            delay(5000); 
            currentScreen = OUTSIDE_WEATHER;
            break;
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

    String currentTime = String("");
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

    display.clearDisplay();
    display.setTextSize(1, 2);
    display.setTextColor(WHITE);
    display.setCursor(0, 0);
    display.print("Outside Temp: ");
    display.print(temperatureWeb);
    display.println(" C");
    display.print("Precipitation: ");
    display.print(precipitationProbability);
    display.println(" %");
    display.print("Time: ");
    display.print(getCurrentTime());
    display.display();

    Serial.print("Outside Temp: ");
    Serial.print(temperatureWeb);
    Serial.print(" C, Precipitation: ");
    Serial.print(precipitationProbability);
    Serial.print(" %, Time: ");
    Serial.println(getCurrentTime());
}

void readDHTSensor(float& temperature, float& humidity) {
    delay(2000); 
    temperature = dht.readTemperature(); 
    humidity = dht.readHumidity(); 
}

void showInsideTemperatureHumidity() {
    float temperatureDHT, humidityDHT;
    readDHTSensor(temperatureDHT, humidityDHT);

    display.clearDisplay();
    display.setTextSize(1, 2);
    display.setTextColor(WHITE);
    display.setCursor(0, 0);
    display.print("Inside Temp: ");
    display.print(temperatureDHT);
    display.println(" C");
    display.print("Humidity: ");
    display.print(humidityDHT);
    display.println(" %");
    display.print("Time: ");
    display.print(getCurrentTime());
    display.display();

    Serial.print("Inside Temp: ");
    Serial.print(temperatureDHT);
    Serial.print(" C, Humidity: ");
    Serial.print(humidityDHT);
    Serial.print(" %, Time: ");
    Serial.println(getCurrentTime());
}
