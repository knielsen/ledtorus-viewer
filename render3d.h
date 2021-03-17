#include "stanford_ply_loader.h"

struct vec3d {
  float x, y, z;
};


extern void check_point_against_poly(struct vec3d q, uint32_t face,
                                     const struct stanford_ply *ply);
