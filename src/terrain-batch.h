// SPDX-License-Identifier: MPL-2.0
/* Small spatial batches for stable, decimated patches. Original VBOs remain
 * available to water, reflections and morphing. Additional VRAM is capped. */
#define TERRAIN_BATCHES 48
typedef struct {
  uint32_t key;
  int used, count, atlas;
  V3 center;
  GLuint vbo, ebo;
  size_t bytes;
} TerrainBatch;
static TerrainBatch terrainBatches[TERRAIN_BATCHES];
static int batchedNodes[1024], batchDraws, batchPatches;
static size_t batchBytes;
static int batchCompatible(Node *a, Node *b) {
  return a->face == b->face && a->level == b->level &&
         (a->x >> 2) == (b->x >> 2) && (a->y >> 2) == (b->y >> 2) &&
         a->slot / 256 == b->slot / 256;
}
static int batchEligible(int i) {
  Node *n = &nodes[selected[i]];
  return n->visible && n->maxHeight >= 0 && n->meshLevel >= 2 && n->vertices &&
         morphFor(selected[i]) < 0 && textureFadeFor(selected[i]) < 0;
}
static void drawTerrainBatches(void) {
  int done[1024] = {0}, uploads = 0;
  memset(batchedNodes, 0, sizeof(batchedNodes));
  batchDraws = batchPatches = 0;
  Uint64 start = SDL_GetPerformanceCounter();
  for (int i = 0; i < selectedCount; i++) {
    if (done[i] || !batchEligible(i))
      continue;
    Node *first = &nodes[selected[i]];
    int group[16], members = 0;
    /* Stable ID order keeps a camera-distance sort from rebuilding buffers. */
    for (int j = i; j < selectedCount && members < 16; j++)
      if (!done[j] && batchEligible(j) &&
          batchCompatible(first, &nodes[selected[j]]))
        group[members++] = j;
    if (members < 2)
      continue;
    for (int j = 1; j < members; j++) {
      int a = group[j], k = j;
      while (k && selected[group[k - 1]] > selected[a]) {
        group[k] = group[k - 1];
        k--;
      }
      group[k] = a;
    }
    uint32_t key = 2166136261u;
    for (int j = 0; j < members; j++) {
      Node *n = &nodes[selected[group[j]]];
      uint32_t data[] = {selected[group[j]],
                         n->slot,
                         n->meshLevel,
                         n->vbo,
                         n->edgeKey,
                         n->x,
                         n->y,
                         n->level,
                         n->face};
      key = cacheHash(data, sizeof(data), key);
      done[group[j]] = 1;
    }
    int slot = -1;
    for (int j = 0; j < TERRAIN_BATCHES; j++)
      if (terrainBatches[j].vbo && terrainBatches[j].key == key) {
        slot = j;
        break;
      }
    if (slot < 0) {
      if (uploads || (SDL_GetPerformanceCounter() - start) * 1000.0 /
                             SDL_GetPerformanceFrequency() >
                         2)
        continue;
      for (int j = 0; j < TERRAIN_BATCHES; j++)
        if (slot < 0 || terrainBatches[j].used < terrainBatches[slot].used)
          slot = j;
      TerrainBatch *batch = &terrainBatches[slot];
      if (batch->used == frameNo && batch->vbo)
        continue;
      Vertex vertices[16 * 100];
      unsigned short indices[16 * 400];
      int nv = 0, ni = 0;
      V3 center = first->center;
      for (int j = 0; j < members; j++) {
        Node *n = &nodes[selected[group[j]]];
        int map[NV], step = 1 << n->meshLevel;
        for (int k = 0; k < NV; k++)
          map[k] = -1;
        unsigned short source[400];
        int ns = 0;
        for (int y = 0; y < PATCH; y += step)
          for (int x = 0; x < PATCH; x += step) {
            int a = y * (PATCH + 1) + x, b = a + step * (PATCH + 1);
            source[ns++] = a;
            source[ns++] = a + step;
            source[ns++] = b;
            source[ns++] = a + step;
            source[ns++] = b + step;
            source[ns++] = b;
          }
        int base = (PATCH + 1) * (PATCH + 1);
        for (int e = 0; e < 4; e++)
          for (int k = 0; k < PATCH; k += step) {
            int a = e == 0   ? k
                    : e == 1 ? k * (PATCH + 1) + PATCH
                    : e == 2 ? PATCH * (PATCH + 1) + PATCH - k
                             : (PATCH - k) * (PATCH + 1);
            int b = e == 0   ? a + step
                    : e == 1 ? a + step * (PATCH + 1)
                    : e == 2 ? a - step
                             : a - step * (PATCH + 1),
                sa = base + e * (PATCH + 1) + k;
            source[ns++] = a;
            source[ns++] = sa;
            source[ns++] = b;
            source[ns++] = sa;
            source[ns++] = sa + step;
            source[ns++] = b;
          }
        for (int k = 0; k < ns; k++) {
          int old = source[k];
          if (map[old] < 0) {
            map[old] = nv;
            vertices[nv] = (n->stitched ? n->stitched : n->vertices)[old];
            vertices[nv].p =
                add(vertices[nv].p, add(n->center, mul(center, -1)));
            vertices[nv].u += (n->slot % 16) * 128.f / 120;
            vertices[nv].v += ((n->slot % 256) / 16) * 128.f / 120;
            nv++;
          }
          indices[ni++] = map[old];
        }
      }
      size_t bytes = nv * sizeof(Vertex) + ni * sizeof(unsigned short);
      if (batchBytes - batch->bytes + bytes > 3 * 1024 * 1024)
        continue;
      if (!batch->vbo)
        glGenBuffers(1, &batch->vbo);
      if (!batch->ebo)
        glGenBuffers(1, &batch->ebo);
      glBindBuffer(GL_ARRAY_BUFFER, batch->vbo);
      glBufferData(GL_ARRAY_BUFFER, nv * sizeof(Vertex), vertices,
                   GL_STATIC_DRAW);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, batch->ebo);
      glBufferData(GL_ELEMENT_ARRAY_BUFFER, ni * sizeof(unsigned short),
                   indices, GL_STATIC_DRAW);
      batchBytes = batchBytes - batch->bytes + bytes;
      batch->bytes = bytes;
      batch->key = key;
      batch->center = center;
      batch->count = ni;
      batch->atlas = first->slot / 256;
      uploads++;
    }
    TerrainBatch *batch = &terrainBatches[slot];
    batch->used = frameNo;
    int mode = 1;
    for (int j = 0; j < members; j++)
      if (!terrainFastMode(&nodes[selected[group[j]]])) mode = 0;
    glUseProgram(landPrograms[mode][0]);
    glUniform2f(landPageLocations[mode][0], 0, 0);
    glBindTexture(GL_TEXTURE_2D, atlasTex[batch->atlas]);
    float period = 100.f / 17;
    glUniform3f(landOriginLocations[mode][0],
                fmodf(batch->center.x, period), fmodf(batch->center.y, period),
                fmodf(batch->center.z, period));
    V3 localEye = add(cameraEye, mul(batch->center, -1));
    glUniform3f(landEyeLocations[mode][0], localEye.x, localEye.y,
                localEye.z);
    glPushMatrix();
    V3 offset = add(batch->center, mul(cameraEye, -1));
    glTranslatef(offset.x, offset.y, offset.z);
    glBindBuffer(GL_ARRAY_BUFFER, batch->vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, batch->ebo);
    glVertexPointer(3, GL_FLOAT, sizeof(Vertex), (void *)offsetof(Vertex, p));
    glNormalPointer(GL_BYTE, sizeof(Vertex), (void *)offsetof(Vertex, n));
    glTexCoordPointer(3, GL_FLOAT, sizeof(Vertex), (void *)offsetof(Vertex, u));
    glDrawElements(GL_TRIANGLES, batch->count, GL_UNSIGNED_SHORT, 0);
    glPopMatrix();
    triangles += batch->count / 3;
    batchDraws++;
    batchPatches += members;
    drawn += members;
    for (int j = 0; j < members; j++)
      batchedNodes[group[j]] = 1;
  }
}
