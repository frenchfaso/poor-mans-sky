#version 120
// SPDX-License-Identifier: MPL-2.0
varying vec2 uv;
void main(){uv=gl_MultiTexCoord0.xy;gl_Position=vec4(gl_Vertex.xy,1.0,1.0);}
