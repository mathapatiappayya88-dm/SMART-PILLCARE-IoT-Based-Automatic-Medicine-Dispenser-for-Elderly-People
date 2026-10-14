#define BLYNK_PRINT Serial

#define BLYNK_TEMPLATE_ID   "TMPL3MBBw0v7q"
#define BLYNK_TEMPLATE_NAME "TABLET DISPENSER"
#define BLYNK_AUTH_TOKEN    "qBZyKjcUK2oBUxfJe6ckLz6I0W-emj7k"

#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <ESP32Servo.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <time.h>

// ============================================================
// MULTIPLE WIFI NETWORKS
// ============================================================

const char* wifiSSID[] = {
  "Darshan M",
  "OPPO A3 5G",
  "POCOC50",
  "AndroidAP_4116",
  "vivo Y22"
};

const char* wifiPASS[] = {
  "12345678",
  "laxmi@000",
  "Manya. M@13",
  "88998899",
  "Chamak@20"
};

const int WIFI_COUNT = 5;

// ============================================================
// PINS
// ============================================================

#define BUZZER_PIN 18
#define SERVO_PIN  19

// Physical switches
const int BTN_PIN[3] = {25, 26, 27};

// Servo position for each tablet
const int SERVO_ANGLE[3] = {45, 90, 135};

// ============================================================
// OBJECTS
// ============================================================

LiquidCrystal_I2C lcd(0x27, 16, 2);
Servo dispenserServo;

// ============================================================
// TIME - INDIA
// ============================================================

const long gmtOffset_sec = 19800;   // UTC +5:30
const int daylightOffset_sec = 0;

// ============================================================
// TABLET STATE
// ============================================================

bool taken[3] = {false, false, false};

int storedDay = -1;

unsigned long lcdHoldUntil = 0;
unsigned long lastClockUpdate = 0;
unsigned long lastBlynkTry = 0;

// ============================================================
// FORMAT TIME
// ============================================================

String formatTime12(struct tm &t)
{
  const char *ampm = (t.tm_hour >= 12) ? "PM" : "AM";

  int h = t.tm_hour % 12;

  if (h == 0)
    h = 12;

  char buf[12];

  snprintf(
    buf,
    sizeof(buf),
    "%02d:%02d %s",
    h,
    t.tm_min,
    ampm
  );

  return String(buf);
}

// ============================================================
// GET CURRENT TIME
// ============================================================

String getTimeString()
{
  struct tm t;

  if (getLocalTime(&t, 50))
  {
    return formatTime12(t);
  }

  return "--:--";
}

// ============================================================
// LCD
// ============================================================

void showLCD(String line1, String line2)
{
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print(line1.substring(0, 16));

  lcd.setCursor(0, 1);
  lcd.print(line2.substring(0, 16));
}

// ============================================================
// COUNT REMAINING TABLETS
// ============================================================

int slotsLeft()
{
  int count = 0;

  for (int i = 0; i < 3; i++)
  {
    if (!taken[i])
      count++;
  }

  return count;
}

// ============================================================
// UPDATE BLYNK
// ============================================================

void updateBlynkData(const char *status)
{
  if (!Blynk.connected())
    return;

  // V0 = tablets remaining
  Blynk.virtualWrite(V0, slotsLeft());

  // V1 = status
  Blynk.virtualWrite(V1, status);

  // V2 = current time
  Blynk.virtualWrite(V2, getTimeString());
}

// ============================================================
// BLYNK FRIENDLY DELAY
// ============================================================

void waitWithBlynk(unsigned long ms)
{
  unsigned long start = millis();

  while (millis() - start < ms)
  {
    if (Blynk.connected())
      Blynk.run();

    delay(10);
  }
}

// ============================================================
// BUZZER
// ============================================================

void beepBuzzer(unsigned long durationMs)
{
  unsigned long start = millis();

  while (millis() - start < durationMs)
  {
    digitalWrite(BUZZER_PIN, HIGH);

    waitWithBlynk(200);

    digitalWrite(BUZZER_PIN, LOW);

    waitWithBlynk(200);
  }

  digitalWrite(BUZZER_PIN, LOW);
}

// ============================================================
// SEND BLYNK EMAIL + NOTIFICATION
// ============================================================

void sendTabletNotification(int tabletNumber, String timeNow)
{
  String eventCode;

  // Select event according to switch
  if (tabletNumber == 1)
  {
    eventCode = "tablet_1";
  }
  else if (tabletNumber == 2)
  {
    eventCode = "tablet_2";
  }
  else
  {
    eventCode = "tablet_3";
  }

  String message =
    "Tablet " +
    String(tabletNumber) +
    " dispensed successfully at " +
    timeNow +
    ". Tablets remaining: " +
    String(slotsLeft());

  if (Blynk.connected())
  {
    // Send Blynk event
    Blynk.logEvent(eventCode, message);

    Serial.println("--------------------------------");
    Serial.println("BLYNK EVENT SENT");
    Serial.println("Event: " + eventCode);
    Serial.println("Message: " + message);
    Serial.println("--------------------------------");
  }
  else
  {
    Serial.println("Blynk OFFLINE");
    Serial.println("Notification/email not sent");
  }
}

// ============================================================
// DISPENSE TABLET
// ============================================================

void dispenseTablet(int i)
{
  int tabletNumber = i + 1;

  Serial.println();
  Serial.println("================================");
  Serial.println("BUTTON PRESSED");
  Serial.println("Tablet: " + String(tabletNumber));
  Serial.println("================================");

  // ----------------------------------------------------------
  // CHECK IF ALREADY TAKEN
  // ----------------------------------------------------------

  if (taken[i])
  {
    showLCD(
      "Tablet " + String(tabletNumber),
      "Already taken"
    );

    // Optional notification for repeated press
    if (Blynk.connected())
    {
      Blynk.logEvent(
        "medicine_alert",
        "Tablet " + String(tabletNumber) +
        " was already dispensed."
      );
    }

    lcdHoldUntil = millis() + 3000;

    return;
  }

  // Mark tablet as taken
  taken[i] = true;

  // Current time
  String timeNow = getTimeString();

  // ----------------------------------------------------------
  // LCD - DISPENSING
  // ----------------------------------------------------------

  showLCD(
    "Dispensing...",
    "Tablet " + String(tabletNumber)
  );

  // ----------------------------------------------------------
  // SERVO
  // ----------------------------------------------------------

  Serial.println(
    "Servo moving to " +
    String(SERVO_ANGLE[i]) +
    " degrees"
  );

  dispenserServo.write(SERVO_ANGLE[i]);

  waitWithBlynk(1000);

  // ----------------------------------------------------------
  // SEND EMAIL + NOTIFICATION
  // ----------------------------------------------------------

  sendTabletNotification(
    tabletNumber,
    timeNow
  );

  // ----------------------------------------------------------
  // WAIT
  // ----------------------------------------------------------

  waitWithBlynk(3000);

  // ----------------------------------------------------------
  // LCD RESULT
  // ----------------------------------------------------------

  showLCD(
    timeNow,
    "Pill " + String(tabletNumber) + " Dispensed"
  );

  // ----------------------------------------------------------
  // UPDATE BLYNK
  // ----------------------------------------------------------

  updateBlynkData("Pill dispensed");

  // ----------------------------------------------------------
  // BUZZER
  // ----------------------------------------------------------

  Serial.println("Buzzer ON");

  beepBuzzer(5000);

  Serial.println("Buzzer OFF");

  // Keep message on LCD
  lcdHoldUntil = millis() + 5000;
}

// ============================================================
// CHECK PHYSICAL SWITCHES
// ============================================================

void checkButtons()
{
  for (int i = 0; i < 3; i++)
  {
    // Button pressed
    if (digitalRead(BTN_PIN[i]) == LOW)
    {
      delay(30);   // debounce

      if (digitalRead(BTN_PIN[i]) == LOW)
      {
        // Dispense corresponding tablet
        dispenseTablet(i);

        // Wait until button is released
        while (digitalRead(BTN_PIN[i]) == LOW)
        {
          if (Blynk.connected())
            Blynk.run();

          delay(10);
        }

        delay(50);
      }
    }
  }
}

// ============================================================
// CLOCK + DAILY REFILL
// ============================================================

void updateClockAndLCD()
{
  if (millis() - lastClockUpdate < 1000)
    return;

  lastClockUpdate = millis();

  struct tm t;

  bool ok = getLocalTime(&t, 50);

  // ----------------------------------------------------------
  // NEW DAY = REFILL TABLETS
  // ----------------------------------------------------------

  if (ok)
  {
    if (storedDay == -1)
    {
      storedDay = t.tm_mday;
    }

    if (t.tm_mday != storedDay)
    {
      storedDay = t.tm_mday;

      for (int i = 0; i < 3; i++)
      {
        taken[i] = false;
      }

      Serial.println("New day detected.");
      Serial.println("All tablets refilled.");

      updateBlynkData("Daily refill");
    }
  }

  // Don't overwrite dispensing message
  if (millis() < lcdHoldUntil)
    return;

  // ----------------------------------------------------------
  // NORMAL LCD
  // ----------------------------------------------------------

  if (ok)
  {
    showLCD(
      formatTime12(t),
      "Left: " + String(slotsLeft())
    );
  }
  else
  {
    showLCD(
      "Syncing time...",
      "Check WiFi"
    );
  }
}

// ============================================================
// BLYNK V4 = RESET / REFILL
// ============================================================

BLYNK_WRITE(V4)
{
  if (param.asInt() == 1)
  {
    // Reset all tablets
    for (int i = 0; i < 3; i++)
    {
      taken[i] = false;
    }

    // Return servo to home
    dispenserServo.write(0);

    // LCD
    showLCD(
      "Reset Done",
      "3 tablets left"
    );

    Serial.println("================================");
    Serial.println("TABLETS RESET");
    Serial.println("All 3 tablets available");
    Serial.println("================================");

    // Update Blynk
    updateBlynkData("Reset - Ready");

    lcdHoldUntil = millis() + 2000;

    // Reset Blynk switch
    Blynk.virtualWrite(V4, 0);
  }
}

// ============================================================
// BLYNK CONNECTED
// ============================================================

BLYNK_CONNECTED()
{
  Serial.println("================================");
  Serial.println("BLYNK CONNECTED");
  Serial.println("================================");

  updateBlynkData("Ready");
}

// ============================================================
// CONNECT TO ANY AVAILABLE WIFI
// ============================================================

bool connectToAvailableWiFi()
{
  for (int i = 0; i < WIFI_COUNT; i++)
  {
    Serial.println();
    Serial.println("--------------------------------");
    Serial.println("Trying WiFi:");
    Serial.println(wifiSSID[i]);
    Serial.println("--------------------------------");

    showLCD(
      "Trying WiFi...",
      String(wifiSSID[i]).substring(0, 16)
    );

    WiFi.disconnect(true);
    delay(500);

    WiFi.begin(
      wifiSSID[i],
      wifiPASS[i]
    );

    unsigned long startTime = millis();

    while (
      WiFi.status() != WL_CONNECTED &&
      millis() - startTime < 10000
    )
    {
      delay(500);
      Serial.print(".");
    }

    Serial.println();

    if (WiFi.status() == WL_CONNECTED)
    {
      Serial.println("WiFi CONNECTED!");
      Serial.println("Network: " + String(wifiSSID[i]));
      Serial.println("IP: " + WiFi.localIP().toString());

      showLCD(
        "WiFi Connected",
        String(wifiSSID[i]).substring(0, 16)
      );

      return true;
    }

    Serial.println("Connection failed.");
  }

  Serial.println();
  Serial.println("================================");
  Serial.println("NO WIFI NETWORK AVAILABLE");
  Serial.println("================================");

  showLCD(
    "WiFi Failed",
    "Offline Mode"
  );

  return false;
}

// ============================================================
// SETUP
// ============================================================

void setup()
{
  Serial.begin(115200);

  delay(300);

  // ----------------------------------------------------------
  // BUZZER
  // ----------------------------------------------------------

  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(BUZZER_PIN, LOW);

  // ----------------------------------------------------------
  // BUTTONS
  // ----------------------------------------------------------

  for (int i = 0; i < 3; i++)
  {
    pinMode(
      BTN_PIN[i],
      INPUT_PULLUP
    );
  }

  // ----------------------------------------------------------
  // LCD
  // ----------------------------------------------------------

  Wire.begin(21, 22);

  lcd.init();

  lcd.backlight();

  showLCD(
    "Medicine System",
    "Starting..."
  );

  // ----------------------------------------------------------
  // SERVO
  // ----------------------------------------------------------

  ESP32PWM::allocateTimer(0);

  dispenserServo.setPeriodHertz(50);

  dispenserServo.attach(
    SERVO_PIN,
    500,
    2400
  );

  // Home position
  dispenserServo.write(0);

  // ----------------------------------------------------------
  // WIFI
  // ----------------------------------------------------------

  showLCD(
    "Connecting WiFi",
    "Please wait"
  );

  bool wifiConnected = connectToAvailableWiFi();

  if (wifiConnected)
  {
    Serial.println("Internet connection available.");
  }
  else
  {
    Serial.println("Running in offline mode.");
  }

  delay(1000);

  // ----------------------------------------------------------
  // NTP TIME
  // ----------------------------------------------------------

  configTime(
    gmtOffset_sec,
    daylightOffset_sec,
    "pool.ntp.org",
    "time.nist.gov"
  );

  // ----------------------------------------------------------
  // BLYNK
  // ----------------------------------------------------------

  Blynk.config(
    BLYNK_AUTH_TOKEN
  );

  if (WiFi.status() == WL_CONNECTED)
  {
    Blynk.connect(5000);
  }
}

// ============================================================
// LOOP
// ============================================================

void loop()
{
  // ----------------------------------------------------------
  // BLYNK
  // ----------------------------------------------------------

  if (Blynk.connected())
  {
    Blynk.run();
  }
  else if (
    WiFi.status() == WL_CONNECTED &&
    millis() - lastBlynkTry > 30000
  )
  {
    lastBlynkTry = millis();

    Serial.println(
      "Trying to reconnect Blynk..."
    );

    Blynk.connect(3000);
  }

  // ----------------------------------------------------------
  // PHYSICAL BUTTONS
  // ----------------------------------------------------------

  checkButtons();

  // ----------------------------------------------------------
  // CLOCK / LCD
  // ----------------------------------------------------------

  updateClockAndLCD();
}