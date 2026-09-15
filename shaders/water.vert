#version 120
// SPDX-License-Identifier: MPL-2.0
uniform float time;
uniform vec3 eye;
varying vec3 world;
varying vec4 proj;
varying vec3 viewDir;
varying float fog;
void main(){
 vec4 p=gl_Vertex;
 p.y=sin(p.x*.045+time*.8)*.16+sin(p.z*.035-time*.65)*.13;
 world=p.xyz;
 viewDir=eye-p.xyz;
 fog=clamp(1.0-exp2(-length(viewDir)*.00055),0.0,.94);
 proj=gl_ModelViewProjectionMatrix*p;
 gl_Position=proj;
}
