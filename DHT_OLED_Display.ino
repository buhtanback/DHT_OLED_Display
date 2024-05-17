#include "painlessMesh.h"
#include <DHT.h>
#include <U8g2lib.h>

#define MESH_PREFIX     "buhtan"
#define MESH_PASSWORD   "buhtan123"
#define MESH_PORT       5555

painlessMesh mesh;

#define DHTPIN 4    
#define DHTTYPE DHT22   
DHT dht(DHTPIN, DHTTYPE);

#define OLED_RESET -1
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, OLED_RESET);

unsigned long lastUpdateTime = 0;  // Час останнього оновлення
const long updateInterval = 2000;  // Інтервал оновлень (2000 мілісекунд = 2 секунди)

unsigned long lastTempSendTime = 0;  // Час останнього відправлення температури
unsigned long lastHumiSendTime = 0;  // Час останнього відправлення вологості
const long sendInterval = 100;  // Мінімальний інтервал між відправленнями (100 мілісекунд)

unsigned long startMessageTime = 0; // Час початку показу повідомлення
const long messageDisplayDuration = 2000; // Тривалість показу повідомлення (2000 мілісекунд = 2 секунди)
bool showingMessage = true; // Логічна змінна для стану відображення повідомлення

void setup() {
    Serial.begin(115200);
    dht.begin();

    u8g2.begin();
    u8g2.clearBuffer();
    u8g2.enableUTF8Print();
    u8g2.setFont(u8g2_font_cu12_t_cyrillic);
    u8g2.setCursor(10, 35);
    u8g2.print("ПІШОВ НАХУЙ");
    u8g2.sendBuffer();  // Відправляємо буфер на дисплей
    startMessageTime = millis();  // Записуємо час початку показу повідомлення

    mesh.init(MESH_PREFIX, MESH_PASSWORD, MESH_PORT);
    mesh.onReceive(&receivedCallback);
    mesh.onNewConnection(&newConnectionCallback);
}

void loop() {
    mesh.update();  // Оновлення стану mesh мережі

    if (showingMessage) {
        // Перевіряємо, чи минув час показу повідомлення
        if (millis() - startMessageTime >= messageDisplayDuration) {
            // Змінюємо стан, щоб припинити показ повідомлення
            showingMessage = false;
            u8g2.clearBuffer();
            u8g2.sendBuffer();  // Очищуємо дисплей після показу повідомлення
        }
    } else {
        // Основний функціонал оновлення дисплею і передачі даних через mesh
        if (millis() - lastUpdateTime >= updateInterval) {
            lastUpdateTime = millis();

            float temperature = dht.readTemperature();
            float humidity = dht.readHumidity();

            u8g2.clearBuffer();
            u8g2.enableUTF8Print();
            u8g2.setFont(u8g2_font_cu12_t_cyrillic);
            u8g2.setCursor(0, 15);
            u8g2.printf("Кімната: %.2f C", temperature);
            u8g2.setCursor(0, 30);
            u8g2.printf("Волога: %.2f %%", humidity);
            u8g2.sendBuffer();

            Serial.printf("Temperature: %.2f °C, Humidity: %.2f %%\n", temperature, humidity);

            // Відправлення окремих повідомлень з мінімальною затримкою
            unsigned long currentMillis = millis();
            if (currentMillis - lastTempSendTime >= sendInterval) {
                lastTempSendTime = currentMillis;
                char tempMsg[20];
                sprintf(tempMsg, "05%.2f", temperature);
                mesh.sendBroadcast(tempMsg);
            }

            if (currentMillis - lastHumiSendTime >= sendInterval) {
                lastHumiSendTime = currentMillis;
                char humiMsg[20];
                sprintf(humiMsg, "06%.2f", humidity);
                mesh.sendBroadcast(humiMsg);
            }
        }
    }
}

void receivedCallback(uint32_t from, String &msg) {
    Serial.printf("Received from %u: %s\n", from, msg.c_str());
}

void newConnectionCallback(uint32_t nodeId) {
    Serial.printf("New Connection, nodeId = %u\n", nodeId);
}
