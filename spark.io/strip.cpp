#include<application.h>
#include "strip.h"
#include "math.h"

Color BLACK  = {0x00, 0x00, 0x00};
Color WHITE  = {0x7F, 0x7F, 0x7F};
Color RED    = {0x7F, 0x00, 0x00};
Color GREEN  = {0x00, 0x7F, 0x00};
Color BLUE   = {0x00, 0x00, 0x7F};
Color YELLOW = {0x7F, 0x7F, 0x00};
Color SIGN   = {0x5F, 0x5F, 0x5F};

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

uint8_t morph_shade(uint8_t base, uint8_t target) {
    if (base < target)
        base++;

    if (base > target)
        base--;

    return base;
}

// This has no rounding errors if you want to slowly/smoothly move from color a to b.
Color morph_color(Color base, Color target) {
    Color result;

    result.red = morph_shade(base.red, target.red);
    result.green = morph_shade(base.green, target.green);
    result.blue = morph_shade(base.blue, target.blue);

    return result;
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

unsigned long handle_lava(bool initial, Color a, Color b, unsigned long speed) {

    typedef struct {
        int pos;
        int size;
        int duration;
        Color color;
    } Blob;

    static const int blobs = 3;
    static Blob blob[blobs];
    static Color *strip = NULL;

    // Intialize all of our blobs to be off screen (so to speak).
    if (initial) {
        if (!strip) {
            strip = (Color *)malloc(sizeof(Color) * _pixels);
        }

        // Initialize the strip.
        for (int i = 0; i < _pixels; i++) {
            strip[i] = BLACK;
        }

        // Initialize the blobs to not exist.
        for (int b = 0; b < blobs; b++) {
            blob[b].pos = -1;
        }
    }

    // Mutate our blobs.
    for (Blob *b = blob; b < (blob + blobs); b++) {

        b->duration--;

        // If it's not currently displayed.
        if (b->pos == -1) {
            if (b->duration <= 0) {
                b->pos = random(_pixels);
                b->size = _pixels - log(random(exp(_pixels)));
                b->duration = random(speed);
                b->color = expand_random(a);
            }
        } else {
            if (b->duration <= 0) {
                b->pos = -1;
                b->duration = random(speed);
            }
        }
    }

    // Apply background to strip.
    Color background = expand_random(b);
    for (int i = 0; i < _pixels; i++) {
        strip[i] = morph_color(strip[i], background);
    }

    // Apply blobs to strip.
    for (Blob *b = blob; b < (blob + blobs); b++) {
        if (b->pos == -1)
            continue;

        for (int i = (b->pos - b->size); i <= (b->pos + b->size); i++) {
            int offset = i % _pixels;
            strip[offset] = morph_color(strip[offset], b->color);
        }
    }

    // This draws our strip.
    for (int i = 0; i < _pixels; i++) {
        draw_pixel(strip[i]);
    }
    issue_latch();

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

void strip_update(unsigned long now) {
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
        case LAVA:
            _next_draw += handle_lava(initial, _color_a, _color_b, _speed);
            break;
        default:
            break;
    }
}

// Returns "0xFFFFFF"
String color_to_string(Color color) {
    uint8_t value = color.red << 16 | color.green << 8 | color.blue;

    return String("0x") + String(value, HEX);
}

String strip_get_pattern_text() {
    char *patternMap[] = {"SOLID", "PULSE", "CYLON", "ALTERNATE", "FLICKER", "LAVA"};

    // <PATTERN>,0xFFFFFF,0xFFFFFF,55
    return (String(patternMap[_pattern]) + ',' +
            color_to_string(_color_a) + ',' +
            color_to_string(_color_b) + ',' +
            _speed);
}

int strip_set_pattern_text(String text) {
    return -1;
}
