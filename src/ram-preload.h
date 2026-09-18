// SPDX-License-Identifier: MPL-2.0
/* CPU-only preload: best-first refinement by size/distance, independent of view
 * direction and of the 1024 VRAM slots. The ordinary worker owns generation. */
typedef struct { int id; float priority; } RamCandidate;
static RamCandidate ramHeap[MAXNODE];
static int ramHeapCount;
static float ramPriority(const Node *n) {
  V3 d = add(n->center, mul(eye, -1));
  return n->size / fmaxf(8, sqrtf(dot(d,d)));
}
static void ramPush(int id) {
  Node *n=&nodes[id];
  if(n->level<0 || n->level>=MAXLEVEL || n->child[0]>=0) return;
  RamCandidate item={id,ramPriority(n)};
  int i=ramHeapCount++;
  while(i && ramHeap[(i-1)/2].priority<item.priority) {
    ramHeap[i]=ramHeap[(i-1)/2]; i=(i-1)/2;
  }
  ramHeap[i]=item;
}
static int ramPop(void) {
  int id=ramHeap[0].id, i=0;
  RamCandidate tail=ramHeap[--ramHeapCount];
  while(i*2+1<ramHeapCount) {
    int c=i*2+1;
    if(c+1<ramHeapCount && ramHeap[c+1].priority>ramHeap[c].priority)c++;
    if(ramHeap[c].priority<=tail.priority)break;
    ramHeap[i]=ramHeap[c]; i=c;
  }
  if(ramHeapCount)ramHeap[i]=tail;
  return id;
}
static size_t ramAvailable(void) {
  FILE *f=fopen("/proc/meminfo","r");
  if(!f)return (size_t)-1;
  char line[256]; unsigned long kb;
  while(fgets(line,sizeof(line),f)) {
    if(sscanf(line,"MemAvailable: %lu kB",&kb)==1) {
      fclose(f); return (size_t)kb*1024u;
    }
  }
  fclose(f); return (size_t)-1;
}
static void ramLoadingScreen(size_t bytes, int planning) {
  float ratio=ramPreloadTarget?fminf(1,bytes/(float)ramPreloadTarget):1;
  char text[120];snprintf(text,sizeof(text),"%.0F / %.0F MIB / ENTER TO SKIP",bytes/1048576.,ramPreloadTarget/1048576.);
  bootProgress(.93f+.07f*ratio,planning?"PLANNING NEARBY WORLD":"LOADING OR GENERATING NEARBY WORLD",text);
}
static int ramPreloadWorld(void) {
  if(!ramPreloadTarget)return 1;
  const size_t pageBytes=TERRAIN_BYTES+NV*sizeof(Vertex);
  Uint32 start=SDL_GetTicks(),lastDraw=0,lastLog=0,lastMemory=0;
  int planning=1,running=1,skip=0,cursor=0;
  size_t bytes=0;
  SDL_LockMutex(mutex);
  ramPreloading=1;ramHeapCount=0;
  int valid=0;
  for(int i=0;i<countNode;i++)if(nodes[i].level>=0){valid++;ramPush(i);}
  SDL_UnlockMutex(mutex);
  printf("RAM_PRELOAD start target_mib=%.0f cap=OS\n",ramPreloadTarget/1048576.);fflush(stdout);
  for(;;) {
    SDL_Event e;
    while(SDL_PollEvent(&e)) {
      if(e.type==SDL_QUIT || (e.type==SDL_KEYDOWN && e.key.keysym.sym==SDLK_ESCAPE))running=0;
      if((e.type==SDL_KEYDOWN && e.key.keysym.sym==SDLK_RETURN) ||
         (e.type==SDL_CONTROLLERBUTTONDOWN && e.cbutton.button==SDL_CONTROLLER_BUTTON_START))skip=1;
    }
    if(!running||skip)break;
    Uint32 now=SDL_GetTicks();
    if(now-lastMemory>=1000) {
      lastMemory=now;
      if(ramAvailable()<192u*1048576u) {
        printf("RAM_PRELOAD stopped: less than 192 MiB available; preserving OS headroom\n");break;
      }
    }
    SDL_LockMutex(mutex);
    bytes=ramBytes;
    if(bytes>=ramPreloadTarget){SDL_UnlockMutex(mutex);break;}
    if(planning) {
      for(int work=0;work<16 && ramHeapCount && (size_t)valid*pageBytes<ramPreloadTarget;work++) {
        if(countNode+4>MAXNODE && freeCount<4)break;
        Node *n=&nodes[ramPop()];
        for(int j=0;j<4;j++) {
          int id=newNode(n->face,n->level+1,n->x*2+(j&1),n->y*2+(j>>1));
          n->child[j]=id;ramPush(id);valid++;
        }
      }
      if((size_t)valid*pageBytes>=ramPreloadTarget || !ramHeapCount || (countNode+4>MAXNODE && freeCount<4))planning=0;
    } else {
      int examined=0;
      while(qcount<128 && examined<countNode) {
        int id=cursor++;if(cursor==countNode)cursor=0;
        Node *n=&nodes[id];examined++;
        if(n->level<0 || n->pixels || n->state!=0)continue;
        queue[qtail]=id;qtail=(qtail+1)%512;qcount++;n->state=1;
      }
      SDL_CondSignal(cond);
      if(!qcount && examined==countNode) {
        int busy=0;for(int i=0;i<countNode;i++)busy|=nodes[i].state==1;
        if(!busy){SDL_UnlockMutex(mutex);break;}
      }
    }
    SDL_UnlockMutex(mutex);
    if(now-lastDraw>=100){ramLoadingScreen(bytes,planning);lastDraw=now;}
    if(now-lastLog>=10000){printf("RAM_PRELOAD progress seconds=%.1f ram_mib=%.1f target_mib=%.0f planning=%d\n",(now-start)/1000.,bytes/1048576.,ramPreloadTarget/1048576.,planning);fflush(stdout);lastLog=now;}
    SDL_Delay(planning?1:20);
  }
  SDL_LockMutex(mutex);
  /* Cancel queued CPU-only work when skipped; a single in-flight job may finish. */
  while(qcount){int id=queue[qhead];qhead=(qhead+1)%512;qcount--;nodes[id].state=0;}
  qtail=qhead;ramPreloading=0;bytes=ramBytes;
  SDL_UnlockMutex(mutex);
  printf("RAM_PRELOAD %s seconds=%.3f ram_mib=%.1f target_mib=%.0f resident=%d\n",bytes>=ramPreloadTarget?"complete":"partial",(SDL_GetTicks()-start)/1000.,bytes/1048576.,ramPreloadTarget/1048576.,resident);fflush(stdout);
  return running;
}
