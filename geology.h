// SPDX-License-Identifier: MPL-2.0
/* Immutable spherical fields. Longitude wraps; each pole is a single sample.
 * Generated before workers start. All stages and binary layout are versioned.
 */
#define GEO_W 512
#define GEO_H 257
#define GEO_N (GEO_W * GEO_H)
typedef struct {
  float h, wet, temperature, rock, flow, relief;
  int downstream;
  float basin;
} GeoCell;
static GeoCell *geology;
static uint32_t geologySeed;
static float geoMaximum = 12000;
static void geoProgress(const char *stage, float progress) {
  printf("GEOLOGY %s %.0f%%\n", stage, progress * 100);
  if (!window)
    return;
  SDL_Event e;
  while (SDL_PollEvent(&e))
    if (e.type == SDL_QUIT ||
        (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE))
      die("loading cancelled");
  SDL_SetWindowTitle(window, stage);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glViewport(0, 0, width, height);
  glUseProgram(0);
  glDisable(GL_DEPTH_TEST);
  glClearColor(.015f, .025f, .04f, 1);
  glClear(GL_COLOR_BUFFER_BIT);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, width, height, 0, -1, 1);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glColor3f(.25f, .75f, .85f);
  glBegin(GL_QUADS);
  glVertex2f(width * .15f, height * .5f);
  glVertex2f(width * (.15f + .7f * progress), height * .5f);
  glVertex2f(width * (.15f + .7f * progress), height * .5f + 18);
  glVertex2f(width * .15f, height * .5f + 18);
  glEnd();
  label(width * .15f, height * .5f - 50, "POOR MAN'S SKY / GENERATING", 2);
  label(width * .15f, height * .5f + 40, stage, 1.4f);
  SDL_GL_SwapWindow(window);
}
static int geoIndex(int x, int y) {
  if (y < 0) {
    y = -y;
    x += GEO_W / 2;
  }
  if (y >= GEO_H) {
    y = 2 * (GEO_H - 1) - y;
    x += GEO_W / 2;
  }
  x &= GEO_W - 1;
  if (y == 0 || y == GEO_H - 1)
    x = 0;
  return y * GEO_W + x;
}
static V3 geoDirection(int x, int y) {
  float lat = -PI * .5f + PI * y / (GEO_H - 1), lon = 2 * PI * x / GEO_W;
  return v3(cosf(lat) * cosf(lon), sinf(lat), cosf(lat) * sinf(lon));
}
/* Min heap for priority flood; ties are ordered by cell identity. */
static int geoLess(int a, int b, const float *height) {
  return height[a] < height[b] || (height[a] == height[b] && a < b);
}
static void geoPush(int *heap, int *size, int id, const float *height) {
  int k = (*size)++;
  while (k && geoLess(id, heap[(k - 1) / 2], height)) {
    heap[k] = heap[(k - 1) / 2];
    k = (k - 1) / 2;
  }
  heap[k] = id;
}
static int geoPop(int *heap, int *size, const float *height) {
  int out = heap[0], id = heap[--*size], k = 0;
  while (k * 2 + 1 < *size) {
    int c = k * 2 + 1;
    if (c + 1 < *size && geoLess(heap[c + 1], heap[c], height))
      c++;
    if (!geoLess(heap[c], id, height))
      break;
    heap[k] = heap[c];
    k = c;
  }
  if (*size)
    heap[k] = id;
  return out;
}
static void geologyInit(void) {
  if (!geological)
    return;
  Uint32 start = SDL_GetTicks();
  free(geology);
  geology = calloc(GEO_N, sizeof(*geology));
  if (!geology)
    die("global terrain allocation");
  geologySeed = worldSeed;
  uint32_t bytes = 0;
  if (cacheRead(5, 0, GEO_W, GEO_H, 0, geology, GEO_N * sizeof(*geology), NULL,
                0, &bytes)) {
    geoProgress("LANDSCAPE FROM CACHE", 1);
    printf("GEOLOGY ready cache=1 seconds=%.3f memory_mib=%.2f\n",
           (SDL_GetTicks() - start) / 1000.,
           GEO_N * sizeof(*geology) / 1048576.);
    return;
  }
  geoProgress("CONTINENTS AND UPLIFT", 0);
  V3 plates[18], motion[18];
  float base[18], stone[18];
  float angle = (hash3(171, 9, 2) & 65535) / 65535.f * 2 * PI;
  for (int p = 0; p < 18; p++) {
    float y = 1 - 2 * (p + .5f) / 18, a = p * 2.39996323f + angle;
    plates[p] = v3(sqrtf(1 - y * y) * cosf(a), y, sqrtf(1 - y * y) * sinf(a));
    uint32_t h = hash3(p, 918, 71);
    base[p] = (h & 255) < 140 ? 1800.f : -4200.f;
    stone[p] = ((h >> 8) & 255) / 255.f;
    motion[p] = norm(cross(plates[p], v3(.3f + stone[p], .7f, -.4f)));
  }
  for (int y = 0; y < GEO_H; y++) {
    for (int x = 0; x < GEO_W; x++) {
      V3 d = geoDirection(x, y);
      float sum = 0, h = 0, r = 0, best = -2, next = -2;
      int one = 0, two = 1;
      for (int p = 0; p < 18; p++) {
        float q = dot(d, plates[p]), w = fmaxf(0, (q + 1) * .5f);
        w *= w;
        w *= w;
        w *= w;
        w *= w;
        w *= w;
        sum += w;
        h += w * base[p];
        r += w * stone[p];
        if (q > best) {
          next = best;
          two = one;
          best = q;
          one = p;
        } else if (q > next) {
          next = q;
          two = p;
        }
      }
      float convergence =
          clampf(dot(add(motion[one], mul(motion[two], -1)),
                     norm(add(plates[two], mul(plates[one], -1)))) *
                         .5f +
                     .5f,
                 0, 1);
      float belt = expf(-(best - next) * 30) * convergence;
      GeoCell *c = &geology[y * GEO_W + x];
      c->relief = clampf(belt + (noise3(mul(d, 11)) - .3f) * .4f, 0, 1);
      c->h = h / sum + (noise3(add(mul(d, 5), v3(9, 3, 1))) - .5f) * 1700 +
             belt * 3800 - 350;
      c->rock = r / sum;
      c->downstream = -1;
    }
    if (y % 64 == 0)
      geoProgress("CONTINENTS AND UPLIFT", .25f * y / (GEO_H - 1));
  }
  /* Canonical poles are expanded into their storage rows only after solving. */
  float *filled = malloc(GEO_N * sizeof(float));
  int *heap = malloc(GEO_N * sizeof(int)), *order = malloc(GEO_N * sizeof(int));
  unsigned char *seen = calloc(GEO_N, 1);
  if (!filled || !heap || !order || !seen)
    die("drainage workspace");
  int size = 0, norder = 0;
  for (int y = 0; y < GEO_H; y++)
    for (int x = 0; x < (y == 0 || y == GEO_H - 1 ? 1 : GEO_W); x++) {
      int id = y * GEO_W + x;
      filled[id] = geology[id].h;
      if (filled[id] < 0) {
        seen[id] = 1;
        geoPush(heap, &size, id, filled);
      }
    }
  if (!size) {
    seen[0] = 1;
    geoPush(heap, &size, 0, filled);
  }
  geoProgress("DRAINAGE AND BASINS", .3f);
  while (size) {
    int id = geoPop(heap, &size, filled), x = id % GEO_W, y = id / GEO_W;
    order[norder++] = id;
    /* Pole fans connect all longitudes, rather than making a fake seam. */
    int count = (y == 0 || y == GEO_H - 1) ? GEO_W : 4;
    for (int k = 0; k < count; k++) {
      int j = count == GEO_W
                  ? geoIndex(k, y == 0 ? 1 : GEO_H - 2)
                  : geoIndex(x + (k == 0) - (k == 1), y + (k == 2) - (k == 3));
      if (seen[j])
        continue;
      seen[j] = 1;
      filled[j] = fmaxf(geology[j].h, filled[id] + .05f);
      geology[j].downstream = id;
      geoPush(heap, &size, j, filled);
    }
  }
  for (int k = 0; k < norder; k++) {
    int id = order[k];
    GeoCell *c = &geology[id];
    c->basin = fmaxf(0, filled[id] - c->h);
    c->flow = fmaxf(.01f, cosf(-PI * .5f + PI * (id / GEO_W) / (GEO_H - 1)));
  }
  for (int k = norder - 1; k >= 0; k--) {
    int id = order[k], to = geology[id].downstream;
    if (to >= 0)
      geology[to].flow += geology[id].flow;
  }
  geoProgress("FLUVIAL EROSION", .5f);
  /* Six bounded incision passes, processed outlet first. Drainage stays
   * acyclic and every land receiver stays lower. Sediment flattens basins. */
  for (int pass = 0; pass < 6; pass++)
    for (int k = 0; k < norder; k++) {
      int id = order[k];
      GeoCell *c = &geology[id];
      int to = c->downstream;
      if (to < 0 || c->h < 0)
        continue;
      float channel = clampf(log2f(1 + c->flow) / 14, 0, 1);
      float target =
          filled[id] - channel * (45 + 220 * c->relief) * (pass + 1) / 6;
      c->h = fmaxf(fmaxf(0, geology[to].h) + .05f, target);
    }
  geoProgress("CLIMATE AND BIOMES", .75f);
  for (int y = 1; y < GEO_H - 1; y++)
    for (int x = 0; x < GEO_W; x++) {
      int id = y * GEO_W + x;
      GeoCell *c = &geology[id];
      V3 d = geoDirection(x, y);
      float barrier = 0, sea = 0;
      int wind = fabsf(d.y) > .5f ? -1 : 1;
      for (int k = 1; k <= 12; k++) {
        float up = geology[geoIndex(x - wind * k, y)].h;
        barrier = fmaxf(barrier, up - c->h);
        sea += up < 0 ? 1.f / 12 : 0;
      }
      c->temperature = 28 - 48 * fabsf(d.y) - fmaxf(c->h, 0) * .0065f;
      c->wet = clampf(.22f + sea * .55f + noise3(mul(d, 8)) * .3f -
                          barrier * .00015f + log2f(1 + c->flow) * .022f,
                      0, 1);
    }
  for (int y = 0; y < GEO_H; y += GEO_H - 1) {
    geology[y * GEO_W].temperature = -20;
    geology[y * GEO_W].wet = .3f;
    for (int x = 1; x < GEO_W; x++)
      geology[y * GEO_W + x] = geology[y * GEO_W];
  }
  free(filled);
  free(heap);
  free(order);
  free(seen);
  cacheWrite(5, 0, GEO_W, GEO_H, 0, geology, GEO_N * sizeof(*geology), NULL, 0);
  geoProgress("LANDSCAPE READY", 1);
  printf("GEOLOGY ready cache=0 seconds=%.3f memory_mib=%.2f\n",
         (SDL_GetTicks() - start) / 1000., GEO_N * sizeof(*geology) / 1048576.);
}
/* Cubic B-spline weights: smooth, nonnegative and no height overshoot. */
static void geoWeights(float t, float *w, float *g) {
  float a = 1 - t;
  w[0] = a * a * a / 6;
  w[1] = (3 * t * t * t - 6 * t * t + 4) / 6;
  w[2] = (-3 * t * t * t + 3 * t * t + 3 * t + 1) / 6;
  w[3] = t * t * t / 6;
  g[0] = -a * a * .5f;
  g[1] = 1.5f * t * t - 2 * t;
  g[2] = -1.5f * t * t + t + .5f;
  g[3] = t * t * .5f;
}
static GeoCell geoSample(V3 d, V3 *gradient) {
  GeoCell out = {0};
  if (!geology || geologySeed != worldSeed) {
    out.wet = .5f;
    out.temperature = 18;
    out.relief = .5f;
    return out;
  }
  float lon = atan2f(d.z, d.x) * GEO_W / (2 * PI),
        lat = (asinf(clampf(d.y, -1, 1)) / PI + .5f) * (GEO_H - 1);
  int x = (int)floorf(lon), y = (int)floorf(lat);
  float wx[4], wy[4], gx[4], gy[4], dx = 0, dy = 0;
  geoWeights(lon - x, wx, gx);
  geoWeights(lat - y, wy, gy);
  for (int j = 0; j < 4; j++)
    for (int i = 0; i < 4; i++) {
      GeoCell *c = &geology[geoIndex(x + i - 1, y + j - 1)];
      float w = wx[i] * wy[j];
      out.h += c->h * w;
      out.wet += c->wet * w;
      out.temperature += c->temperature * w;
      out.rock += c->rock * w;
      out.flow += c->flow * w;
      out.relief += c->relief * w;
      dx += c->h * gx[i] * wy[j];
      dy += c->h * wx[i] * gy[j];
    }
  if (gradient) {
    float r2 = d.x * d.x + d.z * d.z;
    *gradient = r2 > 1e-8f
                    ? add(mul(v3(-d.z, 0, d.x), dx * GEO_W / (2 * PI * r2)),
                          mul(add(v3(0, 1, 0), mul(d, -d.y)),
                              dy * (GEO_H - 1) / (PI * sqrtf(r2))))
                    : v3(0, 0, 0);
  }
  return out;
}
static float geoResidual(V3 d, GeoCell sample) {
  float channel = clampf(log2f(1 + sample.flow) / 14, 0, 1),
        amp = (.25f + sample.relief) * (1 - channel * .75f);
  return sample.h + amp * ((noise3(mul(d, RADIUS / 650)) - .5f) * 180 +
                           (noise3(mul(d, RADIUS / 95)) - .5f) * 35 +
                           (noise3(mul(d, RADIUS / 14)) - .5f) * 4);
}
static float geoElevation(V3 d, GeoCell *sample) {
  if (!geological || !geology || geologySeed != worldSeed) {
    *sample = (GeoCell){0};
    sample->wet = .5f;
    sample->temperature = 18;
    sample->relief = .5f;
    return legacyElevation(d);
  }
  *sample = geoSample(d, NULL);
  return geoResidual(d, *sample);
}
static float elevation(V3 d) {
  GeoCell sample;
  return geoElevation(d, &sample);
}
/* Analytic value-noise gradient: the same eight hashes as a height sample. */
static float noiseGradient(V3 p, V3 *gradient) {
  int x = (int)floorf(p.x), y = (int)floorf(p.y), z = (int)floorf(p.z);
  float t[3] = {p.x - x, p.y - y, p.z - z}, w[3], g[3], value = 0;
  *gradient = v3(0, 0, 0);
  for (int j = 0; j < 3; j++) {
    w[j] = t[j] * t[j] * (3 - 2 * t[j]);
    g[j] = 6 * t[j] * (1 - t[j]);
  }
  for (int k = 0; k < 8; k++) {
    float a = (k & 1) ? w[0] : 1 - w[0], b = (k & 2) ? w[1] : 1 - w[1],
          c = (k & 4) ? w[2] : 1 - w[2];
    float v =
        (hash3(x + (k & 1), y + ((k >> 1) & 1), z + ((k >> 2) & 1)) & 65535) /
        65535.f;
    value += v * a * b * c;
    gradient->x += v * ((k & 1) ? g[0] : -g[0]) * b * c;
    gradient->y += v * a * ((k & 2) ? g[1] : -g[1]) * c;
    gradient->z += v * a * b * ((k & 4) ? g[2] : -g[2]);
  }
  return value;
}
static V3 geoNormal(V3 d) {
  V3 g;
  GeoCell c = geoSample(d, &g);
  float amp =
      (.25f + c.relief) * (1 - clampf(log2f(1 + c.flow) / 14, 0, 1) * .75f);
  const float scale[3] = {RADIUS / 650, RADIUS / 95, RADIUS / 14},
              weight[3] = {180, 35, 4};
  for (int i = 0; i < 3; i++) {
    V3 q;
    noiseGradient(mul(d, scale[i]), &q);
    g = add(g, mul(q, scale[i] * weight[i] * amp));
  }
  /* Slowly varying amplitude derivatives are negligible at local scale. */
  g = add(g, mul(d, -dot(g, d)));
  return norm(add(d, mul(g, -1 / RADIUS)));
}
