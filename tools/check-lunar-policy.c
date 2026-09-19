// SPDX-License-Identifier: MPL-2.0
#define main poor_mans_sky_application_main
#include "../src/poor-mans-sky.c"
#undef main
#include <assert.h>
int main(void) {
 SDL_Init(0);mutex=SDL_CreateMutex();cond=SDL_CreateCond();
 celestial=celestialAt(clockTime,dayOffset,lunarPhaseOffset);
 eye=moonWorldPoint(v3(0,0,MOON_RADIUS+100));shipPos=eye;
 heading=norm(v3(.2f,.3f,.7f));shipHeading=heading;flightForward=heading;
 flightUp=norm(v3(.6f,.1f,.3f));velocity=v3(1,2,3);
 V3 fixed[]={eye,shipPos,heading,shipHeading,flightForward,flightUp,velocity};
 for(int i=0;i<1000;i++)advanceCelestial();
 V3 after[]={eye,shipPos,heading,shipHeading,flightForward,flightUp,velocity};
 assert(!memcmp(fixed,after,sizeof(fixed)));
 celestial=(CelestialFrame){{600000,0,0},{1,0,0},{0,1,0},{0,0,1},{1,0,0}};sun=v3(1,0,0);
 assert(solarVisibilityAt(v3(RADIUS+100000,200000,0))==1);
 assert(solarVisibilityAt(v3(RADIUS+100000,0,0))==0); /* Moon across the Sun. */
 assert(solarVisibilityAt(v3(-RADIUS-100000,0,0))==0);
 V3 night=add(moonCenter(),v3(-MOON_RADIUS-100,0,0));
 V3 day=add(moonCenter(),v3(MOON_RADIUS+100,0,0));
 assert(solarVisibilityAt(night)==0 && dot(directSunlightAt(night),directSunlightAt(night))==0);
 assert(solarVisibilityAt(day)==1 && directSunlightAt(day).x>1);
 celestial.center.x=-600000;day=add(moonCenter(),v3(MOON_RADIUS+100,0,0));
 assert(solarVisibilityAt(day)==0); /* Planet eclipse on Moon's day side. */
 float penumbra=bodySolarVisibility(v3(-500000,RADIUS,0),v3(0,0,0),RADIUS,sun);
 assert(penumbra>.49f && penumbra<.51f);
 celestial.center=v3(600000,0,0);
 for(int fly=0;fly<2;fly++)for(int preset=0;preset<3;preset++) {
  qualityPreset=preset;flying=fly;width=640;height=480;clipNear=1;clipFar=100000;
  cameraEye=moonWorldPoint(v3(0,0,MOON_RADIUS+moonHeight(v3(0,0,1))+2.5f));eye=cameraEye;
  viewForward=v3(1,0,0);viewRight=v3(0,1,0);viewUp=v3(0,0,1);
  streamViewSetup();moonPrepareView();
  MoonRAM *front=moonPlanRecord(4,12,2128,2048),*back=moonPlanRecord(4,12,1968,2048);
  assert(front && back && front->band<5 && back->band==5);
  assert(moonRequestPriority(front)<moonRequestPriority(back));
  int evaluations=moonPlanEvaluations;
  assert(moonPlanRecord(4,12,2128,2048)==front && moonPlanEvaluations==evaluations);
  moonPrepareView();moonPlanRecord(4,12,2128,2048);assert(!moonPlanEvaluations);
  unsigned revision=moonViewRevision;
  viewForward=v3(-1,0,0);viewRight=v3(0,-1,0);streamViewSetup();moonPrepareView();
  assert(moonViewRevision!=revision);
  moonPlanRecord(4,12,2128,2048);moonPlanRecord(4,12,1968,2048);
  assert(front->band==5 && back->band<5);
  MoonRAM *near=moonPlanRecord(4,13,4096,4096);assert(near && near->band==0);
 }
 /* A saturated queue must still accept coarse coverage and near-view work. */
 SDL_LockMutex(mutex);moonQueueCount=0;
 for(int i=0;i<512;i++){MoonRAM *r=moonRequest(4,13,i,0);assert(r);r->priority=1e6f;}
 assert(moonQueueCount==512);MoonRAM *root=moonRequest(4,0,0,0);
 assert(root && root->state==1 && moonQueueCount==512);
 int found=0;for(int i=0;i<512;i++)found|=moonQueue[i]==root;assert(found);
 moonClearRAM();SDL_UnlockMutex(mutex);SDL_DestroyCond(cond);SDL_DestroyMutex(mutex);SDL_Quit();
 puts("PASS planet/moon occultation, orbital night, eclipse, penumbra, lunar bubble/cone, all presets walk/fly, cached planning, turn invalidation and full-queue coverage priority");
 return 0;
}
