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
/* Reference before squared-distance optimization. */
static int referenceBand(V3 center,float radius,int previous) {
  V3 d=add(center,mul(streamPriorityEye,-1));
  float distance=fmaxf(0,sqrtf(dot(d,d))-radius);
  if(distance<=streamBubble*(previous==0?1.2f:1))return 0;
  int retained=previous>=0 && previous<5;
  if(distance>streamDetailRange*(retained?1.15f:1))return 5;
  for(int p=0;p<4;p++)if(dot(d,streamPlanes[retained][p]) < -radius)return 5;
  float boundary=streamBubble*4;
  for(int band=1;band<4;band++,boundary*=4)
    if(distance<=boundary*(previous==band?1.15f:previous==band+1?.9f:1))return band;
  return 4;
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
  assert(count>10000 && count<NATURE_CANDIDATES && candidates[count-1].distance>quality()->natureDistance+120);
  for(int i=0;i<20000;i++) {
    uint32_t h=hash3(i,17,91),j=hash3(i,73,21);
    V3 p=v3((int)(h&65535)-32768,(int)(h>>16)-32768,(int)(j&65535)-32768);
    float radius=(j>>16)*.1f;int previous=i%7-1;
    assert(streamBand(p,radius,previous)==referenceBand(p,radius,previous));
  }
  SDL_Init(0);cond=SDL_CreateCond();assert(cond);pose(0,0);
  nodes[0]=front;nodes[0].level=1;
  nodes[1]=front;nodes[1].streamBand=-1;
  nodes[2]=back;nodes[2].streamBand=-1;
  nodes[3]=near;nodes[3].streamBand=-1;
  qhead=0;qtail=qcount=4;for(int i=0;i<4;i++)queue[i]=2-i<0?3:2-i;
  refreshQueuePriorities();
  assert(popPriorityRequest()==0);assert(popPriorityRequest()==3);assert(popPriorityRequest()==1);assert(popPriorityRequest()==2);
  /* Cached uploads use the same order as disk/generation requests. */
  int ids[]={2,1,3,0};memcpy(readyIds,ids,sizeof(ids));readyCount=4;sortReadyRequests();assert(readyIds[0]==0 && readyIds[1]==3 && readyIds[2]==1 && readyIds[3]==2);
  qhead=509;qtail=509;qcount=512;
  for(int i=0;i<512;i++){nodes[i]=back;nodes[i].state=1;nodes[i].center.z=-5000;queue[(509+i)%512]=i;}
  nodes[600]=near;nodes[600].state=0;pendingCount=1;requestCount=0;
  pendingRequests[0]=(PendingRequest){600,requestPriority(600,0)};
  dispatchRequests();assert(qcount==512 && popPriorityRequest()==600);
  int seen[512]={0};while(qcount){int id=popPriorityRequest();assert(id<512 && !seen[id]);seen[id]=1;}
  assert(qhead==qtail);
  /* Parent fallback stays selected until all children cover their quadrant. */
  countNode=5;frameNo=10;selectedCount=readyCount=pendingCount=0;
  eye=v3(0,0,RADIUS+10);pose(0,0);streamPriorityEye=eye;
  nodes[0]=(Node){0};nodes[0].center=add(eye,v3(0,0,20));nodes[0].size=100;nodes[0].level=0;nodes[0].slot=0;nodes[0].streamBand=-1;
  for(int i=0;i<4;i++) {nodes[0].child[i]=i+1;nodes[i+1]=(Node){0};nodes[i+1].center=nodes[0].center;nodes[i+1].size=50;nodes[i+1].level=1;nodes[i+1].slot=-1;nodes[i+1].streamBand=-1;for(int j=0;j<4;j++)nodes[i+1].child[j]=-1;}
  selectNode(0);assert(selectedCount==1 && selected[0]==0 && pendingCount==4);
  for(int p=0;p<3;p++)for(int fly=0;fly<2;fly++) {
    qualityPreset=p;pose(0,fly);
    assert(streamBand(v3(0,0,-streamBubble*.9f),0,-1)==0);
    assert(streamBand(v3(0,0,streamDetailRange*.99f),0,-1)<5);
    assert(streamBand(v3(0,0,streamDetailRange*1.1f),0,-1)==5);
    assert(streamBand(v3(0,0,streamDetailRange*1.1f),0,4)<5);
    assert(streamBand(v3(0,0,streamDetailRange*1.2f),0,4)==5);
    for(int i=0;i<20000;i++) {
      uint32_t h=hash3(i,17,91),j=hash3(i,73,21);
      V3 c=v3((int)(h&65535)-32768,(int)(h>>16)-32768,(int)(j&65535)-32768);
      assert(streamBand(c,(j>>16)*.1f,i%7-1)==referenceBand(c,(j>>16)*.1f,i%7-1));
    }
  }
  SDL_DestroyCond(cond);SDL_Quit();
  puts("PASS bubble, distance bands, expanded FOV, bounds, hysteresis, turn/roll, fly range, work/upload ordering and parent fallback");
  return 0;
}
