// SPDX-License-Identifier: MPL-2.0
#define main poor_mans_sky_application_main
#include "../src/poor-mans-sky.c"
#undef main
#include <assert.h>
int main(void) {
 V3 axis=norm(moonCenter());
 V3 regression=add(moonCenter(),mul(axis,MOON_RADIUS+60000));
 float oldNear=(sqrtf(dot(regression,regression))-RADIUS-20000)*.1f;
 assert(oldNear>60000 && worldNearPlane(regression,1)<60000);
 for(int sign=-1;sign<=1;sign+=2)for(int k=0;k<100;k++) {
  float gap=15000+k*2000.f;
  V3 p=add(moonCenter(),mul(axis,sign*(MOON_RADIUS+gap)));
  float near=worldNearPlane(p,1);
  assert(isfinite(near) && near>=2 && near<gap-12000+2);
  p=mul(axis,sign*(RADIUS+gap));near=worldNearPlane(p,1);
  assert(isfinite(near) && near>=2 && near<fmaxf(3,gap));
 }
 assert(worldNearPlane(v3(0,0,0),0)==.75f);
 V3 grid[13][13];for(int y=0;y<13;y++)for(int x=0;x<13;x++)grid[y][x]=v3(x,y,x*y);
 V3 q=moonParentSample(grid,.5f/12,.25f/12);
 assert(fabsf(q.x-.5f)<1e-6 && fabsf(q.y-.25f)<1e-6 && fabsf(q.z-.25f)<1e-6);
 q=moonParentSample(grid,.25f/12,.5f/12);assert(fabsf(q.z-.25f)<1e-6);
 MoonVertex mesh[MOON_VERTS];V3 center;moonMesh(2,0,0,0,&center,mesh);
 for(int i=0;i<MOON_VERTS;i++)assert(!memcmp(&mesh[i].p,&mesh[i].coarse,sizeof(V3)));
 moonMesh(2,4,5,7,&center,mesh);
 for(int i=0;i<MOON_VERTS;i++)assert(isfinite(mesh[i].coarse.x)&&isfinite(mesh[i].coarse.y)&&isfinite(mesh[i].coarse.z));
 MoonPatch patch={.face=2,.level=4,.x=5,.y=7};float span=2.f/8;
 V3 c=moonPoint(2,-1+(2+.5f)*span,-1+(3+.5f)*span);float threshold=span*MOON_RADIUS*1.6f*moonLodScale;
 cameraEye=add(add(moonCenter(),c),mul(norm(c),threshold));assert(moonMorphFactor(&patch)<.001f);
 cameraEye=add(add(moonCenter(),c),mul(norm(c),threshold*.5f));assert(moonMorphFactor(&patch)>.999f);
 occlusionEnabled=1;wire=0;occlusionCooldown=0;
 assert(occlusionPolicy(30,100,1));assert(!occlusionPolicy(9000,100,1));assert(!occlusionPolicy(30,12,1));
 occlusionCooldown=20;assert(!occlusionPolicy(30,100,10));assert(occlusionPolicy(30,100,20));
 cloudPrepareCatalog();CloudPuff first=cloudCatalog[0];cloudPrepareCatalog();assert(!memcmp(&first,&cloudCatalog[0],sizeof(first)));
 puts("PASS clipping both bodies/both directions, parent triangle interpolation, moon morph endpoints, occlusion policy, cached clouds");
 return 0;
}
