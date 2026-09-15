// SPDX-License-Identifier: MPL-2.0
#define main poor_mans_sky_application_main
#include "../poor-mans-sky.c"
#undef main
int main(void) {
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
  if (wantsSplit(&detailProbe)) return 11;
  detailProbe.size = 16;
  if (!wantsSplit(&detailProbe)) return 12;
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
    fprintf(stderr, "FAIL coarsening left %d/4 patches visible\n", selectedCount);
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
    fprintf(stderr, "FAIL budget tiles=%d ring=%.3f\n", desiredTiles, ringScale);
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
