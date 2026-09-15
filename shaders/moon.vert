#version 120
// SPDX-License-Identifier: MPL-2.0
uniform vec3 sun,localEye,bodyCenter,patchCenter,worldSun;
uniform mat3 bodyRotation;
uniform float morph;
varying float reliefWeight,solarVisibility;
varying vec3 lunarNormal,worldRay;
varying vec3 albedo;
varying vec2 uv;
void main(){
 vec3 position=mix(gl_MultiTexCoord1.xyz,gl_Vertex.xyz,morph);
 vec3 delta=position-localEye;
 reliefWeight=clamp(1.0-dot(delta,delta)/6400.0,0.0,1.0);
 lunarNormal=gl_Normal;albedo=gl_Color.rgb;uv=gl_MultiTexCoord0.xy;
 worldRay=bodyRotation*delta;
 // Planet shadow with a finite solar angular radius (soft penumbra).
 vec3 world=bodyCenter+bodyRotation*(position+patchCenter);
 float along=-dot(world,worldSun),impact=length(world+worldSun*max(along,0.0));
 float penumbra=max(1.0,along*.00465);
 solarVisibility=mix(1.0,smoothstep(200000.0-penumbra,200000.0+penumbra,impact),step(0.0,along));
 gl_Position=gl_ModelViewProjectionMatrix*vec4(position,1);
}
