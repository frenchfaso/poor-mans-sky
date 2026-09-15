// SPDX-License-Identifier: MPL-2.0
#define main poor_mans_sky_application_main
#include "../src/poor-mans-sky.c"
#undef main
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void){
 SDL_Init(SDL_INIT_TIMER);mutex=SDL_CreateMutex();cacheDirectory="/tmp/poor-moon-unit-cache";cacheInit();
 CHECK(fabsf(MOON_RADIUS/RADIUS-.2727f)<1e-6f);
 for(int f=0;f<6;f++)for(int j=0;j<10;j++) {
  V3 d=direction(f,(j-5)*.15f,.24f),p=add(moonCenter(),mul(d,MOON_RADIUS+moonHeight(d)+5));
  CHECK(nearMoon(p));CHECK(fabsf(bodyAltitude(bodyFloor(p,2.5f))-bodyHeight(bodyFloor(p,2.5f))-2.5f)<.2f);
  CHECK(dot(bodyUp(p),d)>.9999f);CHECK(isfinite(moonHeight(d)));
 }
 CHECK(!nearMoon(v3(0,RADIUS+10,0)));
 eye=add(moonCenter(),mul(norm(mul(moonCenter(),-1)),MOON_RADIUS+1500));
 heading=norm(cross(bodyUp(eye),v3(0,1,0)));flying=1;exitShip();CHECK(!flying);
 CHECK(fabsf(bodyAltitude(eye)-bodyHeight(eye)-2.5f)<.2f);interactShip();CHECK(flying && nearMoon(eye));
 MoonVertex a[MOON_VERTS],b[MOON_VERTS];V3 center,again;
 moonMesh(5,10,341,447,&center,a);uint32_t used;
 for(int i=0;i<MOON_VERTS;i++)CHECK(isfinite(a[i].p.x) && fabsf(dot(a[i].n,a[i].n)-1)<.001f);
 cacheWrite(6,5,10,341,447,&center,sizeof(center),a,sizeof(a));
 CHECK(cacheRead(6,5,10,341,447,&again,sizeof(again),b,sizeof(b),&used));CHECK(used==sizeof(b) && !memcmp(a,b,sizeof(a)));
 cond=SDL_CreateCond();
 SDL_LockMutex(mutex);
 MoonRAM *cached=moonRequest(5,10,341,447);CHECK(cached && cached->state==1);
 CHECK(moonQueueCount==1);CHECK(moonRequest(5,10,341,447)==cached && moonQueueCount==1);
 moonStreamWork();CHECK(cached->state==3 && moonRAMBytes==sizeof(a));
 uint64_t hits=diskHits[6];
 CHECK(moonRequest(5,10,341,447)==cached && diskHits[6]==hits && !moonQueueCount);
 CHECK(!memcmp(cached->vertices,a,sizeof(a)));
 frameNo=100;cached->pins=1;CHECK(!moonReclaimRAM());cached->pins=0;
 CHECK(moonReclaimRAM() && !moonRAMBytes && cached->state==0);
 CHECK(moonRequest(5,10,341,447)==cached);moonStreamWork();CHECK(cached->state==3);
 /* Hash table grows beyond the previous 1024-record ceiling. */
 for(int i=0;i<1100;i++) {MoonRAM *r=moonRequest(1,12,i,0);CHECK(r);moonQueueCount=0;r->state=0;}
 int entries=0;for(int i=0;i<4096;i++)for(MoonRAM *r=moonRAM[i];r;r=r->next)entries++;
 CHECK(entries==1101);
 moonClearRAM();SDL_UnlockMutex(mutex);
 /* Exercise the actual shared worker, its wakeup and shutdown path. */
 SDL_Thread *thread=SDL_CreateThread(streamWorker,"test-stream",NULL);CHECK(thread);
 SDL_LockMutex(mutex);MoonRAM *job=moonRequest(5,10,341,447);SDL_UnlockMutex(mutex);
 int ready=0;for(int i=0;i<5000;i++) {SDL_LockMutex(mutex);ready=job->state==3;SDL_UnlockMutex(mutex);if(ready)break;SDL_Delay(1);}
 CHECK(ready);
 SDL_LockMutex(mutex);quitWorker=1;SDL_CondSignal(cond);SDL_UnlockMutex(mutex);
 SDL_WaitThread(thread,NULL);moonClearRAM();SDL_DestroyCond(cond);
 SDL_DestroyMutex(mutex);cacheClose();puts("PASS moon ratio, six-face collision floor, local up, exit/board, finite normals, disk roundtrip, async worker wakeup, RAM reuse, pinned reclamation, >1024 entries");SDL_Quit();return 0;
}
