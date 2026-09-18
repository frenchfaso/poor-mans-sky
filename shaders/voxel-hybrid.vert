#version 120
// SPDX-License-Identifier: MPL-2.0
uniform vec3 patchDelta,screenUp,positionScale;
uniform float tanHalfFov;
varying vec3 delta,normal,baseColor;
invariant gl_Position;
void main() {
 vec3 top=gl_Vertex.xyz*positionScale+patchDelta;
 vec4 projected=gl_ModelViewProjectionMatrix*vec4(top,1.0);
 float bottom=min(projected.y,-projected.w);
 float shift=(bottom-projected.y)*gl_Vertex.w;
 gl_Position=vec4(projected.x,projected.y+shift,projected.zw);
 // Reconstruct the position down the curtain without changing forward depth.
 delta=top+screenUp*(shift*tanHalfFov);
 normal=gl_Normal;baseColor=gl_Color.rgb;
}
