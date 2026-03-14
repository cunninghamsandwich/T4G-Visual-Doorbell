#include "driver.h"
#include "driver/rtc_io.h"
#include "esp_sleep.h"
#include <RCSwitch.h>
#include <TFT_eSPI.h>

// ==================== SLEEP SETTINGS =================
// The specific pad on the back of the XIAO ESP32C6 is GPIO4
#define WAKEUP_GPIO GPIO_NUM_4 // Only RTC IO are allowed - GPIO0-7
// How long to stay awake looking for signal (in milliseconds)
#define RF_LISTEN_TIMEOUT 60000
RTC_DATA_ATTR int bootCount = 0; // Configure a boot counter
unsigned long wakeStartTime = 0;
// ==============================================================

// ==================== SCREEN ROTATION SETTING =================
// 0 = Portrait (0°)
// 1 = Landscape (90°)
// 2 = Portrait Inverted (180°)
// 3 = Landscape Inverted (270°)
// CHANGE THIS NUMBER if the screen is still not facing the right way
#define DISPLAY_ROTATION 3
// ==============================================================

// ==================== RF SETTINGS =============================
#define RECEIVE_PIN D4        // Using GPIO22 (D4) for RF receiver
#define BUTTON_CODE_1 4137576 // Your specific RF code (Button 2)
// ==============================================================

EPaper epaper;
RCSwitch mySwitch = RCSwitch();
// unsigned long lastTriggerTime = 0;
const unsigned long DISPLAY_DURATION = 10000; // 10 seconds
// bool messageDisplayed = false;

// ===========================================================
//                    SLEEP FUNCTIONS
// ===========================================================

void enterDeepSleep() {
  Serial.println("[SYSTEM] Preparing for Deep Sleep...");
  // 1. Put the display controller to sleep to save power and protect screen
  epaper.sleep();
  // 2. Configure Wakeup Pin
  // We use INPUT_PULLUP assuming the button connects the pin to GND
  pinMode(WAKEUP_GPIO, INPUT_PULLUP);
  // 3. Enable Wakeup
  // 1ULL << WAKEUP_GPIO creates the bitmask
  // ESP_GPIO_WAKEUP_GPIO_LOW means wake up when button is pressed (connected to
  // GND)
  esp_deep_sleep_enable_gpio_wakeup(1ULL << WAKEUP_GPIO,
                                    ESP_GPIO_WAKEUP_GPIO_LOW);
  Serial.println("[SYSTEM] Goodnight.");
  Serial.flush(); // Ensure serial message sends before processor cuts off
  esp_deep_sleep_start();
}

void print_wakeup_reason() {
  esp_sleep_wakeup_cause_t wakeup_reason;
  wakeup_reason = esp_sleep_get_wakeup_cause();
  switch (wakeup_reason) {
  case ESP_SLEEP_WAKEUP_EXT0:
    Serial.println("Wakeup caused by external signal using RTC_IO");
    break;
  case ESP_SLEEP_WAKEUP_EXT1:
    Serial.println("Wakeup caused by external signal using RTC_CNTL");
    break;
  case ESP_SLEEP_WAKEUP_TIMER:
    Serial.println("Wakeup caused by timer");
    break;
  case ESP_SLEEP_WAKEUP_TOUCHPAD:
    Serial.println("Wakeup caused by touchpad");
    break;
  case ESP_SLEEP_WAKEUP_ULP:
    Serial.println("Wakeup caused by ULP program");
    break;
  default:
    Serial.printf("Wakeup was not caused by deep sleep: %d\n", wakeup_reason);
    break;
  }
}

// ===========================================================
//                    DISPLAY FUNCTIONS
// ===========================================================

void setScreenWhite() {
  Serial.println("[DISPLAY] Cleaning screen...");
  epaper.fillRect(
      0, 0, epaper.width(), epaper.height(),
      TFT_WHITE); // Erase the screen by filling it with a white rectangle
  epaper.update();
  Serial.println("[DISPLAY] Screen cleared.");
}

void showMessage() {
  Serial.println("[DISPLAY] RF Signal Received -> Showing Message");
  setScreenWhite();
  // epaper.fillScreen(TFT_WHITE); // Clear screen to White
  epaper.setTextColor(TFT_BLACK, TFT_WHITE); // Set Text color, Background color
  epaper.setTextSize(3);                     // Set Text size
  epaper.setTextDatum(MC_DATUM);             // Set middle Center alignment
  epaper.drawString(
      "I'm coming", epaper.width() / 2,
      epaper.height() /
          2); // Draw the string in the exact center of the current rotation
  epaper.update();
  Serial.println("[DISPLAY] Message displayed.");
}

// ===========================================================
//                           SETUP
// ===========================================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  ++bootCount;

  Serial.println("\n\n========== Xiao ESP32-C6 SYSTEM START ==========");
  Serial.println("Boot number: " + String(bootCount));
  print_wakeup_reason();

  Serial.println("Board: Xiao ESP32-C6");
  Serial.println("Display: TFT_eSPI");
  Serial.println("RF: 433MHz Receiver");

  // 1. Initialize Display
  Serial.println("[DISPLAY] Initializing Display...");
  epaper.begin(); // Initialize epaper display
  epaper.setRotation(DISPLAY_ROTATION);
  // But don't draw anything yet! We don't want the screen flashing/refreshing
  // just because we woke up to listen.

  /* (this is the old screen refresh code)
  epaper.fillRect(0,0,epaper.width(),epaper.height(),TFT_WHITE); // Erase the
  screen by filling it with a white rectangle
  // Test the display with a simple pattern
  epaper.setTextColor(TFT_BLACK, TFT_WHITE);
  epaper.setTextSize(1);
  epaper.drawString("System Ready", 10, 10);
  epaper.update();
  */
  Serial.println("[DISPLAY] Display initialized.");

  // 2. Initialize RF
  Serial.print("[RF] Enabling Receiver on Pin: D");
  Serial.println(RECEIVE_PIN);
  mySwitch.enableReceive(
      digitalPinToInterrupt(RECEIVE_PIN)); // Initialize RCSwitch

  // 3. Start Timer
  wakeStartTime = millis();
  Serial.println("[SYSTEM] Setup Complete. Waiting 60s for RF signal...");
}

// ===========================================================
//                           LOOP
// ===========================================================
void loop() {
  // -------------------------------------------------
  // CONDITION 1: RF Signal Received
  // -------------------------------------------------
  if (mySwitch.available()) {
    unsigned long value = mySwitch.getReceivedValue();
    if (value != 0) {
      Serial.print("[RF] Signal Detected - Code: ");
      Serial.println(value);
      Serial.print("[RF] Bit length: ");
      Serial.println(mySwitch.getReceivedBitlength());
      if (value == BUTTON_CODE_1) {
        Serial.println("[RF] Valid Code! Updating Display...");
        showMessage();
        Serial.println("[SYSTEM] Waiting 10 seconds...");
        delay(DISPLAY_DURATION);
        setScreenWhite();
        enterDeepSleep();
      }
    }
    mySwitch.resetAvailable();
  }

  // -------------------------------------------------
  // CONDITION 2: 60 Second Timeout Reached
  // -------------------------------------------------
  if (millis() - wakeStartTime >= RF_LISTEN_TIMEOUT) {
    Serial.println("[TIMEOUT] No signal received in 60 seconds.");
    enterDeepSleep();
  }

  // Small delay to prevent watchdog issues
  delay(10);
}