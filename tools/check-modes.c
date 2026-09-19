// SPDX-License-Identifier: MPL-2.0
/* Target-hardware smoke check: resize scene targets without changing scanout. */
#define main poor_mans_sky_application_main
#include "../src/poor-mans-sky.c"
#undef main
#include <assert.h>
int main(void) {
  setvbuf(stdout,NULL,_IOLBF,0);resourceInit();
  if(SDL_Init(SDL_INIT_VIDEO))die(SDL_GetError());
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);
  SDL_DisplayMode output;int found=0;
  for(int i=0;i<SDL_GetNumDisplayModes(0);i++) {
    SDL_DisplayMode m;if(SDL_GetDisplayMode(0,i,&m))continue;
    if(m.w==1024 && m.h==768){output=m;found=1;break;}
  }
  assert(found);
  window=SDL_CreateWindow("Poor Man's Sky fixed output check",0,0,1024,768,SDL_WINDOW_OPENGL);
  if(!window||SDL_SetWindowDisplayMode(window,&output)||SDL_SetWindowFullscreen(window,SDL_WINDOW_FULLSCREEN))die(SDL_GetError());
  context=SDL_GL_CreateContext(window);if(!context)die(SDL_GetError());
  renderSizeIndex=0;resolution();
  for(int i=0;i<RENDER_SIZE_COUNT*2;i++) {
    changeRenderSize(i<RENDER_SIZE_COUNT?1:-1);
    SDL_DisplayMode mode;assert(!SDL_GetCurrentDisplayMode(0,&mode));
    assert(mode.w==1024 && mode.h==768 && width==1024 && height==768);
    assert(SDL_GetWindowFlags(window)&SDL_WINDOW_FULLSCREEN);
    assert(SDL_GL_GetCurrentContext()==context && glGetError()==GL_NO_ERROR);
    glBindFramebuffer(GL_FRAMEBUFFER,0);glViewport(0,0,width,height);
    glClearColor(.1,.2,.3,1);glClear(GL_COLOR_BUFFER_BIT);SDL_GL_SwapWindow(window);
  }
  puts("PASS fixed 1024x768 fullscreen/context across all internal resolutions");
  SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();return 0;
}
