#include <stdint.h>
#include <math.h>
#include <stdio.h>

#include "ledtorus2_anim.h"
#include "render3d.h"


static struct vec3d
cross_prod(float ax, float ay, float az, float bx, float by, float bz)
{
  struct vec3d c;
  c.x = ay*bz - az*by;
  c.y = az*bx - ax*bz;
  c.z = ax*by - ay*bx;
  return c;
}

static float
dot_prod(float ax, float ay, float az, float bx, float by, float bz)
{
  return ax*bx + ay*by + az*bz;
}


static struct vec3d
norm_vec(struct vec3d v)
{
  float l = sqrtf(v.x*v.x + v.y*v.y + v.z*v.z);
  v.x /= l;
  v.y /= l;
  v.z /= l;
  return v;
}


static struct colour3
check_point_against_poly(struct vec3d q, const struct stanford_ply *ply,
                         float max_inside_dist, float max_outside_dist)
{
  uint32_t face;
  uint32_t best_face;
  float best_dist;
  int found_any = 0;
  struct colour3 col;

  for (face = 0; face < ply->num_face; ++face) {
    uint32_t num_edge = ply->face_idx[face][0];
    uint32_t *vert = &(ply->face_idx[face][1]);
    struct vec3d p, n, v, u1, u2;
    float dist;
    int inside;

    p.x = ply->vertex[vert[0]][0];
    p.y = ply->vertex[vert[0]][1];
    p.z = ply->vertex[vert[0]][2];
    /* For now take a random vertex normal. */
    n.x = ply->normal[vert[0]][0];
    n.y = ply->normal[vert[0]][1];
    n.z = ply->normal[vert[0]][2];

    /* Find the normal distance to the plane of the poly as projection onto the normal. */
    v.x = q.x - p.x;
    v.y = q.y - p.y;
    v.z = q.z - p.z;
    dist = dot_prod(v.x, v.y, v.z, n.x, n.y, n.z);
    if ((dist < 0 && -dist > max_inside_dist) ||
        (dist > 0 && dist > max_outside_dist) ||
        (found_any && fabsf(dist) > best_dist))
      continue;

    /*
      Check against all edges if q is inside.
      We extend each edge along the poly normal to form a plane.
      And compute the (signed) distance from point q to each plane to see if q is
      on the inside of each plane.
    */
    inside = 1;
    u2.x = ply->vertex[vert[num_edge-1]][0];
    u2.y = ply->vertex[vert[num_edge-1]][1];
    u2.z = ply->vertex[vert[num_edge-1]][2];
    for (int i = 0; i < num_edge; ++i) {
      struct vec3d r;
      float dist2;
      u1 = u2;
      u2.x = ply->vertex[vert[i]][0];
      u2.y = ply->vertex[vert[i]][1];
      u2.z = ply->vertex[vert[i]][2];
      /* ToDo: these r normals to each edge could be pre-computed. */
      r = norm_vec(cross_prod(u2.x-u1.x, u2.y-u1.y, u2.z-u1.z, n.x, n.y, n.z));
      dist2 = dot_prod(q.x-u1.x, q.y-u1.y, q.z-u1.z, r.x, r.y, r.z);
      if (dist2 > 0.0f) {
        inside = 0;
        break;
      }
    }

    if (!inside)
      continue;

    found_any = 1;
    best_dist = fabs(dist);
    best_face = face;
  }

  if (found_any) {
    /* ToDo: Here we might interpolate colours from different corners of the face. */
    col.r = ply->c_r[ply->face_idx[best_face][1]];
    col.g = ply->c_g[ply->face_idx[best_face][1]];
    col.b = ply->c_b[ply->face_idx[best_face][1]];
  } else {
    col.r = 0; col.g = 0; col.b = 0;
  }
  return col;
}


uint32_t
render3d_init(struct st_render3d *c)
{
  //const char *filename = "vertex_colour_test2.ply";
  //const char *filename = "blender/rubberduck.ply";
  const char *filename = "blender/Cactus.ply";

return 0;

  if (load_ply(filename, &c->ply))
    return 1;
  normalize_ply(&c->ply);

  return 0;
}


uint32_t
render3d_anim_frame(frame_t *f, uint32_t frame, struct st_render3d *c)
{
  int ix, iz, ia;
  char buf[100];
  const char *filename = "/tmp/x/frame_%06u.ply";
  const int num_frames = 210;

  snprintf(buf, sizeof(buf)-1, filename, (unsigned)(frame%num_frames) + 1);
  if (load_ply(buf, &c->ply))
    return 1;
  normalize_ply(&c->ply);

  cls(f);
  envelope(f, frame);

  for (ix = 0; ix < LEDS_X; ++ix) {
    for (ia = 0; ia < LEDS_TANG; ia+=2) {
      struct torus_xz pos2 = torus_polar2rect((float)ix,
                                              (float)ia+0.0f*0.3f*(float)frame+30.0f);
      float x = pos2.x;
      float y = pos2.z;
      for (iz = 0; iz < LEDS_Y; ++iz) {
        const float scaling = 0.9f*/*ToDo*/(2.0f/(float)(LEDS_X-1));
        float z = (float)iz;
        float rdx = (x - 8.25) * scaling;
        float rdy = y * scaling;
        float rdz = (z - 0.5f*(float)(LEDS_Y-1)) * scaling;
        struct vec3d q;
        struct colour3 col;
        q.x = rdx;
        q.y = rdy;
        q.z = rdz;
/*
        col = check_point_against_poly(q, &c->ply,
                                       0.125f + 0.125f*sinf((float)frame*.02f),
                                       0*0.0333f);
*/
        col = check_point_against_poly(q, &c->ply,
                                       .15f,
                                       0.05f);
        if (col.r != 0 || col.g != 0 || col.b != 0)
          setpix(f, ix, (LEDS_Y-1)-iz, (LEDS_TANG-1)-ia, col.r, col.g, col.b);
      }
    }
  }

  free_ply(&c->ply);
  return (frame > 2*60*25);
}
