struct stanford_ply {
  int num_vertex;
  int num_face;
  float (*vertex)[3];
  float (*normal)[3];
  uint8_t *c_r;
  uint8_t *c_g;
  uint8_t *c_b;
  uint8_t *c_a;
  /*
    Each idx entry points to an array, the first element is the number of
    corners of the face and then follows N indexes into the vertex array.
  */
  uint32_t **face_idx;
};

extern int load_ply(const char *file_name, struct stanford_ply *p);
extern void free_ply(const struct stanford_ply *p);
