#version 120
// SPDX-License-Identifier: MPL-2.0
uniform vec3 eye,ambientLight,ambientUp;
varying vec3 ambientTerm;
uniform vec3 detailOrigin;
uniform float fogHeightFactor;
uniform vec4 fogPlane;
varying vec3 materialPos;
varying vec3 normal;
varying vec2 localUV;
varying float fog;
varying float reliefWeight;
varying float terrainHeight;
uniform vec4 reflectionPlane;
void main(){vec3 reliefDelta=gl_Vertex.xyz-eye;reliefWeight=clamp(1.0-dot(reliefDelta,reliefDelta)/10000.0,0.0,1.0);ambientTerm=mix(ambientLight*vec3(.60,.48,.32),ambientLight,.35+.65*max(dot(gl_Normal,ambientUp),0.0));terrainHeight=dot(gl_Vertex,reflectionPlane);materialPos=gl_Vertex.xyz+detailOrigin;normal=gl_Normal;localUV=gl_MultiTexCoord0.xy;fog=clamp(dot(vec4(gl_Vertex.xyz,1),fogPlane),0.0,fogHeightFactor);gl_Position=gl_ModelViewProjectionMatrix*gl_Vertex;}
