// This #include statement was automatically added by the Spark IDE.
#include "strip.h"


const int PIXELS = 6;

const int BUTTON_BACKLIGHT = D0;
const int BUTTON_SENSOR = D1;
const int DOOR_SENSOR = D2;
const int DOOR_CONTROL = D7;

const unsigned long CONTROL_DELAY = 400;
const unsigned long DOOR_MOTION_DELAY = 20 * 1000;

typedef enum {
    KNOWN_FALSE = 0,
    KNOWN_TRUE = 1,
    UNKNOWN = 2,
} FuzzyBoolean;

String FuzzyBooleanJson(int value) {
    switch ((FuzzyBoolean)value) {
        case KNOWN_FALSE:
            return "false";
        case KNOWN_TRUE:
            return "true";
        default:
            return "null";
    }
}

static bool buttonPushed = false;
static unsigned long controlTimeout = 0;
static unsigned long doorMotionTimeout = 0;
static FuzzyBoolean doorState = KNOWN_FALSE; // Reset in Setup.
static FuzzyBoolean doorTarget = KNOWN_FALSE; // Reset in Setup.

void setup() {
    strip_init(PIXELS);
    strip_pattern(SOLID, SIGN, BLACK, 1000);

    // Init random number generator by reading from an unconnected pin.
    randomSeed(analogRead(A0));

    pinMode(BUTTON_BACKLIGHT, OUTPUT);
    pinMode(BUTTON_SENSOR, INPUT_PULLDOWN);
    pinMode(DOOR_SENSOR, INPUT_PULLDOWN);
    pinMode(DOOR_CONTROL, OUTPUT);

    digitalWrite(BUTTON_BACKLIGHT, true);
    digitalWrite(DOOR_CONTROL, false);

    update_door_state(UNKNOWN);
    update_door_target(UNKNOWN);

    Spark.publish("strip", strip_get_pattern_text(), 60, PRIVATE);

    Spark.function("refresh", refresh);
    Spark.function("door_target", door_open_target);
    Spark.function("strip_target", strip_set_pattern_text);
}

int refresh(String text) {
    Spark.publish("door_open", FuzzyBooleanJson(doorState), 60, PRIVATE);
    Spark.publish("door_open_target", FuzzyBooleanJson(doorTarget), 60, PRIVATE);
    Spark.publish("strip", strip_get_pattern_text(), 60, PRIVATE);
    return 0;
}

void update_door_state(int newState) {
    if (newState == doorState) {
        return;
    }

    doorState = (FuzzyBoolean)newState;

    // The backlight is on if we are open or closed, but not moving.
    digitalWrite(BUTTON_BACKLIGHT,
                 (doorState == KNOWN_TRUE) || (doorState == KNOWN_FALSE));
    Spark.publish("door_open", FuzzyBooleanJson(doorState), 60, PRIVATE);
}

void update_door_target(int newTarget) {
    if (newTarget == doorTarget) {
        return;
    }

    doorTarget = (FuzzyBoolean)newTarget;
    Spark.publish("door_open_target", FuzzyBooleanJson(doorTarget), 60, PRIVATE);
}

void start_door_motion(unsigned long now) {
    digitalWrite(DOOR_CONTROL, true);
    controlTimeout = now + CONTROL_DELAY;

    doorMotionTimeout = now + DOOR_MOTION_DELAY;
    update_door_state(UNKNOWN);
}

void handle_garage_door_button(unsigned long now) {
    if (controlTimeout && now >= controlTimeout) {
        digitalWrite(DOOR_CONTROL, false);
        controlTimeout = 0;
    }

    // Is the Garage Door button pushed?
    bool newPushed = digitalRead(BUTTON_SENSOR);
    if (newPushed != buttonPushed) {
        // Publish Event
        buttonPushed = newPushed;

        if (buttonPushed) {
            start_door_motion(now);
            Spark.publish("door_button", NULL, 60, PRIVATE);
        }
    }
}

void read_door_sensor(unsigned long now) {
    if (now >= doorMotionTimeout) {
        doorMotionTimeout = 0;
    }

    if (doorMotionTimeout == 0) {
        // Is the Garage Door open?
        update_door_state(digitalRead(DOOR_SENSOR) ? KNOWN_FALSE : KNOWN_TRUE);
    }

}

void handle_door_to_target(unsigned long now) {
    // If we have no target, or the door state is currently uknown, wait.
    if ((doorTarget == UNKNOWN) || (doorState == UNKNOWN)) {
        return;
    }

    // If we are already at the target, nothing to do.
    if (doorState != doorTarget) {
        // Toggle the state to what we want.
        start_door_motion(now);
    }

    // We clear the target so we don't keep trying.
    doorTarget = UNKNOWN;
}

int door_open_target(String args) {
    if (args == "true") {
        update_door_target(KNOWN_TRUE);
        return 0;
    } else if (args == "false") {
        update_door_target(KNOWN_FALSE);
        return 0;
    } else if (args == "toggle") {
        if (doorState != UNKNOWN) {
            update_door_target(!doorState);
            return 0;
        }
        if (doorTarget != UNKNOWN) {
            update_door_target(!doorTarget);
            return 0;
        }

        return -2;
    }

    return -10;
}

void loop() {
    unsigned long now = millis();

    read_door_sensor(now);
    handle_garage_door_button(now);

    handle_door_to_target(now);

    strip_update(now);
}
