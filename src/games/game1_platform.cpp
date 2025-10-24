#include "game1_platform.h"
#include <Wire.h>

// Game constants
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240
#define GRAVITY 0.5
#define JUMP_STRENGTH -8.0
#define MOVE_SPEED 3.0
#define RUN_SPEED 6.0
#define PLAYER_SIZE 12

// Custom colors
#define TFT_BROWN 0x79E0
#define TFT_SKYBLUE 0x867D

// Faces GameBoy I2C address
#define FACES_ADDR 0x08

// Player structure
struct Player {
    float x, y;
    float lastX, lastY;
    float vx, vy;
    bool onGround;
    uint16_t color;
};

// Platform structure
struct Platform {
    int16_t x, y, w, h;
    uint16_t color;
};

static Player player;
static Platform platforms[8];
static int platformCount = 0;
static bool needsFullRedraw = true;
static uint8_t facesData = 0xFF;

static void readFacesButtons() {
    Wire.requestFrom(FACES_ADDR, 1);
    if (Wire.available()) {
        facesData = Wire.read();
    }
}

void setupGameState() {
    player.x = 50;
    player.y = 100;
    player.lastX = 50;
    player.lastY = 100;
    player.vx = 0;
    player.vy = 0;
    player.onGround = false;
    player.color = TFT_GREEN;
    needsFullRedraw = true;

    platformCount = 0;
    platforms[platformCount] = {0, 220, 320, 20, TFT_BROWN};
    platformCount++;
    platforms[platformCount] = {40, 180, 80, 10, TFT_ORANGE};
    platformCount++;
    platforms[platformCount] = {160, 150, 100, 10, TFT_ORANGE};
    platformCount++;
    platforms[platformCount] = {80, 120, 60, 10, TFT_ORANGE};
    platformCount++;
    platforms[platformCount] = {200, 90, 80, 10, TFT_ORANGE};
    platformCount++;
    platforms[platformCount] = {20, 60, 70, 10, TFT_ORANGE};
    platformCount++;
    platforms[platformCount] = {250, 140, 60, 10, TFT_ORANGE};
    platformCount++;
    platforms[platformCount] = {140, 200, 50, 10, TFT_ORANGE};
    platformCount++;
}

void drawPlatforms() {
    for (int i = 0; i < platformCount; i++) {
        M5.Lcd.fillRect(platforms[i].x, platforms[i].y,
                       platforms[i].w, platforms[i].h,
                       platforms[i].color);
    }
}

void erasePlayer(float x, float y) {
    M5.Lcd.fillRect((int)x - PLAYER_SIZE/2 - 1,
                    (int)y - PLAYER_SIZE/2 - 1,
                    PLAYER_SIZE + 2, PLAYER_SIZE + 2, TFT_SKYBLUE);
}

void drawPlayer() {
    M5.Lcd.fillRect((int)player.x - PLAYER_SIZE/2,
                    (int)player.y - PLAYER_SIZE/2,
                    PLAYER_SIZE, PLAYER_SIZE, player.color);

    M5.Lcd.fillCircle((int)player.x - 3, (int)player.y - 2, 2, TFT_WHITE);
    M5.Lcd.fillCircle((int)player.x + 3, (int)player.y - 2, 2, TFT_WHITE);
    M5.Lcd.fillCircle((int)player.x - 3, (int)player.y - 2, 1, TFT_BLACK);
    M5.Lcd.fillCircle((int)player.x + 3, (int)player.y - 2, 1, TFT_BLACK);
}

void updatePlayer() {
    player.lastX = player.x;
    player.lastY = player.y;

    M5.update();
    readFacesButtons();

    bool facesUp = !(facesData & 0x01);
    bool facesLeft = !(facesData & 0x04);
    bool facesRight = !(facesData & 0x08);
    bool facesA = !(facesData & 0x10);
    bool facesB = !(facesData & 0x20);

    float currentSpeed = (facesB) ? RUN_SPEED : MOVE_SPEED;

    if (facesLeft || M5.BtnA.isPressed()) {
        player.vx = -currentSpeed;
    } else if (facesRight || M5.BtnC.isPressed()) {
        player.vx = currentSpeed;
    } else {
        player.vx = 0;
    }

    static bool lastButtonA = false;
    bool buttonA = facesA || facesUp || M5.BtnB.isPressed();
    if (buttonA && !lastButtonA && player.onGround) {
        player.vy = JUMP_STRENGTH;
        player.onGround = false;
    }
    lastButtonA = buttonA;

    player.vy += GRAVITY;

    float newX = player.x + player.vx;
    float newY = player.y + player.vy;

    if (newX < PLAYER_SIZE/2) newX = PLAYER_SIZE/2;
    if (newX > SCREEN_WIDTH - PLAYER_SIZE/2) newX = SCREEN_WIDTH - PLAYER_SIZE/2;

    player.onGround = false;

    if (player.vy > 0) {
        for (int i = 0; i < platformCount; i++) {
            if (newX + PLAYER_SIZE/2 > platforms[i].x &&
                newX - PLAYER_SIZE/2 < platforms[i].x + platforms[i].w) {
                if (player.y + PLAYER_SIZE/2 <= platforms[i].y &&
                    newY + PLAYER_SIZE/2 >= platforms[i].y) {
                    newY = platforms[i].y - PLAYER_SIZE/2;
                    player.vy = 0;
                    player.onGround = true;
                    break;
                }
            }
        }
    } else if (player.vy < 0) {
        for (int i = 0; i < platformCount; i++) {
            if (newX + PLAYER_SIZE/2 > platforms[i].x &&
                newX - PLAYER_SIZE/2 < platforms[i].x + platforms[i].w) {
                if (player.y - PLAYER_SIZE/2 >= platforms[i].y + platforms[i].h &&
                    newY - PLAYER_SIZE/2 <= platforms[i].y + platforms[i].h) {
                    newY = platforms[i].y + platforms[i].h + PLAYER_SIZE/2;
                    player.vy = 0;
                    break;
                }
            }
        }
    }

    player.x = newX;
    player.y = newY;

    if (player.y > SCREEN_HEIGHT + 20) {
        setupGameState();
    }
}

void game1Setup() {
    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(40, 100);
    M5.Lcd.println("PLATFORM GAME");
    M5.Lcd.setTextSize(1);
    M5.Lcd.setCursor(10, 130);
    M5.Lcd.println("D-Pad:Move A/UP:Jump B:Run");
    delay(2000);

    setupGameState();
}

void game1Loop() {
    if (needsFullRedraw) {
        M5.Lcd.fillScreen(TFT_SKYBLUE);
        M5.Lcd.setTextSize(1);
        M5.Lcd.setCursor(5, 5);
        M5.Lcd.setTextColor(TFT_WHITE, TFT_SKYBLUE);
        M5.Lcd.print("Game 1: Platform");
        drawPlatforms();
        needsFullRedraw = false;
    }

    updatePlayer();
    erasePlayer(player.lastX, player.lastY);

    for (int i = 0; i < platformCount; i++) {
        if ((int)player.lastX + PLAYER_SIZE/2 + 1 >= platforms[i].x &&
            (int)player.lastX - PLAYER_SIZE/2 - 1 <= platforms[i].x + platforms[i].w &&
            (int)player.lastY + PLAYER_SIZE/2 + 1 >= platforms[i].y &&
            (int)player.lastY - PLAYER_SIZE/2 - 1 <= platforms[i].y + platforms[i].h) {
            M5.Lcd.fillRect(platforms[i].x, platforms[i].y,
                           platforms[i].w, platforms[i].h,
                           platforms[i].color);
        }
    }

    drawPlayer();
    delay(20);
}
