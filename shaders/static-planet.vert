#version 120
// SPDX-License-Identifier: MPL-2.0
uniform vec3 eye,sun,sunLight,ambientLight,ambientUp;
uniform float fogHeightFactor;
uniform vec4 waterPlane;
varying vec3 lit;
varying float fog,waterHeight;
void main(){
 vec3 delta=gl_Vertex.xyz-eye,n=normalize(gl_Normal);
 vec3 ambient=mix(ambientLight*vec3(.60,.48,.32),ambientLight,.35+.65*max(dot(n,ambientUp),0.0));
 lit=gl_Color.rgb*(ambient+sunLight*max(dot(n,sun),0.0));
 fog=(1.0-exp2(-length(delta)*.00009))*fogHeightFactor;
 waterHeight=dot(gl_Vertex.xyz,waterPlane.xyz)-waterPlane.w;
 gl_Position=gl_ModelViewProjectionMatrix*vec4(delta,1.0);
}
