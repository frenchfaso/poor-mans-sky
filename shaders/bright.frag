#version 120
// SPDX-License-Identifier: MPL-2.0
uniform sampler2D sceneTex;
uniform vec2 scale;
varying vec2 uv;
void main(){
 vec3 c=texture2D(sceneTex,uv*scale).rgb;
 gl_FragColor=vec4(max(c-.65,0.0)*1.8,1.0);
}
