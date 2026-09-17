#version 120
// SPDX-License-Identifier: MPL-2.0
uniform sampler2D depthTex,shadowTex;
uniform vec3 forward,right,up,eyeDelta,lightRight,lightUp,lightDir;
uniform vec2 lens;
uniform float nearPlane,strength;
varying vec2 uv;
float occluded(vec2 sampleUV,float receiver) {
 return step(dot(texture2D(shadowTex,sampleUV).rg,vec2(1.0,1.0/255.0))+.0008,receiver);
}
void main() {
 vec4 depthSample=texture2D(depthTex,uv.yx);if(depthSample.a<.5)discard;
 float inv=dot(depthSample.rgb,vec3(16711680.0,65280.0,255.0))/16777215.0;
 float z=nearPlane/max(inv,1.0/16777215.0);
 vec3 relative=eyeDelta+(forward+right*((uv.x*2.0-1.0)*lens.x)+up*((uv.y*2.0-1.0)*lens.y))*z;
 vec3 coord=vec3(dot(relative,lightRight)/32.0+.5,dot(relative,lightUp)/32.0+.5,.5-dot(relative,lightDir)/256.0);
 float fade=1.0-smoothstep(10.0,14.0,length(relative));
 float inside=step(.005,coord.x)*step(coord.x,.995)*step(.005,coord.y)*step(coord.y,.995)*step(0.0,coord.z)*step(coord.z,1.0);
 float s=(occluded(coord.xy+vec2(-.5,-.5)/512.0,coord.z)+occluded(coord.xy+vec2(.5,-.5)/512.0,coord.z)+occluded(coord.xy+vec2(-.5,.5)/512.0,coord.z)+occluded(coord.xy+vec2(.5,.5)/512.0,coord.z))*.25;
 gl_FragColor=vec4(vec3(1.0-strength*s*fade*inside),1.0);
}
