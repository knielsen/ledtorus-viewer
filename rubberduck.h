#include "ledtorus_anim.h"

struct st_rubberduck {
  float density[LEDS_X][LEDS_Y][LEDS_TANG];
  float *points;
  int num_points;
};

extern uint32_t rubberduck_init(struct st_rubberduck *c);
extern uint32_t rubberduck_anim_frame(frame_t *f, uint32_t frame, struct st_rubberduck *c);
