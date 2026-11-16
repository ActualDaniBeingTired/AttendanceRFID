/*
  RFID Attendance System using ESP32 + MFRC522 + I2C LCD + Buzzer

  Pins:
  RFID  SDA(SS)=5  SCK=18  MISO=19  MOSI=23  RST=4  (3.3V!)
  LCD   SDA=21  SCL=22  (default I2C pins)
  Buzzer = 27

  USE_SERVER:
  false -> attendance is checked locally on the ESP32 (works right now)
  true  -> UID is POSTed to your backend, which replies with
  {"status":"ok|duplicate|unknown","name":"..."}
*/

#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <time.h>

#define USE_SERVER false

#if USE_SERVER
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
const char* SERVER_URL = "https://your-server.com/api/tap"; // Change server URL
const char* API_KEY    = "change-me"; // Change API KEY
#endif

// ---------- Pins ----------
#define SS_PIN     5
#define RST_PIN    4
#define BUZZER_PIN 27
#define SDA_PIN    21
#define SCL_PIN    22

// ---------- WI-FI Config ----------
const char* WIFI_SSID = "Wokwi-GUEST"; // <- change to actual WI-FI name
const char* WIFI_PASS = ""; // <- change to actual password that the WI-FI is currently in use
const char* DEVICE_ID = "door-1";

const long  GMT_OFFSET_SEC = 7 * 3600;   // Set as UTC+7 (WIB), Indonesia Timezone
const char* NTP_SERVER     = "pool.ntp.org";

const unsigned long WIFI_TIMEOUT_MS  = 15000;
const unsigned long MESSAGE_SHOW_MS  = 1800;

MFRC522 rfid(SS_PIN, RST_PIN);
LiquidCrystal_I2C lcd(0x27, 16, 2);      // Or change to 0x3F, since some models use this

// ---------- Local student table USE IF SERVER IS DOWN ----------
struct Student {
const char* uid;
const char* name;
String lastDate;   // "YYYY-MM-DD" of last recorded attendance
};

// FAKE DATA
Student students[] = {
  { "01020304", "Daniel", "" },
  { "11223344", "Andi",   "" },
};
const int STUDENT_COUNT = sizeof(students) / sizeof(students[0]);

enum Result { RES_OK, RES_DUPLICATE, RES_UNKNOWN, RES_ERROR };

bool timeSynced = false;
unsigned long messageShownAt = 0;
bool showingMessage = false;

// ---------- LCD ----------
void showMessage(const String& line1, const String& line2) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(line1.substring(0, 16));
  lcd.setCursor(0, 1);
  lcd.print(line2.substring(0, 16));
}

void showIdle() {
  showMessage("RFID Attendance", "Scan your card");
  showingMessage = false;
}

void showTimed(const String& l1, const String& l2) {
  showMessage(l1, l2);
  messageShownAt = millis();
  showingMessage = true;
}

// ---------- Buzzer patterns ---------- // If you want to change,
void beepSuccess() {            // one short, high beep
  tone(BUZZER_PIN, 2000, 150); // 2000Hz at 150ms
  delay(200);
}

void beepDuplicate() {          // two medium beeps
  for (int i = 0; i < 2; i++) {
    tone(BUZZER_PIN, 1200, 100);
    delay(180);
  }
}

void beepUnknown() {            // one long, low buzz
  tone(BUZZER_PIN, 400, 700);
  delay(750);
}

void beepError() {              // three fast beeps (network/server problem)
  for (int i = 0; i < 3; i++) {
  tone(BUZZER_PIN, 800, 60);
  delay(110);
  }
}

// ---------- WiFi & time ----------
bool connectWiFi() {
  showMessage("Connecting Wi-Fi", "Please wait...");
  Serial.print("Connecting to Wi-Fi");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_TIMEOUT_MS) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Wi-Fi connected, IP: ");
    Serial.println(WiFi.localIP());
    return true;
  }
  Serial.println("Wi-Fi FAILED");
  return false;
}


void syncTime() {
showMessage("Syncing time", "Please wait...");
configTime(GMT_OFFSET_SEC, 0, NTP_SERVER);

struct tm t;
  timeSynced = getLocalTime(&t, 8000);   // wait up to 8s
  Serial.println(timeSynced ? "Time synced" : "Time sync FAILED");
}

String getDateString() {
  struct tm t;
  if (!getLocalTime(&t, 200)) return "unknown";
  char buf[11];
  strftime(buf, sizeof(buf), "%Y-%m-%d", &t);
  return String(buf);
}

String getTimeString() {
  struct tm t;
  if (!getLocalTime(&t, 200)) return "--:--:--";
  char buf[9];
  strftime(buf, sizeof(buf), "%H:%M:%S", &t);
  return String(buf);
}

// Reconnect quietly if WiFi dropped (non-blocking)
void keepWiFiAlive() {
  static unsigned long lastTry = 0;
  if (WiFi.status() != WL_CONNECTED && millis() - lastTry > 10000) {
    lastTry = millis();
    Serial.println("WiFi lost, reconnecting...");
    WiFi.disconnect();
    WiFi.begin(WIFI_SSID, WIFI_PASS);
  }
}

// ---------- Attendance logic ----------
#if USE_SERVER

Result checkWithServer(const String& uid, String& name) {
  if (WiFi.status() != WL_CONNECTED) return RES_ERROR;

  WiFiClientSecure client;
  client.setInsecure();   // TESTING ONLY. For production, use client.setCACert(...)


  HTTPClient http;
  http.setTimeout(5000);
  if (!http.begin(client, SERVER_URL)) return RES_ERROR;

  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-API-Key", API_KEY);

  JsonDocument req;
  req["uid"]    = uid;
  req["device"] = DEVICE_ID;
  req["date"]   = getDateString();
  req["time"]   = getTimeString();
  String body;
  serializeJson(req, body);

  int code = http.POST(body);
  if (code != 200) {
  Serial.printf("HTTP error: %d\n", code);
  http.end();
  return RES_ERROR;
  }

  JsonDocument res;
  DeserializationError err = deserializeJson(res, http.getString());
  http.end();
  if (err) return RES_ERROR;

  name = res["name"] | "";
  String status = res["status"] | "";
  if (status == "ok")        return RES_OK;
  if (status == "duplicate") return RES_DUPLICATE;
  if (status == "unknown")   return RES_UNKNOWN;
  return RES_ERROR;
}

#else

Result checkLocally(const String& uid, String& name) {
String today = getDateString();

  for (int i = 0; i < STUDENT_COUNT; i++) {
    if (uid == students[i].uid) {
      name = students[i].name;
    if (students[i].lastDate == today) return RES_DUPLICATE;  // already today
      students[i].lastDate = today;
    return RES_OK;
    }
  }
  return RES_UNKNOWN;
  }

#endif


// ---------- Setup ----------
void setup() {
  Serial.begin(115200);

  pinMode(BUZZER_PIN, OUTPUT);

  Wire.begin(SDA_PIN, SCL_PIN);
  lcd.init();
  lcd.backlight();
  showMessage("RFID Attendance", "Starting...");

  SPI.begin(18, 19, 23, SS_PIN);
  rfid.PCD_Init();

  if (connectWiFi()) {
    syncTime();
  } else {
    showMessage("WiFi failed", "Offline mode");
    delay(1500);
  }


  showIdle();
  Serial.println("RFID Attendance Ready");
}

// ---------- Main loop ----------
void loop() {
  keepWiFiAlive();

  // Return to the idle screen after a message has been shown for a while
  if (showingMessage && millis() - messageShownAt > MESSAGE_SHOW_MS) {
    showIdle();
  }

  if (!rfid.PICC_IsNewCardPresent()) return;
  if (!rfid.PICC_ReadCardSerial())   return;

// Build UID string, e.g. "01020304"
  String uid = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) uid += "0";
      uid += String(rfid.uid.uidByte[i], HEX);
  }
  uid.toUpperCase();

  String timestamp = getDateString() + " " + getTimeString();
  Serial.println("Card UID: " + uid + "  @ " + timestamp);

  showMessage("Checking...", "");

  String name = "";
  #if USE_SERVER
    Result result = checkWithServer(uid, name);
  #else
    Result result = checkLocally(uid, name);
  #endif

  // Switch with 4 cases : First Tap, Duplicated Tap, Uknown Card, Error Card
  switch (result) {
    case RES_OK:
      showTimed(name, "Attendance OK");
      beepSuccess();
      Serial.println(name + ": PRESENT at " + timestamp);
    break;

    case RES_DUPLICATE:
      showTimed(name, "Already Recorded");
      beepDuplicate();
      Serial.println(name + ": DUPLICATE");
    break;

    case RES_UNKNOWN:
      showTimed("Unknown Card", "Access Denied");
      beepUnknown();
      Serial.println("UNKNOWN CARD: " + uid);
    break;

    case RES_ERROR:
      showTimed("System Error", "Try again");
      beepError();
      Serial.println("ERROR: could not process tap");
    break;
  }

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}



