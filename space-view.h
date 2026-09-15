// SPDX-License-Identifier: MPL-2.0
/* Near clipping must be safe for BOTH bodies, independently of control mode.
 * The envelopes include relief; using only the active body's altitude cuts
 * a circular hole in the other body during interplanetary approaches. */
static float worldNearPlane(V3 camera,int flight) {
 if(!flight)return .75f;
 V3 lunar=add(camera,mul(moonCenter(),-1));
 float planetGap=sqrtf(dot(camera,camera))-RADIUS-20000;
 float moonGap=sqrtf(dot(lunar,lunar))-MOON_RADIUS-12000;
 return fmaxf(2,fminf(planetGap,moonGap)*.1f);
}
