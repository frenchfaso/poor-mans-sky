#version 120
// SPDX-License-Identifier: MPL-2.0
varying vec2 uv;
uniform sampler2D noiseTex;
void main(){
 float y=uv.y;
 float grain=texture2D(noiseTex,uv*.21).r;
 float fine=texture2D(noiseTex,uv*.7).g;
 float branch=fract(y*19.0+grain*.20);
 float width=(1.0-y)*(.30+branch*.25)*(grain*.32+.84);
 float x=abs(uv.x-.5);
 float needles=(1.0-smoothstep(width-.018,width,x))*step(.13,y)*step(y,.985);
 needles*=step(.24,fine+grain*.25);
 float trunk=(1.0-step(.009*(1.2-y),x))*step(y,.80);
 vec3 c=mix(vec3(.035,.060,.047),vec3(.17,.23,.135),grain*.65+fine*.35);
 c*=.55+y*.55;
 gl_FragColor=vec4(mix(c,vec3(.17,.14,.10),trunk*(1.0-needles)),max(needles,trunk));
}
