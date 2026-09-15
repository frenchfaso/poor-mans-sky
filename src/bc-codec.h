// SPDX-License-Identifier: MPL-2.0
/* Offline endpoint search. The original bounding-box candidate is retained,
 * so the selected block never increases RGB squared error over the old coder.
 */
static unsigned bcCandidate(const unsigned char v[16][4], unsigned short a,
                            unsigned short b, unsigned char out[8]) {
  if (a < b) {
    unsigned short t = a;
    a = b;
    b = t;
  }
  if (a == b) {
    if (a < 65535)
      a++;
    else
      b--;
  }
  int colors[4][3];
  unpack565(a, colors[0]);
  unpack565(b, colors[1]);
  for (int c = 0; c < 3; c++) {
    colors[2][c] = (2 * colors[0][c] + colors[1][c]) / 3;
    colors[3][c] = (colors[0][c] + 2 * colors[1][c]) / 3;
  }
  unsigned error = 0;
  uint32_t bits = 0;
  for (int j = 0; j < 16; j++) {
    unsigned best = ~0u;
    int index = 0;
    for (int k = 0; k < 4; k++) {
      unsigned d = 0;
      for (int c = 0; c < 3; c++) {
        int q = v[j][c] - colors[k][c];
        d += q * q;
      }
      if (d < best) {
        best = d;
        index = k;
      }
    }
    error += best;
    bits |= (uint32_t)index << (j * 2);
  }
  out[0] = a;
  out[1] = a >> 8;
  out[2] = b;
  out[3] = b >> 8;
  for (int k = 0; k < 4; k++)
    out[4 + k] = bits >> (8 * k);
  return error;
}
static void bcColor(const unsigned char v[16][4], unsigned char out[8]) {
  unsigned char lo[3] = {255, 255, 255}, hi[3] = {0};
  for (int i = 0; i < 16; i++)
    for (int c = 0; c < 3; c++) {
      if (v[i][c] < lo[c])
        lo[c] = v[i][c];
      if (v[i][c] > hi[c])
        hi[c] = v[i][c];
    }
  unsigned best = bcCandidate(v, rgb565(hi), rgb565(lo), out);
  /* The farthest pair follows the chromatic axis rather than empty RGB-box
   * corners. */
  int ia = 0, ib = 0, far = -1;
  for (int i = 0; i < 16; i++)
    for (int j = 0; j < i; j++) {
      int d = 0;
      for (int c = 0; c < 3; c++) {
        int q = v[i][c] - v[j][c];
        d += q * q;
      }
      if (d > far) {
        far = d;
        ia = i;
        ib = j;
      }
    }
  unsigned char candidate[8];
  unsigned err = bcCandidate(v, rgb565(v[ia]), rgb565(v[ib]), candidate);
  if (err < best) {
    best = err;
    memcpy(out, candidate, 8);
  }
  /* Least-squares endpoint refinement with fixed palette assignments. */
  for (int pass = 0; pass < 3; pass++) {
    float aa = 0, ab = 0, bb = 0, ac[3] = {0}, bc[3] = {0};
    uint32_t bits = out[4] | (uint32_t)out[5] << 8 | (uint32_t)out[6] << 16 |
                    (uint32_t)out[7] << 24;
    for (int i = 0; i < 16; i++) {
      int k = (bits >> (2 * i)) & 3;
      float a = k == 0 ? 1 : k == 1 ? 0 : k == 2 ? 2.f / 3 : 1.f / 3, b = 1 - a;
      aa += a * a;
      ab += a * b;
      bb += b * b;
      for (int c = 0; c < 3; c++) {
        ac[c] += a * v[i][c];
        bc[c] += b * v[i][c];
      }
    }
    float det = aa * bb - ab * ab;
    if (det < .0001f)
      break;
    for (int c = 0; c < 3; c++) {
      hi[c] = (unsigned char)lrintf(
          clampf((ac[c] * bb - bc[c] * ab) / det, 0, 255));
      lo[c] = (unsigned char)lrintf(
          clampf((bc[c] * aa - ac[c] * ab) / det, 0, 255));
    }
    err = bcCandidate(v, rgb565(hi), rgb565(lo), candidate);
    if (err >= best)
      break;
    best = err;
    memcpy(out, candidate, 8);
  }
}
static void bcEncode(const unsigned char *rgba, int w, int h, int alpha,
                     unsigned char *out) {
  for (int y = 0; y < h; y += 4)
    for (int x = 0; x < w; x += 4) {
      unsigned char v[16][4];
      for (int j = 0; j < 16; j++)
        memcpy(v[j], rgba + ((y + j / 4) * w + x + j % 4) * 4, 4);
      if (alpha) {
        int a = 0, b = 255;
        for (int j = 0; j < 16; j++) {
          if (v[j][3] > a)
            a = v[j][3];
          if (v[j][3] < b)
            b = v[j][3];
        }
        if (a == b) {
          if (a < 255)
            a++;
          else
            b--;
        }
        int palette[8] = {a, b};
        for (int k = 2; k < 8; k++)
          palette[k] = ((8 - k) * a + (k - 1) * b) / 7;
        uint64_t bits = 0;
        for (int j = 0; j < 16; j++) {
          int best = 1000, index = 0;
          for (int k = 0; k < 8; k++) {
            int e = abs(v[j][3] - palette[k]);
            if (e < best) {
              best = e;
              index = k;
            }
          }
          bits |= (uint64_t)index << (j * 3);
        }
        out[0] = a;
        out[1] = b;
        for (int j = 0; j < 6; j++)
          out[2 + j] = bits >> (j * 8);
        out += 8;
      }
      bcColor(v, out);
      out += 8;
    }
}
static size_t bcMipBytes(int size, int levels) {
  size_t n = 0;
  for (int i = 0; i <= levels; i++, size /= 2)
    n += size * size;
  return n;
}
static void bcMipEncode(unsigned char *rgba, int size, int levels,
                        unsigned char *out) {
  unsigned char *p = malloc(size * size * 4);
  if (!p)
    die("BC3 mip allocation");
  memcpy(p, rgba, size * size * 4);
  for (int level = 0; level <= levels; level++) {
    bcEncode(p, size, size, 1, out);
    out += size * size;
    if (level == levels)
      break;
    int next = size / 2;
    for (int y = 0; y < next; y++)
      for (int x = 0; x < next; x++) {
        int sum[4] = {0}, rgb[3] = {0};
        for (int j = 0; j < 2; j++)
          for (int i = 0; i < 2; i++) {
            unsigned char *q = p + ((y * 2 + j) * size + x * 2 + i) * 4;
            for (int c = 0; c < 4; c++)
              sum[c] += q[c];
            for (int c = 0; c < 3; c++)
              rgb[c] += q[c] * q[3];
          }
        unsigned char *d = p + (y * next + x) * 4;
        for (int c = 0; c < 3; c++)
          d[c] = sum[3] ? rgb[c] / sum[3] : sum[c] / 4;
        d[3] = sum[3] / 4;
      }
    size = next;
  }
  free(p);
}
static void bcMipUpload(const unsigned char *p, int size, int levels) {
  for (int i = 0; i <= levels; i++, size /= 2) {
    glCompressedTexImage2D(GL_TEXTURE_2D, i, GL_COMPRESSED_RGBA_S3TC_DXT5_EXT,
                           size, size, 0, size * size, p);
    p += size * size;
  }
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, levels);
}
