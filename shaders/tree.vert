#version 120
// SPDX-License-Identifier: MPL-2.0
uniform vec3 eye;
uniform vec3 rightDir;
uniform float time;
varying vec2 uv;
varying float fog;
varying float tint;
void main(){
 uv=gl_MultiTexCoord0.xy;
 float size=gl_Vertex.w;
 vec3 p=gl_Vertex.xyz+rightDir*(uv.x-.5)*size*.55;
 p.y+=uv.y*size;
 p.x+=sin(time*1.2+p.x*.11)*uv.y*uv.y*.17;
 float dist=length(p-eye);fog=clamp(1.0-exp2(-dist*.00105),0.0,.94);
 tint=.84+fract(gl_Vertex.x*.073)*.27;
 gl_Position=gl_ModelViewProjectionMatrix*vec4(p,1.0);
}
