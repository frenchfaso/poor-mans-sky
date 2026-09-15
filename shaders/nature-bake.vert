#version 120
// SPDX-License-Identifier: MPL-2.0
varying vec3 color;
varying vec2 uv;
void main(){color=gl_Color.rgb;uv=gl_MultiTexCoord0.xy;gl_Position=gl_ModelViewProjectionMatrix*gl_Vertex;}
