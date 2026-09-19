// SPDX-License-Identifier: MPL-2.0
#define SDL_MAIN_HANDLED
#define main poor_mans_sky_application_main
#include "../src/poor-mans-sky.c"
#undef main
#include <assert.h>
static int readCloud(CloudVertex *vertices) {
 glBindBuffer(GL_ARRAY_BUFFER,cloudVbo);
 GLint bytes;glGetBufferParameteriv(GL_ARRAY_BUFFER,GL_BUFFER_SIZE,&bytes);
 assert(bytes>0 && bytes<=54*(int)sizeof(*vertices));
 glGetBufferSubData(GL_ARRAY_BUFFER,0,bytes,vertices);return bytes;
}
int main(void) {
 V3 axes[3];cloudAxes(v3(1300,2700,RADIUS),axes);
 for(int i=0;i<3;i++)for(int j=0;j<3;j++)assert(fabsf(dot(axes[i],axes[j])-(i==j))<1e-5f);
 for(int i=0;i<3;i++)for(int sign=-1;sign<=1;sign+=2) {
  float weights[3];cloudWeights(mul(axes[i],sign),axes,weights);
  for(int k=0;k<3;k++)assert(fabsf(weights[k]-(k==2-i))<1e-5f);
 }
 assert(!SDL_Init(SDL_INIT_VIDEO));
 SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);
 window=SDL_CreateWindow("cloud orientation check",0,0,128,128,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);assert(window);
 context=SDL_GL_CreateContext(window);assert(context);resourceInit();
 width=height=rw=rh=128;glViewport(0,0,128,128);clipNear=1;clipFar=80000;
 cameraEye=v3(0,0,RADIUS+100);sun=v3(0,0,1);viewForward=v3(0,1,0);
 cloudCatalogReady=1;cloudSeed=worldSeed;
 for(int i=0;i<2304;i++)cloudCatalog[i]=(CloudPuff){add(cameraEye,v3(0,-10000,0)),100,0,1};
 cloudCatalog[0]=(CloudPuff){add(cameraEye,v3(800,10000,3000)),1200,0,1};
 CloudVertex reference[54],actual[54];int referenceBytes=0;
 for(int i=0;i<=24;i++) {
  float a=i*2*PI/24;
  viewRight=v3(cosf(a),0,sinf(a));viewUp=cross(viewRight,viewForward);camera();
  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);cloudsDraw();assert(cloudDrawCount==1);
  int bytes=readCloud(actual);
  if(i==0){referenceBytes=bytes;memcpy(reference,actual,bytes);}
  else assert(bytes==referenceBytes && !memcmp(reference,actual,bytes));
 }
 /* Compound yaw/pitch loop: even intermediate views must not rotate cards.
  * This catches the accumulated twist missed by a single-axis pitch loop. */
 for(int i=0;i<=120;i++) {
  float a=i*2*PI/120;
  viewForward=norm(v3(.1f*sinf(a),1,.1f*(1-cosf(a))));
  viewRight=norm(cross(viewForward,v3(0,0,1)));viewUp=cross(viewRight,viewForward);camera();
  cloudsDraw();assert(cloudDrawCount==1);int bytes=readCloud(actual);
  assert(bytes==referenceBytes && !memcmp(reference,actual,bytes));
 }
 assert(glGetError()==GL_NO_ERROR);
 cloudsClose();SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();
 puts("PASS cloud cards: fixed geometry/UV/alpha through roll and compound camera loops; three orthogonal projection weights");
 return 0;
}
