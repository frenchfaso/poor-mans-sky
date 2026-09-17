// SPDX-License-Identifier: MPL-2.0
/* Project finer boundary samples onto the actual coarser rendered triangles.
 * Runs after morphing. Geometry caches retain the original immutable surface.
 */
/* Use double cube coordinates for topology only: a small cross-edge offset
 * must remain representable even on the smallest patches near cube corners. */
static void seamDirection(const Node *n, double u, double v, double d[3]) {
  double scale = 2.0 / (1u << n->level);
  u = -1 + (n->x + u) * scale;
  v = -1 + (n->y + v) * scale;
  switch (n->face) {
  case 0:
    d[0] = 1;
    d[1] = v;
    d[2] = -u;
    break;
  case 1:
    d[0] = -1;
    d[1] = v;
    d[2] = u;
    break;
  case 2:
    d[0] = u;
    d[1] = 1;
    d[2] = -v;
    break;
  case 3:
    d[0] = u;
    d[1] = -1;
    d[2] = v;
    break;
  case 4:
    d[0] = u;
    d[1] = v;
    d[2] = 1;
    break;
  default:
    d[0] = -u;
    d[1] = v;
    d[2] = -1;
    break;
  }
}
static void seamChart(const double d[3], int face, double *u, double *v) {
  double den = d[face / 2];
  if (face & 1)
    den = -den;
  *u = face == 0   ? -d[2] / den
       : face == 1 ? d[2] / den
       : face == 5 ? -d[0] / den
                   : d[0] / den;
  *v = face == 2 ? -d[2] / den : face == 3 ? d[2] / den : d[1] / den;
}
static int seamCover(const Node *n, double u, double v) {
  double d[3];
  seamDirection(n, u, v, d);
  double x = fabs(d[0]), y = fabs(d[1]), z = fabs(d[2]);
  int face = x >= y && x >= z ? (d[0] > 0 ? 0 : 1)
             : y >= z         ? (d[1] > 0 ? 2 : 3)
                              : (d[2] > 0 ? 4 : 5);
  seamChart(d, face, &u, &v);
  int id = face;
  for (int depth = 0; depth <= MAXLEVEL; depth++) {
    Node *o = &nodes[id];
    if (previousIndex[id] >= 0)
      return id;
    if (o->child[0] < 0)
      return -1;
    int scale = 1 << (o->level + 1);
    int ix = (int)fmax(0, fmin(scale - 1, (u + 1) * .5 * scale));
    int iy = (int)fmax(0, fmin(scale - 1, (v + 1) * .5 * scale));
    id = o->child[(ix & 1) + 2 * (iy & 1)];
  }
  return -1;
}
typedef struct {
  int id;
  unsigned char edge, first, last, pad;
} SeamRun;
typedef struct {
  uint32_t topologyXor, topologySum;
  int id, count;
  SeamRun runs[4 * (PATCH + 1)];
} SeamNeighbors;
/* Cache topology per VRAM slot. Camera rotation and mesh LOD changes do not
 * repeat quadtree searches; run eligibility is still checked every frame. */
static SeamNeighbors seamNeighbors[SLOTS];
static const SeamNeighbors *findSeamNeighbors(int id, uint32_t tx,
                                              uint32_t ts) {
  Node *n = &nodes[id];
  SeamNeighbors *cache = &seamNeighbors[n->slot];
  if (cache->count && cache->id == id && cache->topologyXor == tx &&
      cache->topologySum == ts)
    return cache;
  cache->count = 0;
  cache->id = id;
  cache->topologyXor = tx;
  cache->topologySum = ts;
  for (int e = 0; e < 4; e++)
    for (int k = 0; k <= PATCH; k++) {
      double t = k / (double)PATCH, tangent = fmax(.000001, fmin(.999999, t));
      double u = e == 1   ? 1.000001
                 : e == 3 ? -.000001
                 : e == 2 ? 1 - tangent
                          : tangent;
      double v = e == 0   ? -.000001
                 : e == 2 ? 1.000001
                 : e == 3 ? 1 - tangent
                          : tangent;
      int other = seamCover(n, u, v);
      SeamRun *last = cache->count ? &cache->runs[cache->count - 1] : NULL;
      if (last && last->edge == e && last->id == other)
        last->last = k;
      else
        cache->runs[cache->count++] = (SeamRun){other, e, k, k, 0};
    }
  return cache;
}
static int edgeVertex(int edge, int k) {
  return edge == 0   ? k
         : edge == 1 ? k * (PATCH + 1) + PATCH
         : edge == 2 ? PATCH * (PATCH + 1) + PATCH - k
                     : (PATCH - k) * (PATCH + 1);
}
static int coarseFirst(const void *a, const void *b) {
  Node *x = &nodes[*(const int *)a], *y = &nodes[*(const int *)b];
  float sx = x->size * (1 << x->meshLevel), sy = y->size * (1 << y->meshLevel);
  return sx > sy ? -1 : sx < sy ? 1 : 0;
}
typedef struct { int id, slot, lod; GLuint vbo; } TerrainSeamState;
static TerrainSeamState terrainSeamState[1024];
static uint64_t terrainSeamRevision;
static int terrainSeamCount=-1, terrainSeamReused;
static int seamIdFirst(const void *a,const void *b) {
  return ((const TerrainSeamState*)a)->id-((const TerrainSeamState*)b)->id;
}
static void stitchTerrainEdges(void) {
  /* Morph/upload changes invalidate both terrain and sea geometry. Cache the
   * completed pass, including its own VBO updates, only for an identical cover.
   * Camera motion alone cannot change a seam while mesh LOD stays unchanged. */
  TerrainSeamState state[1024];
  for(int i=0;i<selectedCount;i++) {
    Node *n=&nodes[selected[i]];
    state[i]=(TerrainSeamState){selected[i],n->slot,n->meshLevel,n->vbo};
  }
  terrainSeamReused=0;
  if(terrainSeamCount==selectedCount && terrainSeamRevision==terrainGeometryRevision) {
    terrainSeamReused=!memcmp(state,terrainSeamState,selectedCount*sizeof(*state));
    if(!terrainSeamReused) {
      /* Usually already in the same order: only sort on a camera reorder. */
      TerrainSeamState sorted[1024];
      memcpy(sorted,state,selectedCount*sizeof(*state));
      qsort(sorted,selectedCount,sizeof(*sorted),seamIdFirst);
      qsort(terrainSeamState,selectedCount,sizeof(*state),seamIdFirst);
      terrainSeamReused=!memcmp(sorted,terrainSeamState,selectedCount*sizeof(*state));
      memcpy(terrainSeamState,state,selectedCount*sizeof(*state));
    }
  }
  if(terrainSeamReused)return;
  uint32_t topologyXor = 0, topologySum = (uint32_t)selectedCount;
  for (int i = 0; i < selectedCount; i++) {
    Node *n = &nodes[selected[i]];
    uint32_t identity[] = {selected[i], n->face, n->level, n->x, n->y};
    uint32_t h = cacheHash(identity, sizeof(identity), 2166136261u);
    topologyXor ^= h;
    topologySum += h;
  }
  /* Preserve the original ordering of equal-sized patches on rebuilds. */
  int order[1024];
  memcpy(order, selected, selectedCount * sizeof(int));
  qsort(order, selectedCount, sizeof(int), coarseFirst);
  for (int i = 0; i < selectedCount; i++) {
    int id = order[i];
    Node *n = &nodes[id];
    if (!n->vertices || !n->vbo || n->slot < 0 || n->slot >= SLOTS)
      continue;
    const SeamNeighbors *neighbors =
        findSeamNeighbors(id, topologyXor, topologySum);
    unsigned char enabled[4 * (PATCH + 1)] = {0};
    int active = morphFor(id), any = 0;
    uint32_t key = 2166136261u;
    for (int r = 0; r < neighbors->count; r++) {
      const SeamRun *run = &neighbors->runs[r];
      int other = run->id;
      if (other < 0 || other == id)
        continue;
      Node *o = &nodes[other];
      if (o->vertices && o->size * (1 << o->meshLevel) >
                             n->size * (1 << n->meshLevel) * 1.01f) {
        enabled[r] = 1;
        any = 1;
        uint32_t data[] = {run->edge,    run->first, run->last, other,
                           o->meshLevel, o->vbo,     o->edgeKey};
        key = cacheHash(data, sizeof(data), key);
        if (morphFor(other) >= 0)
          key ^= SDL_GetTicks();
      }
    }
    if (!any) {
      if (n->stitched) {
        if (active < 0)
          morphUpload(n, n->vertices);
        SDL_LockMutex(mutex);
        free(n->stitched);
        n->stitched = NULL;
        ramBytes -= NV * sizeof(Vertex);
        SDL_UnlockMutex(mutex);
        n->edgeKey = 0;
        Vertex *original = n->vertices;
        if (active >= 0)
          n->vertices = lodMorphs[active].current;
        measureBounds(n);
        n->vertices = original;
        lodRevision++;
      }
      continue;
    }
    key ^= (uint32_t)n->meshLevel * 7919;
    if (n->stitched && n->edgeKey == key && active < 0)
      continue;
    if (!n->stitched) {
      n->stitched = malloc(NV * sizeof(Vertex));
      if (!n->stitched)
        die("terrain seam allocation");
      SDL_LockMutex(mutex);
      ramBytes += NV * sizeof(Vertex);
      SDL_UnlockMutex(mutex);
    }
    memcpy(n->stitched, active >= 0 ? lodMorphs[active].current : n->vertices,
           NV * sizeof(Vertex));
    for (int r = 0; r < neighbors->count; r++)
      if (enabled[r]) {
        const SeamRun *run = &neighbors->runs[r];
        Node *o = &nodes[run->id];
        int om = morphFor(run->id);
        const Vertex *source = o->stitched ? o->stitched
                               : om >= 0   ? lodMorphs[om].current
                                           : o->vertices;
        double scale = 1.0 / (1u << o->level);
        for (int k = run->first; k <= run->last; k++) {
          Vertex *out = &n->stitched[edgeVertex(run->edge, k)];
          double d[3], u, v;
          seamDirection(n, out->u, out->v, d);
          seamChart(d, o->face, &u, &v);
          u = ((u + 1) * .5 - o->x * scale) / scale;
          v = ((v + 1) * .5 - o->y * scale) / scale;
          /* Never collapse vertices outside a neighbor onto its endpoint. */
          if (u < -.0001 || u > 1.0001 || v < -.0001 || v > 1.0001)
            continue;
          Vertex sample = sampleMesh(source, (float)u, (float)v, o->meshLevel);
          out->p = add(sample.p, add(o->center, mul(n->center, -1)));
          out->n = sample.n;
          out->h = sample.h;
        }
      }
    /* Corners belong to two edges. Rebuild each skirt from the final surface
     * once: incremental deltas use an already moved corner on the second edge
     * and can push its skirt tens of kilometres OUT of coarse orbital patches.
     */
    for (int e = 0; e < 4; e++)
      for (int k = 0; k <= PATCH; k++) {
        int j = edgeVertex(e, k),
            skirt = (PATCH + 1) * (PATCH + 1) + e * (PATCH + 1) + k;
        Vertex surface = n->stitched[j];
        V3 radial = norm(add(n->center, surface.p));
        n->stitched[skirt] = surface;
        n->stitched[skirt].p =
            add(surface.p, mul(radial, -fmaxf(1, n->size * .06f)));
      }
    n->edgeKey = key;
    morphUpload(n, n->stitched);
    Vertex *original = n->vertices;
    n->vertices = n->stitched;
    measureBounds(n);
    n->vertices = original;
    lodRevision++;
  }
  memcpy(terrainSeamState,state,selectedCount*sizeof(*state));
  terrainSeamCount=selectedCount;terrainSeamRevision=terrainGeometryRevision;
}
static void prepareTerrainGeometry(void) {
  deferMorphUploads = 1;
  prepareMorphs();
  deferMorphUploads = 0;
  stitchTerrainEdges();
  /* Includes unstitched patches and morphs that left the selected cover. */
  flushMorphUploads();
}
