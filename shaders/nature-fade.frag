#version 120
// SPDX-License-Identifier: MPL-2.0
uniform vec3 fogColor;
uniform float exposure,alphaCutoff;
uniform sampler2D foliageTex,lodMask;
uniform vec2 fadeRange;
varying vec3 color;
varying float fog,visibility;
varying vec2 uv;
void main(){
 float rank=texture2D(lodMask,gl_FragCoord.xy/8.0).r;
 if(rank<fadeRange.x || rank>=fadeRange.y)discard;
 vec4 leaf=texture2D(foliageTex,uv);
 if(leaf.a*visibility<alphaCutoff)discard;
 gl_FragColor=vec4(sqrt(max(mix(color*leaf.rgb*leaf.rgb,fogColor,fog)*exposure,vec3(0))),1);
}
