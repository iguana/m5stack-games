#include "game2_pinball.h"
#include <Wire.h>

// Faces GameBoy I2C address
#define FACES_ADDR 0x08

// Game state
static uint8_t facesData = 0xFF;
static int score = 0;

static void readFacesButtons() {
    Wire.requestFrom(FACES_ADDR, 1);
    if (Wire.available()) {
        facesData = Wire.read();
    }
}

void game2Setup() {
    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(70, 100);
    M5.Lcd.println("PINBALL");
    M5.Lcd.setTextSize(1);
    M5.Lcd.setCursor(60, 130);
    M5.Lcd.println("Coming Soon!");
    delay(2000);

    score = 0;
}

void game2Loop() {
    M5.update();
    readFacesButtons();

    M5.Lcd.fillScreen(TFT_NAVY);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setTextColor(TFT_YELLOW);
    M5.Lcd.setCursor(70, 100);
    M5.Lcd.println("PINBALL");
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.setCursor(40, 130);
    M5.Lcd.println("Game Under Construction");

    M5.Lcd.setCursor(60, 150);
    M5.Lcd.print("Score: ");
    M5.Lcd.println(score);

    // Simple placeholder - increment score on button press
    bool facesA = !(facesData & 0x10);
    static bool lastA = false;
    if (facesA && !lastA) {
        score += 100;
    }
    lastA = facesA;

    delay(50);
}
