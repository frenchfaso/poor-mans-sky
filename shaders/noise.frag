#version 120
// SPDX-License-Identifier: MPL-2.0
varying vec2 uv;
void main(){
 vec2 p=floor(uv*256.0);
 vec3 q=fract(vec3(p.x,p.y,p.x)*vec3(.1031,.1030,.0973));
 q+=dot(q,q.yxz+33.33);
 gl_FragColor=vec4(fract((q.xxy+q.yzz)*q.zyx),1.0);
}
