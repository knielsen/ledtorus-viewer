#include "stanford_ply_loader.h"
#include "ledtorus2_anim.h"


struct st_render3d {
  struct stanford_ply ply;
};


extern uint32_t render3d_init(struct st_render3d *c);
extern uint32_t render3d_anim_frame(frame_t *f, uint32_t frame, struct st_render3d *c);
