// SPDX-License-Identifier: MPL-2.0
/* CPU Hi-Z from the SAME FRAME's caster depth; no GPU readback. In inverse
 * depth the conservative reduction is MIN, and uncovered sky is zero. */
#define VOXEL_HIZ_LEVELS 16
static uint32_t *voxelHiZData[VOXEL_HIZ_LEVELS];
static int voxelHiZW[VOXEL_HIZ_LEVELS],voxelHiZH[VOXEL_HIZ_LEVELS];
static int voxelHiZLevels,voxelHiZFrame=-1,voxelHiZSourceW,voxelHiZSourceH;
static size_t voxelHiZBytes;
static GLuint voxelHiZQuery;
static unsigned long long voxelHiZTests,voxelHiZCulled,voxelHiZVerified;
static double voxelHiZBuildMS;
static int voxelHiZBuilds,voxelHiZQuietFrames;
static int voxelHiZMisses,voxelHiZQuietUntil;
static unsigned long long voxelHiZLastCulled;
static V3 voxelHiZLastEye,voxelHiZLastForward,voxelHiZLastRight;
static void voxelHiZFree(void) {
  for(int l=0;l<voxelHiZLevels;l++){free(voxelHiZData[l]);voxelHiZData[l]=NULL;}
  voxelHiZLevels=0;voxelHiZBytes=0;voxelHiZFrame=-1;
}
static void voxelHiZBuild(void) {
  voxelHiZFrame=-1;if(!voxelHiZ)return;
  if(!voxelHiZCheck) {
    V3 delta=add(cameraEye,mul(voxelHiZLastEye,-1));
    int moved=dot(delta,delta)>.0001f || dot(viewForward,voxelHiZLastForward)<.999999f || dot(viewRight,voxelHiZLastRight)<.999999f;
    if(moved || voxelHiZCulled!=voxelHiZLastCulled) {voxelHiZMisses=0;voxelHiZQuietUntil=0;}
    else if(voxelHiZMisses>=30) {voxelHiZQuietUntil=frameNo+120;voxelHiZMisses=0;}
    voxelHiZLastCulled=voxelHiZCulled;voxelHiZLastEye=cameraEye;
    voxelHiZLastForward=viewForward;voxelHiZLastRight=viewRight;
    if(frameNo<voxelHiZQuietUntil) {voxelHiZQuietFrames++;return;}
    voxelHiZMisses++;
  }
  Uint64 start=SDL_GetPerformanceCounter();voxelHiZBuilds++;
  if(voxelHiZSourceW!=voxelWidth || voxelHiZSourceH!=voxelHeight || !voxelHiZLevels) {
    voxelHiZFree();int w=(voxelWidth+7)/8,h=(voxelHeight+7)/8;
    for(int l=0;l<VOXEL_HIZ_LEVELS;l++) {
      voxelHiZW[l]=w;voxelHiZH[l]=h;size_t bytes=(size_t)w*h*sizeof(uint32_t);
      voxelHiZData[l]=malloc(bytes);if(!voxelHiZData[l])die("voxel Hi-Z allocation");
      voxelHiZBytes+=bytes;voxelHiZLevels++;
      if(w==1 && h==1)break;
      w=(w+1)/2;h=(h+1)/2;
    }
    voxelHiZSourceW=voxelWidth;voxelHiZSourceH=voxelHeight;
  }
  for(int by=0;by<voxelHiZH[0];by++)for(int bx=0;bx<voxelHiZW[0];bx++) {
    uint32_t farthest=0xffffffu;
    for(int x=bx*8;x<bx*8+8;x++)for(int y=by*8;y<by*8+8;y++) {
      uint32_t depth=0;
      if(x<voxelWidth && y<voxelHeight) {
        const unsigned char *p=voxelBuffers[2]+((size_t)x*voxelHeight+y)*4;
        if(p[3])depth=(uint32_t)p[0]<<16|(uint32_t)p[1]<<8|p[2];
      }
      if(depth<farthest)farthest=depth;
    }
    voxelHiZData[0][by*voxelHiZW[0]+bx]=farthest;
  }
  for(int l=1;l<voxelHiZLevels;l++)for(int y=0;y<voxelHiZH[l];y++)for(int x=0;x<voxelHiZW[l];x++) {
    uint32_t farthest=0xffffffu;
    for(int j=0;j<2;j++)for(int i=0;i<2;i++) {
      int sx=x*2+i,sy=y*2+j;
      uint32_t d=sx<voxelHiZW[l-1] && sy<voxelHiZH[l-1]?voxelHiZData[l-1][sy*voxelHiZW[l-1]+sx]:0;
      if(d<farthest)farthest=d;
    }
    voxelHiZData[l][y*voxelHiZW[l]+x]=farthest;
  }
  voxelHiZFrame=frameNo;
  voxelHiZBuildMS+=(SDL_GetPerformanceCounter()-start)*1000.0/SDL_GetPerformanceFrequency();
}
static int voxelHiZRect(int x0,int y0,int x1,int y1,uint32_t closest) {
  if(x0<0 || y0<0 || x1>=voxelWidth || y1>=voxelHeight || x1<x0 || y1<y0)return 0;
  int level=0,shift=3;
  while(level+1<voxelHiZLevels && ((x1>>shift)-(x0>>shift)>1 || (y1>>shift)-(y0>>shift)>1)) {level++;shift++;}
  for(int y=y0>>shift;y<=y1>>shift;y++)for(int x=x0>>shift;x<=x1>>shift;x++)
    if(voxelHiZData[level][y*voxelHiZW[level]+x]<=closest)return 0;
  return 1;
}
/* Debug-only GPU audit of rejected boxes. It stalls deliberately and must
 * never be enabled in timings. No color/depth writes and no production readback. */
static void voxelHiZAudit(V3 center,V3 half) {
  if(!voxelHiZQuery)glGenQueries(1,&voxelHiZQuery);
  GLint saved;glGetIntegerv(GL_CURRENT_PROGRAM,&saved);glUseProgram(0);
  glColorMask(GL_FALSE,GL_FALSE,GL_FALSE,GL_FALSE);glDepthMask(GL_FALSE);
  static const unsigned char face[24]={0,1,3,2,4,6,7,5,0,4,5,1,2,3,7,6,0,2,6,4,1,5,7,3};
  V3 d=add(center,mul(cameraEye,-1));
  glBeginQuery(GL_SAMPLES_PASSED,voxelHiZQuery);glBegin(GL_QUADS);
  for(int k=0;k<24;k++) {int v=face[k];glVertex3f(d.x+(v&1?half.x:-half.x),d.y+(v&2?half.y:-half.y),d.z+(v&4?half.z:-half.z));}
  glEnd();glEndQuery(GL_SAMPLES_PASSED);
  GLuint samples;glGetQueryObjectuiv(voxelHiZQuery,GL_QUERY_RESULT,&samples);
  glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glDepthMask(GL_TRUE);glUseProgram(saved);
  if(samples)die("voxel Hi-Z rejected GPU-visible bounding box");voxelHiZVerified++;
}
static int voxelHiddenBox(V3 center,V3 half) {
  if(!voxelScene || voxelMix<1 || !voxelHiZ || voxelHiZFrame!=frameNo || overdrawView || wire)return 0;
  voxelHiZTests++;
  V3 d=add(center,mul(cameraEye,-1));half=add(half,v3(.25f,.25f,.25f));
  float x0=1e30f,y0=1e30f,x1=-1e30f,y1=-1e30f,nearest=1e30f;
  float tx=tanf(PI/6)*width/height,ty=tanf(PI/6);
  for(int k=0;k<8;k++) {
    V3 p=add(d,v3(k&1?half.x:-half.x,k&2?half.y:-half.y,k&4?half.z:-half.z));
    float z=dot(p,viewForward);if(z<=clipNear+.25f)return 0;
    float x=(dot(p,viewRight)/(z*tx)+1)*.5f*voxelWidth;
    float y=(dot(p,viewUp)/(z*ty)+1)*.5f*voxelHeight;
    x0=fminf(x0,x);x1=fmaxf(x1,x);y0=fminf(y0,y);y1=fmaxf(y1,y);nearest=fminf(nearest,z);
  }
  /* Clip/round conservatively. Near-plane or partially off-screen bounds
   * stay visible. Bias covers packed depth and hardware depth quantization. */
  if(x0<1 || y0<1 || x1>=voxelWidth-1 || y1>=voxelHeight-1)return 0;
  uint32_t closest=(uint32_t)ceilf(clipNear/(nearest-.25f)*16777215.f)+16;
  if(!voxelHiZRect((int)floorf(x0)-1,(int)floorf(y0)-1,(int)ceilf(x1)+1,(int)ceilf(y1)+1,closest))return 0;
  if(voxelHiZCheck)voxelHiZAudit(center,half);
  voxelHiZCulled++;return 1;
}
