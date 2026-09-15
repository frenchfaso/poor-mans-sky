#version 120
// SPDX-License-Identifier: MPL-2.0
uniform sampler2D lightTex;
varying vec3 world;
varying float fog;
void main(){gl_FragColor=vec4(mix(texture2D(lightTex,world.xz/2048.0+.5).rgb,vec3(.57,.64,.68),fog),1.0);}
