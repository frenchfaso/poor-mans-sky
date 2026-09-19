// SPDX-License-Identifier: MPL-2.0
#define SDL_MAIN_HANDLED
#define main poor_mans_sky_application_main
#include "../src/poor-mans-sky.c"
#undef main
#include <assert.h>
static void pose(float yaw,int fly) {
  flying=fly;cameraEye=v3(0,0,0);width=640;height=480;
  viewForward=v3(sinf(yaw),0,cosf(yaw));viewRight=v3(cosf(yaw),0,-sinf(yaw));viewUp=v3(0,1,0);
  streamViewSetup();
}
int main(void) {
  pose(0,0);
  assert(streamBand(v3(0,0,-80),0,-1)==0);
  assert(streamBand(v3(0,0,200),0,-1)==1);
  assert(streamBand(v3(0,0,800),0,-1)==2);
  assert(streamBand(v3(0,0,3200),0,-1)==3);
  assert(streamBand(v3(0,0,12800),0,-1)==4);
  assert(streamBand(v3(0,0,-200),0,-1)==5);
  assert(streamBand(v3(0,0,-200),110,-1)==0); /* overlap, not center-only */
  float a=44*PI/180;
  assert(streamBand(v3(sinf(a)*1000,0,cosf(a)*1000),0,-1)==2); /* wider than rendered */
  a=51*PI/180;V3 edge=v3(sinf(a)*1000,0,cosf(a)*1000);
  assert(streamBand(edge,0,-1)==5 && streamBand(edge,0,2)==2);
  assert(streamBand(v3(0,0,430),0,1)==1 && streamBand(v3(0,0,430),0,-1)==2);
  assert(streamBand(v3(0,0,-110),0,0)==0 && streamBand(v3(0,0,-110),0,-1)==5);
  /* A turn must keep near detail and change distant refinement, not coverage. */
  Node near={0},front={0},back={0};
  near.center=v3(0,0,20);near.size=16;near.level=15;near.streamBand=-1;
  front.center=v3(0,0,1000);front.size=1000;front.level=8;front.streamBand=-1;
  back=front;back.center.z=-1000;
  /* Measured bounds isolate the far patches from the near bubble. */
  front.boundCenter=front.center;back.boundCenter=back.center;front.boundRadius=back.boundRadius=100;
  assert(wantsSplit(&near));assert(wantsSplit(&front));assert(!wantsSplit(&back));
  pose(PI,0);assert(wantsSplit(&near));assert(!wantsSplit(&front));assert(wantsSplit(&back));
  pose(0,1);assert(streamBand(v3(0,0,-250),0,-1)==0);assert(streamBand(v3(0,0,800),0,-1)==1);
  /* Roll must rotate the rectangular expanded frustum with the camera. */
  pose(0,0);V3 high=v3(0,1000,1000);assert(streamBand(high,0,-1)==5);
  viewRight=v3(0,1,0);viewUp=v3(-1,0,0);streamViewSetup();assert(streamBand(high,0,-1)==2);
  pose(0,0);nodes[0]=front;nodes[1]=back;
  nodes[0].child[0]=nodes[1].child[0]=-1;ramHeapCount=0;
  ramPush(1);assert(ramHeapCount==0);ramPush(0);assert(ramHeapCount==1);ramHeapCount=0;
  static NatureCandidate candidates[NATURE_CANDIDATES];
  int count=natureCandidates(norm(v3(1,1,1)),candidates);
  assert(count>40000 && count<NATURE_CANDIDATES && candidates[count-1].distance>2620);
  SDL_Init(0);cond=SDL_CreateCond();assert(cond);pose(0,0);
  nodes[0]=front;nodes[0].level=1;
  nodes[1]=front;nodes[1].streamBand=-1;
  nodes[2]=back;nodes[2].streamBand=-1;
  nodes[3]=near;nodes[3].streamBand=-1;
  qhead=0;qtail=qcount=4;for(int i=0;i<4;i++)queue[i]=2-i<0?3:2-i;
  assert(popPriorityRequest()==0);assert(popPriorityRequest()==3);assert(popPriorityRequest()==1);assert(popPriorityRequest()==2);
  /* Cached uploads use the same order as disk/generation requests. */
  int ids[]={2,1,3,0};qsort(ids,4,sizeof(int),readyPriority);assert(ids[0]==0 && ids[1]==3 && ids[2]==1 && ids[3]==2);
  /* Parent fallback stays selected until all children cover their quadrant. */
  countNode=5;frameNo=10;selectedCount=readyCount=pendingCount=0;
  eye=v3(0,0,RADIUS+10);pose(0,0);streamPriorityEye=eye;
  nodes[0]=(Node){0};nodes[0].center=add(eye,v3(0,0,20));nodes[0].size=100;nodes[0].level=0;nodes[0].slot=0;nodes[0].streamBand=-1;
  for(int i=0;i<4;i++) {nodes[0].child[i]=i+1;nodes[i+1]=(Node){0};nodes[i+1].center=nodes[0].center;nodes[i+1].size=50;nodes[i+1].level=1;nodes[i+1].slot=-1;nodes[i+1].streamBand=-1;for(int j=0;j<4;j++)nodes[i+1].child[j]=-1;}
  selectNode(0);assert(selectedCount==1 && selected[0]==0 && pendingCount==4);
  SDL_DestroyCond(cond);SDL_Quit();
  puts("PASS bubble, distance bands, expanded FOV, bounds, hysteresis, turn/roll, fly range, work/upload ordering and parent fallback");
  return 0;
}
