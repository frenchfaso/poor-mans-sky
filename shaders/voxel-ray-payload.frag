#version 120
// SPDX-License-Identifier: MPL-2.0
uniform sampler2D stateTex,payloadTex;
uniform vec3 eyeLocal;
uniform vec2 warp;
varying vec2 uv;
varying vec3 ray;
void main(){
 vec4 state=texture2D(stateTex,uv);if(state.a<.5)discard;
 float t=dot(state.rgb,vec3(1.0,1.0/255.0,1.0/65025.0));
 vec3 p=eyeLocal+ray*t;
 gl_FragColor=texture2D(payloadTex,p.xz/(warp.x+abs(p.xz))*warp.y+.5);
}
