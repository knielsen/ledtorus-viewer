#include <stdint.h>


#define FRAMERATE 25

void start_io_threads();

const uint8_t *get_current_frame();
void release_frame();
