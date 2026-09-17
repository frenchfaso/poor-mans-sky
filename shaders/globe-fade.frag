#version 120
// SPDX-License-Identifier: MPL-2.0
uniform sampler2D transitionAtlas;
uniform vec2 transitionPage;
uniform float transitionMix;
uniform sampler2D atlas;
uniform sampler2D detailTex;
uniform vec2 page;
uniform vec3 sun;
uniform vec3 fogColor;
uniform vec3 ambientLight,sunLight;
uniform float exposure;

varying vec3 materialPos;
varying vec3 normal;
varying vec3 ambientTerm;
varying vec2 localUV;
varying float fog;
varying float reliefWeight;
void main(){
 vec3 n=normalize(normal);
 vec3 w=abs(n);w/=w.x+w.y+w.z;
 vec3 a=texture2D(detailTex,materialPos.yz*.17).rgb;
 vec3 b=texture2D(detailTex,materialPos.xz*.17).rgb;
 vec3 c=texture2D(detailTex,materialPos.xy*.17).rgb;
 vec3 d=a*w.x+b*w.y+c*w.z;
 vec3 base=texture2D(atlas,page+(vec2(4.0)+localUV*120.0)/2048.0).rgb;
 base=mix(texture2D(transitionAtlas,transitionPage+(vec2(4.0)+localUV*120.0)/vec2(1024.0,512.0)).rgb,base,transitionMix);
 base*=base;
 float micro=mix(1.025,.65+d.r*.75,reliefWeight);
 // Reuse the existing three detail samples as directional microrelief.
 vec3 gradient=vec3((b.g-.5)*w.y+(c.g-.5)*w.z,
                    (a.g-.5)*w.x+(c.b-.5)*w.z,
                    (a.b-.5)*w.x+(b.b-.5)*w.y);
 // Small-slope approximation avoids a second per-pixel normalization.
 float diffuse=clamp(dot(n,sun)-dot(gradient,sun)*.26*reliefWeight,0.0,1.0);
 vec3 col=base*micro*(ambientTerm+sunLight*diffuse);
 gl_FragColor=vec4(sqrt(max(mix(col,fogColor,fog)*exposure,vec3(0))),1.0);
}
