// SPDX-License-Identifier: MPL-2.0
#define _DARWIN_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
static int failAfter=-1,failAll,injected;
static void *testMalloc(size_t n) {
 if(failAll){injected++;return NULL;}
 if(failAfter==0){failAfter=-1;injected++;return NULL;}
 if(failAfter>0)failAfter--;
 return malloc(n);
}
#define malloc testMalloc
#define main poor_mans_sky_application_main
#include "../poor-mans-sky.c"
#undef main
#undef malloc
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void) {
 SDL_Init(SDL_INIT_TIMER);mutex=SDL_CreateMutex();materialInit();initTreeModels();cacheEnabled=0;geologyInit();
 V3 d=homeDirection();spawnPoint=mul(d,RADIUS+fmaxf(elevation(d),0));
 NatureCell c={0};c.face=4;c.x=(int)((d.x/d.z+1)*4096);c.y=(int)((d.y/d.z+1)*4096);
 countNode=7;frameNo=100;Node *n=&nodes[6];n->pixels=malloc(TERRAIN_BYTES);n->vertices=malloc(NV*sizeof(Vertex));n->state=2;ramBytes=TERRAIN_BYTES+NV*sizeof(Vertex);
 failAfter=0;CHECK(buildNature(&c));CHECK(injected==1 && !n->pixels && !n->vertices && !ramBytes);free(c.packed);
 failAll=1;CHECK(!buildNature(&c));CHECK(!c.cpu && !c.packed);failAll=0;
 /* The actual worker must preserve the current LOD, back off, and recover. */
 natureMutex=SDL_CreateMutex();natureCond=SDL_CreateCond();natureUsed=1;
 nature[0]=c;nature[0].count=123;nature[0].state=1;nature[0].vbo=123;
 unsigned char *resident=malloc(16);nature[0].resident=resident;
 SDL_LockMutex(natureMutex);failAll=1;
 SDL_Thread *worker=SDL_CreateThread(natureWork,"oom-test",NULL);CHECK(worker);SDL_UnlockMutex(natureMutex);
 int failed=0;
 for(int i=0;i<600;i++){SDL_LockMutex(natureMutex);failed=natureAllocationFailures>0 && nature[0].state==1;SDL_UnlockMutex(natureMutex);if(failed)break;SDL_Delay(5);}
 CHECK(failed);SDL_LockMutex(natureMutex);
 CHECK(nature[0].count==123 && nature[0].vbo==123 && nature[0].resident==resident && !nature[0].packed);
 CHECK(natureMemoryPressure && (Sint32)(natureRetryAt-SDL_GetTicks())>0);
 failAll=0;natureRetryAt=0;SDL_CondSignal(natureCond);SDL_UnlockMutex(natureMutex);
 int ready=0;
 for(int i=0;i<600;i++){SDL_LockMutex(natureMutex);ready=nature[0].state==3;SDL_UnlockMutex(natureMutex);if(ready)break;SDL_Delay(5);}
 CHECK(ready);CHECK(nature[0].packed && nature[0].resident==resident);
 SDL_LockMutex(natureMutex);natureQuit=1;SDL_CondSignal(natureCond);SDL_UnlockMutex(natureMutex);SDL_WaitThread(worker,NULL);
 free(nature[0].packed);free(resident);SDL_DestroyCond(natureCond);SDL_DestroyMutex(natureMutex);SDL_DestroyMutex(mutex);SDL_Quit();
 puts("PASS allocation recovery, exhausted cache without abort, worker backoff, preserved current mesh, successful retry and shutdown");return 0;
}
