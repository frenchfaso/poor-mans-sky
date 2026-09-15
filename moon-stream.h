// SPDX-License-Identifier: MPL-2.0
/* Variable-sized RAM cache: no per-body byte/entry cap. Hash buckets do not
 * limit entry count. Shared worker publishes immutable payloads under mutex. */
typedef struct MoonRAM {
 int face,level,x,y,stamp,state,pins;
 V3 center;MoonVertex *vertices;struct MoonRAM *next;
} MoonRAM;
static MoonRAM *moonRAM[4096],*moonQueue[512];
static int moonQueueCount;
static unsigned moonHash(int face,int level,int x,int y) {
 return ((unsigned)face*73856093u ^ (unsigned)level*19349663u ^
         (unsigned)x*83492791u ^ (unsigned)y*2654435761u)&4095u;
}
static MoonRAM *moonRequest(int face,int level,int x,int y) {
 unsigned h=moonHash(face,level,x,y);MoonRAM *r=moonRAM[h];
 while(r && !(r->face==face && r->level==level && r->x==x && r->y==y))r=r->next;
 if(!r) {
  if(moonQueueCount==512)return NULL;
  r=calloc(1,sizeof(*r));if(!r)return NULL;
  r->face=face;r->level=level;r->x=x;r->y=y;r->next=moonRAM[h];moonRAM[h]=r;
 }
 r->stamp=frameNo;
 if(r->state==0 && moonQueueCount<512){r->state=1;moonQueue[moonQueueCount++]=r;SDL_CondSignal(cond);}
 return r;
}
static int moonWorkPending(void){return moonQueueCount!=0;}
static float moonRequestPriority(const MoonRAM *r) {
 float span=2.f/(1<<r->level);
 V3 center=moonWorldPoint(mul(direction(r->face,-1+(r->x+.5f)*span,-1+(r->y+.5f)*span),MOON_RADIUS));
 V3 delta=add(streamPriorityEye,mul(center,-1));
 float distance=fmaxf(0,sqrtf(dot(delta,delta))-span*MOON_RADIUS*1.6f);
 return streamDistancePriority(distance,r->level,0);
}
static void moonStreamWork(void) {
 int best=0;float score=moonRequestPriority(moonQueue[0]);
 for(int i=1;i<moonQueueCount;i++){float p=moonRequestPriority(moonQueue[i]);if(p<score){best=i;score=p;}}
 MoonRAM *r=moonQueue[best];moonQueue[best]=moonQueue[--moonQueueCount];
 if(r->level>0 && r->stamp<frameNo-90){r->state=0;return;}
 r->state=2;SDL_UnlockMutex(mutex);
 MoonVertex *v=streamAlloc(MOON_VERTS*sizeof(*v));V3 center;uint32_t bytes=0;int built=0;
 if(v && (!cacheRead(6,r->face,r->level,r->x,r->y,&center,sizeof(center),v,MOON_VERTS*sizeof(*v),&bytes) || bytes!=MOON_VERTS*sizeof(*v))) {
  moonMesh(r->face,r->level,r->x,r->y,&center,v);
  cacheWrite(6,r->face,r->level,r->x,r->y,&center,sizeof(center),v,MOON_VERTS*sizeof(*v));built=1;
 }
 SDL_LockMutex(mutex);
 if(!v){r->state=0;return;}
 r->vertices=v;r->center=center;r->state=3;
 moonRAMBytes+=MOON_VERTS*sizeof(*v);moonBuilds+=built;
}
static int moonReclaimRAM(void) {
 MoonRAM *victim=NULL;
 for(int i=0;i<4096;i++)for(MoonRAM *r=moonRAM[i];r;r=r->next)
  if(r->state==3 && !r->pins && r->stamp<frameNo-10 && (!victim || r->stamp<victim->stamp))victim=r;
 if(!victim)return 0;
 free(victim->vertices);victim->vertices=NULL;victim->state=0;
 moonRAMBytes-=MOON_VERTS*sizeof(MoonVertex);return 1;
}
static void moonClearRAM(void) {
 for(int i=0;i<4096;i++){MoonRAM *r=moonRAM[i];while(r){MoonRAM *next=r->next;free(r->vertices);free(r);r=next;}moonRAM[i]=NULL;}
 moonQueueCount=0;moonRAMBytes=0;
}
