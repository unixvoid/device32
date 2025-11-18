#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_SDA_PIN 7 // D5
#define OLED_SCL_PIN 6 // D4

// button config
#define BUTTON_PIN 5 // D3
#define BUTTON_TAP_TIME 20

// game constants
#define GAME_WIDTH 64   // logical width after rotation
#define GAME_HEIGHT 128 // logical height after rotation
#define BRICK_ROWS 4
#define BRICK_COLS 5
#define BRICK_WIDTH (GAME_WIDTH / BRICK_COLS)  // 8
#define BRICK_HEIGHT 8
#define PADDLE_WIDTH 16
#define PADDLE_HEIGHT 4
#define BALL_RADIUS 2
#define BALL_SPEED 1.4

// globals
#include <Adafruit_SSD1306.h>
extern Adafruit_SSD1306 display;