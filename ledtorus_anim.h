#ifndef LEDTORUS_ANIM_H
#define LEDTORUS_ANIM_H

#include <stdint.h>

#define LEDS_X 7
#define LEDS_Y 8
#define LEDS_TANG 205
typedef uint8_t frame_t[LEDS_Y*LEDS_X*LEDS_TANG][3];

#define F_PI 3.141592654f

struct torus_xz { float x, z; };


static inline void
setpix(frame_t *f, uint32_t x, uint32_t y, uint32_t a,
       uint8_t r, uint8_t g, uint8_t b)
{
  uint8_t *p = (*f)[y+x*LEDS_Y+a*(LEDS_Y*LEDS_X)];
  p[0] = r;
  p[1] = g;
  p[2] = b;
}


extern struct torus_xz torus_polar2rect(float x, float a);
extern void cls(frame_t *f);
extern void envelope(frame_t *f, uint32_t c);

#endif  /* LEDTORUS_ANIM_H */
