#ifndef ESP32_WS_SERVER_H
#define ESP32_WS_SERVER_H

#include <Arduino.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>

class ESP32_WS_Server {
public:
    ESP32_WS_Server(uint16_t port = 81);
    
    void begin();
    void loop();
    
    // Configure authentication
    void setSecretCode(const char* code);
    void setMaxClients(uint8_t count);
    
    // Broadcast data to all authenticated clients
    void broadcastTXT(const char* payload);
    void broadcastTXT(String& payload);
    
    // Check if anyone is connected/authenticated
    bool hasAuthenticatedClients();
    
    // Optional: Callback for authentication events
    typedef void (*AuthEventCallback)(uint8_t clientId, bool authenticated);
    void onAuthEvent(AuthEventCallback cb);

    // Callback for messages received from authenticated clients
    typedef void (*MessageCallback)(uint8_t clientId, const char* message);
    void onMessage(MessageCallback cb);

private:
    WebSocketsServer _webSocket;
    String _secretCode = "123456";
    uint8_t _maxClients = 8;
    bool* _authenticatedClients;
    AuthEventCallback _authCallback = nullptr;
    MessageCallback _messageCallback = nullptr;

    void _onEvent(uint8_t clientId, WStype_t type, uint8_t* payload, size_t length);
    static ESP32_WS_Server* _instance;
    static void _staticEventWrapper(uint8_t clientId, WStype_t type, uint8_t* payload, size_t length);
};

#endif
