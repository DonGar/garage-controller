#include<application.h>
#include "strip.h"

Color BLACK  = {0x00, 0x00, 0x00};
Color WHITE  = {0x7F, 0x7F, 0x7F};
Color RED    = {0x7F, 0x00, 0x00};
Color GREEN  = {0x00, 0x7F, 0x00};
Color BLUE   = {0x00, 0x00, 0x7F};
Color YELLOW = {0x7F, 0x7F, 0x00};

// Because these values include the high bit, they are not in the valid color space.
Color RANDOM         = {0x80, 0x80, 0x80};
Color RANDOM_PRIMARY = {0x81, 0x81, 0x81};


static int _pixels = 0;

//
// Internal draw routines.
//

void issue_latch() {
    int latch_size = ((_pixels+31) / 32) * 8;

    for (int i = 0; i < latch_size; i++) {
        SPI.transfer(0);
    }
}

void draw_pixel(Color color) {
    // GRB color order, oddly.
    SPI.transfer(color.green  | 0x80);
    SPI.transfer(color.red    | 0x80);
    SPI.transfer(color.blue   | 0x80);
}

void draw_solid(Color color) {
    // GRB color order, oddly.
    for (int i = 0; i < _pixels; i++) {
        draw_pixel(color);
    }

    issue_latch();
}

//
// Internal color manipulation helpers.
//

// ratio 0 means all left color. ratio 1.0 means all right color.
Color mix_color(Color left, Color right, float ratio) {

    float l_ratio = 1.0 - ratio;
    float r_ratio = ratio;

    Color result;

    result.red = ((left.red & 0x7F) * l_ratio) + ((right.red & 0x7F) * r_ratio);
    result.green = ((left.green & 0x7F) * l_ratio) + ((right.green & 0x7F) * r_ratio);
    result.blue = ((left.blue & 0x7F) * l_ratio) + ((right.blue & 0x7F) * r_ratio);

    return result;
}

// brightness is from 0.0 to 1.0.
Color dim_color(Color color, float brightness) {
    return mix_color(BLACK, color, brightness);
}

// This will pick truely random red/green/blue components.
// This will tend more towards dirty/dim white than you would expect.
Color random_color() {
    Color result;

    result.red = random(0, 0x80);
    result.green = random(0, 0x80);
    result.blue = random(0, 0x80);

    return result;
}

Color random_primary_color() {
    Color result;

    result.red = random(0, 2) ? 0x7F : 0;
    result.green = random(0, 2) ? 0x7F : 0;
    result.blue = random(0, 2) ? 0x7F : 0;

    return result;
}

bool color_equal(Color left, Color right) {
    return ((left.red == right.red) &&
            (left.green == right.green) &&
            (left.blue == right.blue));
}

Color expand_random(Color color) {
    if (color_equal(color, RANDOM))
        return random_color();

    if (color_equal(color, RANDOM_PRIMARY))
        return random_primary_color();

    return color;
}

//
// Routines to implement Pattern animations.
//

unsigned long handle_pulse(bool initial, Color a, Color b, unsigned long speed) {
    static int steps = 0x7F;

    static Color left;
    static Color right;
    static bool go_right;
    static int pulse_count;

    if (initial) {
        left = BLACK;
        right = BLACK;
        go_right = true;
        pulse_count = 0;
    }

    // Bounce directions, if needed.
    if (pulse_count >= steps) {
        go_right = false;
        left = expand_random(a);
    }
    if (pulse_count <= 0) {
        go_right = true;
        right = expand_random(b);
    }

    float ratio = pulse_count / (float)steps;

    draw_solid(mix_color(left, right, ratio));

    // Increment.
    if (go_right) {
        pulse_count++;
    } else {
        pulse_count--;
    }

    return speed / steps;
}


unsigned long handle_cylon(bool initial, Color a, Color b, unsigned long speed) {
    static bool right;
    static int eye;
    static Color eye_center;
    static Color background;
    static Color eye_edge;

    if (initial) {
        right = true;
        eye = 0;
        eye_center = expand_random(a);
        background = expand_random(b);
        eye_edge = mix_color(eye_center, background, 0.95);
    }

    unsigned long delay = speed / (_pixels * 2);

    // Bounce directions, if needed.
    if (eye >= (_pixels-1)) {
        right = false;
        delay = delay * 3;
    }

    if (eye <= 0) {
        right = true;
        delay = delay * 3;
    }

    // Do the draw.
    for (int i = 0; i < _pixels; i++) {
        if (i == eye - 1) {
            draw_pixel(eye_edge);
        } else if (i == eye) {
            draw_pixel(eye_center);
        } else if (i == eye + 1) {
            draw_pixel(eye_edge);
        } else {
            draw_pixel(background);
        }
    }
    issue_latch();

    // Increment.
    if (right) {
        eye++;
    } else {
        eye--;
    }

    return delay;
}

unsigned long handle_alternate(bool initial, Color a, Color b, unsigned long speed) {
    static bool state;

    if (initial) {
        state = false;
    }

    a = expand_random(a);
    b = expand_random(b);

    for (int i = 0; i < _pixels; i++) {
        draw_pixel(((i % 2) == state) ? a : b);
    }
    issue_latch();

    state = !state;

    return speed;
}

// This attempts to simulate a light with a poor electrical connection (often used
// at halloween). This is done by using a bounded drunkards walk across a threshold
// that changes the lights state.
//
// speed values between 20 and 100 are recommended for best effect (the larger the
// range, the less frequent flicker behavior is).
unsigned long handle_flicker(bool initial, Color a, Color b, unsigned long speed) {
    static Color on;
    static Color off;
    static bool state;
    static int weight;

    if (initial) {
        on = expand_random(a);
        off = expand_random(b);
        state = false;
        weight = speed / 2;
    }

    // -1, 0, 1
    weight += random(-1, 2);

    // Ensure state remains in range.
    if (weight < 0) {
        weight = 0;
        on = expand_random(a);
    }

    if (weight > speed) {
        weight = speed;
        off = expand_random(b);
    }

    bool new_state = weight >= (speed / 2);

    if (new_state != state) {
        state = new_state;
        draw_solid(state ? on : off);
    }

    return 10;
}


//
// Public Methods
//

void strip_init(int pixels) {
    _pixels = pixels;

    SPI.begin();
    SPI.setBitOrder(MSBFIRST);
    SPI.setDataMode(SPI_MODE0);
    SPI.setClockDivider(32);

    issue_latch();
    draw_solid(BLACK);
}

static Pattern _pattern = SOLID;
static Color _color_a = BLACK;
static Color _color_b = BLACK;
static int _speed = 0;
static unsigned long _next_draw = 0;

void strip_pattern(Pattern pattern, Color a, Color b, int speed) {
    _pattern = pattern;
    _color_a = a;
    _color_b = b;
    _speed = speed;

    _next_draw = 0;
}

void strip_update() {
    unsigned long now = millis();

    bool initial = _next_draw == 0;

    if (initial) {
        _next_draw = now;
    }

    if (now < _next_draw)
        return;

    switch (_pattern) {
        case SOLID:
            draw_solid(expand_random(_color_a));
            _next_draw += _speed;
            break;
        case PULSE:
            _next_draw += handle_pulse(initial, _color_a, _color_b, _speed);
            break;
        case CYLON:
            _next_draw += handle_cylon(initial, _color_a, _color_b, _speed);
            break;
        case ALTERNATE:
            _next_draw += handle_alternate(initial, _color_a, _color_b, _speed);
            break;
        case FLICKER:
            _next_draw += handle_flicker(initial, _color_a, _color_b, _speed);
            break;
    }
}
