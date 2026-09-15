#version 120
// SPDX-License-Identifier: MPL-2.0
uniform sampler2D lightTex;
uniform sampler2D detailTex;
varying vec3 world;
varying float fog;
void main(){
 vec4 light=texture2D(lightTex,world.xz/2048.0+.5);
 vec3 top=texture2D(detailTex,world.xz*.047).rgb;
 vec3 wall=texture2D(detailTex,vec2(world.x+world.z,world.y*1.3)*.038).rgb;
 vec3 detail=mix(wall,top,light.a);
 float micro=.68+detail.r*.70+(detail.g-detail.b)*.16;
 gl_FragColor=vec4(mix(light.rgb*micro,vec3(.57,.64,.68),fog),1.0);
}
