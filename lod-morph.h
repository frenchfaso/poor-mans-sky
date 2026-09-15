// SPDX-License-Identifier: MPL-2.0
/* CPU geomorph: transient RAM only, no permanent extra VRAM attributes. */
#define MORPH_SLOTS 24
typedef struct {
  int id, lod, goal, reverse;
  Uint32 start;
  V3 half;
  Vertex from[NV], current[NV];
} LODMorph;
static LODMorph lodMorphs[MORPH_SLOTS];
static int morphInitialized, morphActive, morphStarted;

static int previousIndex[MAXNODE];
static int previousCover[1024], previousLevels[1024], previousCount;
static Vertex sampleMesh(const Vertex *v, float u, float w, int lod) {
  int step = 1 << lod;
  float x = clampf(u, 0, 1) * PATCH, y = clampf(w, 0, 1) * PATCH;
  int bx = (int)x / step * step, by = (int)y / step * step;
  if (bx >= PATCH)
    bx = PATCH - step;
  if (by >= PATCH)
    by = PATCH - step;
  float a = (x - bx) / step, b = (y - by) / step;
  int ia, ib, ic;
  float wa, wb, wc;
  if (a + b <= 1) {
    ia = by * (PATCH + 1) + bx;
    ib = ia + step;
    ic = ia + step * (PATCH + 1);
    wa = 1 - a - b;
    wb = a;
    wc = b;
  } else {
    ia = (by + step) * (PATCH + 1) + bx + step;
    ib = by * (PATCH + 1) + bx + step;
    ic = (by + step) * (PATCH + 1) + bx;
    wa = a + b - 1;
    wb = 1 - b;
    wc = 1 - a;
  }
  Vertex out = v[ia];
  out.p = add(mul(v[ia].p, wa), add(mul(v[ib].p, wb), mul(v[ic].p, wc)));
  out.n = packNormal(norm(add(mul(unpackNormal(v[ia].n), wa), add(mul(unpackNormal(v[ib].n), wb), mul(unpackNormal(v[ic].n), wc)))));
  out.h = v[ia].h * wa + v[ib].h * wb + v[ic].h * wc;
  return out;
}
static int morphFor(int id) {
  for (int i = 0; i < MORPH_SLOTS; i++)
    if (lodMorphs[i].id == id)
      return i;
  return -1;
}
static void morphUpload(Node *n, const Vertex *v) {
  glBindBuffer(GL_ARRAY_BUFFER, n->vbo);
  glBufferData(GL_ARRAY_BUFFER, NV * sizeof(Vertex), NULL, GL_STREAM_DRAW);
  glBufferSubData(GL_ARRAY_BUFFER, 0, NV * sizeof(Vertex), v);
}
static void prepareMorphs(void) {
  if (!morphInitialized) {
    for (int i = 0; i < MORPH_SLOTS; i++)
      lodMorphs[i].id = -1;
    for (int i = 0; i < MAXNODE; i++)
      previousIndex[i] = -1;
    morphInitialized = 1;
  }
  Uint32 now = SDL_GetTicks();
  morphActive = 0;
  for (int k = 0; k < MORPH_SLOTS; k++) {
    LODMorph *m = &lodMorphs[k];
    if (m->id < 0)
      continue;
    Node *n = &nodes[m->id];
    float t = clampf((now - m->start) / 450.0f, 0, 1);
    if (!n->vbo || !n->vertices || t >= 1 || n->used != frameNo) {
      if (n->vbo && n->vertices)
        morphUpload(n, n->vertices);
      n->edgeKey = 0;
      n->meshLevel = m->goal;
      for (int p = 0; p < previousCount; p++)
        if (previousCover[p] == m->id)
          previousLevels[p] = m->goal;
      n->boundHalf = m->half;
      n->boundRadius = sqrtf(dot(m->half, m->half));
      measureBounds(n);
      m->id = -1;
      lodRevision++;
      continue;
    }
    t = t * t * (3 - 2 * t);
    if (m->reverse)
      t = 1 - t;
    for (int j = 0; j < NV; j++) {
      m->current[j] = n->vertices[j];
      m->current[j].p = add(mul(m->from[j].p, 1 - t), mul(n->vertices[j].p, t));
      m->current[j].n = packNormal(norm(add(mul(unpackNormal(m->from[j].n), 1 - t), mul(unpackNormal(n->vertices[j].n), t))));
      m->current[j].h = m->from[j].h * (1 - t) + n->vertices[j].h * t;
      n->minHeight=fminf(n->minHeight,m->current[j].h);
      n->maxHeight=fmaxf(n->maxHeight,m->current[j].h);
    }
    n->meshLevel = m->lod;
    morphUpload(n, m->current);
    morphActive++;
  }
  for (int i = 0; i < selectedCount; i++) {
    int id = selected[i];
    Node *n = &nodes[id];
    if (!n->vertices || morphFor(id) >= 0 || !nodeVisible(n))
      continue;
    int same = previousIndex[id], candidates[1024], count = 0;
    float scale = 1.0f / (1 << n->level);
    float x0 = n->x * scale, y0 = n->y * scale;
    for (int p = 0; same < 0 && p < previousCount; p++) {
      int oldId = previousCover[p];
      Node *o = &nodes[oldId];
      if (oldId == id) {
        same = p;
        break;
      }
      if (o->face != n->face || !o->vertices)
        continue;
      float os = 1.0f / (1 << o->level), ox = o->x * os, oy = o->y * os;
      if (ox < x0 + scale && ox + os > x0 && oy < y0 + scale && oy + os > y0)
        candidates[count++] = p;
    }
    if (same >= 0 && previousLevels[same] == n->meshLevel)
      continue;
    if (same < 0 && !count)
      continue;
    int slot = -1;
    for (int k = 0; k < MORPH_SLOTS; k++)
      if (lodMorphs[k].id < 0) {
        slot = k;
        break;
      }
    if (slot < 0) {
      /* Keep an unchanged patch at its old index LOD until capacity frees. */
      if (same >= 0)
        n->meshLevel = previousLevels[same];
      continue;
    }
    LODMorph *m = &lodMorphs[slot];
    int wanted = n->meshLevel;
    m->id = id;
    m->start = now;
    m->half = n->boundHalf;
    m->lod = same >= 0 ? (wanted < previousLevels[same] ? wanted
                                                        : previousLevels[same])
                       : wanted;
    /* Refinement starts on the previous coarse surface. For decimation,
       reverse the endpoints and retain fine indices through the transition. */
    int coarsening = same >= 0 && wanted > previousLevels[same];
    m->goal = wanted;
    m->reverse = coarsening;
    V3 expand = v3(0, 0, 0);
    for (int j = 0; j < NV; j++) {
      Vertex original = n->vertices[j], source = original;
      if (same >= 0) {
        source = sampleMesh(n->vertices, original.u, original.v,
                            coarsening ? wanted : previousLevels[same]);
      } else {
        float x = x0 + original.u * scale, y = y0 + original.v * scale;
        for (int c = 0; c < count; c++) {
          int p = candidates[c];
          Node *o = &nodes[previousCover[p]];
          float os = 1.0f / (1 << o->level), u = x / os - o->x,
                v = y / os - o->y;
          if (u < -.0001f || u > 1.0001f || v < -.0001f || v > 1.0001f)
            continue;
          int active = morphFor(previousCover[p]);
          const Vertex *verts =
              active >= 0 ? lodMorphs[active].current : o->vertices;
          source = sampleMesh(verts, u, v, previousLevels[p]);
          source.p = add(source.p, add(o->center, mul(n->center, -1)));
          break;
        }
      }
      if (j >= (PATCH + 1) * (PATCH + 1))
        source.p = add(source.p, mul(norm(add(n->center, source.p)),
                                     -fmaxf(1, n->size * .06f)));
      source.u = original.u;
      source.v = original.v;
      m->from[j] = source;
      m->current[j] = coarsening ? original : source;
      V3 d = add(source.p, mul(original.p, -1));
      expand.x = fmaxf(expand.x, fabsf(d.x));
      expand.y = fmaxf(expand.y, fabsf(d.y));
      expand.z = fmaxf(expand.z, fabsf(d.z));
    }

    n->meshLevel =
        same >= 0
            ? (wanted < previousLevels[same] ? wanted : previousLevels[same])
            : wanted;
    n->boundHalf = add(m->half, expand);
    n->boundRadius = sqrtf(dot(n->boundHalf, n->boundHalf));
    morphUpload(n, m->current);
    morphActive++;
    morphStarted++;
    lodRevision++;
  }
  if (morphActive)
    lodRevision++;
  for (int i = 0; i < previousCount; i++)
    previousIndex[previousCover[i]] = -1;
  previousCount = selectedCount;
  for (int i = 0; i < selectedCount; i++) {
    previousCover[i] = selected[i];
    previousIndex[selected[i]] = i;
    previousLevels[i] = nodes[selected[i]].meshLevel;
  }
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}
