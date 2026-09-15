// SPDX-License-Identifier: MPL-2.0
/* Select the extra scattering shader only when a ray towards the lunar disk
 * can cross the planet atmosphere. The finite ray ends at the moon surface. */
static int moonAtmosphereNeeded(void) {
 V3 delta=add(moonCenter(),mul(cameraEye,-1));
 float t=clampf(-dot(cameraEye,delta)/fmaxf(dot(delta,delta),1),0,1);
 V3 closest=add(cameraEye,mul(delta,t));
 float radius=RADIUS+60000+MOON_RADIUS;
 return dot(closest,closest)<radius*radius;
}
static void moonAtmosphereUniforms(GLuint p) {
 float altitude=sqrtf(dot(cameraEye,cameraEye))-RADIUS,solar=dot(sun,norm(cameraEye));
 float air=exp2f(-fmaxf(altitude,0)/9000);
 u3(p,"observer",cameraEye);u3(p,"skyRadial",norm(cameraEye));u3(p,"skySun",sun);
 u3(p,"zenithColor",mul(v3(.026,.072,.16),solarDaylight*air));u3(p,"horizonColor",fogColor);
 u3(p,"sunsetColor",mul(v3(.24,.055,.009),clampf(1-fabsf(solar)*6,0,1)*air));
 u1(p,"horizonDip",sqrtf(fmaxf(0,1-powf(RADIUS/(RADIUS+fmaxf(altitude,0)),2))));
 u1(p,"altitude",altitude);u1(p,"planetRadius",RADIUS);u1(p,"exposure",sceneExposure);
}
