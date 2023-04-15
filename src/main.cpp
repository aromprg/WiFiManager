#include <Arduino.h>
#include <WebServer.h>
#include "WiFiManager.h"

WebServer server(80);

bool server_started = false;

const char demo_page_html[] = R"rawliteral(
<html>
<head>
<meta http-equiv='refresh' content='2'/>
<title>ESP32 Demo</title>
<style>body {font-size:20px;}</style>
</head>
<body>
<h1>Hello from ESP32</h1>
<p>Uptime: %02d:%02d:%02d</p>
<p>Hall Sensor: %d</p>
</body>
</html>
)rawliteral";

void handleRoot() {
    char temp[sizeof(demo_page_html) + 50];
    int sec = millis() / 1000;
    int min = sec / 60;
    int hr = min / 60;

    snprintf(temp, sizeof(temp), demo_page_html, hr, min % 60, sec % 60, hallRead());
    server.send(200, "text/html", temp);
}

void setup() {
    Serial.begin(115200);
    Serial.setDebugOutput(false);

    WiFiManager.start();
}

void loop() {
    
    delay(1);  // allow the cpu to switch to other tasks

    if (!server_started) {
        if (WiFiManager.isConnected()) {
            server_started = true;
            server.on("/", handleRoot);
            server.onNotFound([]() {
                server.send(404, "text/plain", "File Not Found");
            });
            server.begin();
            Serial.println("Demo HTTP server started");
        }
    } else {
        server.handleClient();
    }

    if (Serial.available()) {
        switch (Serial.read()) {
            case '-':
                WiFiManager.cleanWiFiAuthData();
                Serial.println("cleanWiFiAuthData");
                break;
            case 'm':
                WiFiManager.debugMemory("debugMemory");
                break;
            case 'r':
                ESP.restart();
                break;

            default:
                break;
        }
    }
}
