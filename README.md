# WS_Cone_Penetrometer_ESP32

ESP32-based WebSocket server (AP mode) that streams soil penetrometer sensor data at 1 Hz to connected clients. Includes a modular WebSocket library and an HTML test client.

---

## Project Structure

```
WS_Cone_Penetrometer_ESP32/
├── WS_Cone_Penetrometer_ESP32.ino  ← Main sketch (data & JSON defined here)
├── ESP32_WS_Server.h               ← WebSocket library header
├── ESP32_WS_Server.cpp             ← WebSocket library implementation
├── index.html                      ← Browser-based test client
└── .gitignore
```

---

## ESP32_WS_Server Library

A data-structure-independent WebSocket server library for ESP32. It handles:
- WiFi AP mode setup (handled externally, see main sketch)
- WebSocket server on a configurable port
- Secret-code-based client authentication
- Broadcasting text payloads to all authenticated clients

### What it does NOT do
It does **not** define what data is sent. That is fully controlled by your main sketch.

---

## API Reference

### Include
```cpp
#include "ESP32_WS_Server.h"
```

### Constructor
```cpp
ESP32_WS_Server wsServer(81);   // port number, default: 81
```

### Configuration (call before `begin()`)
```cpp
wsServer.setSecretCode("123456");  // Authentication secret
wsServer.setMaxClients(8);         // Max concurrent clients (default: 8)
wsServer.onAuthEvent(myCallback);  // Optional: auth event callback
```

### Auth Event Callback
```cpp
void onAuthEvent(uint8_t clientId, bool authenticated) {
  // called when a client authenticates or disconnects
}
```

### Lifecycle
```cpp
wsServer.begin();   // Call once in setup()
wsServer.loop();    // Call every loop() iteration
```

### Sending Data
```cpp
wsServer.broadcastTXT("your json string here");  // const char*
// or
String payload = "{\"key\": \"value\"}";
wsServer.broadcastTXT(payload);                  // String&
```

### Checking State
```cpp
wsServer.hasAuthenticatedClients();  // Returns true if at least one client is authed
```

---

## Authentication Flow

1. Client connects to WebSocket
2. Client sends: `{"secret": "123456"}`
3. Server replies: `{"status": "connected"}`
4. Server starts broadcasting data every 1 second to authenticated clients

---

## Main Sketch Usage Example

```cpp
#include <WiFi.h>
#include <ArduinoJson.h>
#include "ESP32_WS_Server.h"

ESP32_WS_Server wsServer(81);

// Your data variables (modify as needed)
float temperature = 25.0;
float pressure    = 1013.0;

void setup() {
  Serial.begin(115200);
  WiFi.softAP("MyDevice", "password123");

  wsServer.setSecretCode("mySecret");
  wsServer.begin();
}

void loop() {
  wsServer.loop();

  if (wsServer.hasAuthenticatedClients()) {
    StaticJsonDocument<128> doc;
    doc["temperature"] = temperature;
    doc["pressure"]    = pressure;
    doc["timestamp"]   = millis();

    char buf[128];
    serializeJson(doc, buf);
    wsServer.broadcastTXT(buf);
    delay(1000);
  }
}
```

---

## Required Libraries

Install via Arduino Library Manager:
- **WebSockets** by Markus Sattler (`arduinoWebSockets`)
- **ArduinoJson** by Benoit Blanchon

---

## WebSocket URL

After connecting to the ESP32's WiFi AP:
```
ws://192.168.4.1:81
```

---

## Test Client

Open `index.html` in a browser (while connected to the ESP32 AP):
1. Click **Connect**
2. Click **Send Auth**
3. Live sensor data appears in the dashboard
