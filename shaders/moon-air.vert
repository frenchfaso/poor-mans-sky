#version 120
// SPDX-License-Identifier: MPL-2.0
uniform vec3 sun,localEye,bodyCenter,patchCenter,worldSun;
uniform mat3 bodyRotation;
uniform float morph;
varying float reliefWeight,solarVisibility;
varying vec3 lunarNormal,worldRay;
varying vec3 albedo;
varying vec2 uv;
uniform vec3 observer,skyRadial,skySun,zenithColor,horizonColor,sunsetColor;
uniform float horizonDip,altitude,planetRadius,exposure;
float airDensity(vec3 p){return exp2(-max(length(p)-1.0,0.0)*planetRadius/8000.0*1.442695);}
float airColumn(vec3 ray,float distance){
 vec3 origin=observer/planetRadius;
 float b=dot(origin,ray),outer=1.0+60000.0/planetRadius;
 float disc=b*b-dot(origin,origin)+outer*outer;
 if(disc<=0.0)return 0.0;
 float root=sqrt(disc),lo=max(0.0,-b-root),hi=min(distance/planetRadius,-b+root);
 if(hi<=lo)return 0.0;
 float stepLength=(hi-lo)*.25;
 vec3 start=origin+ray*lo,stepRay=ray*stepLength;
 return stepLength*planetRadius*(airDensity(start+stepRay*.5)+airDensity(start+stepRay*1.5)+airDensity(start+stepRay*2.5)+airDensity(start+stepRay*3.5));
}

varying vec3 atmosphericSky,atmosphericTransmission;
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
 float distance=length(worldRay);vec3 ray=worldRay/max(distance,.001);
 float column=airColumn(ray,distance),limb,spaceBlend;
 atmosphericSky=atmosphereSky(ray,skyRadial,skySun,zenithColor,horizonColor,sunsetColor,horizonDip,altitude,planetRadius,limb,spaceBlend);
 atmosphericSky*=clamp(column/10.0,0.0,1.0);
 atmosphericTransmission=exp2(-column*.00003*vec3(.55,1,1.6));
 gl_Position=gl_ModelViewProjectionMatrix*vec4(position,1);
}
