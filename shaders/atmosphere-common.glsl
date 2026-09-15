// SPDX-License-Identifier: MPL-2.0
// Shared by the background sky and the atmosphere in front of the moon.
vec3 atmosphereSky(vec3 ray,vec3 radial,vec3 sun,vec3 zenithColor,vec3 horizonColor,vec3 sunsetColor,float horizonDip,float altitude,float planetRadius,out float limb,out float spaceBlend){
 float radialRay=dot(ray,radial);
 float elevation=radialRay+horizonDip;
 float rim=exp2(-abs(elevation)*7.0);
 vec3 sky=mix(zenithColor,horizonColor,rim);
 float towards=max(dot(ray,sun),0.0);
 sky+=sunsetColor*towards*towards*rim;
 // Outside the atmosphere, glow belongs to rays grazing the planet,
 // not to an angular gradient covering the entire background.
 spaceBlend=smoothstep(8000.0,50000.0,altitude);
 float impact=(planetRadius+max(altitude,0.0))*sqrt(max(0.0,1.0-radialRay*radialRay));
 float tangentHeight=max(impact-planetRadius,0.0);
 limb=exp2(-tangentHeight/5000.0)*(1.0-smoothstep(20000.0,40000.0,tangentHeight))*step(radialRay,0.0);
 vec3 orbitalGlow=(horizonColor+sunsetColor*towards*towards)*limb;
 sky=mix(sky,orbitalGlow,spaceBlend);
 return sky;
}
