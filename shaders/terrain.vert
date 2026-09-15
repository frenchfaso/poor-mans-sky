#version 120
// SPDX-License-Identifier: MPL-2.0
uniform vec3 eye;
varying vec3 world;
varying float fog;
void main(){
 world=gl_Vertex.xyz;
 float dist=length(world-eye);
 fog=clamp(1.0-exp2(-dist*.00105),0.0,.94);
 gl_Position=gl_ModelViewProjectionMatrix*gl_Vertex;
}
