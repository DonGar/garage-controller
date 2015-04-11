
typedef struct {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} Color;

// The colors go from 0x00 to 0x7F in brightness (the high bit is ignored).
extern Color BLACK;
extern Color WHITE;
extern Color RED;
extern Color GREEN;
extern Color BLUE;
extern Color YELLOW;
extern Color SIGN;

// These special colors will be converted to a random value by most patterns.
extern Color RANDOM;
extern Color RANDOM_PRIMARY;

typedef enum {
    SOLID,
    PULSE,
    CYLON,
    ALTERNATE,
    FLICKER,
    LAVA
} Pattern;

void strip_init(int pixels);
void strip_pattern(Pattern pattern, Color a, Color b, int speed);
void strip_update(unsigned long now);

String strip_get_pattern_text();
int strip_set_pattern_text(String text);
