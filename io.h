#include <stdint.h>

#define LEDS_X 7
#define LEDS_Y 8
#define LEDS_TANG 205


#define FRAMERATE 25

void start_io_threads();

const uint8_t *get_current_frame();
void release_frame();
