#version 120
// SPDX-License-Identifier: MPL-2.0
uniform vec3 ambientUp;
varying float hemisphere;
varying vec3 n;
varying vec3 v;
varying vec2 uv;
void main(){vec4 p=gl_ModelViewMatrix*gl_Vertex;v=-p.xyz;n=normalize(gl_NormalMatrix*gl_Normal);hemisphere=.35+.65*max(dot(n,ambientUp),0.0);uv=gl_MultiTexCoord0.xy;gl_Position=gl_ProjectionMatrix*p;}
