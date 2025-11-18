#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "config.h"

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Game variables
int ballX = SCREEN_WIDTH / 2;
int ballY = SCREEN_HEIGHT / 2;
int ballVelX = 2;
int ballVelY = 2;
int paddleWidth = 4;
int paddleHeight = 16;
int leftPaddleY = (SCREEN_HEIGHT - paddleHeight) / 2;
int rightPaddleY = leftPaddleY;
int ballSize = 4;

void setup() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  randomSeed(analogRead(0));

  // Initialize display
  Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    for (;;);
  }
  display.clearDisplay();
  display.display();
}

void loop() {
  // Simple AI for paddles
  if (ballVelX < 0) {
    if (leftPaddleY + paddleHeight / 2 < ballY) {
      leftPaddleY += 1;
    } else if (leftPaddleY + paddleHeight / 2 > ballY) {
      leftPaddleY -= 1;
    }
  } else {
    if (rightPaddleY + paddleHeight / 2 < ballY) {
      rightPaddleY += 1;
    } else if (rightPaddleY + paddleHeight / 2 > ballY) {
      rightPaddleY -= 1;
    }
  }

  // Keep paddles in bounds
  leftPaddleY = constrain(leftPaddleY, 0, SCREEN_HEIGHT - paddleHeight);
  rightPaddleY = constrain(rightPaddleY, 0, SCREEN_HEIGHT - paddleHeight);

  // Calculate next ball position
  int nextBallX = ballX + ballVelX;
  int nextBallY = ballY + ballVelY;

  // Ball collision with paddles
  if (nextBallX < paddleWidth && nextBallX + ballSize > 0 && ballY < leftPaddleY + paddleHeight && ballY + ballSize > leftPaddleY) {
    ballVelX = -ballVelX;
    nextBallX = ballX + ballVelX;
  }
  if (nextBallX + ballSize > SCREEN_WIDTH - paddleWidth && nextBallX < SCREEN_WIDTH && ballY < rightPaddleY + paddleHeight && ballY + ballSize > rightPaddleY) {
    ballVelX = -ballVelX;
    nextBallX = ballX + ballVelX;
  }

  // Ball collision with top and bottom
  if (nextBallY < 0 || nextBallY + ballSize > SCREEN_HEIGHT) {
    ballVelY = -ballVelY;
    nextBallY = ballY + ballVelY;
  }

  // Update ball position
  ballX = nextBallX;
  ballY = nextBallY;

  // Reset ball if it goes off screen
  if (ballX < 0 || ballX > SCREEN_WIDTH) {
    ballX = SCREEN_WIDTH / 2;
    ballY = SCREEN_HEIGHT / 2;
    ballVelX = random(0, 2) ? 2 : -2;
    ballVelY = random(0, 2) ? 2 : -2;
  }

  display.clearDisplay();
  display.drawRoundRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 3, SSD1306_WHITE);
  display.fillRect(0, leftPaddleY, paddleWidth, paddleHeight, SSD1306_WHITE);
  display.fillRect(SCREEN_WIDTH - paddleWidth, rightPaddleY, paddleWidth, paddleHeight, SSD1306_WHITE);
  display.fillRect(ballX, ballY, ballSize, ballSize, SSD1306_WHITE);
  display.display();

  delay(2);
}