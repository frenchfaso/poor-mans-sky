// SPDX-License-Identifier: MPL-2.0
#define main poor_mans_sky_application_main
#include "../poor-mans-sky.c"
#undef main
#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x)) {                                                                \
      fprintf(stderr, "FAIL edge %d: %s\n", __LINE__, #x);                     \
      return 1;                                                                \
    }                                                                          \
  } while (0)
int main(void) {
  CHECK(!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER));
  window = SDL_CreateWindow("Terrain seams", 0, 0, 64, 64,
                            SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  CHECK(window);
  context = SDL_GL_CreateContext(window);
  CHECK(context);
  mutex = SDL_CreateMutex();
  materialInit();
  cacheEnabled = 0;
  geological = 0;
  for (int i = 0; i < MAXNODE; i++)
    previousIndex[i] = -1;
  for (int i = 0; i < 6; i++)
    newNode(i, 0, 0, 0);
  for (int i = 0; i < 4; i++) {
    int id = newNode(4, 1, i & 1, i >> 1);
    Node *n = &nodes[id];
    nodes[4].child[i] = id;
    generate(n, &n->pixels, &n->vertices);
    n->slot = i;
    n->meshLevel = i ? 3 : 0;
    n->used = frameNo = 100;
    glGenBuffers(1, &n->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, n->vbo);
    glBufferData(GL_ARRAY_BUFFER, NV * sizeof(Vertex), n->vertices,
                 GL_STATIC_DRAW);
    selected[i] = id;
    previousIndex[id] = i;
    measureBounds(n);
    ramBytes += PAGE_BYTES + NV * sizeof(Vertex);
  }
  selectedCount = 4;
  for (int i = 0; i < MORPH_SLOTS; i++)
    lodMorphs[i].id = -1;
  stitchTerrainEdges();
  Node *fine = &nodes[6], *coarse = &nodes[7];
  CHECK(fine->stitched);
  for (int k = 1; k < PATCH; k++) {
    int j = k * (PATCH + 1) + PATCH;
    Vertex expected = sampleMesh(coarse->vertices, 0, k / (float)PATCH, 3), gpu;
    expected.p = add(expected.p, add(coarse->center, mul(fine->center, -1)));
    V3 error = add(expected.p, mul(fine->stitched[j].p, -1));
    CHECK(dot(error, error) < .05f);
    glBindBuffer(GL_ARRAY_BUFFER, fine->vbo);
    glGetBufferSubData(GL_ARRAY_BUFFER, j * sizeof(Vertex), sizeof(Vertex),
                       &gpu);
    CHECK(!memcmp(&gpu.p, &fine->stitched[j].p, sizeof(V3)));
  }
  fine->visible=1;fine->minHeight=-1;
  prepareWaterSeams();
  for(int k=1;k<PATCH;k++) {
    Vertex expected=sampleMesh(waterGrids[coarse->slot],0,k/(float)PATCH,3);
    V3 actual=waterGrids[fine->slot][k*(PATCH+1)+PATCH].p;
    V3 delta=add(add(expected.p,add(coarse->center,mul(fine->center,-1))),mul(actual,-1));
    CHECK(dot(delta,delta)<.0001f);
  }
  /* Two cube faces: one coarse patch, four children beside it. */
  moonSelectedCount=5;
  for(int i=0;i<5;i++) {
    MoonPatch *p=&moonPatches[i];p->face=i?0:4;p->level=i?1:0;p->x=i?(i-1)&1:0;p->y=i?(i-1)>>1:0;
    MoonVertex vertices[MOON_VERTS];moonMesh(p->face,p->level,p->x,p->y,&p->center,vertices);
    glGenBuffers(1,&p->vbo);glBindBuffer(GL_ARRAY_BUFFER,p->vbo);glBufferData(GL_ARRAY_BUFFER,sizeof(vertices),vertices,GL_DYNAMIC_DRAW);
    moonRememberGrid(i,vertices);moonSelected[i]=p;
  }
  for(int pose=0;pose<4;pose++) {
    cameraEye=add(moonCenter(),v3(45000+pose*18000,1000,45000));moonStitch();
    int links=0;
    for(int i=0;i<5;i++)for(int j=0;j<MOON_POINTS;j++) {
      MoonLink link=moonLinks[i][j];if(link.slot<0)continue;links++;
      MoonVertex expected=moonGridSample(moonGrid[link.slot],link.u,link.v);
      V3 delta=add(add(expected.p,add(moonPatches[link.slot].center,mul(moonPatches[i].center,-1))),mul(moonGrid[i][j].p,-1));
      CHECK(dot(delta,delta)<.0001f);CHECK(dot(expected.n,moonGrid[i][j].n)>.9999f);
    }
    CHECK(links>40);moonStitch();CHECK(moonSeamUploads==0);
  }
  coarse->meshLevel = 0;
  stitchTerrainEdges();
  for (int k = 1; k < PATCH; k++) {
    int j = k * (PATCH + 1) + PATCH;
    Vertex *current = fine->stitched ? fine->stitched : fine->vertices;
    V3 error = add(current[j].p, mul(fine->vertices[j].p, -1));
    CHECK(dot(error, error) < .0001f);
  }
  fine->visible=1;fine->minHeight=-1;prepareWaterSeams();
  for(int k=1;k<PATCH;k++) {
    Vertex a=waterGrids[fine->slot][k*(PATCH+1)+PATCH],b=waterGrids[coarse->slot][k*(PATCH+1)];
    V3 delta=add(add(a.p,fine->center),mul(add(b.p,coarse->center),-1));CHECK(dot(delta,delta)<.005f);
  }
  CHECK(glGetError() == GL_NO_ERROR);
  puts("PASS sea projection and moon cross-face/morph seams, stationary upload reuse; actual VBO "
       "readback, restoration after equal LOD");
  SDL_GL_DeleteContext(context);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
