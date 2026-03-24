/*
 * ============================================================
 *  3D Footprint Scanner – ESP32 WROOM
 *  WebSocket Server (AP Mode)
 * ============================================================
 *
 *  Peripherals:
 *    - Stepper Motor via DM556 (STEP/DIR)
 *    - 2× Limit Switches (INPUT_PULLUP)
 *    - Waveshare Laser Range Sensor (Serial2, UART)
 *    - 2× Physical Buttons (INPUT_PULLUP)
 *
 *  Libraries needed:
 *    - arduinoWebSockets  (Markus Sattler)
 *    - ArduinoJson        (Benoit Blanchon)
 *
 *  WebSocket URL  : ws://192.168.4.1:81
 *  Auth           : send {"secret": "123456"}
 *  Commands       : {"cmd":"start"}
 *                   {"cmd":"stop"}
 *                   {"cmd":"set_speed","rpm":45.0}
 * ============================================================
 *
 *  Comment out the line below to use REAL hardware sensors.
 *  When defined, all sensor reads return plausible random values.
 */
#define USE_DUMMY_DATA

// ── Includes ─────────────────────────────────────────────────
#include "ESP32_WS_Server.h"
#include <ArduinoJson.h>
#include <WiFi.h>
#include <Wire.h>



// ── Hardware Pins ─────────────────────────────────────────────
// LEDs
const int PIN_LED_TX = 2;
const int PIN_LED_CONN = 3;

// Stepper (STEP / DIR)
const int PIN_STEP = 16;
const int PIN_DIR = 17;

// Limit switches (active LOW, INPUT_PULLUP)
const int PIN_LIM_LOW = 18;  // lower limit (descend stop)
const int PIN_LIM_HIGH = 19; // upper limit (ascend stop)

// Physical buttons (active LOW, INPUT_PULLUP)
const int PIN_BTN_START = 21;
const int PIN_BTN_STOP = 22;

// Waveshare Laser Range Sensor – Serial2
const int PIN_LASER_RX = 13;
const int PIN_LASER_TX = 14;
const long LASER_BAUD = 115200;

// ── Motor Configuration ───────────────────────────────────────
const int STEPS_PER_REV = 200; // 1.8° motor
const int MICROSTEPPING = 64;  // DM556 DIP switch setting
float motorRpm = 30.0f;        // runtime-adjustable via WebSocket

// ── Timing & durations ───────────────────────────────────────
const unsigned long BROADCAST_INTERVAL_MS = 1000; // data broadcast cadence
const unsigned long PAUSE_DURATION_MS = 5000;     // dwell at bottom (ms)
const unsigned long BTN_DEBOUNCE_MS = 50;

// ── WiFi / WebSocket ─────────────────────────────────────────
const char ssid[] = "Tyre Footprint Scanner";
const char pass[] = "99999998";
ESP32_WS_Server wsServer(82);



// ── State Machine ────────────────────────────────────────────
enum State { IDLE, PREPARING, DESCENDING, PAUSED, ASCENDING };
State state = IDLE;

// ── Live sensor values ───────────────────────────────────────
float x = 0.0;
float y = 0.0;
float depth_mm = 0.0; // from laser range sensor (mm)

// ── Timing ───────────────────────────────────────────────────
unsigned long lastBroadcastMs = 0;
unsigned long pauseStartMs = 0;

// Button edge-detection state
bool prevBtnStart = HIGH;
bool prevBtnStop = HIGH;

// ─────────────────────────────────────────────────────────────
//  Position coordinates
// ─────────────────────────────────────────────────────────────
void updateXY() {
  // TODO: Implement logic to update x and y here
}

// ─────────────────────────────────────────────────────────────
//  Laser range sensor (Waveshare, Serial2, UART)
//  Protocol: send 0x01 0x06 0x00 0x00 0x01 0x00 0x21 0x85
//  Response: 9 bytes, distance in bytes [3–4] as uint16 (mm)
// ─────────────────────────────────────────────────────────────
float readDepth() {
#ifndef USE_DUMMY_DATA
  const uint8_t cmd[] = {0x01, 0x06, 0x00, 0x00, 0x01, 0x00, 0x21, 0x85};
  Serial2.write(cmd, sizeof(cmd));
  delay(20);
  if (Serial2.available() >= 9) {
    uint8_t buf[9];
    Serial2.readBytes(buf, 9);
    uint16_t dist = ((uint16_t)buf[3] << 8) | buf[4];
    return (float)dist; // mm
  }
  return depth_mm;
#else
  return (float)random(0, 3000); // 0–3000 mm range
#endif
}

// ─────────────────────────────────────────────────────────────
//  Stepper helpers
// ─────────────────────────────────────────────────────────────
float stepDelayUs() {
  // Half-period (µs) for one STEP pulse at motorRpm
  float stepsPerSec = (STEPS_PER_REV * MICROSTEPPING * motorRpm) / 60.0f;
  return 1000000.0f / (2.0f * stepsPerSec);
}

// Move one step; return false if limit switch hit (stops immediately)
bool stepOnce(int dir, int limitPin) {
  if (digitalRead(limitPin) == LOW)
    return false; // limit hit
  digitalWrite(PIN_DIR, dir);
  float d = stepDelayUs();
  digitalWrite(PIN_STEP, HIGH);
  delayMicroseconds((unsigned long)d);
  digitalWrite(PIN_STEP, LOW);
  delayMicroseconds((unsigned long)d);
  return true;
}


// ─────────────────────────────────────────────────────────────
//  JSON payload builder (used by both real and dummy broadcast)
// ─────────────────────────────────────────────────────────────
void buildPayload(char *buf, size_t len) {
  StaticJsonDocument<320> doc;
  doc["state"] = (int)state;
  doc["x"] = x;
  doc["y"] = y;
  doc["depth"] = (float)depth_mm;
  doc["timestamp"] = millis();
  serializeJson(doc, buf, len);
}

// Broadcast to all authenticated WS clients
void broadcastPayload() {
  if (!wsServer.hasAuthenticatedClients())
    return;

  updateXY();
  depth_mm = readDepth();

  char buf[320];
  buildPayload(buf, sizeof(buf));

  digitalWrite(PIN_LED_TX, HIGH);
  wsServer.broadcastTXT(buf);
  Serial.print("[TX] ");
  Serial.println(buf);
  delay(10);
  digitalWrite(PIN_LED_TX, LOW);
}

// ─────────────────────────────────────────────────────────────
//  Dummy data mode – same broadcast path, but also usable in IDLE
// ─────────────────────────────────────────────────────────────
void sendDummyData() {
  broadcastPayload(); // buildPayload() uses readLoad()/readDepth() which are
                      // already stubbed in USE_DUMMY_DATA mode
}

// ─────────────────────────────────────────────────────────────
//  WebSocket auth event
// ─────────────────────────────────────────────────────────────
void onAuthEvent(uint8_t clientId, bool authenticated) {
  digitalWrite(PIN_LED_CONN, wsServer.hasAuthenticatedClients() ? HIGH : LOW);
}

// ─────────────────────────────────────────────────────────────
//  WebSocket inbound command handler
// ─────────────────────────────────────────────────────────────
void onWsMessage(uint8_t clientId, const char *message) {
  StaticJsonDocument<128> doc;
  if (deserializeJson(doc, message) != DeserializationError::Ok)
    return;

  const char *cmd = doc["cmd"] | "";

  if (strcmp(cmd, "start") == 0) {
    if (state == IDLE) {
      Serial.println("[CMD] WS start received");
      state = PREPARING;
    }
  } else if (strcmp(cmd, "stop") == 0) {
    Serial.println("[CMD] WS stop received");
    state = IDLE;
    x = 0.0;
    y = 0.0;
  } else if (strcmp(cmd, "set_speed") == 0) {
    float newRpm = doc["rpm"] | motorRpm;
    if (newRpm > 0 && newRpm <= 200) {
      motorRpm = newRpm;
      Serial.printf("[CMD] Motor RPM set to %.1f\n", motorRpm);
    }
  }
}

// ─────────────────────────────────────────────────────────────
//  State machine
// ─────────────────────────────────────────────────────────────
void runStateMachine() {
  switch (state) {

  case IDLE:
    // GPS is updated in loop()
    break;

  case PREPARING:
    updateXY();
    Serial.println("[STATE] Prepared → DESCENDING");
    state = DESCENDING;
    break;

  case DESCENDING:
    // Move one step down; transition when lower limit hit
    if (!stepOnce(LOW, PIN_LIM_LOW)) {
      Serial.println("[STATE] Lower limit → PAUSED");
      pauseStartMs = millis();
      state = PAUSED;
      // Final reading at bottom
      broadcastPayload();
    } else {
      // Stream data continuously while moving
      unsigned long now = millis();
      if (now - lastBroadcastMs >= BROADCAST_INTERVAL_MS) {
        lastBroadcastMs = now;
        broadcastPayload();
      }
    }
    break;

  case PAUSED:
    if (millis() - pauseStartMs >= PAUSE_DURATION_MS) {
      Serial.println("[STATE] Pause done → ASCENDING");
      state = ASCENDING;
    }
    break;

  case ASCENDING:
    // Move one step up; transition when upper limit hit
    if (!stepOnce(HIGH, PIN_LIM_HIGH)) {
      Serial.println("[STATE] Upper limit → IDLE");
      state = IDLE;
      x = 0.0;
      y = 0.0; // reset coordinates on finish
    } else {
      unsigned long now = millis();
      if (now - lastBroadcastMs >= BROADCAST_INTERVAL_MS) {
        lastBroadcastMs = now;
        broadcastPayload();
      }
    }
    break;
  }
}

// ─────────────────────────────────────────────────────────────
//  Button read with debounce (rising-edge → active LOW)
// ─────────────────────────────────────────────────────────────
bool buttonPressed(int pin, bool &prevState) {
  bool cur = digitalRead(pin);
  if (prevState == HIGH && cur == LOW) {
    delay(BTN_DEBOUNCE_MS);
    cur = digitalRead(pin);
    if (cur == LOW) {
      prevState = cur;
      return true;
    }
  }
  prevState = cur;
  return false;
}

// ─────────────────────────────────────────────────────────────
//  Setup
// ─────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("\n=== 3D Footprint Scanner ===");

  // ── GPIO setup ───────────────────────────────────────────
  pinMode(PIN_LED_TX, OUTPUT);
  pinMode(PIN_LED_CONN, OUTPUT);
  pinMode(PIN_STEP, OUTPUT);
  pinMode(PIN_DIR, OUTPUT);
  pinMode(PIN_LIM_LOW, INPUT_PULLUP);
  pinMode(PIN_LIM_HIGH, INPUT_PULLUP);
  pinMode(PIN_BTN_START, INPUT_PULLUP);
  pinMode(PIN_BTN_STOP, INPUT_PULLUP);
  digitalWrite(PIN_LED_TX, LOW);
  digitalWrite(PIN_LED_CONN, LOW);
  digitalWrite(PIN_STEP, LOW);
  digitalWrite(PIN_DIR, LOW);

  // ── I2C ──────────────────────────────────────────────────
  Wire.begin();

  // ── Laser range serial ───────────────────────────────────
#ifndef USE_DUMMY_DATA
  Serial2.begin(LASER_BAUD, SERIAL_8N1, PIN_LASER_RX, PIN_LASER_TX);
  Serial.println("[LASER] Serial2 started.");
#else
  Serial.println("[LASER] DUMMY mode – simulated depth values.");
#endif

  // ── WiFi AP ──────────────────────────────────────────────
  Serial.printf("[WiFi] Starting AP: %s\n", ssid);
  if (!WiFi.softAP(ssid, pass)) {
    Serial.println("[ERROR] Failed to start AP.");
    while (true) {
      digitalWrite(PIN_LED_TX, !digitalRead(PIN_LED_TX));
      delay(300);
    }
  }
  delay(500);
  Serial.print("[WiFi] AP IP: ");
  Serial.println(WiFi.softAPIP());

  // ── WebSocket server ─────────────────────────────────────
  wsServer.setSecretCode("123456");
  wsServer.setMaxClients(8);
  wsServer.onAuthEvent(onAuthEvent);
  wsServer.onMessage(onWsMessage);
  wsServer.begin();
  Serial.println("[WS] Server started on ws://192.168.4.1:81");

  randomSeed(esp_random());

  Serial.println("=== Ready ===\n");
}

// ─────────────────────────────────────────────────────────────
//  Main Loop
// ─────────────────────────────────────────────────────────────
void loop() {
  wsServer.loop();

  // ── Physical buttons ─────────────────────────────────────
  if (buttonPressed(PIN_BTN_START, prevBtnStart)) {
    if (state == IDLE) {
      Serial.println("[BTN] Start pressed → PREPARING");
      state = PREPARING;
    }
  }
  if (buttonPressed(PIN_BTN_STOP, prevBtnStop)) {
    if (state != IDLE) {
      Serial.println("[BTN] Stop pressed → IDLE");
      state = IDLE;
      x = 0.0;
      y = 0.0;
    }
  }

  // ── State machine ────────────────────────────────────────
  runStateMachine();

  // ── Broadcast coordinates & depth (only while IDLE) ──────
  if (state == IDLE) {
    unsigned long now = millis();
    if (now - lastBroadcastMs >= BROADCAST_INTERVAL_MS) {
      lastBroadcastMs = now;
      updateXY();
      broadcastPayload();
    }
  }
}