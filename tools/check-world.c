// SPDX-License-Identifier: MPL-2.0
#define main poor_mans_sky_application_main
#include "../poor-mans-sky.c"
#undef main
int main(void) {
  worldSeed=20260911;
  V3 d=norm(v3(.31f,.52f,.79f));
  float first=elevation(d),lo=1e9f,hi=-1e9f;
  worldSeed=42;float other=elevation(d);worldSeed=20260911;
  float repeated=elevation(d);printf("Seed samples: %.6f / %.6f / %.6f\n",first,other,repeated);
  if(fabsf(first-repeated)>.001f||fabsf(first-other)<1)return 1;
  for(int face=0;face<6;face++)for(int y=0;y<20;y++)for(int x=0;x<20;x++){
    float h=elevation(direction(face,-1+x*2.0f/19,-1+y*2.0f/19));
    if(!isfinite(h))return 2;lo=fminf(lo,h);hi=fmaxf(hi,h);
  }
  for(int j=0;j<=64;j++){
    float v=-1+j/32.0f;
    if(fabsf(elevation(direction(0,1,v))-elevation(direction(5,-1,v)))>.01f)return 3;
  }
  if(hi-lo<4000)return 4;
  printf("PASS seed repeatability, distinct seeds, finite terrain, shared face edge; sampled relief %.0f..%.0f metres\n",lo,hi);
  return 0;
}
