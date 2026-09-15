#version 120
// SPDX-License-Identifier: MPL-2.0
uniform sampler2D reflectionTex;
uniform sampler2D detailTex;
uniform sampler2D heightTex;
uniform vec3 sun;
uniform float time;
uniform float reflections;
varying vec3 world;
varying vec4 proj;
varying vec3 viewDir;
varying float fog;
void main(){
 vec2 a=texture2D(detailTex,world.xz*.012+vec2(time*.007,time*.003)).gb-.5;
 vec2 b=texture2D(detailTex,world.xz*.027+vec2(-time*.004,time*.005)).gb-.5;
 vec3 N=normalize(vec3((a+b)*.20,1.0).xzy);
 vec3 V=normalize(viewDir);
 float fres=.04+.80*pow(1.0-max(dot(N,V),0.0),5.0);
 vec2 uv=proj.xy/proj.w*.5+.5+(a+b)*.009;
 vec3 refl=texture2D(reflectionTex,clamp(uv,.002,.998)).rgb;
 refl=mix(vec3(.46,.59,.64),refl,reflections);
 float h=texture2D(heightTex,world.xz/2048.0+.5).r*512.0-96.0;
 float shallow=clamp(1.0+h*.045,0.0,1.0);
 vec3 water=mix(vec3(.025,.105,.13),vec3(.18,.32,.27),shallow);
 vec3 col=mix(water,refl,fres);
 vec3 H=normalize(V+sun);
 float spec=pow(max(dot(N,H),0.0),96.0);
 col+=vec3(1.0,.73,.38)*spec*.5;
 float foam=clamp(1.0-abs(h+0.5)*.42,0.0,1.0)*clamp((a.x+b.y)*3.0+.4,0.0,1.0);
 col=mix(col,vec3(.67,.72,.66),foam*.5);
 gl_FragColor=vec4(mix(col,vec3(.57,.64,.68),fog),1.0);
}
