#version 120
// SPDX-License-Identifier: MPL-2.0
uniform sampler2D rockTex;
uniform vec2 bumpSun;
uniform vec3 sun,planetDir;
uniform float planetshine;
varying float reliefWeight,solarVisibility;
varying vec3 lunarNormal,albedo;
varying vec2 uv;
void main(){
 vec3 sampleColor=texture2D(rockTex,uv).rgb;
 vec3 n=normalize(lunarNormal);
 float direct=clamp(dot(n,sun)-dot(sampleColor.gb-vec2(128.0/255.0),bumpSun)*.38*reliefWeight,0.0,1.0);
 float lighting=.00015+direct*solarVisibility+planetshine*max(dot(n,planetDir),0.0);
 gl_FragColor=vec4(sqrt(max(vec3(sampleColor.r)*albedo*lighting,vec3(0))),1);
}
