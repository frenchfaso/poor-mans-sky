#version 120
// SPDX-License-Identifier: MPL-2.0
uniform sampler2D baseTex,normalTex,depthTex,detailTex;
uniform vec3 sun,ambientLight,sunLight,fogColor,ambientUp;
uniform vec3 forward,right,up,eyePhase;
uniform vec2 lens,clip,depthRange;
uniform float exposure,fogHeightFactor,fastMode;
varying vec2 uv;
void main(){
 vec4 depthSample=texture2D(depthTex,uv.yx);if(depthSample.a<.5)discard;
 float inv=dot(depthSample.rgb,vec3(16711680.0,65280.0,255.0))/16777215.0;
 float z=clip.x/max(inv,1.0/16777215.0);
 vec3 delta=(forward+right*((uv.x*2.0-1.0)*lens.x)+up*((uv.y*2.0-1.0)*lens.y))*z;
 vec3 n=normalize((texture2D(normalTex,uv.yx).rgb*255.0-128.0)/127.0);
 vec3 base=texture2D(baseTex,uv.yx).rgb;base*=base;
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
 float depth=clip.y/(clip.y-clip.x)*(1.0-inv);
 gl_FragDepth=mix(depthRange.x,depthRange.y,depth);
}
