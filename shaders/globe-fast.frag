#version 120
// SPDX-License-Identifier: MPL-2.0
uniform sampler2D atlas;
uniform vec2 page;
uniform vec3 sun,fogColor,ambientLight,sunLight;
uniform float exposure;
varying vec3 normal;
varying vec3 ambientTerm;
varying vec2 localUV;
varying float fog;
varying float terrainHeight;
uniform float reflectionPass;
void main(){
 if(reflectionPass>.5 && terrainHeight<.05)discard;
 vec3 base=texture2D(atlas,page+(vec2(4.0)+localUV*120.0)/2048.0).rgb;
 vec3 col=base*base*1.025*(ambientTerm+sunLight*max(dot(normalize(normal),sun),0.0));
 gl_FragColor=vec4(sqrt(max(mix(col,fogColor,fog)*exposure,vec3(0))),1);
}
