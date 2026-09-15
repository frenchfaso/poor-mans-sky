#version 120
// SPDX-License-Identifier: MPL-2.0
varying vec2 uv;
varying vec4 color;
void main(){uv=gl_MultiTexCoord0.xy;color=gl_Color;gl_Position=gl_ModelViewProjectionMatrix*gl_Vertex;}
