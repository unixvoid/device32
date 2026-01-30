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