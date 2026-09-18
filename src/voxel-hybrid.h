// SPDX-License-Identifier: MPL-2.0
/* Persistent heightfield curtains. CPU uploads terrain samples on residency
 * changes; the vertex shader projects grid lines and extends them to screen
 * bottom. Hardware depth replaces the CPU column Y-buffer. There is no surface
 * mesh underneath, per-pixel search, readback, or per-frame vertex upload. */
#define HYBRID_CACHE_LIMIT (8u*1048576u)
#define HYBRID_VERTS (VOXEL_GRID*2)
typedef struct { int16_t p[3],bottom; PackedNormal n; unsigned char color[4]; } HybridVertex;
typedef struct { GLuint vbo; int id,stamp; V3 scale; } HybridBlock;
typedef struct { int id,lod,axis,reverse; float distance; } HybridDraw;
static HybridBlock hybridBlocks[SLOTS];
static HybridDraw hybridList[1024];
static GLuint hybridIndices[4][2][2],hybridP,hybridShadowP;
static int hybridIndexCount[4],hybridCount,hybridBuilds,hybridReuses,hybridEvictions;
static size_t hybridIndexBytes;
static double hybridBuildMS;
static void hybridInvalidateSlot(int slot) {
  HybridBlock *b=&hybridBlocks[slot];
  if(b->vbo){glDeleteBuffers(1,&b->vbo);hybridGPUBytes-=HYBRID_VERTS*sizeof(HybridVertex);}
  memset(b,0,sizeof(*b));
}
static int hybridEvict(void) {
  int oldest=-1;
  for(int i=0;i<SLOTS;i++)if(hybridBlocks[i].vbo &&
      (oldest<0 || hybridBlocks[i].stamp<hybridBlocks[oldest].stamp))oldest=i;
  if(oldest<0)return 0;
  hybridInvalidateSlot(oldest);hybridEvictions++;return 1;
}
static int hybridLine(unsigned short *out,int at,int a,int b) {
  out[at++]=2*a;out[at++]=2*a+1;out[at++]=2*b+1;out[at++]=2*b;
  return at;
}
/* A dominant set of rows plus the other two perimeter edges. Every patch
 * boundary is included. Reverse rows for near-to-far submission; changing view
 * or sampling stride only selects immutable indices, never rebuilds vertices. */
static int hybridMakeIndices(unsigned short *out,int stride,int axis,int reverse) {
  int at=0,side=PATCH/stride;
  for(int row=0;row<=side;row++)for(int col=0;col<side;col++) {
    int y=(reverse?side-row:row)*stride,x=col*stride;
    int a=axis?x*(PATCH+1)+y:y*(PATCH+1)+x;
    int b=a+(axis?stride*(PATCH+1):stride);
    at=hybridLine(out,at,a,b);
  }
  for(int edge=0;edge<=PATCH;edge+=PATCH)for(int i=0;i<PATCH;i+=stride) {
    int a=axis?edge*(PATCH+1)+i:i*(PATCH+1)+edge;
    at=hybridLine(out,at,a,a+(axis?stride:stride*(PATCH+1)));
  }
  return at;
}
static void hybridInit(void) {
  if(hybridP)return;
  hybridP=program("voxel-hybrid.vert","voxel-hybrid.frag");
  hybridShadowP=program("voxel-hybrid.vert","voxel-hybrid-shadow.frag");
  unsigned short indices[4*PATCH*(PATCH+3)];
  size_t bytes=0;
  for(int lod=0;lod<4;lod++) {int side=PATCH/(1<<lod);bytes+=4u*4*side*(side+3)*sizeof(unsigned short);}
  if(!vramReserve(bytes))die("hybrid index VRAM budget");
  for(int lod=0;lod<4;lod++)for(int axis=0;axis<2;axis++)for(int reverse=0;reverse<2;reverse++) {
    int count=hybridMakeIndices(indices,1<<lod,axis,reverse);hybridIndexCount[lod]=count;
    GLuint *ibo=&hybridIndices[lod][axis][reverse];glGenBuffers(1,ibo);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,*ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,count*sizeof(unsigned short),indices,GL_STATIC_DRAW);
  }
  hybridIndexBytes=bytes;hybridGPUBytes+=bytes;
}
static HybridBlock *hybridBlock(Node *n) {
  HybridBlock *b=&hybridBlocks[n->slot];
  if(b->vbo && b->id==(int)(n-nodes)){b->stamp=frameNo;hybridReuses++;return b;}
  hybridInvalidateSlot(n->slot);
  size_t bytes=HYBRID_VERTS*sizeof(HybridVertex);
  while(hybridGPUBytes+bytes>HYBRID_CACHE_LIMIT && hybridEvict()){}
  while(vramEstimate()+bytes>VRAM_BUDGET && hybridEvict()){}
  if(!vramReserve(bytes))die("hybrid sample VRAM budget");
  Uint64 start=SDL_GetPerformanceCounter();HybridVertex vertices[HYBRID_VERTS];
  SDL_LockMutex(mutex);
  /* Signed-short local positions give sub-millimetre near-field precision and
   * reduce fetch/cache traffic by one third. Scale is immutable with the block. */
  V3 extent=v3(.001f,.001f,.001f);
  for(int i=0;i<VOXEL_GRID;i++) {
    V3 p=n->vertices[i].p;extent=v3(fmaxf(extent.x,fabsf(p.x)),fmaxf(extent.y,fabsf(p.y)),fmaxf(extent.z,fabsf(p.z)));
  }
  b->scale=mul(extent,1/32767.f);
  memset(voxelBlocks,0,sizeof(voxelBlocks));int compact=voxelCompact;voxelCompact=0;
  for(int y=0;y<=PATCH;y++)for(int x=0;x<=PATCH;x++) {
    int i=y*(PATCH+1)+x;const Vertex *v=&n->vertices[i];
    HybridVertex a={{(int16_t)roundf(v->p.x/b->scale.x),(int16_t)roundf(v->p.y/b->scale.y),(int16_t)roundf(v->p.z/b->scale.z)},0,v->n,{0}};voxelColor(n,(float)x/PATCH,(float)y/PATCH,a.color);
    vertices[2*i]=a;a.bottom=1;vertices[2*i+1]=a;
  }
  voxelCompact=compact;SDL_UnlockMutex(mutex);
  glGenBuffers(1,&b->vbo);glBindBuffer(GL_ARRAY_BUFFER,b->vbo);
  glBufferData(GL_ARRAY_BUFFER,bytes,vertices,GL_STATIC_DRAW);
  b->id=(int)(n-nodes);b->stamp=frameNo;hybridGPUBytes+=bytes;hybridBuilds++;
  hybridBuildMS+=(SDL_GetPerformanceCounter()-start)*1000.0/SDL_GetPerformanceFrequency();return b;
}
static int hybridNearFirst(const void *a,const void *b) {
  float x=((const HybridDraw*)a)->distance,y=((const HybridDraw*)b)->distance;
  return x<y?-1:x>y;
}
static int hybridVisible(const Node *n) {
  V3 delta=add(n->boundCenter,mul(cameraEye,-1));
  /* Tops above the upper frustum plane can still cover the screen below. */
  for(int i=0;i<6;i++)if(i!=3) {
    V3 p=frustumPlanes[i];float support=fabsf(p.x)*n->boundHalf.x+fabsf(p.y)*n->boundHalf.y+fabsf(p.z)*n->boundHalf.z;
    if(dot(p,delta)+frustumOffsets[i]+support<0)return 0;
  }
  return !behindPlanet(n->boundCenter,n->boundRadius);
}
static void hybridPrepare(void) {
  hybridCount=0;
  for(int i=0;i<selectedCount;i++) {
    Node *n=&nodes[selected[i]];
    if(n->slot<0 || !n->vertices || !n->pixels || !hybridVisible(n))continue;
    V3 delta=add(n->boundCenter,mul(cameraEye,-1));
    float distance=sqrtf(dot(delta,delta)),near=fmaxf(clipNear,distance-n->boundRadius);
    float spacing=n->size/PATCH*rh/(2*tanf(PI/6)*near);
    int lod=0;while(lod<3 && spacing*(2<<lod)<2)lod++;
    V3 a=add(n->vertices[PATCH].p,mul(n->vertices[0].p,-1));
    V3 b=add(n->vertices[PATCH*(PATCH+1)].p,mul(n->vertices[0].p,-1));
    int axis=fabsf(dot(b,viewRight))>fabsf(dot(a,viewRight));
    int reverse=dot(axis?a:b,viewForward)<0;
    hybridList[hybridCount++]=(HybridDraw){selected[i],lod,axis,reverse,near};
  }
  qsort(hybridList,hybridCount,sizeof(*hybridList),hybridNearFirst);
}
static void hybridUniforms(GLuint p) {
  glUseProgram(p);u3(p,"screenUp",viewUp);u1(p,"tanHalfFov",tanf(PI/6));
}
static void hybridGeometry(GLuint p,const HybridDraw *d) {
  Node *n=&nodes[d->id];HybridBlock *b=hybridBlock(n);
  u3(p,"patchDelta",add(n->center,mul(cameraEye,-1)));u3(p,"positionScale",b->scale);
  glBindBuffer(GL_ARRAY_BUFFER,b->vbo);
  glVertexPointer(4,GL_SHORT,sizeof(HybridVertex),(void*)offsetof(HybridVertex,p));
  glNormalPointer(GL_BYTE,sizeof(HybridVertex),(void*)offsetof(HybridVertex,n));
  glColorPointer(4,GL_UNSIGNED_BYTE,sizeof(HybridVertex),(void*)offsetof(HybridVertex,color));
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,hybridIndices[d->lod][d->axis][d->reverse]);
  glDrawElements(GL_QUADS,hybridIndexCount[d->lod],GL_UNSIGNED_SHORT,0);
}
static void hybridDraw(void) {
  hybridInit();hybridPrepare();GLuint p=hybridP;hybridUniforms(p);
  tex(p,"detailTex",0,detailMap.tex);
  u3(p,"sun",sun);u3(p,"ambientLight",ambientLight);u3(p,"sunLight",sunLight);u3(p,"fogColor",fogColor);u3(p,"ambientUp",norm(cameraEye));
  float period=100.f/17;u3(p,"eyePhase",v3(fmodf(cameraEye.x,period),fmodf(cameraEye.y,period),fmodf(cameraEye.z,period)));
  u1(p,"exposure",sceneExposure);u1(p,"fastMode",performanceMode);
  u1(p,"fogHeightFactor",exp2f(-fmaxf(sqrtf(dot(cameraEye,cameraEye))-RADIUS,0)/13000));
  glDisable(GL_CULL_FACE);glDepthFunc(GL_LESS);glEnableClientState(GL_COLOR_ARRAY);glDisableClientState(GL_TEXTURE_COORD_ARRAY);
  int quads=0;
  for(int i=0;i<hybridCount;i++){hybridGeometry(p,&hybridList[i]);quads+=hybridIndexCount[hybridList[i].lod]/4;drawn++;}
  triangles+=quads*2;
  glDisableClientState(GL_COLOR_ARRAY);glEnableClientState(GL_TEXTURE_COORD_ARRAY);glEnable(GL_CULL_FACE);
  if(frameNo%120==119)printf("HYBRID frame=%d blocks=%d quads=%d builds=%d reuse=%d evictions=%d build_total_ms=%.3f gpu_bytes=%zu cpu_casts=%llu\n",frameNo,hybridCount,quads,hybridBuilds,hybridReuses,hybridEvictions,hybridBuildMS,hybridGPUBytes,voxelRasterBuilds);
  checkGL("hybrid curtains");
}
static void hybridShadow(void) {
  sunReceiverDraws=0;if(!sunReady || wire)return;
  Uint64 start=SDL_GetPerformanceCounter();GLuint p=hybridShadowP;hybridUniforms(p);
  tex(p,"shadowTex",0,sunMap.tex);u3(p,"eyeDelta",add(cameraEye,mul(sunAnchor,-1)));
  u3(p,"lightRight",sunRight);u3(p,"lightUp",sunUp);u3(p,"lightDir",sunDirection);
  u1(p,"strength",.38f*clampf((dot(norm(cameraEye),sun)-.12f)/.18f,0,1));
  glDisable(GL_CULL_FACE);glDepthFunc(GL_EQUAL);glDepthMask(GL_FALSE);
  glEnable(GL_BLEND);glBlendFunc(GL_ZERO,GL_SRC_COLOR);
  glEnableClientState(GL_COLOR_ARRAY);glDisableClientState(GL_TEXTURE_COORD_ARRAY);
  /* Drawing the same curtains preserves their exact hardware depth. The shadow
   * is applied before water/actors, with the same caster stipple as the base. */
  for(int i=0;i<hybridCount;i++) {
    Node *n=&nodes[hybridList[i].id];V3 d=add(n->boundCenter,mul(sunAnchor,-1));
    /* Extrusion can lower a distant top into the receiver volume; only reject
     * by horizontal distance, independent of height. */
    V3 radial=norm(cameraEye);d=add(d,mul(radial,-dot(d,radial)));
    if(dot(d,d)>(n->boundRadius+16)*(n->boundRadius+16))continue;
    hybridGeometry(p,&hybridList[i]);sunReceiverDraws++;
  }
  glDisableClientState(GL_COLOR_ARRAY);glEnableClientState(GL_TEXTURE_COORD_ARRAY);
  glDisable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
  glDepthMask(GL_TRUE);glDepthFunc(GL_LEQUAL);glEnable(GL_CULL_FACE);
  sunShadowMS+=(SDL_GetPerformanceCounter()-start)*1000.0/SDL_GetPerformanceFrequency();checkGL("hybrid shadow receiver");
}
static void hybridClose(void) {
  for(int i=0;i<SLOTS;i++)hybridInvalidateSlot(i);
  glDeleteBuffers(16,&hybridIndices[0][0][0]);memset(hybridIndices,0,sizeof(hybridIndices));
  hybridGPUBytes-=hybridIndexBytes;hybridIndexBytes=0;
  glDeleteProgram(hybridP);glDeleteProgram(hybridShadowP);hybridP=hybridShadowP=0;
}
