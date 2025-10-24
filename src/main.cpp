#include <M5Stack.h>
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

Player player;
Platform platforms[8];
int platformCount = 0;
bool needsFullRedraw = true;

// Button states
uint8_t facesData = 0xFF;

void readFacesButtons() {
    Wire.requestFrom(FACES_ADDR, 1);
    if (Wire.available()) {
        facesData = Wire.read();
    }
}

void setupGame() {
    // Initialize player
    player.x = 50;
    player.y = 100;
    player.lastX = 50;
    player.lastY = 100;
    player.vx = 0;
    player.vy = 0;
    player.onGround = false;
    player.color = TFT_GREEN;
    needsFullRedraw = true;

    // Create platforms
    platformCount = 0;

    // Ground
    platforms[platformCount].x = 0;
    platforms[platformCount].y = 220;
    platforms[platformCount].w = 320;
    platforms[platformCount].h = 20;
    platforms[platformCount].color = TFT_BROWN;
    platformCount++;

    // Floating platforms
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
    // Erase old player position
    M5.Lcd.fillRect((int)x - PLAYER_SIZE/2 - 1,
                    (int)y - PLAYER_SIZE/2 - 1,
                    PLAYER_SIZE + 2, PLAYER_SIZE + 2, TFT_SKYBLUE);
}

void drawPlayer() {
    // Draw player body
    M5.Lcd.fillRect((int)player.x - PLAYER_SIZE/2,
                    (int)player.y - PLAYER_SIZE/2,
                    PLAYER_SIZE, PLAYER_SIZE, player.color);

    // Draw eyes
    M5.Lcd.fillCircle((int)player.x - 3, (int)player.y - 2, 2, TFT_WHITE);
    M5.Lcd.fillCircle((int)player.x + 3, (int)player.y - 2, 2, TFT_WHITE);
    M5.Lcd.fillCircle((int)player.x - 3, (int)player.y - 2, 1, TFT_BLACK);
    M5.Lcd.fillCircle((int)player.x + 3, (int)player.y - 2, 1, TFT_BLACK);
}

bool checkCollision(float px, float py) {
    for (int i = 0; i < platformCount; i++) {
        if (px + PLAYER_SIZE/2 > platforms[i].x &&
            px - PLAYER_SIZE/2 < platforms[i].x + platforms[i].w &&
            py + PLAYER_SIZE/2 > platforms[i].y &&
            py - PLAYER_SIZE/2 < platforms[i].y + platforms[i].h) {
            return true;
        }
    }
    return false;
}

void updatePlayer() {
    // Save last position
    player.lastX = player.x;
    player.lastY = player.y;

    // Read Faces buttons
    M5.update();
    readFacesButtons();

    // Correct Faces GameBoy bit mapping (active low, so 0 = pressed)
    bool facesUp = !(facesData & 0x01);     // Bit 0: UP
    bool facesDown = !(facesData & 0x02);   // Bit 1: DOWN
    bool facesLeft = !(facesData & 0x04);   // Bit 2: LEFT
    bool facesRight = !(facesData & 0x08);  // Bit 3: RIGHT
    bool facesA = !(facesData & 0x10);      // Bit 4: A button
    bool facesB = !(facesData & 0x20);      // Bit 5: B button

    // B button for running faster
    float currentSpeed = (facesB) ? RUN_SPEED : MOVE_SPEED;

    // D-pad horizontal movement (Faces or M5Stack buttons)
    if (facesLeft || M5.BtnA.isPressed()) {
        player.vx = -currentSpeed;
    } else if (facesRight || M5.BtnC.isPressed()) {
        player.vx = currentSpeed;
    } else {
        player.vx = 0;
    }

    // Jump with A button or UP (Faces or M5Stack button B)
    static bool lastButtonA = false;
    bool buttonA = facesA || facesUp || M5.BtnB.isPressed();
    if (buttonA && !lastButtonA && player.onGround) {
        player.vy = JUMP_STRENGTH;
        player.onGround = false;
    }
    lastButtonA = buttonA;

    // Apply gravity
    player.vy += GRAVITY;

    // Update position
    float newX = player.x + player.vx;
    float newY = player.y + player.vy;

    // Keep player on screen horizontally
    if (newX < PLAYER_SIZE/2) newX = PLAYER_SIZE/2;
    if (newX > SCREEN_WIDTH - PLAYER_SIZE/2) newX = SCREEN_WIDTH - PLAYER_SIZE/2;

    // Check vertical collisions
    player.onGround = false;

    if (player.vy > 0) {  // Falling
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
    } else if (player.vy < 0) {  // Jumping up
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

    // Respawn if fallen off screen
    if (player.y > SCREEN_HEIGHT + 20) {
        setupGame();
    }
}

void setup() {
    M5.begin();
    M5.Power.begin();

    // Initialize I2C for Faces
    Wire.begin();

    // Properly disable speaker to prevent buzzing
    M5.Speaker.mute();
    M5.Speaker.end();

    // Set up display
    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.setTextSize(2);

    // Show title
    M5.Lcd.setCursor(60, 100);
    M5.Lcd.println("PLATFORM DEMO");
    M5.Lcd.setCursor(10, 130);
    M5.Lcd.setTextSize(1);
    M5.Lcd.println("Faces: D-Pad=Move, A/UP=Jump, B=Run");
    M5.Lcd.setCursor(10, 145);
    M5.Lcd.println("M5: A=Left, B=Jump, C=Right");
    delay(3000);

    setupGame();
}

void loop() {

    // Full redraw if needed
    if (needsFullRedraw) {
        M5.Lcd.fillScreen(TFT_SKYBLUE);

        // Draw title
        M5.Lcd.setTextSize(1);
        M5.Lcd.setCursor(5, 5);
        M5.Lcd.setTextColor(TFT_WHITE, TFT_SKYBLUE);
        M5.Lcd.print("Platform Game");

        // Draw all platforms
        drawPlatforms();
        needsFullRedraw = false;
    }

    // Update game
    updatePlayer();

    // Erase player at old position
    erasePlayer(player.lastX, player.lastY);

    // Redraw any platforms that might have been erased
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

    // Draw player at new position
    drawPlayer();

    // Small delay for game loop
    delay(20);
}
