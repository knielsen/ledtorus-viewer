#include <stdint.h>
#include <math.h>

#include "simplex_noise.h"


/*
  Simplex noise, converted from the Java code in Ken Perlin's paper / chapter
  2 "Noise Hardware":

    http://www.csee.umbc.edu/~olano/s2002c36/ch02.pdf
*/

static const uint32_t noise_seeds[8] = {0x15, 0x38, 0x32, 0x2c, 0x0d, 0x13, 0x07, 0x2a};

static inline uint32_t
shuffle2(uint32_t N, uint32_t B)
{
  return (N>>B) & 1;
}


static uint32_t
shuffle4(uint32_t i, uint32_t j, uint32_t k, uint32_t B)
{
  return noise_seeds[(shuffle2(i,B)<<2) | (shuffle2(j,B)<<1) | shuffle2(k,B)];
}


static inline uint32_t
shuffle(uint32_t i, uint32_t j, uint32_t k)
{
  return shuffle4(i, j, k, 0) + shuffle4(j, k, i, 1) +
    shuffle4(k, i, j, 2) + shuffle4(i, j, k, 3) +
    shuffle4(j, k, i, 4) + shuffle4(k, i, j, 5) +
    shuffle4(i, j, k, 6) + shuffle4(j, k, i, 7);
}


static inline float
K(uint32_t a, uint32_t A[3], uint32_t i, uint32_t j, uint32_t k, float u, float v, float w) {
  float s = (float)(A[0]+A[1]+A[2])/6.f;
  float x = u-(float)A[0]+s;
  float y = v-(float)A[1]+s;
  float z = w-(float)A[2]+s;
  float t = .6f-x*x-y*y-z*z;
  uint32_t h = shuffle(i+A[0], j+A[1], k+A[2]);
  A[a]++;
  if (t < 0)
    return 0;
  uint32_t b5 = h>>5 & 1;
  uint32_t b4 = h>>4 & 1;
  uint32_t b3 = h>>3 & 1;
  uint32_t b2= h>>2 & 1;
  uint32_t b = h & 3;
  float p = (b==1 ? x : (b==2 ? y : z));
  float q = (b==1 ? y : (b==2 ? z : x));
  float r = (b==1 ? z : (b==2 ? x : y));
  p = (b5==b3 ? -p : p);
  q = (b5==b4 ? -q : q);
  r = (b5!=(b4^b3) ? -r : r);
  t *= t;
  return 25.0f * t * t * (p + (b==0 ? q+r : (b2==0 ? q : r)));
}


float
simplex_noise_3d(float x, float y, float z)
{
  float s, u, v, w;
  float i, j, k;
  uint32_t ix, jx, kx;
  uint32_t hi, lo;
  uint32_t A[3];
  s = (x+y+z)/3;
  i = floorf(x+s);
  j = floorf(y+s);
  k = floorf(z+s);
  s = (i+j+k)/6.f;
  u = x-i+s;
  v = y-j+s;
  w = z-k+s;
  A[0] = A[1] = A[2] = 0;
  hi = (u>=w ? (u>=v ? 0 : 1) : (v>=w ? 1 : 2));
  lo = (u<w ? (u<v ? 0 : 1) : (v<w ? 1 : 2));
  ix = (uint32_t)i;
  jx = (uint32_t)j;
  kx = (uint32_t)k;
  return K(hi, A, ix, jx, kx, u, v, w) +
    K(3-hi-lo, A, ix, jx, kx, u, v, w) +
    K(lo, A, ix, jx, kx, u, v, w) +
    K(0, A, ix, jx, kx, u, v, w);
}


/* This is kind of a misunderstanding. The output of the plain simplex
   function is already -1..1, it seems, just apparently very unlikely
   to go outside -.9...9 in practice?
*/
float
simplex_noise_3d_norm(float x, float y, float z)
{
  const float s_min = -0.906f;
  const float s_max = 0.658f;
  const float s_scale = 2.0f/(s_max-s_min);
  float v = simplex_noise_3d(x, y, z);
  return (v-(s_min+1/s_scale))*s_scale;
}
