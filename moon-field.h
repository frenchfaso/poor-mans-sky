// SPDX-License-Identifier: MPL-2.0
/* Separate body coordinates: planet stays at zero, moon has its own surface.
 * Radius ratio is the Earth/Moon mean ratio; geometry stays body-local. */
#define MOON_RADIUS (RADIUS * .2727f)
#include "celestial.h"
static V3 moonCenter(void) { return celestial.center; }
static int nearMoon(V3 p) {V3 d=add(p,mul(moonCenter(),-1));return dot(d,d)<4*MOON_RADIUS*MOON_RADIUS;}
static float moonHeight(V3 d) {
  float h=180+750*noise3(mul(d,9))+240*noise3(mul(d,47));
  /* Impact catalog is prepared once per seed, never per vertex. */
  static V3 sites[28];static float radii[28];static uint32_t seed;static int ready;
  if(!ready || seed!=worldSeed) {
    for(int i=0;i<28;i++) {
      uint32_t a=hash3(i,913,worldSeed),b=hash3(i,727,worldSeed);
      float z=(a&65535)/32767.5f-1,angle=(b&65535)*(2*PI/65536.f);
      sites[i]=v3(sqrtf(fmaxf(0,1-z*z))*cosf(angle),z,sqrtf(fmaxf(0,1-z*z))*sinf(angle));
      radii[i]=1500+(a>>16)/65535.f*6500;
    }seed=worldSeed;ready=1;
  }
  for(int i=0;i<28;i++) {
    V3 c=sites[i];float radius=radii[i];
    V3 delta=add(d,mul(c,-1));
    float q=sqrtf(dot(delta,delta))*MOON_RADIUS/radius;
    if(q<1.4f) {float rim=1-fabsf(q-1)/.18f;h+=radius*(.075f*fmaxf(rim,0)-.12f*fmaxf(1-q*q,0));}
  }
  return h+26*noise3(mul(d,MOON_RADIUS/85))+3*noise3(mul(d,MOON_RADIUS/8));
}
static V3 bodyCenter(V3 p){return nearMoon(p)?moonCenter():v3(0,0,0);}
static V3 bodyUp(V3 p){return norm(add(p,mul(bodyCenter(p),-1)));}
static float bodyRadius(V3 p){return nearMoon(p)?MOON_RADIUS:RADIUS;}
static float bodyHeight(V3 p){V3 d=bodyUp(p);return nearMoon(p)?moonHeight(moonLocalVector(d)):fmaxf(elevation(d),0);}
static float bodyAltitude(V3 p){V3 d=add(p,mul(bodyCenter(p),-1));return sqrtf(dot(d,d))-bodyRadius(p);}
static V3 bodyFloor(V3 p,float offset){return add(bodyCenter(p),mul(bodyUp(p),bodyRadius(p)+bodyHeight(p)+offset));}
