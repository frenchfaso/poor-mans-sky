#version 120
// SPDX-License-Identifier: MPL-2.0
uniform sampler2D foliageTex;
varying vec3 color;
varying vec2 uv;
void main(){vec4 leaf=texture2D(foliageTex,uv);if(leaf.a<.4)discard;gl_FragColor=vec4(sqrt(max(color,vec3(0)))*leaf.rgb,1);}
