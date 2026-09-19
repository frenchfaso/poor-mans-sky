// SPDX-License-Identifier: MPL-2.0
/* Same bubble/cone/distance policy as the planet. Geometry metadata is cached
 * in MoonRAM; view-dependent math is evaluated once per node/view, not once
 * per budget, prefetch, draw and morph pass. Offline baking remains radial. */
static V3 moonViewEye,moonViewPlanes[6];
static unsigned moonViewRevision;
static int moonPlanEvaluations;
static void moonPrepareView(void) {
 moonPlanEvaluations=0;
 V3 localEye=moonLocalPoint(cameraEye),axes[3]={moonLocalVector(viewForward),moonLocalVector(viewRight),moonLocalVector(viewUp)};
 static V3 previousEye,previousAxes[3];static int previousWidth,previousHeight,previousPreset=-1,previousFlying=-1,previousDirectional=-1;
 if(!moonViewRevision || memcmp(&localEye,&previousEye,sizeof(V3)) || memcmp(axes,previousAxes,sizeof(axes)) ||
    width!=previousWidth || height!=previousHeight || qualityPreset!=previousPreset || flying!=previousFlying || directionalStreaming!=previousDirectional) {
  moonViewRevision++;previousEye=localEye;memcpy(previousAxes,axes,sizeof(axes));
  previousWidth=width;previousHeight=height;previousPreset=qualityPreset;previousFlying=flying;previousDirectional=directionalStreaming;
 }
 moonViewEye=localEye;
 visibilitySetup();
 for(int i=0;i<6;i++)moonViewPlanes[i]=moonLocalVector(frustumPlanes[i]);
}
static MoonRAM *moonPlanRecord(int face,int level,int x,int y) {
 SDL_LockMutex(mutex);MoonRAM *r=moonRecord(face,level,x,y,1);SDL_UnlockMutex(mutex);
 if(!r)return NULL;
 if(r->viewRevision!=moonViewRevision) {
  moonPlanEvaluations++;
  V3 delta=add(r->planCenter,mul(moonViewEye,-1));
  r->distance2=dot(delta,delta);r->distance=sqrtf(r->distance2);
  r->band=streamViewReady?streamBandDelta(moonVector(delta),r->distance2,r->planRadius,r->band):0;
  r->visible=1;
  for(int i=0;i<4;i++)if(dot(delta,moonViewPlanes[i])+r->planRadius<0){r->visible=0;break;}
  r->viewRevision=moonViewRevision;
 }
 return r;
}
static float moonSplitThreshold(const MoonRAM *r) {
 static const float scales[6]={1,1,.85f,.7f,.6f,.12f};
 return r->planSize*1.6f*moonLodScale*quality()->terrainDensity*scales[r->band];
}
static int moonDirectionalNode(int face,int level,int x,int y) {
 MoonRAM *r=moonPlanRecord(face,level,x,y);if(!r)return 0;
 if(dot(r->planRadial,moonViewEye)<MOON_RADIUS-2500-r->planRadius)return 1;
 if(!moonPlanning && !moonRequestOnly && r->band==5 && !r->visible)return 1;
 if(moonRequestOnly){SDL_LockMutex(mutex);moonRequest(face,level,x,y);SDL_UnlockMutex(mutex);}
 static const float floors[6]={8,16,64,256,1024,2048};
 float limit=moonSplitThreshold(r)*(r->split?1.1f:1);
 int split=level<13 && r->planSize>floors[r->band]*quality()->patchScale && r->distance2<limit*limit;
 if(moonRequestOnly)r->split=split;
 if(split) {
  if(r->orderRevision!=moonViewRevision) {
   float score[4],span=2.f/(1<<level);
   for(int k=0;k<4;k++) {
    r->order[k]=k;V3 d=direction(face,-1+(x*2+(k&1)+.5f)*span*.5f,-1+(y*2+(k>>1)+.5f)*span*.5f);
    score[k]=-dot(d,moonViewEye);
   }
   for(int i=1;i<4;i++){int k=r->order[i],j=i;while(j && score[r->order[j-1]]>score[k]){r->order[j]=r->order[j-1];j--;}r->order[j]=k;}
   r->orderRevision=moonViewRevision;
  }
  int before=moonSelectedCount,complete=1;
  for(int i=0;i<4;i++){int k=r->order[i];if(!moonDirectionalNode(face,level+1,x*2+(k&1),y*2+(k>>1)))complete=0;}
  if(complete || moonPlanning || moonRequestOnly)return 1;
  moonSelectedCount=before;
 }
 if(moonPlanning){moonDraws++;return 1;}
 if(moonRequestOnly)return 1;
 MoonPatch *p=moonPatch(face,level,x,y);if(!p || moonSelectedCount==MOON_SLOTS)return 0;
 p->band=r->band;moonSelected[moonSelectedCount++]=p;return 1;
}
static int moonNode(int face,int level,int x,int y) {
 return moonBaking || !streamViewReady?moonLegacyNode(face,level,x,y):moonDirectionalNode(face,level,x,y);
}
static float moonMorphFactor(const MoonPatch *p) {
 if(!streamViewReady)return moonLegacyMorphFactor(p);
 if(!p->level)return 1;
 MoonRAM *r=moonPlanRecord(p->face,p->level-1,p->x/2,p->y/2);if(!r)return 1;
 float threshold=moonSplitThreshold(r)*1.1f;
 float t=clampf((threshold-r->distance)/fmaxf(1,threshold*.25f),0,1);
 return t*t*(3-2*t);
}
