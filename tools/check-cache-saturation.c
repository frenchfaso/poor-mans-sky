// SPDX-License-Identifier: MPL-2.0
#define CACHE_TABLE 8
#define main poor_mans_sky_application_main
#include "../src/poor-mans-sky.c"
#undef main
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void) {
  cacheBuckets=malloc(CACHE_TABLE*sizeof(int));cacheEntries=calloc(CACHE_TABLE,sizeof(CacheEntry));
  CHECK(cacheBuckets && cacheEntries);for(int i=0;i<CACHE_TABLE;i++)cacheBuckets[i]=-1;
  uint32_t h[10]={1,0,0,0,0,10,20,30,40,0};
  for(int i=0;i<CACHE_TABLE;i++){h[3]=i;entryPut(0,h);}
  CHECK(entryCount==CACHE_TABLE);
  for(int i=0;i<10000;i++){h[3]=100+i;entryPut(1,h);}
  CHECK(cacheIndexCompactions==0 && entryCount==CACHE_TABLE);
  h[3]=3;h[5]=99;entryPut(2,h);CHECK(entryFind(h)->offset==99);
  entryRetire(&cacheEntries[0]);CHECK(cacheDeadEntries==1);
  entryRetire(&cacheEntries[0]);CHECK(cacheDeadEntries==1);
  h[3]=999;entryPut(1,h);CHECK(entryFind(h) && cacheIndexCompactions==1 && cacheDeadEntries==0);
  CHECK(entryCount==CACHE_TABLE);h[3]=1000;entryPut(1,h);CHECK(cacheIndexCompactions==1);
  h[3]=3;CHECK(entryFind(h)->offset==99);
  entryRetire(entryFind(h));CHECK(cacheDeadEntries==1);entryPut(2,h);
  CHECK(cacheDeadEntries==0 && entryFind(h)->pack==2);
  free(cacheBuckets);free(cacheEntries);puts("cache saturation: bounded insertion, existing-key updates and post-eviction compaction OK");return 0;
}
