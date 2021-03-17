#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "render3d.h"
#include "stanford_ply_loader.h"


static int
read_float(FILE *f, float *p, const char *file_name)
{
  union { uint32_t u ; float f; } pun;
  unsigned char buf[4];
  if (1 != fread(buf, 4, 1, f)) {
    fprintf(stderr, "EOF or error reading file '%s'.\n", file_name);
    return -1;
  }
  /* Little-endian conversion to float. */
  pun.u = (uint32_t) buf[0] |
    ((uint32_t) buf[1] << 8) |
    ((uint32_t) buf[2] << 16) |
    ((uint32_t) buf[3] << 24);
  if (!isfinite(pun.f)) {
    fprintf(stderr, "Invalid floating point number while reading file '%s'\n",
            file_name);
    return -1;
  }
  *p = pun.f;
  return 0;
}

static int
read_uchar(FILE *f, uint8_t *p, const char *file_name)
{
  unsigned char buf[1];
  if (1 != fread(buf, 1, 1, f)) {
    fprintf(stderr, "EOF or error reading file '%s'.\n", file_name);
    return -1;
  }
  *p = buf[0];
  return 0;
}

static int
read_uint(FILE *f, uint32_t *p, const char *file_name)
{
  unsigned char buf[4];
  if (1 != fread(buf, 4, 1, f)) {
    fprintf(stderr, "EOF or error reading file '%s'.\n", file_name);
    return -1;
  }
  /* Little-endian conversion. */
  *p = (uint32_t) buf[0] |
    ((uint32_t) buf[1] << 8) |
    ((uint32_t) buf[2] << 16) |
    ((uint32_t) buf[3] << 24);
  return 0;
}

int
load_ply(const char *file_name, struct stanford_ply *p)
{
  FILE *f;
  char buf[256];
  int num_vertex;
  int num_face;

  p->num_vertex = -1;
  p->num_face = -1;
  p->vertex = NULL;
  p->normal = NULL;
  p->c_r = NULL;
  p->c_g = NULL;
  p->c_b = NULL;
  p->c_a = NULL;
  p->face_idx = NULL;

  f = fopen(file_name, "rb");
  if (!f) {
    fprintf(stderr, "Failed to open file '%s' for reading.\n", file_name);
    return -1;
  }

  if (!fgets(buf, sizeof(buf), f)) {
    fprintf(stderr, "Error/EOF reading file '%s' while looking for header.\n",
            file_name);
    goto err;
  }
  if (0 != strcmp(buf, "ply\n")) {
    fprintf(stderr, "'ply' header not found while reading file '%s'.\n",
            file_name);
    goto err;
  }

  /* Now look for end_header while finding number of vertexes and faces. */
  num_vertex = -1;
  num_face = -1;
  for (;;) {
    int n;

    if (!fgets(buf, sizeof(buf), f)) {
      fprintf(stderr, "Error/EOF reading file '%s' while reading ply header.\n",
              file_name);
      goto err;
    }
    if (0 == strcmp(buf, "end_header\n")) {
      break;
    } else if (sscanf(buf, "element vertex %d", &n)) {
      if (n <= 0 && n >= 1e8) {
        fprintf(stderr, "Invalid number of vertexes found: %d\n", n);
        goto err;
      }
      num_vertex = n;
    } else if (sscanf(buf, "element face %d", &n)) {
      if (n <= 0 && n >= 1e8) {
        fprintf(stderr, "Invalid number of faces found: %d\n", n);
        goto err;
      }
      num_face = n;
    }
    /*
      For now, just assume the format of the data:
        format binary_little_endian 1.0
        element vertex *
        property float x
        property float y
        property float z
        property float nx
        property float ny
        property float nz
        property float s
        property float t
        property uchar red
        property uchar green
        property uchar blue
        property uchar alpha
        element face *
        property list uchar uint vertex_indices
    */
  }
  if (num_vertex < 0 || num_face < 0) {
    fprintf(stderr, "Incomplete header while reading file '%s'\n", file_name);
    goto err;
  }
  if (!(p->vertex = calloc(num_vertex, sizeof((*p->vertex))))) {
    fprintf(stderr, "Out of memory allocating %d vertices\n", num_vertex);
    goto err;
  }
  if (!(p->normal = calloc(num_vertex, sizeof((*p->normal))))) {
    fprintf(stderr, "Out of memory allocating %d normals\n", num_vertex);
    goto err;
  }
  if (!(p->c_r = calloc(num_vertex, sizeof(p->c_r[0])))) {
    fprintf(stderr, "Out of memory allocating %d red-colour values\n", num_vertex);
    goto err;
  }
  if (!(p->c_g = calloc(num_vertex, sizeof(p->c_g[0])))) {
    fprintf(stderr, "Out of memory allocating %d green-colour values\n", num_vertex);
    goto err;
  }
  if (!(p->c_b = calloc(num_vertex, sizeof(p->c_b[0])))) {
    fprintf(stderr, "Out of memory allocating %d blue-colour values\n", num_vertex);
    goto err;
  }
  if (!(p->c_a = calloc(num_vertex, sizeof(p->c_a[0])))) {
    fprintf(stderr, "Out of memory allocating %d alpha values\n", num_vertex);
    goto err;
  }
  if (!(p->face_idx = calloc(num_face, sizeof(*p->face_idx)))) {
    fprintf(stderr, "Out of memory allocating %d face pointers\n", num_face);
    goto err;
  }
  p->num_vertex = num_vertex;
  p->num_face = num_face;

  for (int i = 0; i < num_vertex; ++i) {
    float arr[8];
    uint8_t col[4];

    for (int j = 0; j < 8; ++j) {
      if (read_float(f, &arr[j], file_name))
        goto err;
    }
    for (int j = 0; j < 4; ++j) {
      if (read_uchar(f, &col[j], file_name))
        goto err;
    }
    for (int j = 0; j < 3; ++j) {
      p->vertex[i][j] = arr[j];
      p->normal[i][j] = arr[j+3];
    }
    p->c_r[i] = col[0];
    p->c_g[i] = col[1];
    p->c_b[i] = col[2];
    p->c_a[i] = col[3];
  }

  for (int i = 0; i < num_face; ++i) {
    uint8_t list_len;
    uint32_t idx;
    uint32_t *list;

    if (read_uchar(f, &list_len, file_name))
      goto err;
    if (!(list = calloc(list_len+1, sizeof(*list)))) {
      fprintf(stderr, "Out of memory allocating %d face corners.\n", (int)list_len);
      goto err;
    }
    list[0] = list_len;
    p->face_idx[i] = list;
    for (int j = 0; j < list_len; ++j) {
      if (read_uint(f, &idx, file_name))
        goto err;
      list[j+1] = idx;
    }
  }

  if (!feof(f) && fgetc(f) != EOF) {
    fprintf(stderr, "Warning: Did not read to end of file '%s' (ended at %ld)\n",
            file_name, ftell(f));
  }

  fclose(f);
  return 0;
err:
  free_ply(p);
  fclose(f);
  return -1;
}

void
free_ply(const struct stanford_ply *p)
{
  if (p->face_idx) {
    for (int i = 0; i < p->num_face; ++i)
      free(p->face_idx[i]);
    free(p->face_idx);
  }
  free(p->vertex);
  free(p->normal);
  free(p->c_r);
  free(p->c_g);
  free(p->c_b);
  free(p->c_a);
}

__attribute__((unused))
static void
dump_ply(const struct stanford_ply *p)
{
  printf("Vertices: %d ; faces: %d\n", p->num_vertex, p->num_face);
  for (int i = 0; i < p->num_vertex; ++i) {
    printf("  V[%4d]: %10.2f  %10.2f  %10.2f  %08x\n",
           i, p->vertex[i][0], p->vertex[i][1], p->vertex[i][2],
           ((uint32_t)p->c_r[i] << 24) | ((uint32_t)p->c_g[i] << 16) |
           ((uint32_t)p->c_b[i] << 8) | (uint32_t)p->c_a[i]);
  }
  for (int i = 0; i < p->num_face; ++i) {
    printf("  F[%4d]:", i);
    for (uint32_t j = 0; j < p->face_idx[i][0]; ++j) {
      printf(" %4u", (unsigned)p->face_idx[i][j+1]);
    }
    printf("\n");
  }
}

#ifdef TEST_MAIN
int
main(int argc, char *argv[])
{
  struct stanford_ply ply;
  if (!load_ply("vertex_colour_test2.ply", &ply)) {
    struct vec3d q;

    dump_ply(&ply);

    q.x = -0.8;
    q.y = 0.2;
    q.z = 0.3;
    for (int i = 0; i < ply.num_face; ++i) {
      check_point_against_poly(q, i, &ply);
    }

    q.x = -1.2;
    q.y = 0.2;
    q.z = 0.3;
    for (int i = 0; i < ply.num_face; ++i) {
      check_point_against_poly(q, i, &ply);
    }

    q.x = -1.2;
    q.y = 1.5;
    q.z = 0.3;
    for (int i = 0; i < ply.num_face; ++i) {
      check_point_against_poly(q, i, &ply);
    }

    free_ply(&ply);
  }
  return 0;
}
#endif
