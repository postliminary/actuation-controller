#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <EEPROM.h>

#define SCREEN_WIDTH          128 // OLED display width, in pixels
#define SCREEN_HEIGHT         32 // OLED display height, in pixels
#define OLED_RESET            -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS        0x3C // See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
#define MIN_ACTUATIONS        5000L
#define MAX_ACTUATIONS        100000L
#define SET_ACTUATIONS_STEP   5000L
#define IR_SENSOR_CAL_SAMPLE  INT16_MAX // Number of sample to take to calibrate sensor
#define IR_SENSOR_CAL_DELAY   50 // Millis delay between sampling

enum ANALOG_PINS {
  A_IR_SENSOR = A0
};

enum DIGITAL_PINS {
  D_BUTTON_ACCEPT = 2,
  D_BUTTON_CHANGE = 3,
  D_PMOS_GATE = 7
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

Adafruit_SSD1306 oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

int controllerState = CTRL_BOOT;
int controllerPrevState = CTRL_BOOT;

long actuationCount = 0;
long actuationPrevCount = 0;
long actuationSetCount = MIN_ACTUATIONS;

int acceptButtonState = 0;
int acceptButtonPrevState = 0;
bool acceptButtonUp = false;

int changeButtonState = 0;
int changeButtonPrevState = 0;
bool changeButtonUp = false;

int irSensorValue = 0;
int prevIrSensorValue = 0;
int irSensorCalibrationCount = 0;
unsigned long irCalibrationTimestamp = 0;
unsigned long irCalibrationPrevTimestamp = 0;
int irSensorCalibratedThreshold = 0;

char trackProgressString[16];
char trackSensorString[16];
char TRACK_PROGRESS_FORMAT[] = "%-6ld/%6ld";
char TRACK_SENSOR_FORMAT[] = "tracking %4d";

bool pauseContinue = true;

void setup() {
  oled.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS);
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);

  pinMode(D_BUTTON_ACCEPT, INPUT);
  pinMode(D_BUTTON_CHANGE, INPUT);
  pinMode(D_PMOS_GATE, OUTPUT);

  digitalWrite(D_PMOS_GATE, HIGH);

  controllerState = CTRL_START;
}

void loop() {
  // Check the status of the buttons
  acceptButtonState = digitalRead(D_BUTTON_ACCEPT);
  acceptButtonUp = acceptButtonState != acceptButtonPrevState && acceptButtonState == LOW;
  acceptButtonPrevState = acceptButtonState;

  changeButtonState = digitalRead(D_BUTTON_CHANGE);
  changeButtonUp = changeButtonState != changeButtonPrevState && changeButtonState == LOW;
  changeButtonPrevState = changeButtonState;

  if (controllerState != controllerPrevState) {
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
    controllerPrevState = controllerState;
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

void handleActuation() {
  if (controllerState == CTRL_TRACK) {
    actuationCount++;
  }
}

void handleControllerStart() {
  EEPROM.get(ADDR_LAST_COUNT, actuationPrevCount);

  oled.clearDisplay();
  oled.setCursor(0, 0);
  oled.println("Last run:");
  oled.println(actuationPrevCount);
  oled.display();
  
  actuationCount = 0;
  actuationPrevCount = 0;
  irSensorCalibrationCount = 0;
  irSensorCalibratedThreshold = 0;
}

void handleControllerStarting() {
  if (acceptButtonUp) {
    controllerState = CTRL_SET;
    return;
  }
}

void handleControllerSet() {
  oled.clearDisplay();
  oled.setCursor(0, 0);
  oled.println("Set actuations:");
  oled.println(actuationSetCount);
  oled.display();
}

void handleControllerSetting() {
  if (acceptButtonUp) {
    controllerState = CTRL_TRACK;
    return;
  }


  if (changeButtonUp) {
    actuationSetCount += SET_ACTUATIONS_STEP;

    if (actuationSetCount > MAX_ACTUATIONS) {
      actuationSetCount = MIN_ACTUATIONS;
    }

    oled.clearDisplay();
    oled.setCursor(0, 0);
    oled.println("Set actuations:");
    oled.println(actuationSetCount);
    oled.display();
  }
}

void handleControllerTrack() {
  digitalWrite(D_PMOS_GATE, LOW);

  sprintf(trackProgressString, TRACK_PROGRESS_FORMAT, actuationCount, actuationSetCount);
  oled.clearDisplay();
  oled.setCursor(0, 0);
  oled.println("Running:");
  oled.println(trackProgressString);
  oled.display();
}

void handleControllerTracking() {
  irSensorValue = analogRead(A_IR_SENSOR) / 4;

  // "Auto-calibrate" sensor
  if (irSensorCalibrationCount < INT16_MAX) {
    irCalibrationTimestamp = millis();
    if (irCalibrationTimestamp - irCalibrationPrevTimestamp > IR_SENSOR_CAL_DELAY) {
      // Add value to average
      irSensorCalibrationCount++;
      irSensorCalibratedThreshold = irSensorCalibratedThreshold + ((irSensorValue - irSensorCalibratedThreshold) / irSensorCalibrationCount);
      irCalibrationPrevTimestamp = irCalibrationTimestamp;
    }
  }

  // IR Sensor values are HIGH by default and change to LOW when something gets close enough
  // So read a RISING signal past the threshold to detect object leaving sensor area
  if (prevIrSensorValue < irSensorCalibratedThreshold && irSensorValue > irSensorCalibratedThreshold) { 
    actuationCount++;
  }
  prevIrSensorValue = irSensorValue;

  if (actuationCount != actuationPrevCount) {
    actuationPrevCount = actuationCount;

    // Save to eeprom every 5% progress made
    if (actuationCount * 100L / actuationSetCount % 5L) {
      EEPROM.put(ADDR_LAST_COUNT, actuationCount);
    }

    sprintf(trackProgressString, TRACK_PROGRESS_FORMAT, actuationCount, actuationSetCount);
    sprintf(trackSensorString, TRACK_SENSOR_FORMAT, irSensorCalibratedThreshold);
    oled.clearDisplay();
    oled.setCursor(0, 0);
    oled.println("Running:");
    oled.println(trackProgressString);
    oled.println(trackSensorString);
    oled.display();
  }

  if (actuationCount >= actuationSetCount) {
    controllerState = CTRL_DONE;
    return;
  }

  if (acceptButtonUp) {
    controllerState = CTRL_PAUSE;
    return;
  }
}

void handleControllerPause() {
  digitalWrite(D_PMOS_GATE, HIGH);

  oled.clearDisplay();
  oled.setCursor(0, 0);
  oled.println("Continue?");
  oled.println(pauseContinue ? "Yes": "No");
  oled.display();
}

void handleControllerPausing() {
  if (acceptButtonUp) {
    if (pauseContinue) {
      controllerState = CTRL_TRACK;
      return;
    }
    
    controllerState = CTRL_DONE;
    return;
  }

  if (changeButtonUp) {
    pauseContinue = !pauseContinue;

    oled.clearDisplay();
    oled.setCursor(0, 0);
    oled.println("Continue?");
    oled.println(pauseContinue ? "Yes": "No");
    oled.display();
  }
}

void handleControllerDone() {
  digitalWrite(D_PMOS_GATE, HIGH);
  EEPROM.put(ADDR_LAST_COUNT, actuationPrevCount);

  sprintf(trackProgressString, TRACK_PROGRESS_FORMAT, actuationCount, actuationSetCount);
  oled.clearDisplay();
  oled.setCursor(0, 0);
  oled.println("Finished!");
  oled.println(trackProgressString);
  oled.display();
}

void handleControllerWaiting() {
  if (acceptButtonUp) {
    controllerState = CTRL_START;
  }
}