#version 120
// SPDX-License-Identifier: MPL-2.0
varying vec2 uv;
uniform sampler2D heightTex;
uniform sampler2D normalTex;
uniform sampler2D detailTex;
uniform sampler2D shadowTex;
void main(){
 float h=texture2D(heightTex,uv).r*512.0-96.0;
 vec4 n=texture2D(normalTex,uv);
 float slope=n.g*2.0-1.0;
 float variation=texture2D(detailTex,uv*7.0).r;
 float grass=smoothstep(.65,.92,slope)*smoothstep(1.0,12.0,h);
 vec3 rock=mix(vec3(.24,.245,.24),vec3(.43,.40,.34),variation);
 vec3 c=mix(rock,vec3(.18,.215,.095)*(.65+variation*.65),grass);
 float snow=smoothstep(195.0,247.0,h+variation*27.0)*smoothstep(.42,.85,slope);
 c=mix(c,vec3(.78,.82,.83),snow);
 float shore=1.0-smoothstep(0.0,4.0,h);
 c=mix(c,vec3(.19,.20,.17),shore*.6);
 c*=n.a;
 gl_FragColor=vec4(c,texture2D(shadowTex,uv).r);
}
