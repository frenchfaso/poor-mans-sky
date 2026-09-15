// SPDX-License-Identifier: MPL-2.0
#define main poor_mans_sky_application_main
#include "../poor-mans-sky.c"
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
 SDL_DestroyMutex(mutex);SDL_Quit();puts("PASS circular orbit, synchronous rotation, local/world inverse, rotating collision floor, phases, planetshine, attached player, space flight and atmosphere selection");return 0;
}
