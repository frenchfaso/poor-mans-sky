#version 120
// SPDX-License-Identifier: MPL-2.0
uniform vec3 eye;
uniform vec3 sun;
varying vec3 world;
varying vec3 normal;
void main(){vec3 n=normalize(normal);vec3 v=normalize(eye-world);float rim=1.0-abs(dot(n,v));float a=pow(rim,4.0)*.32*max(dot(n,sun)*.7+.3,0.0);gl_FragColor=vec4(.30,.56,.87,a);}
