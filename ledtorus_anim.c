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

struct colour3 {
  uint8_t r, g, b;
};

struct torus_xz { float x, z; };


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


int
main(int argc, char *argv[])
{
  uint32_t n;
  frame_t frame;

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
