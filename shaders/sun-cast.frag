#version 120
// SPDX-License-Identifier: MPL-2.0
uniform sampler2D foliageTex;
uniform float cutoff;
varying vec2 uv;
void main(){
 if(texture2D(foliageTex,uv).a<cutoff)discard;
 vec2 encoded=fract(gl_FragCoord.z*vec2(1.0,255.0));
 encoded.x-=encoded.y/255.0;
 gl_FragColor=vec4(encoded,0.0,1.0);
}
