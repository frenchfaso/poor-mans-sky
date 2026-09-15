#version 120
// SPDX-License-Identifier: MPL-2.0
uniform vec3 paint,ambientUp;
varying float hemisphere;
uniform vec3 fogColor;
uniform vec3 ambientLight,sunLight;
uniform float exposure;
uniform vec3 lightDir;
uniform float shine;
uniform float textured;
uniform vec2 tile;
uniform sampler2D materialTex;
varying vec3 n;
varying vec3 v;
varying vec2 uv;
void main(){
 vec2 t=tile+vec2(.003)+uv*.494;
 vec3 albedo=texture2D(materialTex,t).rgb;
 float dx=albedo.r-texture2D(materialTex,t+vec2(.00049,0)).r;
 float dy=albedo.r-texture2D(materialTex,t+vec2(0,.00049)).r;
 vec3 N=normalize(n+vec3(dx,dy,0)*textured*.65);
 float d=max(dot(N,lightDir),0.0);
 vec3 V=normalize(v);
 float s=pow(max(dot(N,normalize(lightDir+V)),0.0),32.0)*shine;
 vec3 base=paint*mix(vec3(1),albedo*albedo*1.6,textured);
 vec3 ambient=mix(ambientLight*vec3(.60,.48,.32),ambientLight,hemisphere);
 vec3 c=base*(ambient+d*sunLight)+s*.22*sunLight;
 float skyFacing=clamp(dot(reflect(-V,N),ambientUp)*.5+.5,0.0,1.0);
 vec3 environment=mix(ambientLight*vec3(.35,.27,.17),ambientLight*1.4,skyFacing);
 float fresnel=1.0-max(dot(N,V),0.0);fresnel*=fresnel;
 c+=environment*min(shine,1.0)*(.04+.30*fresnel);
 c+=paint*max(shine-1.0,0.0)*.35;
 float fog=1.0-exp2(-length(v)*.00009);
 gl_FragColor=vec4(sqrt(max(mix(c,fogColor,fog)*exposure,vec3(0))),1.0);
}
