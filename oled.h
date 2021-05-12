#define OLED_CS 10
#define OLED_RESET 9
#define OLED_DC 8

void oledInit();

void oledWriteFb(uint8_t *fb);
