#version 120
// SPDX-License-Identifier: MPL-2.0
varying vec3 world;
varying vec3 normal;
void main(){world=gl_Vertex.xyz;normal=gl_Normal;gl_Position=gl_ModelViewProjectionMatrix*gl_Vertex;}
