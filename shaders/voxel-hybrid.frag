#version 120
// SPDX-License-Identifier: MPL-2.0
uniform sampler2D detailTex;
uniform vec3 sun,ambientLight,sunLight,fogColor,ambientUp,eyePhase;
uniform float exposure,fogHeightFactor,fastMode;
varying vec3 delta,normal,baseColor;
void main(){
 vec3 n=normalize(normal),base=baseColor*baseColor;
 float reliefWeight=clamp(1.0-dot(delta,delta)/10000.0,0.0,1.0)*(1.0-fastMode);
 vec3 materialPos=delta+eyePhase;
 vec3 w=abs(n);w/=w.x+w.y+w.z;
 vec3 a=texture2D(detailTex,materialPos.yz*.17).rgb;
 vec3 b=texture2D(detailTex,materialPos.xz*.17).rgb;
 vec3 c=texture2D(detailTex,materialPos.xy*.17).rgb;
 vec3 detail=a*w.x+b*w.y+c*w.z;
 vec3 gradient=vec3((b.g-.5)*w.y+(c.g-.5)*w.z,(a.g-.5)*w.x+(c.b-.5)*w.z,(a.b-.5)*w.x+(b.b-.5)*w.y);
 float diffuse=clamp(dot(n,sun)-dot(gradient,sun)*.26*reliefWeight,0.0,1.0);
 vec3 ambient=mix(ambientLight*vec3(.60,.48,.32),ambientLight,.35+.65*max(dot(n,ambientUp),0.0));
 vec3 color=base*mix(1.025,.65+detail.r*.75,reliefWeight)*(ambient+sunLight*diffuse);
 float fog=(1.0-exp2(-length(delta)*.00009))*fogHeightFactor;
 gl_FragColor=vec4(sqrt(max(mix(color,fogColor,fog)*exposure,vec3(0))),1);
}
