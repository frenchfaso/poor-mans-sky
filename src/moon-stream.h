// SPDX-License-Identifier: MPL-2.0
/* Variable-sized RAM cache: no per-body byte/entry cap. Hash buckets do not
 * limit entry count. Shared worker publishes immutable payloads under mutex. */
typedef struct MoonRAM {
 int face,level,x,y,stamp,state,pins;
 V3 center;MoonVertex *vertices;struct MoonRAM *next;
 /* Immutable local metadata plus main-thread view cache; no payload changes. */
 V3 planCenter,planRadial;float planSize,planRadius,distance,distance2,priority;
 unsigned viewRevision,orderRevision;int band,visible,split,order[4];
} MoonRAM;
static MoonRAM *moonRAM[4096],*moonQueue[512];
static int moonQueueCount;
static unsigned moonHash(int face,int level,int x,int y) {
 return ((unsigned)face*73856093u ^ (unsigned)level*19349663u ^
         (unsigned)x*83492791u ^ (unsigned)y*2654435761u)&4095u;
}
/* All lookup/queue mutations hold the shared mutex. Planning records survive
 * payload eviction, avoiding procedural height evaluations in every pass. */
static MoonRAM *moonRecord(int face,int level,int x,int y,int create) {
 unsigned h=moonHash(face,level,x,y);MoonRAM *r=moonRAM[h];
 while(r && !(r->face==face && r->level==level && r->x==x && r->y==y))r=r->next;
 if(!r && create) {
  r=calloc(1,sizeof(*r));if(!r)return NULL;
  r->face=face;r->level=level;r->x=x;r->y=y;r->band=-1;
  float span=2.f/(1<<level);
  r->planRadial=direction(face,-1+(x+.5f)*span,-1+(y+.5f)*span);
  r->planCenter=mul(r->planRadial,MOON_RADIUS+moonHeight(r->planRadial));
  r->planSize=span*MOON_RADIUS;r->planRadius=r->planSize*1.6f+32;
  r->next=moonRAM[h];moonRAM[h]=r;
 }
 return r;
}
static float moonRequestPriority(const MoonRAM *r) {
 V3 delta=add(moonWorldPoint(r->planCenter),mul(streamPriorityEye,-1));
 float distance=fmaxf(0,sqrtf(dot(delta,delta))-r->planRadius);
 if(!streamViewReady)return streamDistancePriority(distance,r->level,0);
 if(r->level<=2)return -65536+r->level*8192+distance*.001f;
 int band=streamBandDelta(delta,dot(delta,delta),r->planRadius,-1);
 return band*8192+fminf(distance/streamBubble*256,4095)+r->level*.01f;
}
static void moonRefreshPriorities(void) {
 for(int i=0;i<moonQueueCount;i++)moonQueue[i]->priority=moonRequestPriority(moonQueue[i]);
}
static MoonRAM *moonRequest(int face,int level,int x,int y) {
 MoonRAM *r=moonRecord(face,level,x,y,1);if(!r)return NULL;
 r->stamp=frameNo;
 if(r->state==0) {
  r->priority=moonRequestPriority(r);
  int slot=moonQueueCount;
  if(slot==512) {
   int worst=0;for(int i=1;i<512;i++)if(moonQueue[i]->priority>moonQueue[worst]->priority)worst=i;
   if(r->priority>=moonQueue[worst]->priority)return r;
   moonQueue[worst]->state=0;slot=worst;
  } else moonQueueCount++;
  r->state=1;moonQueue[slot]=r;SDL_CondSignal(cond);
 }
 return r;
}
static int moonWorkPending(void){return moonQueueCount!=0;}
static void moonStreamWork(void) {
 int best=0;float score=moonQueue[0]->priority;
 for(int i=1;i<moonQueueCount;i++){float p=moonQueue[i]->priority;if(p<score){best=i;score=p;}}
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
