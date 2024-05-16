#include "painlessMesh.h"
#include <DHT.h>
#include <U8g2lib.h>

#define MESH_PREFIX     "buhtan"
#define MESH_PASSWORD   "buhtan123"
#define MESH_PORT       5555  // Додано визначення порту для mesh

painlessMesh mesh;

#define DHTPIN 4    
#define DHTTYPE DHT22   
DHT dht(DHTPIN, DHTTYPE);

#define OLED_RESET -1
U8G2_SSD1306_128X64_NONAME_F_HW_I2C display(U8G2_R0, OLED_RESET);

void setup() {
    Serial.begin(115200);
    dht.begin();

    display.begin();
    display.clearBuffer();
    display.setFont(u8g2_font_ncenB14_tr);
    display.drawStr(0, 24, "WELCOME");
    display.sendBuffer();
    delay(2000);

    mesh.init(MESH_PREFIX, MESH_PASSWORD, MESH_PORT);
    mesh.onReceive(&receivedCallback);
    mesh.onNewConnection(&newConnectionCallback);
}

unsigned long lastUpdateTime = 0;  // Час останнього оновлення
const long updateInterval = 2000;  // Інтервал оновлень (2000 мілісекунд = 2 секунди)

void loop() {
    mesh.update();  // Оновлення стану mesh мережі

    unsigned long currentMillis = millis();
    
    if (currentMillis - lastUpdateTime >= updateInterval) {
        lastUpdateTime = currentMillis;

        float temperature = dht.readTemperature();
        float humidity = dht.readHumidity();

        display.clearBuffer();
        display.setFont(u8g2_font_ncenB08_tr);
        display.setCursor(0, 15);
        display.printf("Temperature: %.2f C", temperature);
        display.setCursor(0, 30);
        display.printf("Humidity: %.2f %%", humidity);
        display.sendBuffer();

        Serial.print("Temperature: ");
        Serial.print(temperature);
        Serial.print(" °C, Humidity: ");
        Serial.print(humidity);
        Serial.println(" %");

        String message = String("Temperature: ") + temperature + " C, Humidity: " + humidity + " %";
        mesh.sendBroadcast(message);
    }
}

void receivedCallback(uint32_t from, String &msg) {
    Serial.printf("Received from %u: %s\n", from, msg.c_str());
}

void newConnectionCallback(uint32_t nodeId) {
    Serial.printf("New Connection, nodeId = %u\n", nodeId);
}
