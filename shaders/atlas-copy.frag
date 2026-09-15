#version 120
// SPDX-License-Identifier: MPL-2.0
uniform sampler2D sourceTex;
varying vec2 uv;
void main(){gl_FragColor=texture2D(sourceTex,uv);}
