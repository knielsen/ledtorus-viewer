#include <stdint.h>
#include <math.h>
#include <stdio.h>

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


void
check_point_against_poly2(struct vec3d q, uint32_t face, const struct stanford_ply *ply)
{
  uint32_t num_face = ply->face_idx[face][0];
  uint32_t *vert = &(ply->face_idx[face][1]);
  struct vec3d p, n, v, w, u1, u2, r, qp;
  float dist, a;
  int inside;

  /* Compute the projection qp of q onto the plane of the poly. */
  p.x = ply->vertex[vert[0]][0];
  p.y = ply->vertex[vert[0]][1];
  p.z = ply->vertex[vert[0]][2];
  /* For now take a random vertex normal. */
  n.x = ply->normal[vert[0]][0];
  n.y = ply->normal[vert[0]][1];
  n.z = ply->normal[vert[0]][2];

  v.x = q.x - p.x;
  v.y = q.y - p.y;
  v.z = q.z - p.z;
  dist = dot_prod(v.x, v.y, v.z, n.x, n.y, n.z);
  qp.x = q.x - dist*n.x;
  qp.y = q.y - dist*n.y;
  qp.z = q.z - dist*n.z;

  /* Check against all edges if qp is inside. */
  inside = 1;
  w.x = qp.x - p.x;
  w.y = qp.y - p.y;
  w.z = qp.z - p.z;
  u2 = p;
  for (int i = 0; i < num_face-1; ++i) {
    u1 = u2;
    u2.x = ply->vertex[vert[i+1]][0];
    u2.y = ply->vertex[vert[i+1]][1];
    u2.z = ply->vertex[vert[i+1]][2];
    r = cross_prod(u2.x-u1.x, u2.y-u1.y, u2.z-u1.z, w.x, w.y, w.z);
    a = dot_prod(r.x, r.y, r.z, n.x, n.y, n.z);
    if (a < 1e-4) {
      inside = 0;
      break;
    }
  }

  printf("Face: %u dist %g is %s\n", (unsigned)face, dist, (inside ? "inside" : "outside"));
}


void
check_point_against_poly(struct vec3d q, uint32_t face, const struct stanford_ply *ply)
{
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
    r = norm_vec(cross_prod(u2.x-u1.x, u2.y-u1.y, u2.z-u1.z, n.x, n.y, n.z));
    dist2 = dot_prod(q.x-u1.x, q.y-u1.y, q.z-u1.z, r.x, r.y, r.z);
    if (dist2 > 0.0f) {
      inside = 0;
      break;
    }
  }

  printf("Face: %u dist %g is %s\n", (unsigned)face, dist, (inside ? "inside" : "outside"));
}
