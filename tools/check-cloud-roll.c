// SPDX-License-Identifier: MPL-2.0
#define SDL_MAIN_HANDLED
#define main poor_mans_sky_application_main
#include "../src/poor-mans-sky.c"
#undef main
#include <assert.h>
static void checkFrame(V3 r,V3 u) {
 assert(fabsf(dot(r,r)-1)<1e-4f && fabsf(dot(u,u)-1)<1e-4f);
 assert(fabsf(dot(r,u))<1e-4f && fabsf(dot(r,viewForward))<1e-4f && fabsf(dot(u,viewForward))<1e-4f);
}
int main(void) {
 cameraEye=v3(0,0,RADIUS+100);viewForward=v3(0,1,0);
 V3 r,u;cloudBillboardBasis(&r,&u);checkFrame(r,u);
 V3 initial=r;
 /* A continuous pitch loop crosses zenith and nadir without a frame flip. */
 for(int i=1;i<=1440;i++) {
  float a=i*2*PI/1440;viewForward=v3(0,cosf(a),sinf(a));
  V3 previous=r;cloudBillboardBasis(&r,&u);checkFrame(r,u);
  assert(dot(previous,r)>.999f);
 }
 assert(dot(r,initial)>.999f);
 viewForward=mul(viewForward,-1);cloudBillboardBasis(&r,&u);checkFrame(r,u);
 for(int sign=-1;sign<=1;sign+=2) {
  cloudBasisReady=0;viewForward=v3(0,0,sign);cloudBillboardBasis(&r,&u);checkFrame(r,u);
 }
 assert(!SDL_Init(SDL_INIT_VIDEO));
 SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);
 window=SDL_CreateWindow("cloud roll check",0,0,128,128,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);assert(window);
 context=SDL_GL_CreateContext(window);assert(context);resourceInit();
 width=height=rw=rh=128;glViewport(0,0,128,128);clipNear=1;clipFar=80000;
 sun=v3(0,0,1);viewForward=v3(0,1,0);cloudBasisReady=0;
 cloudCatalogReady=1;cloudSeed=worldSeed;
 for(int i=0;i<2304;i++)cloudCatalog[i]=(CloudPuff){add(cameraEye,mul(viewForward,-10000)),100,0,1};
 cloudCatalog[0]=(CloudPuff){add(cameraEye,mul(viewForward,10000)),1200,0,1};
 CloudVertex reference[18],actual[18];
 for(int i=0;i<=24;i++) {
  float a=i*2*PI/24;
  viewRight=v3(cosf(a),0,sinf(a));viewUp=cross(viewRight,viewForward);camera();
  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);cloudsDraw();
  assert(cloudDrawCount==1);
  glBindBuffer(GL_ARRAY_BUFFER,cloudVbo);
  GLint bytes;glGetBufferParameteriv(GL_ARRAY_BUFFER,GL_BUFFER_SIZE,&bytes);assert(bytes==sizeof(actual));
  glGetBufferSubData(GL_ARRAY_BUFFER,0,sizeof(actual),actual);
  if(i==0)memcpy(reference,actual,sizeof(actual));
  else assert(!memcmp(reference,actual,sizeof(actual)));
 }
 assert(glGetError()==GL_NO_ERROR);
 cloudsClose();SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();
 puts("PASS cloud geometry invariant under full camera roll; continuous zenith/nadir and reverse-view frames");
 return 0;
}
