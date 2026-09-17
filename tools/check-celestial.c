// SPDX-License-Identifier: MPL-2.0
#define main poor_mans_sky_application_main
#include "../src/poor-mans-sky.c"
#undef main
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void) {
 SDL_Init(SDL_INIT_TIMER);mutex=SDL_CreateMutex();
 float distance=sqrtf(480000.f*480000.f+280000.f*280000.f+560000.f*560000.f);
 V3 localNear=mul(norm(v3(480000,280000,560000)),-1),point=mul(localNear,MOON_RADIUS+1200);
 for(int i=0;i<96;i++) {
  celestial=celestialAt(i*dayLength/12,.12f,.18f);
  CHECK(fabsf(sqrtf(dot(celestial.center,celestial.center))-distance)<.2f);
  CHECK(fabsf(dot(celestial.x,celestial.y))<1e-6f);CHECK(fabsf(dot(celestial.x,celestial.x)-1)<1e-6f);
  CHECK(dot(moonVector(localNear),mul(norm(moonCenter()),-1))>.99999f);
  V3 recovered=moonLocalPoint(moonWorldPoint(point));CHECK(sqrtf(dot(add(recovered,mul(point,-1)),add(recovered,mul(point,-1))))<.2f);
  V3 floor=bodyFloor(moonWorldPoint(point),2.5f);CHECK(fabsf(bodyAltitude(floor)-bodyHeight(floor)-2.5f)<.25f);
 }
 celestial=celestialAt(0,0,0);CHECK(lunarIlluminatedFraction(v3(0,0,0))<.001f);float earthlight=lunarPlanetshine();
 celestial=celestialAt(0,0,.5f);CHECK(lunarIlluminatedFraction(v3(0,0,0))>.999f);CHECK(lunarPlanetshine()<earthlight*.01f);
 celestial=celestialAt(0,0,.25f);CHECK(fabsf(lunarIlluminatedFraction(v3(0,0,0))-.5f)<.001f);
 celestial=celestialAt(0,.05f,.15f);float phase=lunarIlluminatedFraction(v3(0,0,0));
 celestial=celestialAt(0,.85f,.15f);CHECK(fabsf(lunarIlluminatedFraction(v3(0,0,0))-phase)<1e-6f);
 lunarPhaseOffset=.18f;clockTime=0;dayOffset=.12f;celestial=celestialAt(0,dayOffset,lunarPhaseOffset);
 eye=moonWorldPoint(point);shipPos=moonWorldPoint(add(point,v3(30,0,0)));heading=moonVector(v3(0,1,0));velocity=v3(0,0,0);
 for(int i=0;i<100;i++){clockTime+=.1f;advanceCelestial();}
 V3 recovered=moonLocalPoint(eye),delta=add(recovered,mul(point,-1));CHECK(sqrtf(dot(delta,delta))<2);
 V3 away=v3(-2000000,0,0);eye=away;clockTime+=10;advanceCelestial();CHECK(!memcmp(&eye,&away,sizeof(eye)));
 cameraEye=mul(norm(moonCenter()),RADIUS+2);CHECK(moonAtmosphereNeeded());
 cameraEye=moonWorldPoint(point);CHECK(!moonAtmosphereNeeded());
 /* Startup framing changes the orbit, not the observer or solar direction. */
 for(int spawn=0;spawn<3;spawn++) {
  V3 up=norm(v3(.3f+spawn*.4f,.7f-spawn*.5f,.8f));
  V3 observer=mul(up,RADIUS+35),forward=norm(cross(v3(0,1,0),up));
  V3 savedObserver=observer;
  celestialFrameSpawnMoon(observer,forward);
  celestial=celestialAt(0,.12f,.18f);
  CHECK(!memcmp(&observer,&savedObserver,sizeof(observer)));
  V3 delta=add(celestial.center,mul(observer,-1));
  V3 cameraForward=norm(add(mul(forward,cosf(-.24f)),mul(up,sinf(-.24f))));
  V3 right=norm(cross(cameraForward,up)),cameraUp=cross(right,cameraForward);
  float z=dot(delta,cameraForward),tx=.5773503f*640/480,ty=.5773503f;
  CHECK(z>0);
  CHECK(fabsf(dot(delta,right))+MOON_RADIUS*sqrtf(1+tx*tx)<z*tx);
  CHECK(fabsf(dot(delta,cameraUp))+MOON_RADIUS*sqrtf(1+ty*ty)<z*ty);
  CHECK(dot(norm(delta),up)>.05f);
  for(int step=0;step<96;step++) {
   celestial=celestialAt(step*dayLength/12,.12f,.18f);
   CHECK(fabsf(sqrtf(dot(celestial.center,celestial.center))-distance)<.3f);
   CHECK(dot(moonVector(localNear),mul(norm(moonCenter()),-1))>.99999f);
   CHECK(fabsf(dot(celestial.x,celestial.y))<1e-6f);
  }
 }
 SDL_DestroyMutex(mutex);SDL_Quit();puts("PASS circular orbit, synchronous rotation, local/world inverse, rotating collision floor, phases, planetshine, attached player, space flight, atmosphere selection and startup Moon framing");return 0;
}
