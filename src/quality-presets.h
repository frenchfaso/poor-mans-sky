// SPDX-License-Identifier: MPL-2.0
/* Runtime policy only: switching presets never changes procedural asset IDs. */
typedef struct {
  const char *name,*label;
  float renderScale,terrainDensity,patchScale,meshBudget;
  float walkBubble,flyBubble,walkDetail,flyDetail,streamMargin;
  float natureDistance,natureLodScale,grassNear,grassFar;
  int bloomSize,postMode,reflectionEvery,reflectionSize,shadowSize,queryBudget,cloudLimit;
  float cloudArea;
} QualityPreset;
/* postMode: 0=dither/vignette+bloom, 1=bloom composite, 2=one-tap upscale.
 * Detail ranges limit refinement, never the coarse planet cover/space view. */
static const QualityPreset qualityPresets[3]={
  {"low","PERFORMANCE",.75f,.55f,2,.55f,80,240,8000,24000,8,700,.55f,18,30,128,2,0,64,256,12,48,1.2f},
  {"medium","BALANCED",1,.8f,1.25f,.8f,100,300,16000,48000,10,1400,.8f,25,42,128,1,1,64,512,18,72,1.8f},
  {"high","QUALITY",1,1,1,1,140,420,32000,96000,12,2500,1,30,50,256,0,1,128,512,24,96,2.5f}
};
static int qualityPreset=1;
static const QualityPreset *quality(void){return &qualityPresets[qualityPreset];}
