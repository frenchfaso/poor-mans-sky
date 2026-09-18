// SPDX-License-Identifier: MPL-2.0
#define SDL_MAIN_HANDLED
#define main poor_mans_sky_application_main
#include "../src/poor-mans-sky.c"
#undef main
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void) {
  resourceInit();CHECK(!SDL_Init(SDL_INIT_VIDEO));
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);
  window=SDL_CreateWindow("performance UI check",0,0,640,480,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);CHECK(window);
  context=SDL_GL_CreateContext(window);CHECK(context);width=640;height=480;
  GLuint p=program("bake.vert","voxel-shadow.frag");glUseProgram(p);glDeleteProgram(p);
  bootProgress(.6f,"COMPILING RENDERING SHADERS",NULL);
  CHECK(bootProgressValue==.6f && glGetError()==GL_NO_ERROR);
  bootProgress(.2f,"LOADING TERRAIN ROOTS",NULL);CHECK(bootProgressValue==.6f);
  perfInit();CHECK(perfResidentMiB()>0);CHECK(perfCPUClock()>=0);
  if(perfVRAMActual)CHECK(perfVRAMMiB>=0 && perfVRAMCapacity>0 && perfVRAMNext==.5);
  overlay();CHECK(glGetError()==GL_NO_ERROR); /* Empty startup history. */
  perfBegin();glClear(GL_COLOR_BUFFER_BIT);perfEnd();glFinish(); /* test-only sync */
  perfBegin();perfEnd();CHECK(!perfResult64 || perfGPUFrames>0);
  perfFrame(101);CHECK(perfCount==1 && perfHistory[0].cpu>=0 && perfHistory[0].ram>0);
  if(perfVRAMActual)CHECK(perfVRAMNext==.5); /* No driver poll on every HUD sample. */
  overlay();CHECK(glGetError()==GL_NO_ERROR); /* A single point has no segment. */
  int timerAvailable=perfResult64!=NULL;
  for(int i=0;i<PERF_HISTORY+3;i++)perfFrame(101);
  CHECK(perfCount==PERF_HISTORY);fps=60;overlay();CHECK(glGetError()==GL_NO_ERROR);
  /* Unsupported/pending GPU timing stays N/A, never substitutes CPU submit time. */
  PerfQueryResult64 saved=perfResult64;perfResult64=NULL;perfGPUFrames=0;perfFrame(101);
  CHECK(perfHistory[(perfHead+PERF_HISTORY-1)%PERF_HISTORY].gpu<0);overlay();
  perfResult64=saved;CHECK(glGetError()==GL_NO_ERROR);
#ifdef __linux__
  if(perfVRAMActual) {
    close(perfRadeonFD);perfRadeonFD=-1;perfFrame(501);
    CHECK(!perfVRAMActual && perfVRAMCapacity==56);
    int latest=(perfHead+PERF_HISTORY-1)%PERF_HISTORY;
    CHECK(perfHistory[latest].vram==vramEstimate()/1048576.f);
    CHECK(perfHistory[(latest+PERF_HISTORY-1)%PERF_HISTORY].vram<0);
  }
#endif
  perfClose();
  bootProgress(1,"READY",NULL);CHECK(bootProgressValue==1);
  SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();
  printf("performance UI: monotonic boot, deleted shader restoration, RSS, ring wrap and GPU availability=%d OK\n",timerAvailable);return 0;
}
