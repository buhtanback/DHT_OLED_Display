#include <Wire.h>
#include <U8g2lib.h>
#include <DHT.h>
#include "painlessMesh.h"

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

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, OLED_RESET);

int menuOption = 0;
int buttonState = 0;
int lastButtonState = 0;
unsigned long lastDebounceTime = 0;  // Час останнього зміни стану джойстика
unsigned long debounceDelay = 200;   // Затримка для debounce джойстика

bool inSubMenu = false; // Прапорець для відстеження чи ми в підменю

float lastTemperature = 0;
float lastHumidity = 0;

String currentDate = "2024-07-23";
int currentHour = 14;
int currentMinute = 30;
int currentSecond = 0;

void setup() {
  pinMode(BUTTON_PIN, INPUT_PULLUP); // Налаштування піну кнопки з pull-up
  u8g2.begin(); // Ініціалізація дисплея
  dht.begin();  // Ініціалізація датчика DHT
  Serial.begin(115200); // Ініціалізація Serial для діагностики
  
  // Ініціалізація мережі
  mesh.setDebugMsgTypes(ERROR | STARTUP | CONNECTION);  // Встановлення типів повідомлень
  mesh.init(MESH_PREFIX, MESH_PASSWORD, MESH_PORT);
  mesh.onReceive([](uint32_t from, String &msg) {
    Serial.printf("Received from %u msg=%s\n", from, msg.c_str());
  });
}

void loop() {
  mesh.update();  // Оновлення стану мережі
  
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
          Serial.print("Button pressed. Entering subMenu: ");
          Serial.println(menuOption);
        }
      }
    }
    lastButtonState = reading;

    // Відображення меню
    u8g2.clearBuffer(); // Очищення буфера дисплея
    u8g2.setFont(u8g2_font_cu12_t_cyrillic); // Вибір шрифту

    u8g2.setCursor(10, 20);
    if (menuOption == 0) {
      u8g2.print("> Кімната");
    } else {
      u8g2.print("  Кімната");
    }

    u8g2.setCursor(10, 40);
    if (menuOption == 1) {
      u8g2.print("> Вулиця");
    } else {
      u8g2.print("  Вулиця");
    }

    u8g2.setCursor(10, 60);
    if (menuOption == 2) {
      u8g2.print("> Секундомір");
    } else {
      u8g2.print("  Секундомір");
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
      u8g2.setCursor(10, 20);
      u8g2.print("Submenu Option 3");
      u8g2.setCursor(10, 40);
      u8g2.print("Press to return");
      u8g2.sendBuffer();
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
          inSubMenu = false;
          Serial.println("Button pressed. Returning to main menu.");
        }
      }
    }
    lastButtonState = reading;
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
    u8g2.printf("%s %02d:%02d:%02d", currentDate.c_str(), currentHour, currentMinute, currentSecond);
    u8g2.sendBuffer();

    // Only send data to the mesh and update serial monitor if there's a significant change
    if (abs(temperature - lastTemperature) >= 0.1 || abs(humidity - lastHumidity) >= 1.0) {
        Serial.printf("Temperature: %.2f °C, Humidity: %.2f %%\n", temperature, humidity);
        Serial.printf("Current Date and Time: %s %02d:%02d:%02d\n", currentDate.c_str(), currentHour, currentMinute, currentSecond);
        sendTemperatureAndHumidityData(temperature, humidity);
        lastTemperature = temperature;
        lastHumidity = humidity;
    }
}

void sendTemperatureAndHumidityData(float temperature, float humidity) {
    char tempMsg[20], humiMsg[20];
    sprintf(tempMsg, "05%.2f", temperature);
    sprintf(humiMsg, "06%.2f", humidity);
    mesh.sendBroadcast(tempMsg);
    mesh.sendBroadcast(humiMsg);
    Serial.printf("Sent to mesh: Temperature: %.2f °C, Humidity: %.2f %%\n", temperature, humidity); // Debugging output
}
