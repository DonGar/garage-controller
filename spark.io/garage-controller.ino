// This #include statement was automatically added by the Spark IDE.
#include "strip.h"


const int PIXELS = 26;

const int BUTTON_BACKLIGHT = D0;
const int BUTTON_SENSOR = D1;
const int DOOR_SENSOR = D2;
const int DOOR_CONTROL = D7;

const unsigned long CONTROL_DELAY = 400;

const int UNKNOWN = 2;

void setup() {
    strip_init(PIXELS);
    // strip_pattern(CYLON, RED, BLACK, 5000);

    // Init random number generator by reading from an unconnected pin.
    randomSeed(analogRead(A0));

    pinMode(BUTTON_BACKLIGHT, OUTPUT);
    pinMode(BUTTON_SENSOR, INPUT_PULLDOWN);
    pinMode(DOOR_SENSOR, INPUT_PULLDOWN);
    pinMode(DOOR_CONTROL, OUTPUT);

    digitalWrite(BUTTON_BACKLIGHT, false);
    digitalWrite(DOOR_CONTROL, false);
}

static bool buttonPushed = false;
static unsigned long controlTimeout = 0;
static int doorOpen = UNKNOWN; // This is a boolean value that isn't known at start.

void handle_garage_door_button() {
    unsigned long now = millis();

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
            digitalWrite(DOOR_CONTROL, true);
            controlTimeout = now + CONTROL_DELAY;
            Spark.publish("door_button", NULL, 60, PRIVATE);
        }
    }
}

void update_door_sensor() {
    // Is the Garage Door open?
    bool newOpen = digitalRead(DOOR_SENSOR);
    if (newOpen != doorOpen) {
        // Publish Event
        doorOpen = newOpen;

        digitalWrite(BUTTON_BACKLIGHT, !doorOpen);
        Spark.publish("door_open", doorOpen ? "true" : "false", 60, PRIVATE);
    }
}

void loop() {
    strip_update();
    update_door_sensor();
    handle_garage_door_button();
}
