#version 120
// SPDX-License-Identifier: MPL-2.0
varying vec2 uv;
uniform sampler2D normalTex;
uniform sampler2D materialTex;
uniform vec3 sun;
void main(){
 vec3 n=normalize(texture2D(normalTex,uv).rgb*2.0-1.0);
 vec4 m=texture2D(materialTex,uv);
 vec3 light=vec3(.30,.39,.49)+vec3(1.38,1.04,.71)*max(dot(n,sun),0.0)*m.a;
 gl_FragColor=vec4(m.rgb*light,smoothstep(.5,.85,n.y));
}
