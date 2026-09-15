#version 120
// SPDX-License-Identifier: MPL-2.0
uniform sampler2D sceneTex,bloomTex;
uniform vec2 scale,halfTexel;
uniform float bloom;
varying vec2 uv;
void main(){
 vec3 c=texture2D(sceneTex,clamp(uv*scale,halfTexel,scale-halfTexel)).rgb;
 c+=texture2D(bloomTex,uv).rgb*bloom*.09;
 gl_FragColor=vec4(c,1.0);
}
