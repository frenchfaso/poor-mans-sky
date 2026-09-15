#version 120
// SPDX-License-Identifier: MPL-2.0
uniform sampler2D detailTex,reflectionTex;
uniform vec3 sunLight;
uniform float time,exposure;
varying vec3 radial,waterUV,halfVector,waterBase,skyBase;
varying vec4 reflectionProjection,response;
void main(){
 if(response.x<.04)discard;
 vec3 w=abs(radial);w/=w.x+w.y+w.z;
 vec3 a=texture2D(detailTex,waterUV.yz*.025+vec2(time*.006,-time*.004)).rgb;
 vec3 b=texture2D(detailTex,waterUV.xz*.025+vec2(-time*.004,time*.005)).rgb;
 vec3 c=texture2D(detailTex,waterUV.xy*.025+vec2(time*.005,time*.003)).rgb;
 vec3 d=a*w.x+b*w.y+c*w.z;
 vec3 N=normalize(radial+(d-.5)*response.z);
 vec2 uv=reflectionProjection.xy/max(reflectionProjection.w,.01)*.5+.5;
 uv+=(d.gb-.5)*.006;
 float edge=clamp(min(min(uv.x,uv.y),min(1.0-uv.x,1.0-uv.y))*30.0,0.0,1.0);
 vec4 scene=texture2D(reflectionTex,clamp(uv,.004,.996));
 float weight=scene.a*edge*response.w*step(.01,reflectionProjection.w);
 vec3 reflection=mix(skyBase,scene.rgb*scene.rgb/max(exposure,.001),weight);
 float spec=pow(max(dot(N,normalize(halfVector)),0.0),96.0);
 vec3 col=mix(waterBase,reflection,response.y)+sunLight*spec*.34;
 float foam=clamp(1.0-response.x*.8,0.0,1.0)*clamp((d.r-.48)*4.0,0.0,1.0)*.22;
 col+=sunLight*foam;
 gl_FragColor=vec4(sqrt(max(col*exposure,vec3(0))),1.0);
}
