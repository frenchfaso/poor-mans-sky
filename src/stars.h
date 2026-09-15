// SPDX-License-Identifier: MPL-2.0
#define STAR_SIZE 512
#define STAR_BYTES (6 * STAR_SIZE * STAR_SIZE)
static void initStars(void) {
  unsigned char *p = calloc(STAR_BYTES, 1);
  uint32_t bytes = 0, random = 0x31415926u;
  if (!p)
    die("star allocation");
  if (!cacheRead(3, 0, 0, 0, 0, p, STAR_BYTES, p, 0, &bytes)) {
    for (int i = 0; i < 2400; i++) {
      float q[3];
      for (int j = 0; j < 3; j++) {
        random ^= random << 13;
        random ^= random >> 17;
        random ^= random << 5;
        q[j] = (random & 0xffffff) / 16777216.0f;
      }
      float z = q[0] * 2 - 1, a = q[1] * 2 * PI, r = sqrtf(1 - z * z);
      V3 d = v3(r * cosf(a), z, r * sinf(a));
      float ax = fabsf(d.x), ay = fabsf(d.y), az = fabsf(d.z), u, v;
      int face;
      if (ax >= ay && ax >= az) {
        face = d.x > 0 ? 0 : 1;
        u = (d.x > 0 ? -d.z : d.z) / ax;
        v = -d.y / ax;
      } else if (ay >= az) {
        face = d.y > 0 ? 2 : 3;
        u = d.x / ay;
        v = (d.y > 0 ? d.z : -d.z) / ay;
      } else {
        face = d.z > 0 ? 4 : 5;
        u = (d.z > 0 ? d.x : -d.x) / az;
        v = -d.y / az;
      }
      int x = (int)clampf((u + 1) * .5f * STAR_SIZE, 0, STAR_SIZE - 1),
          y = (int)clampf((v + 1) * .5f * STAR_SIZE, 0, STAR_SIZE - 1);
      int k = (face * STAR_SIZE + y) * STAR_SIZE + x;
      int value = (int)(18 + 220 * powf(q[2], 5));
      if (value > p[k])
        p[k] = value;
    }
    cacheWrite(3, 0, 0, 0, 0, p, STAR_BYTES, p, 0);
  }
  glGenTextures(1, &skyCube);
  glBindTexture(GL_TEXTURE_CUBE_MAP, skyCube);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  for (int f = 0; f < 6; f++)
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + f, 0, GL_LUMINANCE8,
                 STAR_SIZE, STAR_SIZE, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE,
                 p + f * STAR_SIZE * STAR_SIZE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
  free(p);
}
