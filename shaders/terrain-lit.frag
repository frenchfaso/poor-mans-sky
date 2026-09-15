#version 120
// SPDX-License-Identifier: MPL-2.0
uniform sampler2D normalTex;
uniform sampler2D detailTex;
uniform sampler2D materialTex;
uniform vec3 sun;
varying vec3 world;
varying float fog;
void main(){
 vec2 uv=world.xz/2048.0+.5;
 vec3 n=texture2D(normalTex,uv).rgb*2.0-1.0;
 vec4 material=texture2D(materialTex,uv);
 vec3 top=texture2D(detailTex,world.xz*.047).rgb;
 vec3 wall=texture2D(detailTex,vec2(world.x+world.z,world.y*1.3)*.038).rgb;
 vec3 det=mix(wall,top,smoothstep(.5,.85,n.y));
 vec3 N=normalize(n+vec3(det.g-.5,0.0,det.b-.5)*.52);
 float diffuse=max(dot(N,sun),0.0)*material.a;
 vec3 col=material.rgb*(.73+det.r*.65)*(vec3(.30,.39,.49)+vec3(1.38,1.04,.71)*diffuse);
 col=mix(col,vec3(.57,.64,.68),fog);
 gl_FragColor=vec4(col,1.0);
}
