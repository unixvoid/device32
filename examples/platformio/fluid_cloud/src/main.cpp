#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define SDA_PIN 7
#define SCL_PIN 6
#define BUTTON_PIN 5

#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

const int MAX_PARTICLES = 32;
struct Particle {
  float x, y;
  float vx, vy;
  float r;
  float mass;
  float temp;
};
Particle P[MAX_PARTICLES];
int particleCount = 24;

const float G = 40.0f;
const float K_BOUY = 170.0f;
const float K_COHESE = 40.0f;
const float REST_DIST_FACTOR = 0.9f;
const float VISCOSITY = 0.05f;
const float DT = 0.016f;

const int TG_W = 12;
const int TG_H = 24;
float tgrid[TG_W * TG_H];
float tgrid_tmp[TG_W * TG_H];
const float T_AMBIENT = 800.0f;
const float T_BOTTOM = 1000.0f;
const float T_DIFF = 0.1f;
const float T_COOL_RATE = 0.004f;

const float METABALL_SCALE = 1.0f;
const float THRESHOLD = 10.0f;
const float INFLUENCE_MULT = 3.0f;
uint8_t pixelBuf[(SCREEN_WIDTH * SCREEN_HEIGHT) / 8];

inline int tg_idx(int gx, int gy){ return gy * TG_W + gx; }

void seedParticles(){
  for (int i=0;i<particleCount;i++){
    float rx = random(10, SCREEN_WIDTH - 10);
    float ry = random(SCREEN_HEIGHT/2, SCREEN_HEIGHT - 5);
    float rr = random(5, 12);
    P[i].x = rx;
    P[i].y = ry;
    P[i].vx = random(-10,10) * 0.02f;
    P[i].vy = random(-10,10) * 0.02f;
    P[i].r = rr;
    P[i].mass = rr * rr;
    P[i].temp = T_AMBIENT - 100.0f;
  }
}

void initTGrid(){
  for (int y=0;y<TG_H;y++){
    for (int x=0;x<TG_W;x++){
      tgrid[tg_idx(x,y)] = T_AMBIENT;
    }
  }
}

float sampleGridTemp(float px, float py){
  int gx = constrain((int)(px * TG_W / SCREEN_WIDTH), 0, TG_W-1);
  int gy = constrain((int)(py * TG_H / SCREEN_HEIGHT), 0, TG_H-1);
  return tgrid[tg_idx(gx,gy)];
}

void updateTempGrid(){
  for (int gy=0; gy<TG_H; gy++){
    for (int gx=0; gx<TG_W; gx++){
      int idx = tg_idx(gx,gy);
      float v = tgrid[idx];
      float sumNeigh = 0.0f;
      int ncount = 0;
      if (gx>0){ sumNeigh += tgrid[tg_idx(gx-1,gy)]; ncount++; }
      if (gx<TG_W-1){ sumNeigh += tgrid[tg_idx(gx+1,gy)]; ncount++; }
      if (gy>0){ sumNeigh += tgrid[tg_idx(gx,gy-1)]; ncount++; }
      if (gy<TG_H-1){ sumNeigh += tgrid[tg_idx(gx,gy+1)]; ncount++; }
      float neighAvg = (ncount>0) ? (sumNeigh / ncount) : v;
      float dv = T_DIFF * (neighAvg - v);
      float nv = v + dv;
      if (gy >= TG_H - 2) {
        nv += (T_BOTTOM - nv) * 0.02f;
      } else if (gy >= TG_H - 5) {
        nv += (T_BOTTOM * 0.6f - nv) * 0.01f;
      } else {
        nv += (T_AMBIENT - nv) * T_COOL_RATE;
      }
      tgrid_tmp[idx] = nv;
    }
  }
  for (int i=0;i<TG_W*TG_H;i++) tgrid[i] = tgrid_tmp[i];

  for (int i=0;i<particleCount;i++){
    int gx = constrain((int)(P[i].x * TG_W / SCREEN_WIDTH), 0, TG_W-1);
    int gy = constrain((int)(P[i].y * TG_H / SCREEN_HEIGHT), 0, TG_H-1);
    int idx = tg_idx(gx,gy);
    float gT = tgrid[idx];
    float exch = 0.05f;
    float newPgT = P[i].temp + exch * (gT - P[i].temp);
    float newG = gT + exch * (P[i].temp - gT) * 0.5f;
    P[i].temp = newPgT;
    tgrid[idx] = newG;
  }
}

void physicsStep(){
  for (int i=0;i<particleCount;i++){
    float localT = sampleGridTemp(P[i].x, P[i].y);
    float buoy = K_BOUY * (P[i].temp - T_AMBIENT);
    float fx = -buoy - P[i].mass * G;
    float ax = fx / P[i].mass, ay = 0.0f;
    
    float sideForce = 50.0f * sin(millis() * 0.003f);  // oscillates side to side
    ay += sideForce / P[i].mass;

    // pairwise cohesion/repulsion and viscous damping
    for (int j=0;j<particleCount;j++){
      if (j==i) continue;
      float dx = P[j].x - P[i].x;
      float dy = P[j].y - P[i].y;
      float d2 = dx*dx + dy*dy;
      if (d2 < 0.0001f) continue;
      float d = sqrt(d2);
      float rest = (P[i].r + P[j].r) * REST_DIST_FACTOR;
      if (d < (P[i].r + P[j].r) * INFLUENCE_MULT){
        float diff = d - rest;
        float fs = K_COHESE * diff;
        float fx = fs * (dx / d);
        float fy2 = fs * (dy / d);
        ax += fx / P[i].mass;
        ay += fy2 / P[i].mass;
        float rvx = P[j].vx - P[i].vx;
        float rvy = P[j].vy - P[i].vy;
        ax += (rvx * VISCOSITY) / P[i].mass;
        ay += (rvy * VISCOSITY) / P[i].mass;
      }
    }

    // integrate velocity
    P[i].vx += ax * DT;
    P[i].vy += ay * DT;
  }

  // integrate positions, collisions, temperature effects
  for (int i=0;i<particleCount;i++){
    P[i].x += P[i].vx * DT;
    P[i].y += P[i].vy * DT;

    // walls
    if (P[i].x < P[i].r){
      P[i].x = P[i].r;
      P[i].vx *= -0.3f;
    } else if (P[i].x > SCREEN_WIDTH - P[i].r){
      P[i].x = SCREEN_WIDTH - P[i].r;
      P[i].vx *= -0.3f;
    }
    if (P[i].y < P[i].r){
      P[i].y = P[i].r;
      P[i].vy *= -0.3f;
    } else if (P[i].y > SCREEN_HEIGHT - P[i].r){
      P[i].y = SCREEN_HEIGHT - P[i].r;
      P[i].temp += 0.5f;
      P[i].vy *= -0.25f;
      P[i].y = SCREEN_HEIGHT - P[i].r;
    }

    float scale = 1.0f + (P[i].temp - T_AMBIENT) * 0.003f;
    if (scale < 0.8f) scale = 0.8f;
    P[i].r *= (1.0f - 0.001f) + 0.001f * scale;
    if (P[i].r < 3.0f) P[i].r = 3.0f;
    if (P[i].r > 20.0f) P[i].r = 20.0f;
    P[i].temp += (T_AMBIENT - P[i].temp) * 0.0001f;
  }
}

// Render metaballs to display
void renderMetaballs(){
  display.clearDisplay();
  static float field[SCREEN_WIDTH * SCREEN_HEIGHT];
  int W = SCREEN_WIDTH, H = SCREEN_HEIGHT;
  int WH = W * H;
  for (int i=0;i<WH;i++) field[i] = 0.0f;

  for (int p=0;p<particleCount;p++){
    int ix0 = max(0, (int)floor(P[p].x - P[p].r * INFLUENCE_MULT));
    int iy0 = max(0, (int)floor(P[p].y - P[p].r * INFLUENCE_MULT));
    int ix1 = min(W-1, (int)ceil(P[p].x + P[p].r * INFLUENCE_MULT));
    int iy1 = min(H-1, (int)ceil(P[p].y + P[p].r * INFLUENCE_MULT));
    float pr = P[p].r;
    float pr2 = pr*pr;
    for (int yy = iy0; yy <= iy1; yy++){
      int base = yy * W;
      for (int xx = ix0; xx <= ix1; xx++){
        float dx = xx + 0.5f - P[p].x;
        float dy = yy + 0.5f - P[p].y;
        float d2 = dx*dx + dy*dy + 0.0001f;
        float contrib = (pr2 * METABALL_SCALE) / d2;
        field[base + xx] += contrib;
      }
    }
  }

  float glowThresh = THRESHOLD * 0.4f;
  for (int y=0;y<H;y++){
    for (int x=0;x<W;x++){
      float v = field[y*W + x];
      if (v >= THRESHOLD){
        display.drawPixel(x,y,SSD1306_WHITE);
      } else if (v >= glowThresh){
        if (((x ^ y) & 1) == 0) display.drawPixel(x,y,SSD1306_WHITE);
      }
    }
  }

  display.display();
}

unsigned long lastSim = 0;
unsigned long lastDraw = 0;
unsigned long lastButtonCheck = 0;
bool buttonPressed = false;

void setup() {
  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    while (1);
  }
  display.clearDisplay();
  display.setRotation(0);
  initTGrid();
  seedParticles();

  lastSim = millis();
  lastDraw = millis();
  lastButtonCheck = millis();
}

void loop(){
  unsigned long now = millis();
  
  if (now - lastButtonCheck >= 20) {
    bool buttonState = digitalRead(BUTTON_PIN) == LOW;
    if (buttonState && !buttonPressed) {
      initTGrid();
      seedParticles();
      Serial.println("Simulation reset");
    }
    buttonPressed = buttonState;
    lastButtonCheck = now;
  }
  
  if (now - lastSim >= (unsigned long)(DT * 1000.0f)) {
    updateTempGrid();
    physicsStep();
    lastSim = now;
  }
  if (now - lastDraw >= 30) {
    renderMetaballs();
    lastDraw = now;
  }

  if (random(0,1000) < 2){
    int cellx = random(0, TG_W);
    int celly = TG_H-1 - random(0,2);
    tgrid[tg_idx(cellx,celly)] += random(5,30);
  }
}