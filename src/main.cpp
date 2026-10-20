#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

// --- ESP32 DevKit V1 TFT pins ---
#define TFT_CS   13
#define TFT_DC   27
#define TFT_MOSI 23
#define TFT_SCK  18

// --- Sensor & Button Pins ---
#define BTN_EMERGENCY   32
#define BTN_WATER       33
#define BTN_WASHROOM    25
#define BTN_FOOD        14
#define FIRE_SENSOR_PIN 12   // IR Flame Sensor Module (Digital DO)
#define STATUS_LED      4

// --- Initialize TFT & MPU6050 ---
Adafruit_ILI9341 tft = Adafruit_ILI9341(&SPI, TFT_DC, TFT_CS);
Adafruit_MPU6050 mpu;

// --- TFT Layout Dimensions ---
#define BTN_X 20
#define BTN_W 200
#define BTN_H 38

#define Y_FIRE      10
#define Y_EMERGENCY 55
#define Y_WATER     105
#define Y_FOOD      155
#define Y_WASHROOM  205

static const uint8_t buttonPins[] = {
  BTN_EMERGENCY,
  BTN_WATER,
  BTN_FOOD,
  BTN_WASHROOM,
};

static const char* const alertNames[] = {
  "EMERGENCY",
  "WATER",
  "FOOD",
  "WASHROOM",
};

// --- FUNCTION PROTOTYPES ---
void drawMainMenu();
void triggerAlert(const char* alertType);
int getPhysicalButtonAlert();
void checkSafetySensors();

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("BOOT: ESP32 SmartCare Triage & Safety Node");

  // Initialize Pins
  pinMode(BTN_EMERGENCY, INPUT_PULLUP);
  pinMode(BTN_WATER, INPUT_PULLUP);
  pinMode(BTN_WASHROOM, INPUT_PULLUP);
  pinMode(BTN_FOOD, INPUT_PULLUP);
  pinMode(FIRE_SENSOR_PIN, INPUT_PULLUP);
  pinMode(STATUS_LED, OUTPUT);
  digitalWrite(STATUS_LED, LOW);

  // Initialize I2C and MPU6050 (SDA=21, SCL=22)
  Wire.begin(21, 22);
  if (!mpu.begin()) {
    Serial.println("WARNING: MPU6050 not detected. Fall detection inactive.");
  } else {
    Serial.println("MPU6050 Initialized.");
    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  }

  // Initialize SPI Display
  SPI.begin(TFT_SCK, -1, TFT_MOSI, TFT_CS);
  tft.begin(8000000);
  tft.setRotation(0);
  
  drawMainMenu();
  digitalWrite(STATUS_LED, HIGH);
  Serial.println("System Ready.");
}

void drawMainMenu() {
  tft.fillScreen(ILI9341_BLACK);
  
  tft.setCursor(20, 10);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  tft.print("SMARTCARE");

  // Fire Hazard Banner
  tft.fillRoundRect(BTN_X, Y_FIRE, BTN_W, 30, 6, 0xFD20);
  tft.setCursor(BTN_X + 25, Y_FIRE + 8);
  tft.setTextSize(1);
  tft.print("FIRE & FALL SENSORS ACTIVE");

  // Emergency Button
  tft.fillRoundRect(BTN_X, Y_EMERGENCY, BTN_W, BTN_H, 8, ILI9341_RED);
  tft.setCursor(BTN_X + 45, Y_EMERGENCY + 12);
  tft.setTextSize(2);
  tft.print("EMERGENCY");

  // Water Button
  tft.fillRoundRect(BTN_X, Y_WATER, BTN_W, BTN_H, 8, ILI9341_BLUE);
  tft.setCursor(BTN_X + 75, Y_WATER + 12);
  tft.print("WATER");

  // Food Button
  tft.fillRoundRect(BTN_X, Y_FOOD, BTN_W, BTN_H, 8, ILI9341_ORANGE);
  tft.setCursor(BTN_X + 75, Y_FOOD + 12);
  tft.print("FOOD");

  // Washroom Button
  tft.fillRoundRect(BTN_X, Y_WASHROOM, BTN_W, BTN_H, 8, 0x07E0);
  tft.setCursor(BTN_X + 45, Y_WASHROOM + 12);
  tft.print("WASHROOM");
}

void triggerAlert(const char* alertType) {
  static unsigned long lastAlertMs = 0;
  unsigned long now = millis();

  if (now - lastAlertMs < 500) {
    return;
  }
  lastAlertMs = now;

  Serial.print("ALERT:");
  Serial.println(alertType);
  
  tft.fillScreen(ILI9341_BLACK);
  tft.setCursor(20, 140);
  tft.setTextColor(ILI9341_RED);
  tft.setTextSize(3);
  tft.print("SENT!");
  
  delay(2000);
  drawMainMenu();
}

int getPhysicalButtonAlert() {
  static bool buttonState[4] = {false, false, false, false};

  for (uint8_t i = 0; i < 4; ++i) {
    bool isPressed = (digitalRead(buttonPins[i]) == LOW);

    if (isPressed && !buttonState[i]) {
      buttonState[i] = true;
      return i;
    }
    if (!isPressed) {
      buttonState[i] = false;
    }
  }
  return -1;
}

void checkSafetySensors() {
  static bool fireTriggered = false;
  static bool fallTriggered = false;

  // 1. Check IR Flame Sensor (Active LOW)
  if (digitalRead(FIRE_SENSOR_PIN) == LOW) {
    if (!fireTriggered) {
      fireTriggered = true;
      triggerAlert("FIRE_EMERGENCY");
    }
  } else {
    fireTriggered = false;
  }

  // 2. Check MPU6050 Fall Detection / Sudden Bed Motion
  sensors_event_t a, g, temp;
  if (mpu.getEvent(&a, &g, &temp)) {
    float accelMag = sqrt(a.acceleration.x * a.acceleration.x +
                          a.acceleration.y * a.acceleration.y +
                          a.acceleration.z * a.acceleration.z);

    // Threshold ~2.5G impact magnitude representing a fall
    if (accelMag > 25.0) {
      if (!fallTriggered) {
        fallTriggered = true;
        triggerAlert("FALL_DETECTED");
      }
    } else {
      fallTriggered = false;
    }
  }
}

void loop() {
  checkSafetySensors();

  int physicalIndex = getPhysicalButtonAlert();
  if (physicalIndex >= 0) {
    triggerAlert(alertNames[physicalIndex]);
    delay(150);
    return;
  }
}