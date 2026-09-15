#version 120
// SPDX-License-Identifier: MPL-2.0
varying vec2 uv;
uniform sampler2D heightTex;
uniform vec3 sun;
uniform vec4 distances;
float h(vec2 p){return texture2D(heightTex,p).r*512.0-96.0;}
void main(){
 float base=h(uv)+1.5;
 vec2 stepUV=sun.xz/2048.0;
 float s=1.0;
 s=min(s,clamp((base+sun.y*distances.x-h(uv+stepUV*distances.x))*.14+.5,0.0,1.0));
 s=min(s,clamp((base+sun.y*distances.y-h(uv+stepUV*distances.y))*.12+.5,0.0,1.0));
 s=min(s,clamp((base+sun.y*distances.z-h(uv+stepUV*distances.z))*.09+.5,0.0,1.0));
 s=min(s,clamp((base+sun.y*distances.w-h(uv+stepUV*distances.w))*.06+.5,0.0,1.0));
 gl_FragColor=vec4(s,s,s,1.0);
}
