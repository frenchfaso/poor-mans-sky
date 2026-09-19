#version 120
// SPDX-License-Identifier: MPL-2.0
uniform sampler2D sceneTex;
uniform vec2 scale,halfTexel;
varying vec2 uv;
void main(){
 gl_FragColor=vec4(texture2D(sceneTex,clamp(uv*scale,halfTexel,scale-halfTexel)).rgb,1.0);
}
