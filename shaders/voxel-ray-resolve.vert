#version 120
// SPDX-License-Identifier: MPL-2.0
uniform vec3 rayForward,rayRight,rayUp;
uniform vec2 lens;
varying vec2 uv;
varying vec3 ray;
void main(){
 uv=gl_MultiTexCoord0.yx;gl_Position=gl_Vertex;
 ray=rayForward+rayRight*((uv.x*2.0-1.0)*lens.x)+rayUp*((uv.y*2.0-1.0)*lens.y);
}
