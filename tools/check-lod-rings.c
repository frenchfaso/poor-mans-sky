// SPDX-License-Identifier: MPL-2.0
/* Historical filename: now validates adaptive geometry + streaming rings. */
#define main poor_mans_sky_application_main
#include "../src/poor-mans-sky.c"
#undef main
#include <assert.h>
static void page(int id,float distance) {
  nodes[id]=(Node){0};nodes[id].level=5;nodes[id].slot=-1;
  nodes[id].center=v3(distance,0,0);nodes[id].state=1;
}
int main(void) {
  SDL_Init(0);cond=SDL_CreateCond();assert(cond);
  streamPriorityEye=v3(0,0,0);streamPriorityFrame=frameNo=0;
  float distances[]={5,20,40,80,200};
  qhead=510;qtail=3;qcount=5;
  for(int i=0;i<5;i++){page(i,distances[i]);queue[(510+i)%512]=4-i;}
  for(int i=0;i<5;i++)assert(popPriorityRequest()==i);
  assert(qhead==qtail && !qcount);
  qhead=qtail=0;qcount=512;
  for(int i=0;i<512;i++){page(i,500);queue[i]=i;}
  page(600,2);nodes[600].state=0;pendingCount=1;requestCount=0;
  pendingRequests[0]=(PendingRequest){600,requestPriority(600,0)};
  dispatchRequests();assert(qcount==512 && nodes[600].state==1);
  assert(popPriorityRequest()==600);
  int seen[512]={0};
  while(qcount){int id=popPriorityRequest();assert(id<512 && !seen[id]);seen[id]=1;}
  assert(qhead==qtail);
  page(0,500);page(1,2);streamPriorityFrame=1000;queueBorn[0]=0;queueBorn[1]=1000;
  assert(requestPriority(0,1)<requestPriority(1,1));
  streamPriorityFrame=frameNo=0;qhead=qtail=qcount=0;requestCount=0;pendingCount=32;
  for(int i=0;i<32;i++){
    page(i,i+1);nodes[i].state=0;
    pendingRequests[31-i]=(PendingRequest){i,requestPriority(i,0)};
  }
  dispatchRequests();assert(qcount==24);
  for(int i=0;i<24;i++)assert(popPriorityRequest()==i);
  eye=v3(0,0,0);meshTolerance=1;
  Node n={0};n.center=v3(5,0,0);n.meshLevel=3;n.pixelScale=100;
  assert(chooseMeshLOD(&n)==3); /* flat terrain may simplify even nearby */
  n.geomError[2]=100;n.geomError[3]=200;
  assert(chooseMeshLOD(&n)==1); /* rough terrain keeps detail, never level 0 */
  viewForward=v3(1,0,0);int lod=chooseMeshLOD(&n);
  viewForward=v3(-1,0,0);assert(chooseMeshLOD(&n)==lod);
  n.size=16;n.level=15;n.center=v3(30,0,0);
  int split=wantsSplit(&n);viewForward=v3(0,1,0);assert(wantsSplit(&n)==split);
  SDL_DestroyCond(cond);SDL_Quit();
  puts("PASS ring ordering, wrapped queue, full-queue replacement, aging, nearest-first admission, adaptive flat/rough LOD and view independence");
  return 0;
}
