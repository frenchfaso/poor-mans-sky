#version 120
// SPDX-License-Identifier: MPL-2.0
uniform vec3 eye,patchCenter,waterOffset,sun,ambientLight,sunLight,fogColor;
uniform float time,daylight,reflectionReady;
uniform vec2 wavePhase;
uniform mat4 reflectionVP;
uniform vec3 reflectionOffset;
varying vec3 radial,waterUV,halfVector,waterBase,skyBase;
varying vec4 reflectionProjection,response;
void main(){
 radial=normalize(patchCenter+gl_Vertex.xyz);
 float depth=-gl_MultiTexCoord0.z;
 waterUV=gl_Vertex.xyz+waterOffset;
 // Sea vertices already lie on the stitched surface. Waves remain in shading.
 vec3 p=gl_Vertex.xyz;
 vec3 viewDir=eye-p;
 float distance=length(viewDir);
 vec3 V=normalize(viewDir);
 float facing=max(dot(radial,V),0.0),f=1.0-facing;
 response=vec4(depth,.025+.9*f*f*f*f*f,.14/(1.0+distance*.001),reflectionReady*clamp((1500.0-distance)/750.0,0.0,1.0));
 halfVector=V+sun;
 waterBase=mix(vec3(.006,.019,.025),vec3(.048,.069,.050),exp2(-max(depth,0.0)*.20))*(ambientLight+sunLight*.32);
 skyBase=mix(fogColor,vec3(.026,.072,.16)*daylight,facing);
 reflectionProjection=reflectionVP*vec4(p+reflectionOffset,1.0);
 gl_Position=gl_ModelViewProjectionMatrix*vec4(p,1.0);
}
