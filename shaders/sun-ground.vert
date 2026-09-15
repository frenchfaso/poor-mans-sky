#version 120
// SPDX-License-Identifier: MPL-2.0
uniform vec3 patchDelta,lightRight,lightUp,lightDir;
varying vec3 coord,relative;
void main(){
 relative=gl_Vertex.xyz+patchDelta;
 coord=vec3(dot(relative,lightRight)/32.0+.5,dot(relative,lightUp)/32.0+.5,.5-dot(relative,lightDir)/256.0);
 gl_Position=gl_ModelViewProjectionMatrix*gl_Vertex;
}
