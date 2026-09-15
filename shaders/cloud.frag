#version 120
// SPDX-License-Identifier: MPL-2.0
uniform sampler2D cloudTex;
varying vec2 uv;
varying vec4 color;
void main(){vec4 t=texture2D(cloudTex,uv);gl_FragColor=vec4(t.rgb*color.rgb,t.a*color.a);}
