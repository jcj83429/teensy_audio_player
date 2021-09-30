#define OLED_CS 10
#define OLED_RESET 14
#define OLED_DC 12

void oledInit();

void oledWriteFb(uint8_t *fb);
