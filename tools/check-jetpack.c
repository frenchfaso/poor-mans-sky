// SPDX-License-Identifier: MPL-2.0
#define main poor_mans_sky_application_main
#include "../poor-mans-sky.c"
#undef main
#include <assert.h>
int main(void) {
  V3 axes[]={ {1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1} };
  for(int i=0;i<6;i++) {
    V3 up=axes[i], forward=norm(cross(up,v3(.31f,.52f,.79f)));
    V3 level=jetpackDirection(up,forward,0), above=jetpackDirection(up,forward,.8f);
    assert(dot(level,up)>.9999f && dot(above,up)>.9999f);
    float previous=0;
    for(int j=1;j<=15;j++) {
      V3 force=jetpackDirection(up,forward,-j*.1f);
      assert(fabsf(dot(force,force)-1)<.00001f);
      assert(dot(force,forward)>=previous-.00001f);
      assert(30*dot(force,up)>12); /* retains lift above gravity */
      previous=dot(force,forward);
    }
    V3 velocity=v3(0,0,0);
    V3 thrust=mul(jetpackDirection(up,forward,-.6f),30);
    for(int frame=0;frame<60;frame++)
      velocity=add(velocity,mul(add(thrust,mul(up,-12)),1.f/60));
    assert(dot(velocity,forward)>16 && dot(velocity,up)>12);
  }
  puts("PASS directional thrust: all six planet axes, upward lift, forward acceleration, fixed magnitude and tilt limit");
  return 0;
}
