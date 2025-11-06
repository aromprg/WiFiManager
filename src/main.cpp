/*
 * main.cpp - example use WiFiManager for esp32.
 *
 * aromprog 2023
 * 
 */

#include <Arduino.h>
#include <WebServer.h>
#include "WiFiManager.h"

WebServer server(80);

const char demo_page_html[] = R"rawliteral(<html>
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
</html>)rawliteral";

void handleRoot() {
    char temp[sizeof(demo_page_html) + 50];
    int sec = millis() / 1000;
    int min = sec / 60;
    int hr = min / 60;

    snprintf(temp, sizeof(temp), demo_page_html, hr, min % 60, sec % 60, hallRead());
    server.send(200, "text/html", temp);
}

uint8_t ping_err_cnt = 0;

void OnPingOK() {
    ping_err_cnt = 0;
}

void OnPingERR() {
    ping_err_cnt++;
    if (ping_err_cnt > 50) {
        Serial.println("Too many ping errors - restart");
        Serial.flush();
        ESP.restart();
    }
}

void OnFirstConnect() {
    server.on("/", handleRoot);
    server.onNotFound([]() {
        server.send(404, "text/plain", "File Not Found");
    });
    server.begin();
    Serial.println("Demo HTTP server started");

    // attach user callback to ping events
    WiFiManager.attachOnPingOK(OnPingOK);
    WiFiManager.attachOnPingERR(OnPingERR);
}

void setup() {
    Serial.begin(115200);
    Serial.setDebugOutput(false);

    WiFiManager.setStaticIP();
    WiFiManager.configAP("my_ap_ssid", "123456789");
    WiFiManager.attachOnFirstConnect(OnFirstConnect);
    WiFiManager.start("esp_hostname");
}

void loop() {
    
    delay(1);  // allow the cpu to switch to other tasks

    server.handleClient();

    if (Serial.available()) {
        switch (Serial.read()) {
            case '-':
                WiFiManager.cleanWiFiAuthData();
                Serial.println("cleanWiFiAuthData");
                break;
            case 'i':
                Serial.printf("CPU freq=%lu MHz\n", ESP.getCpuFreqMHz());
                break;
            case 'r':
                ESP.restart();
                break;

            default:
                break;
        }
    }
}
