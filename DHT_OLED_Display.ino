#include "Config.h"
#include "painlessMesh.h"
#include <DHT.h>
#include <U8g2lib.h>

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

unsigned long lastUpdateTime = 0;  // Час останнього оновлення
const long updateInterval = 2000;  // Інтервал оновлень (2000 мілісекунд = 2 секунди)

unsigned long lastTempSendTime = 0;  // Час останнього відправлення температури
unsigned long lastHumiSendTime = 0;  // Час останнього відправлення вологості
const long sendInterval = 500;  // Мінімальний інтервал між відправленнями (500 мілісекунд)

unsigned long startMessageTime = 0; // Час початку показу повідомлення
const long messageDisplayDuration = 2000; // Тривалість показу повідомлення (2000 мілісекунд = 2 секунди)
bool showingMessage = true; // Логічна змінна для стану відображення повідомлення

bool isAnimating = false; // Логічна змінна для стану анімації
unsigned long lastAnimationTime = 0; // Час останньої анімації



void showImage() {
  u8g2.clearBuffer();
  u8g2.drawBitmap(0, 0, 16, 128, image);  // Виведення зображення на дисплей
  u8g2.sendBuffer();
}

void showMessage() {
  u8g2.clearBuffer();
  u8g2.enableUTF8Print();
  u8g2.setFont(u8g2_font_cu12_t_cyrillic);
  u8g2.setCursor(10, 35);
  u8g2.print("ДЕ Я НАХУЙ:?");
  u8g2.sendBuffer();  // Відправляємо буфер на дисплей
}

void setup() {
    Serial.begin(115200);
    dht.begin();

    pinMode(TRIGGER_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);

    u8g2.begin();
    
    // Показуємо зображення на дисплеї
    showImage();
    startMessageTime = millis();  // Записуємо час початку показу зображення

    mesh.init(MESH_PREFIX, MESH_PASSWORD, MESH_PORT);
    mesh.onReceive(&receivedCallback);
    mesh.onNewConnection(&newConnectionCallback);
}

float measureDistance() {
    digitalWrite(TRIGGER_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIGGER_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIGGER_PIN, LOW);

    float duration = pulseIn(ECHO_PIN, HIGH);
    float distance = duration * 0.034 / 2;

    return distance;
}

void animateTriangle() {
    if (millis() - lastAnimationTime >= 100) { // інтервал анімації 100 мс
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

void loop() {
    mesh.update();  // Оновлення стану mesh мережі

    if (showingMessage) {
        // Перевіряємо, чи минув час показу повідомлення
        if (millis() - startMessageTime >= messageDisplayDuration) {
            // Змінюємо стан, щоб припинити показ повідомлення
            showingMessage = false;
            showMessage();
            startMessageTime = millis();  // Оновлюємо час початку показу повідомлення
        }
    } else {
        float distance = measureDistance();

        Serial.printf("Measured distance: %.2f cm\n", distance); // Додаємо вивід виміряної відстані для налагодження

        if (distance <= 25) {
            isAnimating = true;
        } else {
            isAnimating = false;
        }

        if (isAnimating) {
            animateTriangle();
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

                if (currentMillis - lastHumiSendTime >= sendInterval + 100) { // Додатковий інтервал для уникнення одночасного відправлення
                    lastHumiSendTime = currentMillis;
                    char humiMsg[20];
                    sprintf(humiMsg, "06%.2f", humidity);
                    mesh.sendBroadcast(humiMsg);
                }
            } else {
                // Якщо не час оновлювати дисплей, все одно оновлюємо анімацію (щоб дисплей був живим)
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
