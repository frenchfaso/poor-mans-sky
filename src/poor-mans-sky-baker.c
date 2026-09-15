// SPDX-License-Identifier: MPL-2.0
/* Compile the exact game generators; no video subsystem or GL context is used.
 */
#define main poorMansSkyInteractiveMain
#include "poor-mans-sky.c"
#undef main
#include <pthread.h>
static pthread_mutex_t bakeMutex = PTHREAD_MUTEX_INITIALIZER;
static int bakeNext, bakeDone;
static void *bakeWorker(void *unused) {
  (void)unused;
  for (;;) {
    pthread_mutex_lock(&bakeMutex);
    int id = bakeNext++;
    pthread_mutex_unlock(&bakeMutex);
    if (id >= countNode)
      break;
    unsigned char *pixels;
    Vertex *vertices;
    generate(&nodes[id], &pixels, &vertices);
    free(pixels);
    free(vertices);
    pthread_mutex_lock(&bakeMutex);
    bakeDone++;
    if (bakeDone % 500 == 0 || bakeDone == countNode)
      printf("BAKE %d/%d %.1f%%\n", bakeDone, countNode,
             100. * bakeDone / countNode);
    pthread_mutex_unlock(&bakeMutex);
  }
  return NULL;
}
int main(int argc, char **argv) {
  resourceInit();
  int threads = 8, mib = 576, planOnly = 0, plants = 1;
  const char *manifest = NULL, *manifestOut = NULL;
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--threads") && i + 1 < argc)
      threads = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--mib") && i + 1 < argc)
      mib = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--cache-dir") && i + 1 < argc)
      cacheDirectory = argv[++i];
    else if (!strcmp(argv[i], "--seed") && i + 1 < argc)
      worldSeed = strtoul(argv[++i], NULL, 10);
    else if (!strcmp(argv[i], "--manifest") && i + 1 < argc)
      manifest = argv[++i];
    else if (!strcmp(argv[i], "--manifest-out") && i + 1 < argc)
      manifestOut = argv[++i];
    else if (!strcmp(argv[i], "--plan-only"))
      planOnly = 1;
    else if (!strcmp(argv[i], "--no-nature"))
      plants = 0;
    else {
      fprintf(stderr, "poor-mans-sky-baker [--threads 1..32] [--mib 1..1536] [--seed "
                      "N] [--cache-dir path] [--manifest file] [--manifest-out "
                      "file] [--plan-only] [--no-nature]\n");
      return 2;
    }
  }
  if (threads < 1 || threads > 32 || mib < 1 || mib > 1536)
    return 2;
  setvbuf(stdout, NULL, _IOLBF, 0);
  SDL_Init(SDL_INIT_TIMER);
  cacheInit();
  if (!cacheEnabled)
    die("baker needs a writable, unused cache directory");
  geologyInit();
  materialInit();
  view(1);
  if (manifest) {
    FILE *f = fopen(manifest, "r");
    if (!f)
      die("manifest open");
    int face, level, x, y;
    float cx, cy, cz;
    while (fscanf(f, "%d %d %d %d %f %f %f", &face, &level, &x, &y, &cx, &cy,
                  &cz) == 7) {
      if (face < 0 || face > 5 || level < 0 || level > MAXLEVEL || x < 0 ||
          y < 0 || x >= (1 << level) || y >= (1 << level))
        die("invalid manifest");
      int id = newNode(face, level, x, y);
      if (id < 0)
        die("manifest too large");
      nodes[id].center = v3(cx, cy, cz);
    }
    if (!feof(f))
      die("malformed manifest");
    fclose(f);
  } else {
    for (int i = 0; i < 6; i++)
      newNode(i, 0, 0, 0);
    for (int pass = 0; pass < 4; pass++) {
      planCover();
      int ignored = 0;
      for (int i = 0; i < 6; i++)
        preloadCount(i, &ignored, 1);
    }
    for (int i = 0; i < countNode; i++)
      ramPush(i);
    while ((size_t)countNode * (TERRAIN_BYTES + NV * sizeof(Vertex)) <
               (size_t)mib * 1048576u &&
           ramHeapCount && countNode + 4 <= MAXNODE) {
      Node *n = &nodes[ramPop()];
      for (int j = 0; j < 4; j++) {
        int id = newNode(n->face, n->level + 1, n->x * 2 + (j & 1),
                         n->y * 2 + (j >> 1));
        n->child[j] = id;
        ramPush(id);
      }
    }
  }
  if (manifestOut) {
    FILE *f = fopen(manifestOut, "w");
    if (!f)
      die("manifest write");
    for (int i = 0; i < countNode; i++) {
      Node *n = &nodes[i];
      fprintf(f, "%d %d %d %d %.9g %.9g %.9g\n", n->face, n->level, n->x, n->y,
              n->center.x, n->center.y, n->center.z);
    }
    fclose(f);
  }
  printf("BAKER threads=%d pages=%d terrain_vertex=%zu nature_vertex=%zu "
         "estimated_mib=%.1f\n",
         threads, countNode, sizeof(Vertex), sizeof(NaturePacked),
         countNode * (TERRAIN_BYTES + NV * sizeof(Vertex)) / 1048576.);
  if (!planOnly) {
    Uint32 start = SDL_GetTicks();
    pthread_t pool[32];
    for (int i = 0; i < threads; i++)
      if (pthread_create(&pool[i], NULL, bakeWorker, NULL))
        die("baker thread");
    for (int i = 0; i < threads; i++)
      pthread_join(pool[i], NULL);
    if (plants) {
      initTreeModels();
      static NatureCandidate candidates[NATURE_CANDIDATES];
      int count = natureCandidates(norm(eye), candidates);
      for (int i = 0; i < count; i++)
        for (int lod = candidates[i].distance < 150 ? 0 : candidates[i].distance < 440 ? 2 : 3; lod < 4; lod++) {
          NatureCell c = {0};
          c.face = candidates[i].face;
          c.x = candidates[i].x;
          c.y = candidates[i].y;
          c.buildLod = lod;
          if(!buildNature(&c))die("offline nature payload allocation");
          free(c.packed);
        }
      printf("BAKER nature=%d cells (near: 4 LODs, mid: LOD2/3, far: LOD3)\n", count);
    }
    /* Same generator and native payload as the executable, no GL context. */
    moonBaking=1;
    for(int altitude=0;altitude<4;altitude++) {
      float heights[4]={2.5f,500,5000,50000};V3 d=norm(mul(moonCenter(),-1));
      cameraEye=add(moonCenter(),mul(d,MOON_RADIUS+moonHeight(d)+heights[altitude]));
      moonPlanning=1;moonLodScale=.9f;
      do {moonDraws=0;for(int face=0;face<6;face++)moonNode(face,0,0,0);if(moonDraws<=220)break;moonLodScale*=.85f;}while(moonLodScale>.1f);
      moonPlanning=0;moonDraws=0;for(int face=0;face<6;face++)moonNode(face,0,0,0);
    }
    moonBaking=0;printf("BAKER moon_generated=%d\n",moonBuilds);
    printf("BAKER completed seconds=%.3f\n", (SDL_GetTicks() - start) / 1000.);
  }
  cacheClose();
  SDL_Quit();
  return 0;
}
