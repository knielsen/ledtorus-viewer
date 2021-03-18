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


struct colour3
check_point_against_poly(struct vec3d q, const struct stanford_ply *ply)
{
  const float max_inside_dist = 0.3;
  const float max_outside_dist = 0.125;
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
printf("   dist @ %g\n", dist);
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

    printf("  face: %u dist %g is %s\n", (unsigned)face, dist, (inside ? "inside" : "outside"));
    if (!inside)
      continue;

    found_any = 1;
    best_dist = fabs(dist);
    best_face = face;
  }

  if (found_any) {
    printf("Face %u -> dist %g\n", best_face, best_dist);
    /* ToDo: Here we might interpolate colours from different corners of the face. */
    col.r = ply->c_r[ply->face_idx[best_face][1]];
    col.g = ply->c_g[ply->face_idx[best_face][1]];
    col.b = ply->c_b[ply->face_idx[best_face][1]];
  } else {
    printf("no close face\n");
    col.r = 0; col.g = 0; col.b = 0;
  }
  printf("  colour %3d %3d %3d\n", col.r, col.g, col.b);
  return col;
}
