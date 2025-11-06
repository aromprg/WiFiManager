/*
 * WiFiManager.h - WiFi management Arduino library for ESP32.
 *
 * First: a simple setup of the configuration portal to connect your Wi-Fi router.
 *
 * Next: Automatically check the status of the WiFi connection
 * using a ping gateway (router) and reconnect if it fails.
 *
 * aromprg@github.com 2023-2025
 * 
 */

#ifndef WiFiManager_h
#define WiFiManager_h

#if !defined(WFM_ST_MDNS_ENABLE)
#define WFM_ST_MDNS_ENABLE 0  // station mDNS service http://%HOSTNAME%.local"
#endif

#if !defined(WFM_AP_DNS_ENABLE)
#define WFM_AP_DNS_ENABLE 1  // access point DNS service
#endif

extern "C" {
  typedef void (*callback_fn_t)(void);
}

class WiFiManagerClass {
   private:
   public:
    WiFiManagerClass();
    ~WiFiManagerClass(){};
    bool start(const char *hostname = nullptr);
    bool isConnected();
    void attachOnFirstConnect(callback_fn_t callback_fn);
    void attachOnPingOK(callback_fn_t callback_fn);
    void attachOnPingERR(callback_fn_t callback_fn);
    void cleanWiFiAuthData();
    void setStaticIP(const char *ip = "192.168.0.200",
                     const char *subnet = "255.255.255.0",
                     const char *gateway = nullptr,
                     const char *dns1 = nullptr,
                     const char *dns2 = nullptr);
    void configAP(const char *ssidAP = nullptr,
               const char *passwordAP = nullptr,
               bool hidden = false);
    static char *url_encode(const char *str);
    static char *url_decode(const char *str);
};

extern WiFiManagerClass WiFiManager;

#endif
