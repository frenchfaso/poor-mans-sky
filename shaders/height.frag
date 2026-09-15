#version 120
// SPDX-License-Identifier: MPL-2.0
varying vec2 uv;
uniform sampler2D noiseTex;
uniform sampler2D broadTex;
void main(){
 vec2 p=uv*2.0-1.0;
 vec2 broad=texture2D(broadTex,uv).rg;
 float a=broad.r;
 float b=broad.g;
 float c=texture2D(noiseTex,uv*.13).b;
 float d=texture2D(noiseTex,uv*.31).r;
 float e=texture2D(noiseTex,uv*.79).g;
 float f=texture2D(noiseTex,uv*1.73).b;
 vec2 l=(p-vec2(-.57,-.05))*vec2(2.2,1.05);
 vec2 r=(p-vec2(.58,-.27))*vec2(2.55,1.13);
 vec2 far=(p-vec2(.03,-.82))*vec2(1.3,4.0);
 float land=max(max(1.0-dot(l,l),1.0-dot(r,r)),1.0-dot(far,far));
 float ridge=1.0-abs(a*2.0-1.0);
 float detail=(b-.5)*65.0+(c-.5)*30.0+(d-.5)*13.0+(e-.5)*5.0+(f-.5)*2.2;
 float h=max(land,0.0)*(85.0+ridge*ridge*240.0)+detail-42.0;
 float h01=clamp((h+96.0)/512.0,0.0,.9999);
 gl_FragColor=vec4(h01,b,0.0,1.0);
}
