#include "game4_tetris.h"
#include <Wire.h>

// Game constants
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240
#define BOARD_WIDTH 10
#define BOARD_HEIGHT 20
#define BLOCK_SIZE 10
#define BOARD_X 110
#define BOARD_Y 20

// Faces GameBoy I2C address
#define FACES_ADDR 0x08

// Tetromino shapes (7 pieces, 4 rotations each)
// Each shape is 4x4 grid
const bool SHAPES[7][4][4][4] = {
    // I piece
    {
        {{0,0,0,0}, {1,1,1,1}, {0,0,0,0}, {0,0,0,0}},
        {{0,0,1,0}, {0,0,1,0}, {0,0,1,0}, {0,0,1,0}},
        {{0,0,0,0}, {0,0,0,0}, {1,1,1,1}, {0,0,0,0}},
        {{0,1,0,0}, {0,1,0,0}, {0,1,0,0}, {0,1,0,0}}
    },
    // O piece
    {
        {{0,1,1,0}, {0,1,1,0}, {0,0,0,0}, {0,0,0,0}},
        {{0,1,1,0}, {0,1,1,0}, {0,0,0,0}, {0,0,0,0}},
        {{0,1,1,0}, {0,1,1,0}, {0,0,0,0}, {0,0,0,0}},
        {{0,1,1,0}, {0,1,1,0}, {0,0,0,0}, {0,0,0,0}}
    },
    // T piece
    {
        {{0,1,0,0}, {1,1,1,0}, {0,0,0,0}, {0,0,0,0}},
        {{0,1,0,0}, {0,1,1,0}, {0,1,0,0}, {0,0,0,0}},
        {{0,0,0,0}, {1,1,1,0}, {0,1,0,0}, {0,0,0,0}},
        {{0,1,0,0}, {1,1,0,0}, {0,1,0,0}, {0,0,0,0}}
    },
    // S piece
    {
        {{0,1,1,0}, {1,1,0,0}, {0,0,0,0}, {0,0,0,0}},
        {{0,1,0,0}, {0,1,1,0}, {0,0,1,0}, {0,0,0,0}},
        {{0,0,0,0}, {0,1,1,0}, {1,1,0,0}, {0,0,0,0}},
        {{1,0,0,0}, {1,1,0,0}, {0,1,0,0}, {0,0,0,0}}
    },
    // Z piece
    {
        {{1,1,0,0}, {0,1,1,0}, {0,0,0,0}, {0,0,0,0}},
        {{0,0,1,0}, {0,1,1,0}, {0,1,0,0}, {0,0,0,0}},
        {{0,0,0,0}, {1,1,0,0}, {0,1,1,0}, {0,0,0,0}},
        {{0,1,0,0}, {1,1,0,0}, {1,0,0,0}, {0,0,0,0}}
    },
    // J piece
    {
        {{1,0,0,0}, {1,1,1,0}, {0,0,0,0}, {0,0,0,0}},
        {{0,1,1,0}, {0,1,0,0}, {0,1,0,0}, {0,0,0,0}},
        {{0,0,0,0}, {1,1,1,0}, {0,0,1,0}, {0,0,0,0}},
        {{0,1,0,0}, {0,1,0,0}, {1,1,0,0}, {0,0,0,0}}
    },
    // L piece
    {
        {{0,0,1,0}, {1,1,1,0}, {0,0,0,0}, {0,0,0,0}},
        {{0,1,0,0}, {0,1,0,0}, {0,1,1,0}, {0,0,0,0}},
        {{0,0,0,0}, {1,1,1,0}, {1,0,0,0}, {0,0,0,0}},
        {{1,1,0,0}, {0,1,0,0}, {0,1,0,0}, {0,0,0,0}}
    }
};

// Piece colors
const uint16_t PIECE_COLORS[7] = {
    TFT_CYAN,     // I
    TFT_YELLOW,   // O
    TFT_PURPLE,   // T
    TFT_GREEN,    // S
    TFT_RED,      // Z
    TFT_BLUE,     // J
    TFT_ORANGE    // L
};

// Game state
static uint8_t board[BOARD_HEIGHT][BOARD_WIDTH];
static int currentPiece;
static int currentRotation;
static int currentX;
static int currentY;
static int nextPiece;
static unsigned long lastMoveTime;
static unsigned long moveDelay;
static int score;
static int linesCleared;
static int level;
static bool gameOver;
static uint8_t facesData = 0xFF;
static bool needsFullRedraw = true;

static void readFacesButtons() {
    Wire.requestFrom(FACES_ADDR, 1);
    if (Wire.available()) {
        facesData = Wire.read();
    }
}

void drawBlock(int x, int y, uint16_t color) {
    M5.Lcd.fillRect(BOARD_X + x * BLOCK_SIZE, BOARD_Y + y * BLOCK_SIZE,
                    BLOCK_SIZE - 1, BLOCK_SIZE - 1, color);
}

void drawBoard() {
    // Draw border
    M5.Lcd.drawRect(BOARD_X - 2, BOARD_Y - 2,
                    BOARD_WIDTH * BLOCK_SIZE + 4,
                    BOARD_HEIGHT * BLOCK_SIZE + 4, TFT_WHITE);

    // Draw placed blocks
    for (int y = 0; y < BOARD_HEIGHT; y++) {
        for (int x = 0; x < BOARD_WIDTH; x++) {
            if (board[y][x] > 0) {
                drawBlock(x, y, PIECE_COLORS[board[y][x] - 1]);
            } else {
                drawBlock(x, y, TFT_BLACK);
            }
        }
    }
}

void drawCurrentPiece(uint16_t color) {
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            if (SHAPES[currentPiece][currentRotation][y][x]) {
                int boardX = currentX + x;
                int boardY = currentY + y;
                if (boardY >= 0 && boardY < BOARD_HEIGHT &&
                    boardX >= 0 && boardX < BOARD_WIDTH) {
                    drawBlock(boardX, boardY, color);
                }
            }
        }
    }
}

void drawNextPiece() {
    // Clear next piece area
    M5.Lcd.fillRect(10, 60, 50, 50, TFT_BLACK);
    M5.Lcd.drawRect(9, 59, 52, 52, TFT_WHITE);

    // Draw next piece
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            if (SHAPES[nextPiece][0][y][x]) {
                M5.Lcd.fillRect(15 + x * 10, 65 + y * 10, 9, 9,
                               PIECE_COLORS[nextPiece]);
            }
        }
    }
}

void drawUI() {
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);

    M5.Lcd.setCursor(10, 20);
    M5.Lcd.print("NEXT:");

    M5.Lcd.setCursor(10, 120);
    M5.Lcd.print("SCORE:");
    M5.Lcd.setCursor(10, 135);
    M5.Lcd.fillRect(10, 135, 80, 10, TFT_BLACK);
    M5.Lcd.print(score);

    M5.Lcd.setCursor(10, 155);
    M5.Lcd.print("LINES:");
    M5.Lcd.setCursor(10, 170);
    M5.Lcd.fillRect(10, 170, 80, 10, TFT_BLACK);
    M5.Lcd.print(linesCleared);

    M5.Lcd.setCursor(10, 190);
    M5.Lcd.print("LEVEL:");
    M5.Lcd.setCursor(10, 205);
    M5.Lcd.fillRect(10, 205, 80, 10, TFT_BLACK);
    M5.Lcd.print(level);

    M5.Lcd.setCursor(230, 20);
    M5.Lcd.print("CONTROLS:");
    M5.Lcd.setTextSize(1);
    M5.Lcd.setCursor(230, 40);
    M5.Lcd.print("L/R:Move");
    M5.Lcd.setCursor(230, 55);
    M5.Lcd.print("DOWN:Drop");
    M5.Lcd.setCursor(230, 70);
    M5.Lcd.print("A:Rotate");
    M5.Lcd.setCursor(230, 85);
    M5.Lcd.print("B:Fast");
}

bool checkCollision(int piece, int rotation, int x, int y) {
    for (int py = 0; py < 4; py++) {
        for (int px = 0; px < 4; px++) {
            if (SHAPES[piece][rotation][py][px]) {
                int boardX = x + px;
                int boardY = y + py;

                // Check boundaries
                if (boardX < 0 || boardX >= BOARD_WIDTH ||
                    boardY >= BOARD_HEIGHT) {
                    return true;
                }

                // Check collision with placed blocks (only if within board)
                if (boardY >= 0 && board[boardY][boardX] > 0) {
                    return true;
                }
            }
        }
    }
    return false;
}

void placePiece() {
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            if (SHAPES[currentPiece][currentRotation][y][x]) {
                int boardY = currentY + y;
                int boardX = currentX + x;
                if (boardY >= 0 && boardY < BOARD_HEIGHT &&
                    boardX >= 0 && boardX < BOARD_WIDTH) {
                    board[boardY][boardX] = currentPiece + 1;
                }
            }
        }
    }
}

int clearLines() {
    int cleared = 0;

    for (int y = BOARD_HEIGHT - 1; y >= 0; y--) {
        bool lineFull = true;
        for (int x = 0; x < BOARD_WIDTH; x++) {
            if (board[y][x] == 0) {
                lineFull = false;
                break;
            }
        }

        if (lineFull) {
            cleared++;
            // Flash line before clearing
            for (int x = 0; x < BOARD_WIDTH; x++) {
                drawBlock(x, y, TFT_WHITE);
            }
            delay(100);

            // Move all lines above down
            for (int yy = y; yy > 0; yy--) {
                for (int x = 0; x < BOARD_WIDTH; x++) {
                    board[yy][x] = board[yy - 1][x];
                }
            }
            // Clear top line
            for (int x = 0; x < BOARD_WIDTH; x++) {
                board[0][x] = 0;
            }
            y++; // Recheck this line
        }
    }

    return cleared;
}

void spawnNewPiece() {
    currentPiece = nextPiece;
    nextPiece = random(7);
    currentRotation = 0;
    currentX = BOARD_WIDTH / 2 - 2;
    currentY = 0;

    if (checkCollision(currentPiece, currentRotation, currentX, currentY)) {
        gameOver = true;
    }
}

static void resetGame() {
    // Clear board
    for (int y = 0; y < BOARD_HEIGHT; y++) {
        for (int x = 0; x < BOARD_WIDTH; x++) {
            board[y][x] = 0;
        }
    }

    score = 0;
    linesCleared = 0;
    level = 1;
    moveDelay = 500;
    gameOver = false;
    needsFullRedraw = true;

    currentPiece = random(7);
    nextPiece = random(7);
    currentRotation = 0;
    currentX = BOARD_WIDTH / 2 - 2;
    currentY = 0;
    lastMoveTime = millis();
}

void game4Setup() {
    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.setTextSize(3);
    M5.Lcd.setCursor(90, 100);
    M5.Lcd.println("TETRIS");
    M5.Lcd.setTextSize(1);
    M5.Lcd.setCursor(60, 140);
    M5.Lcd.println("Get ready...");
    delay(2000);

    randomSeed(analogRead(0));
    resetGame();
}

void game4Loop() {
    if (gameOver) {
        M5.Lcd.fillRect(BOARD_X + 10, BOARD_Y + 80, 80, 60, TFT_RED);
        M5.Lcd.setTextColor(TFT_WHITE);
        M5.Lcd.setTextSize(2);
        M5.Lcd.setCursor(BOARD_X + 15, BOARD_Y + 90);
        M5.Lcd.println("GAME");
        M5.Lcd.setCursor(BOARD_X + 15, BOARD_Y + 110);
        M5.Lcd.println("OVER");
        M5.Lcd.setTextSize(1);
        M5.Lcd.setCursor(BOARD_X + 12, BOARD_Y + 135);
        M5.Lcd.println("A:Retry");

        M5.update();
        readFacesButtons();
        bool facesA = !(facesData & 0x10);

        static bool lastA = false;
        if (facesA && !lastA) {
            resetGame();
        }
        lastA = facesA;
        delay(50);
        return;
    }

    if (needsFullRedraw) {
        M5.Lcd.fillScreen(TFT_BLACK);
        drawUI();
        drawNextPiece();
        drawBoard();
        needsFullRedraw = false;
    }

    M5.update();
    readFacesButtons();

    bool facesLeft = !(facesData & 0x04);
    bool facesRight = !(facesData & 0x08);
    bool facesDown = !(facesData & 0x02);
    bool facesA = !(facesData & 0x10);
    bool facesB = !(facesData & 0x20);

    static bool lastLeft = false;
    static bool lastRight = false;
    static bool lastDown = false;
    static bool lastA = false;
    static bool lastB = false;

    // Erase current piece (draw over it with board state)
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            if (SHAPES[currentPiece][currentRotation][y][x]) {
                int boardX = currentX + x;
                int boardY = currentY + y;
                if (boardY >= 0 && boardY < BOARD_HEIGHT &&
                    boardX >= 0 && boardX < BOARD_WIDTH) {
                    // Redraw the board cell at this position
                    if (board[boardY][boardX] > 0) {
                        drawBlock(boardX, boardY, PIECE_COLORS[board[boardY][boardX] - 1]);
                    } else {
                        drawBlock(boardX, boardY, TFT_BLACK);
                    }
                }
            }
        }
    }

    // Move left
    if (facesLeft && !lastLeft) {
        if (!checkCollision(currentPiece, currentRotation, currentX - 1, currentY)) {
            currentX--;
        }
    }

    // Move right
    if (facesRight && !lastRight) {
        if (!checkCollision(currentPiece, currentRotation, currentX + 1, currentY)) {
            currentX++;
        }
    }

    // Rotate
    if (facesA && !lastA) {
        int newRotation = (currentRotation + 1) % 4;
        if (!checkCollision(currentPiece, newRotation, currentX, currentY)) {
            currentRotation = newRotation;
        }
    }

    // Fast drop with B button
    unsigned long currentDelay = (facesB) ? 50 : moveDelay;

    // Manual drop
    if (facesDown && !lastDown) {
        if (!checkCollision(currentPiece, currentRotation, currentX, currentY + 1)) {
            currentY++;
            score += 1;
        }
    }

    // Auto drop
    if (millis() - lastMoveTime > currentDelay) {
        if (!checkCollision(currentPiece, currentRotation, currentX, currentY + 1)) {
            currentY++;
        } else {
            // Place piece
            placePiece();

            // Draw the placed piece immediately
            drawBoard();

            // Clear lines
            int cleared = clearLines();
            if (cleared > 0) {
                linesCleared += cleared;
                // Scoring: 100, 300, 500, 800 for 1, 2, 3, 4 lines
                int points[] = {0, 100, 300, 500, 800};
                score += points[cleared] * level;

                // Level up every 10 lines
                level = linesCleared / 10 + 1;
                moveDelay = max(100, 500 - (level - 1) * 40);

                drawBoard();
                drawUI();
            }

            // Spawn new piece
            spawnNewPiece();
            drawNextPiece();
        }
        lastMoveTime = millis();
    }

    // Draw current piece
    drawCurrentPiece(PIECE_COLORS[currentPiece]);

    lastLeft = facesLeft;
    lastRight = facesRight;
    lastDown = facesDown;
    lastA = facesA;
    lastB = facesB;

    delay(20);
}
