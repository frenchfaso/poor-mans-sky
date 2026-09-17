// SPDX-License-Identifier: MPL-2.0
/* Run normally and with --immediate: STATE lines must match byte for byte.
 * Count actual driver uploads; CPU source equality alone misses stale VBOs. */
#define _DARWIN_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#define SDL_MAIN_HANDLED
#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <stdio.h>
static Uint32 testClock = 1000;
static Uint32 fixedTicks(void) { return testClock; }
static GLuint watched[4];
static int writes[4];
static void countedSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void *data) {
  GLint bound = 0;
  glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &bound);
  for (int i = 0; i < 4; i++)
    if (watched[i] == (GLuint)bound) writes[i]++;
  glBufferSubData(target, offset, size, data);
}
#define SDL_GetTicks fixedTicks
#define glBufferSubData countedSubData
#define main poor_mans_sky_application_main
#include "../src/poor-mans-sky.c"
#undef main
#undef glBufferSubData
#undef SDL_GetTicks
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL upload %d: %s\n",__LINE__,#x); return 1; } } while (0)
int main(int argc, char **argv) {
  int immediate = argc > 1 && !strcmp(argv[1], "--immediate");
  CHECK(!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER));
  window = SDL_CreateWindow("Morph uploads",0,0,64,64,SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  CHECK(window); context = SDL_GL_CreateContext(window); CHECK(context);
  mutex = SDL_CreateMutex(); materialInit(); cacheEnabled = geological = 0;
  cullingMode = 1; /* Zero frustum planes and camera at origin: all test patches visible. */
  for (int i = 0; i < MAXNODE; i++) previousIndex[i] = -1;
  for (int i = 0; i < MORPH_SLOTS; i++) lodMorphs[i].id = -1;
  morphInitialized = 1;
  for (int f = 0; f < 6; f++) newNode(f,0,0,0);
  for (int i = 0; i < 4; i++) {
    int id = newNode(4,1,i&1,i>>1);
    Node *n = &nodes[id]; nodes[4].child[i] = id;
    generate(n,&n->pixels,&n->vertices);
    n->slot = i; n->meshLevel = 3;
    glGenBuffers(1,&n->vbo); watched[i] = n->vbo;
    glBindBuffer(GL_ARRAY_BUFFER,n->vbo);
    glBufferData(GL_ARRAY_BUFFER,NV*sizeof(Vertex),n->vertices,GL_STATIC_DRAW);
    measureBounds(n);
    previousCover[i] = id; previousLevels[i] = 3; previousIndex[id] = i;
  }
  previousCount = 4;
  /* Refinement with seams; removal of seams; coarsening; cover exit/re-entry;
   * completion and immediate reuse of a morph slot; unchanged final cover. */
  const int poses[][4] = {
    {0,0,3,0},{225,0,3,0},{500,0,3,0},
    {550,0,0,0},{775,0,0,0},{1050,0,0,0},
    {1100,3,0,0},{1325,3,0,0},{1600,3,0,0},
    {1650,0,3,0},{1750,0,3,1},{1850,0,3,0},
    {2400,0,3,0},{2450,0,3,0}
  };
  int total = 0, doubled = 0;
  for (unsigned p = 0; p < sizeof(poses)/sizeof(poses[0]); p++) {
    frameNo++; testClock = 1000 + poses[p][0]; selectedCount = 0;
    for (int i = 0; i < 4; i++) {
      Node *n = &nodes[6+i]; n->meshLevel = poses[p][i ? 2 : 1];
      if (i == 0 && poses[p][3]) continue;
      selected[selectedCount++] = 6+i; n->used = frameNo;
    }
    memset(writes,0,sizeof(writes));
    if (immediate) { prepareMorphs(); stitchTerrainEdges(); }
    else prepareTerrainGeometry();
    CHECK(!pendingMorphCount && !deferMorphUploads);
    uint32_t hash = 2166136261u;
    for (int i = 0; i < 4; i++) {
      Node *n = &nodes[6+i]; Vertex gpu[NV];
      glBindBuffer(GL_ARRAY_BUFFER,n->vbo);
      glGetBufferSubData(GL_ARRAY_BUFFER,0,sizeof(gpu),gpu);
      hash = cacheHash(gpu,sizeof(gpu),hash);
      hash = cacheHash(&n->boundCenter,sizeof(n->boundCenter),hash);
      hash = cacheHash(&n->boundHalf,sizeof(n->boundHalf),hash);
      hash = cacheHash(&n->meshLevel,sizeof(n->meshLevel),hash);
      if (n->used == frameNo) {
        int m = morphFor(6+i);
        const Vertex *expected = n->stitched ? n->stitched : m >= 0 ? lodMorphs[m].current : n->vertices;
        CHECK(!memcmp(gpu,expected,sizeof(gpu)));
      }
      if (!immediate) CHECK(writes[i] <= 1);
      total += writes[i]; doubled += writes[i] > 1;
    }
    CHECK(glGetError() == GL_NO_ERROR);
    printf("STATE pose=%u hash=%08x morphs=%d\n",p,hash,morphActive);
  }
  if (immediate) CHECK(doubled > 0);
  printf("PASS uploads=%d duplicate_buffers=%d bytes=%zu mode=%s\n",total,doubled,(size_t)total*NV*sizeof(Vertex),immediate ? "immediate" : "deferred");
  SDL_GL_DeleteContext(context); SDL_DestroyWindow(window); SDL_Quit();
  return 0;
}
