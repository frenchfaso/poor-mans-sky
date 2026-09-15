// SPDX-License-Identifier: MPL-2.0
/* Procedural source mipmaps stay in RAM. Virtual pages bake their blend, so
 * materials cost no additional terrain samplers or VRAM atlas space. */
static unsigned char *materialMip[2][10];
static unsigned char materialStorage[2 * 1048575];
static void materialInit(void) {
  size_t offset = 0;
  for (int m = 0; m < 2; m++)
    for (int l = 0; l < 10; l++) {
      materialMip[m][l] = materialStorage + offset;
      offset += (512 >> l) * (512 >> l) * 3;
    }
  const char *files[] = {"assets/ground-procedural.bmp", "assets/rock-procedural.bmp"};
  for (int m = 0; m < 2; m++) {
    SDL_Surface *bmp = SDL_LoadBMP(resourcePath(files[m]));
    if (!bmp)
      die(files[m]);
    SDL_Surface *rgb = SDL_ConvertSurfaceFormat(bmp, SDL_PIXELFORMAT_RGB24, 0);
    SDL_FreeSurface(bmp);
    if (!rgb || rgb->w != 512 || rgb->h != 512)
      die("material dimensions");
    for (int y = 0; y < 512; y++)
      memcpy(materialMip[m][0] + y * 512 * 3,
             (unsigned char *)rgb->pixels + y * rgb->pitch, 512 * 3);
    SDL_FreeSurface(rgb);
    for (int l = 1, n = 256; l < 10; l++, n /= 2)
      for (int y = 0; y < n; y++)
        for (int x = 0; x < n; x++)
          for (int c = 0; c < 3; c++) {
            unsigned char *p = materialMip[m][l - 1];
            int k = (y * 2 * n * 2 + x * 2) * 3 + c;
            materialMip[m][l][(y * n + x) * 3 + c] =
                (p[k] + p[k + 3] + p[k + n * 6] + p[k + n * 6 + 3]) / 4;
          }
  }
}
static V3 materialSampleBase(int m, float u, float v, float footprint) {
  static __thread float lastFootprint = -1;
  static __thread int level;
  if (footprint != lastFootprint) {
    level = (int)clampf(floorf(log2f(fmaxf(1, footprint * 512 / 3.2f))), 0, 9);
    lastFootprint = footprint;
  }
  int size = 512 >> level;
  float x = (u / 3.2f - floorf(u / 3.2f)) * size,
        y = (v / 3.2f - floorf(v / 3.2f)) * size;
  int ix = (int)x, iy = (int)y;
  float fx = x - ix, fy = y - iy;
  V3 result = v3(0, 0, 0);
  for (int j = 0; j < 2; j++)
    for (int i = 0; i < 2; i++) {
      unsigned char *p = materialMip[m][level] +
                         (((iy + j) % size) * size + (ix + i) % size) * 3;
      float w = (i ? fx : 1 - fx) * (j ? fy : 1 - fy) / 255.0f;
      result = add(result, mul(v3(p[0], p[1], p[2]), w));
    }
  /* Convert material sRGB approximately to linear before baking. */
  return v3(result.x * result.x, result.y * result.y, result.z * result.z);
}
/* Continuous two-orientation blend breaks the single 3.2 m repeat. All
 * work is baked on the worker; GPU sampler count and atlas size are unchanged.
 */
static V3 materialSample(int m, float u, float v, float footprint) {
  V3 a = materialSampleBase(m, u, v, footprint);
  V3 b = materialSampleBase(m, u * .8f - v * .6f + 19, v * .8f + u * .6f + 37,
                            footprint);
  return add(mul(a, .65f), mul(b, .35f));
}
static V3 materialTriplanar(int m, V3 p, V3 n, float footprint) {
  V3 w = v3(fabsf(n.x), fabsf(n.y), fabsf(n.z));
  w = mul(w, 1 / (w.x + w.y + w.z));
  return add(mul(materialSample(m, p.y, p.z, footprint), w.x),
             add(mul(materialSample(m, p.x, p.z, footprint), w.y),
                 mul(materialSample(m, p.x, p.y, footprint), w.z)));
}
