// SPDX-License-Identifier: MPL-2.0
#define _DARWIN_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#define SDL_MAIN_HANDLED
#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <dlfcn.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#define PI 3.14159265358979323846f
#define RADIUS 200000.0f
#define PAGE 128
#define ATLASES 4
#define SLOTS (256 * ATLASES)
#define PAGE_BYTES (PAGE * PAGE / 2)
#define MAXNODE 65536
#define MAXLEVEL 20
#define PATCH 24
#define NV ((PATCH + 1) * (PATCH + 1) + 4 * (PATCH + 1))
#define RAM_PRELOAD (512u * 1024u * 1024u)
typedef struct {
  float x, y, z;
} V3;
typedef struct {
  GLuint tex, fbo, depth;
  int w, h;
} Target;
typedef struct { int8_t x,y,z,pad; } PackedNormal;
typedef struct {
  V3 p; PackedNormal n;
  float u, v, h;
} Vertex;
typedef struct {
  int face, level, x, y, child[4], state, slot, wanted, used;
  GLuint vbo;
  V3 center;
  float size, minHeight, maxHeight, geomError[4];
  V3 boundCenter, boundHalf;
  float boundRadius, sortDistance, pixelScale;
  int visible;
  GLuint query;
  unsigned queryEpoch, testedEpoch;
  int queryPending, occluded;
  int split, meshLevel, streamBand;
  unsigned char *pixels;
  Vertex *vertices, *stitched;
  uint32_t edgeKey;
} Node;
static void *gameModeLib;
static int (*gameModeEnd)(void);
static SDL_Window *window;
static SDL_GLContext context;
static Target noiseMap, detailMap, scene, glow[2];
static GLuint skyCube, atlasTex[ATLASES], landP, waterP, skyP, postP, brightP,
    blurP, ebo[4];
static Node nodes[MAXNODE];
static int countNode = 0, owners[SLOTS], frameNo, drawn, triangles, uploaded,
           evicted, resident;
/* Main-thread geometry changes invalidate terrain/sea seams, not disk caches. */
static uint64_t terrainGeometryRevision;
static int freeNodes[MAXNODE], freeCount, recycleCursor = 6, recycledNodes;
static int readyIds[2048], readyCount;
static int queue[512], qhead, qtail, qcount, quitWorker;
static size_t ramBytes,moonRAMBytes;
static size_t ramPreloadTarget = RAM_PRELOAD;
static int ramPreloading;
static SDL_mutex *mutex;
static SDL_cond *cond;
static SDL_Thread *worker;
static int selected[1024], selectedCount, requestCount, ramHits;
static int width = 1024, height = 768, rw = 768, rh = 576,
           indexCount[4], mouse = 0, flying = 1, wire = 0, hud = 1, bloom = 1;
static SDL_DisplayMode displayModes[64];
static int displayModeCount, displayModeIndex, requestedWidth=640, requestedHeight=480;
static int fixedWindowed;
static SDL_GameController *pad;
static V3 velocity, cullForward, flightForward, flightUp, flightRates;
static float throttle, solarDaylight = 1, dayOffset = .12f, dayLength = 2400;
static float sceneExposure = 1;
static V3 ambientLight, sunLight;
static V3 fogColor = {.52f, .64f, .73f};
static uint32_t worldSeed = 20260911;
static int geological = 1;
static void cameraBasis(V3 *f, V3 *r, V3 *u);
static void prepareView(void);
static float cameraClearance;
static float jetFuel = 1, roll, lookX, lookY;
static int sprint, paused;
static int shaderWarmupEnabled=1,cloudsEnabled=1,overdrawView;
static GLuint overdrawP;
static GLint pageUniform, originUniform, landEyeUniform, waterEyeUniform,
    waterCenterUniform, waterOffsetUniform, waterPhaseUniform;
static V3 cameraEye, spawnPoint, viewForward, viewRight, viewUp;
static int spawnReady;
static int natureQuality = 1, performanceMode;
/* Worker priorities are runtime state, separate from serialized nature data. */
static float natureRequestPriority[65536];
static GLuint postQualityP, postPerformanceP, landPrograms[2][2];
static GLint landPageLocations[2][2], landOriginLocations[2][2],
    landEyeLocations[2][2], fogPlaneLocations[2][2];
static int cpuFogPatches, gpuFogPatches;
static unsigned lodRevision;
static int cullingMode = 1, profileFrames, occlusionEnabled = 1,
           batchingEnabled = 1;
static double generationMS;
static int generatedJobs;
static GLuint occlusionP;
static double profileTimes[6];
static Uint64 profileStamp;
static int profileSamples;
static float clipNear, clipFar;
static double worldDepthLo=.01,worldDepthHi=1;
static V3 eye, sun = {.6f, .5f, .6f}, heading;
static float pitch = -1.48f, clockTime, fps;
static size_t vramEstimate(void);
static int vramReserve(size_t incoming);
static int moonWorkPending(void); /* Called with the shared streaming mutex. */
static void moonStreamWork(void); /* Enters/leaves with that mutex held. */
static int moonReclaimRAM(void);
static void *streamAlloc(size_t bytes);
// clang-format off
#include "cache.h"
#include "engine-common.h"
#include "materials.h"
#include "stars.h"
#include "terrain-field.h"
#include "geology.h"
// clang-format on
/* Identical 3D field on all six faces, including page gutters. */
static V3 direction(int face, float u, float v) {
  switch (face) {
  case 0:
    return norm(v3(1, v, -u));
  case 1:
    return norm(v3(-1, v, u));
  case 2:
    return norm(v3(u, 1, -v));
  case 3:
    return norm(v3(u, -1, v));
  case 4:
    return norm(v3(u, v, 1));
  default:
    return norm(v3(-u, v, -1));
  }
}
#include "moon-field.h"
#include "space-view.h"
static V3 surfaceNormal(V3 d) {
  if (geological && geology && geologySeed == worldSeed)
    return geoNormal(d);
  V3 t = norm(cross(fabsf(d.y) < .9f ? v3(0, 1, 0) : v3(1, 0, 0), d)),
     b = cross(d, t);
  float eps = 1.5f / RADIUS;
  float dx = elevation(norm(add(d, mul(t, eps)))) -
             elevation(norm(add(d, mul(t, -eps))));
  float dy = elevation(norm(add(d, mul(b, eps)))) -
             elevation(norm(add(d, mul(b, -eps))));
  return norm(add(d, add(mul(t, -dx / (2 * eps * RADIUS)),
                         mul(b, -dy / (2 * eps * RADIUS)))));
}
static int newNode(int face, int level, int x, int y) {
  if (countNode == MAXNODE && !freeCount)
    return -1;
  int id = freeCount ? freeNodes[--freeCount] : countNode++;
  Node *n = &nodes[id];
  memset(n, 0, sizeof(*n));
  n->face = face;
  n->level = level;
  n->x = x;
  n->y = y;
  n->slot = -1;
  n->streamBand = -1;
  for (int i = 0; i < 4; i++)
    n->child[i] = -1;
  float s = 2.0f / (1 << level);
  V3 d = direction(face, -1 + (x + .5f) * s, -1 + (y + .5f) * s);
  n->center = mul(d, RADIUS + elevation(d));
  n->size = RADIUS * s;
  return id;
}
static V3 nodeDir(const Node *n, float u, float v) {
  float s = 2.0f / (1 << n->level);
  return direction(n->face, -1 + (n->x + u) * s, -1 + (n->y + v) * s);
}
/* BC1 opaque RGB, encoded once by the asset worker; native GPU decoding. */
static unsigned short rgb565(const unsigned char *p) {
  return ((p[0] >> 3) << 11) | ((p[1] >> 2) << 5) | (p[2] >> 3);
}
static void unpack565(unsigned short c, int *p) {
  p[0] = ((c >> 11) & 31) * 255 / 31;
  p[1] = ((c >> 5) & 63) * 255 / 63;
  p[2] = (c & 31) * 255 / 31;
}
#include "bc-codec.h"
static unsigned char *compressPage(const unsigned char *rgba) {
  unsigned char *out=malloc(PAGE_BYTES);if(!out)die("BC1 allocation");
  bcEncode(rgba,PAGE,PAGE,0,out);return out;
}
#include "terrain-bake.h"
typedef struct {
  V3 center, boundCenter, boundHalf;
  float boundRadius, minHeight, maxHeight, geomError[4];
} TerrainMeta;
#define TERRAIN_BYTES (PAGE_BYTES + sizeof(TerrainMeta))
static void measureGeometry(Node *n);
static void measureBounds(Node *n);
static void generate(const Node *n, unsigned char **pixels, Vertex **vertices) {
  unsigned char *p=streamAlloc(TERRAIN_BYTES);
  Vertex *v=streamAlloc(NV*sizeof(Vertex));
  if(!p||!v)die("cached terrain allocation");
  uint32_t bytes=0;
  if(cacheRead(1,n->face,n->level,n->x,n->y,p,TERRAIN_BYTES,v,NV*sizeof(Vertex),&bytes)
      && bytes==NV*sizeof(Vertex)) { *pixels=p;*vertices=v;return; }
  free(p);free(v);
  generateFresh(n,pixels,vertices);
  p=realloc(*pixels,TERRAIN_BYTES);if(!p)die("terrain metadata");*pixels=p;
  Node temp=*n;temp.vertices=*vertices;
  measureGeometry(&temp);measureBounds(&temp);
  TerrainMeta meta={temp.center,temp.boundCenter,temp.boundHalf,temp.boundRadius,
    temp.minHeight,temp.maxHeight,{0}};
  memcpy(meta.geomError,temp.geomError,sizeof(meta.geomError));
  memcpy(p+PAGE_BYTES,&meta,sizeof(meta));
  cacheWrite(1,n->face,n->level,n->x,n->y,p,TERRAIN_BYTES,*vertices,NV*sizeof(Vertex));
}
/* Called under the streaming mutex: publish the current camera and reprioritize
 * coarse coverage before near-to-far detail bands. */
#include "stream-view.h"
static int queueBorn[MAXNODE], streamPriorityFrame;
typedef struct { int id; float priority; } PendingRequest;
static PendingRequest pendingRequests[2048];
static int pendingCount, priorityReplacements;
static float streamDistancePriority(float distance,int level,float age) {
  int ring=distance<=10?0:distance<=25?1:distance<=50?2:distance<=100?3:4;
  return ring*4096+fminf(distance,4095)+level*.01f-age*32;
}
static float requestPriority(int id, int aging) {
  Node *n = &nodes[id];
  V3 d = add(streamPriorityEye, mul(n->center, -1));
  float distance = fmaxf(0, sqrtf(dot(d,d)) - n->size*.75f);
  float age = aging && !ramPreloading ? fmaxf(0, streamPriorityFrame-queueBorn[id]) : 0;
  if(!streamViewReady)return streamDistancePriority(distance,n->level,age);
  if(n->level<=2)return -65536+(n->level*8192)+distance*.001f;
  int band=streamBand(streamCenter(n),streamRadius(n),n->streamBand);
  return band*8192+fminf(distance/streamBubble*256,4095)+n->level*.01f-fminf(age,120)*8;
}
static int popPriorityRequest(void) {
  int best=qhead;
  float score=requestPriority(queue[best],1);
  for(int i=1;i<qcount;i++) {
    int at=(qhead+i)%512;
    float p=requestPriority(queue[at],1);
    if(p<score){score=p;best=at;}
  }
  int id=queue[best];queue[best]=queue[qhead];
  qhead=(qhead+1)%512;qcount--;
  return id;
}
static int compareRequests(const void *a,const void *b) {
  const PendingRequest *x=a,*y=b;
  if(x->priority!=y->priority)return x->priority<y->priority?-1:1;
  return (x->id>y->id)-(x->id<y->id);
}
static void dispatchRequests(void) {
  qsort(pendingRequests,pendingCount,sizeof(*pendingRequests),compareRequests);
  for(int i=0;i<pendingCount && requestCount<24;i++) {
    int id=pendingRequests[i].id;
    if(nodes[id].state!=0)continue;
    if(qcount==512) {
      int worst=qhead;float score=requestPriority(queue[worst],1);
      for(int j=1;j<qcount;j++) {
        int at=(qhead+j)%512;float p=requestPriority(queue[at],1);
        if(p>score){score=p;worst=at;}
      }
      if(requestPriority(id,0)>=score)continue;
      nodes[queue[worst]].state=0;queue[worst]=id;priorityReplacements++;
    } else {
      queue[qtail]=id;qtail=(qtail+1)%512;qcount++;
    }
    nodes[id].state=1;queueBorn[id]=frameNo;requestCount++;
  }
  if(qcount)SDL_CondSignal(cond);
}
/* Reclaim only after an allocation fails, rather than at a fixed RAM cap.
 * Called with mutex held; protected/current payloads remain pinned. */
static int reclaimTerrainRAM(void) {
  int victim=-1;
  for(int i=6;i<countNode;i++)
    if(nodes[i].pixels && nodes[i].state!=4 && nodes[i].wanted<frameNo-10 &&
       (victim<0 || nodes[i].used<nodes[victim].used))victim=i;
  if(victim<0)return 0;
  Node *n=&nodes[victim];free(n->pixels);free(n->vertices);
  if(n->stitched){free(n->stitched);n->stitched=NULL;ramBytes-=NV*sizeof(Vertex);}
  n->edgeKey=0;n->pixels=NULL;n->vertices=NULL;
  if(n->state==2)n->state=0;
  ramBytes-=TERRAIN_BYTES+NV*sizeof(Vertex);return 1;
}
static void *streamAlloc(size_t bytes) {
  void *p=malloc(bytes);
  if(p || !mutex)return p;
  SDL_LockMutex(mutex);
  while(!p) {
    int reclaimed=nearMoon(streamPriorityEye)?reclaimTerrainRAM():moonReclaimRAM();
    if(!reclaimed)reclaimed=nearMoon(streamPriorityEye)?moonReclaimRAM():reclaimTerrainRAM();
    if(!reclaimed)break;
    p=malloc(bytes);
  }
  SDL_UnlockMutex(mutex);return p;
}
static int streamWorker(void *unused) {
  (void)unused;
  SDL_SetThreadPriority(SDL_THREAD_PRIORITY_LOW);
  int lastMoon=0;
  for (;;) {
    SDL_LockMutex(mutex);
    while (!qcount && !moonWorkPending() && !quitWorker)
      SDL_CondWait(cond, mutex);
    if (quitWorker) {
      SDL_UnlockMutex(mutex);
      break;
    }
    /* One scheduler for both bodies; neither backlog can starve the other. */
    if(moonWorkPending() && (!qcount || !lastMoon)) {
      moonStreamWork();lastMoon=1;SDL_UnlockMutex(mutex);continue;
    }
    lastMoon=0;
    int id = popPriorityRequest();
    if (!ramPreloading && nodes[id].wanted < frameNo - 15) {
      nodes[id].state = 0;
      SDL_UnlockMutex(mutex);
      continue;
    }
    Node copy = nodes[id];
    SDL_UnlockMutex(mutex);
    unsigned char *p;
    Vertex *v;
    Uint64 generationStart = SDL_GetPerformanceCounter();
    generate(&copy, &p, &v);
    generationMS += (SDL_GetPerformanceCounter() - generationStart) * 1000.0 /
                    SDL_GetPerformanceFrequency();
    generatedJobs++;
    SDL_LockMutex(mutex);
    nodes[id].pixels = p;
    nodes[id].vertices = v;
    nodes[id].state = 2;
    ramBytes += TERRAIN_BYTES + NV * sizeof(Vertex);
    SDL_UnlockMutex(mutex);
  }
  return 0;
}
static void request(int id) {
  Node *n = &nodes[id];
  n->wanted = frameNo;
  if (n->slot >= 0)
    return;
  if (n->pixels) {
    n->state = 2;
    ramHits++;
    if (readyCount < 2048)
      readyIds[readyCount++] = id;
    return;
  }
  if (n->state == 0 && pendingCount < 2048)
    pendingRequests[pendingCount++] = (PendingRequest){id,requestPriority(id,0)};
}
static void measureGeometry(Node *n) {
  n->geomError[0] = 0;
  for (int lod = 1; lod < 4; lod++) {
    int step = 1 << lod;
    float worst = 0;
    for (int y = 0; y <= PATCH; y++)
      for (int x = 0; x <= PATCH; x++) {
        int bx = (x == PATCH ? x - step : x / step * step),
            by = (y == PATCH ? y - step : y / step * step);
        float u = (x - bx) / (float)step, v = (y - by) / (float)step;
        V3 a = n->vertices[by * (PATCH + 1) + bx].p,
           b = n->vertices[by * (PATCH + 1) + bx + step].p,
           c = n->vertices[(by + step) * (PATCH + 1) + bx].p,
           d = n->vertices[(by + step) * (PATCH + 1) + bx + step].p;
        V3 approx =
            u + v <= 1
                ? add(mul(a, 1 - u - v), add(mul(b, u), mul(c, v)))
                : add(mul(d, u + v - 1), add(mul(b, 1 - v), mul(c, 1 - u)));
        V3 error = add(n->vertices[y * (PATCH + 1) + x].p, mul(approx, -1));
        worst = fmaxf(worst, dot(error, error));
      }
    n->geomError[lod] = sqrtf(worst);
  }
}
static void measureBounds(Node *n);
static int upload(int id) {
  if(!vramReserve(NV*sizeof(Vertex)))return 0;
  SDL_LockMutex(mutex);
  Node *n = &nodes[id];
  if (!n->pixels || n->slot >= 0) {
    SDL_UnlockMutex(mutex);
    return 0;
  }
  int slot = -1, oldest = frameNo - 4;
  for (int s = 0; s < SLOTS; s++) {
    int o = owners[s];
    if (o < 0) {
      slot = s;
      break;
    }
    if (o >= 6 && nodes[o].wanted < oldest) {
      oldest = nodes[o].wanted;
      slot = s;
    }
  }
  if (slot < 0) {
    SDL_UnlockMutex(mutex);
    return 0;
  }
  n->state = 4;
  int old = owners[slot];
  GLuint oldQuery = 0, oldVBO = 0;
  if (old >= 0) {
    oldQuery = nodes[old].query;
    oldVBO = nodes[old].vbo;
    nodes[old].query = 0;
    nodes[old].queryPending = 0;
    nodes[old].testedEpoch = 0;
    nodes[old].vbo = 0;
    nodes[old].slot = -1;
    nodes[old].state = nodes[old].pixels ? 2 : 0;
    evicted++;
  } else
    resident++;
  owners[slot] = id;
  terrainGeometryRevision++;
  SDL_UnlockMutex(mutex);
  if (oldQuery)
    glDeleteQueries(1, &oldQuery);
  if (oldVBO)
    glDeleteBuffers(1, &oldVBO);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, atlasTex[slot / 256]);
  glCompressedTexSubImage2D(
      GL_TEXTURE_2D, 0, (slot % 16) * PAGE, ((slot % 256) / 16) * PAGE, PAGE,
      PAGE, GL_COMPRESSED_RGB_S3TC_DXT1_EXT, PAGE_BYTES, n->pixels);
  TerrainMeta meta;memcpy(&meta,n->pixels+PAGE_BYTES,sizeof(meta));
  n->center=meta.center;n->boundCenter=meta.boundCenter;n->boundHalf=meta.boundHalf;
  n->boundRadius=meta.boundRadius;n->minHeight=meta.minHeight;n->maxHeight=meta.maxHeight;
  memcpy(n->geomError,meta.geomError,sizeof(meta.geomError));
  glGenBuffers(1, &n->vbo);
  glBindBuffer(GL_ARRAY_BUFFER, n->vbo);
  glBufferData(GL_ARRAY_BUFFER, NV * sizeof(Vertex), n->vertices,
               GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  SDL_LockMutex(mutex);
  n->slot = slot;
  n->state = 3;
  n->edgeKey = 0;
  owners[slot] = id;
  uploaded++;
  SDL_UnlockMutex(mutex);
  return 1;
}
/* Residency uses concentric distance rings, never the camera direction. */
static void recycleNodes(void) {
  if (countNode < MAXNODE * 3 / 4)
    return;
  for (int work = 0; work < 512; work++) {
    if (recycleCursor >= countNode)
      recycleCursor = 6;
    Node *p = &nodes[recycleCursor++];
    if (p->level < 0 || p->child[0] < 0)
      continue;
    int safe = 1;
    for (int k = 0; k < 4; k++) {
      Node *c = &nodes[p->child[k]];
      if (c->child[0] >= 0 || c->slot >= 0 || c->state == 1 || c->state == 4 ||
          c->wanted >= frameNo - 180 || c->used >= frameNo - 180)
        safe = 0;
    }
    if (!safe)
      continue;
    for (int k = 0; k < 4; k++) {
      int id = p->child[k];
      Node *c = &nodes[id];
      if (c->pixels)
        ramBytes -= TERRAIN_BYTES + NV * sizeof(Vertex);
      free(c->pixels);
      free(c->vertices);
      if (c->stitched) {
        free(c->stitched);
        ramBytes -= NV * sizeof(Vertex);
      }
      memset(c, 0, sizeof(*c));
      c->level = -1;
      c->slot = -1;
      for (int j = 0; j < 4; j++)
        c->child[j] = -1;
      freeNodes[freeCount++] = id;
      p->child[k] = -1;
      recycledNodes++;
    }
  }
}
static int residentRegion(const Node *n) {
  if (n->level < 2)
    return 1;
  return dot(norm(n->center), eye) > RADIUS - 6500 - n->size * 1.5f;
}
static int legacyVisible(const Node *n) {
  V3 delta = add(n->center, mul(cameraEye, -1));
  return dot(delta, cullForward) >
         sqrtf(dot(delta, delta)) * .55f - n->size * 1.6f - 32;
}
#include "visibility.h"
static float ringScale = 1;
static int desiredTiles, preloading;
static float terrainDetail = 1, textureDetail = 1;
static int wantsSplit(Node *n) {
  V3 delta = add(streamViewReady?streamPriorityEye:eye, mul(n->center, -1));
  float dist = sqrtf(dot(delta, delta));
  /* Distance bands cap page refinement; hysteresis protects their boundaries.
   * Outside the cone a coarse covering mesh always remains available. */
  float density = ringScale * (1 + 1 / (1 + dist / 50));
  float minimum=8;
  if(streamViewReady) {
    static const float floors[6]={8,16,64,256,1024,2048};
    static const float scales[6]={1,1,.85f,.7f,.6f,.12f};
    n->streamBand=streamBand(streamCenter(n),streamRadius(n),n->streamBand);
    minimum=floors[n->streamBand];density*=scales[n->streamBand];
  }
  n->split = n->level < MAXLEVEL && n->size > minimum &&
             dist < n->size * (n->split ? 1.65f : 1.50f) * density;
  return n->split;
}
static int estimateCover(int id) {
  Node *n = &nodes[id];
  if (!residentRegion(n))
    return 0;
  if (!wantsSplit(n))
    return 1;
  if (n->child[0] < 0)
    return 4;
  int total = 0;
  for (int j = 0; j < 4; j++)
    total += estimateCover(n->child[j]);
  return total;
}
static void planCover(void) {
  if (desiredTiles < 700)
    ringScale = fminf(1, ringScale + .002f);
  for (int attempt = 0; attempt < 32; attempt++) {
    desiredTiles = 0;
    for (int i = 0; i < 6; i++)
      desiredTiles += estimateCover(i);
    if (desiredTiles <= 940)
      break;
    ringScale *= .96f;
  }
}
static int hasCoverage(int id) {
  Node *n = &nodes[id];
  if (!residentRegion(n))
    return 1;
  if (n->slot >= 0)
    return 1;
  if (n->child[0] < 0)
    return 0;
  for (int j = 0; j < 4; j++)
    if (!hasCoverage(n->child[j]))
      return 0;
  return 1;
}
static void selectNode(int id) {
  Node *n = &nodes[id];
  if (!residentRegion(n))
    return;
  if (wantsSplit(n) &&
      (n->child[0] >= 0 || countNode + 4 <= MAXNODE || freeCount >= 4)) {
    if (n->child[0] < 0)
      for (int j = 0; j < 4; j++)
        n->child[j] = newNode(n->face, n->level + 1, n->x * 2 + (j & 1),
                              n->y * 2 + (j >> 1));
    int ready = 1;
    for (int j = 0; j < 4; j++) {
      if (!residentRegion(&nodes[n->child[j]]))
        continue;
      if (!hasCoverage(n->child[j])) {
        request(n->child[j]);
        ready = 0;
      }
    }
    if (ready) {
      for (int j = 0; j < 4; j++)
        selectNode(n->child[j]);
      return;
    }
  }
  request(id);
  /* Keep the existing finer cover until the requested coarse tile is uploaded.
     Ancestors only descend here after hasCoverage proved all branches ready. */
  if (n->slot < 0 && n->child[0] >= 0) {
    for (int j = 0; j < 4; j++)
      selectNode(n->child[j]);
    return;
  }
  if (n->slot >= 0 && selectedCount < 1024) {
    selected[selectedCount++] = id;
    n->used = frameNo;
  }
}
static void prefetchAhead(void) {
  float speed = sqrtf(dot(velocity, velocity));
  if (speed < 12)
    return;
  V3 future = add(eye, mul(velocity, fminf(.6f, 2000 / speed))),
     d = norm(future);
  float a = fabsf(d.x), b = fabsf(d.y), c = fabsf(d.z), u, v;
  int face;
  if (a >= b && a >= c) {
    face = d.x > 0 ? 0 : 1;
    u = (d.x > 0 ? -d.z : d.z) / a;
    v = d.y / a;
  } else if (b >= c) {
    face = d.y > 0 ? 2 : 3;
    u = d.x / b;
    v = (d.y > 0 ? -d.z : d.z) / b;
  } else {
    face = d.z > 0 ? 4 : 5;
    u = (d.z > 0 ? d.x : -d.x) / c;
    v = d.y / c;
  }
  float target = fmaxf(48, speed * .3f);
  int id = face;
  for (int depth = 0; depth < MAXLEVEL; depth++) {
    Node *n = &nodes[id];
    if (n->size <= target)
      break;
    if (n->child[0] < 0) {
      if (countNode + 4 > MAXNODE && freeCount < 4)
        break;
      for (int j = 0; j < 4; j++)
        n->child[j] = newNode(face, n->level + 1, n->x * 2 + (j & 1),
                              n->y * 2 + (j >> 1));
    }
    int scale = 1 << (n->level + 1),
        x = (int)clampf((u + 1) * .5f * scale, 0, scale - 1),
        y = (int)clampf((v + 1) * .5f * scale, 0, scale - 1);
    id = n->child[(x & 1) + 2 * (y & 1)];
  }
  request(id);
}
static int readyPriority(const void *a,const void *b) {
  float x=requestPriority(*(const int*)a,0),y=requestPriority(*(const int*)b,0);
  return x<y?-1:x>y;
}
static void streamUpdate(void) {
  prepareView();
  SDL_LockMutex(mutex);
  streamViewSetup();
  recycleNodes();
  selectedCount = requestCount = readyCount = pendingCount = 0;
  streamPriorityFrame = frameNo;
  planCover();
  for (int i = 0; i < 6; i++)
    selectNode(i);
  if (!preloading && !streamViewReady)
    prefetchAhead();
  dispatchRequests();
  qsort(readyIds,readyCount,sizeof(*readyIds),readyPriority);
  SDL_UnlockMutex(mutex);
  Uint64 uploadStart = SDL_GetPerformanceCounter();
  int budget = preloading ? 8 : 4, bytes = 0;
  for (int i = 0; i < readyCount && budget; i++) {
    if (upload(readyIds[i])) {
      budget--;
      bytes += TERRAIN_BYTES + NV * sizeof(Vertex);
    }
    if (!preloading &&
        (bytes >= 128 * 1024 || (SDL_GetPerformanceCounter() - uploadStart) *
                                        1000.0 / SDL_GetPerformanceFrequency() >
                                    3))
      break;
  }
  SDL_LockMutex(mutex);
  /* Release stale mesh buffers as well as slots; CPU and disk data survive. */
  int retired = 0;
  for (int slot = 6; slot < SLOTS && retired < 8; slot++) {
    int id = owners[slot];
    if (id >= 6 && nodes[id].wanted < frameNo - 90 &&
        resident > desiredTiles + 48) {
      if (nodes[id].query)
        glDeleteQueries(1, &nodes[id].query);
      nodes[id].query = 0;
      nodes[id].queryPending = 0;
      nodes[id].testedEpoch = 0;
      glDeleteBuffers(1, &nodes[id].vbo);
      nodes[id].vbo = 0;
      nodes[id].slot = -1;
      nodes[id].state = nodes[id].pixels ? 2 : 0;
      owners[slot] = -1;
      resident--;
      retired++;
    }
  }
  SDL_UnlockMutex(mutex);
}
static void initWorld(void) {
  mutex = SDL_CreateMutex();
  cond = SDL_CreateCond();
  if (!mutex || !cond)
    die("stream synchronization");

  for (int i = 0; i < SLOTS; i++)
    owners[i] = -1;
  const char *ext = (const char *)glGetString(GL_EXTENSIONS);
  if (!strstr(ext, "GL_EXT_texture_compression_s3tc"))
    die("S3TC required");
  glGenTextures(ATLASES, atlasTex);
  unsigned char *empty = calloc(1, 2048 * 2048 / 2);
  if (!empty)
    die("atlas allocation");
  for (int i = 0; i < ATLASES; i++) {
    glBindTexture(GL_TEXTURE_2D, atlasTex[i]);
    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGB_S3TC_DXT1_EXT,
                           2048, 2048, 0, 2048 * 2048 / 2, empty);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  }
  free(empty);
  printf("CACHE BC1 %d pages, %d MiB texture + %.1f MiB maximum mesh\n", SLOTS,
         ATLASES * 2, SLOTS * NV * sizeof(Vertex) / 1048576.0);
  unsigned short ix[PATCH * PATCH * 6 + PATCH * 24];
  for (int lod = 0; lod < 4; lod++) {
    int stride = 1 << lod, k = 0;
    for (int y = 0; y < PATCH; y += stride)
      for (int x = 0; x < PATCH; x += stride) {
        int a = y * (PATCH + 1) + x, b = a + stride * (PATCH + 1);
        ix[k++] = a;
        ix[k++] = a + stride;
        ix[k++] = b;
        ix[k++] = a + stride;
        ix[k++] = b + stride;
        ix[k++] = b;
      }
    int base = (PATCH + 1) * (PATCH + 1);
    for (int e = 0; e < 4; e++)
      for (int j = 0; j < PATCH; j += stride) {
        int a = e == 0   ? j
                : e == 1 ? j * (PATCH + 1) + PATCH
                : e == 2 ? PATCH * (PATCH + 1) + PATCH - j
                         : (PATCH - j) * (PATCH + 1);
        int b = e == 0   ? a + stride
                : e == 1 ? a + stride * (PATCH + 1)
                : e == 2 ? a - stride
                         : a - stride * (PATCH + 1);
        int sa = base + e * (PATCH + 1) + j;
        ix[k++] = a;
        ix[k++] = sa;
        ix[k++] = b;
        ix[k++] = sa;
        ix[k++] = sa + stride;
        ix[k++] = b;
      }
    indexCount[lod] = k;
    glGenBuffers(1, &ebo[lod]);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo[lod]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, k * sizeof(*ix), ix, GL_STATIC_DRAW);
  }
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
  for (int f = 0; f < 6; f++) {
    int id = newNode(f, 0, 0, 0);
    generate(&nodes[id], &nodes[id].pixels, &nodes[id].vertices);
    nodes[id].state = 2;
    ramBytes += TERRAIN_BYTES + NV * sizeof(Vertex);
    upload(id);
  }
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  (void)moonHeight(v3(0,1,0)); /* Publish immutable crater catalog before worker starts. */
  worker = SDL_CreateThread(streamWorker, "asset-stream", NULL);
  if (!worker)
    die(SDL_GetError());
  checkGL("planet and cache initialization");
}
static void initMaps(void) {
  GLuint p = program("bake.vert", "noise.frag");
  noiseMap = target(256, 256, 0, 1);
  bake(noiseMap, p);
  glDeleteProgram(p);
  p = program("bake.vert", "detail.frag");
  detailMap = target(512, 512, 0, 1);
  glUseProgram(p);
  tex(p, "noiseTex", 0, noiseMap.tex);
  bake(detailMap, p);
  mipmaps(detailMap);
  if (strstr((const char *)glGetString(GL_EXTENSIONS),
             "GL_EXT_texture_filter_anisotropic")) {
    glBindTexture(GL_TEXTURE_2D, detailMap.tex);
    glTexParameterf(GL_TEXTURE_2D, 0x84FE, 2.0f);
  }
  glDeleteProgram(p);
  initStars();
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
static V3 rotateAround(V3 v, V3 axis, float a) {
  axis = norm(axis);
  return add(add(mul(v, cosf(a)), mul(cross(axis, v), sinf(a))),
             mul(axis, dot(axis, v) * (1 - cosf(a))));
}
static void basis(V3 *f, V3 *r, V3 *u) {
  if (flying) {
    *f = norm(flightForward);
    *r = norm(cross(*f, flightUp));
    *u = cross(*r, *f);
    return;
  }
  V3 radial = bodyUp(eye);
  heading = norm(add(heading, mul(radial, -dot(heading, radial))));
  *f = norm(add(mul(heading, cosf(pitch)), mul(radial, sinf(pitch))));
  *r = norm(cross(*f, radial));
  *u = cross(*r, *f);
  if (flying) {
    V3 right = *r, up = *u;
    *r = add(mul(right, cosf(roll)), mul(up, sinf(roll)));
    *u = add(mul(up, cosf(roll)), mul(right, -sinf(roll)));
  }
}
/* Pitch the thrust forward as the player looks down, retaining enough lift.
 * Looking up returns to vertical thrust instead of pushing backward. */
static V3 jetpackDirection(V3 radial, V3 forward, float lookPitch) {
  float tilt = clampf(-lookPitch, 0, .96f);
  return add(mul(radial, cosf(tilt)), mul(forward, sinf(tilt)));
}
static void cameraBasis(V3 *f, V3 *r, V3 *u) {
  basis(f, r, u);
  if (flying) {
    *f = rotateAround(*f, *u, -lookX);
    *r = norm(cross(*f, *u));
    *f = rotateAround(*f, *r, lookY);
    *u = norm(cross(*r, *f));
  }
}
/* Normal flight has a bounded acceleration and a precise trigger response.
 * Boost retains the high-speed servo; leaving it sheds excess speed quickly. */
static V3 assistedFlight(V3 current, V3 forward, float drive, int boostMode,
                         int braking, float step) {
  float limit=boostMode==2?20000.0f:boostMode==1?2000.0f:120.0f;
  float input=boostMode?drive:drive*fabsf(drive);
  V3 target=mul(forward,braking?0:input*limit);
  float speed=sqrtf(dot(current,current));
  V3 delta=mul(add(target,mul(current,-1)),1-expf(-step*(braking?8.0f:1.5f)));
  if(!boostMode) {
    float acceleration=speed>240?6000:braking?120:
      sqrtf(dot(target,target))<speed?45:25;
    float length=sqrtf(dot(delta,delta)),maximum=acceleration*step;
    if(length>maximum)delta=mul(delta,maximum/length);
  }
  return add(current,delta);
}
static V3 homeDirection(void) {
  if (geological && geology) {
    V3 best = norm(v3(.7f, .25f, .5f));
    float score = -1e9;
    for (int y = GEO_H / 3; y < GEO_H * 2 / 3; y++)
      for (int x = 0; x < GEO_W; x++) {
        GeoCell c = geology[y * GEO_W + x];
        if (c.h < 0 || c.h > 250 || c.temperature < 8 || c.wet < .32f)
          continue;
        V3 d = geoDirection(x, y);
        float h = elevation(d);
        if (h < 8 || h > 150 || dot(d, norm(v3(.7f, .68f, .18f))) < .35f)
          continue;
        float candidate = -fabsf(h - 28) + 120 * c.relief -
                          600 * (1 - dot(surfaceNormal(d), d));
        if (candidate > score) {
          score = candidate;
          best = d;
        }
      }
    return best;
  }
  V3 best = norm(v3(.35f, .65f, .8f));
  float score = -1e9;
  for (int i = 0; i < 900; i++) {
    V3 d = norm(v3(.1f + (i % 30) * .03f, .3f + (i / 30) * .026f, .8f));
    float h = elevation(d);
    float s = -fabsf(h - 28) - 600 * (1 - dot(surfaceNormal(d), d));
    if (s > score) {
      score = s;
      best = d;
    }
  }
  return best;
}
#include "actors.h"
#include "nature.h"
/* Carry local occupants with the body's rigid frame; flight velocities remain
 * relative to the active body, as in the existing arcade controller. */
static void advanceCelestial(void) {
 CelestialFrame old=celestial,next=celestialAt(clockTime,dayOffset,lunarPhaseOffset);
 int rider=nearMoon(eye),parked=nearMoon(shipPos);
 V3 playerLocal=celestialInverse(&old,add(eye,mul(old.center,-1)));
 V3 shipLocal=celestialInverse(&old,add(shipPos,mul(old.center,-1)));
 SDL_LockMutex(mutex);celestial=next;SDL_UnlockMutex(mutex);
 if(rider) {
  eye=moonWorldPoint(playerLocal);
  heading=moonVector(celestialInverse(&old,heading));velocity=moonVector(celestialInverse(&old,velocity));
  flightForward=moonVector(celestialInverse(&old,flightForward));flightUp=moonVector(celestialInverse(&old,flightUp));
 }
 if(parked){shipPos=moonWorldPoint(shipLocal);shipHeading=moonVector(celestialInverse(&old,shipHeading));}
 sun=celestial.sun;
}
static void view(int n) {
  V3 d = homeDirection();
  float h = fmaxf(elevation(d), 0);
  eye = mul(d, RADIUS + (n == 2 ? h + 220 : h + 2.5f));
  heading = norm(cross(v3(0, 1, 0), d));
  V3 initialHeading = heading;
  float lowest = 1e9;
  for (int k = 0; k < 16; k++) {
    V3 dir = rotateAround(initialHeading, d, k * 2 * PI / 16);
    float h2 = elevation(norm(add(d, mul(dir, 250 / RADIUS))));
    if (h2 < lowest) {
      lowest = h2;
      heading = dir;
    }
  }
  if (!spawnReady) {
    spawnPoint = mul(d, RADIUS + h);
    spawnReady = 1;
  }
  pitch = n == 2 ? -.20f : -.24f;
  flying = n == 2;
  flightForward = norm(add(mul(heading, cosf(pitch)), mul(d, sinf(pitch))));
  flightUp = d;
  flightRates = v3(0, 0, 0);
  throttle = n == 2 ? .45f : 0;
  roll = lookX = lookY = 0;
  sprint = 0;
  velocity = flying ? mul(flightForward, 65) : v3(0, 0, 0);
  shipHeading = heading;
  shipPos = add(eye, add(mul(heading, 8), mul(norm(cross(heading, d)), 3)));
  parkShip();
  landing = 0;
  printf("LOD_ADAPTIVE max_grid=12 streaming_rings=10,25,50,100 detail=1\n");
  printf("VIEW %d altitude %.1f slope %.3f\n", n, sqrtf(dot(eye, eye)) - RADIUS,
         dot(surfaceNormal(d), d));
}
static void resizeBloom(void) {
  int size = performanceMode ? 128 : 256;
  for (int k = 0; k < 2; k++)
    if (glow[k].w != size) {
      glDeleteFramebuffers(1, &glow[k].fbo);
      glDeleteTextures(1, &glow[k].tex);
      glow[k] = target(size, size, 0, 0);
    }
}
static void resolution(void) {
  SDL_GL_GetDrawableSize(window, &width, &height);
  rw = width;
  rh = height;
  if (scene.tex) {
    int tw = 1, th = 1;
    while (tw < rw)
      tw *= 2;
    while (th < rh)
      th *= 2;
    if (tw != scene.w || th != scene.h) {
      glDeleteFramebuffers(1, &scene.fbo);
      glDeleteRenderbuffers(1, &scene.depth);
      glDeleteTextures(1, &scene.tex);
      scene = target(tw, th, 1, 0);
    }
  }
  resizeBloom();
  printf("RESOLUTION internal=%dx%d display=%dx%d target=%dx%d\n", rw, rh,
         width, height, scene.w, scene.h);
}
static void changeDisplayMode(int delta) {
  static Uint32 lastChange;
  Uint32 now=SDL_GetTicks();
  int next=displayModeIndex+delta;
  if(next<0 || next>=displayModeCount || (lastChange && now-lastChange<1000))return;
  SDL_DisplayMode old=displayModes[displayModeIndex], chosen=displayModes[next];
  int swapInterval=SDL_GL_GetSwapInterval();
  printf("DISPLAY_CHANGE begin %dx%d -> %dx%d\n",old.w,old.h,chosen.w,chosen.h);
  fflush(stdout);
  /* Drain RV350 command buffers before X replaces scanout/back buffers. Release
   * the drawable, leave exclusive fullscreen, then set the new mode once. */
  glBindFramebuffer(GL_FRAMEBUFFER,0);glUseProgram(0);glFinish();
  if(SDL_GL_MakeCurrent(window,NULL)) {fprintf(stderr,"DISPLAY detach: %s\n",SDL_GetError());return;}
  int failed=0;
  if(!fixedWindowed && SDL_SetWindowFullscreen(window,0))failed=1;
  if(!failed && !fixedWindowed && SDL_SetWindowDisplayMode(window,&chosen))failed=1;
  if(!failed)SDL_SetWindowSize(window,chosen.w,chosen.h);
  if(!failed && !fixedWindowed && SDL_SetWindowFullscreen(window,SDL_WINDOW_FULLSCREEN))failed=1;
  if(failed) {
    fprintf(stderr,"DISPLAY rollback: %s\n",SDL_GetError());
    SDL_SetWindowDisplayMode(window,&old);SDL_SetWindowSize(window,old.w,old.h);
    if(!fixedWindowed)SDL_SetWindowFullscreen(window,SDL_WINDOW_FULLSCREEN);
  } else displayModeIndex=next;
  if(SDL_GL_MakeCurrent(window,context))die(SDL_GetError());
  SDL_GL_SetSwapInterval(swapInterval);
  SDL_PumpEvents();
  resolution();lodRevision++;lastChange=SDL_GetTicks();
  printf("DISPLAY_CHANGE complete %dx%d\n",width,height);fflush(stdout);
}
static void camera(void) {
  V3 f = viewForward, r = viewRight, u = viewUp;
  float near = clipNear;
  float top = near * tanf(PI / 6);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glFrustum(-top * width / height, top * width / height, -top, top, near,
            clipFar);
  float m[16] = {r.x, u.x, -f.x, 0, r.y, u.y, -f.y, 0,
                 r.z, u.z, -f.z, 0, 0,   0,   0,    1};
  glMatrixMode(GL_MODELVIEW);
  glLoadMatrixf(m);
}
static float meshTolerance = 1;
static int chooseMeshLOD(Node *n) {
  int lod = 3;
  V3 delta = add(streamViewReady?streamPriorityEye:eye, mul(n->center, -1));
  float distance = sqrtf(dot(delta,delta));
  /* Prefer fidelity nearby without forcing triangles onto flat patches. */
  float pixelScale = n->pixelScale * (1 + 1.5f / (1 + distance/50));
  /* Hysteresis on index reduction; a full patch is always available. */
  while (lod > 1 &&
         n->geomError[lod] * pixelScale >
             (lod > n->meshLevel ? 5.0f : 7.0f) / terrainDetail * meshTolerance)
    lod--;
  /* Keep coast silhouettes from following the coarse patch grid in orbit. */
  if(n->size>4000 && n->minHeight<0 && n->maxHeight>0 && lod>1)lod=1;
  return lod;
}
static void prepareMeshLODs(void) {
  int choice[1024];
  for (int i = 0; i < selectedCount; i++) {
    Node *n = &nodes[selected[i]];
    V3 delta = add(streamViewReady?streamPriorityEye:eye, mul(n->center, -1));
    n->sortDistance = dot(delta, delta);
    n->pixelScale =
        rh * .8660254f / fmaxf(1, sqrtf(n->sortDistance) - n->size * .5f);
  }
  meshTolerance = 1;
  int triangleBudget =
      (int)clampf(160000.0f * terrainDetail * rw * rh / (768.0f * 576.0f),
                  90000 * terrainDetail, 750000);
  /* Budget the complete selected cover, including the coarse off-cone terrain. */
  for (int attempt = 0; attempt < 16; attempt++) {
    int total = 0;
    for (int i = 0; i < selectedCount; i++) {
      Node *n = &nodes[selected[i]];
      choice[i] = chooseMeshLOD(n);
      int passes = (n->maxHeight >= 0) + (n->minHeight < 0);
      total += indexCount[choice[i]] / 3 * passes;
    }
    if (total <= triangleBudget || attempt == 15)
      break;
    meshTolerance *= 1.35f;
  }
  for (int i = 0; i < selectedCount; i++)
    nodes[selected[i]].meshLevel = choice[i];
}
static int meshLOD(Node *n) { return n->meshLevel; }
#include "lod-morph.h"
#include "texture-fade.h"
/* First-order Taylor fog, with a conservative Hessian error bound over
 * the whole patch sphere. Fall back to exact GPU fog above 0.3% absolute error.
 */
static int fogCoefficients(const Node *n, float heightFactor, float plane[4]) {
  V3 delta = add(n->center, mul(cameraEye, -1));
  V3 shift = add(n->boundCenter, mul(n->center, -1));
  float distance = sqrtf(dot(delta, delta));
  float radius = n->boundRadius + sqrtf(dot(shift, shift));
  float nearest = distance - radius, a = .00009f * .69314718056f;
  if (nearest <= 0)
    return 0;
  float bound = .5f * radius * radius * heightFactor * expf(-a * nearest) *
                (a * a + a / nearest);
  if (bound > .003f)
    return 0;
  float attenuation = expf(-a * distance),
        gradient = heightFactor * a * attenuation / distance;
  plane[0] = delta.x * gradient;
  plane[1] = delta.y * gradient;
  plane[2] = delta.z * gradient;
  plane[3] = heightFactor * (1 - attenuation);
  return 1;
}
static void geometry(GLuint vbo, int lod) {
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo[lod]);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glVertexPointer(3, GL_FLOAT, sizeof(Vertex), (void *)offsetof(Vertex, p));
  glNormalPointer(GL_BYTE, sizeof(Vertex), (void *)offsetof(Vertex, n));
  glTexCoordPointer(3, GL_FLOAT, sizeof(Vertex), (void *)offsetof(Vertex, u));
  glDrawElements(GL_TRIANGLES, indexCount[lod], GL_UNSIGNED_SHORT, (void *)0);
  triangles += indexCount[lod] / 3;
}
/* Both globe vertex shaders clamp microdetail to zero beyond 100 m.
 * Use the containing sphere (including skirts/morph bounds), with 1 m margin,
 * so every vertex of a simplified patch has exactly zero relief weight. */
static int terrainFastMode(const Node *n) {
  V3 delta = add(n->boundCenter, mul(cameraEye, -1));
  float limit = n->boundRadius + 101;
  return performanceMode || dot(delta, delta) > limit * limit;
}
#include "terrain-batch.h"
#include "terrain-edges.h"
#include "water-seams.h"
static int nearFirst(const void *a, const void *b) {
  float aa = nodes[*(const int *)a].sortDistance,
        bb = nodes[*(const int *)b].sortDistance;
  return aa < bb ? -1 : aa > bb ? 1 : 0;
}
/* Small cached contact meshes: no shadow render target or extra scene pass. */
static void contactShadow(int id, V3 position, float radius) {
  static V3 previous[2], vertices[2][33];
  V3 radial = norm(position);
  float ground = elevation(radial),
        alt = sqrtf(dot(position, position)) - RADIUS - ground;
  if (ground < .1f || alt > 18)
    return;
  V3 change = add(position, mul(previous[id], -1));
  if (dot(change, change) > .25f) {
    previous[id] = position;
    V3 right = norm(cross(radial, v3(.01, 1, .02))),
       forward = cross(right, radial);
    vertices[id][0] = mul(radial, RADIUS + ground + .08f);
    for (int ring = 0; ring < 2; ring++)
      for (int i = 0; i < 16; i++) {
        float a = i * 2 * PI / 16, r = radius * (ring ? 1 : .45f);
        V3 d = norm(add(
            position, add(mul(right, cosf(a) * r), mul(forward, sinf(a) * r))));
        vertices[id][1 + ring * 16 + i] =
            mul(d, RADIUS + fmaxf(elevation(d), 0) + .08f);
      }
  }
  float alpha = (.10f + .16f * solarDaylight) * clampf(1 - alt / 18, 0, 1);
  glUseProgram(0);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDepthMask(GL_FALSE);
  glDisable(GL_CULL_FACE);
  glEnable(GL_POLYGON_OFFSET_FILL);
  glPolygonOffset(-2, -2);
  glColor4f(0, 0, 0, alpha);
  glBegin(GL_TRIANGLE_FAN);
  V3 p = add(vertices[id][0], mul(cameraEye, -1));
  glVertex3f(p.x, p.y, p.z);
  for (int i = 0; i <= 16; i++) {
    p = add(vertices[id][1 + i % 16], mul(cameraEye, -1));
    glVertex3f(p.x, p.y, p.z);
  }
  glEnd();
  glBegin(GL_TRIANGLE_STRIP);
  for (int i = 0; i <= 16; i++)
    for (int ring = 0; ring < 2; ring++) {
      glColor4f(0, 0, 0, ring ? 0 : alpha);
      p = add(vertices[id][1 + ring * 16 + i % 16], mul(cameraEye, -1));
      glVertex3f(p.x, p.y, p.z);
    }
  glEnd();
  glDisable(GL_POLYGON_OFFSET_FILL);
  glDepthMask(GL_TRUE);
  glDisable(GL_BLEND);
  glEnable(GL_CULL_FACE);
}
static void drawSky(void) {
  V3 f = viewForward, r = viewRight, u = viewUp;
  float altitude = sqrtf(dot(cameraEye,cameraEye))-RADIUS;
  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
  glDisable(GL_CULL_FACE);
  glUseProgram(skyP);
  u3(skyP, "forwardDir", f);
  u3(skyP, "rightDir", r);
  u3(skyP, "upDir", u);
  u3(skyP, "radial", norm(cameraEye));
  u3(skyP, "sun", sun);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_CUBE_MAP, skyCube);
  glUniform1i(uniformLocation(skyP, "starTex"), 0);
  float solar = dot(sun, norm(cameraEye));
  float air = exp2f(-fmaxf(altitude, 0) / 9000);
  u1(skyP, "horizonDip",
     sqrtf(fmaxf(0, 1 - powf(RADIUS / (RADIUS + fmaxf(altitude, 0)), 2))));
  u1(skyP, "starVisibility",
     fmaxf(clampf((-solar - .055f) * 3, 0, 1), 1 - air));
  u1(skyP, "planetRadius", RADIUS);
  u3(skyP, "zenithColor", mul(v3(.026, .072, .16), solarDaylight * air));
  u3(skyP, "horizonColor", fogColor);
  u3(skyP, "sunsetColor",
     mul(v3(.24, .055, .009), clampf(1 - fabsf(solar) * 6, 0, 1) * air));
  u1(skyP, "exposure", sceneExposure);
  u1(skyP, "time", clockTime);
  u1(skyP, "altitude", altitude);
  u2(skyP, "lens", tanf(PI / 6) * width / height, tanf(PI / 6));
  quad();
  glDepthMask(GL_TRUE);
}
#include "reflection.h"
#include "sun-shadow.h"
#include "moon-render.h"
#include "clouds.h"
#include "vram-budget.h"
/* Lighting follows the final chase camera, after this frame's simulation. */
static void updateSceneLighting(void) {
  float solarHeight = dot(sun, norm(cameraEye));
  solarDaylight = clampf((solarHeight + .10f) * 3, .025f, 1);
  float dusk = clampf(1 - fabsf(solarHeight) * 5, 0, 1);
  float localAir=exp2f(-fmaxf(sqrtf(dot(cameraEye,cameraEye))-RADIUS,0)/13000.f);
  sceneExposure = 1 + 2.5f * (1 - solarDaylight)*localAir;
  ambientLight = add(mul(v3(.12, .16, .22), solarDaylight),
                     mul(v3(.008, .013, .021), 1 - solarDaylight));
  ambientLight=add(mul(ambientLight,localAir),mul(v3(.003,.004,.006),1-localAir));
  sunLight = mul(v3(1.05f,.94f,.79f),1-localAir+localAir*clampf((solarHeight+.06f)*5,0,1));
  fogColor = add(mul(v3(.19f, .29f, .42f), solarDaylight * (1 - dusk * .45f)),
                 add(mul(v3(.24f, .083f, .028f), dusk * solarDaylight),
                     mul(v3(.0025, .005, .009), 1 - solarDaylight)));
}
/* Share world depth near either surface so terrain/vegetation can occlude the
 * third-person ship. Reserve foreground depth only in clear interplanetary
 * space, where the world's distant near plane would clip the ship itself. */
static void actorCamera(float near,float far,double lo,double hi) {
  if(flying && near>2) {near=2;far=2000;lo=0;hi=.01;}
  clipNear=near;clipFar=far;glDepthRange(lo,hi);camera();
}
static void prepareView(void) {
  V3 f, r, u;
  cameraBasis(&f, &r, &u);
  cameraEye = flying ? add(eye, add(mul(f, -14), mul(u, 4)))
                     : add(eye, mul(bodyUp(eye), -.75f));
  cameraClearance=bodyAltitude(cameraEye)-bodyHeight(cameraEye);
  if (cameraClearance < .5f) {
    cameraEye = bodyFloor(cameraEye,.5f);
    cameraClearance=.5f;
  }
  if (flying) {
    V3 target = eye;
    f = norm(add(target, mul(cameraEye, -1)));
  }
  r = norm(cross(f, u));
  u = cross(r, f);
  viewForward = cullForward = f;
  viewRight = r;
  viewUp = u;
}
static void drawScene(void) {
  updateSceneLighting();
  /* Disjoint celestial bodies get separate depth ranges and projections.
   * A nearby moon must not steal depth precision from distant planet coasts. */
  float planetDistance=sqrtf(dot(cameraEye,cameraEye));
  V3 moonDelta=add(cameraEye,mul(moonCenter(),-1));float moonDistance=sqrtf(dot(moonDelta,moonDelta));
  int moonCloser=moonDistance-MOON_RADIUS<planetDistance-RADIUS;
  float safeNear=worldNearPlane(cameraEye,flying);
  /* The chase camera can sit only 0.5 m above ground after collision. */
  safeNear=fminf(safeNear,fmaxf(.25f,cameraClearance*.5f));
  float planetNear=fmaxf(safeNear,(planetDistance-RADIUS-20000)*.1f);
  float moonNear=fmaxf(safeNear,(moonDistance-MOON_RADIUS-12000)*.1f);
  float planetFar=fmaxf(80000,planetDistance+RADIUS+20000),moonFar=fmaxf(80000,moonDistance+MOON_RADIUS+12000);
  double planetLo=moonCloser?.5:.01,planetHi=moonCloser?1:.5;
  double moonLo=moonCloser?.01:.5,moonHi=moonCloser?.5:1;
  clipNear=planetNear;clipFar=planetFar;worldDepthLo=planetLo;worldDepthHi=planetHi;
  glDepthRange(worldDepthLo,worldDepthHi);
  if(overdrawView) {
    visibilitySetup();camera();glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    if(!overdrawP)overdrawP=program("nature.vert","overdraw.frag");
    natureDraw();cloudsDraw();glDepthRange(0,1);return;
  }
  cpuFogPatches = gpuFogPatches = 0;
  visibilitySetup();
  camera();
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_CULL_FACE);
  glFrontFace(GL_CCW);
  glEnableClientState(GL_VERTEX_ARRAY);
  glEnableClientState(GL_NORMAL_ARRAY);
  glEnableClientState(GL_TEXTURE_COORD_ARRAY);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo[0]);
  float fogHeight = exp2f(-fmaxf(sqrtf(dot(cameraEye,cameraEye))-RADIUS, 0) / 13000.0f);
  for (int mode = performanceMode; mode < 2; mode++)
  for (int approximation = 0; approximation < 2; approximation++) {
    for (int transition = 0; transition < 2; transition++) {
      GLuint p = transition ? fadePrograms[mode][approximation]
                            : landPrograms[mode][approximation];
      glUseProgram(p);
      u1(p, "fogHeightFactor", fogHeight);
      u3(p, "sun", sun);
      u3(p, "fogColor", fogColor);
      u3(p, "ambientLight", ambientLight);
      u3(p, "ambientUp", norm(cameraEye));
      u3(p, "sunLight", sunLight);
      u1(p, "exposure", sceneExposure);
      tex(p, "atlas", 0, atlasTex[0]);
      tex(p, "detailTex", 1, detailMap.tex);
      if (transition)
        tex(p, "transitionAtlas", 2, transitionAtlas);
    }
  }
  if (wire)
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
  prepareMeshLODs();
  qsort(selected, selectedCount, sizeof(*selected), nearFirst);
  prepareTextureFades();
  prepareTerrainGeometry();
  visibleCount = frustumRejected = horizonRejected = 0;
  for (int i = 0; i < selectedCount; i++) {
    Node *n = &nodes[selected[i]];
    n->visible = nodeVisible(n);
    visibleCount += n->visible;
  }
  occlusionPrepare();
  gpuCheckpoint("reflection");updateReflection();
  gpuCheckpoint("sun-shadow");sunShadowUpdate();
  gpuCheckpoint("opaque-terrain");
  drawn = 0;
  glActiveTexture(GL_TEXTURE0);
  if (batchingEnabled)
    drawTerrainBatches();
  else {
    memset(batchedNodes, 0, sizeof(batchedNodes));
    batchDraws = batchPatches = 0;
  }
  int boundAtlas = -1;
  for (int i = 0; i < selectedCount; i++) {
    Node *n = &nodes[selected[i]];
    if (!n->visible || n->maxHeight < 0 || batchedNodes[i])
      continue;
    drawn++;
    float plane[4];
    int approximation = fogCoefficients(n, fogHeight, plane);
    cpuFogPatches += approximation;
    gpuFogPatches += !approximation;
    int mode = terrainFastMode(n);
    int fade = textureFadeFor(selected[i]);
    landP = fade >= 0 ? fadePrograms[mode][approximation]
                      : landPrograms[mode][approximation];
    glUseProgram(landP);
    pageUniform = landPageLocations[mode][approximation];
    originUniform = landOriginLocations[mode][approximation];
    landEyeUniform = landEyeLocations[mode][approximation];
    if (fade >= 0) {
      pageUniform = uniformLocation(landP, "page");
      originUniform = uniformLocation(landP, "detailOrigin");
      landEyeUniform = uniformLocation(landP, "eye");
      tex(landP, "transitionAtlas", 2, transitionAtlas);
      glActiveTexture(GL_TEXTURE0);
      glUniform2f(uniformLocation(landP, "transitionPage"), (fade % 8) / 8.f,
                  (fade / 8) / 4.f);
      u1(landP, "transitionMix",
         clampf((SDL_GetTicks() - textureFades[fade].start) / 450.f, 0, 1));
    }
    if (approximation)
      glUniform4fv(fade >= 0
                       ? uniformLocation(landP, "fogPlane")
                       : fogPlaneLocations[mode][approximation],
                   1, plane);
    int atlasIndex = n->slot / 256;
    if (boundAtlas != atlasIndex) {
      glBindTexture(GL_TEXTURE_2D, atlasTex[atlasIndex]);
      boundAtlas = atlasIndex;
    }
    glUniform2f(pageUniform, (n->slot % 16) / 16.0f,
                ((n->slot % 256) / 16) / 16.0f);
    float period = 100.0f / 17.0f;
    glUniform3f(originUniform, fmodf(n->center.x, period),
                fmodf(n->center.y, period), fmodf(n->center.z, period));
    V3 localEye = add(cameraEye, mul(n->center, -1));
    glUniform3f(landEyeUniform, localEye.x, localEye.y, localEye.z);
    V3 offset = add(n->center, mul(cameraEye, -1));
    glPushMatrix();
    glTranslatef(offset.x, offset.y, offset.z);
    geometry(n->vbo, meshLOD(n));
    glPopMatrix();
  }
  sunShadowGround();
  occlusionIssue();
  profileMark(0);
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  prepareWaterSeams();
  glUseProgram(waterP);
  u3(waterP, "eye", eye);
  u3(waterP, "sun", sun);
  u1(waterP, "daylight", solarDaylight);
  u3(waterP, "ambientLight", ambientLight);
  u3(waterP, "sunLight", sunLight);
  u3(waterP, "fogColor", fogColor);
  u1(waterP, "exposure", sceneExposure);
  u1(waterP, "time", clockTime);
  tex(waterP, "detailTex", 0, detailMap.tex);
  tex(waterP, "reflectionTex", 1, reflectionMap.tex);
  u1(waterP, "reflectionReady", reflectionReady ? 1 : 0);
  glUniformMatrix4fv(uniformLocation(waterP, "reflectionVP"), 1, GL_FALSE,
                     reflectionVP);
  glEnable(GL_POLYGON_OFFSET_FILL);
  glPolygonOffset(-1, -1);
  for (int i = 0; i < selectedCount; i++) {
    Node *n = &nodes[selected[i]];
    if (!n->visible || n->minHeight >= 0)
      continue;
    V3 localEye = add(cameraEye, mul(n->center, -1));
    glUniform3f(waterEyeUniform, localEye.x, localEye.y, localEye.z);
    glUniform3f(waterCenterUniform, n->center.x, n->center.y, n->center.z);
    u3(waterP, "reflectionOffset", add(n->center, mul(reflectionEye, -1)));
    glUniform2f(waterPhaseUniform,
                fmodf(dot(n->center, v3(.07f, .04f, .05f)), 2 * PI),
                fmodf(dot(n->center, v3(.11f, .08f, -.06f)), 2 * PI));
    glUniform3f(waterOffsetUniform, fmodf(n->center.x, 400),
                fmodf(n->center.y, 400), fmodf(n->center.z, 400));
    V3 offset = add(n->center, mul(cameraEye, -1));
    glPushMatrix();
    glTranslatef(offset.x, offset.y, offset.z);
    int lod = meshLOD(n), saved = indexCount[lod];
    indexCount[lod] = PATCH * PATCH * 6 / ((1 << lod) * (1 << lod));
    geometry(waterGeometry(n), lod);
    indexCount[lod] = saved;
    glPopMatrix();
  }
  glDisable(GL_POLYGON_OFFSET_FILL);
  glDisableClientState(GL_VERTEX_ARRAY);
  glDisableClientState(GL_NORMAL_ARRAY);
  glDisableClientState(GL_TEXTURE_COORD_ARRAY);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
  profileMark(1);
  gpuCheckpoint("vegetation");natureDraw();
  profileMark(2);
  if(!nearMoon(eye))contactShadow(1, flying ? eye : shipPos, 4.8f);
  drawSky();
  gpuCheckpoint("moon");
  clipNear=moonNear;clipFar=moonFar;glDepthRange(moonLo,moonHi);camera();moonDraw();
  if(moonCloser)actorCamera(moonNear,moonFar,moonLo,moonHi);
  else actorCamera(planetNear,planetFar,planetLo,planetHi);
  drawActors();profileMark(5);
  /* Transparent clouds blend over opaque actors and respect their depth. */
  clipNear=planetNear;clipFar=planetFar;glDepthRange(planetLo,planetHi);camera();cloudsDraw();
  glDepthRange(0, 1);
  profileMark(3);
}
static void overlay(void) {
  glUseProgram(0);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, width, height, 0, -1, 1);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glColor4f(.015, .025, .04, .8);
  glBegin(GL_QUADS);
  glVertex2f(16, 16);
  glVertex2f(520, 16);
  glVertex2f(520, 120);
  glVertex2f(16, 120);
  glEnd();
  glColor4f(.86, .94, .97, 1);
  label(28, 28, "POOR MAN'S SKY", 2);
  char s[160];
  snprintf(s, sizeof(s), "%.1F FPS | %d X %d | %s | ALT %.0F M", fps, rw, rh,
           nearMoon(eye)?(flying?"MOON FLY":"MOON WALK"):(flying ? "FLY" : "WALK"), bodyAltitude(eye));
  label(28, 54, s, 1.5f);
  SDL_LockMutex(mutex);
  snprintf(s, sizeof(s), "PAGES %d/1024 | WORLD RAM %.1F MiB / OS | QUEUE %d",
           resident, (ramBytes+moonRAMBytes) / 1048576.0f, qcount+moonQueueCount);
  SDL_UnlockMutex(mutex);
  label(28, 77, s, 1);
  snprintf(s, sizeof(s), "%d PATCHES | %.0F M/S | LOD ADAPTIVE | PAD %s", drawn,
           sqrtf(dot(velocity, velocity)), pad ? "ON" : "OFF");
  label(28, 95, s, 1);
  V3 destination=nearMoon(eye)?v3(0,0,0):moonCenter(),to=add(destination,mul(eye,-1));
  float distance=sqrtf(dot(to,to));
  snprintf(s,sizeof(s),"%s %.1F KM | %s",nearMoon(eye)?"PLANET":"MOON",(distance-(nearMoon(eye)?RADIUS:MOON_RADIUS))/1000,dot(to,viewForward)>0?"AHEAD":"BEHIND");
  label(28,125,s,1);
  snprintf(s,sizeof(s),"MOON LIT %.0F%% | ORBIT 8 GAME DAYS",lunarIlluminatedFraction(eye)*100);label(28,155,s,1);
  snprintf(s,sizeof(s),"VRAM EST %.1F/56 MIB | RAM/DISK BACKING",vramEstimate()/1048576.0);label(28,140,s,1);

  label(22, height - 24,
        "RT/LT DRIVE | HOLD A SUPERBOOST | LB/RB YAW | X BRAKE | Y LAND", 1);
  glDisable(GL_BLEND);
}
static void render(void) {
  triangles = 0;
  glBindFramebuffer(GL_FRAMEBUFFER, scene.fbo);
  glViewport(0, 0, rw, rh);
  glClear(GL_DEPTH_BUFFER_BIT);
  drawScene();
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  if (bloom && !overdrawView) {
    glBindFramebuffer(GL_FRAMEBUFFER, glow[0].fbo);
    glViewport(0, 0, glow[0].w, glow[0].h);
    glUseProgram(brightP);
    tex(brightP, "sceneTex", 0, scene.tex);
    u2(brightP, "scale", rw / (float)scene.w, rh / (float)scene.h);
    quad();
    glBindFramebuffer(GL_FRAMEBUFFER, glow[1].fbo);
    glUseProgram(blurP);
    tex(blurP, "sceneTex", 0, glow[0].tex);
    u2(blurP, "direction", 1.0f / glow[0].w, 0);
    quad();
    glBindFramebuffer(GL_FRAMEBUFFER, glow[0].fbo);
    tex(blurP, "sceneTex", 0, glow[1].tex);
    u2(blurP, "direction", 0, 1.0f / glow[0].h);
    quad();
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glViewport(0, 0, width, height);
  postP = performanceMode ? postPerformanceP : postQualityP;
  glUseProgram(postP);
  tex(postP, "sceneTex", 0, scene.tex);
  tex(postP, "bloomTex", 1, glow[0].tex);
  u2(postP, "scale", rw / (float)scene.w, rh / (float)scene.h);
  u2(postP, "halfTexel", .5f / scene.w, .5f / scene.h);
  u1(postP, "bloom", bloom && !overdrawView ? 1 : 0);

  quad();
  if (hud)
    overlay();
  profileMark(4);
}
static void screenshot(const char *path) {
  unsigned char *p = malloc(width * height * 3);
  if (!p)
    die("screenshot memory");
  glReadBuffer(GL_BACK);
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, p);
  FILE *f = fopen(path, "wb");
  if (!f)
    die("screenshot file");
  fprintf(f, "P6\n%d %d\n255\n", width, height);
  for (int y = height - 1; y >= 0; y--)
    fwrite(p + y * width * 3, 1, width * 3, f);
  fclose(f);
  free(p);
}
#include "shader-warmup.h"
#include "preload.h"
static float axis(SDL_GameControllerAxis a) {
  if (!pad)
    return 0;
  float v = SDL_GameControllerGetAxis(pad, a) / 32767.0f;
  float d = .18f;
  return fabsf(v) < d ? 0 : copysignf((fabsf(v) - d) / (1 - d), v);
}
static int button(SDL_GameControllerButton b) {
  return pad && SDL_GameControllerGetButton(pad, b);
}
static void openPad(void) {
  if (pad)
    return;
  for (int i = 0; i < SDL_NumJoysticks(); i++)
    if (SDL_IsGameController(i)) {
      pad = SDL_GameControllerOpen(i);
      if (pad) {
        printf("CONTROLLER %s\n", SDL_GameControllerName(pad));
        break;
      }
    }
}
int main(int argc, char **argv) {
  resourceInit();
  int preloadMode = -1, startMoon = 0, probeBody = 0, moonView=0;
  int frames = 0, viewNo = 1, vsync = 1, windowed = 0, tour = 0, still = 0;
  const char *capture = NULL;
  float captureAltitude = -1, capturePitch = -10;
  Uint64 boot = SDL_GetPerformanceCounter();
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--frames") && i + 1 < argc)
      frames = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--capture") && i + 1 < argc)
      capture = argv[++i];
    else if (!strcmp(argv[i], "--probe-body") && i+1<argc)
      probeBody=atoi(argv[++i]);
    else if (!strcmp(argv[i], "--moon-phase") && i+1<argc)
      lunarPhaseOffset=strtof(argv[++i],NULL);
    else if (!strcmp(argv[i], "--moon-view"))
      moonView=1;
    else if (!strcmp(argv[i], "--moon"))
      startMoon = 1;
    else if (!strcmp(argv[i], "--view") && i + 1 < argc)
      viewNo = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--altitude") && i + 1 < argc)
      captureAltitude = strtof(argv[++i], NULL);
    else if (!strcmp(argv[i], "--pitch") && i + 1 < argc)
      capturePitch = strtof(argv[++i], NULL);
    else if (!strcmp(argv[i], "--cache-dir") && i + 1 < argc)
      cacheDirectory = argv[++i];
    else if (!strcmp(argv[i], "--legacy-terrain"))
      geological = 0;
    else if (!strcmp(argv[i], "--no-cache"))
      cacheEnabled = 0;
    else if (!strcmp(argv[i], "--time") && i + 1 < argc)
      dayOffset = strtof(argv[++i], NULL);
    else if (!strcmp(argv[i], "--seed") && i + 1 < argc)
      worldSeed = (uint32_t)strtoul(argv[++i], NULL, 10);
    else if (!strcmp(argv[i], "--performance")) {
      performanceMode = 1;
    } else if (!strcmp(argv[i], "--resolution") && i + 1 < argc) {
      if(sscanf(argv[++i], "%dx%d", &requestedWidth, &requestedHeight)!=2)die("invalid resolution");
    }
    else if (!strcmp(argv[i], "--nature-quality") && i + 1 < argc)
      natureQuality = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--legacy-culling"))
      cullingMode = 0;
    else if (!strcmp(argv[i], "--no-batches"))
      batchingEnabled = 0;
    else if (!strcmp(argv[i], "--no-occlusion"))
      occlusionEnabled = 0;
    else if (!strcmp(argv[i], "--trace-gpu") && i+1<argc) {
      gpuTrace=fopen(argv[++i],"w");if(!gpuTrace)die("GPU trace file");
    }
    else if (!strcmp(argv[i], "--no-shader-warmup"))
      shaderWarmupEnabled=0;
    else if (!strcmp(argv[i], "--no-clouds"))
      cloudsEnabled=0;
    else if (!strcmp(argv[i], "--no-reflections"))
      reflectionEnabled=0;
    else if (!strcmp(argv[i], "--no-sun-shadows"))
      sunShadows = 0;
    else if (!strcmp(argv[i], "--legacy-streaming"))
      directionalStreaming=0;
    else if (!strcmp(argv[i], "--profile"))
      profileFrames = 1;
    else if (!strcmp(argv[i], "--terrain-detail") && i + 1 < argc)
      terrainDetail = strtof(argv[++i], NULL);
    else if (!strcmp(argv[i], "--texture-detail") && i + 1 < argc)
      textureDetail = strtof(argv[++i], NULL);
    else if (!strcmp(argv[i], "--reflection-interval") && i + 1 < argc)
      reflectionInterval = atoi(argv[++i]) == 2 ? 2 : 1;
    else if (!strcmp(argv[i], "--ram-preload-mib") && i + 1 < argc)
      ramPreloadTarget = (size_t)clampf(strtof(argv[++i], NULL), 0, (float)(SIZE_MAX/1048576u-1)) * 1048576u;
    else if (!strcmp(argv[i], "--preload"))
      preloadMode = 1;
    else if (!strcmp(argv[i], "--no-preload"))
      preloadMode = 0;
    else if (!strcmp(argv[i], "--still"))
      still = 1;
    else if (!strcmp(argv[i], "--no-vsync"))
      vsync = 0;
    else if (!strcmp(argv[i], "--windowed"))
      windowed = 1;
    else if (!strcmp(argv[i], "--no-hud"))
      hud = 0;
    else if (!strcmp(argv[i], "--tour"))
      tour = 1;
    else if (!strcmp(argv[i], "--help")) {
      puts("poor-mans-sky [--frames N] [--capture image.ppm] [--view 1|2] [--seed N] "
           "[--moon] [--time 0..1] [--resolution WxH] [--altitude metres-AGL] [--pitch radians] [--still] "
           "[--cache-dir directory] [--no-cache] [--no-vsync] [--windowed] "
           "[--nature-quality 0|1|2] [--performance] [--tour] [--no-hud] "
           "[--no-sun-shadows] [--no-clouds] [--no-reflections] [--no-shader-warmup] [--trace-gpu FILE] [--probe-body 1|2] [--moon-view] [--moon-phase 0..1] [--reflection-interval 1|2] [--ram-preload-mib N] [--profile] [--legacy-streaming] [--legacy-terrain] [--preload|--no-preload] "
           "[--terrain-detail 1] [--texture-detail 1]");
      return 0;
    } else
      die("unknown argument");
  }
  if (textureDetail != 1 || terrainDetail != 1 || natureQuality < 0 ||
      natureQuality > 2 || viewNo < 1 ||
      viewNo > 2 || frames < 0 || !isfinite(lunarPhaseOffset) || (capture && !frames))
    die("invalid arguments");
  setvbuf(stdout, NULL, _IOLBF, 0);
  gameModeLib = dlopen("libgamemode.so.0", RTLD_NOW);
  if (gameModeLib) {
    int (*startGameMode)(void) =
        (int (*)(void))dlsym(gameModeLib, "real_gamemode_request_start");
    gameModeEnd =
        (int (*)(void))dlsym(gameModeLib, "real_gamemode_request_end");
    printf("GAMEMODE %s\n",
           startGameMode && startGameMode() == 0 ? "active" : "unavailable");
  }
  mkdir("screenshots", 0755);
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER))
    die(SDL_GetError());
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_DisplayMode display;
  if (SDL_GetCurrentDisplayMode(0, &display))
    die(SDL_GetError());
  fixedWindowed=windowed;
  for(int i=0;i<SDL_GetNumDisplayModes(0) && displayModeCount<64;i++) {
    SDL_DisplayMode m;
    if(SDL_GetDisplayMode(0,i,&m) || m.w<640 || m.h<480 || m.w>2048 || m.h>2048)continue;
    int duplicate=0;
    for(int j=0;j<displayModeCount;j++)if(displayModes[j].w==m.w && displayModes[j].h==m.h) {
      if(abs(m.refresh_rate-60)<abs(displayModes[j].refresh_rate-60))displayModes[j]=m;
      duplicate=1;break;
    }
    if(!duplicate)displayModes[displayModeCount++]=m;
  }
  if(!displayModeCount)displayModes[displayModeCount++]=display;
  for(int i=0;i<displayModeCount;i++)for(int j=i+1;j<displayModeCount;j++)
    if(displayModes[j].w*displayModes[j].h<displayModes[i].w*displayModes[i].h) {
      SDL_DisplayMode t=displayModes[i];displayModes[i]=displayModes[j];displayModes[j]=t;
    }
  for(int i=0;i<displayModeCount;i++) {
    printf("DISPLAY_MODE %d %dx%d @%d\n",i,displayModes[i].w,displayModes[i].h,displayModes[i].refresh_rate);
    if(displayModes[i].w==requestedWidth && displayModes[i].h==requestedHeight)displayModeIndex=i;
  }
  width=displayModes[displayModeIndex].w;height=displayModes[displayModeIndex].h;
  window = SDL_CreateWindow("Poor Man's Sky", SDL_WINDOWPOS_CENTERED,
      SDL_WINDOWPOS_CENTERED, width, height, SDL_WINDOW_OPENGL);
  if (!window)die(SDL_GetError());
  if(!windowed && (SDL_SetWindowDisplayMode(window,&displayModes[displayModeIndex]) ||
      SDL_SetWindowFullscreen(window,SDL_WINDOW_FULLSCREEN)))die(SDL_GetError());
  gpuCheckpoint("create-context");
  context = SDL_GL_CreateContext(window);
  if (!context)
    die(SDL_GetError());
  SDL_GL_GetDrawableSize(window, &width, &height);
  int swapResult = SDL_GL_SetSwapInterval(vsync);
  printf("SWAP requested=%d result=%d actual=%d\n", vsync, swapResult,
         SDL_GL_GetSwapInterval());
  const char *gpu = (const char *)glGetString(GL_RENDERER);
  printf("GPU %s | GL %s | display %dx%d\n", gpu, glGetString(GL_VERSION),
         width, height);
  if (strstr(gpu, "llvmpipe") || strstr(gpu, "softpipe"))
    die("software rasterizer rejected");
  GLint maxtex;
  glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxtex);
  if (maxtex < 2048 || width > 2048 || height > 2048)
    die("render target exceeds hardware");
  glDisable(GL_DITHER);
  celestial=celestialAt(0,dayOffset,lunarPhaseOffset);sun=celestial.sun;
  cacheInit();
  geologyInit();
  materialInit();
  initMaps();
  initWorld();
  for (int mode = 0; mode < 2; mode++)
    for (int approximate = 0; approximate < 2; approximate++) {
      GLuint p = program(approximate ? "globe-cpu-fog.vert" : "globe.vert",
                         mode ? "globe-fast.frag" : "globe.frag");
      landPrograms[mode][approximate] = p;
      fadePrograms[mode][approximate] =
          program(approximate ? "globe-cpu-fog.vert" : "globe.vert",
                  mode ? "globe-fade-fast.frag" : "globe-fade.frag");
      landPageLocations[mode][approximate] = glGetUniformLocation(p, "page");
      landOriginLocations[mode][approximate] =
          glGetUniformLocation(p, "detailOrigin");
      landEyeLocations[mode][approximate] = glGetUniformLocation(p, "eye");
      fogPlaneLocations[mode][approximate] =
          glGetUniformLocation(p, "fogPlane");
    }
  reflectionLandP = program("globe.vert", "globe-reflection.frag");
  landP = landPrograms[0][0];
  waterP = program("globe-water.vert", "globe-water.frag");
  landEyeUniform = glGetUniformLocation(landP, "eye");
  waterEyeUniform = glGetUniformLocation(waterP, "eye");
  waterCenterUniform = glGetUniformLocation(waterP, "patchCenter");
  waterOffsetUniform = glGetUniformLocation(waterP, "waterOffset");
  waterPhaseUniform = glGetUniformLocation(waterP, "wavePhase");
  skyP = program("sky.vert", "globe-sky.frag");
  occlusionP = program("occlusion.vert", "occlusion.frag");
  GLint queryBits = 0;
  glGetQueryiv(GL_SAMPLES_PASSED, GL_QUERY_COUNTER_BITS, &queryBits);
  if (!queryBits)
    occlusionEnabled = 0;
  printf("OCCLUSION enabled=%d counter_bits=%d\n", occlusionEnabled, queryBits);

  postP = postQualityP = program("bake.vert", "post.frag");
  postPerformanceP = program("bake.vert", "post-fast.frag");
  brightP = program("bake.vert", "bright.frag");
  blurP = program("bake.vert", "blur.frag");
  int tw = 1, th = 1;
  while (tw < width)
    tw *= 2;
  while (th < height)
    th *= 2;
  scene = target(tw, th, 1, 0);
  glow[0] = target(256, 256, 0, 0);
  glow[1] = target(256, 256, 0, 0);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LEQUAL);
  glCullFace(GL_BACK);
  actorInit();
  natureInit();
  if (!still)
    openPad();
  resolution();
  view(viewNo);
  celestialFrameSpawnMoon(eye,heading);
  celestial=celestialAt(0,dayOffset,lunarPhaseOffset);sun=celestial.sun;
  if(startMoon) {
    V3 d=norm(mul(moonCenter(),-1));
    eye=add(moonCenter(),mul(d,MOON_RADIUS+moonHeight(moonLocalVector(d))+2.5f));
    heading=norm(cross(d,v3(0,1,0)));flying=0;pitch=-.12f;velocity=v3(0,0,0);
    shipPos=add(eye,mul(heading,8));shipHeading=heading;parkShip();
    preloadMode=0;
  }
  if (captureAltitude >= 0) {
    eye=bodyFloor(eye,fmaxf(2.5f,captureAltitude));
  }
  if (capturePitch > -10) {
    pitch = clampf(capturePitch, -1.45f, 1.45f);
    flightForward =
        norm(add(mul(heading, cosf(pitch)), mul(bodyUp(eye), sinf(pitch))));
    flightUp = bodyUp(eye);
  }
  if(probeBody==1 || probeBody==2) {
    V3 center=probeBody==2?moonCenter():v3(0,0,0);
    V3 radial=norm(probeBody==2?mul(moonCenter(),-1):moonCenter());
    float radius=probeBody==2?MOON_RADIUS:RADIUS;
    eye=add(center,mul(radial,radius+fmaxf(captureAltitude,60000)));
    flightForward=mul(radial,-1);flightUp=norm(cross(radial,v3(0,1,0)));
    heading=flightUp;flying=1;velocity=v3(0,0,0);throttle=0;preloadMode=0;
  }
  if(moonView) {
    V3 d=norm(moonCenter());eye=bodyFloor(mul(d,RADIUS),fmaxf(2.5f,captureAltitude));
    heading=norm(cross(v3(0,0,1),d));pitch=PI*.5f-.01f;flying=0;velocity=v3(0,0,0);preloadMode=0;
  }
  if(shaderWarmupEnabled)warmShaders();
  int preloadRunning =
      preloadMode == 1 || (preloadMode < 0 && !still) ? preloadWorld() : 1;
  printf("INITIALIZED seconds=%.3f playerAGL=%.1f phase=%.4f pitch=%.3f\n",
         (SDL_GetPerformanceCounter() - boot) /
             (double)SDL_GetPerformanceFrequency(),
         bodyAltitude(eye)-bodyHeight(eye),
         dayOffset, pitch);
  checkGL("initialization");
  Uint64 freq = SDL_GetPerformanceFrequency(),
         prev = SDL_GetPerformanceCounter();
  double elapsed = 0, windowMS = 0, simulationDebt = 0;
  double frameSamples[4096];
  int frameSampleCount = 0;
  int samples = 0, windowFrames = 0, running = preloadRunning;

  while (running) {
    Uint64 start = SDL_GetPerformanceCounter();
    double realDT = (start - prev) / (double)freq;
    float dt = 1.f / 60;
    prev = start;
    if (still || paused)
      realDT = 0;
    clockTime += realDT;
    simulationDebt = fmin(.5, simulationDebt + realDT);
    advanceCelestial();
    SDL_Event e;
    int shot = 0;
    while (SDL_PollEvent(&e)) {
      if (still && e.type != SDL_QUIT)
        continue;
      if (e.type == SDL_CONTROLLERDEVICEADDED)
        openPad();
      if (e.type == SDL_CONTROLLERDEVICEREMOVED && pad &&
          e.cdevice.which ==
              SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pad))) {
        SDL_GameControllerClose(pad);
        pad = NULL;
        velocity = v3(0, 0, 0);
        openPad();
      }
      if (e.type == SDL_CONTROLLERBUTTONDOWN) {
        int b = e.cbutton.button;
        if ((!flying && b == SDL_CONTROLLER_BUTTON_X) ||
            (flying && b == SDL_CONTROLLER_BUTTON_Y))
          interactShip();
        if (b == SDL_CONTROLLER_BUTTON_RIGHTSTICK) {
          if(flying) lookX=lookY=0;
          else sprint=!sprint;
        }
        if (b == SDL_CONTROLLER_BUTTON_START)
          paused = !paused;
      }
      if (e.type == SDL_WINDOWEVENT &&
          e.window.event == SDL_WINDOWEVENT_FOCUS_LOST)
        velocity = v3(0, 0, 0);
      if (e.type == SDL_QUIT)
        running = 0;
      if (e.type == SDL_MOUSEBUTTONDOWN) {
        mouse = 1;
        SDL_SetRelativeMouseMode(SDL_TRUE);
      }
      if (e.type == SDL_MOUSEMOTION && mouse && !flying) {
        V3 radial = bodyUp(eye);
        float a = -e.motion.xrel * .0025f;
        heading =
            add(mul(heading, cosf(a)), mul(cross(radial, heading), sinf(a)));
        pitch = clampf(pitch - e.motion.yrel * .0025f, -1.55f, 1.50f);
      }
      if (e.type == SDL_KEYDOWN && !e.key.repeat) {
        SDL_Keycode k = e.key.keysym.sym;
        if (k == SDLK_ESCAPE) {
          if (mouse) {
            mouse = 0;
            SDL_SetRelativeMouseMode(SDL_FALSE);
          } else
            running = 0;
        }
        if (k >= SDLK_1 && k <= SDLK_2) {
          view(k - SDLK_0);
          velocity = v3(0, 0, 0);
        }
        if (k == SDLK_MINUS || k == SDLK_KP_MINUS) {
          changeDisplayMode(-1);
        }
        if (k == SDLK_PLUS || k == SDLK_EQUALS || k == SDLK_KP_PLUS) {
          changeDisplayMode(1);
        }
        if (k == SDLK_f || k == SDLK_e)
          interactShip();
        if (k == SDLK_F7)overdrawView=!overdrawView;
        if (k == SDLK_F8)cloudsEnabled=!cloudsEnabled;
        if (k == SDLK_F5) {sunShadows=!sunShadows;sunReady=0;}
        if (k == SDLK_F6)
          dayOffset += 1.0f / 12;
        if (k == SDLK_b)
          bloom = !bloom;
        if (k == SDLK_F2)
          hud = !hud;
        if (k == SDLK_F4) {
          performanceMode = !performanceMode;
          resolution();
        }
        if (k == SDLK_F3)
          wire = !wire;
        if (k == SDLK_F12)
          shot = 1;
        if (k == SDLK_F1)
          SDL_ShowSimpleMessageBox(
              SDL_MESSAGEBOX_INFORMATION, "Poor Man's Sky controls",
              "WASD: walk / directional flight. Mouse: look. E/F: board/land.\n"
              "F5: sun shadows. F7: vegetation/cloud overdraw. F8: clouds.\n"
              "Space: jetpack / rise. Right mouse held: walk jetpack. C: descend. Shift: sprint / boost.\n"
              "Xbox foot: LS move, RS look/click sprint, A jetpack, X board.\n"
              "Flight: LS pitch/roll, RS look, RT forward, LT reverse.\n"
              "RT/LT: gradual forward/reverse, 120 m/s. RS: orbit; click: reset.\n"
              "Hold A: superboost (20 km/s). LT reverses boost.\n"
              "Flight: LB/RB yaw, X brake, Y land. WASD pitch/roll, Q/R yaw.\n"
              "PgUp/PgDn forward/reverse (hold). F6 advance time. Start pause. "
              "1/2 reset.\n"
              "+/- resolution; F2 HUD; F4 quality/performance; F3 wireframe; "
              "F12 screenshot; "
              "Esc release/exit.",
              window);
      }
    }
    if (!running)
      break;
    if (paused) {
      SDL_Delay(15);
      prev = SDL_GetPerformanceCounter();
      continue;
    }
    int simulationSteps = 0;
    while (simulationDebt >= 1.0 / 60 && simulationSteps++ < 30) {
      simulationDebt -= 1.0 / 60;
      const Uint8 *keys = SDL_GetKeyboardState(NULL);
      float moveX = keys[SDL_SCANCODE_D] - keys[SDL_SCANCODE_A] +
                    (flying ? 0 : axis(SDL_CONTROLLER_AXIS_LEFTX));
      float moveY = keys[SDL_SCANCODE_W] - keys[SDL_SCANCODE_S] -
                    (flying ? 0 : axis(SDL_CONTROLLER_AXIS_LEFTY));
      V3 radial = bodyUp(eye), f, r, u;
      if (flying) {
        V3 right = norm(cross(flightForward, flightUp));
        flightUp = norm(cross(right, flightForward));
        float pitchInput = axis(SDL_CONTROLLER_AXIS_LEFTY) +
                           keys[SDL_SCANCODE_S] - keys[SDL_SCANCODE_W];
        float rollInput = axis(SDL_CONTROLLER_AXIS_LEFTX) +
                          keys[SDL_SCANCODE_D] - keys[SDL_SCANCODE_A];
        float yawInput = button(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) -
                         button(SDL_CONTROLLER_BUTTON_LEFTSHOULDER) +
                         keys[SDL_SCANCODE_R] - keys[SDL_SCANCODE_Q];
        V3 wanted = v3(pitchInput * .7f, rollInput * 1.25f, yawInput * .45f);
        flightRates = add(flightRates, mul(add(wanted, mul(flightRates, -1)),
                                           1 - expf(-dt * 5)));
        flightForward = rotateAround(flightForward, right, flightRates.x * dt);
        flightUp = rotateAround(flightUp, right, flightRates.x * dt);
        flightUp = rotateAround(flightUp, flightForward, flightRates.y * dt);
        flightForward =
            rotateAround(flightForward, flightUp, -flightRates.z * dt);
        float bank = dot(flightUp, norm(cross(flightForward, radial)));
        flightForward = rotateAround(flightForward, radial, -bank * dt * .22f);
        flightForward = norm(flightForward);
        flightUp =
            norm(cross(norm(cross(flightForward, flightUp)), flightForward));
        heading =
            norm(add(flightForward, mul(radial, -dot(flightForward, radial))));
        pitch = asinf(clampf(dot(flightForward, radial), -1, 1));
        /* Signed, momentary arcade throttle; equal triggers cancel out. */
        throttle =
            clampf(axis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT) -
                       axis(SDL_CONTROLLER_AXIS_TRIGGERLEFT) +
                       keys[SDL_SCANCODE_PAGEUP] - keys[SDL_SCANCODE_PAGEDOWN],
                   -1, 1);
        lookX = remainderf(lookX + axis(SDL_CONTROLLER_AXIS_RIGHTX)*1.6f*dt,2*PI);
        lookY = clampf(lookY-axis(SDL_CONTROLLER_AXIS_RIGHTY)*1.2f*dt,-1.2f,1.2f);
      } else {
        float yaw = -axis(SDL_CONTROLLER_AXIS_RIGHTX) * 1.8f * dt;
        heading = rotateAround(heading, radial, yaw);
        pitch = clampf(pitch - axis(SDL_CONTROLLER_AXIS_RIGHTY) * 1.5f * dt,
                       -1.5f, 1.5f);
      }
      basis(&f, &r, &u);
      int boost = keys[SDL_SCANCODE_LSHIFT] || (!flying && sprint);
      int rise = keys[SDL_SCANCODE_SPACE] ||
                 (!flying && (button(SDL_CONTROLLER_BUTTON_A) ||
                  (mouse && (SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS) &&
                   (SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(SDL_BUTTON_RIGHT)))));
      int descend = keys[SDL_SCANCODE_C];
      float alt = bodyAltitude(eye);
      if (!flying && fabsf(moveX) + fabsf(moveY) < .1f)
        sprint = 0;
      if (flying && boost)
        moveY = fmaxf(moveY, 1);
      V3 wish = add(mul(flying ? f : heading, moveY), mul(r, moveX));
      if (flying)
        wish = add(wish, mul(radial, rise - descend));
      float len = sqrtf(dot(wish, wish));
      if (len > 1)
        wish = mul(wish, 1 / len);
      float speed = flying
                        ? (boost ? 180 : 35) * (1 + clampf(alt / 1500, 0, 30))
                        : (boost ? 9 : 4.5f);
      V3 targetVelocity = mul(wish, speed);
      float vertical = dot(velocity, radial);
      V3 tangent = add(velocity, mul(radial, -vertical));
      if (flying) {
        /* Assisted hover and symmetric reverse, independent of air density.
           Exponential response remains stable at low frame rates. */
        int braking = button(SDL_CONTROLLER_BUTTON_X);
        int superboost = button(SDL_CONTROLLER_BUTTON_A) && !landing;
        /* A alone boosts forward; LT selects reverse. Brake has priority. */
        float drive = superboost ? (throttle < -.01f ? -1.0f : 1.0f)
                                 : boost && fabsf(throttle)<.01f ? 1.0f : throttle;
        velocity=assistedFlight(velocity,flightForward,drive,
                                superboost?2:boost?1:0,braking,dt);
      } else {
        int grounded = vertical <= .5f &&
                       alt - bodyHeight(eye) <= 2.65f;
        /* Air control must preserve thrust momentum instead of applying the
         * walking servo's 10/s braking to every airborne frame. */
        float walkBlend = 1 - expf(-dt * (grounded && !rise ? 10.0f : .8f));
        tangent = add(tangent, mul(add(targetVelocity, mul(tangent, -1)), walkBlend));
        vertical -= (nearMoon(eye)?3.2f:12.0f) * dt;
        if (rise && jetFuel > 0) {
          V3 thrust = mul(jetpackDirection(radial, heading, pitch), 30);
          float lift = dot(thrust, radial);
          vertical += lift * dt;
          tangent = add(tangent, mul(add(thrust, mul(radial, -lift)), dt));
          jetFuel = fmaxf(0, jetFuel - dt * .16f);
        } else
          jetFuel = fminf(1, jetFuel + dt * .12f);
        float tangentSpeed = sqrtf(dot(tangent, tangent));
        if (tangentSpeed > 24) tangent = mul(tangent, 24 / tangentSpeed);
        velocity = add(tangent, mul(radial, clampf(vertical, -50, 24)));
      }
      if (tour)
        velocity = add(mul(r, 18), mul(radial, dot(velocity, radial)));
      if (landing) {
        pitch *= expf(-dt * 2);
        roll *= expf(-dt * 2);
        flightForward =
            norm(add(mul(heading, cosf(pitch)), mul(radial, sinf(pitch))));
        flightUp = radial;
        throttle = 0;
      }
      if (landing)
        velocity = mul(
            radial, -fminf(30, 5 + (alt - bodyHeight(eye)) * .15f));
      /* Substeps keep ground collision reliable during fast low flight. */
      V3 segment = mul(velocity, dt);
      float along = clampf(
          -dot(eye, segment) / fmaxf(dot(segment, segment), 1e-12f), 0, 1);
      V3 closest = add(eye, mul(segment, along));
      V3 mc=add(eye,mul(moonCenter(),-1));
      float ma=clampf(-dot(mc,segment)/fmaxf(dot(segment,segment),1e-12f),0,1);
      V3 cm=add(mc,mul(segment,ma));
      int clear = dot(closest, closest) >
                  (RADIUS + geoMaximum + 3) * (RADIUS + geoMaximum + 3) &&
                  dot(cm,cm)>(MOON_RADIUS+2500)*(MOON_RADIUS+2500);
      int steps =
          clear ? 1 : (int)ceilf(sqrtf(dot(velocity, velocity)) * dt / 2);
      steps = (int)clampf(steps, 1, 1024);
      for (int step = 0; step < steps; step++) {
        eye = add(eye, mul(velocity, dt / steps));
        radial = bodyUp(eye);
        if (!clear && bodyAltitude(eye) < bodyHeight(eye)+2.5f) {
          eye = bodyFloor(eye,2.5f);
          float down = dot(velocity, radial);
          if (down < 0) velocity = add(velocity, mul(radial, -down));
        }

      }
      if (landing &&
          bodyAltitude(eye) - bodyHeight(eye) < 2.6f)
        exitShip();
    }
    streamUpdate();
    if (!nearMoon(eye)) natureUpdate();
    if (profileFrames && frameNo >= 30) {
      glFinish();
      profileStamp = SDL_GetPerformanceCounter();
      profileSamples++;
    }
    gpuCheckpoint("render-begin");render();gpuCheckpoint("render-end");
    if (frameNo == 0) {
      glFinish();
      checkGL("first frame");
      printf("FIRST_FRAME seconds=%.3f\n",
             (SDL_GetPerformanceCounter() - boot) /
                 (double)SDL_GetPerformanceFrequency());
    }
    int taking = shot || (capture && frameNo == frames - 1);
    if (taking || (frames && frameNo == frames - 1)) {
      if (taking)
        screenshot(capture ? capture : "screenshots/poor-mans-sky.ppm");
      printf("MOON patches=%d generated=%d near_clip=%.1f\n",moonDraws,moonBuilds,clipNear);
      printf("CLOUDS enabled=%d sprites=%d screen_area=%.2f OCCLUSION_AUTO active=%d cooldown=%d\n",cloudsEnabled,cloudDrawCount,cloudScreenArea,occlusionActive,occlusionCooldown);
      printf("CPU_FOG patches=%d exact_gpu=%d max_error=0.003\n", cpuFogPatches,
             gpuFogPatches);
      SDL_LockMutex(mutex);
      printf("NATURE quality=%d cells=%d trees=%d rocks=%d grass=%d "
             "gpu_mib=%.2f\n",
             natureQuality, natureDraws, natureVisibleTrees, natureVisibleRocks,
             natureVisibleGrass, natureGPUBytes / 1048576.0);
      printf("SUN_SHADOW enabled=%d ready=%d updates=%d casters=%d receivers=%d total_ms=%.3f\n",sunShadows,sunReady,sunUpdates,sunCasterDraws,sunReceiverDraws,sunShadowMS);
      printf("NATURE_FADE active=%d started=%d duration_ms=650\n",natureFading,natureFadeStarted);
      printf("NATURE_BATCH groups=%d draws=%d rebuilds=%d\n",natureGroupUsed,natureGroupDraws,natureGroupRebuilds);
      printf("OCCLUSION tested=%d hidden=%d issued=%d epoch=%u\n",
             occlusionTested, occlusionHidden, occlusionIssued, occlusionEpoch);
      printf("VISIBILITY selected=%d visible=%d frustum_rejected=%d "
             "horizon_rejected=%d\n",
             selectedCount, visibleCount, frustumRejected, horizonRejected);
      printf("LOD_BUDGET desired=%d ring_scale=%.3f geometry_tolerance=%.2f "
             "recent_fps=%.2f\n",
             desiredTiles, ringScale, meshTolerance, fps);
      int waiting = 0, active = 0;
      for (int k = 0; k < countNode; k++) {
        if (nodes[k].wanted == frameNo) {
          active++;
          if (nodes[k].state == 2 && nodes[k].slot < 0)
            waiting++;
        }
      }
      printf("COVERAGE active=%d ready_without_slot=%d triangles=%d drawn=%d\n",
             active, waiting, triangles, drawn);
      printf("CAPTURE frame=%d selected=%d queue=%d resident=%d daylight=%.3f "
             "solarHeight=%.3f\n",
             frameNo, selectedCount, qcount, resident, solarDaylight, dot(sun,norm(cameraEye)));
      SDL_UnlockMutex(mutex);
    }
    gpuCheckpoint("swap-begin");SDL_GL_SwapWindow(window);gpuCheckpoint("swap-end");
    double ms = (SDL_GetPerformanceCounter() - start) * 1000.0 / freq;
    if (frameNo > 10 && !taking) {
      frameSamples[frameSampleCount++ % 4096] = ms;
      elapsed += ms;
      samples++;
      windowMS += ms;
      windowFrames++;
    }
    if (windowFrames >= 30) {
      fps = 1000 * windowFrames / windowMS;
      windowMS = 0;
      windowFrames = 0;
    }
    SDL_LockMutex(mutex);
    frameNo++;
    SDL_UnlockMutex(mutex);
    if (frames && frameNo >= frames)
      running = 0;
  }
  SDL_LockMutex(mutex);
  quitWorker = 1;
  SDL_CondSignal(cond);
  SDL_UnlockMutex(mutex);
  SDL_WaitThread(worker, NULL);
  glFinish(); /* Complete FBO work before deleting resources or the drawable. */
  checkGL("last frame");
  if (frameSampleCount) {
    int n = frameSampleCount < 4096 ? frameSampleCount : 4096;
    for (int i = 1; i < n; i++) {
      double a = frameSamples[i];
      int j = i;
      while (j && frameSamples[j - 1] > a) {
        frameSamples[j] = frameSamples[j - 1];
        j--;
      }
      frameSamples[j] = a;
    }
    printf("FRAME_TIME samples=%d p50=%.3f p95=%.3f p99=%.3f recycled=%d\n", n,
           frameSamples[n / 2], frameSamples[(n - 1) * 95 / 100],
           frameSamples[(n - 1) * 99 / 100], recycledNodes);
  }
  printf("STATE altitude=%.1f pitch=%.3f throttle=%.2f mode=%s\n",
         sqrtf(dot(eye, eye)) - RADIUS, pitch, throttle,
         flying ? "FLY" : "WALK");
  printf("REFLECTION ready=%d terrain_patches=%d size=128 interval=%d updates=%d max_gap=%d MORPH "
         "transitions=%d active=%d\n",
         reflectionReady, reflectionDraws, reflectionInterval, reflectionUpdates,
         reflectionMaxGap, morphStarted, morphActive);
  printf("STREAM_PRIORITY directional=%d bubble=%.0f bands=4,16,64 margin_deg=10 retain_deg=16 replacements=%d\n", streamViewReady,streamBubble,priorityReplacements);
  printf("GENERATION jobs=%d worker_ms=%.1f\n", generatedJobs, generationMS);
  printf("TEXTURE_TRANSITIONS started=%d extra_vram_kib=256\n",
         textureTransitions);
  printf("BATCH draws=%d patches=%d extra_vram_mib=%.3f\n", batchDraws,
         batchPatches, batchBytes / 1048576.);
  printf("WORLD seed=%u radius=%.0f maxLOD=%d nature_uploads=%d\n", worldSeed,
         RADIUS, MAXLEVEL, natureUploads);
  printf("RESULT frames=%d fps=%.2f uploads=%d evictions=%d cache_hits=%d "
         "RAM=%.1fMiB nodes=%d resident=%d\n",
         frameNo, samples ? 1000 * samples / elapsed : 0, uploaded, evicted,
         ramHits, (ramBytes+moonRAMBytes) / 1048576.0, countNode, resident);
  if (profileSamples) {
    printf("PROFILE samples=%d terrain_ms=%.3f water_ms=%.3f "
           "nature_ms=%.3f sky_ms=%.3f post_ms=%.3f actors_ms=%.3f\n",
           profileSamples, profileTimes[0] / profileSamples,
           profileTimes[1] / profileSamples, profileTimes[2] / profileSamples,
           profileTimes[3] / profileSamples, profileTimes[4] / profileSamples,
           profileTimes[5] / profileSamples);
  }
  for (int i = 0; i < countNode; i++) {
    free(nodes[i].pixels);
    free(nodes[i].vertices);
    free(nodes[i].stitched);
  }
  sunShadowClose();moonClose();cloudsClose();glDeleteBuffers(1,&waterSeamVBO);
  natureClose();
  free(geology);
  cacheClose();
  SDL_DestroyCond(cond);
  SDL_DestroyMutex(mutex);
  SDL_GL_DeleteContext(context);
  SDL_DestroyWindow(window);
  if (pad)
    SDL_GameControllerClose(pad);
  gpuCheckpoint("shutdown-complete");if(gpuTrace){fclose(gpuTrace);gpuTrace=NULL;}
  SDL_Quit();
  if (gameModeEnd)
    gameModeEnd();
  if (gameModeLib)
    dlclose(gameModeLib);
  return 0;
}
