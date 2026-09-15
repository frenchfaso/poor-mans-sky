#version 120
// SPDX-License-Identifier: MPL-2.0
varying vec2 uv;
uniform sampler2D noiseTex;
void main(){
 vec3 a=texture2D(noiseTex,uv*.0625).rgb;
 vec3 b=texture2D(noiseTex,uv*.25).rgb;
 vec3 c=texture2D(noiseTex,uv).rgb;
 vec3 grain=texture2D(noiseTex,uv*2.0).rgb;
 float h=dot(a,vec3(.20,.12,.08))+dot(b,vec3(.13,.10,.07))+dot(c,vec3(.13,.10,.07));
 h=clamp(h*.8+grain.r*.22,0.0,1.0);
 gl_FragColor=vec4(h,clamp((grain.r-grain.b)*.22+(a.r-a.b)*.5+(b.r-b.b)*.3+.5,0.0,1.0),clamp((grain.g-grain.r)*.22+(a.g-a.r)*.5+(b.g-b.r)*.3+.5,0.0,1.0),1.0);
}
