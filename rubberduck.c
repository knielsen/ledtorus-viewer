#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "rubberduck.h"


static int
cmp_3float(const void *va, const void *vb)
{
  const float *a = va;
  const float *b = vb;
  if (a[0] < b[0])
    return -1;
  else if (a[0] > b[0])
    return 1;
  else if (a[1] < b[1])
    return -1;
  else if (a[1] > b[1])
    return 1;
  else if (a[2] < b[2])
    return -1;
  else if (a[2] > b[2])
    return 1;
  else
    return 0;
}



static int
low_limit(float *p, int count, float low_val)
{
  int l, h, m;

  l = 0;
  h = count-1;
  for (;;) {
    float v;

    if (l == h)
      break;
    m = (l + h)/2;
    v = p[3*m];
    if (v < low_val)
      l = m+1;
    else
      h = m;
  }
  return l;
}


static int
high_limit(float *p, int count, float high_val)
{
  int l, h, m;

  l = 0;
  h = count-1;
  for (;;) {
    float v;

    if (l == h)
      break;
    m = (l + h + 1)/2;
    v = p[3*m];
    if (v > high_val)
      h = m-1;
    else
      l = m;
  }
  return l;
}


static float
fold_points_with_kernel(struct st_rubberduck *c, float x, float y, float z)
{
  /*
    Now sure which kernel is best to use?
    For now, let's start with something simple.
    Everything within a radius of 0.5 is counted with equal weight.
  */
  const float range = 0.074f;

  float s = 0.0f;
  float *p;
  int low_idx, high_idx;
  int i;

  p = c->points;
  low_idx = low_limit(p, c->num_points, x-range);
  high_idx = high_limit(p, c->num_points, x+range);
  for (i = low_idx; i <= high_idx; ++i) {
    float dx = x - p[3*i];
    float dy = y - p[3*i+1];
    float dz = z - p[3*i+2];
    float dist = sqrt(dx*dx + dy*dy + dz*dz);
    if (dist > range)
      continue;
    s = s +1.0f;
  }

  return s;
}


uint32_t
rubberduck_init(struct st_rubberduck *c)
{
  const char *filename = "rubber-duck-10_samp.pcd";
  FILE *fp;
  char buf[1024];
  int in_header;
  int idx;
  float cx, cy, cz;
  float size;

  c->points = NULL;

  if (!(fp = fopen(filename, "r")))
    return 1;

  in_header = 1;
  c->num_points = 0;
  for (;;) {
    char *res = fgets(buf, sizeof(buf), fp);
    if (!res)
      break;

    if (in_header) {
      if (0 == strncmp("POINTS ", buf, 7))
        c->num_points = atoi(buf + 7);
      else if (0 == strncmp("DATA ascii", buf, 10))
      {
        if (!c->num_points)
          return 1;
        c->points = calloc(c->num_points*3, sizeof(*c->points));
        in_header = 0;
        idx = 0;
      }
    } else {
      float x = 0.0f, y = 0.0f, z = 0.0f;

      if (idx > 3*c->num_points)
        break;
      sscanf(buf, "%f %f %f", &x, &z, &y);
      c->points[idx++] = x;
      c->points[idx++] = -y;
      c->points[idx++] = z;
    }
  }
  fclose(fp);

  /*
    Translate the points to put the center-of-mass at (0,0,0).
    And scale so that it fits within [-1,1] in all dimensions.
  */
  cx = cy = cz = 0.0f;
  for (idx = 0; idx < 3*c->num_points; idx += 3) {
    cx += c->points[idx];
    cy += c->points[idx+1];
    cz += c->points[idx+2];
  }
  cx /= (float)c->num_points;
  cy /= (float)c->num_points;
  cz /= (float)c->num_points;
  size = 0.0f;
  for (idx = 0; idx < 3*c->num_points; idx += 3) {
    c->points[idx] -= cx;
    c->points[idx+1] -= cy;
    c->points[idx+2] -= cz;
    if (fabsf(c->points[idx]) > size)
      size = fabsf(c->points[idx]);
    if (fabsf(c->points[idx+1]) > size)
      size = fabsf(c->points[idx+1]);
    if (fabsf(c->points[idx+2]) > size)
      size = fabsf(c->points[idx+2]);
  }
  if (size != 0.0f)
    size = 1.0f/size;
  for (idx = 0; idx < 3*c->num_points; idx += 3) {
    c->points[idx] *= size;
    c->points[idx+1] *= size;
    c->points[idx+2] *= size;
  }

  /* Sort the points by x,y,z. */
  qsort(c->points, c->num_points, 3*sizeof(float), cmp_3float);

  return 0;
}


uint32_t
rubberduck_anim_frame(frame_t *f, uint32_t frame, struct st_rubberduck *c)
{
  int ix, iy, ia;
  float max_density;

  cls(f);
  envelope(f, frame);

  max_density = 0.0f;
  for (ix = 0; ix < LEDS_X; ++ix) {
    for (ia = 0; ia < LEDS_TANG; ++ia) {
      struct torus_xz pos2 = torus_polar2rect((float)ix,
                                              (float)ia+0.3*(float)frame);
      float x = pos2.x;
      float z = pos2.z;
      for (iy = 0; iy < LEDS_Y; ++iy) {
        const float scaling = 0.55f*/*ToDo*/(2.0f/(float)(LEDS_X-1));
        float y = (float)iy;
        float rdx = (x - 5.6f) * scaling;
        float rdy = (y - 2 - 0.5f*(float)(LEDS_Y-1)) * scaling;
        float rdz = z * scaling;
        c->density[ix][iy][ia] = fold_points_with_kernel(c, rdx, rdy, rdz);
        if (c->density[ix][iy][ia] > max_density)
          max_density = c->density[ix][iy][ia];
      }
    }
  }

  if (max_density > 0.0f)
    max_density = 1.0f/max_density;
  for (ix = 0; ix < LEDS_X; ++ix) {
    for (ia = 0; ia < LEDS_TANG; ++ia) {
      for (iy = 0; iy < LEDS_Y; ++iy) {
        float density = c->density[ix][iy][ia] * max_density;
        if (density > 0.01) {
          float cr = 1.0f*density;
          float cg = 1.0f*density;
          float cb = 0.0f*density;
          setpix(f, ix, iy, ia, (uint8_t)255.0*cr, (uint8_t)255.0*cg, (uint8_t)255.0*cb);
        }
#if 0
        // Some kind of axis...
        if (iy == LEDS_Y/2 && (ia == 0 || ia == LEDS_TANG/2))
          setpix(f, ix, iy, ia, 255, 0, 0);
        else if (iy == LEDS_Y/2 && (ia == LEDS_TANG/4 || ia == 3*LEDS_TANG/4))
          setpix(f, ix, iy, ia, 0, 255, 0);
#endif
      }
    }
  }

  return (frame > 2*60*25);
}
