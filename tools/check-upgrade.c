// SPDX-License-Identifier: MPL-2.0
#define main poor_mans_sky_application_main
#include "../src/poor-mans-sky.c"
#undef main
#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x)) {                                                                \
      fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x);                          \
      return 1;                                                                \
    }                                                                          \
  } while (0)
int main(int argc, char **argv) {
  CHECK(argc == 2);
  SDL_Init(SDL_INIT_TIMER);
  cacheDirectory = argv[1];
  cacheInit();
  geologyInit();
  int land = 0;
  float lo = 1e9f, hi = -1e9f;
  for (int y = 0; y < GEO_H; y++)
    for (int x = 0; x < (y == 0 || y == GEO_H - 1 ? 1 : GEO_W); x++) {
      int id = geoIndex(x, y);
      GeoCell c = geology[id];
      CHECK(isfinite(c.h) && c.wet >= 0 && c.wet <= 1);
      CHECK(c.downstream >= -1 && c.downstream < GEO_N && c.downstream != id);
      if (c.h > 0 && c.downstream >= 0) {
        CHECK(c.h > geology[c.downstream].h);
        land++;
      }
      lo = fminf(lo, c.h);
      hi = fmaxf(hi, c.h);
    }
  CHECK(land > 1000 && hi - lo > 4000 && hi < geoMaximum - 200);
  uint32_t sum = cacheHash(geology, GEO_N * sizeof(*geology), 2166136261u);
  geologyInit();
  CHECK(sum == cacheHash(geology, GEO_N * sizeof(*geology), 2166136261u));
  for (int j = 0; j <= 100; j++) {
    float v = -1 + j * .02f;
    CHECK(fabsf(elevation(direction(0, 1, v)) -
                elevation(direction(5, -1, v))) < .01f);
    CHECK(fabsf(elevation(direction(2, v, 1)) -
                elevation(direction(5, -v, 1))) < .05f);
  }
  float normalError = 0;
  for (int j = 0; j < 100; j++) {
    V3 d = norm(v3(.31f + j * .001f, .5f, .7f)),
       t = norm(cross(d, v3(0, 1, 0))), b = cross(d, t);
    float dx = (elevation(norm(add(d, mul(t, 2 / RADIUS)))) -
                elevation(norm(add(d, mul(t, -2 / RADIUS))))) /
               4;
    float dy = (elevation(norm(add(d, mul(b, 2 / RADIUS)))) -
                elevation(norm(add(d, mul(b, -2 / RADIUS))))) /
               4;
    V3 expected = norm(add(d, add(mul(t, -dx), mul(b, -dy))));
    normalError = fmaxf(normalError, 1 - dot(expected, geoNormal(d)));
  }
  CHECK(normalError < .02f);
  static NatureCandidate candidates[NATURE_CANDIDATES];
  for (int i = 0; i < 8; i++) {
    V3 d = norm(v3(i & 1 ? 1 : -1, i & 2 ? 1 : -1, i & 4 ? 1 : -1));
    int n = natureCandidates(d, candidates), faces = 0;
    CHECK(n > 121 && n < NATURE_CELLS);
    for (int j = 0; j < n; j++) {
      faces |= 1 << candidates[j].face;
      for (int k = 0; k < j; k++)
        CHECK(candidates[j].face != candidates[k].face ||
              candidates[j].x != candidates[k].x ||
              candidates[j].y != candidates[k].y);
    }
    CHECK(__builtin_popcount((unsigned)faces) == 3);
  }
  countNode = MAXNODE;
  frameNo = 1000;
  recycleCursor = 6;
  for (int i = 0; i < countNode; i++) {
    nodes[i] = (Node){0};
    nodes[i].level = -1;
    nodes[i].slot = -1;
    for (int k = 0; k < 4; k++)
      nodes[i].child[k] = -1;
  }
  nodes[6].level = 3;
  for (int k = 0; k < 4; k++) {
    nodes[6].child[k] = 7 + k;
    nodes[7 + k].level = 4;
  }
  nodes[7].state = 1;
  recycleNodes();
  CHECK(!freeCount);
  nodes[7].state = 0;
  recycleCursor = 6;
  recycleNodes();
  CHECK(freeCount == 4);
  int reused = newNode(0, 4, 0, 0);
  CHECK(reused >= 7 && reused <= 10 && countNode == MAXNODE && freeCount == 3);
  int testX=(int)(SDL_GetTicks()%10000);
  diskLimit = diskBytes + 120;
  unsigned char data[64] = {1};
  cacheWrite(1, 99, 0, testX, 0, data, 64, NULL, 0);
  cacheWrite(1, 99, 0, testX+1, 0, data, 64, NULL, 0);
  CHECK(diskEvicted > 0 && diskBytes <= diskLimit);
  uint32_t bytes = 0;
  unsigned char read[64];
  CHECK(cacheRead(1, 99, 0, testX+1, 0, read, 64, NULL, 0, &bytes) &&
        !memcmp(data, read, 64));
  printf("PASS geology cache/determinism, downhill drainage=%d "
         "relief=%.1f..%.1f, seams, analytic normals error=%.6f, 8 vegetation "
         "corners, pinned node reclamation and disk eviction\n",
         land, lo, hi, normalError);
  free(geology);
  cacheClose();
  SDL_Quit();
  return 0;
}
