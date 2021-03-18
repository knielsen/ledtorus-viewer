#include "stanford_ply_loader.h"
#include "ledtorus2_anim.h"

struct vec3d {
  float x, y, z;
};


extern struct colour3 check_point_against_poly(struct vec3d q,
                                               const struct stanford_ply *ply);
