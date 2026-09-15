#version 120
// SPDX-License-Identifier: MPL-2.0
uniform vec3 sun,offset,ambientLight,sunLight,ambientUp;
varying vec3 color;
varying float fog,visibility;
varying vec2 uv;
void main(){
 vec3 p=gl_Vertex.xyz;
 float distance=length(p+offset);
 float grass=step(.13,gl_MultiTexCoord0.x)*step(gl_MultiTexCoord0.x,.25)*step(gl_MultiTexCoord0.y,.25);
 visibility=1.0-grass*smoothstep(30.0,50.0,distance);
 vec3 ambient=mix(ambientLight*vec3(.60,.48,.32),ambientLight,.35+.65*max(dot(gl_Normal,ambientUp),0.0));
 color=gl_Color.rgb*(ambient+sunLight*max(dot(gl_Normal,sun),0.0));
 fog=1.0-exp2(-distance*.00009);
 uv=gl_MultiTexCoord0.xy;
 gl_Position=gl_ModelViewProjectionMatrix*vec4(p,1);
}
