// SPDX-License-Identifier: MPL-2.0
/* Experimental CPU heightfield column caster. Only the main planet surface is
 * replaced; auxiliary terrain passes and the Moon retain their original path.
 * Samples radial heights from the current streamed cover, never calls elevation.
 * Camera pitch must keep screen-up pointing into the outward radial hemisphere.
 */
static GLuint voxelTextures[3], voxelProgram;
static unsigned char *voxelBuffers[3];
static int voxelWidth, voxelHeight;
static unsigned char voxelCover[MAXNODE];
static double voxelCpuMS, voxelUploadMS;
static unsigned long long voxelSamples, voxelSpans, voxelLookupHits, voxelHorizonStops, voxelSkippedSamples;
static int voxelFrames;

static void voxelFaceUV(V3 p,int *face,float *u,float *v) {
  float x=fabsf(p.x),y=fabsf(p.y),z=fabsf(p.z),a;
  if(x>=y && x>=z){a=x;*face=p.x>=0?0:1;*u=p.x>=0?-p.z:p.z;*v=p.y;}
  else if(y>=z){a=y;*face=p.y>=0?2:3;*u=p.x;*v=p.y>=0?-p.z:p.z;}
  else {a=z;*face=p.z>=0?4:5;*u=p.z>=0?p.x:-p.x;*v=p.y;}
  *u=clampf((*u/a+1)*.5f,0,.99999994f);
  *v=clampf((*v/a+1)*.5f,0,.99999994f);
}
/* Consecutive ray samples usually remain in the same selected patch. Keep
 * its face-space bounds; subdivision uses powers of two, as the root walk. */
typedef struct { Node *node; int face; float scale, x, y; } VoxelCursor;
static Node *voxelNode(V3 p,float *u,float *v,VoxelCursor *cursor) {
  int face;voxelFaceUV(p,&face,u,v);int id=face;
  if(cursor->node && face==cursor->face) {
    float x=*u*cursor->scale-cursor->x,y=*v*cursor->scale-cursor->y;
    if(x>=0 && x<1 && y>=0 && y<1) {
      *u=x;*v=y;voxelLookupHits++;return cursor->node;
    }
  }
  float scale=1,x0=0,y0=0;
  while(!voxelCover[id] && nodes[id].child[0]>=0) {
    int x=*u>=.5f,y=*v>=.5f;
    int next=nodes[id].child[x+2*y];if(next<0)break;
    *u=*u*2-x;*v=*v*2-y;id=next;
    scale*=2;x0=x0*2+x;y0=y0*2+y;
  }
  Node *n=voxelCover[id] && nodes[id].vertices && nodes[id].pixels?&nodes[id]:NULL;
  *cursor=(VoxelCursor){n,face,scale,x0,y0};return n;
}
/* Cube-face coordinates before perspective division. A patch boundary is
 * a plane through the planet centre, so a tangent ray exits it at a linear
 * equation's root. Subtract a guard band before skipping any samples. */
static V3 voxelFaceAxes(V3 p,int face) {
  switch(face) {
    case 0:return v3(p.x,-p.z,p.y);
    case 1:return v3(-p.x,p.z,p.y);
    case 2:return v3(p.y,p.x,-p.z);
    case 3:return v3(-p.y,p.x,p.z);
    case 4:return v3(p.z,p.x,p.y);
    default:return v3(-p.z,-p.x,p.y);
  }
}
static float voxelPatchExit(V3 origin,V3 direction,const VoxelCursor *c,float d) {
  V3 a=voxelFaceAxes(origin,c->face),b=voxelFaceAxes(direction,c->face);
  float bounds[4]={2*c->x/c->scale-1,2*(c->x+1)/c->scale-1,
                   2*c->y/c->scale-1,2*(c->y+1)/c->scale-1};
  float exit=clipFar;
  for(int k=0;k<4;k++) {
    float start=(k<2?a.y:a.z)-bounds[k]*a.x;
    float slope=(k<2?b.y:b.z)-bounds[k]*b.x;
    /* Only the outward-facing boundary can be an exit. */
    if((k%2==0 && slope<0) || (k%2==1 && slope>0)) {
      float t=-start/slope;
      if(t<=d)return d; /* boundary/rounding ambiguity: do not skip */
      exit=fminf(exit,t);
    }
  }
  return fmaxf(d,exit-fmaxf(.05f,exit*.00001f));
}
static int voxelSphereHidden(V3 radial,V3 up,float radius,float outerRadius,
                             float focal,int horizon) {
  if(horizon>=voxelHeight || dot(radial,up)*radius>outerRadius)return 0;
  V3 upper=add(mul(add(radial,mul(up,-1)),outerRadius),mul(up,outerRadius-radius));
  float z=dot(upper,viewForward);
  return z>clipNear && voxelHeight*.5f-dot(upper,viewUp)*focal/z>=horizon+.5f;
}
/* Tiny direct-mapped BC1 block cache, valid only while cover payloads are
 * pinned. Decode once per block instead of four times per bilinear sample. */
typedef struct { const unsigned char *source; unsigned char rgb[16][3]; } VoxelBlock;
#define VOXEL_BLOCK_CACHE 64
static VoxelBlock voxelBlocks[VOXEL_BLOCK_CACHE];
static void voxelTexel(const Node *n,int x,int y,float *rgb) {
  x=x<0?0:x>=PAGE?PAGE-1:x;y=y<0?0:y>=PAGE?PAGE-1:y;
  const unsigned char *b=n->pixels+((y/4)*(PAGE/4)+x/4)*8;
  VoxelBlock *block=&voxelBlocks[((uintptr_t)b>>3)&(VOXEL_BLOCK_CACHE-1)];
  if(block->source!=b) {
    int colors[4][3];unsigned a=b[0]|b[1]<<8,c=b[2]|b[3]<<8;
    unpack565(a,colors[0]);unpack565(c,colors[1]);
    for(int k=0;k<3;k++) {colors[2][k]=(2*colors[0][k]+colors[1][k])/3;colors[3][k]=(colors[0][k]+2*colors[1][k])/3;}
    unsigned bits=(unsigned)b[4]|(unsigned)b[5]<<8|(unsigned)b[6]<<16|(unsigned)b[7]<<24;
    for(int i=0;i<16;i++)for(int k=0;k<3;k++)block->rgb[i][k]=colors[(bits>>(2*i))&3][k];
    block->source=b;
  }
  int index=(y&3)*4+(x&3);
  for(int k=0;k<3;k++)rgb[k]=block->rgb[index][k];
}
static void voxelColor(const Node *n,float u,float v,unsigned char *out) {
  float x=3.5f+120*u,y=3.5f+120*v;int ix=(int)x,iy=(int)y;x-=ix;y-=iy;
  float rgb[3]={0};
  for(int j=0;j<2;j++)for(int i=0;i<2;i++) {
    float c[3],weight=(i?x:1-x)*(j?y:1-y);voxelTexel(n,ix+i,iy+j,c);
    for(int k=0;k<3;k++)rgb[k]+=weight*c[k];
  }
  for(int k=0;k<3;k++)out[k]=(unsigned char)clampf(rgb[k]+.5f,0,255);
  out[3]=255;
}
static float voxelHeightAt(Node *n,float u,float v,V3 *normal) {
  float x=clampf(u*PATCH,0,PATCH-.00001f),y=clampf(v*PATCH,0,PATCH-.00001f);
  int ix=(int)x,iy=(int)y;x-=ix;y-=iy;float h=0;if(normal)*normal=v3(0,0,0);
  for(int j=0;j<2;j++)for(int i=0;i<2;i++) {
    const Vertex *p=&n->vertices[(iy+j)*(PATCH+1)+ix+i];
    float weight=(i?x:1-x)*(j?y:1-y);h+=weight*p->h;
    if(normal)*normal=add(*normal,mul(unpackNormal(p->n),weight));
  }
  return h;
}
/* A column/Y-buffer representation cannot handle arbitrary roll or a
 * vertical view. Preserve the complete scene using the mesh path there. */
static int voxelSupported(void) {
  V3 radial=norm(cameraEye);
  return dot(viewUp,radial)>.05f && fabsf(dot(viewRight,radial))<.0001f;
}
static void voxelResize(void) {
  int w=(rw+voxelScale-1)/voxelScale,h=(rh+voxelScale-1)/voxelScale;
  if(w==voxelWidth && h==voxelHeight)return;
  if(!voxelProgram)voxelProgram=program("bake.vert","voxel-terrain.frag");
  if(!voxelTextures[0])glGenTextures(3,voxelTextures);
  size_t bytes=(size_t)w*h*12;
  if(bytes>voxelGPUBytes && !vramReserve(bytes-voxelGPUBytes))
    die("voxel targets exceed VRAM budget; try --voxel-scale 2");
  voxelGPUBytes=bytes;
  for(int k=0;k<3;k++) {
    free(voxelBuffers[k]);voxelBuffers[k]=calloc((size_t)w*h,4);
    if(!voxelBuffers[k])die("voxel framebuffer allocation");
    glActiveTexture(GL_TEXTURE0+k);glBindTexture(GL_TEXTURE_2D,voxelTextures[k]);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,h,w,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
  }
  voxelWidth=w;voxelHeight=h;checkGL("voxel targets");
}
static void voxelRaster(int bounded) {
  memset(voxelCover,0,sizeof(voxelCover));
  float maximum=0;
  for(int i=0;i<selectedCount;i++) {
    voxelCover[selected[i]]=1;maximum=fmaxf(maximum,nodes[selected[i]].maxHeight);
  }
  float outerRadius=RADIUS+maximum+2;
  memset(voxelBlocks,0,sizeof(voxelBlocks));
  V3 up=norm(cameraEye);float radius=sqrtf(dot(cameraEye,cameraEye));
  float upDot=dot(viewUp,up),focal=voxelHeight/(2*tanf(PI/6));
  if(upDot<.05f)die("voxel prototype requires upright camera, pitch less than about 87 degrees");
  /* Worker reclamation is mutex-protected. The diagnostic caster pins the
   * current cover while reading its payloads; this cost is included in timing. */
  SDL_LockMutex(mutex);
  for(int x=0;x<voxelWidth;x++) {
    float sx=(2*(x+.5f)/voxelWidth-1)*tanf(PI/6)*width/height;
    V3 plane=add(viewForward,mul(viewRight,sx));
    V3 lateral=norm(add(plane,mul(viewUp,-dot(plane,up)/upDot)));
    int horizon=voxelHeight,steps=0;float d=fmaxf(.25f,clipNear);
    VoxelCursor cursor={0};
    while(d<clipFar && horizon>0) {
      int checkBounds=bounded && !(steps++ & 15);
      V3 radial=norm(add(mul(up,RADIUS),mul(lateral,d)));
      /* Beyond the tangent of an enclosing sphere its projected upper
       * profile decreases monotonically. Once hidden, every later radial
       * height is hidden too. This uses the whole cover's maximum, not sea
       * level or the current patch; distant mountains remain eligible. */
      if(checkBounds && voxelSphereHidden(radial,up,radius,outerRadius,focal,horizon)) {
        voxelHorizonStops++;break;
      }
      float u,v;Node *n=voxelNode(radial,&u,&v,&cursor);voxelSamples++;
      float step=fmaxf(.25f,d*.004f);
      if(n) {
        step=fmaxf(step,n->size/(PATCH*2));
        if(checkBounds && voxelSphereHidden(radial,up,radius,RADIUS+n->maxHeight+2,focal,horizon)) {
          float exit=voxelPatchExit(mul(up,RADIUS),lateral,&cursor,d);
          /* Preserve the original distance sampling sequence, including the
           * first sample in the next patch. Only omit hidden evaluations. */
          if(d+step<exit) {
            do { d+=step;voxelSkippedSamples++;
              step=fmaxf(fmaxf(.25f,d*.004f),n->size/(PATCH*2));
            } while(d+step<exit);
            continue;
          }
        }
        V3 normal;float h=voxelHeightAt(n,u,v,NULL);
        /* Stable camera-relative expression avoids cancelling ~200 km values. */
        V3 delta=add(mul(add(radial,mul(up,-1)),RADIUS+h),mul(up,RADIUS+h-radius));
        float z=dot(delta,viewForward);
        if(z>clipNear && z<clipFar) {
          float screen=voxelHeight*.5f-dot(delta,viewUp)*focal/z;
          int top=(int)clampf(ceilf(screen-.5f),0,voxelHeight);
          if(top<horizon) {
            unsigned char color[4];voxelColor(n,u,v,color);
            voxelHeightAt(n,u,v,&normal);normal=norm(normal);
            unsigned inv=(unsigned)clampf(clipNear/z*16777215.f,1,16777215);
            unsigned char normBytes[4]={(unsigned char)(normal.x*127+128),(unsigned char)(normal.y*127+128),(unsigned char)(normal.z*127+128),255};
            /* Transposed textures make a vertical span contiguous in RAM.
             * memcpy handles unaligned/aliasing rules and becomes word stores. */
            unsigned char depth[4]={inv>>16,inv>>8,inv,255};
            size_t at=((size_t)x*voxelHeight+voxelHeight-horizon)*4;
            size_t end=((size_t)x*voxelHeight+voxelHeight-top)*4;
            for(;at<end;at+=4) {
              memcpy(voxelBuffers[0]+at,color,4);memcpy(voxelBuffers[1]+at,normBytes,4);
              memcpy(voxelBuffers[2]+at,depth,4);
            }
            horizon=top;voxelSpans++;
          }
        }
      }
      d+=step;
    }
    /* Only clear uncovered sky; all covered depth pixels were overwritten. */
    memset(voxelBuffers[2]+((size_t)x*voxelHeight+voxelHeight-horizon)*4,0,(size_t)horizon*4);
  }
  SDL_UnlockMutex(mutex);
}
static void voxelDraw(void) {
  voxelResize();Uint64 start=SDL_GetPerformanceCounter();
  V3 up=norm(cameraEye);float radius=sqrtf(dot(cameraEye,cameraEye));
  voxelRaster(1);
  if(voxelCheck && frameNo%120==119) {
    size_t bytes=(size_t)voxelWidth*voxelHeight*4;
    unsigned char *saved=malloc(bytes*3);if(!saved)die("voxel validation allocation");
    for(int k=0;k<3;k++)memcpy(saved+bytes*k,voxelBuffers[k],bytes);
    voxelRaster(0);
    for(int k=0;k<3;k++)if(memcmp(saved+bytes*k,voxelBuffers[k],bytes))
      die("voxel enclosing-sphere culling changed framebuffer");
    free(saved);printf("VOXEL_CHECK frame=%d bounded/unbounded identical\n",frameNo);
  }
  Uint64 filled=SDL_GetPerformanceCounter();
  for(int k=0;k<3;k++) {
    glActiveTexture(GL_TEXTURE0+k);glBindTexture(GL_TEXTURE_2D,voxelTextures[k]);
    glTexSubImage2D(GL_TEXTURE_2D,0,0,0,voxelHeight,voxelWidth,GL_RGBA,GL_UNSIGNED_BYTE,voxelBuffers[k]);
  }
  GLuint p=voxelProgram;glUseProgram(p);
  tex(p,"baseTex",0,voxelTextures[0]);tex(p,"normalTex",1,voxelTextures[1]);tex(p,"depthTex",2,voxelTextures[2]);tex(p,"detailTex",3,detailMap.tex);
  u3(p,"sun",sun);u3(p,"ambientLight",ambientLight);u3(p,"sunLight",sunLight);u3(p,"fogColor",fogColor);u3(p,"ambientUp",up);
  u3(p,"forward",viewForward);u3(p,"right",viewRight);u3(p,"up",viewUp);
  float period=100.f/17;u3(p,"eyePhase",v3(fmodf(cameraEye.x,period),fmodf(cameraEye.y,period),fmodf(cameraEye.z,period)));
  u2(p,"lens",tanf(PI/6)*width/height,tanf(PI/6));u2(p,"clip",clipNear,clipFar);
  u2(p,"depthRange",worldDepthLo,worldDepthHi);u1(p,"exposure",sceneExposure);
  u1(p,"fogHeightFactor",exp2f(-fmaxf(radius-RADIUS,0)/13000));u1(p,"fastMode",performanceMode);
  glDisable(GL_CULL_FACE);glDepthRange(0,1);glDepthFunc(GL_ALWAYS);quad();
  glDepthFunc(GL_LESS);glDepthRange(worldDepthLo,worldDepthHi);glEnable(GL_CULL_FACE);glActiveTexture(GL_TEXTURE0);
  checkGL("voxel composite");
  voxelCpuMS+=(filled-start)*1000.0/SDL_GetPerformanceFrequency();
  voxelUploadMS+=(SDL_GetPerformanceCounter()-filled)*1000.0/SDL_GetPerformanceFrequency();voxelFrames++;
  if(frameNo%120==119) { printf("VOXEL frame=%d size=%dx%d cpu_ms=%.3f upload_submit_ms=%.3f samples=%llu spans=%llu lookup_hits=%llu horizon_stops=%llu skipped_samples=%llu gpu_bytes=%zu\n",frameNo,voxelWidth,voxelHeight,voxelCpuMS/voxelFrames,voxelUploadMS/voxelFrames,voxelSamples/voxelFrames,voxelSpans/voxelFrames,voxelLookupHits/voxelFrames,voxelHorizonStops/voxelFrames,voxelSkippedSamples/voxelFrames,voxelGPUBytes);
    voxelCpuMS=voxelUploadMS=0;voxelSamples=voxelSpans=voxelLookupHits=voxelHorizonStops=voxelSkippedSamples=0;voxelFrames=0;
  }
}
static void voxelClose(void) {
  if(voxelTextures[0])glDeleteTextures(3,voxelTextures);
  if(voxelProgram)glDeleteProgram(voxelProgram);
  for(int k=0;k<3;k++)free(voxelBuffers[k]);
}
