// SPDX-License-Identifier: MPL-2.0
#define main poor_mans_sky_application_main
#include "../poor-mans-sky.c"
#undef main
int main(int argc, char **argv) {
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
