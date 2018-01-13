// This #include statement was automatically added by the Spark IDE.
#include "particle-strip.h"

#define SIGN (Color{0x00, 0x5F, 0x5F, 0x5F})

const int PIXELS = 6;

// LPD8806 Strip with 26 LEDs
DigitalStrip stripRgb = DigitalStrip(PIXELS);
Pattern stripPattern = Pattern(&stripRgb);


const int BUTTON_BACKLIGHT = D0;
const int BUTTON_SENSOR = D1;
const int DOOR_SENSOR = D2;
const int DOOR_CONTROL = D7;

const unsigned long BUTTON_DELAY = 50;
const unsigned long CONTROL_DELAY = 200;
const unsigned long DOOR_MOTION_DELAY = 20 * 1000;

typedef enum {
    KNOWN_FALSE = 0,
    KNOWN_TRUE = 1,
    UNKNOWN = 2,
} FuzzyBoolean;

static unsigned long doorControlTimeout = 0;
static unsigned long doorMotionTimeout = 0;
static FuzzyBoolean doorState = UNKNOWN; // Reset in Setup.
static FuzzyBoolean doorTarget = UNKNOWN; // Reset in Setup.

void setup() {
    stripPattern.setPattern(SOLID, SIGN, BLACK, 1000);

    // Init random number generator by reading from an unconnected pin.
    randomSeed(analogRead(A0));

    pinMode(BUTTON_BACKLIGHT, OUTPUT);
    pinMode(BUTTON_SENSOR, INPUT_PULLDOWN);
    pinMode(DOOR_SENSOR, INPUT_PULLDOWN);
    pinMode(DOOR_CONTROL, OUTPUT);

    digitalWrite(BUTTON_BACKLIGHT, false);
    digitalWrite(DOOR_CONTROL, false);

    publishDoorState(UNKNOWN);

    refresh("");

    Spark.function("refresh", refresh);
    Spark.function("door_target", doorOpenTarget);
    Spark.function("strip_target", setStripPattern);
}

int refresh(String text) {
    Spark.publish("door", DoorStateString(doorState), 60, PRIVATE);
    Spark.publish("strip", stripPattern.getText(), 60, PRIVATE);
    return 0;
}

void publishDoorState(int newState) {
    if (newState == doorState) {
        return;
    }

    doorState = (FuzzyBoolean)newState;

    // The backlight is on if we are open or closed, but not moving.
    digitalWrite(BUTTON_BACKLIGHT,
                 (doorState == KNOWN_TRUE) || (doorState == KNOWN_FALSE));
    Spark.publish("door", DoorStateString(doorState), 60, PRIVATE);
}

int setStripPattern(String text) {
  return stripPattern.setText(text);
}

void startDoorMotion(unsigned long now) {
    digitalWrite(DOOR_CONTROL, true);
    doorControlTimeout = now + CONTROL_DELAY;

    doorMotionTimeout = now + DOOR_MOTION_DELAY;
    publishDoorState(UNKNOWN);
}

void handleDoorControl(unsigned long now) {
    if (doorControlTimeout && now >= doorControlTimeout) {
        digitalWrite(DOOR_CONTROL, false);
        doorControlTimeout = 0;
    }
}

void handleGarageDoorButton(unsigned long now) {
    static unsigned long doorButtonDown = 0;
    static bool doorButtonPushed = false;

    // Is the Garage Door button pushed?
    bool newPushed = digitalRead(BUTTON_SENSOR);

    if (newPushed) {
        if (doorButtonPushed) {
            if (doorButtonDown && (now >= (doorButtonDown + BUTTON_DELAY))) {
                doorButtonDown = 0;
                startDoorMotion(now);
                Spark.publish("door_button", NULL, 60, PRIVATE);
            }
        } else {
            doorButtonDown = now;
        }
    }

    doorButtonPushed = newPushed;
}

void readDoorSensor(unsigned long now) {
    if (now >= doorMotionTimeout) {
        doorMotionTimeout = 0;
    }

    if (doorMotionTimeout == 0) {
        // Is the Garage Door open?
        publishDoorState(digitalRead(DOOR_SENSOR) ? KNOWN_FALSE : KNOWN_TRUE);
    }
}

void handleDoorToTarget(unsigned long now) {
    // If we have no target, or the door state is currently uknown, wait.
    if ((doorTarget == UNKNOWN) || (doorState == UNKNOWN)) {
        return;
    }

    // If we are already at the target, nothing to do.
    if (doorState != doorTarget) {
        // Toggle the state to what we want.
        startDoorMotion(now);
    }

    // We clear the target so we don't keep trying.
    doorTarget = UNKNOWN;
}

String DoorStateString(int value) {
    switch ((FuzzyBoolean)value) {
        case KNOWN_FALSE:
            return "closed";
        case KNOWN_TRUE:
            return "open";
        default:
            return "unknown";
    }
}

int doorOpenTarget(String args) {
    if (args == "open") {
        doorTarget = KNOWN_TRUE;
        return 0;
    } else if (args == "closed") {
        doorTarget = KNOWN_FALSE;
        return 0;
    } else if (args == "toggle") {
        if (doorState != UNKNOWN) {
            doorTarget = (FuzzyBoolean)!doorState;
            return 0;
        }
        if (doorTarget != UNKNOWN) {
            doorTarget = (FuzzyBoolean)!doorTarget;
            return 0;
        }

        return -2;
    }

    return -10;
}

void loop() {
    unsigned long now = millis();

    readDoorSensor(now);
    handleDoorControl(now);
    handleGarageDoorButton(now);

    handleDoorToTarget(now);

    if (stripPattern.drawUpdate()) {
        // If the pattern was updated, publish the new one.
        Spark.publish("strip", stripPattern.getText(), 60, PRIVATE);
    }
}
