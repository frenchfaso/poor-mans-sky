// SPDX-License-Identifier: MPL-2.0
/* Two analytic body tests per object, never per fragment. The solar disk gives
 * a narrow penumbra; local terrain shadows remain a separate near-field pass. */
static float bodySolarVisibility(V3 position,V3 center,float radius,V3 light) {
  V3 p=add(position,mul(center,-1));
  float along=-dot(p,light);
  if(along<=0)return 1;
  /* Crater floors can be below the nominal sphere. Avoid placing an observer
   * inside its own analytic occluder; preserve the actual local horizon. */
  float distance2=dot(p,p);
  if(distance2<radius*radius)radius=fmaxf(0,sqrtf(distance2)-.5f);
  V3 perpendicular=cross(p,light);
  float impact2=dot(perpendicular,perpendicular),penumbra=fmaxf(1,along*.00465f);
  float outer=radius+penumbra,inner=fmaxf(0,radius-penumbra);
  if(impact2>=outer*outer)return 1;
  if(impact2<=inner*inner)return 0;
  float t=clampf((sqrtf(impact2)-radius+penumbra)/(2*penumbra),0,1);
  return t*t*(3-2*t);
}
static float solarVisibilityAt(V3 position) {
  V3 light=norm(sun);
  float visible=bodySolarVisibility(position,v3(0,0,0),RADIUS,light);
  if(visible>0)visible*=bodySolarVisibility(position,moonCenter(),MOON_RADIUS,light);
  return visible;
}
static V3 directSunlightAt(V3 position) {
  float distance=sqrtf(dot(position,position));
  float air=exp2f(-fmaxf(distance-RADIUS,0)/13000.f);
  float height=dot(position,sun)/fmaxf(distance,1);
  float strength=(1-air+air*clampf((height+.06f)*5,0,1))*solarVisibilityAt(position);
  return mul(v3(1.05f,.94f,.79f),strength);
}
