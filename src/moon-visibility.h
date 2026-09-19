// SPDX-License-Identifier: MPL-2.0
/* Same conservative async policy as planetary occlusion: results belong to a
 * complete camera/body/mesh snapshot. Moving or morphing views fail open. */
static unsigned moonOcclusionEpoch=1;
static uint32_t moonOcclusionSignature;
static int moonOcclusionStable,moonOcclusionActive,moonOcclusionCooldown;
static int moonOcclusionSamples,moonOcclusionSavings,moonHidden,moonQueryIssued,moonVisible;
static void moonVisibilityPrepare(void) {
 uint32_t h=2166136261u;
 h=cacheHash(&cameraEye,sizeof(cameraEye),h);h=cacheHash(&viewForward,sizeof(viewForward),h);
 h=cacheHash(&viewRight,sizeof(viewRight),h);h=cacheHash(&viewUp,sizeof(viewUp),h);
 h=cacheHash(&celestial.center,sizeof(V3)*4,h); /* Body pose, not solar shading. */
 h=cacheHash(&clipNear,sizeof(clipNear),h);h=cacheHash(&clipFar,sizeof(clipFar),h);
 h=cacheHash(&rw,sizeof(rw),h);h=cacheHash(&rh,sizeof(rh),h);
 h=cacheHash(&lodRevision,sizeof(lodRevision),h);h=cacheHash(&wire,sizeof(wire),h);
 h=cacheHash(&occlusionSignature,sizeof(occlusionSignature),h);
 moonHidden=moonQueryIssued=moonVisible=0;
 for(int i=0;i<moonSelectedCount;i++) {
  MoonPatch *p=moonSelected[i];
  p->worldCenter=moonWorldPoint(p->boundCenter);
  V3 b=p->boundHalf;
  p->worldHalf=v3(fabsf(celestial.x.x)*b.x+fabsf(celestial.y.x)*b.y+fabsf(celestial.z.x)*b.z,
    fabsf(celestial.x.y)*b.x+fabsf(celestial.y.y)*b.y+fabsf(celestial.z.y)*b.z,
    fabsf(celestial.x.z)*b.x+fabsf(celestial.y.z)*b.y+fabsf(celestial.z.z)*b.z);
  p->visible=boxInFrustum(p->worldCenter,p->worldHalf) && !behindPlanet(p->worldCenter,p->boundRadius);
  moonVisible+=p->visible;
  int data[]={p-moonPatches,p->face,p->level,p->x,p->y,p->visible};
  h=cacheHash(data,sizeof(data),h);h=cacheHash(&moonUploadedHash[p-moonPatches],sizeof(uint32_t),h);
 }
 if(h!=moonOcclusionSignature){moonOcclusionSignature=h;moonOcclusionEpoch++;moonOcclusionStable=0;}
 else moonOcclusionStable++;
 moonOcclusionActive=occlusionEnabled && !wire && nearMoon(cameraEye) && bodyAltitude(cameraEye)<8000 && moonVisible>=24 && frameNo>=moonOcclusionCooldown;
 if(!moonOcclusionActive)return;
 for(int i=0;i<moonSelectedCount;i++) {
  MoonPatch *p=moonSelected[i];
  if(p->queryPending) {
   GLint ready=0;glGetQueryObjectiv(p->query,GL_QUERY_RESULT_AVAILABLE,&ready);
   if(ready) {
    GLuint samples=1;glGetQueryObjectuiv(p->query,GL_QUERY_RESULT,&samples);p->queryPending=0;
    if(p->queryEpoch==moonOcclusionEpoch) {
     p->testedEpoch=moonOcclusionEpoch;p->hidden=samples==0;
     moonOcclusionSamples++;moonOcclusionSavings+=p->hidden;
    }
   }
  }
  if(p->visible && p->testedEpoch==moonOcclusionEpoch && p->hidden){p->visible=0;moonHidden++;}
 }
 if(moonOcclusionSamples>=96) {
  if(moonOcclusionSavings*20<moonOcclusionSamples)moonOcclusionCooldown=frameNo+240;
  moonOcclusionSamples=moonOcclusionSavings=0;
 }
}
static void moonVisibilityIssue(void) {
 if(!moonOcclusionActive || moonOcclusionStable<3)return;
 glUseProgram(occlusionP);glColorMask(GL_FALSE,GL_FALSE,GL_FALSE,GL_FALSE);glDepthMask(GL_FALSE);glDisable(GL_CULL_FACE);
 static const unsigned char faces[24]={0,1,3,2,4,6,7,5,0,4,5,1,2,3,7,6,0,2,6,4,1,5,7,3};
 for(int i=moonSelectedCount-1;i>=0 && moonQueryIssued<quality()->queryBudget;i--) {
  MoonPatch *p=moonSelected[i];
  if(!p->visible || p->queryPending || p->testedEpoch==moonOcclusionEpoch)continue;
  V3 d=add(p->worldCenter,mul(cameraEye,-1)),b=p->worldHalf;
  float z=dot(d,viewForward),support=fabsf(viewForward.x)*b.x+fabsf(viewForward.y)*b.y+fabsf(viewForward.z)*b.z;
  if(z-support<=clipNear || p->boundRadius*rh/fmaxf(1,z)<16)continue;
  if(!p->query)glGenQueries(1,&p->query);
  glBeginQuery(GL_SAMPLES_PASSED,p->query);glBegin(GL_QUADS);
  for(int k=0;k<24;k++){int c=faces[k];glVertex3f(d.x+(c&1?b.x:-b.x),d.y+(c&2?b.y:-b.y),d.z+(c&4?b.z:-b.z));}
  glEnd();glEndQuery(GL_SAMPLES_PASSED);
  p->queryPending=1;p->queryEpoch=moonOcclusionEpoch;moonQueryIssued++;
 }
 glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glDepthMask(GL_TRUE);glEnable(GL_CULL_FACE);
}
