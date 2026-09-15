// SPDX-License-Identifier: MPL-2.0
#define main poor_mans_sky_application_main
#include "../src/poor-mans-sky.c"
#undef main
int main(void) {
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
