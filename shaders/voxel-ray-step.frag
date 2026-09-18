#version 120
// SPDX-License-Identifier: MPL-2.0
uniform sampler2D stateTex,heightTex;
uniform vec3 eyeLocal;
uniform vec2 warp;
uniform float slope;
uniform vec2 tolerance;
varying vec2 uv;
varying vec3 ray;
vec3 packDistance(float t){
 vec3 e=fract(t*vec3(1.0,255.0,65025.0));
 return e-e.yzz*vec3(1.0/255.0,1.0/255.0,0.0);
}
void main(){
 vec4 state=texture2D(stateTex,uv);
 float t=dot(state.rgb,vec3(1.0,1.0/255.0,1.0/65025.0));
 vec3 p=eyeLocal+ray*t;
 vec2 mapUV=p.xz/(warp.x+abs(p.xz))*warp.y+.5;
 vec4 terrain=texture2D(heightTex,mapUV);
 float encoded=dot(terrain.rgb,vec3(1.0,1.0/255.0,1.0/65025.0))*2.0-1.0;
 float ground=(64.0/65536.0)*encoded/max(1.0-abs(encoded),.00001);
 float gap=p.y-ground;
 float valid=step(.999,terrain.a);
 float found=max(state.a,step(gap,max(tolerance.x,t*tolerance.y))*valid*step(t,.99998));
 float advance=max(gap,0.0)/(abs(ray.y)+slope*length(ray.xz)+.000001);
 t=min(.99999,mix(1.0,t+advance*(1.0-found)*.95,valid));
 gl_FragColor=vec4(packDistance(t),found);
}
