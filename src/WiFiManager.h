#ifndef WiFiManager_h
#define WiFiManager_h

#if !defined(WFM_AP_SSID)
#define WFM_AP_SSID "iot_cfg"
#endif

#if !defined(WFM_AP_PSWD)
#define WFM_AP_PSWD "espress.if"  // if not empty - must be at least 8 characters
#endif

#if !defined(WFM_HOSTNAME)
#define WFM_HOSTNAME WFM_AP_SSID
#endif

#if !defined(WFM_ST_MDNS_ENABLE)
#define WFM_ST_MDNS_ENABLE 0  // station mDNS service http://%HOSTNAME%.local"
#endif

#if !defined(WFM_AP_DNS_ENABLE)
#define WFM_AP_DNS_ENABLE 1  // access point dns service
#endif

class WiFiManagerClass {
   private:
   public:
    WiFiManagerClass();
    ~WiFiManagerClass(){};
    bool start();
    bool isConnected();
    void cleanWiFiAuthData();
    static char *url_encode(char *str);
    static char *url_decode(char *str);
    static void debugMemory(const char *caller);
};

extern WiFiManagerClass WiFiManager;

#endif
