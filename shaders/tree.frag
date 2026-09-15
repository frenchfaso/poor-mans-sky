#version 120
// SPDX-License-Identifier: MPL-2.0
uniform sampler2D treeTex;
varying vec2 uv;
varying float fog;
varying float tint;
void main(){vec4 c=texture2D(treeTex,uv);if(c.a<.45)discard;gl_FragColor=vec4(mix(c.rgb*tint,vec3(.57,.64,.68),fog),1.0);}
