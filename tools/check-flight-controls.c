// SPDX-License-Identifier: MPL-2.0
#define main poor_mans_sky_application_main
#include "../poor-mans-sky.c"
#undef main
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void){
 V3 f=v3(0,0,-1),v=v3(0,0,0);
 for(int i=0;i<60;i++)v=assistedFlight(v,f,1,0,0,1.f/60);
 CHECK(sqrtf(dot(v,v))<=25.01f && sqrtf(dot(v,v))>24);
 for(int i=0;i<1200;i++)v=assistedFlight(v,f,1,0,0,1.f/60);
 CHECK(fabsf(v.z+120)<.01f);
 v=v3(0,0,0);for(int i=0;i<1200;i++)v=assistedFlight(v,f,-.5f,0,0,1.f/60);
 CHECK(fabsf(v.z-30)<.01f);
 v=v3(0,0,0);for(int i=0;i<1200;i++)v=assistedFlight(v,f,1,2,0,1.f/60);
 CHECK(fabsf(v.z+20000)<1);
 for(int i=0;i<600;i++)v=assistedFlight(v,f,0,0,1,1.f/60);
 CHECK(sqrtf(dot(v,v))<.1f);
 flying=1;flightForward=f;flightUp=v3(0,1,0);lookX=PI;lookY=0;
 V3 cf,cr,cu;cameraBasis(&cf,&cr,&cu);CHECK(dot(cf,f)<-.999f);
 lookX=lookY=0;cameraBasis(&cf,&cr,&cu);CHECK(dot(cf,f)>.999f);
 lookY=1.2f;cameraBasis(&cf,&cr,&cu);CHECK(fabsf(dot(cf,cu))<.0001f);
 CHECK(dayLength==2400);
 puts("PASS acceleration, cruise, precise reverse, superboost/brake, 180-degree orbit/reset, 40-minute cycle");return 0;
}
