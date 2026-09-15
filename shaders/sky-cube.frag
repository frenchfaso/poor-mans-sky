#version 120
// SPDX-License-Identifier: MPL-2.0
varying vec2 uv;
uniform vec3 forwardDir;
uniform vec3 rightDir;
uniform vec3 upDir;
uniform vec2 lens;
uniform samplerCube skyTex;
void main(){vec2 p=(uv*2.0-1.0)*lens;gl_FragColor=textureCube(skyTex,forwardDir+rightDir*p.x+upDir*p.y);}
