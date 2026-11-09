#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_SDA_PIN 7 // D5
#define OLED_SCL_PIN 6 // D4

// button config
#define BUTTON_PIN 5 // D3
#define BUTTON_TAP_TIME 20

// NTP server
#define NTP_SERVER "pool.ntp.org"

// globals
#include <Adafruit_SSD1306.h>
extern Adafruit_SSD1306 display;

// ----- WiFi provisioning ----------------------------------------------------
// No credentials are compiled into the firmware. On first boot (or after a
// credential reset) the device raises a SoftAP + captive portal so the network
// can be configured from a phone, and stores the result in NVS. This keeps the
// published binary safe to hand out.
#define WIFI_AP_SSID            "device32-weather"
#define PROV_AP_CHANNEL         1
#define PROV_AP_MAX_CONN        4

// Fixed address of the setup SoftAP. Set explicitly (rather than read back from
// WiFi.softAPIP(), which can still be 0.0.0.0 immediately after softAP()) and
// used as the answer to every DNS query, so the phone's captive-portal sheet
// opens on its own.
#define WIFI_AP_IP              192, 168, 4, 1
#define WIFI_AP_IP_STR          "192.168.4.1"

// How long a single STA association attempt is given before giving up.
#define WIFI_CONNECT_TIMEOUT_MS 20000
// Portal poll cadence (DNS + HTTP servicing).
#define WIFI_SETUP_POLL_MS      2

// Hold the button this long during normal operation to wipe the stored
// credentials and re-enter the setup portal.
#define RESET_WIFI_HOLD_TIME    4000

// NVS namespace + keys for the stored credentials
#define WIFI_NVS_NAMESPACE      "device32"
#define WIFI_NVS_SSID_KEY       "wifi_ssid"
#define WIFI_NVS_PASS_KEY       "wifi_pass"