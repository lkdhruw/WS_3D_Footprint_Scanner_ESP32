#include "ESP32_WS_Server.h"

ESP32_WS_Server* ESP32_WS_Server::_instance = nullptr;

ESP32_WS_Server::ESP32_WS_Server(uint16_t port) : _webSocket(port) {
    _instance = this;
    _authenticatedClients = new bool[_maxClients];
    for (int i = 0; i < _maxClients; i++) _authenticatedClients[i] = false;
}

void ESP32_WS_Server::begin() {
    _webSocket.begin();
    _webSocket.onEvent(_staticEventWrapper);
    Serial.println("[WS Manager] Server started.");
}

void ESP32_WS_Server::loop() {
    _webSocket.loop();
}

void ESP32_WS_Server::setSecretCode(const char* code) {
    _secretCode = String(code);
}

void ESP32_WS_Server::setMaxClients(uint8_t count) {
    if (count > 0) {
        delete[] _authenticatedClients;
        _maxClients = count;
        _authenticatedClients = new bool[_maxClients];
        for (int i = 0; i < _maxClients; i++) _authenticatedClients[i] = false;
    }
}

void ESP32_WS_Server::onAuthEvent(AuthEventCallback cb) {
    _authCallback = cb;
}

void ESP32_WS_Server::onMessage(MessageCallback cb) {
    _messageCallback = cb;
}

bool ESP32_WS_Server::hasAuthenticatedClients() {
    for (int i = 0; i < _maxClients; i++) {
        if (_authenticatedClients[i]) return true;
    }
    return false;
}

void ESP32_WS_Server::broadcastTXT(const char* payload) {
    for (uint8_t i = 0; i < _maxClients; i++) {
        if (_authenticatedClients[i]) {
            _webSocket.sendTXT(i, payload);
        }
    }
}

void ESP32_WS_Server::broadcastTXT(String& payload) {
    broadcastTXT(payload.c_str());
}

void ESP32_WS_Server::_staticEventWrapper(uint8_t clientId, WStype_t type, uint8_t* payload, size_t length) {
    if (_instance) {
        _instance->_onEvent(clientId, type, payload, length);
    }
}

void ESP32_WS_Server::_onEvent(uint8_t clientId, WStype_t type, uint8_t* payload, size_t length) {
    switch (type) {
        case WStype_CONNECTED:
            Serial.printf("[WS Manager] Client #%u connected\n", clientId);
            if (clientId < _maxClients) {
                _authenticatedClients[clientId] = false;
            } else {
                Serial.println("[WS Manager] Client limit exceeded, disconnecting.");
                _webSocket.disconnect(clientId);
            }
            break;

        case WStype_DISCONNECTED:
            Serial.printf("[WS Manager] Client #%u disconnected\n", clientId);
            if (clientId < _maxClients) {
                _authenticatedClients[clientId] = false;
                if (_authCallback) _authCallback(clientId, false);
            }
            break;

        case WStype_TEXT: {
            Serial.printf("[WS Manager] Payload from #%u: %s\n", clientId, (char*)payload);
            
            StaticJsonDocument<128> doc;
            DeserializationError error = deserializeJson(doc, payload);

            if (!error && doc.containsKey("secret")) {
                const char* receivedSecret = doc["secret"];
                if (_secretCode.equals(receivedSecret)) {
                    if (clientId < _maxClients) {
                        _authenticatedClients[clientId] = true;
                        Serial.printf("[WS Manager] Client #%u authenticated\n", clientId);
                        _webSocket.sendTXT(clientId, "{\"status\": \"connected\"}");
                        if (_authCallback) _authCallback(clientId, true);
                    }
                } else {
                    Serial.println("[WS Manager] Invalid secret.");
                    _webSocket.sendTXT(clientId, "{\"error\": \"Unauthorized\"}");
                }
            } else if (!error && clientId < _maxClients && _authenticatedClients[clientId]) {
                // Already authenticated — forward message to sketch callback
                if (_messageCallback) _messageCallback(clientId, (const char*)payload);
            }
            break;
        }
        
        case WStype_ERROR:
            Serial.printf("[WS Manager] Error on #%u\n", clientId);
            break;
            
        default:
            break;
    }
}
