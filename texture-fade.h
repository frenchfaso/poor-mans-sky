// SPDX-License-Identifier: MPL-2.0
/* Snapshot the old virtual coverage into a tiny BC1 transition atlas. This
 * handles both refinement and coarsening, even when old pages span atlases.
 * The additional fragment sample exists only in the transition shader. */
#define TEXTURE_FADES 32
typedef struct {
  int id;
  Uint32 start;
} TextureFade;
static TextureFade textureFades[TEXTURE_FADES];
static GLuint transitionAtlas, fadePrograms[2][2];
static int fadeInitialized, textureTransitions;
static int textureFadeFor(int id) {
  if (!fadeInitialized)
    return -1;
  for (int i = 0; i < TEXTURE_FADES; i++)
    if (textureFades[i].id == id)
      return i;
  return -1;
}
static void pagePixel(const unsigned char *page, float u, float v,
                      unsigned char *out) {
  int x = (int)clampf(4 + u * 120, 0, 127),
      y = (int)clampf(4 + v * 120, 0, 127);
  const unsigned char *block = page + ((y / 4) * 32 + x / 4) * 8;
  int color[4][3];
  unpack565(block[0] | block[1] << 8, color[0]);
  unpack565(block[2] | block[3] << 8, color[1]);
  for (int c = 0; c < 3; c++) {
    color[2][c] = (2 * color[0][c] + color[1][c]) / 3;
    color[3][c] = (color[0][c] + 2 * color[1][c]) / 3;
  }
  uint32_t bits = (uint32_t)block[4] | (uint32_t)block[5] << 8 |
                  (uint32_t)block[6] << 16 | (uint32_t)block[7] << 24;
  int index = (bits >> (2 * ((y % 4) * 4 + x % 4))) & 3;
  for (int c = 0; c < 3; c++)
    out[c] = color[index][c];
  out[3] = 255;
}
static void prepareTextureFades(void) {
  if (!fadeInitialized) {
    for (int i = 0; i < TEXTURE_FADES; i++)
      textureFades[i].id = -1;
    fadeInitialized = 1;
  }
  Uint32 now = SDL_GetTicks();
  for (int i = 0; i < TEXTURE_FADES; i++)
    if (textureFades[i].id >= 0 && (now - textureFades[i].start >= 450 ||
                                    nodes[textureFades[i].id].used != frameNo))
      textureFades[i].id = -1;
  if (!previousCount)
    return;
  int created = 0;
  for (int i = 0; i < selectedCount && created < 1; i++) {
    int id = selected[i];
    Node *n = &nodes[id];
    if (previousIndex[id] >= 0 || textureFadeFor(id) >= 0 || !n->pixels ||
        !nodeVisible(n))
      continue;
    int slot = -1;
    for (int k = 0; k < TEXTURE_FADES; k++)
      if (textureFades[k].id < 0) {
        slot = k;
        break;
      }
    if (slot < 0)
      break;
    int sources[1024], count = 0;
    float scale = 1.f / (1 << n->level), x0 = n->x * scale, y0 = n->y * scale;
    for (int k = 0; k < previousCount; k++) {
      Node *old = &nodes[previousCover[k]];
      if (old->face != n->face || !old->pixels)
        continue;
      float os = 1.f / (1 << old->level), ox = old->x * os, oy = old->y * os;
      if (ox < x0 + scale && ox + os > x0 && oy < y0 + scale && oy + os > y0)
        sources[count++] = previousCover[k];
    }
    if (!count)
      continue;
    unsigned char pixels[PAGE * PAGE * 4];
    for (int y = 0; y < PAGE; y++)
      for (int x = 0; x < PAGE; x++) {
        float u = (x - 3.5f) / 120, v = (y - 3.5f) / 120,
              wx = x0 + clampf(u, 0, 1) * scale,
              wy = y0 + clampf(v, 0, 1) * scale;
        unsigned char *out = pixels + (y * PAGE + x) * 4;
        pagePixel(n->pixels, u, v, out);
        for (int k = 0; k < count; k++) {
          Node *old = &nodes[sources[k]];
          float os = 1.f / (1 << old->level), a = wx / os - old->x,
                b = wy / os - old->y;
          if (a >= -.00001f && b >= -.00001f && a <= 1.00001f &&
              b <= 1.00001f) {
            pagePixel(old->pixels, a, b, out);
            break;
          }
        }
      }
    unsigned char *bc1 = compressPage(pixels);
    glActiveTexture(GL_TEXTURE2);
    if (!transitionAtlas) {
      glGenTextures(1, &transitionAtlas);
      glBindTexture(GL_TEXTURE_2D, transitionAtlas);
      unsigned char *empty = calloc(1, 1024 * 512 / 2);
      if (!empty)
        die("transition atlas");
      glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGB_S3TC_DXT1_EXT,
                             1024, 512, 0, 1024 * 512 / 2, empty);
      free(empty);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    } else
      glBindTexture(GL_TEXTURE_2D, transitionAtlas);
    glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, (slot % 8) * 128,
                              (slot / 8) * 128, 128, 128,
                              GL_COMPRESSED_RGB_S3TC_DXT1_EXT, PAGE_BYTES, bc1);
    free(bc1);
    glActiveTexture(GL_TEXTURE0);
    textureFades[slot] = (TextureFade){id, now};
    created++;
    textureTransitions++;
  }
}
