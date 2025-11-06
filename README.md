# Lightweight WiFiManager library for Arduino ESP32

This Arduino library is improving the usage ESP32 WiFi module.
The change log of this library can be found in [CHANGELOG.md](CHANGELOG.md).

## Purpose

The library provides simple WiFi connection management. On the first launch, WiFiManager creates a Wi-Fi Access Point to setup a future connection to an existing Wi-Fi network. After checking connection, WiFiManager will save this configutation and restart the CPU. Next, WiFiManager will automatically connect to the saved Wi-Fi network, as well as periodically check the connection with the gateway (router). If the ping fails, a reconnect should occur.

## States diagram

![diagram.drawio.png](/doc/diagram.drawio.png)

## Configuration Access Point

To get the setup web page after connecting to the configuration Access Point, use [http://192.168.4.1](http://192.168.4.1) in a browser. If DNS is enabled (default), it is also possible to redirect any [http://xxx.xxx](http://xxx.xxx) (**http*S*** doesn't redirect!) address to the configuration portal.  

![config_portal.jpg](/doc/config_portal.jpg)  

On the configuration web page, you can select the Wi-Fi networks that were found.

## Design

In order to use memory efficiently WiFiManager uses some low-level ESP32 API calls (nvs, ping, httpd_server). WiFiManagerClass is only used as a wrapper for user-friendly interface, making it easy to access c-callback API functions.  
Many ideas are used from the project [s60sc/ESP32-CAM_MJPEG2SD](https://github.com/s60sc/ESP32-CAM_MJPEG2SD).

## Getting Started

Copy the files [src/WiFiManager.cpp](/src/WiFiManager.cpp) and [src/WiFiManager.h](/src/WiFiManager.h) to your project directory.

```CPP
#include <Arduino.h>
#include "WiFiManager.h"
```
### Some features can be configured with the -Dxxx compiler option (see [platformio.ini](/platformio.ini)) 

```CPP
#define WFM_ST_MDNS_ENABLE 1 // station mDNS service "http://%HOSTNAME%.local" - DISABLED BY DEFAULT
#define WFM_AP_DNS_ENABLE  1 // access point DNS service - ENABLED BY DEFAULT
#define WFM_SHOW_LOG         // show debug messages over serial port - DISABLED BY DEFAULT
```
### Configutation before start

##### Set a static IP address to call your device if necessary.
```CPP
// default ip = "192.168.0.200"
// subnet = "255.255.255.0"
// gateway = router ip
// dns1 = router ip
WiFiManager.setStaticIP();

// or set the desired parameters
WiFiManager.setStaticIP("192.168.0.123");
```
If `setStaticIP()` is not called, the IP address set by the router DHCP.

##### Set a configuration Access Point if necessary.
```CPP
WiFiManager.configAP("my_ap_ssid", "123456789");
WiFiManager.configAP("my_ap_ssid", "123456789", true); // create hidden AP
```
If `configAP()` is not called, the default access point name "ESP-XXXX" is used, where XXXX is the end MAC address of the device, with an empty password (open).

### Start WiFiManager
```CPP
WiFiManager.start(); // start with default Hostname = Access Point name
WiFiManager.start("esp_hostname"); // or set Hostname and start
```

### Check WiFi-connection
```CPP
WiFiManager.isConnected(); // if true - connection OK
```

### Attach user callback function to first successful connection event
```CPP
void OnFirstConnect() {
    Serial.println("TODO");
}

WiFiManager.attachOnFirstConnect(OnFirstConnect);
```

### Attach user callback function to ping successful/timeout event
```CPP
WiFiManager.attachOnPingOK(OnPingOK);
WiFiManager.attachOnPingERR(OnPingERR);
```

### Clean stored WiFi settings
```CPP
WiFiManager.cleanWiFiAuthData();
```

## Example

This repository is created as a [Platformio](https://platformio.org/) project. See [src/main.cpp](/src/main.cpp) for an example of usage.
