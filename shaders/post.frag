#version 120
// SPDX-License-Identifier: MPL-2.0
uniform sampler2D sceneTex,bloomTex;
uniform vec2 scale,halfTexel;
uniform float bloom;
varying vec2 uv;
void main(){
 // Scene is perceptually encoded before RGBA8 quantization, including night.
 vec3 c=texture2D(sceneTex,clamp(uv*scale,halfTexel,scale-halfTexel)).rgb;
 c+=texture2D(bloomTex,uv).rgb*bloom*.09;
 vec2 d=uv-.5;c*=1.0-dot(d,d)*.16;
 float noise=fract(dot(gl_FragCoord.xy,vec2(.75487766,.56984029)))-.5;
 gl_FragColor=vec4(clamp(c+noise/255.0,0.0,1.0),1.0);
}
