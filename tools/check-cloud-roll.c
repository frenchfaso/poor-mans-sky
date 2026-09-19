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
 clipNear=1;clipFar=100;
 assert(fabsf(cloudCardArea(v3(0,0,10),v3(5,0,0),v3(0,5,0))-.21875f)<1e-6f);
 assert(cloudCardArea(v3(30,0,10),v3(5,0,0),v3(0,5,0))==0);
 assert(cloudCardArea(v3(0,0,-10),v3(5,0,0),v3(0,5,0))==0);
 assert(cloudCardArea(v3(0,0,110),v3(5,0,0),v3(0,5,0))==0);
 float nearArea=cloudCardArea(v3(0,0,1),v3(2,0,2),v3(0,2,0));
 assert(isfinite(nearArea) && nearArea>0 && nearArea<=1.00001f);
 float previous=1;
 for(int i=0;i<=200;i++) {
  float area=cloudCardArea(v3(i*.1f,0,10),v3(5,0,0),v3(0,5,0));
  assert(area<=previous+1e-6f && area>=0);previous=area;
 }
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
 /* A nearby puff just outside the image must not spend the distant cloud's
  * entire budget when its conservative bound enters the frustum. */
 CloudVertex crowded[96*54];
 for(int fly=0;fly<2;fly++) {
  flying=fly;cameraEye=v3(0,0,RADIUS+(fly?5000:100));
  for(int i=0;i<2304;i++)cloudCatalog[i]=(CloudPuff){add(cameraEye,v3(0,-10000,0)),100,0,1};
  cloudCatalog[0]=(CloudPuff){add(cameraEye,v3(3070,1000,200)),2000,0,1};
  cloudCatalog[1]=(CloudPuff){add(cameraEye,v3(0,10000,3000)),1000,0,1};
  for(int step=-20;step<=20;step++) {
   float yaw=step*.001f;viewForward=v3(sinf(yaw),cosf(yaw),0);
   viewRight=v3(cosf(yaw),-sinf(yaw),0);viewUp=v3(0,0,1);camera();cloudsDraw();
   GLint bytes=0;glBindBuffer(GL_ARRAY_BUFFER,cloudVbo);glGetBufferParameteriv(GL_ARRAY_BUFFER,GL_BUFFER_SIZE,&bytes);
   assert(bytes>=0 && bytes<=(int)sizeof(crowded));glGetBufferSubData(GL_ARRAY_BUFFER,0,bytes,crowded);
   float farAlpha=0;for(int i=0;i<bytes/(int)sizeof(*crowded);i++)if(crowded[i].p.y>5000)farAlpha+=crowded[i].alpha;
   assert(farAlpha>10); /* Same distant puff remains fully represented. */
  }
 }
 /* At altitude the geometric horizon includes both observer and cloud height. */
 cameraEye=v3(0,0,RADIUS+30000);clipFar=500000;
 V3 horizonPuff=mul(v3(0,sinf(.65f),cosf(.65f)),RADIUS+5000);
 viewForward=norm(add(horizonPuff,mul(cameraEye,-1)));
 viewRight=norm(cross(viewForward,v3(0,0,1)));viewUp=cross(viewRight,viewForward);
 for(int i=0;i<2304;i++)cloudCatalog[i]=(CloudPuff){add(cameraEye,mul(viewForward,-10000)),100,0,1};
 cloudCatalog[0]=(CloudPuff){horizonPuff,2400,0,1};camera();cloudsDraw();assert(cloudDrawCount==1);
 assert(glGetError()==GL_NO_ERROR);
 cloudsClose();SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();
 puts("PASS fixed cloud cards/weights, roll and camera loops, offscreen-budget continuity in walk/fly and high-altitude horizon");
 return 0;
}
