#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "rubberduck.h"

uint32_t
rubberduck_init(struct st_rubberduck *c)
{
  const char *filename = "rubber-duck-10_samp.pcd";
  FILE *fp;
  char buf[1024];
  int in_header;
  int idx;

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
      sscanf(buf, "%f %f %f", &x, &y, &z);
      c->points[idx++] = x;
      c->points[idx++] = y;
      c->points[idx++] = z;
    }
  }
  fclose(fp);

  return 0;
}


uint32_t
rubberduck_anim_frame(frame_t *f, uint32_t frame, struct st_rubberduck *c)
{
  int i;

  // ToDo
  return 0;
}
