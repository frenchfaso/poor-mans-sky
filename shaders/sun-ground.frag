#version 120
// SPDX-License-Identifier: MPL-2.0
uniform sampler2D shadowTex;
uniform float strength,shadowTexel;
varying vec3 coord,relative;
float occluded(vec2 uv){return step(dot(texture2D(shadowTex,uv).rg,vec2(1.0,1.0/255.0))+.0008,coord.z);}
void main(){
 float fade=1.0-smoothstep(10.0,14.0,length(relative));
 float inside=step(.005,coord.x)*step(coord.x,.995)*step(.005,coord.y)*step(coord.y,.995)*step(0.0,coord.z)*step(coord.z,1.0);
 float s=(occluded(coord.xy+vec2(-.5,-.5)*shadowTexel)+occluded(coord.xy+vec2(.5,-.5)*shadowTexel)+occluded(coord.xy+vec2(-.5,.5)*shadowTexel)+occluded(coord.xy+vec2(.5,.5)*shadowTexel))*.25;
 gl_FragColor=vec4(vec3(1.0-strength*s*fade*inside),1.0);
}
