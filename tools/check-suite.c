// SPDX-License-Identifier: MPL-2.0
#define main poor_mans_sky_application_main
#include "../src/poor-mans-sky.c"
#undef main
#include <sys/wait.h>

#undef CHECK

static int test_world(void) {
  worldSeed = 20260911;
  V3 d = norm(v3(.31f, .52f, .79f));
  float first = elevation(d), lo = 1e9f, hi = -1e9f;
  worldSeed = 42;
  float other = elevation(d);
  worldSeed = 20260911;
  float repeated = elevation(d);
  printf("Seed samples: %.6f / %.6f / %.6f\n", first, other, repeated);
  if (fabsf(first - repeated) > .001f || fabsf(first - other) < 1)
    return 1;
  for (int face = 0; face < 6; face++)
    for (int y = 0; y < 20; y++)
      for (int x = 0; x < 20; x++) {
        float h =
            elevation(direction(face, -1 + x * 2.0f / 19, -1 + y * 2.0f / 19));
        if (!isfinite(h))
          return 2;
        lo = fminf(lo, h);
        hi = fmaxf(hi, h);
      }
  for (int j = 0; j <= 64; j++) {
    float v = -1 + j / 32.0f;
    if (fabsf(elevation(direction(0, 1, v)) - elevation(direction(5, -1, v))) >
        .01f)
      return 3;
  }
  if (hi - lo < 4000)
    return 4;
  printf("PASS seed repeatability, distinct seeds, finite terrain, shared face "
         "edge; sampled relief %.0f..%.0f metres\n",
         lo, hi);
  return 0;
}

#undef CHECK

static int test_render(void) {
  SDL_Init(SDL_INIT_TIMER);
  cond = SDL_CreateCond();
  mutex = SDL_CreateMutex();
  nv = calloc(NATURE_VERTS, sizeof(*nv));
  nn = 0;
  nrng = 123;
  blob(v3(0, 0, 0), v3(1, 0, 0), v3(0, 1, 0), v3(0, 0, 1), v3(1, 1, 1),
       v3(1, 1, 1), 0);
  for (int i = 0; i < nn; i += 3)
    if (dot(nv[i].p, nv[i].n) <= 0)
      return 1;
  printf("PASS outward normals on %d nondegenerate rock triangles\n", nn / 3);
  free(nv);
  Node detailProbe = {0};
  detailProbe.size = 5;
  detailProbe.level = 14;
  eye = v3(0, 0, RADIUS + 10);
  detailProbe.center = v3(0, 0, RADIUS);
  terrainDetail = textureDetail = 1;
  if (wantsSplit(&detailProbe))
    return 11;
  detailProbe.size = 16;
  if (!wantsSplit(&detailProbe))
    return 12;
  puts("PASS adaptive split respects the minimum patch size");
  frameNo = 100;
  countNode = 5;
  eye = v3(0, 0, RADIUS + 10);
  for (int i = 0; i < 5; i++) {
    nodes[i] = (Node){0};
    nodes[i].face = 4;
    nodes[i].center = eye;
    nodes[i].size = i ? 12 : 24;
    nodes[i].level = i ? MAXLEVEL : MAXLEVEL - 1;
    nodes[i].slot = i ? i : -1;
    for (int j = 0; j < 4; j++)
      nodes[i].child[j] = -1;
  }
  for (int j = 0; j < 4; j++)
    nodes[0].child[j] = j + 1;
  if (!hasCoverage(0))
    return 2;
  selectNode(0);
  if (selectedCount != 4 || nodes[0].wanted == frameNo)
    return 3;
  int selection[4];
  memcpy(selection, selected, sizeof(selection));
  cullForward = v3(0, 0, -1);
  selectedCount = 0;
  selectNode(0);
  if (selectedCount != 4 || memcmp(selection, selected, sizeof(selection)))
    return 4;
  /* Retreat rapidly: coarse parent was evicted, children still resident. */
  eye.z += 100;
  selectedCount = 0;
  selectNode(0);
  if (selectedCount != 4) {
    fprintf(stderr, "FAIL coarsening left %d/4 patches visible\n",
            selectedCount);
    return 10;
  }
  eye.z -= 100;
  printf("PASS resident children cover pending coarse parent\n");
  nodes[0].slot = 99;
  nodes[3].slot = -1;
  selectedCount = 0;
  selectNode(0);
  if (selectedCount != 1 || selected[0] != 0 || nodes[3].wanted != frameNo)
    return 5;
  printf("PASS descendant coverage without resident parent, 180-degree "
         "invariant selection, fallback on missing child\n");
  Node n = {0};
  n.vertices = calloc(NV, sizeof(Vertex));
  n.size = 24;
  n.center = eye;
  for (int y = 0; y <= PATCH; y++)
    for (int x = 0; x <= PATCH; x++)
      n.vertices[y * (PATCH + 1) + x].p = v3(x, 0, y);
  measureGeometry(&n);
  if (n.geomError[3] > .001)
    return 6;
  n.vertices[12 * (PATCH + 1) + 12].p.y = 5;
  measureGeometry(&n);
  if (n.geomError[3] < 4.9)
    return 7;
  printf(
      "PASS geometric error detects relief and preserves planar decimation\n");
  countNode = 0;
  eye = mul(homeDirection(), RADIUS + 70.5f);
  ringScale = 1.2f;
  for (int face = 0; face < 6; face++)
    newNode(face, 0, 0, 0);
  for (int i = 0; i < countNode; i++)
    if (residentRegion(&nodes[i]) && wantsSplit(&nodes[i]))
      for (int j = 0; j < 4; j++)
        nodes[i].child[j] =
            newNode(nodes[i].face, nodes[i].level + 1, nodes[i].x * 2 + (j & 1),
                    nodes[i].y * 2 + (j >> 1));
  desiredTiles = 1000;
  planCover();
  if (desiredTiles > 940) {
    fprintf(stderr, "FAIL budget tiles=%d ring=%.3f\n", desiredTiles,
            ringScale);
    return 8;
  }
  for (int i = 0; i < countNode; i++)
    nodes[i].slot = i + 1;
  selectedCount = requestCount = 0;
  for (int i = 0; i < 6; i++)
    selectNode(i);
  int selectedCopy[1024], countCopy = selectedCount;
  memcpy(selectedCopy, selected, selectedCount * sizeof(int));
  cullForward = v3(0, 0, 1);
  selectedCount = requestCount = 0;
  planCover();
  for (int i = 0; i < 6; i++)
    selectNode(i);
  if (selectedCount != countCopy ||
      memcmp(selectedCopy, selected, selectedCount * sizeof(int)))
    return 9;
  printf("PASS full-world coverage budget: %d leaves, ring scale %.3f, yaw "
         "invariant\n",
         desiredTiles, ringScale);
  free(n.vertices);
  SDL_DestroyCond(cond);
  SDL_DestroyMutex(mutex);
  SDL_Quit();
  return 0;
}

#undef CHECK

#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x)) {                                                                \
      fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x);                     \
      return 1;                                                                \
    }                                                                          \
  } while (0)
static int test_visibility(void) {
  cameraEye = v3(0, 0, 0);
  viewForward = v3(0, 0, -1);
  viewRight = v3(1, 0, 0);
  viewUp = v3(0, 1, 0);
  clipNear = 1;
  clipFar = 100;
  width = 1024;
  height = 768;
  visibilitySetup();
  CHECK(boxInFrustum(v3(0, 0, -10), v3(1, 1, 1)));
  CHECK(!boxInFrustum(v3(0, 0, 10), v3(1, 1, 1)));
  CHECK(!boxInFrustum(v3(0, 0, -102), v3(.5, .5, .5)));
  CHECK(!boxInFrustum(v3(0, 20, -10), v3(1, 1, 1)));
  CHECK(!boxInFrustum(v3(20, 0, -10), v3(1, 1, 1)));
  CHECK(boxInFrustum(v3(0, 0, 0), v3(2, 2, 2)));
  CHECK(boxInFrustum(v3(0, 0, -100), v3(1, 1, 1)));
  CHECK(boxInFrustum(v3(7.698f, 0, -10), v3(.01, .01, .01)));
  CHECK(boxInFrustum(v3(0, 5.7735f, -10), v3(.01, .01, .01)));
  puts(
      "PASS six frustum planes, intersecting near/far planes and screen edges");
  cameraEye = v3(0, 0, RADIUS + 20000);
  CHECK(behindPlanet(v3(0, 0, -RADIUS), 100));
  CHECK(!behindPlanet(v3(0, 0, RADIUS), 100));
  float tangentZ = RADIUS * RADIUS / (RADIUS + 20000);
  CHECK(!behindPlanet(
      v3(sqrtf(RADIUS * RADIUS - tangentZ * tangentZ), 0, tangentZ), 100));
  CHECK(!behindPlanet(cameraEye, 100));
  puts("PASS planet horizon: far side rejected; near side, limb and camera "
       "retained");
  for (int face = 0; face < 6; face++)
    for (int y = 0; y < 3; y++)
      for (int x = 0; x < 3; x++) {
        V3 mid =
            direction(face, -1 + (x + .5f) * 2 / 3, -1 + (y + .5f) * 2 / 3);
        for (int j = 0; j < 4; j++) {
          V3 d = direction(face, -1 + (x + (j & 1)) * 2.0f / 3,
                           -1 + (y + (j >> 1)) * 2.0f / 3);
          CHECK(dot(mid, d) * (RADIUS - 7165) > RADIUS * .8f);
        }
      }
  puts("PASS horizon core remains inside even the coarsest root surface "
       "triangles");
  Node n = {0};
  n.center = v3(0, 0, RADIUS);
  n.vertices = calloc(NV, sizeof(Vertex));
  for (int i = 0; i < NV; i++) {
    n.vertices[i].p = v3(i % 25 - 12, i / 25 - 14, (i % 9) * 7 - 21);
    n.vertices[i].h = n.vertices[i].p.z;
  }
  measureBounds(&n);
  for (int i = 0; i < NV; i++)
    for (int water = 0; water < 2; water++) {
      V3 p = add(n.center, n.vertices[i].p);
      if (water)
        p = add(p, mul(norm(p), -n.vertices[i].h + .38f));
      V3 d = add(p, mul(n.boundCenter, -1));
      CHECK(fabsf(d.x) <= n.boundHalf.x && fabsf(d.y) <= n.boundHalf.y &&
            fabsf(d.z) <= n.boundHalf.z);
    }
  free(n.vertices);
  puts("PASS bounds include terrain, skirts and displaced sea vertices");
  occlusionEnabled = 0;
  selectedCount = 0;
  occlusionPrepare();
  unsigned epoch = occlusionEpoch;
  occlusionPrepare();
  CHECK(occlusionEpoch == epoch);
  cameraEye.x += .01f;
  occlusionPrepare();
  CHECK(occlusionEpoch != epoch);
  epoch = occlusionEpoch;
  viewForward = mul(viewForward, -1);
  occlusionPrepare();
  CHECK(occlusionEpoch != epoch);
  epoch = occlusionEpoch;
  rw--;
  occlusionPrepare();
  CHECK(occlusionEpoch != epoch);
  puts("PASS occlusion results invalidate on translation, rotation and "
       "resolution change");
  ActorVertex mesh[9] = {
      {{1, 0, 0}, {1, 0, 0}, 0, 0}, {{1, 0, 1}, {1, 0, 0}, 1, 0},
      {{1, 1, 0}, {1, 0, 0}, 0, 1}, {{1, 0, 0}, {1, 0, 0}, 0, 0},
      {{1, 1, 0}, {1, 0, 0}, 0, 1}, {{1, 0, 1}, {1, 0, 0}, 1, 0},
      {{1, 0, 0}, {1, 0, 0}, 0, 0}, {{1, 0, 0}, {1, 0, 0}, 0, 0},
      {{1, 0, 0}, {1, 0, 0}, 0, 0}};
  unsigned short ix[9];
  int count = 0;
  CHECK(indexActorMesh(mesh, 9, ix, &count) == 3 && count == 6);
  for (int i = 0; i < count; i += 3) {
    V3 a = mesh[ix[i]].p, b = mesh[ix[i + 1]].p, c = mesh[ix[i + 2]].p;
    CHECK(dot(cross(add(b, mul(a, -1)), add(c, mul(a, -1))), mesh[ix[i]].n) >
          0);
  }
  puts("PASS actor indexing preserves attributes, fixes winding and removes "
       "zero-area triangles");
  return 0;
}

#undef CHECK

static int test_fog(void) {
  cameraEye = v3(0, 0, 0);
  int accepted = 0, rejected = 0;
  float worst = 0;
  for (int d = 0; d < 9; d++)
    for (int r = 0; r < 8; r++)
      for (int h = 0; h < 3; h++) {
        float distance = 10 * powf(3, d), radius = .5f * powf(3, r),
              factor = h == 0   ? 1
                       : h == 1 ? .5f
                                : .01f;
        Node n = {0};
        n.center = v3(distance * .6f, 0, distance * .8f);
        n.boundCenter = n.center;
        n.boundRadius = radius;
        float plane[4];
        if (!fogCoefficients(&n, factor, plane)) {
          rejected++;
          continue;
        }
        accepted++;
        for (int i = 0; i < 500; i++) {
          float z = 1 - 2 * (i + .5f) / 500.0f, a = i * 2.39996323f;
          V3 p = v3(radius * sqrtf(1 - z * z) * cosf(a), radius * z,
                    radius * sqrtf(1 - z * z) * sinf(a));
          V3 delta = add(n.center, p);
          float exact =
              factor * (1 - exp2f(-sqrtf(dot(delta, delta)) * .00009f));
          float approx = clampf(p.x * plane[0] + p.y * plane[1] +
                                    p.z * plane[2] + plane[3],
                                0, factor);
          float error = fabsf(exact - approx);
          worst = fmaxf(worst, error);
          if (error > .00302f) {
            fprintf(stderr, "FAIL fog error %.6f distance %.1f radius %.1f\n",
                    error, distance, radius);
            return 1;
          }
        }
      }
  if (!accepted || !rejected)
    return 2;
  printf("PASS fog error bound: %d accepted, %d fallback, worst sampled error "
         "%.6f\n",
         accepted, rejected, worst);
  return 0;
}

#undef CHECK

static int test_index(void) {
  SDL_Init(SDL_INIT_TIMER);
  cacheDirectory = "/tmp/poor-mans-sky-index-test-v3";
  cacheInit();
  materialInit();
  initTreeModels();
  V3 d = homeDirection();
  spawnPoint = mul(d, RADIUS + fmaxf(elevation(d), 0));
  int face = d.y > d.z ? 2 : 4;
  float u = face == 2 ? d.x / d.y : d.x / d.z,
        v = face == 2 ? -d.z / d.y : d.y / d.z;
  int cx = (int)((u + 1) * 4096), cy = (int)((v + 1) * 4096);
  unsigned long total = 0, unique = 0;
  for (int q = 0; q < 3; q++)
    for (int lod = 0; lod < 3; lod++)
      for (int y = -2; y <= 2; y++)
        for (int x = -2; x <= 2; x++) {
          natureQuality = q;
          NatureCell c = {0};
          c.face = face;
          c.x = cx + x;
          c.y = cy + y;
          c.buildLod = lod;
          buildNature(&c);
          NaturePacked *v=(NaturePacked*)c.packed;
          unsigned short *ix=(unsigned short*)(c.packed+c.unique*sizeof(*v));
          NatureCell fresh=c;fresh.packed=NULL;buildNatureFresh(&fresh);
          for(int k=0;k<c.count;k++) {
            if(ix[k]>=c.unique)return 1;
            NatureVertex q=fresh.cpu[k];NaturePacked actual=v[ix[k]];
            if(memcmp(&actual.p,&q.p,sizeof(V3)) || actual.u!=q.u || actual.v!=q.v ||
               fabsf(actual.r/255.f-q.r)>.0021f || fabsf(actual.g/255.f-q.g)>.0021f ||
               fabsf(actual.b/255.f-q.b)>.0021f || dot(unpackNormal(actual.n),q.n)<.98f)return 1;
          }
          total+=c.count;unique+=c.unique;
          free(fresh.cpu);free(c.packed);
        }
  printf("PASS 225 indexed cells preserve every triangle attribute exactly: "
         "%lu -> %lu vertices\n",
         total, unique);
  cacheClose();
  SDL_Quit();
  return 0;
}


#undef CHECK

#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x)) {                                                                \
      fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x);                          \
      return 1;                                                                \
    }                                                                          \
  } while (0)
static int test_morph(void) {
  CHECK(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  window = SDL_CreateWindow("Morph test", 0, 0, 64, 64,
                            SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  CHECK(window);
  context = SDL_GL_CreateContext(window);
  CHECK(context);
  Node *n = &nodes[0];
  n->vertices = calloc(NV, sizeof(Vertex));
  CHECK(n->vertices);
  n->center = v3(0, 0, RADIUS);
  n->size = 24;
  n->slot = 0;
  n->face = 4;
  for (int y = 0; y <= PATCH; y++)
    for (int x = 0; x <= PATCH; x++) {
      Vertex *v = &n->vertices[y * (PATCH + 1) + x];
      v->p = v3(x, (x == 12 && y == 12) ? 5 : 0, y);
      v->h = v->p.y;
      v->n = packNormal(v3(0, 1, 0));
      v->u = x / (float)PATCH;
      v->v = y / (float)PATCH;
    }
  measureBounds(n);
  V3 originalHalf = n->boundHalf;
  glGenBuffers(1, &n->vbo);
  glBindBuffer(GL_ARRAY_BUFFER, n->vbo);
  glBufferData(GL_ARRAY_BUFFER, NV * sizeof(Vertex), n->vertices,
               GL_DYNAMIC_DRAW);
  cullingMode = 0;
  cameraEye = v3(0, 0, RADIUS + 50);
  cullForward = v3(0, 0, -1);
  selected[0] = 0;
  selectedCount = 1;
  n->meshLevel = 3;
  n->used = frameNo = 0;
  prepareMorphs();
  n->meshLevel = 0;
  n->used = frameNo = 1;
  prepareMorphs();
  int m = morphFor(0), center = 12 * (PATCH + 1) + 12;
  CHECK(m >= 0);
  CHECK(fabsf(lodMorphs[m].current[center].p.y) < .001);
  lodMorphs[m].start -= 225;
  n->used = frameNo = 2;
  prepareMorphs();
  CHECK(lodMorphs[m].current[center].p.y > 2.3 &&
        lodMorphs[m].current[center].p.y < 2.8);
  lodMorphs[m].start -= 450;
  n->used = frameNo = 3;
  prepareMorphs();
  CHECK(morphFor(0) < 0 && n->meshLevel == 0);
  CHECK(fabsf(n->boundHalf.y - originalHalf.y) < .001);
  n->meshLevel = 3;
  n->used = frameNo = 4;
  prepareMorphs();
  m = morphFor(0);
  CHECK(m >= 0 && n->meshLevel == 0 && lodMorphs[m].reverse);
  CHECK(fabsf(lodMorphs[m].current[center].p.y - 5) < .001);
  lodMorphs[m].start -= 225;
  n->used = frameNo = 5;
  prepareMorphs();
  CHECK(lodMorphs[m].current[center].p.y > 2.2 &&
        lodMorphs[m].current[center].p.y < 2.7);
  lodMorphs[m].start -= 450;
  n->used = frameNo = 6;
  prepareMorphs();
  CHECK(morphFor(0) < 0 && n->meshLevel == 3);
  Node *child = &nodes[1];
  *child = *n;
  child->level = 1;
  child->size = 12;
  child->used = frameNo = 7;
  child->vertices = malloc(NV * sizeof(Vertex));
  CHECK(child->vertices);
  memcpy(child->vertices, n->vertices, NV * sizeof(Vertex));
  for (int j = 0; j < NV; j++) {
    child->vertices[j].p.x *= .5f;
    child->vertices[j].p.z *= .5f;
  }
  glGenBuffers(1, &child->vbo);
  glBindBuffer(GL_ARRAY_BUFFER, child->vbo);
  glBufferData(GL_ARRAY_BUFFER, NV * sizeof(Vertex), child->vertices,
               GL_DYNAMIC_DRAW);
  child->meshLevel = 0;
  selected[0] = 1;
  prepareMorphs();
  m = morphFor(1);
  CHECK(m >= 0);
  Vertex gpuVertex;
  glBindBuffer(GL_ARRAY_BUFFER, child->vbo);
  glGetBufferSubData(GL_ARRAY_BUFFER, center * sizeof(Vertex), sizeof(Vertex),
                     &gpuVertex);
  CHECK(fabsf(gpuVertex.p.y) < .001);
  CHECK(fabsf(gpuVertex.p.x - 6) < .001 && fabsf(gpuVertex.p.z - 6) < .001);
  V3 v = v3(.2f, .5f, .7f), normal = v3(0, 1, 0),
     mirrored = mirrorDirection(v, normal);
  CHECK(fabsf(dot(v, v) - dot(mirrored, mirrored)) < .0001);
  CHECK(fabsf(mirrored.y + v.y) < .0001);
  CHECK(glGetError() == GL_NO_ERROR);
  puts("PASS real VBO refinement/coarsening endpoints and midpoint, bound "
       "restoration, child starts on parent surface (GPU readback), reflection "
       "direction");
  SDL_GL_DeleteContext(context);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}

#undef CHECK

#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x)) {                                                                \
      fprintf(stderr, "FAIL edge %d: %s\n", __LINE__, #x);                     \
      return 1;                                                                \
    }                                                                          \
  } while (0)
static int test_edges(void) {
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
    ramBytes += TERRAIN_BYTES + NV * sizeof(Vertex);
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
  coarse->meshLevel = 0;
  stitchTerrainEdges();
  for (int k = 1; k < PATCH; k++) {
    int j = k * (PATCH + 1) + PATCH;
    Vertex *current = fine->stitched ? fine->stitched : fine->vertices;
    V3 error = add(current[j].p, mul(fine->vertices[j].p, -1));
    CHECK(dot(error, error) < .0001f);
  }
  CHECK(glGetError() == GL_NO_ERROR);
  puts("PASS fine/coarse boundary follows rendered neighbor, actual VBO "
       "readback, restoration after equal LOD");
  SDL_GL_DeleteContext(context);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}

#undef CHECK

#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x)) {                                                                \
      fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x);                          \
      return 1;                                                                \
    }                                                                          \
  } while (0)
static int test_upgrade(int argc, char **argv) {
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

#undef CHECK

#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x)) {                                                                \
      fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x);                     \
      return 1;                                                                \
    }                                                                          \
  } while (0)
static int test_nature(int argc,char **argv){
 SDL_Init(SDL_INIT_TIMER);materialInit();initTreeModels();
 cacheDirectory=argc>1?argv[1]:"/tmp/poor-mans-sky-nature-test";cacheInit();geologyInit();
 V3 d=homeDirection();spawnPoint=mul(d,RADIUS+fmaxf(elevation(d),0));
 int face=4,x=(int)((d.x/d.z+1)*4096),y=(int)((d.y/d.z+1)*4096);
 int trees[3]={0},verts[3]={0};
 for(int q=0;q<3;q++){
  natureQuality=q;
  for(int dy=-2;dy<=2;dy++)for(int dx=-2;dx<=2;dx++) {
   int counts[3];
   for(int lod=0;lod<3;lod++) {
    NatureCell c={0};c.face=face;c.x=x+dx;c.y=y+dy;c.buildLod=lod;buildNature(&c);
    CHECK(c.count<NATURE_VERTS && c.count%3==0 && c.solid<=c.count);
    NaturePacked *v=(NaturePacked*)c.packed;
    for(int i=0;i<c.unique;i++)CHECK(v[i].u>=0 && v[i].u<=1 && v[i].v>=0 && v[i].v<=1);
    counts[lod]=c.trees;
    if(q==1){trees[lod]+=c.trees;verts[lod]+=c.count;}
    if(lod>0)CHECK(c.grass==0);
    uint32_t first=cacheHash(c.packed,c.unique*sizeof(NaturePacked)+c.count*sizeof(uint16_t),2166136261u);
    NatureCell repeat=c;repeat.packed=NULL;buildNature(&repeat);
    CHECK(c.count==repeat.count && first==cacheHash(repeat.packed,repeat.unique*sizeof(NaturePacked)+repeat.count*sizeof(uint16_t),2166136261u));
    free(c.packed);free(repeat.packed);
   }
   CHECK(counts[0]==counts[1] && counts[1]==counts[2]);
  }
 }
 CHECK(verts[0]>verts[1] && verts[1]>verts[2]);
 printf("PASS seed/LOD/cache/UV/capacity: balanced trees=%d,%d,%d vertices=%d,%d,%d\n",trees[0],trees[1],trees[2],verts[0],verts[1],verts[2]);
 cacheClose();SDL_Quit();return 0;
}


#undef CHECK

static int test_cache(int argc, char **argv) {
  if (argc < 2 || argc > 3) return 2;
  if (argc == 3) worldSeed = (uint32_t)strtoul(argv[2], NULL, 10);
  SDL_Init(SDL_INIT_TIMER);
  cacheDirectory = argv[1]; cacheInit();geologyInit(); materialInit(); initTreeModels();
  Uint64 start = SDL_GetPerformanceCounter();
  V3 d = homeDirection();
  spawnPoint = mul(d, RADIUS + fmaxf(elevation(d), 0));
  int face = d.y > d.z ? 2 : 4;
  float u = face == 2 ? d.x/d.y : d.x/d.z;
  float w = face == 2 ? -d.z/d.y : d.y/d.z;
  uint32_t sum = 2166136261u;
  for (int level = 0; level <= 18; level++) {
    int x = (int)((u+1)*.5f*(1<<level));
    int y = (int)((w+1)*.5f*(1<<level));
    int id = newNode(face, level, x, y);
    unsigned char *p; Vertex *v;
    generate(&nodes[id], &p, &v);
    sum = cacheHash(p, PAGE_BYTES, sum);
    sum = cacheHash(v, NV*sizeof(*v), sum);
    free(p); free(v);
  }
  int x=(int)((u+1)*4096), y=(int)((w+1)*4096);
  for(int j=-2;j<=2;j++) for(int i=-2;i<=2;i++) {
    NatureCell cell = {0}; cell.face=face; cell.x=x+i; cell.y=y+j;
    buildNature(&cell);
    sum=cacheHash(cell.packed, cell.unique*sizeof(NaturePacked)+cell.count*sizeof(uint16_t), sum);
    sum=cacheHash(&cell.center, sizeof(cell.center), sum);
    free(cell.packed);
  }
  printf("CACHE_TEST checksum=%08x seconds=%.6f home=%.6f,%.6f,%.6f\n", sum,
    (SDL_GetPerformanceCounter()-start)/(double)SDL_GetPerformanceFrequency(),d.x,d.y,d.z);
  cacheClose(); SDL_Quit(); return 0;
}


int main(void) {
  setvbuf(stdout, NULL, _IOLBF, 0);
  int failures = 0;
  {
    char *args[] = {"check", "/tmp/poor-mans-sky-suite-world", NULL};
    (void)args;
    pid_t pid = fork();
    if (pid == 0) {
      int result = test_world();
      exit(result);
    }
    int status = 0;
    if (pid < 0 || waitpid(pid, &status, 0) < 0)
      return 99;
    int code = WIFEXITED(status) ? WEXITSTATUS(status) : 99;
    printf("SUITE world result=%d\n", code);
    failures += code != 0;
  }
  {
    char *args[] = {"check", "/tmp/poor-mans-sky-suite-render", NULL};
    (void)args;
    pid_t pid = fork();
    if (pid == 0) {
      int result = test_render();
      exit(result);
    }
    int status = 0;
    if (pid < 0 || waitpid(pid, &status, 0) < 0)
      return 99;
    int code = WIFEXITED(status) ? WEXITSTATUS(status) : 99;
    printf("SUITE render result=%d\n", code);
    failures += code != 0;
  }
  {
    char *args[] = {"check", "/tmp/poor-mans-sky-suite-visibility", NULL};
    (void)args;
    pid_t pid = fork();
    if (pid == 0) {
      int result = test_visibility();
      exit(result);
    }
    int status = 0;
    if (pid < 0 || waitpid(pid, &status, 0) < 0)
      return 99;
    int code = WIFEXITED(status) ? WEXITSTATUS(status) : 99;
    printf("SUITE visibility result=%d\n", code);
    failures += code != 0;
  }
  {
    char *args[] = {"check", "/tmp/poor-mans-sky-suite-fog", NULL};
    (void)args;
    pid_t pid = fork();
    if (pid == 0) {
      int result = test_fog();
      exit(result);
    }
    int status = 0;
    if (pid < 0 || waitpid(pid, &status, 0) < 0)
      return 99;
    int code = WIFEXITED(status) ? WEXITSTATUS(status) : 99;
    printf("SUITE fog result=%d\n", code);
    failures += code != 0;
  }
  {
    char *args[] = {"check", "/tmp/poor-mans-sky-suite-index", NULL};
    (void)args;
    pid_t pid = fork();
    if (pid == 0) {
      int result = test_index();
      exit(result);
    }
    int status = 0;
    if (pid < 0 || waitpid(pid, &status, 0) < 0)
      return 99;
    int code = WIFEXITED(status) ? WEXITSTATUS(status) : 99;
    printf("SUITE index result=%d\n", code);
    failures += code != 0;
  }
  {
    char *args[] = {"check", "/tmp/poor-mans-sky-suite-morph", NULL};
    (void)args;
    pid_t pid = fork();
    if (pid == 0) {
      int result = test_morph();
      exit(result);
    }
    int status = 0;
    if (pid < 0 || waitpid(pid, &status, 0) < 0)
      return 99;
    int code = WIFEXITED(status) ? WEXITSTATUS(status) : 99;
    printf("SUITE morph result=%d\n", code);
    failures += code != 0;
  }
  {
    char *args[] = {"check", "/tmp/poor-mans-sky-suite-edges", NULL};
    (void)args;
    pid_t pid = fork();
    if (pid == 0) {
      int result = test_edges();
      exit(result);
    }
    int status = 0;
    if (pid < 0 || waitpid(pid, &status, 0) < 0)
      return 99;
    int code = WIFEXITED(status) ? WEXITSTATUS(status) : 99;
    printf("SUITE edges result=%d\n", code);
    failures += code != 0;
  }
  {
    char *args[] = {"check", "/tmp/poor-mans-sky-suite-upgrade", NULL};
    (void)args;
    pid_t pid = fork();
    if (pid == 0) {
      int result = test_upgrade(2, args);
      exit(result);
    }
    int status = 0;
    if (pid < 0 || waitpid(pid, &status, 0) < 0)
      return 99;
    int code = WIFEXITED(status) ? WEXITSTATUS(status) : 99;
    printf("SUITE upgrade result=%d\n", code);
    failures += code != 0;
  }
  {
    char *args[] = {"check", "/tmp/poor-mans-sky-suite-nature", NULL};
    (void)args;
    pid_t pid = fork();
    if (pid == 0) {
      int result = test_nature(2, args);
      exit(result);
    }
    int status = 0;
    if (pid < 0 || waitpid(pid, &status, 0) < 0)
      return 99;
    int code = WIFEXITED(status) ? WEXITSTATUS(status) : 99;
    printf("SUITE nature result=%d\n", code);
    failures += code != 0;
  }
  {
    char *args[] = {"check", "/tmp/poor-mans-sky-suite-cache", NULL};
    (void)args;
    pid_t pid = fork();
    if (pid == 0) {
      int result = test_cache(2, args);
      exit(result);
    }
    int status = 0;
    if (pid < 0 || waitpid(pid, &status, 0) < 0)
      return 99;
    int code = WIFEXITED(status) ? WEXITSTATUS(status) : 99;
    printf("SUITE cache result=%d\n", code);
    failures += code != 0;
  }
  return failures ? 1 : 0;
}
