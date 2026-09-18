// SPDX-License-Identifier: MPL-2.0
#include "ram-preload.h"
/* Preload the real concentric cover; cache pages retain their existing format.
 */
static void loadingScreen(int ready,int total,int plants,int plantTotal) {
  float ratio=total+plantTotal?(ready+plants)/(float)(total+plantTotal):1;
  char text[120];snprintf(text,sizeof(text),"TERRAIN %d/%d / VEGETATION %d/%d",ready,total,plants,plantTotal);
  bootProgress(.85f+.08f*ratio,"LOADING OR GENERATING SCENE",text);
}
/* cacheInit runs before workers/startup preloading; keep the window responsive
 * while indexing an existing disk cache, including large/full indexes. */
static void cacheLoadingProgress(void) {
  static Uint32 last;Uint32 now=SDL_GetTicks();
  if(last && now-last<100)return;
  last=now;SDL_Event event;
  while(SDL_PollEvent(&event)) {
    if(event.type==SDL_QUIT || (event.type==SDL_KEYDOWN && event.key.keysym.sym==SDLK_ESCAPE)) {
      SDL_Quit();exit(0);
    }
  }
  char text[100];snprintf(text,sizeof(text),"%d PACKS / %d ENTRIES",packCount,entryCount);
  bootProgress(.02f,"INDEXING DISK CACHE",text);
}
static int preloadCount(int id, int *ready, int expand) {
  Node *n = &nodes[id];
  if (!residentRegion(n))
    return 0;
  if (wantsSplit(n)) {
    if (expand && n->child[0] < 0 &&
        (countNode + 4 <= MAXNODE || freeCount >= 4))
      for (int j = 0; j < 4; j++)
        n->child[j] = newNode(n->face, n->level + 1, n->x * 2 + (j & 1),
                              n->y * 2 + (j >> 1));
    if (n->child[0] >= 0) {
      int count = 0;
      for (int j = 0; j < 4; j++)
        count += preloadCount(n->child[j], ready, expand);
      return count;
    }
  }
  *ready += n->slot >= 0;
  return 1;
}
static int preloadWorld(void) {
  Uint32 start = SDL_GetTicks(), lastDraw = 0, lastLog = 0;
  int running = 1, complete = 0, ready = 0, total = 0, plants = 0,
      plantTotal = 0;
  preloading = 1;
  /* Materialize the desired tree before reporting its work count. */
  SDL_LockMutex(mutex);
  for (int pass = 0; pass < 4; pass++) {
    planCover();
    int ignored = 0;
    for (int i = 0; i < 6; i++)
      preloadCount(i, &ignored, 1);
  }
  SDL_UnlockMutex(mutex);
  printf("PRELOAD start RAM_limit=1024MiB detail=%.0fx\n", terrainDetail);
  fflush(stdout);
  while (!complete) {
    SDL_Event event;
    int skip = 0;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT)
        running = 0;
      if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)
        running = 0;
      if ((event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_RETURN) ||
          (event.type == SDL_CONTROLLERBUTTONDOWN &&
           event.cbutton.button == SDL_CONTROLLER_BUTTON_START))
        skip = 1;
    }
    if (!running || skip)
      break;
    streamUpdate();
    natureUpdate();
    ready = total = plants = plantTotal = 0;
    SDL_LockMutex(mutex);
    for (int i = 0; i < 6; i++)
      total += preloadCount(i, &ready, 0);
    frameNo++;
    SDL_UnlockMutex(mutex);
    SDL_LockMutex(natureMutex);
    for (int i = 0; i < NATURE_CELLS; i++)
      if (nature[i].state && nature[i].wanted == natureFrame) {
        plantTotal++;
        plants += nature[i].state == 4;
      }
    SDL_UnlockMutex(natureMutex);
    complete = ready == total && plants == plantTotal;
    Uint32 now = SDL_GetTicks();
    if (now - start - lastLog >= 10000) {
      printf("PRELOAD progress seconds=%.1f terrain=%d/%d plants=%d/%d\n",
             (now - start) / 1000., ready, total, plants, plantTotal);
      lastLog = now - start;
    }
    if (now - lastDraw >= 100 || complete) {
      loadingScreen(ready, total, plants, plantTotal);
      lastDraw = now;
    }
    /* Keep entry possible if the residency budget cannot satisfy this view. */
    if (now - start > 180000)
      break;
    SDL_Delay(10);
  }
  if (running && complete)
    running = ramPreloadWorld();
  preloading = 0;
  SDL_LockMutex(mutex);
  for (int i = 0; i < countNode; i++) {
    nodes[i].wanted -= frameNo;
    nodes[i].used -= frameNo;
  }
  frameNo = 0;
  SDL_UnlockMutex(mutex);
  printf("PRELOAD %s seconds=%.3f terrain=%d/%d vegetation=%d/%d RAM=%.1fMiB\n",
         complete ? "complete" : "partial", (SDL_GetTicks() - start) / 1000.0,
         ready, total, plants, plantTotal, ramBytes / 1048576.0);
  fflush(stdout);
  return running;
}
