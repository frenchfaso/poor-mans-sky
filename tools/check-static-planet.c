// SPDX-License-Identifier: MPL-2.0
#define main poor_mans_sky_application_main
#include "../src/poor-mans-sky.c"
#undef main
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);return 1;}}while(0)
static int edgeCompare(const void *a,const void *b) {uint32_t x=*(const uint32_t*)a,y=*(const uint32_t*)b;return (x>y)-(x<y);}
int main(void) {
  resourceInit();materialInit();
  StaticPlanetVertex *v=malloc(STATIC_PLANET_VERTS*sizeof(*v));
  unsigned short *ix=malloc(STATIC_PLANET_INDICES*sizeof(*ix));
  uint32_t *edges=malloc(STATIC_PLANET_INDICES*sizeof(*edges));
  CHECK(v && ix && edges);staticPlanetBuild(v,ix);
  int used[STATIC_PLANET_VERTS]={0};
  for(int i=0;i<STATIC_PLANET_INDICES;i+=3) {
    for(int j=0;j<3;j++) {
      unsigned a=ix[i+j],b=ix[i+(j+1)%3];CHECK(a<STATIC_PLANET_VERTS && b<STATIC_PLANET_VERTS && a!=b);used[a]=1;
      edges[i+j]=a<b?(a<<16)|b:(b<<16)|a;
    }
    V3 a=v[ix[i]].p,b=v[ix[i+1]].p,c=v[ix[i+2]].p;
    CHECK(dot(cross(add(b,mul(a,-1)),add(c,mul(a,-1))),a)>0);
  }
  for(int i=0;i<STATIC_PLANET_VERTS;i++)CHECK(used[i]);
  qsort(edges,STATIC_PLANET_INDICES,sizeof(*edges),edgeCompare);
  for(int i=0;i<STATIC_PLANET_INDICES;i+=2) {
    CHECK(edges[i]==edges[i+1]);if(i+2<STATIC_PLANET_INDICES)CHECK(edges[i]!=edges[i+2]);
  }
  CHECK(STATIC_PLANET_VERTS-STATIC_PLANET_INDICES/2+STATIC_PLANET_INDICES/3==2);
  free(v);free(ix);free(edges);
  puts("static planet: closed welded sphere, every edge shared twice, outward winding, Euler characteristic OK");return 0;
}
