#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET -1  // No reset pin
#define SDA_PIN 7 // D5
#define SCL_PIN 6 // D4

// button config
#define BUTTON_PIN 5 // D3
#define BUTTON_TAP_TIME 20

// Grid settings
#define GRID_WIDTH 64
#define GRID_HEIGHT 32
#define CELL_SIZE 2

// globals
#include <Adafruit_SSD1306.h>
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);