#version 120
// SPDX-License-Identifier: MPL-2.0
uniform sampler2D sceneTex;
uniform vec2 direction;
varying vec2 uv;
void main(){
 vec3 c=texture2D(sceneTex,uv).rgb*.227027;
 c+=texture2D(sceneTex,uv+direction*1.384615).rgb*.316216;
 c+=texture2D(sceneTex,uv-direction*1.384615).rgb*.316216;
 c+=texture2D(sceneTex,uv+direction*3.230769).rgb*.070270;
 c+=texture2D(sceneTex,uv-direction*3.230769).rgb*.070270;
 gl_FragColor=vec4(c,1.0);
}
