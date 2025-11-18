#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
#include "config.h"

// Starfield parameters
const int NUM_STARS = 100;
const float SPEED = 0.01f;
const float SCALE = 50.0f;

struct Star {
    float x, y, z;
};

Star stars[NUM_STARS];

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

void initializeStars() {
    for (int i = 0; i < NUM_STARS; i++) {
        stars[i].x = random(-1000, 1000) / 1000.0f;
        stars[i].y = random(-1000, 1000) / 1000.0f;
        stars[i].z = random(100, 1000) / 1000.0f;
    }
}

void updateStars() {
    for (int i = 0; i < NUM_STARS; i++) {
        stars[i].z -= SPEED;
        if (stars[i].z <= 0.0f) {
            stars[i].x = random(-1000, 1000) / 1000.0f;
            stars[i].y = random(-1000, 1000) / 1000.0f;
            stars[i].z = 1.0f;
        }
    }
}

void drawStars() {
    display.clearDisplay();
    for (int i = 0; i < NUM_STARS; i++) {
        float z = stars[i].z;
        int sx = SCREEN_WIDTH / 2 + (int)(stars[i].x / z * SCALE);
        int sy = SCREEN_HEIGHT / 2 + (int)(stars[i].y / z * SCALE);
        if (sx >= 0 && sx < SCREEN_WIDTH && sy >= 0 && sy < SCREEN_HEIGHT) {
            int size = (z < 0.5f) ? 2 : 1;
            display.fillRect(sx, sy, size, size, SSD1306_WHITE);
        }
    }
    display.display();
}

void setup() {
    randomSeed(analogRead(0));
    Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        for (;;) ;
    }
    display.clearDisplay();
    display.display();

    initializeStars();
}

void loop() {
    updateStars();
    drawStars();
    delay(30);
}
