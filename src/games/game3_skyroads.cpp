#include "game3_skyroads.h"
#include <Wire.h>

// Faces GameBoy I2C address
#define FACES_ADDR 0x08

// Game constants
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240
#define TRACK_ROWS 12
#define TRACK_LANES 5
#define BASE_SPEED 3.0
#define BOOST_SPEED 6.0
#define BRAKE_SPEED 1.5
#define JUMP_DURATION 20
#define BOOST_DURATION 30

// Custom colors
#define TFT_DARKBLUE 0x0010
#define TFT_GRAY 0x7BEF
#define TFT_DARKGRAY 0x39E7
#define TFT_SPACE 0x0008

// Tile types
enum TileType {
    TILE_NORMAL = 0,
    TILE_SPEED,
    TILE_DEADLY,
    TILE_JUMP,
    TILE_GAP
};

// Track tile structure
struct Tile {
    TileType type;
    uint16_t color;
};

// Ship structure
struct Ship {
    float lane;          // 0 to TRACK_LANES-1 (float for smooth movement)
    float targetLane;    // Target lane for smooth transition
    bool jumping;
    int jumpCounter;
    uint16_t color;
};

// Game state
static Ship ship;
static Tile track[TRACK_ROWS][TRACK_LANES];
static float scrollOffset = 0.0;
static float currentSpeed = BASE_SPEED;
static int boostCounter = 0;
static int score = 0;
static int distance = 0;
static int lives = 3;
static bool gameOver = false;
static uint8_t facesData = 0xFF;
static int invulnerable = 0;  // Invulnerability frames after hit

// Forward declarations
void checkCollision();

static void readFacesButtons() {
    Wire.requestFrom(FACES_ADDR, 1);
    if (Wire.available()) {
        facesData = Wire.read();
    }
}

uint16_t getTileColor(TileType type) {
    switch (type) {
        case TILE_NORMAL: return TFT_GRAY;
        case TILE_SPEED: return TFT_GREEN;
        case TILE_DEADLY: return TFT_RED;
        case TILE_JUMP: return TFT_CYAN;
        case TILE_GAP: return TFT_BLACK;
        default: return TFT_GRAY;
    }
}

void generateTrackRow(int row) {
    // Generate random track with patterns
    for (int lane = 0; lane < TRACK_LANES; lane++) {
        int rand_val = random(100);

        if (rand_val < 60) {
            track[row][lane].type = TILE_NORMAL;
        } else if (rand_val < 70) {
            track[row][lane].type = TILE_SPEED;
        } else if (rand_val < 80) {
            track[row][lane].type = TILE_JUMP;
        } else if (rand_val < 88) {
            track[row][lane].type = TILE_DEADLY;
        } else {
            track[row][lane].type = TILE_GAP;
        }

        track[row][lane].color = getTileColor(track[row][lane].type);
    }

    // Ensure at least 2 safe tiles per row (no impossible situations)
    int safeCount = 0;
    for (int lane = 0; lane < TRACK_LANES; lane++) {
        if (track[row][lane].type == TILE_NORMAL ||
            track[row][lane].type == TILE_SPEED ||
            track[row][lane].type == TILE_JUMP) {
            safeCount++;
        }
    }

    if (safeCount < 2) {
        // Make first two lanes safe
        track[row][0].type = TILE_NORMAL;
        track[row][0].color = getTileColor(TILE_NORMAL);
        track[row][1].type = TILE_NORMAL;
        track[row][1].color = getTileColor(TILE_NORMAL);
    }
}

void initializeTrack() {
    for (int row = 0; row < TRACK_ROWS; row++) {
        generateTrackRow(row);
    }

    // Make first 3 rows all normal for safe start
    for (int row = 0; row < 3; row++) {
        for (int lane = 0; lane < TRACK_LANES; lane++) {
            track[row][lane].type = TILE_NORMAL;
            track[row][lane].color = getTileColor(TILE_NORMAL);
        }
    }
}

static void resetGame() {
    ship.lane = TRACK_LANES / 2.0;
    ship.targetLane = ship.lane;
    ship.jumping = false;
    ship.jumpCounter = 0;
    ship.color = TFT_YELLOW;

    scrollOffset = 0.0;
    currentSpeed = BASE_SPEED;
    boostCounter = 0;
    score = 0;
    distance = 0;
    lives = 3;
    gameOver = false;
    invulnerable = 0;

    initializeTrack();
}

void drawTile(int row, int lane, uint16_t color) {
    // Calculate perspective dimensions
    float rowProgress = (float)row / (TRACK_ROWS - 1);

    // Y position (from bottom to top of screen)
    int yBottom = SCREEN_HEIGHT - 40 - row * 15;
    int yTop = yBottom - 14;

    if (yTop < 30) return;  // Don't draw beyond horizon

    // X position with perspective narrowing
    float trackWidthAtRow = 280 - rowProgress * 180;  // Track narrows into distance
    float laneWidth = trackWidthAtRow / TRACK_LANES;
    float trackLeft = (SCREEN_WIDTH - trackWidthAtRow) / 2;

    int xLeft = trackLeft + lane * laneWidth;
    int xRight = trackLeft + (lane + 1) * laneWidth;

    // Draw tile
    M5.Lcd.fillRect(xLeft + 1, yTop, xRight - xLeft - 2, yBottom - yTop, color);

    // Draw tile border for depth
    M5.Lcd.drawRect(xLeft, yTop, xRight - xLeft, yBottom - yTop, TFT_DARKGRAY);
}

void drawTrack() {
    // Draw space background
    M5.Lcd.fillRect(0, 30, SCREEN_WIDTH, SCREEN_HEIGHT - 70, TFT_SPACE);

    // Draw stars (simple)
    for (int i = 0; i < 30; i++) {
        int sx = random(SCREEN_WIDTH);
        int sy = random(30, SCREEN_HEIGHT - 40);
        M5.Lcd.drawPixel(sx, sy, TFT_WHITE);
    }

    // Draw horizon line
    M5.Lcd.drawLine(0, 30, SCREEN_WIDTH, 30, TFT_DARKBLUE);

    // Draw all tiles from back to front
    for (int row = TRACK_ROWS - 1; row >= 0; row--) {
        for (int lane = 0; lane < TRACK_LANES; lane++) {
            drawTile(row, lane, track[row][lane].color);
        }
    }
}

void drawShip() {
    // Calculate ship position on screen
    float rowProgress = 1.0 / (TRACK_ROWS - 1);
    float trackWidthAtShip = 280 - rowProgress * 180;
    float laneWidth = trackWidthAtShip / TRACK_LANES;
    float trackLeft = (SCREEN_WIDTH - trackWidthAtShip) / 2;

    int shipX = trackLeft + ship.lane * laneWidth + laneWidth / 2;
    int shipY = SCREEN_HEIGHT - 55;

    // Draw ship as a triangle/arrow
    int shipSize = 12;

    // Jump offset
    int jumpOffset = 0;
    if (ship.jumping) {
        jumpOffset = -10;
    }

    shipY += jumpOffset;

    // Draw ship shadow if jumping
    if (ship.jumping) {
        M5.Lcd.fillTriangle(
            shipX, SCREEN_HEIGHT - 55 + 5,
            shipX - shipSize/2, SCREEN_HEIGHT - 55 + shipSize + 5,
            shipX + shipSize/2, SCREEN_HEIGHT - 55 + shipSize + 5,
            TFT_DARKGRAY
        );
    }

    // Invulnerability flashing
    if (invulnerable > 0 && (invulnerable % 4) < 2) {
        return;  // Flash by not drawing
    }

    // Draw ship body (triangle pointing forward/up)
    M5.Lcd.fillTriangle(
        shipX, shipY,
        shipX - shipSize/2, shipY + shipSize,
        shipX + shipSize/2, shipY + shipSize,
        ship.color
    );

    // Draw ship cockpit
    M5.Lcd.fillCircle(shipX, shipY + 6, 3, TFT_CYAN);

    // Draw exhaust flames
    if (!ship.jumping) {
        M5.Lcd.fillRect(shipX - 2, shipY + shipSize, 4, 4, TFT_ORANGE);
        M5.Lcd.fillRect(shipX - 1, shipY + shipSize + 4, 2, 2, TFT_RED);
    }
}

void updateShip() {
    M5.update();
    readFacesButtons();

    bool facesLeft = !(facesData & 0x04);
    bool facesRight = !(facesData & 0x08);
    bool facesUp = !(facesData & 0x01);
    bool facesDown = !(facesData & 0x02);
    bool facesA = !(facesData & 0x10);
    bool facesB = !(facesData & 0x20);

    static bool lastLeft = false;
    static bool lastRight = false;
    static bool lastJump = false;
    static bool lastBoost = false;

    // Lane movement
    if ((facesLeft && !lastLeft) || M5.BtnA.wasPressed()) {
        if (ship.targetLane > 0) {
            ship.targetLane--;
        }
    }
    if ((facesRight && !lastRight) || M5.BtnC.wasPressed()) {
        if (ship.targetLane < TRACK_LANES - 1) {
            ship.targetLane++;
        }
    }

    lastLeft = facesLeft;
    lastRight = facesRight;

    // Smooth lane transition
    if (ship.lane < ship.targetLane) {
        ship.lane += 0.2;
        if (ship.lane > ship.targetLane) ship.lane = ship.targetLane;
    } else if (ship.lane > ship.targetLane) {
        ship.lane -= 0.2;
        if (ship.lane < ship.targetLane) ship.lane = ship.targetLane;
    }

    // Jump
    bool jumpPressed = facesA || facesUp || M5.BtnB.isPressed();
    if (jumpPressed && !lastJump && !ship.jumping) {
        ship.jumping = true;
        ship.jumpCounter = JUMP_DURATION;
    }
    lastJump = jumpPressed;

    if (ship.jumping) {
        ship.jumpCounter--;
        if (ship.jumpCounter <= 0) {
            ship.jumping = false;
        }
    }

    // Speed control
    if (facesB && boostCounter == 0) {
        boostCounter = BOOST_DURATION;
    }

    if (boostCounter > 0) {
        currentSpeed = BOOST_SPEED;
        boostCounter--;
    } else if (facesDown) {
        currentSpeed = BRAKE_SPEED;
    } else {
        currentSpeed = BASE_SPEED;
    }

    // Decrease invulnerability
    if (invulnerable > 0) {
        invulnerable--;
    }
}

void scrollTrack() {
    scrollOffset += currentSpeed;

    // When scrolled a full tile
    if (scrollOffset >= 15) {  // Tile height
        scrollOffset -= 15;
        distance++;
        score += 10;

        // Shift all rows down
        for (int row = 0; row < TRACK_ROWS - 1; row++) {
            for (int lane = 0; lane < TRACK_LANES; lane++) {
                track[row][lane] = track[row + 1][lane];
            }
        }

        // Generate new row at the back
        generateTrackRow(TRACK_ROWS - 1);

        // Check collision with new front row (row 0)
        checkCollision();
    }
}

void checkCollision() {
    if (ship.jumping || invulnerable > 0) {
        return;  // No collision while jumping or invulnerable
    }

    int currentLane = (int)(ship.lane + 0.5);  // Round to nearest lane
    TileType currentTile = track[0][currentLane].type;

    switch (currentTile) {
        case TILE_DEADLY:
            // Hit deadly tile
            lives--;
            invulnerable = 60;  // 1 second of invulnerability
            if (lives <= 0) {
                gameOver = true;
            }
            break;

        case TILE_GAP:
            // Fell into gap
            lives--;
            invulnerable = 60;
            if (lives <= 0) {
                gameOver = true;
            }
            break;

        case TILE_SPEED:
            // Speed boost tile
            score += 20;
            break;

        case TILE_JUMP:
            // Auto jump on jump pad
            if (!ship.jumping) {
                ship.jumping = true;
                ship.jumpCounter = JUMP_DURATION;
                score += 15;
            }
            break;

        case TILE_NORMAL:
        default:
            // Safe
            break;
    }
}

void drawHUD() {
    M5.Lcd.fillRect(0, 0, SCREEN_WIDTH, 25, TFT_BLACK);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);

    M5.Lcd.setCursor(5, 5);
    M5.Lcd.print("Score:");
    M5.Lcd.print(score);

    M5.Lcd.setCursor(180, 5);
    M5.Lcd.print("Dist:");
    M5.Lcd.print(distance);

    M5.Lcd.setCursor(290, 5);
    M5.Lcd.print(lives);

    // Speed indicator
    M5.Lcd.fillRect(0, SCREEN_HEIGHT - 15, SCREEN_WIDTH, 15, TFT_BLACK);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setCursor(5, SCREEN_HEIGHT - 12);
    M5.Lcd.setTextColor(TFT_CYAN, TFT_BLACK);

    if (boostCounter > 0) {
        M5.Lcd.print("BOOST!");
    } else if (currentSpeed > BASE_SPEED) {
        M5.Lcd.print("SPEED UP");
    } else if (currentSpeed < BASE_SPEED) {
        M5.Lcd.print("BRAKING");
    } else {
        M5.Lcd.print("NORMAL");
    }

    // Controls hint
    M5.Lcd.setCursor(SCREEN_WIDTH - 120, SCREEN_HEIGHT - 12);
    M5.Lcd.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    M5.Lcd.print("L/R:Move A:Jump");
}

void game3Setup() {
    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.setTextColor(TFT_CYAN);
    M5.Lcd.setTextSize(3);
    M5.Lcd.setCursor(50, 60);
    M5.Lcd.println("SKYROADS");

    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.setCursor(30, 110);
    M5.Lcd.println("Race through space!");

    M5.Lcd.setCursor(20, 130);
    M5.Lcd.println("Left/Right: Change lanes");
    M5.Lcd.setCursor(20, 145);
    M5.Lcd.println("A/Up: Jump over danger");
    M5.Lcd.setCursor(20, 160);
    M5.Lcd.println("B: Speed boost");
    M5.Lcd.setCursor(20, 175);
    M5.Lcd.println("Down: Brake");

    M5.Lcd.setCursor(40, 200);
    M5.Lcd.setTextColor(TFT_YELLOW);
    M5.Lcd.println("Avoid red tiles and gaps!");

    delay(3500);

    randomSeed(analogRead(0));
    resetGame();
}

void game3Loop() {
    if (gameOver) {
        M5.Lcd.fillScreen(TFT_BLACK);
        M5.Lcd.setTextSize(3);
        M5.Lcd.setTextColor(TFT_RED);
        M5.Lcd.setCursor(50, 70);
        M5.Lcd.println("GAME OVER");

        M5.Lcd.setTextSize(2);
        M5.Lcd.setTextColor(TFT_YELLOW);
        M5.Lcd.setCursor(60, 110);
        M5.Lcd.print("Score: ");
        M5.Lcd.println(score);
        M5.Lcd.setCursor(45, 135);
        M5.Lcd.print("Distance: ");
        M5.Lcd.println(distance);

        M5.Lcd.setTextSize(1);
        M5.Lcd.setTextColor(TFT_WHITE);
        M5.Lcd.setCursor(50, 170);
        M5.Lcd.println("B: Play Again");
        M5.Lcd.setCursor(50, 185);
        M5.Lcd.println("Select: Menu");

        M5.update();
        readFacesButtons();
        bool facesB = !(facesData & 0x20);
        static bool lastB = false;

        if (facesB && !lastB) {
            resetGame();
        }
        lastB = facesB;

        delay(50);
        return;
    }

    updateShip();
    scrollTrack();

    drawTrack();
    drawShip();
    drawHUD();

    delay(30);
}
