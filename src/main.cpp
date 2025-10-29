#include <M5Stack.h>
#include <Wire.h>
#include "games/game1_platform.h"
#include "games/game2_pinball.h"
#include "games/game3_skyroads.h"
#include "games/game4_tetris.h"

enum GameState {
    SPLASH,
    MENU,
    GAME1,
    GAME2,
    GAME3,
    GAME4
};

GameState currentState = SPLASH;
int selectedGame = 0;
unsigned long splashStartTime = 0;

#define FACES_ADDR 0x08
uint8_t facesData = 0xFF;

void readFacesButtons() {
    Wire.requestFrom(FACES_ADDR, 1);
    if (Wire.available()) {
        facesData = Wire.read();
    }
}

void showSplashScreen() {
    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.setTextColor(TFT_CYAN);
    M5.Lcd.setTextSize(3);
    M5.Lcd.setCursor(50, 60);
    M5.Lcd.println("M5 GAMES");

    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(TFT_WHITE);

    // Battery info
    int batteryLevel = M5.Power.getBatteryLevel();
    bool isCharging = M5.Power.isCharging();

    M5.Lcd.setCursor(40, 130);
    M5.Lcd.print("Battery: ");
    M5.Lcd.print(batteryLevel);
    M5.Lcd.println("%");

    M5.Lcd.setCursor(40, 150);
    M5.Lcd.print("Status: ");
    if (isCharging) {
        M5.Lcd.setTextColor(TFT_GREEN);
        M5.Lcd.println("Charging");
    } else {
        M5.Lcd.setTextColor(TFT_YELLOW);
        M5.Lcd.println("On Battery");
    }

    M5.Lcd.setTextColor(TFT_LIGHTGREY);
    M5.Lcd.setCursor(60, 200);
    M5.Lcd.println("Press any button...");
}

void showMenu() {
    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(60, 30);
    M5.Lcd.println("SELECT GAME");

    M5.Lcd.setTextSize(2);

    // Game 1
    if (selectedGame == 0) {
        M5.Lcd.setTextColor(TFT_BLACK, TFT_GREEN);
    } else {
        M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    }
    M5.Lcd.setCursor(40, 80);
    M5.Lcd.println("> Game 1: Platform");

    // Game 2
    if (selectedGame == 1) {
        M5.Lcd.setTextColor(TFT_BLACK, TFT_GREEN);
    } else {
        M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    }
    M5.Lcd.setCursor(40, 110);
    M5.Lcd.println("> Game 2: Pinball");

    // Game 3
    if (selectedGame == 2) {
        M5.Lcd.setTextColor(TFT_BLACK, TFT_GREEN);
    } else {
        M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    }
    M5.Lcd.setCursor(40, 140);
    M5.Lcd.println("> Game 3: Skyroads");

    // Game 4
    if (selectedGame == 3) {
        M5.Lcd.setTextColor(TFT_BLACK, TFT_GREEN);
    } else {
        M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    }
    M5.Lcd.setCursor(40, 170);
    M5.Lcd.println("> Game 4: Tetris");

    M5.Lcd.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setCursor(20, 210);
    M5.Lcd.println("UP/DOWN: Select  A: Play");
}

void setup() {
    M5.begin();
    M5.Power.begin();
    Wire.begin();

    // Disable speaker
    M5.Speaker.mute();
    M5.Speaker.end();

    showSplashScreen();
    splashStartTime = millis();
    currentState = SPLASH;
}

void loop() {
    M5.update();
    readFacesButtons();

    bool facesUp = !(facesData & 0x01);
    bool facesDown = !(facesData & 0x02);
    bool facesA = !(facesData & 0x10);
    bool facesSelect = !(facesData & 0x40);
    bool facesStart = !(facesData & 0x80);  // Bit 7: Start button

    // Power off: Hold Start button for 2 seconds
    static unsigned long startButtonPressStart = 0;
    static bool startButtonWasPressed = false;

    if (facesStart) {
        if (!startButtonWasPressed) {
            startButtonPressStart = millis();
            startButtonWasPressed = true;
        }
        if (millis() - startButtonPressStart > 2000) {
            // Show shutdown message
            M5.Lcd.fillScreen(TFT_BLACK);
            M5.Lcd.setTextColor(TFT_RED);
            M5.Lcd.setTextSize(3);
            M5.Lcd.setCursor(40, 100);
            M5.Lcd.println("POWERING OFF");
            delay(1000);
            // Turn off the device
            M5.Power.deepSleep();
        }
    } else {
        startButtonWasPressed = false;
    }

    static bool lastUp = false;
    static bool lastDown = false;
    static bool lastA = false;
    static bool lastSelect = false;

    switch (currentState) {
        case SPLASH:
            // Auto-advance after 3 seconds or on button press
            if (millis() - splashStartTime > 3000 ||
                M5.BtnA.wasPressed() || M5.BtnB.wasPressed() || M5.BtnC.wasPressed() ||
                facesA) {
                currentState = MENU;
                showMenu();
            }
            break;

        case MENU:
            // Navigate menu
            if (facesUp && !lastUp) {
                selectedGame = (selectedGame - 1 + 4) % 4;
                showMenu();
            }
            if (facesDown && !lastDown) {
                selectedGame = (selectedGame + 1) % 4;
                showMenu();
            }

            // Select game
            if ((facesA && !lastA) || M5.BtnB.wasPressed()) {
                if (selectedGame == 0) {
                    currentState = GAME1;
                    game1Setup();
                } else if (selectedGame == 1) {
                    currentState = GAME2;
                    game2Setup();
                } else if (selectedGame == 2) {
                    currentState = GAME3;
                    game3Setup();
                } else {
                    currentState = GAME4;
                    game4Setup();
                }
            }
            break;

        case GAME1:
            game1Loop();
            // Return to menu with Select button or M5 button long press
            if ((facesSelect && !lastSelect) || M5.BtnA.pressedFor(2000)) {
                currentState = MENU;
                showMenu();
            }
            break;

        case GAME2:
            game2Loop();
            // Return to menu with Select button or M5 button long press
            if ((facesSelect && !lastSelect) || M5.BtnA.pressedFor(2000)) {
                currentState = MENU;
                showMenu();
            }
            break;

        case GAME3:
            game3Loop();
            // Return to menu with Select button or M5 button long press
            if ((facesSelect && !lastSelect) || M5.BtnA.pressedFor(2000)) {
                currentState = MENU;
                showMenu();
            }
            break;

        case GAME4:
            game4Loop();
            // Return to menu with Select button or M5 button long press
            if ((facesSelect && !lastSelect) || M5.BtnA.pressedFor(2000)) {
                currentState = MENU;
                showMenu();
            }
            break;
    }

    lastUp = facesUp;
    lastDown = facesDown;
    lastA = facesA;
    lastSelect = facesSelect;

    delay(10);
}
