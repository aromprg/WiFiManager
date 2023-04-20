/*
 * WiFiManager.h - WiFi management Arduino library for esp32.
 *
 * First: a simple setup of the configuration portal to connect your Wi-Fi router.
 *
 * Next: Automatically check the status of the WiFi connection
 * using a ping gateway (router) and reconnect if it fails.
 *
 * aromprog 2023
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

class WiFiManagerClass {
   private:
   public:
    WiFiManagerClass();
    ~WiFiManagerClass(){};
    bool start(const char *hostname = nullptr);
    bool isConnected();
    void cleanWiFiAuthData();
    void setStaticIP(const char *ip = "192.168.0.200",
                     const char *subnet = "255.255.255.0",
                     const char *gateway = nullptr,
                     const char *dns1 = nullptr,
                     const char *dns2 = nullptr);
    void configAP(const char *ssidAP = nullptr,
               const char *passwordAP = nullptr);
    static char *url_encode(const char *str);
    static char *url_decode(const char *str);
    static void debugMemory(const char *caller);
};

extern WiFiManagerClass WiFiManager;

#endif
