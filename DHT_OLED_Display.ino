#include "Config.h"
#include <Wire.h>
#include <U8g2lib.h>
#include <DHT.h>
#include "painlessMesh.h"
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP085_U.h>

#define MESH_PREFIX "buhtan"
#define MESH_PASSWORD "buhtan123"
#define MESH_PORT 5555

painlessMesh mesh;

#define OLED_RESET      -1  // Скидання не використовується для SSD1306 I2C
#define JOYSTICK_X_PIN  35
#define JOYSTICK_Y_PIN  34
#define BUTTON_PIN      5   // Використовуйте той самий пін для вибору і повернення

#define DHTPIN          4   // Пін, до якого підключений датчик DHT
#define DHTTYPE         DHT22   // Тип датчика DHT

DHT dht(DHTPIN, DHTTYPE);
Adafruit_BMP085_Unified bmp = Adafruit_BMP085_Unified(10085);

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, OLED_RESET);

int menuOption = 0;
int buttonState = 0;
int lastButtonState = 0;
unsigned long lastDebounceTime = 0;  // Час останнього зміни стану джойстика
unsigned long debounceDelay = 200;   // Затримка для debounce джойстика

bool inSubMenu = false; // Прапорець для відстеження чи ми в підменю

float lastTemperature = 0;
float lastHumidity = 0;
float lastPressure = 0;

unsigned long welcomeScreenStartTime = 0;
bool isWelcomeScreenVisible = false;

unsigned long stopwatchStartTime = 0;
bool stopwatchRunning = false;
unsigned long lastSerialUpdateTime = 0;
unsigned long serialUpdateInterval = 1000;
int lastPrintedSecond = -1;

void setup() {
    pinMode(BUTTON_PIN, INPUT_PULLUP); // Налаштування піну кнопки з pull-up
    u8g2.begin(); // Ініціалізація дисплея
    dht.begin();  // Ініціалізація датчика DHT
    if (!bmp.begin()) {
        Serial.print("Не вдалося знайти датчик BMP085.");
        while (1);
    }
    Serial.begin(115200); // Ініціалізація Serial для діагностики
    
    // Ініціалізація мережі
    mesh.setDebugMsgTypes(ERROR | STARTUP | CONNECTION);  // Встановлення типів повідомлень
    mesh.init(MESH_PREFIX, MESH_PASSWORD, MESH_PORT);
    mesh.onReceive([](uint32_t from, String &msg) {
        Serial.printf("Received from %u msg=%s\n", from, msg.c_str());
    });

    showImage(); // Показати картинку привітання при запуску
    welcomeScreenStartTime = millis();
    isWelcomeScreenVisible = true;
}

void loop() {
    mesh.update();  // Оновлення стану мережі

    if (isWelcomeScreenVisible) {
        if (millis() - welcomeScreenStartTime > 3000) { // Показувати картинку привітання протягом 3 секунд
            isWelcomeScreenVisible = false;
        } else {
            return; // Повернення для продовження показу привітання
        }
    }

    // Зчитування значення джойстика по осі Y
    int joystickY = analogRead(JOYSTICK_Y_PIN);
  
    if (!inSubMenu) {
        // Перевірка, чи джойстик був переміщений
        if (millis() - lastDebounceTime > debounceDelay) {
            if (joystickY < 1000) { // Вгору
                menuOption--;
                if (menuOption < 0) {
                    menuOption = 2;
                }
                lastDebounceTime = millis();
                Serial.print("Joystick moved up. New menuOption: ");
                Serial.println(menuOption);
            } else if (joystickY > 3000) { // Вниз
                menuOption++;
                if (menuOption > 2) {
                    menuOption = 0;
                }
                lastDebounceTime = millis();
                Serial.print("Joystick moved down. New menuOption: ");
                Serial.println(menuOption);
            }
        }

        // Зчитування стану кнопки
        int reading = digitalRead(BUTTON_PIN);
        if (reading != lastButtonState) {
            lastDebounceTime = millis();
        }

        // Перевірка стану кнопки
        if ((millis() - lastDebounceTime) > debounceDelay) {
            if (reading != buttonState) {
                buttonState = reading;
                if (buttonState == LOW) {
                    inSubMenu = true;
                    if (menuOption == 2) { // Якщо вибрано секундомір
                        stopwatchStartTime = millis(); // Скидання таймера
                        stopwatchRunning = true; // Запуск секундоміра
                    }
                    Serial.print("Button pressed. Entering subMenu: ");
                    Serial.println(menuOption);
                }
            }
        }
        lastButtonState = reading;

        // Відображення меню
        u8g2.clearBuffer(); // Очищення буфера дисплея
        u8g2.setFont(u8g2_font_cu12_t_cyrillic); // Вибір шрифту

        for (int i = 0; i < 3; i++) {
            int y = 20 + i * 20;
            u8g2.setCursor(10, y);
            if (menuOption == i) {
                u8g2.drawFrame(0, y - 10, 128, 15); // Додаємо рамку навколо активного елемента
                u8g2.print("> ");
            } else {
                u8g2.print("  ");
            }

            if (i == 0) u8g2.print("Кімната");
            else if (i == 1) u8g2.print("Вулиця");
            else if (i == 2) u8g2.print("Секундомір");
        }

        u8g2.sendBuffer(); // Відправка буфера на дисплей
    } else {
        // Відображення підменю
        u8g2.clearBuffer(); // Очищення буфера дисплея
        u8g2.setFont(u8g2_font_cu12_t_cyrillic); // Вибір шрифту

        if (menuOption == 0) {
            showTemperatureAndHumidity();
        } else if (menuOption == 1) {
            u8g2.setCursor(10, 20);
            u8g2.print("Submenu Option 2");
            u8g2.setCursor(10, 40);
            u8g2.print("Press to return");
            u8g2.sendBuffer();
        } else if (menuOption == 2) {
            showStopwatch();
        }

        // Зчитування стану кнопки для повернення
        int reading = digitalRead(BUTTON_PIN);
        if (reading != lastButtonState) {
            lastDebounceTime = millis();
        }

        // Перевірка стану кнопки для повернення
        if ((millis() - lastDebounceTime) > debounceDelay) {
            if (reading != buttonState) {
                buttonState = reading;
                if (buttonState == LOW) {
                    if (menuOption == 2 && inSubMenu) {
                        if (stopwatchRunning) {
                            stopwatchRunning = false;
                            Serial.println("Stopwatch stopped.");
                        } else {
                            stopwatchStartTime = millis(); // Скидання таймера
                            stopwatchRunning = true; // Запуск секундоміра
                            Serial.println("Stopwatch started.");
                        }
                    } else {
                        inSubMenu = false; // Повернення до головного меню
                        Serial.println("Button pressed. Returning to main menu.");
                    }
                }
            }
        }
        lastButtonState = reading;
    }

    // Періодичне зчитування тиску
    if (millis() - lastDebounceTime > 1000) { // Зчитувати тиск кожну секунду
        readAndDisplayPressure();
        lastDebounceTime = millis();
    }
}



void showTemperatureAndHumidity() {
    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();
    sensors_event_t event;
    bmp.getEvent(&event);

    float pressure = event.pressure; // Зчитування тиску

    u8g2.clearBuffer(); // Очищення буфера дисплея
    u8g2.enableUTF8Print();
    u8g2.setFont(u8g2_font_cu12_t_cyrillic);
    u8g2.setCursor(0, 15);
    u8g2.printf("Кімната: %.2f C", temperature);
    u8g2.setCursor(0, 30);
    u8g2.printf("Волога: %.2f %%", humidity);
    u8g2.setCursor(0, 45);
    u8g2.printf("Тиск: %.2f hPa", pressure);
    // Видалено: u8g2.drawFrame(0, 10, 128, 50); // Рамка навколо блоку
    u8g2.sendBuffer();
}


void sendTemperatureAndHumidityData(float temperature, float humidity, float pressure) {
    char tempMsg[20], humiMsg[20], pressureMsg[20];
    sprintf(tempMsg, "05%.2f", temperature);
    sprintf(humiMsg, "06%.2f", humidity);
    sprintf(pressureMsg, "07%.2f", pressure);
    mesh.sendBroadcast(tempMsg);
    mesh.sendBroadcast(humiMsg);
    mesh.sendBroadcast(pressureMsg);
    Serial.printf("Sent to mesh: Temperature: %.2f °C, Humidity: %.2f %%, Pressure: %.2f hPa\n", temperature, humidity, pressure); // Debugging output
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
    // Видалено рамку для секундоміра
    // u8g2.drawFrame(0, 10, 128, 15); // Видалено
    u8g2.sendBuffer();

    // Оновлення Serial монітора, якщо інтервал пройшов
    if (millis() - lastSerialUpdateTime >= serialUpdateInterval && seconds != lastPrintedSecond) {
        Serial.printf("Stopwatch time: %02d:%02d\n", minutes, seconds);
        lastSerialUpdateTime = millis();
        lastPrintedSecond = seconds;
    }

    // Зчитування стану кнопки для повернення
    int reading = digitalRead(BUTTON_PIN);
    if (reading != lastButtonState) {
        lastDebounceTime = millis();
    }

    // Перевірка стану кнопки для повернення
    if ((millis() - lastDebounceTime) > debounceDelay) {
        if (reading != buttonState) {
            buttonState = reading;
            if (buttonState == LOW) {
                inSubMenu = false; // Повернення до головного меню
                Serial.println("Button pressed. Returning to main menu.");
            }
        }
    }
    lastButtonState = reading;
}


void showImage() {
    u8g2.clearBuffer();
    u8g2.drawBitmap(0, 0, 16, 128, image);
    u8g2.sendBuffer();
}
