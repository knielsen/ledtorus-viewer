/*
  gcc -Wall ledtorus_anim.c simplex_noise.c colours.c -o ledtorus_anim -lm
*/

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <unistd.h>

#include "simplex_noise.h"
#include "colours.h"


#define LEDS_X 7
#define LEDS_Y 8
#define LEDS_TANG 205
typedef uint8_t frame_t[LEDS_Y*LEDS_X*LEDS_TANG][3];

#define F_PI 3.141592654f

/*
   A factor to compensate that pixels are much smaller in the tangential
   direction than in the raial and vertical direction.
*/
static const float tang_factor = 5.0f;

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

struct torus_xz { float x, z; };


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
      float x,y,z,v;
      int target, delay, col, new_col;
    } dots[MIG_SIDE*MIG_SIDE];
    /* 0/1 is bottom/top, 2/3 is left/right, 4/5 is front/back. */
    int start_plane, end_plane;
    int base_frame;
    int wait;
    int stage1;
    int text_idx;
  } migrating_dots;
};


/*
  Compute rectangular coordinates in the horizontal plane, taking into account
  the offset of the innermost LEDs from the center.
*/
static struct torus_xz torus_polar2rect(float x, float a)
{
  struct torus_xz res;
  float angle = a * (F_PI*2.0f/(float)LEDS_TANG);
  res.x = (x+2.58f)*cosf(angle);
  res.z = (x+2.58f)*sinf(angle);
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


static inline void
setpix(frame_t *f, uint32_t x, uint32_t y, uint32_t a,
       uint8_t r, uint8_t g, uint8_t b)
{
  uint8_t *p = (*f)[y+x*LEDS_Y+a*(LEDS_Y*LEDS_X)];
  p[0] = r;
  p[1] = g;
  p[2] = b;
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


static void
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


static void
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

  for (a = 0; a < LEDS_TANG; ++a)
  {
    setpix(f, 1, 1, a, 0, 0, c_b);
    setpix(f, 1, 6, a, 0, 0, c_b);
    setpix(f, 6, 1, a, 0, 0, c_b);
    setpix(f, 6, 6, a, 0, 0, c_b);
    for (i = 0; i < 4; ++i)
    {
      setpix(f, 0, i+2, a, 0, 0, c_b);
      setpix(f, 6, i+2, a, 0, 0, c_b);
      setpix(f, i+2, 0, a, 0, 0, c_b);
      setpix(f, i+2, 7, a, 0, 0, c_b);
    }
  }
}


static void
an_ghost(frame_t *f, uint32_t c, void *st __attribute__((unused)))
{
  uint32_t a, x;
  float ph;
  uint32_t skip;

  ph = (float)c * 0.29f;
  skip = (c % 128) < 64;

  cls(f);
  envelope(f, c);
  for (x = 0; x < LEDS_X; ++x)
  {
    float w = (float)x * 0.31f + ph;
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
an_test(frame_t *f, uint32_t c, void *st __attribute__((unused)))
{
  uint32_t x, y, a;
  uint8_t c_r = ((c+5) & 1) ? 255 : 0;
  uint8_t c_g = ((c+5) & 2) ? 255 : 0;
  uint8_t c_b = ((c+5) & 4) ? 255 : 0;

  for (a = 0; a < LEDS_TANG; ++a)
  {
    for (y = 0; y < LEDS_Y; ++y)
    {
      for (x = 0; x < LEDS_X; ++x)
      {
        setpix(f, x, y, a, c_r, c_g, c_b);
      }
    }
  }
}


static void
an_test2(frame_t *f, uint32_t c, void *st __attribute__((unused)))
{
  uint32_t x, y, a;

  cls(f);
  for (a = 0; a < LEDS_TANG; ++a)
  {
    for (y = 0; y < LEDS_Y; ++y)
    {
      for (x = 5; x <= 5; ++x)
      {
        if ((a/(LEDS_TANG/4)) % 2)
          setpix(f, x, y, a, 255, 255, 255);
        else
          setpix(f, x, y, a, 255, 0, 0);
      }
    }
  }
}


static char *
my_str_mk(char *dst, const char *src)
{
  while ((*dst= *src++))
    ++dst;
  return dst;
}

static float
voltage_read(void)
{
  return 3.985f;
}


static void
float_to_str(char *buf, float x, uint32_t dig_before, uint32_t dig_after)
{
  sprintf(buf, "%*.*f", dig_before+dig_after+1, dig_after, (double)x);
}

static void
an_supply_voltage(frame_t *f, uint32_t c, void *st __attribute__((unused)))
{
  char buf[50];
  static float voltage = 0.0f;
  char *p;
  float stretch;
  uint32_t a, c2;

  cls(f);
  if (voltage == 0.0f || (c%64) == 0)
    voltage = voltage_read();
  p = my_str_mk(buf, "Supply: ");
  float_to_str(p, voltage, 1, 3);
  p = my_str_mk(p+strlen(p), "V");
  c2 = c % 270;
  if (c2 < 72)
    stretch = 1.4f - 0.4f * cosf((float)c2*(2.0f*F_PI/72.0f));
  else
    stretch = 1.0f;
  a = (2*c)%LEDS_TANG;
  g_text(f, buf, 4, a, 255, 100, 20, stretch);
}


static void
an_simplex_noise1(frame_t *f, uint32_t c, void *st __attribute__((unused)))
{
  uint32_t x, y, a;

  cls(f);
  for (a = 0; a < LEDS_TANG; a += 4)
  {
    for (x = 0; x < LEDS_X; ++x)
    {
      for (y = 0; y < LEDS_Y; ++y)
      {
        float sn;
        float nx, ny, nz;

        nx = (((float)x+2.58f)*0.06f)*cosf((float)a * (F_PI*2.0f/(float)LEDS_TANG));
        nz = (((float)x+2.58f)*0.06f)*sinf((float)a * (F_PI*2.0f/(float)LEDS_TANG));
        ny = (float)y*0.06f;
        nx = nx + (float)c*0.02f;
        ny = ny + (float)c*0.007f;
        nz = nz + (float)c*0.005f;
        //sn = simplex_noise_3d((float)x*0.2f, (float)y*0.2f, (float)(a+5*c)*0.012f);
        sn = 0.5f*(1.0f+simplex_noise_3d(nx, ny, nz));
/*
        if (sn >= 0.0f && sn <= 1.0f)
        {
          struct colour3 col3;
          col3 = hsv2rgb_f(sn, 1.0f, 1.0f);
          setpix(f, x, y, a, col3.r, col3.g, col3.b);
        }
*/
        if (sn >= 0.4f)
        {
          float fi;
          uint32_t i;
          fi = (sn-0.3f)*(256/0.5f);
          if (fi < 0)
            i = 0;
          else if (fi > 255)
            i = 255;
          else
            i = (uint32_t)fi;
          setpix(f, x, y, a, colour_gradient_blue_green_gold[i][0],
                 colour_gradient_blue_green_gold[i][1],
                 colour_gradient_blue_green_gold[i][2]);
        }
      }
    }
  }
}


static void
an_simplex_noise2(frame_t *f, uint32_t c, void *st __attribute__((unused)))
{
  uint32_t x, y, a;

  cls(f);
  for (a = 0; a < LEDS_TANG; a += 1)
  {
    for (x = 0; x < LEDS_X; ++x)
    {
      for (y = 0; y < LEDS_Y; ++y)
      {
        float sn;
        float nx, ny, nz;

        nx = (((float)x+2.58f)*0.06f)*cosf((float)a * (F_PI*2.0f/(float)LEDS_TANG));
        nz = (((float)x+2.58f)*0.06f)*sinf((float)a * (F_PI*2.0f/(float)LEDS_TANG));
        ny = (float)y*0.06f;
        nx = nx + (float)c*0.02f;
        ny = ny + (float)c*0.007f;
        nz = nz + (float)c*0.005f;
        //sn = simplex_noise_3d((float)x*0.2f, (float)y*0.2f, (float)(a+5*c)*0.012f);
        sn = 0.5f*(1.0f+simplex_noise_3d(nx, ny, nz));
/*
        if (sn >= 0.0f && sn <= 1.0f)
        {
          struct colour3 col3;
          col3 = hsv2rgb_f(sn, 1.0f, 1.0f);
          setpix(f, x, y, a, col3.r, col3.g, col3.b);
        }
*/
        if (sn >= 0.4f)
        {
          float fi;
          uint32_t i;
          fi = (sn-0.4f)*(256/0.5f);
          if (fi < 0)
            i = 0;
          else if (fi > 255)
            i = 255;
          else
            i = (uint32_t)fi;
          setpix(f, x, y, a, colour_gradient_blue_green_gold[i][0],
                 colour_gradient_blue_green_gold[i][1],
                 colour_gradient_blue_green_gold[i][2]);
        }
      }
    }
  }
}


static void
an_simplex_noise3(frame_t *f, uint32_t c, void *st __attribute__((unused)))
{
  /* If tang_spacing > 1, then a dottet appearance results. */
  static const uint32_t tang_spacing = 1;
  /* Frequency of first octave. */
  static const float base_scale = 0.076f;
  /* Level below which voxel is invisible (interval [-1..1]). */
  static const float threshold = -0.2f;
  /* Factor to overshoot intensity, causing some saturation at either end. */
  static const float saturation_fact = 0.36f/0.6f;
  /* Relative frequencies of the different noise octaves. */
  static const float octaves_freq[] = {1.0f, 2.0f, 4.0f};
  /* Relative amplitude of the different noise octaves. */
  static const float octaves_ampl[] = {1.0f, 0.6f, 0.36f};
  uint32_t x, y, a, i;
  float px, py, pz;
  float octave_scaling;

  octave_scaling = 0.0f;
  for (i = 0; i < sizeof(octaves_ampl)/sizeof(octaves_ampl[0]); ++i)
    octave_scaling += octaves_ampl[i];
  octave_scaling = 1.0f/octave_scaling;

  /* Base position moving around in the virtual world of the simplex noise. */
  // ToDo: incremental movement in varying direction.
  px =(float)c*0.010f;
  py =(float)c*0.005f;
  pz =(float)c*0.004f;

  cls(f);

  for (a = 0; a < LEDS_TANG; a += tang_spacing)
  {
    for (x = 0; x < LEDS_X; ++x)
    {
      for (y = 0; y < LEDS_Y; ++y)
      {
        float sn;
        float nx, ny, nz;
        struct torus_xz rect_xz;

        rect_xz = torus_polar2rect((float)x, (float)a);
        nx = base_scale * rect_xz.x;
        ny = base_scale * (float)y;
        nz = base_scale * rect_xz.z;
        // ToDo: Some gentle rotation, eg A*c around X, B*c around Y or something.

        sn = 0.0f;
        for (i = 0; i < sizeof(octaves_freq)/sizeof(octaves_freq[0]); ++i)
        {
          float freq = octaves_freq[i];
          float amp = octaves_ampl[i];
          sn += amp*simplex_noise_3d(freq*nx + px, freq*ny + py, freq*nz + pz);
        }
        sn *= octave_scaling;

        if (sn >= threshold)
        {
          float fi;
          uint32_t i;
          fi = (sn-threshold)*(256/(saturation_fact*(1.0f-threshold)));
          if (fi < 0)
            i = 0;
          else if (fi > 255)
            i = 255;
          else
            i = (uint32_t)fi;
          setpix(f, x, y, a, colour_gradient_blue_green_gold[i][0],
                 colour_gradient_blue_green_gold[i][1],
                 colour_gradient_blue_green_gold[i][2]);
        }
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
in_fireworks(const struct ledtorus_anim *self, union anim_data *data)
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
  static const int new_freq = 25;
  static const float min_height = 4.0f;
  static const float max_height = 7.0f;
  static const uint32_t min_start_delay = 32;
  static const uint32_t max_start_delay = 67;
  static const uint32_t min_end_delay = 50;
  static const uint32_t max_end_delay = 100;
  const float V = 0.5f;
  static const float resist = 0.11f;
  static const float min_fade_factor = 0.22f/15.0f;
  static const float max_fade_factor = 0.27f/15.0f;

  /* Start a new one occasionally. */
  if (c->num_phase1 == 0 || (c->num_phase1 < max_phase1 && irand(new_freq) == 0))
  {
    i = c->num_phase1++;

    c->p1[i].x[0] = (float)(LEDS_X-1)/2.0f - 1.2f + drand(2.4f);
    c->p1[i].y[0] = drand((float)LEDS_TANG/tang_factor);
    c->p1[i].z[0] = 0.0f;
    for (j = 0; j < sizeof(c->p1[0].x)/sizeof(c->p1[0].x[0]) - 1; ++j)
      ut_fireworks_shiftem(c, i);

    c->p1[i].vx = drand(0.35f) - 0.175f;
    c->p1[i].vy = drand(0.35f/tang_factor) - 0.175f/tang_factor;
    c->p1[i].s = min_height + drand(max_height - min_height);
    c->p1[i].vz = sqrt(2*g*c->p1[i].s);
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
    for (j = 2; j < LEDS_X-2; ++j)
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


/* Migrating dots animation. */


static const int migrating_dots_col1 = 15;
static const int migrating_dots_col2 = 9;

static int
ut_migrating_dots_get_colour(struct st_migrating_dots *c, int idx,
                             const char *glyph, int glyph_width)
{
#if 1   /* ToDo */
  return migrating_dots_col1;
#else
  if (!glyph)
    return migrating_dots_col1;
  int x, y;
  switch(c->end_plane)
  {
  case 0:
  case 1:
    if (c->start_plane/2 == 1)
    {
      x = c->dots[idx].target;
      y = (MIG_SIDE-1) - c->dots[idx].y;
    }
    else
    {
      x = c->dots[idx].x;
      y = (MIG_SIDE-1) - c->dots[idx].target;
    }
    break;
  case 2:
    if (c->start_plane/2 == 0)
    {
      x = (MIG_SIDE-1) - c->dots[idx].y;
      y = (MIG_SIDE-1) - c->dots[idx].target;
    }
    else
    {
      x = (MIG_SIDE-1) - c->dots[idx].target;
      y = (MIG_SIDE-1) - c->dots[idx].z;
    }
    break;
  case 3:
    if (c->start_plane/2 == 0)
    {
      x = c->dots[idx].y;
      y = (MIG_SIDE-1) - c->dots[idx].target;
    }
    else
    {
      x = c->dots[idx].target;
      y = (MIG_SIDE-1) - c->dots[idx].z;
    }
    break;
  case 4:
  case 5:
    if (c->start_plane/2 == 0)
    {
      x = c->dots[idx].x;
      y = (MIG_SIDE-1) - c->dots[idx].target;
    }
    else
    {
      x = c->dots[idx].target;
      y = (MIG_SIDE-1) - c->dots[idx].z;
    }
    break;
  }
  x = x - (MIG_SIDE-glyph_width)/2;
  y = y - (MIG_SIDE-9)/2;
  int col;
  if (x < 0 || x >= glyph_width || y < 0 || y >= 9 ||
        glyph[x + glyph_width*y] == ' ')
    col = migrating_dots_col1;
  else
    col = migrating_dots_col2;
  return col;
#endif
}


static uint32_t
in_migrating_dots(const struct ledtorus_anim *self, union anim_data *data)
{
  struct st_migrating_dots *c = &data->migrating_dots;
  uint32_t i;

  c->end_plane = 1;         /* Top; we will copy this to start_plane below. */
  c->wait = 1000000;        /* Will trigger start of new round. */
  c->stage1 = 0;            /* Pretend that we are at the end of stage2. */
  c->text_idx = -5;
  for (i = 0; i < MIG_SIDE*MIG_SIDE; ++i)
    c->dots[i].new_col = migrating_dots_col1;

  return 0;
}


static uint32_t
an_migrating_dots(frame_t *f, uint32_t frame, union anim_data *data)
{
  static const int start_spread = 12;
  static const float v_min = 0.9;
  static const float v_range = 1.2;
  static const float grav = -0.07;
  static const int stage_pause = 8;
  static const char *text = "LABITAT";
  uint32_t i, j;

  struct st_migrating_dots *c = &data->migrating_dots;

  if (c->wait > stage_pause)
  {
    c->base_frame = frame;
    c->wait = 0;

    if (c->stage1)
    {
      /* Move to stage 2. */
      c->stage1 = 0;
      for (i = 0; i < MIG_SIDE*MIG_SIDE; ++i)
      {
        c->dots[i].delay = irand(start_spread);
        if (c->end_plane == 0)
          c->dots[i].v = 0;
        else if (c->end_plane == 1)
          c->dots[i].v = 2 + drand(v_range);
        else
          c->dots[i].v = (2*(c->end_plane%2)-1) * (v_min + drand(v_range));
        c->dots[i].target = (MIG_SIDE-1)*(c->end_plane%2);
      }
    }
    else
    {
      /* Start a new round. */
      c->stage1 = 1;

      /* We start where we last ended. */
      c->start_plane = c->end_plane;
      /*
        Choose a plane to move to, but not the same or the one directly
        opposite.
      */
      do
        c->end_plane = irand(6);
      while ((c->end_plane/2) == (c->start_plane/2));

      const char *glyph = c->text_idx >= 0 ? 0/* ToDo font9[text[c->text_idx]]*/ : NULL;
      ++c->text_idx;
      if (c->text_idx >= strlen(text))
        c->text_idx = -1;           /* -1 => One blank before we start over */
      int glyph_width = glyph ? strlen(glyph)/9 : 0;

      int idx = 0;
      for (i = 0; i < MIG_SIDE; ++i)
      {
        /*
          We will make a random permutation of each row of dots as they migrate
          to a different plane.
        */
        int permute[MIG_SIDE];
        for (j = 0; j < MIG_SIDE; ++j)
          permute[j] = j;
        int num_left = MIG_SIDE;
        for (j = 0; j < MIG_SIDE; ++j)
        {
          int k = irand(num_left);
          c->dots[idx].target = permute[k];
          permute[k] = permute[--num_left];
          int m = (MIG_SIDE-1)*(c->start_plane%2);
          switch (c->start_plane)
          {
          case 0: case 1:
            if (c->end_plane/2 == 1)
            {
              c->dots[idx].y = i;
              c->dots[idx].x = j;
            }
            else
            {
              c->dots[idx].x = i;
              c->dots[idx].y = j;
            }
            c->dots[idx].z = m;
            break;
          case 2: case 3:
            if (c->end_plane/2 == 0)
            {
              c->dots[idx].y = i;
              c->dots[idx].z = j;
            }
            else
            {
              c->dots[idx].z = i;
              c->dots[idx].y = j;
            }
            c->dots[idx].x = m;
            break;
          case 4: case 5:
            if (c->end_plane/2 == 0)
            {
              c->dots[idx].x = i;
              c->dots[idx].z = j;
            }
            else
            {
              c->dots[idx].z = i;
              c->dots[idx].x = j;
            }
            c->dots[idx].y = m;
            break;
          }
          c->dots[idx].delay = irand(start_spread);
          c->dots[idx].col = c->dots[idx].new_col;
          c->dots[idx].new_col =
            ut_migrating_dots_get_colour(c, idx, glyph, glyph_width);
          if (c->start_plane == 1)
            c->dots[idx].v = 0;
          else if (c->start_plane == 0)
            c->dots[idx].v = 2 + v_range;
          else
            c->dots[idx].v = (1-2*(c->start_plane%2)) * (v_min + drand(v_range));
          ++idx;
        }
      }
    }
  }

  int plane = c->stage1 ? c->start_plane : c->end_plane;
  int d = frame - c->base_frame;
  int moving = 0;
  for (i = 0; i < MIG_SIDE*MIG_SIDE; ++i)
  {
    if (d < c->dots[i].delay)
      continue;

    float *m;
    switch(plane)
    {
    case 0: case 1:
      m = &c->dots[i].z;
      break;
    case 2: case 3:
      m = &c->dots[i].x;
      break;
    case 4: case 5:
      m = &c->dots[i].y;
      break;
    }

    *m += c->dots[i].v;
    if ((plane % 2 != c->stage1 && *m >= c->dots[i].target) ||
        (plane % 2 == c->stage1 && *m <= c->dots[i].target))
    {
      *m = c->dots[i].target;
    }
    else
    {
      ++moving;
      if (plane <= 1)
        c->dots[i].v += grav;
    }
  }
  if (moving == 0)
    ++c->wait;

  /* Draw the lot. */
  cls(f);
  for (i = 0; i < MIG_SIDE*MIG_SIDE; ++i)
  {
    int x = round(c->dots[i].x);
    int y = round(c->dots[i].y*tang_factor);
    int z = round(c->dots[i].z);
    int col;
    if (c->stage1)
      col = c->dots[i].col;
    else if (d < c->dots[i].delay)
      col = c->dots[i].col + (c->dots[i].new_col - c->dots[i].col)*d/c->dots[i].delay;
    else
      col = c->dots[i].new_col;
    if (plane/2 == 1)
    {
      if (x >= LEDS_X)
        x = LEDS_X - 1;
      if (x == LEDS_X - 1 && (z == 0 || z == LEDS_Y-1))
        x = LEDS_X - 2;
      else if (x == 0 && (z == 1 || z == LEDS_Y-2))
        x = 1;
      else if (x <= 1 && (z == 0 || z == LEDS_Y-1))
        x = 2;
    }
    else if (plane/2 == 0)
    {
      if (z == 0 && (x == 1 || x == LEDS_X -1))
        z = 1;
      else if (z == LEDS_Y-1 && (x == 1 || x == LEDS_X -1))
        z = LEDS_Y-2;
      else if (x == 0 && z < 2)
        z = 2;
      else if (x == 0 && z > LEDS_Y-2)
        z = LEDS_Y-2;
    }

    if (x >= 0 && x < LEDS_X && z >= 0 && z < LEDS_Y &&
        y >= 0 && y < LEDS_TANG)
      setpix(f, x, (LEDS_Y-1)-z, y, col*15, col*15, col*15);
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
    uint16_t len = sizeof(frame_t);

    switch (argc > 1 ? atoi(argv[1]) : 0)
    {
    case 0:
      an_ghost(&frame, n, NULL);
      break;
    case 1:
      an_test(&frame, n, NULL);
      break;
    case 2:
      an_supply_voltage(&frame, n, NULL);
      break;
    case 3:
      an_simplex_noise1(&frame, n, NULL);
      break;
    case 4:
      an_simplex_noise2(&frame, n, NULL);
      break;
    case 5:
      an_simplex_noise3(&frame, n, NULL);
      break;
    case 7:
    {
      if (n == 0)
        in_fireworks(NULL, &private_data);
      an_fireworks(&frame, n, &private_data);
      break;
    }
    case 8:
    {
      if (n == 0)
        in_migrating_dots(NULL, &private_data);
      an_migrating_dots(&frame, n, &private_data);
      break;
    }
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
