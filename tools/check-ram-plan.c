// SPDX-License-Identifier: MPL-2.0
#define main poor_mans_sky_application_main
#include "../src/poor-mans-sky.c"
#undef main
#include <assert.h>
int main(void) {
  /* Residency is OS managed; the preload target is not a cache cap. */
  assert(ramPreloadTarget==512u*1048576u);
  eye=v3(RADIUS+40,0,0);
  /* Priority is distance-only, including the back of the camera. */
  Node a={0},b={0};a.size=b.size=100;
  a.center=v3(RADIUS,0,200);b.center=v3(RADIUS,0,-200);
  assert(fabsf(ramPriority(&a)-ramPriority(&b))<1e-6f);
  for(int i=0;i<1000;i++) {
    nodes[i].level=1;nodes[i].child[0]=-1;
    nodes[i].size=1+(i*37)%1000;nodes[i].center=v3(RADIUS,100,0);
    ramPush(i);
  }
  float previous=1e9;
  while(ramHeapCount){int id=ramPop();float p=ramPriority(&nodes[id]);assert(p<=previous);previous=p;}
  size_t pages=(RAM_PRELOAD+PAGE_BYTES+NV*sizeof(Vertex)-1)/(PAGE_BYTES+NV*sizeof(Vertex));
  assert(pages<MAXNODE*3/4);
  printf("PASS default 512 MiB, OS-managed residency, distance symmetry, heap order, %lu pages fit before recycling threshold\n",(unsigned long)pages);
  assert(SDL_Init(SDL_INIT_EVENTS)==0);
  mutex=SDL_CreateMutex(); assert(mutex);
  countNode=2; qcount=2;qhead=0;qtail=2;queue[0]=0;queue[1]=1;
  nodes[0].state=nodes[1].state=1;
  SDL_Event event={0};event.type=SDL_KEYDOWN;event.key.keysym.sym=SDLK_RETURN;
  assert(SDL_PushEvent(&event)==1);
  assert(ramPreloadWorld()==1);
  assert(qcount==0 && qtail==qhead && !ramPreloading);
  assert(nodes[0].state==0 && nodes[1].state==0);
  SDL_DestroyMutex(mutex);SDL_Quit();
  puts("PASS skip cancels pending CPU requests and restores interactive mode");
  return 0;
}
