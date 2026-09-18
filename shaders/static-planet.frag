#version 120
// SPDX-License-Identifier: MPL-2.0
uniform vec3 fogColor;
uniform float exposure;
varying vec3 lit;
varying float fog,waterHeight;
void main(){
 if(waterHeight<0.0)discard;
 gl_FragColor=vec4(sqrt(max(mix(lit,fogColor,fog)*exposure,vec3(0))),1);
}
