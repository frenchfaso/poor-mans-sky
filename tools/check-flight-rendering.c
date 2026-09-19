// SPDX-License-Identifier: MPL-2.0
#define SDL_MAIN_HANDLED
#define main poor_mans_sky_application_main
#include "../src/poor-mans-sky.c"
#undef main
#include <assert.h>
static void testPlane(float z,float red,float green,float blue) {
  glColor4f(red,green,blue,1);
  glBegin(GL_QUADS);
  glVertex3f(-2,-2,-z);glVertex3f(2,-2,-z);
  glVertex3f(2,2,-z);glVertex3f(-2,2,-z);glEnd();
}
static void testDepth(float near,float far,double lo,double hi,float obstacle,int visible) {
  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
  clipNear=near;clipFar=far;glDepthRange(lo,hi);camera();
  testPlane(obstacle,0,1,0);
  actorCamera(near,far,lo,hi);testPlane(14,1,0,0);
  unsigned char pixel[4];glReadPixels(32,32,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
  assert(visible ? pixel[0]>240 && pixel[1]<10 : pixel[1]>240 && pixel[0]<10);
}
int main(void) {
  assert(!SDL_Init(SDL_INIT_VIDEO));
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,24);
  window=SDL_CreateWindow("flight rendering check",0,0,64,64,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
  assert(window);context=SDL_GL_CreateContext(window);assert(context);
  resourceInit();width=height=rw=rh=64;glViewport(0,0,64,64);
  viewForward=v3(0,0,-1);viewRight=v3(1,0,0);viewUp=v3(0,1,0);
  glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);glDisable(GL_CULL_FACE);
  flying=1;
  testDepth(.25f,80000,.01,.5,.5f,0);
  testDepth(2,80000,.01,.5,8,0);testDepth(2,80000,.01,.5,20,1);
  testDepth(2,80000,.5,1,8,0);testDepth(2,80000,.5,1,20,1);
  /* Distant near clipping must not erase a ship 14 m from the chase camera. */
  testDepth(6000,2000000,.01,.5,80000,1);
  flying=0;testDepth(.75f,80000,.01,.5,8,0);testDepth(.75f,80000,.01,.5,20,1);
  /* A translucent foreground layer must blend over the opaque ship. */
  flying=1;testDepth(2,80000,.01,.5,20,1);
  glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glDepthMask(GL_FALSE);
  glColor4f(0,0,1,.5f);glBegin(GL_QUADS);
  glVertex3f(-2,-2,-8);glVertex3f(2,-2,-8);glVertex3f(2,2,-8);glVertex3f(-2,2,-8);glEnd();
  unsigned char pixel[4];glReadPixels(32,32,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
  assert(pixel[0]>120 && pixel[0]<136 && pixel[2]>120 && pixel[2]<136);
  glDepthMask(GL_TRUE);glDisable(GL_BLEND);
  /* Camera lighting must not use the old player position or view roll. */
  sun=v3(1,0,0);eye=v3(-RADIUS,0,0);cameraEye=v3(RADIUS,0,0);
  updateSceneLighting();assert(solarDaylight==1 && sceneExposure==1);
  cameraEye=eye;updateSceneLighting();assert(solarDaylight<.03f && sceneExposure>3);
  cameraEye=v3(RADIUS+200000,0,0);updateSceneLighting();assert(sceneExposure==1);
  /* Isolate roll and boarding-state invalidation with an unchanged camera,
   * ship position/forward vector, sun, and a still-valid four-frame cache. */
  V3 radial=homeDirection();cameraEye=mul(radial,RADIUS+elevation(radial)+5);
  eye=add(cameraEye,mul(radial,500));shipPos=eye;
  flightForward=norm(cross(radial,v3(0,1,0)));shipHeading=flightForward;flightUp=radial;
  sun=radial;scene=target(64,64,1,0);flying=1;frameNo=0;
  sunShadowUpdate();assert(sunReady && sunUpdates==1);
  frameNo=1;sunShadowUpdate();assert(sunUpdates==1);
  flightUp=rotateAround(flightUp,flightForward,.3f);
  sunShadowUpdate();assert(sunUpdates==2);
  sunShadowUpdate();assert(sunUpdates==2);
  flying=0;sunShadowUpdate();assert(sunUpdates==3);
  assert(glGetError()==GL_NO_ERROR);
  sunShadowClose();SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();
  puts("PASS ship/world depth, space clipping, foreground blending, camera lighting, shadow roll/state cache");
  return 0;
}
