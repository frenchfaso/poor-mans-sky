#version 120
// SPDX-License-Identifier: MPL-2.0
varying vec2 uv;
uniform vec3 forwardDir;
uniform vec3 rightDir;
uniform vec3 upDir;
uniform vec3 sun;
uniform vec2 lens;
uniform sampler2D noiseTex;
void main(){
 vec2 p=(uv*2.0-1.0)*lens;
 vec3 ray=normalize(forwardDir+rightDir*p.x+upDir*p.y);
 float h=max(ray.y,0.0);
 vec3 sky=mix(vec3(.64,.71,.74),vec3(.12,.29,.47),sqrt(h));
 float sd=max(dot(ray,sun),0.0);
 sky+=vec3(.43,.24,.09)*pow(sd,8.0);
 sky+=vec3(.40,.28,.12)*pow(sd,128.0);
 sky+=vec3(1.0,.82,.48)*smoothstep(.99965,.9999,sd);
 vec2 cuv=ray.xz/(max(ray.y,.04)+.13)*.011+vec2(.23,.41);
 float cloud=texture2D(noiseTex,cuv).r*.50+texture2D(noiseTex,cuv*2.7).g*.25+texture2D(noiseTex,cuv*6.3).b*.16+texture2D(noiseTex,cuv*15.0).r*.09;
 float opacity=smoothstep(.52,.73,cloud)*smoothstep(.02,.20,ray.y)*.65;
 sky=mix(sky,vec3(.88,.83,.73),opacity);
 gl_FragColor=vec4(sky,1.0);
}
