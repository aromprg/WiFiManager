#pragma once

#if !defined(AP_SSID)
#define AP_SSID "iot_cfg"
#endif

#if !defined(AP_PSWD)
#define AP_PSWD "espress.if" // if not empty - must be at least 8 characters
#endif

#if !defined(HOSTNAME)
#define HOSTNAME AP_SSID
#endif

#if !defined(ST_MDNS_ENABLE)
#define ST_MDNS_ENABLE 0 // station mDNS service http://%HOSTNAME%.local"
#endif

#if !defined(AP_DNS_ENABLE)
#define AP_DNS_ENABLE  1 // access point dns service
#endif

bool startWifi(bool firstcall = true);
bool cfgPortalActive();
void cleanWifiAuthData();
