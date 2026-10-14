#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
                                    
// --- ESP32 DevKit V1 TFT pins (from diagram.json) ---
#define TFT_CS   13
#define TFT_DC   27
#define TFT_MOSI 23
#define TFT_SCK  18

// --- ESP32 DevKit V1 physical button pins (from diagram.json) ---
#define BTN_EMERGENCY 32
#define BTN_WATER     33
#define BTN_WASHROOM  25
#define BTN_FOOD      14
#define STATUS_LED    4

// --- Initialize TFT ---
Adafruit_ILI9341 tft = Adafruit_ILI9341(&SPI, TFT_DC, TFT_CS);

// --- TFT Button Coordinates & Dimensions ---
#define BTN_X 20
#define BTN_W 200
#define BTN_H 45

#define Y_EMERGENCY 40
#define Y_WATER     95
#define Y_FOOD      150
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

// --- FUNCTION PROTOTYPES (Required for PlatformIO) ---
void drawMainMenu();
void triggerAlert(const char* alertType);
int getPhysicalButtonAlert();

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("BOOT: ESP32 patient alert firmware");

  // Initialize Physical Buttons with Internal Pull-Ups
  pinMode(BTN_EMERGENCY, INPUT_PULLUP);
  pinMode(BTN_WATER, INPUT_PULLUP);
  pinMode(BTN_WASHROOM, INPUT_PULLUP);
  pinMode(BTN_FOOD, INPUT_PULLUP);
  pinMode(STATUS_LED, OUTPUT);
  digitalWrite(STATUS_LED, LOW);

  // Use the same explicit VSPI pins that are wired in diagram.json.
  SPI.begin(TFT_SCK, -1, TFT_MOSI, TFT_CS);
  Serial.println("BOOT: starting ILI9341");
  tft.begin(8000000);
  Serial.println("BOOT: ILI9341 initialized");
  tft.setRotation(0); // Portrait mode (240x320)
  Serial.println("BOOT: rotation set");
  
  drawMainMenu();
  Serial.println("BOOT: menu drawn");
  digitalWrite(STATUS_LED, HIGH);
  Serial.println("BOOT: status LED on");
  Serial.println("System Ready.");
}

void drawMainMenu() {
  tft.fillScreen(ILI9341_BLACK);
  
  // Title
  tft.setCursor(25, 10);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  tft.print("PATIENT TRIAGE");

  // 1. Emergency Button (Red)
  tft.fillRoundRect(BTN_X, Y_EMERGENCY, BTN_W, BTN_H, 10, ILI9341_RED);
  tft.setCursor(BTN_X + 40, Y_EMERGENCY + 15);
  tft.print("EMERGENCY");

  // 2. Water Button (Blue)
  tft.fillRoundRect(BTN_X, Y_WATER, BTN_W, BTN_H, 10, ILI9341_BLUE);
  tft.setCursor(BTN_X + 65, Y_WATER + 15);
  tft.print("WATER");

  // 3. Food Button (Orange)
  tft.fillRoundRect(BTN_X, Y_FOOD, BTN_W, BTN_H, 10, ILI9341_ORANGE);
  tft.setCursor(BTN_X + 75, Y_FOOD + 15);
  tft.print("FOOD");

  // 4. Washroom Button (Green)
  tft.fillRoundRect(BTN_X, Y_WASHROOM, BTN_W, BTN_H, 10, 0x07E0);
  tft.setCursor(BTN_X + 50, Y_WASHROOM + 15);
  tft.print("WASHROOM");
}

void triggerAlert(const char* alertType) {
  static unsigned long lastAlertMs = 0;
  unsigned long now = millis();

  // Prevent retriggering while the same input remains active.
  if (now - lastAlertMs < 500) {
    return;
  }
  lastAlertMs = now;

  // Transmit the alert over Serial for the Python backend
  Serial.print("ALERT:");
  Serial.println(alertType);
  
  // Provide visual feedback to the patient
  tft.fillScreen(ILI9341_BLACK);
  tft.setCursor(30, 150);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(3);
  tft.print("SENT!");
  
  delay(2000); // Lock screen for 2 seconds
  drawMainMenu(); // Redraw the UI
}

int getPhysicalButtonAlert() {
  static bool buttonState[4] = {false, false, false, false};

  for (uint8_t i = 0; i < 4; ++i) {
    bool isPressed = (digitalRead(buttonPins[i]) == LOW);

    if (isPressed && !buttonState[i]) {
      buttonState[i] = true;
      Serial.print("Button pressed: ");
      Serial.println(alertNames[i]);
      return i;
    }

    if (!isPressed) {
      buttonState[i] = false;
    }
  }

  return -1;
}

void loop() {
  static unsigned long lastStatusMs = 0;
  if (millis() - lastStatusMs >= 2000) {
    Serial.print("STATUS: buttons emergency/water/washroom/food=");
    Serial.print(digitalRead(BTN_EMERGENCY));
    Serial.print(',');
    Serial.print(digitalRead(BTN_WATER));
    Serial.print(',');
    Serial.print(digitalRead(BTN_WASHROOM));
    Serial.print(',');
    Serial.println(digitalRead(BTN_FOOD));
    lastStatusMs = millis();
  }

  // Check physical pushbuttons (single trigger per press).
  int physicalIndex = getPhysicalButtonAlert();
  if (physicalIndex >= 0) {
    triggerAlert(alertNames[physicalIndex]);
    delay(150); // Debounce after a physical button press
    return;
  }
}