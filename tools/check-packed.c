// SPDX-License-Identifier: MPL-2.0
#define main poor_mans_sky_application_main
#include "../poor-mans-sky.c"
#undef main
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %d %s\n",__LINE__,#x);return 1;}}while(0)
int main(int argc,char **argv) {
  CHECK(argc==2);SDL_Init(SDL_INIT_TIMER);cacheDirectory=argv[1];cacheInit();
  CHECK(sizeof(Vertex)==28&&offsetof(Vertex,u)==16&&sizeof(NaturePacked)==28&&sizeof(TerrainMeta)==64);
  unsigned char a[32],b[64],r[32],s[64];for(int i=0;i<32;i++)a[i]=i;for(int i=0;i<64;i++)b[i]=i*3;
  uint32_t nb=0;
  cacheWrite(1,0,2,0,0,a,32,b,64);CHECK(cacheRead(1,0,2,0,0,r,32,s,64,&nb)&&nb==64&&!memcmp(a,r,32)&&!memcmp(b,s,64));
  CHECK(!cacheRead(1,0,2,0,0,r,31,s,64,&nb));CHECK(!cacheRead(1,0,2,0,0,r,32,s,63,&nb));
  uint32_t key[5]={1,0,2,0,0};CacheEntry *e=entryFind(key);CHECK(e&&e->pack>=0);
  unsigned char corrupt=77;CHECK(writeAt(packFD(e->pack),&corrupt,1,e->offset));
  CHECK(!cacheRead(1,0,2,0,0,r,32,s,64,&nb));CHECK(diskBad==1);
  cacheWrite(1,0,2,0,0,a,32,b,64);CHECK(cacheRead(1,0,2,0,0,r,32,s,64,&nb));
  uint32_t rng=31;unsigned oldTotal=0,newTotal=0;
  for(int block=0;block<1000;block++) {
    unsigned char v[16][4],lo[3]={255,255,255},hi[3]={0},old[8],out[8];
    for(int j=0;j<16;j++)for(int c=0;c<4;c++){rng=rng*1664525+1013904223;v[j][c]=rng>>24;if(c<3){if(v[j][c]<lo[c])lo[c]=v[j][c];if(v[j][c]>hi[c])hi[c]=v[j][c];}}
    unsigned before=bcCandidate(v,rgb565(hi),rgb565(lo),old);bcColor(v,out);
    unsigned after=bcCandidate(v,out[0]|out[1]<<8,out[2]|out[3]<<8,old);CHECK(after<=before);oldTotal+=before;newTotal+=after;
  }
  printf("PACKED_TEST BC1 error ratio=%.4f vertices=28/28 corruption_recovered=1\n",newTotal/(double)oldTotal);
  diskLimit=diskBytes+100;
  cacheWrite(1,1,0,0,0,a,32,b,64);
  CHECK(diskEvicted>0 && diskBytes<=diskLimit);
  CHECK(cacheRead(1,1,0,0,0,r,32,s,64,&nb));
  puts("PACKED_TEST bounded pack eviction passed");
  for(int i=entryCount;i<CACHE_TABLE;i++)cacheEntries[i].pack=-1;
  entryCount=CACHE_TABLE;diskLimit=DISK_LIMIT;
  cacheWrite(1,2,0,0,0,a,32,b,64);
  CHECK(entryCount<100 && cacheRead(1,2,0,0,0,r,32,s,64,&nb));
  puts("PACKED_TEST evicted index entries reclaimed");
  cacheClose();return 0;
}
