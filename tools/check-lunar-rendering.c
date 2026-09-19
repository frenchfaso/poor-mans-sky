// SPDX-License-Identifier: MPL-2.0
/* Run --moon --moon-phase 0.68 --still --frames 360 --ram-preload-mib 0.
 * Exercises turns/orbit/presets with workers live and deterministic daylight. */
#define _POSIX_C_SOURCE 200809L
#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
static void testSwap(SDL_Window *window);
#define SDL_GL_SwapWindow testSwap
#define main gameMain
#include "../src/poor-mans-sky.c"
#undef main
#undef SDL_GL_SwapWindow
#include <assert.h>
static int live,started,shadows,turns;
static V3 originalHeading,anchor;
static void testSwap(SDL_Window *w) {
 if(!preloading && (started || moonSelectedCount>0)) {
  if(!started){started=1;originalHeading=heading;}
  assert(nearMoon(eye) && moonSelectedCount>0 && glGetError()==GL_NO_ERROR);
  for(int i=0;i<moonSelectedCount;i++)for(int j=i+1;j<moonSelectedCount;j++) {
   MoonPatch *a=moonSelected[i],*b=moonSelected[j];if(a->face!=b->face)continue;
   if(a->level>b->level){MoonPatch *t=a;a=b;b=t;}
   int shift=b->level-a->level;
   assert((b->x>>shift)!=a->x || (b->y>>shift)!=a->y); /* Disjoint cover. */
  }
  if(live>30 && live<300) {
   assert(sunReady && sunBodyMoon);
   if(sunReceiverDraws>0)shadows++;
  }
  if(live==60){heading=mul(heading,-1);turns++;}
  if(live==100){heading=originalHeading;turns++;}
  if(live==120) {
   flying=1;eye=add(shipPos,mul(bodyUp(shipPos),5));flightForward=shipHeading;
   flightUp=bodyUp(eye);velocity=v3(0,0,0);throttle=0;
  }
  if(live==125)anchor=sunAnchor;
  if(live>125 && live<210) {
   V3 delta=add(sunAnchor,mul(anchor,-1));assert(dot(delta,delta)<.01f);
   lookX=(live-125)*2*PI/84;
  }
  if(live==210){lookX=0;setQualityPreset(0);}
  if(live==240)setQualityPreset(2);
  if(live==270)setQualityPreset(1);
  if(live%60==59)printf("LUNAR_TEST frame=%d selected=%d shadow_receivers=%d hidden=%d policy_evaluations=%d\n",live,moonSelectedCount,sunReceiverDraws,moonHidden,moonPlanEvaluations);
  live++;
 }
 SDL_GL_SwapWindow(w);
}
int main(int argc,char **argv) {
 int result=gameMain(argc,argv);
 assert(!result && live>=300 && shadows>100 && turns==2);
 puts("PASS lunar disjoint coverage, 180-degree turns, ship shadows, orbit-invariant shadow anchor and all presets");return result;
}
