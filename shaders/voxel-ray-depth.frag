#version 120
// SPDX-License-Identifier: MPL-2.0
uniform sampler2D stateTex;
uniform float nearRatio;
varying vec2 uv;
void main(){
 vec4 state=texture2D(stateTex,uv);if(state.a<.5)discard;
 float t=dot(state.rgb,vec3(1.0,1.0/255.0,1.0/65025.0));
 float value=floor(clamp(nearRatio/max(t,nearRatio),0.0,1.0)*16777215.0);
 float hi=floor(value/65536.0);value-=hi*65536.0;
 float mid=floor(value/256.0);
 gl_FragColor=vec4(hi,mid,value-mid*256.0,255.0)/255.0;
}
