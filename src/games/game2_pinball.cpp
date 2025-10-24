#include "game2_pinball.h"
#include <Wire.h>

// Faces GameBoy I2C address
#define FACES_ADDR 0x08

// Game constants
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240
#define BALL_RADIUS 4
#define GRAVITY 0.3
#define BOUNCE_DAMPING 0.85
#define FLIPPER_SPEED 15.0

// Custom colors
#define TFT_DARKGREEN 0x0320

// Ball structure
struct Ball {
    float x, y;
    float vx, vy;
    bool active;
    uint16_t color;
};

// Flipper structure
struct Flipper {
    float x, y;
    float angle;
    float targetAngle;
    bool isLeft;
    uint16_t color;
};

// Bumper structure
struct Bumper {
    int16_t x, y;
    int16_t radius;
    uint16_t color;
    int value;
};

// Game state
static Ball ball;
static Flipper leftFlipper, rightFlipper;
static Bumper bumpers[5];
static int score = 0;
static int lives = 3;
static bool gameOver = false;
static bool ballInPlay = false;
static uint8_t facesData = 0xFF;
static unsigned long lastBumperHit = 0;

static void readFacesButtons() {
    Wire.requestFrom(FACES_ADDR, 1);
    if (Wire.available()) {
        facesData = Wire.read();
    }
}

void resetBall() {
    ball.x = 300;
    ball.y = 180;
    ball.vx = 0;
    ball.vy = 0;
    ball.active = false;
    ball.color = TFT_WHITE;
    ballInPlay = false;
}

void launchBall() {
    if (!ball.active) {
        ball.vx = -8;
        ball.vy = -6;
        ball.active = true;
        ballInPlay = true;
    }
}

void setupPinball() {
    // Initialize ball
    resetBall();

    // Initialize flippers
    leftFlipper.x = 80;
    leftFlipper.y = 220;
    leftFlipper.angle = 45;
    leftFlipper.targetAngle = 45;
    leftFlipper.isLeft = true;
    leftFlipper.color = TFT_YELLOW;

    rightFlipper.x = 240;
    rightFlipper.y = 220;
    rightFlipper.angle = 135;
    rightFlipper.targetAngle = 135;
    rightFlipper.isLeft = false;
    rightFlipper.color = TFT_YELLOW;

    // Initialize bumpers
    bumpers[0] = {80, 60, 12, TFT_RED, 100};
    bumpers[1] = {160, 50, 12, TFT_ORANGE, 100};
    bumpers[2] = {240, 60, 12, TFT_RED, 100};
    bumpers[3] = {120, 100, 12, TFT_CYAN, 50};
    bumpers[4] = {200, 100, 12, TFT_CYAN, 50};

    score = 0;
    lives = 3;
    gameOver = false;
}

void drawBumper(Bumper &b) {
    M5.Lcd.fillCircle(b.x, b.y, b.radius, b.color);
    M5.Lcd.drawCircle(b.x, b.y, b.radius + 1, TFT_WHITE);
}

void drawFlipper(Flipper &f) {
    int length = 40;
    float rad = f.angle * PI / 180.0;
    int x2 = f.x + length * cos(rad);
    int y2 = f.y - length * sin(rad);

    // Draw thick flipper
    for (int i = -2; i <= 2; i++) {
        M5.Lcd.drawLine(f.x, f.y + i, x2, y2 + i, f.color);
    }
    M5.Lcd.fillCircle(f.x, f.y, 4, f.color);
}

void drawBall() {
    if (ball.active) {
        M5.Lcd.fillCircle((int)ball.x, (int)ball.y, BALL_RADIUS, ball.color);
    }
}

void updateFlippers() {
    bool facesLeft = !(facesData & 0x04);
    bool facesRight = !(facesData & 0x08);
    bool facesA = !(facesData & 0x10);

    // Left flipper
    if (facesLeft || facesA || M5.BtnA.isPressed()) {
        leftFlipper.targetAngle = -20;
    } else {
        leftFlipper.targetAngle = 45;
    }

    // Right flipper
    if (facesRight || M5.BtnC.isPressed()) {
        rightFlipper.targetAngle = 200;
    } else {
        rightFlipper.targetAngle = 135;
    }

    // Smooth flipper movement
    if (leftFlipper.angle < leftFlipper.targetAngle) {
        leftFlipper.angle += FLIPPER_SPEED;
        if (leftFlipper.angle > leftFlipper.targetAngle)
            leftFlipper.angle = leftFlipper.targetAngle;
    } else if (leftFlipper.angle > leftFlipper.targetAngle) {
        leftFlipper.angle -= FLIPPER_SPEED;
        if (leftFlipper.angle < leftFlipper.targetAngle)
            leftFlipper.angle = leftFlipper.targetAngle;
    }

    if (rightFlipper.angle < rightFlipper.targetAngle) {
        rightFlipper.angle += FLIPPER_SPEED;
        if (rightFlipper.angle > rightFlipper.targetAngle)
            rightFlipper.angle = rightFlipper.targetAngle;
    } else if (rightFlipper.angle > rightFlipper.targetAngle) {
        rightFlipper.angle -= FLIPPER_SPEED;
        if (rightFlipper.angle < rightFlipper.targetAngle)
            rightFlipper.angle = rightFlipper.targetAngle;
    }
}

void checkFlipperCollision(Flipper &f) {
    if (!ball.active) return;

    int length = 40;
    float rad = f.angle * PI / 180.0;
    int x2 = f.x + length * cos(rad);
    int y2 = f.y - length * sin(rad);

    // Simple distance check to flipper line
    float dist = abs((y2 - f.y) * ball.x - (x2 - f.x) * ball.y + x2 * f.y - y2 * f.x) /
                 sqrt((y2 - f.y) * (y2 - f.y) + (x2 - f.x) * (x2 - f.x));

    if (dist < BALL_RADIUS + 3) {
        // Check if ball is near the flipper segment
        float dotProduct = (ball.x - f.x) * (x2 - f.x) + (ball.y - f.y) * (y2 - f.y);
        float lengthSq = (x2 - f.x) * (x2 - f.x) + (y2 - f.y) * (y2 - f.y);

        if (dotProduct >= 0 && dotProduct <= lengthSq) {
            // Bounce off flipper
            float nx = -(y2 - f.y);
            float ny = (x2 - f.x);
            float len = sqrt(nx * nx + ny * ny);
            nx /= len;
            ny /= len;

            float dotVN = ball.vx * nx + ball.vy * ny;
            ball.vx = ball.vx - 2 * dotVN * nx;
            ball.vy = ball.vy - 2 * dotVN * ny;

            // Add flipper velocity
            if (f.angle != f.targetAngle) {
                ball.vx *= 1.3;
                ball.vy *= 1.3;
            }
        }
    }
}

void updateBall() {
    if (!ball.active) return;

    // Apply gravity
    ball.vy += GRAVITY;

    // Update position
    ball.x += ball.vx;
    ball.y += ball.vy;

    // Wall collisions
    if (ball.x - BALL_RADIUS < 5) {
        ball.x = 5 + BALL_RADIUS;
        ball.vx = -ball.vx * BOUNCE_DAMPING;
    }
    if (ball.x + BALL_RADIUS > 315) {
        ball.x = 315 - BALL_RADIUS;
        ball.vx = -ball.vx * BOUNCE_DAMPING;
    }
    if (ball.y - BALL_RADIUS < 25) {
        ball.y = 25 + BALL_RADIUS;
        ball.vy = -ball.vy * BOUNCE_DAMPING;
    }

    // Check bumper collisions
    for (int i = 0; i < 5; i++) {
        float dx = ball.x - bumpers[i].x;
        float dy = ball.y - bumpers[i].y;
        float dist = sqrt(dx * dx + dy * dy);

        if (dist < BALL_RADIUS + bumpers[i].radius) {
            // Bounce off bumper
            float nx = dx / dist;
            float ny = dy / dist;

            ball.x = bumpers[i].x + nx * (BALL_RADIUS + bumpers[i].radius);
            ball.y = bumpers[i].y + ny * (BALL_RADIUS + bumpers[i].radius);

            float dotVN = ball.vx * nx + ball.vy * ny;
            ball.vx = ball.vx - 2 * dotVN * nx;
            ball.vy = ball.vy - 2 * dotVN * ny;

            // Add boost
            ball.vx *= 1.2;
            ball.vy *= 1.2;

            // Add score
            if (millis() - lastBumperHit > 100) {
                score += bumpers[i].value;
                lastBumperHit = millis();
            }
        }
    }

    // Check flipper collisions
    checkFlipperCollision(leftFlipper);
    checkFlipperCollision(rightFlipper);

    // Check if ball is lost (drain)
    if (ball.y > 235) {
        lives--;
        if (lives > 0) {
            resetBall();
        } else {
            gameOver = true;
            ball.active = false;
        }
    }
}

void game2Setup() {
    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.setTextColor(TFT_YELLOW);
    M5.Lcd.setTextSize(3);
    M5.Lcd.setCursor(50, 80);
    M5.Lcd.println("PINBALL!");

    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.setCursor(40, 130);
    M5.Lcd.println("Left/A: Left Flipper");
    M5.Lcd.setCursor(40, 145);
    M5.Lcd.println("Right/C: Right Flipper");
    M5.Lcd.setCursor(40, 160);
    M5.Lcd.println("B: Launch Ball");

    delay(2500);

    setupPinball();
}

void game2Loop() {
    M5.update();
    readFacesButtons();

    bool facesB = !(facesData & 0x20);
    static bool lastB = false;

    // Launch ball with B button
    if (facesB && !lastB && !ballInPlay && lives > 0 && !gameOver) {
        launchBall();
    }
    lastB = facesB;

    // Restart game on game over
    if (gameOver) {
        M5.Lcd.fillScreen(TFT_BLACK);
        M5.Lcd.setTextSize(3);
        M5.Lcd.setTextColor(TFT_RED);
        M5.Lcd.setCursor(50, 80);
        M5.Lcd.println("GAME OVER");
        M5.Lcd.setTextSize(2);
        M5.Lcd.setTextColor(TFT_YELLOW);
        M5.Lcd.setCursor(60, 120);
        M5.Lcd.print("Score: ");
        M5.Lcd.println(score);
        M5.Lcd.setTextSize(1);
        M5.Lcd.setTextColor(TFT_WHITE);
        M5.Lcd.setCursor(70, 160);
        M5.Lcd.println("B: Play Again");

        if (facesB && !lastB) {
            setupPinball();
        }
        delay(50);
        return;
    }

    // Clear playfield
    M5.Lcd.fillRect(0, 20, 320, 220, TFT_DARKGREEN);

    // Draw boundaries
    M5.Lcd.drawRect(0, 20, 320, 220, TFT_WHITE);
    M5.Lcd.drawRect(1, 21, 318, 218, TFT_WHITE);

    // Draw score and lives
    M5.Lcd.fillRect(0, 0, 320, 20, TFT_BLACK);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setTextColor(TFT_YELLOW, TFT_BLACK);
    M5.Lcd.setCursor(5, 2);
    M5.Lcd.print("Score:");
    M5.Lcd.print(score);
    M5.Lcd.setCursor(220, 2);
    M5.Lcd.print("Lives:");
    M5.Lcd.print(lives);

    // Draw bumpers
    for (int i = 0; i < 5; i++) {
        drawBumper(bumpers[i]);
    }

    // Update and draw game objects
    updateFlippers();
    updateBall();

    drawFlipper(leftFlipper);
    drawFlipper(rightFlipper);
    drawBall();

    // Launch indicator
    if (!ballInPlay && lives > 0) {
        M5.Lcd.setTextSize(1);
        M5.Lcd.setTextColor(TFT_WHITE);
        M5.Lcd.setCursor(240, 190);
        M5.Lcd.println("Press B");
        M5.Lcd.setCursor(235, 200);
        M5.Lcd.println("to Launch!");
    }

    delay(20);
}
