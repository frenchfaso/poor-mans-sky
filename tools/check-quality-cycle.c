// SPDX-License-Identifier: MPL-2.0
/* Full-scene integration: real F4 events with workers, targets and caches live.
 * Run from bin with --windowed --frames 210 --preload --ram-preload-mib 0.
 * Do not use --still: it deliberately ignores keyboard events. */
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
static int live,expected=1,changes;
static void testSwap(SDL_Window *w) {
  if(!preloading && drawn>0) {
    assert(qualityPreset==expected);
    assert(rw==(int)(width*quality()->renderScale));
    assert(rh==(int)(height*quality()->renderScale));
    assert(glow[0].w==quality()->bloomSize);
    assert(postP==(expected==0?postLowP:expected==1?postPerformanceP:postQualityP));
    if(expected==0)assert(!reflectionReady);
    if(reflectionReady) {
      assert(reflectionMap.w==quality()->reflectionSize && reflectionMap.h==quality()->reflectionSize);
      assert(reflectionBlur.w==reflectionMap.w && reflectionBlur.h==reflectionMap.h);
      assert(reflectionLastFrame==frameNo); /* Both enabled presets update every frame. */
      float direction[2];glGetUniformfv(reflectionBlurP,uniformLocation(reflectionBlurP,"direction"),direction);
      assert(direction[0]==0 && fabsf(direction[1]*reflectionMap.h-1)<1e-6f);
    }
    if(sunReady)assert(sunMap.w==quality()->shadowSize);
    assert(selectedCount>0 && glGetError()==GL_NO_ERROR);
    if(live%30==29) {
      SDL_LockMutex(mutex);
      for(int i=0;i<6;i++)assert(hasCoverage(i));
      SDL_UnlockMutex(mutex);
      printf("QUALITY_CYCLE frame=%d preset=%s selected=%d resident=%d fly=%d\n",live,quality()->name,selectedCount,resident,flying);
      if(live<180) {
        SDL_Event event={0};event.type=SDL_KEYDOWN;event.key.keysym.sym=SDLK_F4;
        assert(SDL_PushEvent(&event)==1);expected=(expected+1)%3;changes++;
      }
    }
    if(live==90) {
      flying=1;eye=add(eye,mul(bodyUp(eye),100));
      flightForward=heading;flightUp=bodyUp(eye);velocity=v3(0,0,0);throttle=0;
    }
    live++;
  }
  SDL_GL_SwapWindow(w);
}
int main(int argc,char **argv) {
  int result=gameMain(argc,argv);
  assert(!result && live>=210 && changes==6);
  puts("PASS two complete F4 cycles, walk/fly, live workers, coverage and GPU resources");
  return result;
}
