#include <EEPROM.h>
#include <LiquidCrystal.h>

enum ANALOG_PINS {
  A_KNOB = A0
};

enum DIGITAL_PINS {
  D_BUTTON = 13,
  D_LCD_RS = 12,
  D_LCD_EN = 11,
  D_RLY_ON = 7,
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

const long MIN_ACTUATIONS = 1000L;
const long MAX_ACTUATIONS = 100000L;
const long SET_ACTUATIONS_STEP = 1000L;

int controllerState = CTRL_BOOT;
int prevControllerState = CTRL_BOOT;

long actuationCount = 0;
long prevActuationCount = 0;
long setActuationCount = MIN_ACTUATIONS;

int buttonState = 0;
int prevButtonState = 0;
bool buttonUp = false;

int knobValue = 0;
int prevKnobValue = 0;

bool pauseContinue = true;

void setup() {
  lcd.begin(16, 2);

  pinMode(D_BUTTON, INPUT);
  pinMode(D_RLY_ON, OUTPUT);

  digitalWrite(D_RLY_ON, LOW);

  controllerState = CTRL_START;
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
      case CTRL_SET:
        handleControllerSetting();
        break;
      case CTRL_TRACK:
        handleControllerTracking();
        break;
      case CTRL_PAUSE:
        handleControllerPausing();
        break;
      default:
        handleControllerWaiting();
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
  
  actuationCount = 0;
  prevActuationCount = 0;
}

void handleControllerStarting() {
  if (buttonUp) {
    controllerState = CTRL_SET;
    return;
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
    return;
  }

  knobValue = analogRead(A_KNOB) / 10 + 1; // Compress read resolution to ~100 steps
  if (knobValue != prevKnobValue) {
    setActuationCount = min(knobValue * SET_ACTUATIONS_STEP, MAX_ACTUATIONS);
    lcd.setCursor(0, 1);
    lcd.print("        ");
    lcd.setCursor(0, 1);
    lcd.print(setActuationCount);
    prevKnobValue = knobValue;
  }
}

void handleControllerTrack() {
  digitalWrite(D_RLY_ON, HIGH);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Progress:");
  lcd.setCursor(0, 1);
  lcd.print(actuationCount);
  lcd.setCursor(8, 1);
  lcd.print("/");
  lcd.setCursor(10, 1);
  lcd.print(setActuationCount);
}

unsigned long timestamp = 0;
unsigned long prevTimestamp = 0;
void handleControllerTracking() {
  timestamp = millis();
  if (timestamp - prevTimestamp > 1000UL) {
    actuationCount++;
    prevTimestamp = timestamp;
  }

  if (actuationCount != prevActuationCount) {
    lcd.setCursor(0, 1);
    lcd.print(actuationCount);
    prevActuationCount = actuationCount;

    // Save to eeprom every 5% progress made
    if (actuationCount * 100L / setActuationCount % 5L) {
      EEPROM.put(ADDR_LAST_COUNT, actuationCount);
    }
  }

  if (actuationCount >= setActuationCount) {
    controllerState = CTRL_DONE;
    return;
  }

  if (buttonUp) {
    controllerState = CTRL_PAUSE;
    return;
  }
}

void handleControllerPause() {
  digitalWrite(D_RLY_ON, LOW);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Paused, continue?");
  lcd.setCursor(0, 1);
  lcd.print(pauseContinue ? "Yes" : " No");
}

void handleControllerPausing() {
  if (buttonUp) {
    if (pauseContinue) {
      controllerState = CTRL_TRACK;
      return;
    }
    
    controllerState = CTRL_DONE;
    return;
  }

  knobValue = analogRead(A_KNOB) / 512; // Compress read resolution to on/off
  if (knobValue != prevKnobValue) {
    pauseContinue = knobValue;
    prevKnobValue = knobValue;

    lcd.setCursor(0, 1);
    lcd.print(pauseContinue ? "Yes" : " No");
  }
}

void handleControllerDone() {
  digitalWrite(D_RLY_ON, LOW);
  EEPROM.put(ADDR_LAST_COUNT, prevActuationCount);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Finished!");
  lcd.setCursor(0, 1);
  lcd.print(actuationCount);
  lcd.setCursor(8, 1);
  lcd.print("/");
  lcd.setCursor(10, 1);
  lcd.print(setActuationCount);
}

void handleControllerWaiting() {
  if (buttonUp) {
    controllerState = CTRL_START;
  }
}