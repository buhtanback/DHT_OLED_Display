#include <DHT.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <HTTPClient.h>

#define DHTPIN 4    
#define DHTTYPE DHT22   
DHT dht(DHTPIN, DHTTYPE);

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino resetpin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

const char* ssid = "YourWiFiSSID";
const char* password = "YourWiFiPassword";
const char* url = "https://ua.sinoptik.ua/%D0%BF%D0%BE%D0%B3%D0%BE%D0%B4%D0%B0-%D1%85%D0%BC%D0%B5%D0%BB%D1%8C%D0%BD%D0%B8%D1%86%D1%8C%D0%BA%D0%B8%D0%B9";

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

float getTemperatureFromWeb() {
    HTTPClient http;
    http.begin(url); 
    int httpCode = http.GET(); 
    float temperatureWeb = -999.0; 
    int precipitationProbability = -1; 
    
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
    
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0, 0);
    display.print("Outside Temp: ");
    display.print(temperatureWeb);
    display.println(" C");
    display.print("Precipitation: ");
    display.print(precipitationProbability);
    display.println(" %");
    display.display();

    Serial.print("Outside Temp: ");
    Serial.print(temperatureWeb);
    Serial.print(" C, Precipitation: ");
    Serial.print(precipitationProbability);
    Serial.println(" %");

    return temperatureWeb;
}

void readDHTSensor(float& temperature, float& humidity) {
    delay(2000); 
    temperature = dht.readTemperature(); 
    humidity = dht.readHumidity(); 
}

void showOutsideWeather() {
    float temperatureWeb = getTemperatureFromWeb();
}

void showInsideTemperatureHumidity() {
    float temperatureDHT, humidityDHT;
    readDHTSensor(temperatureDHT, humidityDHT);
    
    display.clearDisplay();
    display.setCursor(0, 0);
    display.print("Inside Temp:");
    display.print(temperatureDHT);
    display.println(" C");
    display.print("Humidity:");
    display.print(humidityDHT);
    display.println(" %");
    display.display();

    Serial.print("Inside Temp:");
    Serial.print(temperatureDHT);
    Serial.print(" C, Humidity:");
    Serial.print(humidityDHT);
    Serial.println(" %");
}
