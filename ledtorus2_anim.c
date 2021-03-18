/*
  gcc -Wall ledtorus2_anim.c simplex_noise.c colours.c -o ledtorus2_anim -lm
*/

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <unistd.h>

#include "ledtorus2_anim.h"
#include "simplex_noise.h"
#include "colours.h"

/*
   A factor to compensate that pixels are much smaller in the tangential
   direction than in the radial and vertical direction.
*/
static const float tang_factor = 2.0f;

struct colour3 {
  uint8_t r, g, b;
};

struct hsv3 {
  uint8_t h, s, v;
};
static inline struct hsv3 mk_hsv3(uint8_t h, uint8_t s, uint8_t v)
{
  struct hsv3 res;
  res.h = h;
  res.s = s;
  res.v = v;
  return res;
}


static inline struct hsv3 mk_hsv3_f(float h, float s, float v)
{
  struct hsv3 res;
  res.h = roundf(h*255.0f);
  res.s = roundf(s*255.0f);
  res.v = roundf(v*255.0f);
  return res;
}


/* Number of migrating dots along one side of the migrating plane. */
#define MIG_SIDE LEDS_Y


/*
  Union with state data for all animations that need one. This way, a single
  statically-allocated memory area can be shared among all animations.
*/
union anim_data {
  struct st_fireworks {
    uint32_t num_phase1;
    uint32_t num_phase2;
    struct {
      float x[3],y[3],z[3],vx,vy,vz,s;
      struct hsv3 col;
      uint32_t base_frame, delay;
      float gl_base, gl_period, gl_amp;
    } p1[10];
    struct {
      float x,y,z,vx,vy,vz,hue;
      struct hsv3 col;
      uint32_t base_frame, delay;
      float fade_factor;
    } p2[300];
  } fireworks;

  struct st_migrating_dots {
    struct {
      float x,y,z,v, hue, sat, val, new_hue, new_sat, new_val;
      int target, delay;
    } dots[MIG_SIDE*MIG_SIDE];
    /* 0/1 is bottom/top, 2/3 is left/right, 4/5 is front/back. */
    int32_t start_plane, end_plane;
    uint32_t base_frame;
    uint32_t wait;
    uint32_t stage1;
    int text_idx;
  } migrating_dots[3];
};

struct ledtorus_anim;


/*
  Compute rectangular coordinates in the horizontal plane, taking into account
  the offset of the innermost LEDs from the center.
*/
struct torus_xz torus_polar2rect(float x, float a)
{
  struct torus_xz res;
  float angle = a * (F_PI*2.0f/(float)LEDS_TANG);
  res.x = (x+2.58f)*cosf(angle);
  res.z = (x+2.58f)*sinf(angle);
  return res;
}


struct torus_xz torus2_polar2rect(float x, float a)
{
  struct torus_xz res;
  float angle = a * (F_PI*2.0f/(float)LEDS_TANG);
  res.x = (x+TORUS2_LED2CENTER)*cosf(angle);
  res.z = (x+TORUS2_LED2CENTER)*sinf(angle);
  return res;
}


/* Random integer 0 <= x < N. */
static int
irand(int n)
{
  return rand() / (RAND_MAX/n+1);
}

/* Random float 0 <= x <= N. */
static float
drand(float n)
{
  return (float)rand() / ((float)RAND_MAX/n);
}

/* Random unit vector of length a, uniform distribution in angular space. */
static void
vrand(float a, float *x, float *y, float *z)
{
  /*
    Sample a random direction uniformly.

    Uses the fact that cylinder projection of the sphere is area preserving,
    so sample uniformly the cylinder, and project onto the sphere.
  */
  float v = drand(2.0f*F_PI);
  float u = drand(2.0f) - 1.0f;
  float r = sqrtf(1.0f - u*u);
  *x = a*r*cosf(v);
  *y = a*r*sinf(v);
  *z = a*u;
}


static struct colour3
hsv2rgb_f(float h, float s, float v)
{
  /* From Wikipedia: https://en.wikipedia.org/wiki/HSL_and_HSV */
  struct colour3 x;
  float c, m, r, g, b;

  c = v * s;
  h *= 6.0f;
  if (h < 1.0f)
  {
    r = c;
    g = c*h;
    b = 0.0f;
  }
  else if (h < 2.0f)
  {
    r = c*(2.0f - h);
    g = c;
    b = 0.0f;
  }
  else if (h < 3.0f)
  {
    r = 0.0f;
    g = c;
    b = c*(h - 2.0f);
  }
  else if (h < 4.0f)
  {
    r = 0.0f;
    g = c*(4.0f - h);
    b = c;
  }
  else if (h < 5.0f)
  {
    r = c*(h - 4.0f);
    g = 0.0f;
    b = c;
  }
  else /* h < 6.0f */
  {
    r = c;
    g = 0.0f;
    b = c*(6.0f - h);
  }
  m = v - c;
  /* Let's try to avoid rounding errors causing under/overflow. */
  x.r = (uint8_t)(0.1f + 255.8f*(r + m));
  x.g = (uint8_t)(0.1f + 255.8f*(g + m));
  x.b = (uint8_t)(0.1f + 255.8f*(b + m));
  return x;
}


void
cls(frame_t *f)
{
  memset(f, 0, sizeof(frame_t));
}


static const uint8_t tonc_font[8*96] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x18, 0x00,
  0x6c, 0x6c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x6c, 0x6c, 0xfe, 0x6c, 0xfe, 0x6c, 0x6c, 0x00,
  0x18, 0x3e, 0x60, 0x3c, 0x06, 0x7c, 0x18, 0x00,
  0x00, 0x66, 0xac, 0xd8, 0x36, 0x6a, 0xcc, 0x00,
  0x38, 0x6c, 0x68, 0x76, 0xdc, 0xce, 0x7b, 0x00,
  0x18, 0x18, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x0c, 0x18, 0x30, 0x30, 0x30, 0x18, 0x0c, 0x00,
  0x30, 0x18, 0x0c, 0x0c, 0x0c, 0x18, 0x30, 0x00,
  0x00, 0x66, 0x3c, 0xff, 0x3c, 0x66, 0x00, 0x00,
  0x00, 0x18, 0x18, 0x7e, 0x18, 0x18, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x30,
  0x00, 0x00, 0x00, 0x7e, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00,
  0x03, 0x06, 0x0c, 0x18, 0x30, 0x60, 0xc0, 0x00,
  0x3c, 0x66, 0x6e, 0x7e, 0x76, 0x66, 0x3c, 0x00,
  0x18, 0x38, 0x78, 0x18, 0x18, 0x18, 0x18, 0x00,
  0x3c, 0x66, 0x06, 0x0c, 0x18, 0x30, 0x7e, 0x00,
  0x3c, 0x66, 0x06, 0x1c, 0x06, 0x66, 0x3c, 0x00,
  0x1c, 0x3c, 0x6c, 0xcc, 0xfe, 0x0c, 0x0c, 0x00,
  0x7e, 0x60, 0x7c, 0x06, 0x06, 0x66, 0x3c, 0x00,
  0x1c, 0x30, 0x60, 0x7c, 0x66, 0x66, 0x3c, 0x00,
  0x7e, 0x06, 0x06, 0x0c, 0x18, 0x18, 0x18, 0x00,
  0x3c, 0x66, 0x66, 0x3c, 0x66, 0x66, 0x3c, 0x00,
  0x3c, 0x66, 0x66, 0x3e, 0x06, 0x0c, 0x38, 0x00,
  0x00, 0x18, 0x18, 0x00, 0x00, 0x18, 0x18, 0x00,
  0x00, 0x18, 0x18, 0x00, 0x00, 0x18, 0x18, 0x30,
  0x00, 0x06, 0x18, 0x60, 0x18, 0x06, 0x00, 0x00,
  0x00, 0x00, 0x7e, 0x00, 0x7e, 0x00, 0x00, 0x00,
  0x00, 0x60, 0x18, 0x06, 0x18, 0x60, 0x00, 0x00,
  0x3c, 0x66, 0x06, 0x0c, 0x18, 0x00, 0x18, 0x00,
  0x3c, 0x66, 0x5a, 0x5a, 0x5e, 0x60, 0x3c, 0x00,
  0x3c, 0x66, 0x66, 0x7e, 0x66, 0x66, 0x66, 0x00,
  0x7c, 0x66, 0x66, 0x7c, 0x66, 0x66, 0x7c, 0x00,
  0x1e, 0x30, 0x60, 0x60, 0x60, 0x30, 0x1e, 0x00,
  0x78, 0x6c, 0x66, 0x66, 0x66, 0x6c, 0x78, 0x00,
  0x7e, 0x60, 0x60, 0x78, 0x60, 0x60, 0x7e, 0x00,
  0x7e, 0x60, 0x60, 0x78, 0x60, 0x60, 0x60, 0x00,
  0x3c, 0x66, 0x60, 0x6e, 0x66, 0x66, 0x3e, 0x00,
  0x66, 0x66, 0x66, 0x7e, 0x66, 0x66, 0x66, 0x00,
  0x3c, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3c, 0x00,
  0x06, 0x06, 0x06, 0x06, 0x06, 0x66, 0x3c, 0x00,
  0xc6, 0xcc, 0xd8, 0xf0, 0xd8, 0xcc, 0xc6, 0x00,
  0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x7e, 0x00,
  0xc6, 0xee, 0xfe, 0xd6, 0xc6, 0xc6, 0xc6, 0x00,
  0xc6, 0xe6, 0xf6, 0xde, 0xce, 0xc6, 0xc6, 0x00,
  0x3c, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3c, 0x00,
  0x7c, 0x66, 0x66, 0x7c, 0x60, 0x60, 0x60, 0x00,
  0x78, 0xcc, 0xcc, 0xcc, 0xcc, 0xdc, 0x7e, 0x00,
  0x7c, 0x66, 0x66, 0x7c, 0x6c, 0x66, 0x66, 0x00,
  0x3c, 0x66, 0x70, 0x3c, 0x0e, 0x66, 0x3c, 0x00,
  0x7e, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00,
  0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3c, 0x00,
  0x66, 0x66, 0x66, 0x66, 0x3c, 0x3c, 0x18, 0x00,
  0xc6, 0xc6, 0xc6, 0xd6, 0xfe, 0xee, 0xc6, 0x00,
  0xc3, 0x66, 0x3c, 0x18, 0x3c, 0x66, 0xc3, 0x00,
  0xc3, 0x66, 0x3c, 0x18, 0x18, 0x18, 0x18, 0x00,
  0xfe, 0x0c, 0x18, 0x30, 0x60, 0xc0, 0xfe, 0x00,
  0x3c, 0x30, 0x30, 0x30, 0x30, 0x30, 0x3c, 0x00,
  0xc0, 0x60, 0x30, 0x18, 0x0c, 0x06, 0x03, 0x00,
  0x3c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x3c, 0x00,
  0x18, 0x3c, 0x66, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0x00,
  0x18, 0x18, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x3c, 0x06, 0x3e, 0x66, 0x3e, 0x00,
  0x60, 0x60, 0x7c, 0x66, 0x66, 0x66, 0x7c, 0x00,
  0x00, 0x00, 0x3c, 0x60, 0x60, 0x60, 0x3c, 0x00,
  0x06, 0x06, 0x3e, 0x66, 0x66, 0x66, 0x3e, 0x00,
  0x00, 0x00, 0x3c, 0x66, 0x7e, 0x60, 0x3c, 0x00,
  0x1c, 0x30, 0x7c, 0x30, 0x30, 0x30, 0x30, 0x00,
  0x00, 0x00, 0x3e, 0x66, 0x66, 0x3e, 0x06, 0x3c,
  0x60, 0x60, 0x7c, 0x66, 0x66, 0x66, 0x66, 0x00,
  0x18, 0x00, 0x18, 0x18, 0x18, 0x18, 0x0c, 0x00,
  0x0c, 0x00, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x78,
  0x60, 0x60, 0x66, 0x6c, 0x78, 0x6c, 0x66, 0x00,
  0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x0c, 0x00,
  0x00, 0x00, 0xec, 0xfe, 0xd6, 0xc6, 0xc6, 0x00,
  0x00, 0x00, 0x7c, 0x66, 0x66, 0x66, 0x66, 0x00,
  0x00, 0x00, 0x3c, 0x66, 0x66, 0x66, 0x3c, 0x00,
  0x00, 0x00, 0x7c, 0x66, 0x66, 0x7c, 0x60, 0x60,
  0x00, 0x00, 0x3e, 0x66, 0x66, 0x3e, 0x06, 0x06,
  0x00, 0x00, 0x7c, 0x66, 0x60, 0x60, 0x60, 0x00,
  0x00, 0x00, 0x3c, 0x60, 0x3c, 0x06, 0x7c, 0x00,
  0x30, 0x30, 0x7c, 0x30, 0x30, 0x30, 0x1c, 0x00,
  0x00, 0x00, 0x66, 0x66, 0x66, 0x66, 0x3e, 0x00,
  0x00, 0x00, 0x66, 0x66, 0x66, 0x3c, 0x18, 0x00,
  0x00, 0x00, 0xc6, 0xc6, 0xd6, 0xfe, 0x6c, 0x00,
  0x00, 0x00, 0xc6, 0x6c, 0x38, 0x6c, 0xc6, 0x00,
  0x00, 0x00, 0x66, 0x66, 0x66, 0x3c, 0x18, 0x30,
  0x00, 0x00, 0x7e, 0x0c, 0x18, 0x30, 0x7e, 0x00,
  0x0c, 0x18, 0x18, 0x30, 0x18, 0x18, 0x0c, 0x00,
  0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00,
  0x30, 0x18, 0x18, 0x0c, 0x18, 0x18, 0x30, 0x00,
  0x00, 0x76, 0xdc, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};


__attribute__((unused))
static uint32_t
g_text(frame_t *f, const char *text, uint32_t x, uint32_t a,
       uint8_t c_r, uint8_t c_g, uint8_t c_b, float s_fact)
{
  uint8_t ch;
  float sofar;

  if (s_fact < 0.01f)
    return a;
  /* ToDo: support s_fact < 1. Requires skipping pixels, or merging them. */
  if (s_fact < 1.0f)
    s_fact = 1.0f;
  s_fact = 1.0f / s_fact;
  sofar = s_fact * 0.5f;

  while ((ch = *text++))
  {
    const uint8_t *font_p;
    uint8_t bit_pos;

    if (ch < 32 || ch > 127)
      ch = '?';
    font_p = &tonc_font[8*ch-8*' '];

    bit_pos = 0x80;
    while (bit_pos)
    {
      uint32_t y;
#if LEDS_Y < 8
#error g_text() requires at least 8 pixes in LEDS_Y direction.
#endif
      for (y = 0; y < 8; ++y)
      {
        if (font_p[y] & bit_pos)
          setpix(f, x, y, a, c_r, c_g, c_b);
      }

      if (a == 0)
        a = LEDS_TANG-1;
      else
        --a;
      sofar += s_fact;
      if (sofar >= 1.0f)
      {
        sofar -= 1.0f;
        bit_pos >>= 1;
      }
    }
  }

  return a;
}


void
envelope(frame_t *f, uint32_t c)
{
  uint32_t a, i;
  uint32_t c2;
  float fact;
  uint8_t c_b;

  c2 = c % 128;
  if (c2 < 32)
    fact = sinf((float)c2 * (F_PI/32.0f));
  else
    fact = 0.0f;
  c_b = (uint32_t)(17.0f + 30.0f*fact);

  cls(f);
  for (a = 0; a < LEDS_TANG; ++a)
  {
    for (i = 0; i < 16; ++i)
    {
      int x0;
      /* Inner edge. */
      x0 = (i < LEDS_Y/2 ? (LEDS_Y/2-1)-i : i-LEDS_Y/2);
      if (x0 >= 3)
        x0 -= 2;
      else if (x0 >= 1)
        x0 -= 1;
      setpix(f, x0, i, a, 0, 0, c_b);
      /* Outer edge. */
      setpix(f, 13, i, a, 0, 0, c_b);
      /* Bottom and top. */
      if (i < 7) {
        setpix(f, 12-i, 0, a, 0, 0, c_b);
        setpix(f, 12-i, 15, a, 0, 0, c_b);
      }
    }
  }
}


static void
an_ghost(frame_t *f, uint32_t c, void *st __attribute__((unused)))
{
  uint32_t a, x;
  float ph;
  uint32_t skip;

  ph = (float)c * 0.19f;
  skip = (c % 128) < 64;

  cls(f);
  envelope(f, c);
  for (x = 0; x < LEDS_X; ++x)
  {
    float w = ((float)x+4.7) * 0.24f + ph;
    float y = ((float)LEDS_Y / 2.0f) * (1.0f + cosf(w));
    int32_t i_y = y;

    for (a = 0; a < LEDS_TANG; ++a)
    {
      if (skip && (a % 8))
        continue;
      // ToDo: try some anti-aliasing?
      if (i_y >= 0 && i_y < LEDS_Y)
      {
        struct colour3 col;
        col = hsv2rgb_f((float)a*(1.0f/(float)LEDS_TANG), 0.9f, 0.9f);
        setpix(f, x, i_y, a, col.r, col.g, col.b);
      }
    }
  }
}


static void
an_graphs(frame_t *f, uint32_t c, void *st __attribute__((unused)))
{
  const float scale_x = .01f;
  const float scale_a = .01f;
  const float scale_t = 0.0063f;
  const float oct2_freq = 2.0f;
  const float oct3_freq = 4.0f;
  const float oct2_scale = 0.6f;
  const float oct3_scale = 0.6f*0.6f;
  const int32_t skip = 2;
  const int32_t antialias = 1;
  int32_t a, x;

  cls(f);
  //envelope(f, c);
  for (a = 0; a < LEDS_TANG; a += skip) {
    for (x = 0; x < LEDS_X; ++x) {
      struct torus_xz rect = torus2_polar2rect(x, a);
      float mx = scale_x*rect.x;
      float my = scale_a*rect.z;
      float mz = scale_t*c;
      float v = simplex_noise_3d(mx, my, mz) +
        oct2_scale*simplex_noise_3d(oct2_freq*mx+100.0f, oct2_freq*my+100.0f, mz) +
        oct3_scale*simplex_noise_3d(oct3_freq*mx+200.0f, oct3_freq*my+200.0f, mz);
      struct colour3 col;
      float y, hue, dummy;
      hue = modff(.87f+.42f*(v+1.0f), &dummy);
      y = .5f*LEDS_Y + .5f*LEDS_Y*(.92f*v);
      if (y < 0.0f)
        y = 0.0f;
      else if (y > LEDS_Y-1)
        y = LEDS_Y-1;
      if (!antialias) {
        col = hsv2rgb_f(hue, 0.9f, 0.9f);
        setpix(f, x, y, a, col.r, col.g, col.b);
      } else {
        const float gamma = 2.3f;
        float delta = modff(y, &dummy);
        uint32_t y_int = y;
        if (y_int < LEDS_Y-1) {
          col = hsv2rgb_f(hue, 0.9f, powf(delta, gamma));
          setpix(f, x, y_int+1, a, col.r, col.g, col.b);
        }
        col = hsv2rgb_f(hue, 0.9f, powf(1.0f-delta, gamma));
        setpix(f, x, y_int, a, col.r, col.g, col.b);
      }
    }
  }
}


/*******************************************************************************
 *
 * Fireworks animation.
 *
 * (Ported from the corresponding LED-cube animation).
 *
 ******************************************************************************/

static void
ut_fireworks_shiftem(struct st_fireworks *c, uint32_t i)
{
  uint32_t j;

  for (j = sizeof(c->p1[0].x)/sizeof(c->p1[0].x[0]) - 1; j > 0; --j)
  {
    c->p1[i].x[j] = c->p1[i].x[j-1];
    c->p1[i].y[j] = c->p1[i].y[j-1];
    c->p1[i].z[j] = c->p1[i].z[j-1];
  }
}


static void
ut_fireworks_setpix(frame_t *f, float xf, float yf, float zf, struct hsv3 col)
{
  int x = roundf(xf);
  int y = roundf(yf*tang_factor);
  int z = roundf(zf);
  if (y < 0)
    y += LEDS_TANG;
  else if (y >= LEDS_TANG)
    y -= LEDS_TANG;
  if (x >= 0 && x < LEDS_X && y >= 0 && y < LEDS_TANG && z >= 0 && z < LEDS_Y)
  {
    struct colour3 rgb =
      hsv2rgb_f((float)col.h/255.0f, (float)col.s/255.0f, (float)col.v/255.0f);
    setpix(f, x, (LEDS_Y-1)-z, y, rgb.r, rgb.g, rgb.b);
  }
}


static uint32_t
in_fireworks(const struct ledtorus_anim *self __attribute__((unused)),
             union anim_data *data)
{
  struct st_fireworks *c = &data->fireworks;
  c->num_phase1 = 0;
  c->num_phase2 = 0;
  return 0;
}


static uint32_t
an_fireworks(frame_t *f, uint32_t frame, union anim_data *data)
{
  uint32_t i, j;

  struct st_fireworks *c= &data->fireworks;

  static const uint32_t max_phase1 = sizeof(c->p1)/sizeof(c->p1[0]);
  static const uint32_t max_phase2 = sizeof(c->p2)/sizeof(c->p2[0]);
  static const float g = 0.045f;
  static const int new_freq = 14;
  static const float min_height = 8.0f;
  static const float max_height = 15.0f;
  static const uint32_t min_start_delay = 32;
  static const uint32_t max_start_delay = 67;
  static const uint32_t min_end_delay = 50;
  static const uint32_t max_end_delay = 100;
  const float V = 0.5f;
  static const float resist = 0.06f;
  static const float min_fade_factor = 0.22f/15.0f;
  static const float max_fade_factor = 0.27f/15.0f;

  /* Start a new one occasionally. */
  if (c->num_phase1 == 0 || (c->num_phase1 < max_phase1 && irand(new_freq) == 0))
  {
    i = c->num_phase1++;

    c->p1[i].x[0] = 4.0f + drand(7.0f);
    c->p1[i].y[0] = drand((float)LEDS_TANG/tang_factor);
    c->p1[i].z[0] = 0.0f;
    for (j = 0; j < sizeof(c->p1[0].x)/sizeof(c->p1[0].x[0]) - 1; ++j)
      ut_fireworks_shiftem(c, i);

    c->p1[i].vx = drand(0.35f) - 0.175f;
    c->p1[i].vy = drand(0.35f/tang_factor) - 0.175f/tang_factor;
    c->p1[i].s = min_height + drand(max_height - min_height);
    c->p1[i].vz = sqrtf(2*g*c->p1[i].s);
    c->p1[i].col = mk_hsv3_f(0.8f, 0.0f, 0.5f);
    c->p1[i].base_frame = frame;
    c->p1[i].delay = min_start_delay + irand(max_start_delay - min_start_delay);
    c->p1[i].gl_base = frame;
    c->p1[i].gl_period = 0;
  }

  for (i = 0; i < c->num_phase1; )
  {
    uint32_t d = frame - c->p1[i].base_frame;
    if (d < c->p1[i].delay)
    {
      /* Waiting for launch - make fuse glow effect. */
      uint32_t gl_delta = frame - c->p1[i].gl_base;
      if (gl_delta >= c->p1[i].gl_period)
      {
        c->p1[i].gl_base = frame;
        c->p1[i].gl_period = 8 + irand(6);
        c->p1[i].gl_amp = 0.7f + drand(0.3f);
        gl_delta = 0;
      }
      float glow = c->p1[i].gl_amp*sin((float)gl_delta/c->p1[i].gl_period*F_PI);
      c->p1[i].col = mk_hsv3_f(0.8f, 0.0f, 0.44f + 0.31f*glow);
      ++i;
    }
    else if (c->p1[i].z[0] > c->p1[i].s)
    {
      /* Kaboom! */
      /* Delete this one, and create a bunch of phase2 ones (if room). */
      int k = 10 + irand(20);
      float common_hue = drand(6.5f);
      while (k-- > 0)
      {
        if (c->num_phase2 >= max_phase2)
          break;            /* No more room */
        j = c->num_phase2++;

        /* Sample a random direction uniformly. */
        float vx;
        float vy;
        float vz;
        vrand(V, &vx, &vy, &vz);

        c->p2[j].x = c->p1[i].x[0];
        c->p2[j].y = c->p1[i].y[0];
        c->p2[j].z = c->p1[i].z[0];
        c->p2[j].vx = c->p1[i].vx + vx;
        c->p2[j].vy = c->p1[i].vy + vy;
        c->p2[j].vz = c->p1[i].vz + vz;
        c->p2[j].hue = common_hue < 6.0f? common_hue : drand(6.0f);
        c->p2[j].col = mk_hsv3_f(c->p2[j].hue, 0.85f, 1.0f);
        c->p2[j].base_frame = frame;
        c->p2[j].delay = min_end_delay + irand(max_end_delay - min_end_delay);
        c->p2[j].fade_factor =
          min_fade_factor + drand(max_fade_factor - min_fade_factor);
      }
      c->p1[i] = c->p1[--c->num_phase1];
    }
    else
    {
      ut_fireworks_shiftem(c, i);
      c->p1[i].col = mk_hsv3_f(0.8f, 0.0f, 0.75f);
      c->p1[i].x[0] += c->p1[i].vx;
      c->p1[i].y[0] += c->p1[i].vy;
      c->p1[i].z[0] += c->p1[i].vz;
      c->p1[i].vz -= g;
      ++i;
    }
  }

  for (i = 0; i < c->num_phase2;)
  {
    c->p2[i].x += c->p2[i].vx;
    c->p2[i].y += c->p2[i].vy;
    c->p2[i].z += c->p2[i].vz;

    c->p2[i].vx -= resist*c->p2[i].vx;
    c->p2[i].vy -= resist*c->p2[i].vy;
    c->p2[i].vz -= resist*c->p2[i].vz + g;

    float value = 1.0f - c->p2[i].fade_factor*(frame - c->p2[i].base_frame);
    if (value < 0.0f)
      value = 0.0f;
    c->p2[i].col = mk_hsv3_f(c->p2[i].hue, 0.85f, value);

    if (c->p2[i].z <= 0.0f)
    {
      c->p2[i].z = 0.0f;
      if (c->p2[i].delay-- == 0 || value <= 0.05f)
      {
        /* Delete it. */
        c->p2[i] = c->p2[--c->num_phase2];
      }
      else
        ++i;
    }
    else
      ++i;
  }

  cls(f);
  /* Mark out the "ground". */
  for (i = 0; i < LEDS_TANG; ++i)
    for (j = 5; j < LEDS_X; ++j)
      setpix(f, j, LEDS_Y-1, i, 0, 0, 17);

  /*
    Draw stage2 first, so we don't overwrite a new rocket with an old, dark
    ember.
  */
  for (i = 0; i < c->num_phase2; ++i)
    ut_fireworks_setpix(f, c->p2[i].x, c->p2[i].y, c->p2[i].z, c->p2[i].col);
  for (i = 0; i < c->num_phase1; ++i)
  {
    for (j = 0; j < sizeof(c->p1[0].x)/sizeof(c->p1[0].x[0]); ++j)
      ut_fireworks_setpix(f, c->p1[i].x[j], c->p1[i].y[j], c->p1[i].z[j], c->p1[i].col);
  }

  return 0;
}


static uint32_t
an_test1(frame_t *f, uint32_t frame, union anim_data *data)
{
  cls(f);
  envelope(f, frame);
  return 0;
}


static uint32_t
an_test2(frame_t *f, uint32_t frame, union anim_data *data)
{
  int i;

  cls(f);
  for (i = 0; i < LEDS_TANG; ++i)
    setpix(f, LEDS_X/2, LEDS_Y/2, i, i, i, i);

  return 0;
}


__attribute__((unused))
static uint32_t
an_test3(frame_t *f, uint32_t frame, union anim_data *data)
{
  int i;
  uint8_t r, g, b;

  cls(f);
  r = 255;
  g = 0;
  b = 0;
  for (i = 0; i < 12; i += 3) {
    uint8_t tmp;

    setpix(f, LEDS_X-1, 1*LEDS_Y/4, i, r, g, b);
    setpix(f, LEDS_X-1, 2*LEDS_Y/4, i, b, r, g);
    setpix(f, LEDS_X-1, 3*LEDS_Y/4, i, g, b, r);
    tmp = b; b = g; g = r; r = tmp;
  }

  return 0;
}


int
main(int argc, char *argv[])
{
  uint32_t n;
  frame_t frame;
  static union anim_data private_data;

  for (n = 0; n < 5000; ++n)
  {
    uint8_t buf[512];
    size_t len = sizeof(frame_t);

    switch (argc > 1 ? atoi(argv[1]) : 0)
    {
    case 0:
      an_ghost(&frame, n, NULL);
      break;
    case 1:
      an_test1(&frame, n, NULL);
      break;
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    {
      if (n == 0)
        in_fireworks(NULL, &private_data);
      an_fireworks(&frame, n, &private_data);
      break;
    }
    case 13:
      an_graphs(&frame, n, NULL);
      break;
    case 8:
    case 9:
    case 10:
    case 11:
    case 12:
    default:
      an_test2(&frame, n, NULL);
      break;
    }
    write(1, frame, len);
    if (len % 512)
    {
      memset(buf, 0, 512 - (len % 512));
      write(1, buf, 512 - (len % 512));
    }
  }
  return 0;
}
