// SPDX-License-Identifier: MPL-2.0
#include "ram-preload.h"
/* Preload the real concentric cover; cache pages retain their existing format.
 */
static const char *loadingStage;
static void loadingScreen(int ready, int total, int plants, int plantTotal) {
  float progress = (ready + plants) / (float)(total + plantTotal + 1);
  if (ready == total && plants == plantTotal)
    progress = 1;
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glViewport(0, 0, width, height);
  glUseProgram(0);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glDisable(GL_BLEND);
  glActiveTexture(GL_TEXTURE0);
  glDisable(GL_TEXTURE_2D);
  glClearColor(.015f, .025f, .04f, 1);
  glClear(GL_COLOR_BUFFER_BIT);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, width, height, 0, -1, 1);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  float left = width * .15f, top = height * .48f, span = width * .7f;
  glColor3f(.13f, .18f, .22f);
  glBegin(GL_QUADS);
  glVertex2f(left, top);
  glVertex2f(left + span, top);
  glVertex2f(left + span, top + 18);
  glVertex2f(left, top + 18);
  glEnd();
  glColor3f(.25f, .75f, .85f);
  glBegin(GL_QUADS);
  glVertex2f(left, top);
  glVertex2f(left + span * progress, top);
  glVertex2f(left + span * progress, top + 18);
  glVertex2f(left, top + 18);
  glEnd();
  glColor3f(.86f, .94f, .97f);
  label(left, top - 58, "POOR MAN'S SKY / LOADING", 2);
  char text[160];
  snprintf(text, sizeof(text), "TERRAIN %d/%d | VEGETATION %d/%d", ready, total,
           plants, plantTotal);
  if(loadingStage)snprintf(text,sizeof(text),"%s / %d PACKS / %d ENTRIES",loadingStage,packCount,entryCount);
  label(left, top + 35, text, 1.3f);
  SDL_LockMutex(diskMutex);
  unsigned long long hits = diskHits[1] + diskHits[2],
                     fresh = diskMisses[1] + diskMisses[2];
  SDL_UnlockMutex(diskMutex);
  snprintf(text, sizeof(text), "FROM DISK CACHE %llu | NEW ASSETS %llu", hits,
           fresh);
  label(left, top + 62, text, 1);
  label(left, top + 84,
        loadingStage ? "READING SAVED WORLD / ESC TO QUIT" :
        fresh ? "BUILDING NEW ASSETS / ENTER TO SKIP"
              : "LOADING CACHE INTO RAM AND GPU / ENTER TO SKIP",
        1);
  SDL_GL_SwapWindow(window);
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
  loadingScreen(0,1,0,0);
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
