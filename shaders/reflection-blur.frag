#version 120
// SPDX-License-Identifier: MPL-2.0
uniform sampler2D sceneTex;
uniform vec2 direction;
uniform float finalPass;
varying vec2 uv;
void main(){
 vec4 c=texture2D(sceneTex,uv)*.227027;
 c+=texture2D(sceneTex,uv+direction*1.384615)*.316216;
 c+=texture2D(sceneTex,uv-direction*1.384615)*.316216;
 c+=texture2D(sceneTex,uv+direction*3.230769)*.070270;
 c+=texture2D(sceneTex,uv-direction*3.230769)*.070270;
 c.rgb=mix(c.rgb,c.rgb/max(c.a,.001),finalPass);
 c.a*=mix(1.0,.65,finalPass);
 gl_FragColor=c;
}
