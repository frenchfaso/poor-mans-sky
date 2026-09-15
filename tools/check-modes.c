// SPDX-License-Identifier: MPL-2.0
#define main poor_mans_sky_application_main
#include "../poor-mans-sky.c"
#undef main
int main(int argc,char **argv) {
  int next=argc>1?atoi(argv[1]):1;
  setvbuf(stdout,NULL,_IOLBF,0);
  if(SDL_Init(SDL_INIT_VIDEO))die(SDL_GetError());
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);
  int sizes[5][2]={{640,480},{720,480},{848,480},{800,600},{1024,768}};
  for(int j=0;j<5;j++)for(int i=0;i<SDL_GetNumDisplayModes(0);i++) {
    SDL_DisplayMode m;if(SDL_GetDisplayMode(0,i,&m))continue;
    if(m.w==sizes[j][0]&&m.h==sizes[j][1]){displayModes[displayModeCount++]=m;break;}
  }
  window=SDL_CreateWindow("Poor Man's Sky mode check",0,0,640,480,SDL_WINDOW_OPENGL);
  if(!window||SDL_SetWindowDisplayMode(window,&displayModes[0])||SDL_SetWindowFullscreen(window,SDL_WINDOW_FULLSCREEN))die(SDL_GetError());
  context=SDL_GL_CreateContext(window);if(!context)die(SDL_GetError());
  glClearColor(.1,.2,.3,1);glClear(GL_COLOR_BUFFER_BIT);SDL_GL_SwapWindow(window);SDL_Delay(500);
  changeDisplayMode(next);
  glBindFramebuffer(GL_FRAMEBUFFER,0);glClear(GL_COLOR_BUFFER_BIT);SDL_GL_SwapWindow(window);SDL_Delay(500);
  printf("MODE_CHECK drawable=%dx%d GL=%x\n",width,height,glGetError());
  SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();return 0;
}
