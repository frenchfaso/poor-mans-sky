#version 120
// SPDX-License-Identifier: MPL-2.0
uniform vec3 forwardDir,rightDir,upDir,radial,sun;
uniform vec3 zenithColor,horizonColor,sunsetColor;
uniform vec2 lens;
uniform float horizonDip,starVisibility,exposure,altitude,planetRadius;
uniform samplerCube starTex;
varying vec2 uv;
void main(){
 vec2 p=(uv*2.0-1.0)*lens;
 vec3 ray=normalize(forwardDir+rightDir*p.x+upDir*p.y);
 float elevation=dot(ray,radial)+horizonDip,limb,spaceBlend;
 vec3 sky=atmosphereSky(ray,radial,sun,zenithColor,horizonColor,sunsetColor,horizonDip,altitude,planetRadius,limb,spaceBlend);
 float star=textureCube(starTex,ray).r;star*=star;
 sky+=vec3(.6,.72,1.0)*star*starVisibility*mix(clamp(elevation*3.0,0.0,1.0),1.0-limb,spaceBlend)*.38;
 sky+=vec3(1.0,.78,.47)*smoothstep(.9996,.9999,dot(ray,sun));
 gl_FragColor=vec4(sqrt(max(sky*exposure,vec3(0))),1.0);
}
