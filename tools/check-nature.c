// SPDX-License-Identifier: MPL-2.0
#define main poor_mans_sky_application_main
#include "../poor-mans-sky.c"
#undef main
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(int argc,char **argv){
 SDL_Init(SDL_INIT_TIMER);materialInit();initTreeModels();
 cacheDirectory=argc>1?argv[1]:"/tmp/poor-mans-sky-nature-test";cacheInit();geologyInit();
 V3 d=homeDirection();spawnPoint=mul(d,RADIUS+fmaxf(elevation(d),0));
 int face=4,x=(int)((d.x/d.z+1)*4096),y=(int)((d.y/d.z+1)*4096);
 int trees[4]={0},verts[4]={0};
 for(int q=0;q<3;q++){
  natureQuality=q;
  for(int dy=-2;dy<=2;dy++)for(int dx=-2;dx<=2;dx++) {
   int counts[4];
   for(int lod=0;lod<4;lod++) {
    NatureCell c={0};c.face=face;c.x=x+dx;c.y=y+dy;c.buildLod=lod;buildNature(&c);
    CHECK(c.count<NATURE_VERTS && c.count%3==0 && c.solid<=c.count);
    NaturePacked *v=(NaturePacked*)c.packed;
    for(int i=0;i<c.unique;i++)CHECK(v[i].u>=0 && v[i].u<=1 && v[i].v>=0 && v[i].v<=1);
    counts[lod]=c.trees;
    if(q==1){trees[lod]+=c.trees;verts[lod]+=c.count;}
    if(lod>0)CHECK(c.grass==0);
    uint32_t first=cacheHash(c.packed,c.unique*sizeof(NaturePacked)+c.count*sizeof(uint16_t),2166136261u);
    uint64_t writesBefore=diskWrites;
    NatureCell repeat=c;repeat.packed=NULL;buildNature(&repeat);
    CHECK(diskWrites==writesBefore);
    if(lod==3) CHECK(c.rocks==0);
    CHECK(c.count==repeat.count && first==cacheHash(repeat.packed,repeat.unique*sizeof(NaturePacked)+repeat.count*sizeof(uint16_t),2166136261u));
    free(c.packed);free(repeat.packed);
   }
   CHECK(counts[0]==counts[1] && counts[1]==counts[2] && counts[2]==counts[3]);
  }
 }
 CHECK(verts[0]>verts[1] && verts[1]>verts[2]);
 printf("PASS seed/LOD/cache/UV/capacity: balanced trees=%d,%d,%d vertices=%d,%d,%d\n",trees[0],trees[1],trees[2],verts[0],verts[1],verts[2]);
 cacheClose();SDL_Quit();return 0;
}
