#version 120
// SPDX-License-Identifier: MPL-2.0
varying vec2 uv;
uniform sampler2D heightTex;
float height(vec2 p){return texture2D(heightTex,p).r*512.0-96.0;}
void main(){
 float l=height(uv-vec2(1.0/1024.0,0.0));
 float r=height(uv+vec2(1.0/1024.0,0.0));
 float d=height(uv-vec2(0.0,1.0/1024.0));
 float u=height(uv+vec2(0.0,1.0/1024.0));
 vec3 n=normalize(vec3(l-r,4.0,d-u));
 float h=height(uv);
 float cavity=clamp(1.0-((l+r+d+u)*.25-h)*.22,.4,1.0);
 gl_FragColor=vec4(n*.5+.5,cavity);
}
