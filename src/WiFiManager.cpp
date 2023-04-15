#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <DNSServer.h>
#include <esp_http_server.h>
#include <ping/ping_sock.h>
#include <nvs_flash.h>
#include <nvs.h>
#include "WiFiManager.h"

#if defined(WFM_SHOW_LOG)
#define LOG_INF(format, ...) Serial.printf(format "\n", ##__VA_ARGS__)
#define LOG_WRN(format, ...) Serial.printf(format "\n", ##__VA_ARGS__)
#define LOG_ERR(format, ...) Serial.printf(format "\n", ##__VA_ARGS__)
#else
#define LOG_INF(format, ...)
#define LOG_WRN(format, ...)
#define LOG_ERR(format, ...)
#endif

// maximum size of a SSID name. 32 is IEEE standard. @warning limit is also hard coded in wifi_config_t. Never extend this value
#define MAX_SSID_SIZE 32

// maximum size of a WPA2 passkey. 64 is IEEE standard. @warning limit is also hard coded in wifi_config_t. Never extend this value
#define MAX_PSWD_SIZE 64

char hostName[15] = "";                // Default Host name
char ST_SSID[MAX_SSID_SIZE + 1] = "";  // Default router ssid
char ST_Pass[MAX_PSWD_SIZE + 1] = "";  // Default router passd

// leave following blank for dhcp
char ST_ip[16] = "192.168.0.200";  // Static IP
char ST_sn[16] = "255.255.255.0";  // subnet normally 255.255.255.0
char ST_gw[16] = "192.168.0.1";    // gateway to internet, normally router IP
char ST_dns1[16] = "";             // DNS Server, can be router IP (needed for SNTP)
char ST_dns2[16] = "";             // alternative DNS Server, can be blank

#define START_WIFI_WAIT_SEC 15  // timeout WL_CONNECTED after board start

static bool APstarted = false;  // internal flag AP state
static httpd_handle_t cfgPortalHttpServer = NULL;

#pragma region "Ping"

#define PING_INTERVAL_SEC 30  // how often to check wifi status

static esp_ping_handle_t pingHandle = NULL;
static void startPing();

#pragma endregion

#pragma region "DNS"

#if (AP_DNS_ENABLE)

DNSServer *p_dnsServer = NULL;
static TaskHandle_t dnsServerHandle = NULL;

static void DnsServerTask(void *parameter) {
    while (true) {
        p_dnsServer->processNextRequest();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

static void startDnsServer() {
    const auto DNS_PORT = 53;
    if (!dnsServerHandle) {
        p_dnsServer = new DNSServer;
        if (p_dnsServer) {
            p_dnsServer->start(DNS_PORT, "*", WiFi.softAPIP());  // all DNS request
            xTaskCreate(&DnsServerTask, "Dnstask", 1024 * 2, NULL, tskIDLE_PRIORITY + 1, &dnsServerHandle);
            LOG_INF("startDnsServer");
        }
    }
}

static void stopDnsServer() {
    if (dnsServerHandle) {
        vTaskDelete(dnsServerHandle);
        dnsServerHandle = NULL;
        p_dnsServer->stop();
        delete p_dnsServer;
        p_dnsServer = NULL;
    }
}

#endif

#pragma endregion

static void startCfgPortalServer();
static void loadWifiAuthData();
static void saveWifiAuthData();

#if ST_MDNS_ENABLE
static void setupMdnsHost() {
    if (strlen(hostName)) {
        // set up MDNS service
        if (MDNS.begin(hostName)) {
            // Add service to MDNS
            MDNS.addService("http", "tcp", 80);
            // MDNS.addService("ws", "udp", 83);
            // MDNS.addService("ftp", "tcp", 21);
            LOG_INF("mDNS service: http://%s.local", hostName);
        } else
            LOG_ERR("mDNS host: %s Failed", hostName);
    }
}
#endif

static void onWiFiEvent(WiFiEvent_t event) {
    // callback to report on wifi events
    if (event == ARDUINO_EVENT_WIFI_READY)
        ;
    else if (event == ARDUINO_EVENT_WIFI_SCAN_DONE)
        ;
    else if (event == ARDUINO_EVENT_WIFI_STA_START)
        LOG_INF("Wifi event: STA started, connecting to: %s", ST_SSID);
    else if (event == ARDUINO_EVENT_WIFI_STA_STOP)
        LOG_INF("Wifi event: STA stopped %s", ST_SSID);
    else if (event == ARDUINO_EVENT_WIFI_AP_START) {   
        if (!strcmp(WiFi.softAPSSID().c_str(), AP_SSID)) {  // filter default AP "ESP_xxxxxx"
            LOG_INF("Wifi event: AP_START: ssid: %s, use 'http://%s' to connect",
                    WiFi.softAPSSID().c_str(),
                    WiFi.softAPIP().toString().c_str());
            APstarted = true;
#if (AP_DNS_ENABLE)
            startDnsServer();
#endif
        }
    } else if (event == ARDUINO_EVENT_WIFI_AP_STOP) {
        if (!strcmp(WiFi.softAPSSID().c_str(), AP_SSID)) {
            LOG_INF("Wifi event: AP_STOP: %s", WiFi.softAPSSID().c_str());
            APstarted = false;
#if (AP_DNS_ENABLE)
            stopDnsServer();
#endif
        }
    } else if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP)
        LOG_INF("Wifi event: STA got IP, use 'http://%s' to connect", WiFi.localIP().toString().c_str());
    else if (event == ARDUINO_EVENT_WIFI_STA_LOST_IP)
        LOG_INF("Wifi event: STA lost IP");
    else if (event == ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED)
        ;
    else if (event == ARDUINO_EVENT_WIFI_STA_CONNECTED)
        LOG_INF("Wifi event: STA connection to %s", ST_SSID);
    else if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED)
        LOG_INF("Wifi event: STA disconnected");
    else if (event == ARDUINO_EVENT_WIFI_AP_STACONNECTED)
        LOG_INF("Wifi event: AP client connection");
    else if (event == ARDUINO_EVENT_WIFI_AP_STADISCONNECTED)
        LOG_INF("Wifi event: AP client disconnection");
    else
        LOG_WRN("Wifi event: Unhandled event %d", event);
}

static bool setWifiAP() {
    if (!APstarted) {
        char ap_ssid[MAX_SSID_SIZE + 1];
        char ap_pswd[MAX_PSWD_SIZE + 1];
        snprintf(ap_ssid, sizeof(ap_ssid), "%s", AP_SSID);
        snprintf(ap_pswd, sizeof(ap_pswd), "%s", AP_PSWD);

        WiFi.mode(WIFI_AP_STA);
        WiFi.softAP(ap_ssid, ap_pswd, 1, 0, 1, false); // only 1 client
    }
    return true;
}

static bool setWifiSTA() {
    if (strlen(ST_SSID)) {
        
        if (!APstarted)
            WiFi.mode(WIFI_STA);

        if (strlen(ST_ip)) {
            IPAddress _ip, _gw, _sn, _dns1, _dns2;
            if (!_ip.fromString(ST_ip))
                LOG_ERR("Failed to parse IP: %s", ST_ip);
            else {
                _ip.fromString(ST_ip);
                _gw.fromString(ST_gw);
                _sn.fromString(ST_sn);
                
                if (strlen(ST_dns1)) {
                    _dns1.fromString(ST_dns1);
                } else {
                    _dns1 = _gw;
                }
                _dns2.fromString(ST_dns2);

                // set static ip
                WiFi.config(_ip, _gw, _sn, _dns1);  // need DNS for SNTP
                LOG_INF("Wifi Station set static IP");
            }
        } else
            LOG_INF("Wifi Station IP from DHCP");

        WiFi.begin(ST_SSID, ST_Pass);
        return true;
    }

    LOG_WRN("No Station SSID provided, use AP");
    return false;
}

static bool startWifi(bool firstcall) {
    // start wifi station (and wifi AP if allowed or station not defined)
    if (firstcall) {
        
        loadWifiAuthData();
        snprintf(hostName, sizeof(hostName), "%s", HOSTNAME);

        WiFi.mode(WIFI_OFF);
        WiFi.persistent(false);        // prevent the flash storage WiFi credentials
        WiFi.setAutoReconnect(false);  // Set whether module will attempt to reconnect to an access point in case it is disconnected
        WiFi.softAPdisconnect(false);  // kill rogue AP on startup
        WiFi.disconnect(true);
        WiFi.setHostname(hostName);
        WiFi.onEvent(onWiFiEvent);
    }

    bool station = setWifiSTA();

    if (station) {
        LOG_INF("check WiFi status");
        uint32_t startAttemptTime = millis();
        // Stop trying on failure timeout, will try to reconnect later by ping
        while (!WiFi.isConnected() && (millis() - startAttemptTime < START_WIFI_WAIT_SEC * 1000)) {
            delay(500);
        }

#if ST_MDNS_ENABLE
        if (firstcall)
            setupMdnsHost();
#endif
        startPing();
    }

    if (!station || (firstcall && !WiFi.isConnected())) {
        setWifiAP();
        startCfgPortalServer();
    }

    return WiFi.isConnected();
}

static void pingSuccess(esp_ping_handle_t hdl, void *args) {
    if (APstarted) {
        LOG_INF("pingSuccess: AP stop");
        if (cfgPortalHttpServer) {
            httpd_stop(cfgPortalHttpServer);
        }
        WiFi.mode(WIFI_STA);
    }
}

static void pingTimeout(esp_ping_handle_t hdl, void *args) {
    LOG_WRN("Failed to ping gateway, restart wifi");
    startWifi(false);
}

static void startPing() {
    if (pingHandle != NULL) {
        return;
    }

    IPAddress ipAddr = WiFi.gatewayIP();
    ip_addr_t pingDest;
    IP_ADDR4(&pingDest, ipAddr[0], ipAddr[1], ipAddr[2], ipAddr[3]);
    esp_ping_config_t pingConfig = ESP_PING_DEFAULT_CONFIG();
    pingConfig.target_addr = pingDest;
    pingConfig.count = ESP_PING_COUNT_INFINITE;
    pingConfig.interval_ms = PING_INTERVAL_SEC * 1000;
    pingConfig.timeout_ms = 5000;
#if CONFIG_IDF_TARGET_ESP32S3
    pingConfig.task_stack_size = 1024 * 6;
#else
    pingConfig.task_stack_size = 1024 * 4;
#endif
    pingConfig.task_prio = 1;
    // set ping task callback functions
    esp_ping_callbacks_t cbs;
    cbs.on_ping_success = pingSuccess;
    cbs.on_ping_timeout = pingTimeout;
    cbs.on_ping_end = NULL;
    cbs.cb_args = NULL;
    esp_ping_new_session(&pingConfig, &cbs, &pingHandle);
    esp_ping_start(pingHandle);
    LOG_INF("Started ping monitoring");
}

static void stopPing() {
    if (pingHandle != NULL) {
        esp_ping_stop(pingHandle);
        esp_ping_delete_session(pingHandle);
        pingHandle = NULL;
    }
}

const char head_chunk_html[] = R"rawliteral(
<!DOCTYPE HTML><html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>WiFi settings</title>
<style>
input[type="text"] {width:250px;margin-bottom:8px;font-size:20px;}
input[type="submit"] {width:250px;height:60px;margin-bottom:8px;font-size:20px;}
body {text-align:center;font-size:15px;}
</style></head><body><br>
)rawliteral";

const char end_chunk_html[] = R"rawliteral(
</body></html>
)rawliteral";

const char cfg_portal_body[] = R"rawliteral(
<form action="/" method="POST">
<input type="text" name="ssid" placeholder="SSID" required maxlength="32"><br>
<input type="text" name="pswd" placeholder="Pass" maxlength="64"><br>
<input type="submit" value="Check Connection">
</form>
)rawliteral";

static esp_err_t indexHandler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_sendstr_chunk(req, head_chunk_html);
    httpd_resp_sendstr_chunk(req, cfg_portal_body);
    httpd_resp_sendstr_chunk(req, end_chunk_html);
    return httpd_resp_sendstr_chunk(req, NULL); // Send empty chunk to signal HTTP response completion
}

static esp_err_t cfgHandler(httpd_req_t *req) {
    const char ssid_str[] = "ssid=";
    const char pswd_str[] = "&pswd=";

    const uint16_t buf_len = 
        (MAX_SSID_SIZE + MAX_PSWD_SIZE) * 3 // url encoded: '?' = "%3F"
        + sizeof(pswd_str) + sizeof(ssid_str)
        + 10; // gap

    char buf[buf_len];

    size_t total_len = req->content_len;
    size_t received = 0;
    size_t cur_len = 0;

    if (total_len >= sizeof(buf)) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "content too long");
        return ESP_FAIL;
    }

    while (cur_len < total_len) {
        received = httpd_req_recv(req, buf + cur_len, total_len);
        if (received <= 0) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to post control value");
            return ESP_FAIL;
        }
        cur_len += received;
    }

    buf[total_len] = '\0';

    char *ssid = strstr(buf, ssid_str);
    char *pswd = strstr(buf, pswd_str);

    if (ssid && pswd) {
        char *pswd_decode = WiFiManagerClass::url_decode(pswd + sizeof(pswd_str) - 1);
        *pswd = '\0';

        char *ssid_decode = WiFiManagerClass::url_decode(ssid + sizeof(ssid_str) - 1);

        size_t ssid_decode_len = strlen(ssid_decode);
        size_t pswd_decode_len = strlen(pswd_decode);

        if ((ssid_decode_len <= MAX_SSID_SIZE) && (pswd_decode_len <= MAX_PSWD_SIZE)) {
            
            LOG_INF(R"~(Check connection to SSID="%s", Pass="%s")~", ssid_decode, pswd_decode);
            
            stopPing();
            WiFi.disconnect(true);
            WiFi.begin(ssid_decode, pswd_decode);

            uint32_t startAttemptTime = millis();
            while (!WiFi.isConnected() && millis() - startAttemptTime < 5000) {
                delay(500);
            }

            if (WiFi.isConnected()) {
                httpd_resp_set_hdr(req, "Connection", "close");
                httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
                httpd_resp_set_type(req, "text/html");
                httpd_resp_sendstr_chunk(req, head_chunk_html);
                httpd_resp_sendstr_chunk(req, "Connection OK.<br>System Restart now...");

                if (strlen(ST_ip)) {
                    snprintf(buf, sizeof(buf) - 1, R"~(<a href="http://%s"><br>Try http://%s later</a>)~", ST_ip, ST_ip);
                    httpd_resp_sendstr_chunk(req, buf);
                }

                httpd_resp_sendstr_chunk(req, end_chunk_html);
                httpd_resp_sendstr_chunk(req, NULL);

                memcpy(ST_SSID, ssid_decode, ssid_decode_len + 1);
                memcpy(ST_Pass, pswd_decode, pswd_decode_len + 1);
                memcpy(ST_gw, WiFi.gatewayIP().toString().c_str(), sizeof(ST_gw));

                saveWifiAuthData();

                free(ssid_decode);
                free(pswd_decode);

                LOG_INF("restart");
                Serial.flush();
                delay(500);
                ESP.restart();

                return ESP_OK;
            }
        }

        free(ssid_decode);
        free(pswd_decode);
    }

    return indexHandler(req);
}

static void startCfgPortalServer() {

    const uint8_t WEB_PORT = 80;
    const uint8_t MAX_CLIENTS = 1;

    if (cfgPortalHttpServer)
        return;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
#if CONFIG_IDF_TARGET_ESP32S3
    config.stack_size = 1024 * 8;
#endif
    config.server_port = WEB_PORT;
    config.ctrl_port = WEB_PORT;
    config.lru_purge_enable = true;
    config.max_open_sockets = MAX_CLIENTS;

    httpd_uri_t indexUri = {.uri = "/", .method = HTTP_GET, .handler = indexHandler, .user_ctx = NULL};
    httpd_uri_t cfgUri = {.uri = "/", .method = HTTP_POST, .handler = cfgHandler, .user_ctx = NULL};
    
    if (httpd_start(&cfgPortalHttpServer, &config) == ESP_OK) {
        httpd_register_uri_handler(cfgPortalHttpServer, &indexUri);
        httpd_register_uri_handler(cfgPortalHttpServer, &cfgUri);
        LOG_INF("start cfgPortalHttpServer on port: %u", config.server_port);
    } else {
        LOG_ERR("Failed to start web server");
    }
}

static bool cfgPortalActive() {
    return cfgPortalHttpServer;
}

static void loadWifiAuthData() {
    // Initialize NVS
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        nvs_flash_erase();
        err = nvs_flash_init();
    }

    if (err != ESP_OK)
        return;

    nvs_handle_t nvs_handle;
    size_t nvs_required_size;
    
    err = nvs_open("wifiAuthData", NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        LOG_ERR("Error (%s) opening NVS handle!", esp_err_to_name(err));
    } else {
                
        if (ESP_OK == nvs_get_str(nvs_handle, "ssid", NULL, &nvs_required_size)) {
            if (nvs_required_size <= sizeof(ST_SSID))
                nvs_get_str(nvs_handle, "ssid", ST_SSID, &nvs_required_size);
        }

        if (ESP_OK == nvs_get_str(nvs_handle, "pswd", NULL, &nvs_required_size)) {
            if (nvs_required_size <= sizeof(ST_Pass))
                nvs_get_str(nvs_handle, "pswd", ST_Pass, &nvs_required_size);
        }

        if (ESP_OK == nvs_get_str(nvs_handle, "gateway", NULL, &nvs_required_size)) {
            if (nvs_required_size <= sizeof(ST_gw))
                nvs_get_str(nvs_handle, "gateway", ST_gw, &nvs_required_size);
        }

        nvs_close(nvs_handle);
    }
}

static void saveWifiAuthData() {
    esp_err_t err;
    nvs_handle_t nvs_handle;
    err = nvs_open("wifiAuthData", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        LOG_ERR("Error (%s) opening NVS handle!", esp_err_to_name(err));
    } else {
        nvs_set_str(nvs_handle, "ssid", ST_SSID);
        nvs_set_str(nvs_handle, "pswd", ST_Pass);
        nvs_set_str(nvs_handle, "gateway", ST_gw);
        
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
    }
}

////////////////////////////////////////////////////////////
//                     WiFiManagerClass                   //
////////////////////////////////////////////////////////////

WiFiManagerClass::WiFiManagerClass() {

}

bool WiFiManagerClass::start() {
    return startWifi(true);
}

bool WiFiManagerClass::isConnected() {
    return WiFi.isConnected();
}

bool WiFiManagerClass::isCfgPortalActive() {
    return cfgPortalActive();
}

void WiFiManagerClass::cleanWifiAuthData() {
    memset(ST_SSID, 0, sizeof(ST_SSID));
    memset(ST_Pass, 0, sizeof(ST_Pass));
    memset(ST_gw, 0, sizeof(ST_gw));
    saveWifiAuthData();
};

/* Converts a hex character to its integer value */
static char from_hex(char ch) {
    return isdigit(ch) ? ch - '0' : tolower(ch) - 'a' + 10;
}

/* Converts an integer value to its hex character*/
static char to_hex(char code) {
    static const char hex[] = "0123456789abcdef";
    return hex[code & 15];
}

/* Returns a url-encoded version of str */
/* IMPORTANT: be sure to free() the returned string after use */
char *WiFiManagerClass::url_encode(char *str) {
    char *pstr = str, *buf = (char *)malloc(strlen(str) * 3 + 1), *pbuf = buf;
    while (*pstr) {
        if (isalnum(*pstr) || *pstr == '-' || *pstr == '_' || *pstr == '.' || *pstr == '~')
            *pbuf++ = *pstr;
        else if (*pstr == ' ')
            *pbuf++ = '+';
        else
            *pbuf++ = '%', *pbuf++ = to_hex(*pstr >> 4), *pbuf++ = to_hex(*pstr & 15);
        pstr++;
    }
    *pbuf = '\0';
    return buf;
}

/* Returns a url-decoded version of str */
/* IMPORTANT: be sure to free() the returned string after use */
char *WiFiManagerClass::url_decode(char *str) {
    char *pstr = str, *buf = (char *)malloc(strlen(str) + 1), *pbuf = buf;
    while (*pstr) {
        if (*pstr == '%') {
            if (pstr[1] && pstr[2]) {
                *pbuf++ = from_hex(pstr[1]) << 4 | from_hex(pstr[2]);
                pstr += 2;
            }
        } else if (*pstr == '+') {
            *pbuf++ = ' ';
        } else {
            *pbuf++ = *pstr;
        }
        pstr++;
    }
    *pbuf = '\0';
    return buf;
}

void WiFiManagerClass::debugMemory(const char *caller) {
    log_e("%s > Free: heap %u, block: %u, pSRAM %u\n",
            caller,
            ESP.getFreeHeap(),
            heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
            ESP.getFreePsram());
}

WiFiManagerClass WiFiManager;
