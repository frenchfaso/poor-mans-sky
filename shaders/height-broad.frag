#version 120
// SPDX-License-Identifier: MPL-2.0
varying vec2 uv;
uniform sampler2D noiseTex;
vec2 smoothNoise(vec2 p){
 vec2 cell=p*256.0-.5;
 vec2 f=fract(cell);f=f*f*(3.0-2.0*f);
 vec2 q=(floor(cell)+.5)/256.0;
 vec2 a=texture2D(noiseTex,q).rg;
 vec2 b=texture2D(noiseTex,q+vec2(1.0/256.0,0.0)).rg;
 vec2 c=texture2D(noiseTex,q+vec2(0.0,1.0/256.0)).rg;
 vec2 d=texture2D(noiseTex,q+vec2(1.0/256.0,1.0/256.0)).rg;
 return mix(mix(a,b,f.x),mix(c,d,f.x),f.y);
}
void main(){gl_FragColor=vec4(smoothNoise(uv*.023+vec2(.31,.17)).r,smoothNoise(uv*.057+vec2(.71,.33)).g,0.0,1.0);}
