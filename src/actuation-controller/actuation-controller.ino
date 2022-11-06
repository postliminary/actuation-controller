#include <EEPROM.h>
#include <LiquidCrystal.h>

enum ANALOG_PINS {
  A_KNOB = A0
};

enum DIGITAL_PINS {
  D_BUTTON = 13,
  D_LCD_RS = 12,
  D_LCD_EN = 11,
  D_LED_ON = 7,
  D_LCD_D4 = 5,
  D_LCD_D5 = 4,
  D_LCD_D6 = 3,
  D_LCD_D7 = 2
};

enum CONTROLLER_STATE {
  CTRL_BOOT,
  CTRL_START,
  CTRL_SET,
  CTRL_TRACK,
  CTRL_PAUSE,
  CTRL_DONE
};

enum EEPROM_ADDRESSES {
  ADDR_LAST_COUNT
};

// initialize the library with the numbers of the interface pins
LiquidCrystal lcd(D_LCD_RS, D_LCD_EN, D_LCD_D4, D_LCD_D5, D_LCD_D6, D_LCD_D7);

long actuationCount = 0;
long prevActuationCount = 0;
long setActuationCount = 10; // Default

int buttonState = 0;
int prevButtonState = 0;
bool buttonUp = false;

int controllerState = CTRL_BOOT;
int prevControllerState = CTRL_BOOT;

// a variable to choose which reply from the crystal ball
int reply;

void setup() {
  lcd.begin(16, 2);

  pinMode(D_BUTTON, INPUT);

  controllerState = CTRL_START;
}

void printLcd(char* line0, char* line1) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(line0);
      lcd.setCursor(0, 1);
      lcd.print(line1);
}

void loop() {
  // check the status of the switch
  buttonState = digitalRead(D_BUTTON);
  buttonUp = buttonState != prevButtonState && buttonState == LOW;
  prevButtonState = buttonState;

  if (controllerState != prevControllerState) {
    // Moving to new state
    switch (controllerState) {
      case CTRL_START:
        handleControllerStart();
        break;
      case CTRL_SET:
        handleControllerSet();
        break;
      case CTRL_TRACK:
        handleControllerTrack();
        break;
      case CTRL_PAUSE:
        handleControllerPause();
        break;
      default:
        handleControllerDone();
        break;
    }
    prevControllerState = controllerState;
  } else {
    switch (controllerState) {
      case CTRL_START:
        handleControllerStarting();
        break;
    }
  }

}

void handleControllerStart() {
  EEPROM.get(ADDR_LAST_COUNT, prevActuationCount);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Last run:");
  lcd.setCursor(0, 1);
  lcd.print(prevActuationCount);
  
  prevActuationCount = 0;
}

void handleControllerStarting() {
  if (buttonUp) {
    controllerState = CTRL_SET;
  }
}

void handleControllerSet() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Set actuations:");
  lcd.setCursor(0, 1);
  lcd.print(setActuationCount);
}

void handleControllerSetting() {
  if (buttonUp) {
    controllerState = CTRL_TRACK;
  }

  // TODO Pot selector for iterations
}

void handleControllerTrack() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Set actuations:");
  lcd.setCursor(0, 1);
  lcd.print(actuationCount);
  lcd.setCursor(8, 1);
  lcd.print("/");
  lcd.setCursor(10, 1);
  lcd.print(setActuationCount);
}

void handleControllerTracking() {
  if (actuationCount != prevActuationCount) {
    lcd.setCursor(0, 1);
    lcd.print(actuationCount);

    // TODO Decide when to save to eeprom
  }
}

void handleControllerPause() {

}

void handleControllerPausing() {

}

void handleControllerDone() {

}