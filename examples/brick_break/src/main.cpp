#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "config.h"

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Game variables
bool bricks[BRICK_ROWS][BRICK_COLS];
float ballX, ballY, ballVelX, ballVelY;
float paddleX;

enum GameState { PLAYING, WIN, LOSE };
GameState gameState = PLAYING;
unsigned long endTime;
int bouncesSinceBrick = 0;

const int brick_draw_width = BRICK_WIDTH - 2;
const int brick_gap = 2;
const int total_brick_width = BRICK_COLS * brick_draw_width + (BRICK_COLS - 1) * brick_gap;
const int brick_start_x = (GAME_WIDTH - total_brick_width) / 2 - 1;
const int brick_start_y = 2;

void resetGame() {
  for (int r = 0; r < BRICK_ROWS; r++) {
    for (int c = 0; c < BRICK_COLS; c++) {
      bricks[r][c] = true;
    }
  }
  ballX = random(10, GAME_WIDTH - 10);
  ballY = GAME_HEIGHT - 20;
  ballVelX = random(-2, 3);
  if (ballVelX == 0) ballVelX = 1;
  ballVelY = random(-3, -1);
  paddleX = GAME_WIDTH / 2.0 - PADDLE_WIDTH / 2.0;
  gameState = PLAYING;
  bouncesSinceBrick = 0;
}

void setup() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  randomSeed(analogRead(0));

  // Initialize display
  Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    for (;;);
  }
  display.setRotation(1); // Rotate 90 degrees for vertical orientation
  display.clearDisplay();
  display.display();

  resetGame();
}

void loop() {
  if (gameState == PLAYING) {
    // Move paddle towards ball lazily
    float targetX = ballX - PADDLE_WIDTH / 2.0 + random(-2, 3);
    targetX = constrain(targetX, 4, GAME_WIDTH - PADDLE_WIDTH - 4);
    paddleX = paddleX * 0.7 + targetX * 0.3;
    paddleX = constrain(paddleX, 4, GAME_WIDTH - PADDLE_WIDTH - 4);

    // Update ball position
    ballX += ballVelX;
    ballY += ballVelY;

    // Ball collision with walls
    if (ballX <= 0 || ballX >= GAME_WIDTH - 4) {
      ballVelX = -ballVelX;
      bouncesSinceBrick++;
    }
    if (ballY <= 0) {
      ballVelY = -ballVelY;
      bouncesSinceBrick++;
    }

    // Ball collision with paddle
    if (ballY + 4 >= GAME_HEIGHT - PADDLE_HEIGHT && ballY <= GAME_HEIGHT && ballX + 4 >= paddleX && ballX <= paddleX + PADDLE_WIDTH) {
      float hitPos = (ballX - paddleX) / PADDLE_WIDTH;
      ballVelX = (hitPos - 0.5) * 3.0;
      ballVelX += random(-1, 2); // Add extra randomness
      if (abs(ballVelX) < 0.5) {
        ballVelX = random(0, 2) ? 0.5 : -0.5;
      }
      ballVelY = -abs(ballVelY); // Always bounce up
      bouncesSinceBrick++;
    }

    // Ball collision with bricks
    for (int r = 0; r < BRICK_ROWS; r++) {
      for (int c = 0; c < BRICK_COLS; c++) {
        if (bricks[r][c]) {
          int bx = brick_start_x + c * BRICK_WIDTH;
          int by = brick_start_y + r * BRICK_HEIGHT;
          if (ballX >= bx && ballX <= bx + BRICK_WIDTH && ballY >= by && ballY <= by + BRICK_HEIGHT) {
            bricks[r][c] = false;
            ballVelY = -ballVelY;
            bouncesSinceBrick = 0;
          }
        }
      }
    }

    // Check if ball is out (below paddle)
    if (ballY > GAME_HEIGHT) {
      gameState = LOSE;
      endTime = millis() + 2000;
    }

    // Check if all bricks are gone
    bool allGone = true;
    for (int r = 0; r < BRICK_ROWS; r++) {
      for (int c = 0; c < BRICK_COLS; c++) {
        if (bricks[r][c]) {
          allGone = false;
        }
      }
    }
    if (allGone) {
      gameState = WIN;
      endTime = millis() + 2000;
    }

    // Draw everything
    display.clearDisplay();
    display.drawRoundRect(0, 0, GAME_WIDTH, GAME_HEIGHT, 4, SSD1306_WHITE);
    for (int r = 0; r < BRICK_ROWS; r++) {
      for (int c = 0; c < BRICK_COLS; c++) {
        if (bricks[r][c]) {
          display.fillRect(brick_start_x + c * BRICK_WIDTH + 1, brick_start_y + r * BRICK_HEIGHT + 1, BRICK_WIDTH - 2, BRICK_HEIGHT - 2, SSD1306_WHITE);
        }
      }
    }
    display.fillRect((int)paddleX, GAME_HEIGHT - PADDLE_HEIGHT, PADDLE_WIDTH, PADDLE_HEIGHT, SSD1306_WHITE);
    display.fillRect(ballX, ballY, 4, 4, SSD1306_WHITE);
    display.display();

    if (bouncesSinceBrick > 20) {
      gameState = LOSE;
      endTime = millis() + 2000;
    }
  } else {
    // Display WIN or LOSE
    display.clearDisplay();
    display.drawRoundRect(0, 0, GAME_WIDTH, GAME_HEIGHT, 4, SSD1306_WHITE);
    String msg = (gameState == WIN) ? "WIN" : "LOSE";
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(msg, 0, 0, &x1, &y1, &w, &h);
    int x = (GAME_WIDTH - w) / 2;
    int y = (GAME_HEIGHT - h) / 2;
    display.drawRoundRect(x - 5, y - 5, w + 10, h + 10, 5, SSD1306_WHITE);
    display.setCursor(x, y);
    display.print(msg);
    display.display();
    if (millis() > endTime) {
      resetGame();
    }
  }

  delay(2);
}