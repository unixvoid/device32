#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "config.h"

#define OLED_RESET -1

// Display instance
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Boids simulation parameters
#define NUM_BOIDS 42
#define MAX_SPEED 2.2f
#define MAX_FORCE 0.35f
#define SEPARATION_DISTANCE 18.0f
#define ALIGNMENT_DISTANCE 8.0f
#define COHESION_DISTANCE 22.0f
#define SEPARATION_WEIGHT 1.2f
#define ALIGNMENT_WEIGHT 0.8f
#define COHESION_WEIGHT 0.9f
#define EDGE_DISTANCE 12.0f
#define EDGE_WEIGHT 1.0f
#define BOID_TAIL_LENGTH 1.0f
#define TRAIL_LENGTH 2

// Spatial partitioning grid
#define GRID_CELL_SIZE 35
#define GRID_WIDTH (SCREEN_WIDTH / GRID_CELL_SIZE + 1)
#define GRID_HEIGHT (SCREEN_HEIGHT / GRID_CELL_SIZE + 1)
#define MAX_BOIDS_PER_CELL 10

struct GridCell {
    uint8_t boid_indices[MAX_BOIDS_PER_CELL];
    uint8_t count;
};

// Boid structure (optimized for cache efficiency)
struct Boid {
    float x, y;
    float vx, vy;
    float ax, ay;
    int8_t trail_x[TRAIL_LENGTH];
    int8_t trail_y[TRAIL_LENGTH];
    uint8_t trail_index;
};

Boid boids[NUM_BOIDS];
GridCell grid[GRID_WIDTH][GRID_HEIGHT];

// Button state tracking
static unsigned long lastButtonChangeTime = 0;
static const unsigned long DEBOUNCE_DELAY = 100;
static bool lastButtonState = HIGH;
static bool buttonCurrentlyPressed = false;
static bool lastDebouncedState = HIGH;

// Inline distance check (squared, to avoid sqrt)
inline float distanceSquared(float x1, float y1, float x2, float y2) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    return dx * dx + dy * dy;
}

// Function to limit a value to a maximum magnitude
inline float limitMagnitude(float val, float limit) {
    if (val > limit) return limit;
    if (val < -limit) return -limit;
    return val;
}

// Build spatial grid
void buildGrid() {
    // Clear grid
    for (int y = 0; y < GRID_HEIGHT; y++) {
        for (int x = 0; x < GRID_WIDTH; x++) {
            grid[x][y].count = 0;
        }
    }

    // Add boids to grid cells
    for (uint8_t i = 0; i < NUM_BOIDS; i++) {
        int cell_x = (int)boids[i].x / GRID_CELL_SIZE;
        int cell_y = (int)boids[i].y / GRID_CELL_SIZE;

        // Clamp to grid bounds
        if (cell_x < 0) cell_x = 0;
        if (cell_x >= GRID_WIDTH) cell_x = GRID_WIDTH - 1;
        if (cell_y < 0) cell_y = 0;
        if (cell_y >= GRID_HEIGHT) cell_y = GRID_HEIGHT - 1;

        GridCell& cell = grid[cell_x][cell_y];
        if (cell.count < MAX_BOIDS_PER_CELL) {
            cell.boid_indices[cell.count++] = i;
        }
    }
}

// Get nearby boids from grid
void getNearbyBoids(int index, uint8_t* nearby, uint8_t& count, float max_dist_sq) {
    count = 0;
    float max_dist_sq_limit = max_dist_sq + 100; // Small buffer

    int cell_x = (int)boids[index].x / GRID_CELL_SIZE;
    int cell_y = (int)boids[index].y / GRID_CELL_SIZE;

    // Clamp to grid bounds
    if (cell_x < 0) cell_x = 0;
    if (cell_x >= GRID_WIDTH) cell_x = GRID_WIDTH - 1;
    if (cell_y < 0) cell_y = 0;
    if (cell_y >= GRID_HEIGHT) cell_y = GRID_HEIGHT - 1;

    // Check current cell and 8 neighbors
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            int nx = cell_x + dx;
            int ny = cell_y + dy;

            if (nx >= 0 && nx < GRID_WIDTH && ny >= 0 && ny < GRID_HEIGHT) {
                GridCell& neighbor_cell = grid[nx][ny];
                for (uint8_t j = 0; j < neighbor_cell.count; j++) {
                    uint8_t boid_idx = neighbor_cell.boid_indices[j];
                    if (boid_idx == index) continue;

                    float dist_sq = distanceSquared(boids[index].x, boids[index].y, boids[boid_idx].x, boids[boid_idx].y);
                    if (dist_sq < max_dist_sq_limit && count < 16) { // Limit to 16 nearby boids
                        nearby[count++] = boid_idx;
                    }
                }
            }
        }
    }
}

// Separation: steer to avoid crowding local flockmates
void separate(uint8_t index) {
    uint8_t nearby[16];
    uint8_t count = 0;
    float sep_dist_sq = SEPARATION_DISTANCE * SEPARATION_DISTANCE;

    getNearbyBoids(index, nearby, count, sep_dist_sq);

    float steerx = 0, steery = 0;
    int sep_count = 0;

    for (uint8_t j = 0; j < count; j++) {
        uint8_t i = nearby[j];
        float dist_sq = distanceSquared(boids[index].x, boids[index].y, boids[i].x, boids[i].y);

        if (dist_sq < sep_dist_sq && dist_sq > 0) {
            float dx = boids[index].x - boids[i].x;
            float dy = boids[index].y - boids[i].y;
            float len = sqrt(dist_sq);
            dx /= len;
            dy /= len;
            steerx += dx;
            steery += dy;
            sep_count++;
        }
    }

    if (sep_count > 0) {
        steerx /= sep_count;
        steery /= sep_count;
        steerx = limitMagnitude(steerx, MAX_FORCE);
        steery = limitMagnitude(steery, MAX_FORCE);
        boids[index].ax += steerx * SEPARATION_WEIGHT;
        boids[index].ay += steery * SEPARATION_WEIGHT;
    }
}

// Alignment: steer towards the average heading of local flockmates
void align(uint8_t index) {
    uint8_t nearby[16];
    uint8_t count = 0;
    float align_dist_sq = ALIGNMENT_DISTANCE * ALIGNMENT_DISTANCE;

    getNearbyBoids(index, nearby, count, align_dist_sq);

    float avgvx = 0, avgvy = 0;
    int align_count = 0;

    for (uint8_t j = 0; j < count; j++) {
        uint8_t i = nearby[j];
        float dist_sq = distanceSquared(boids[index].x, boids[index].y, boids[i].x, boids[i].y);

        if (dist_sq < align_dist_sq) {
            avgvx += boids[i].vx;
            avgvy += boids[i].vy;
            align_count++;
        }
    }

    if (align_count > 0) {
        avgvx /= align_count;
        avgvy /= align_count;
        avgvx = limitMagnitude(avgvx, MAX_FORCE);
        avgvy = limitMagnitude(avgvy, MAX_FORCE);
        boids[index].ax += avgvx * ALIGNMENT_WEIGHT;
        boids[index].ay += avgvy * ALIGNMENT_WEIGHT;
    }
}

// Cohesion: steer to move toward the average location of local flockmates
void cohesion(uint8_t index) {
    uint8_t nearby[16];
    uint8_t count = 0;
    float cohesion_dist_sq = COHESION_DISTANCE * COHESION_DISTANCE;

    getNearbyBoids(index, nearby, count, cohesion_dist_sq);

    float targetx = 0, targety = 0;
    int cohesion_count = 0;

    for (uint8_t j = 0; j < count; j++) {
        uint8_t i = nearby[j];
        float dist_sq = distanceSquared(boids[index].x, boids[index].y, boids[i].x, boids[i].y);

        if (dist_sq < cohesion_dist_sq) {
            targetx += boids[i].x;
            targety += boids[i].y;
            cohesion_count++;
        }
    }

    if (cohesion_count > 0) {
        targetx /= cohesion_count;
        targety /= cohesion_count;
        float dx = targetx - boids[index].x;
        float dy = targety - boids[index].y;
        float len_sq = dx * dx + dy * dy;
        if (len_sq > 0) {
            float len = sqrt(len_sq);
            dx /= len;
            dy /= len;
            dx = limitMagnitude(dx, MAX_FORCE);
            dy = limitMagnitude(dy, MAX_FORCE);
            boids[index].ax += dx * COHESION_WEIGHT;
            boids[index].ay += dy * COHESION_WEIGHT;
        }
    }
}

// Edges: steer away from screen boundaries, allowing slight off-screen movement
inline void avoidEdges(uint8_t index) {
    float steerx = 0, steery = 0;
    const float OFFSCREEN_ALLOWANCE = 6.0f; // Allow boids to go 6 pixels off screen

    // Left edge
    if (boids[index].x < -OFFSCREEN_ALLOWANCE) {
        steerx += 2.0f; // Stronger force when well off screen
    } else if (boids[index].x < EDGE_DISTANCE) {
        steerx += 1.0f; // Normal avoidance near edge
    }
    // Right edge
    if (boids[index].x > SCREEN_WIDTH + OFFSCREEN_ALLOWANCE) {
        steerx -= 2.0f;
    } else if (boids[index].x > SCREEN_WIDTH - EDGE_DISTANCE) {
        steerx -= 1.0f;
    }
    // Top edge
    if (boids[index].y < -OFFSCREEN_ALLOWANCE) {
        steery += 2.0f;
    } else if (boids[index].y < EDGE_DISTANCE) {
        steery += 1.0f;
    }
    // Bottom edge
    if (boids[index].y > SCREEN_HEIGHT + OFFSCREEN_ALLOWANCE) {
        steery -= 2.0f;
    } else if (boids[index].y > SCREEN_HEIGHT - EDGE_DISTANCE) {
        steery -= 1.0f;
    }

    steerx = limitMagnitude(steerx, MAX_FORCE);
    steery = limitMagnitude(steery, MAX_FORCE);
    boids[index].ax += steerx * EDGE_WEIGHT;
    boids[index].ay += steery * EDGE_WEIGHT;
}

// Update boid position and velocity
inline void updateBoid(uint8_t index) {
    // Apply forces
    separate(index);
    align(index);
    cohesion(index);
    avoidEdges(index);

    // Update velocity
    boids[index].vx += boids[index].ax;
    boids[index].vy += boids[index].ay;

    // Limit speed
    float speed_sq = boids[index].vx * boids[index].vx + boids[index].vy * boids[index].vy;
    if (speed_sq > MAX_SPEED * MAX_SPEED) {
        float speed = sqrt(speed_sq);
        boids[index].vx = (boids[index].vx / speed) * MAX_SPEED;
        boids[index].vy = (boids[index].vy / speed) * MAX_SPEED;
    }

    // Update position
    boids[index].x += boids[index].vx;
    boids[index].y += boids[index].vy;

    // Update trail
    boids[index].trail_x[boids[index].trail_index] = (int8_t)boids[index].x;
    boids[index].trail_y[boids[index].trail_index] = (int8_t)boids[index].y;
    boids[index].trail_index = (boids[index].trail_index + 1) % TRAIL_LENGTH;

    // Reset acceleration
    boids[index].ax = 0;
    boids[index].ay = 0;

    // Clamp to screen boundaries
    if (boids[index].x < 0) boids[index].x = 0;
    if (boids[index].x > SCREEN_WIDTH) boids[index].x = SCREEN_WIDTH;
    if (boids[index].y < 0) boids[index].y = 0;
    if (boids[index].y > SCREEN_HEIGHT) boids[index].y = SCREEN_HEIGHT;
}

// Initialize boids with random positions and velocities
void initializeBoids() {
    for (uint8_t i = 0; i < NUM_BOIDS; i++) {
        boids[i].x = random(10, SCREEN_WIDTH - 10);
        boids[i].y = random(10, SCREEN_HEIGHT - 10);
        boids[i].vx = random(-20, 20) / 10.0f;
        boids[i].vy = random(-20, 20) / 10.0f;
        boids[i].ax = 0;
        boids[i].ay = 0;
        
        // Initialize trail
        boids[i].trail_index = 0;
        for (uint8_t j = 0; j < TRAIL_LENGTH; j++) {
            boids[i].trail_x[j] = (int8_t)boids[i].x;
            boids[i].trail_y[j] = (int8_t)boids[i].y;
        }
    }
}

// Draw all boids as directional lines with trails
void drawBoids() {
    display.clearDisplay();
    
    // Draw trails first (behind birds)
    for (uint8_t i = 0; i < NUM_BOIDS; i++) {
        for (uint8_t j = 0; j < TRAIL_LENGTH - 1; j++) {
            uint8_t trail_idx = (boids[i].trail_index + j) % TRAIL_LENGTH;
            uint8_t next_idx = (trail_idx + 1) % TRAIL_LENGTH;
            
            int x1 = boids[i].trail_x[trail_idx];
            int y1 = boids[i].trail_y[trail_idx];
            int x2 = boids[i].trail_x[next_idx];
            int y2 = boids[i].trail_y[next_idx];
            
            // Only draw if points are valid and on screen
            if (x1 >= 0 && x1 < SCREEN_WIDTH && y1 >= 0 && y1 < SCREEN_HEIGHT &&
                x2 >= 0 && x2 < SCREEN_WIDTH && y2 >= 0 && y2 < SCREEN_HEIGHT) {
                display.drawLine(x1, y1, x2, y2, SSD1306_WHITE);
            }
        }
    }
    
    // Draw birds on top
    for (uint8_t i = 0; i < NUM_BOIDS; i++) {
        int x = (int)boids[i].x;
        int y = (int)boids[i].y;
        
        // Calculate direction point based on velocity and tail length
        float speed = sqrt(boids[i].vx * boids[i].vx + boids[i].vy * boids[i].vy);
        int x2 = x, y2 = y;
        
        if (speed > 0.1f) {
            // Normalize velocity and scale to tail length
            x2 = x + (int)(boids[i].vx / speed * BOID_TAIL_LENGTH);
            y2 = y + (int)(boids[i].vy / speed * BOID_TAIL_LENGTH);
        }
        
        // Draw line from current position to direction point
        if (x >= 0 && x < SCREEN_WIDTH && y >= 0 && y < SCREEN_HEIGHT) {
            // Clamp end point to screen bounds
            x2 = constrain(x2, 0, SCREEN_WIDTH - 1);
            y2 = constrain(y2, 0, SCREEN_HEIGHT - 1);
            
            // Draw line from head to tail
            display.drawLine(x, y, x2, y2, SSD1306_WHITE);
        }
    }
    display.display();
}

// Update all boids
void updateAllBoids() {
    buildGrid();
    for (uint8_t i = 0; i < NUM_BOIDS; i++) {
        updateBoid(i);
    }
}

// Handle button press with debouncing
void handleButtonPress() {
    bool currentState = digitalRead(BUTTON_PIN);
    static bool debounceActive = false;

    // Detect when raw state changes
    if (currentState != lastDebouncedState && !debounceActive) {
        lastButtonChangeTime = millis();
        debounceActive = true;
    }

    // Wait for debounce period to complete
    if (debounceActive && (millis() - lastButtonChangeTime) > DEBOUNCE_DELAY) {
        debounceActive = false;
        lastDebouncedState = currentState;

        // Button press detected (transition to HIGH when pressed)
        if (currentState == HIGH) {
            if (!buttonCurrentlyPressed) {
                buttonCurrentlyPressed = true;
                initializeBoids();
            }
        } else {
            buttonCurrentlyPressed = false;
            if (currentState == LOW) {
            }
        }
    }
}

void setup() {
    Serial.begin(115200);
    delay(500);

    // Initialize I2C and display
    Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println(F("SSD1306 allocation failed"));
        for (;;)
            ;
    }

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);

    // Button setup
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    
    // Initialize button state to current reading to avoid false triggers
    lastButtonState = digitalRead(BUTTON_PIN);
    lastDebouncedState = lastButtonState;
    Serial.printf("Button initialized on pin %d\n", BUTTON_PIN);
    Serial.printf("Initial button state: %d\n", lastButtonState);

    // Initialize boids
    initializeBoids();

    Serial.println("Boids demo started");
}

void loop() {
    handleButtonPress();
    updateAllBoids();
    drawBoids();

    delay(1);
}