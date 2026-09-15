// SPDX-License-Identifier: MPL-2.0
#define main poor_mans_sky_application_main
#include "../src/poor-mans-sky.c"
#undef main
#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x)) {                                                                \
      fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x);                     \
      return 1;                                                                \
    }                                                                          \
  } while (0)
int main(void) {
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
