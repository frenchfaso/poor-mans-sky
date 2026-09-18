// SPDX-License-Identifier: MPL-2.0
/* GPU-only intersection experiment for R300: one heightfield step per short
 * fragment pass, RGBA8 ping-pong state, no compute/float render targets.
 * CPU bakes a reusable tangent chart from streamed planet data, never casts
 * screen rays. Unresolved pixels retain the static mesh; report coverage. */
#define RAY_MAP_SIZE 512
#define RAY_RANGE 32768.f
#define RAY_UNITS 65536.f
#define RAY_EXTENT 65536.f
#define RAY_WARP 512.f
static Target rayState[2];
static GLuint rayMap[3],rayOutputFBO,rayStepP,rayPayloadP,rayDepthP;
static V3 rayAnchorUp,rayEast,rayNorth;
static float raySlope=1,rayPixelTolerance=.5f;
static int rayMapReady,rayMapFrame=-1000,rayMapBuilds,rayPasses=64;
static unsigned long long rayMapRevision;
static double rayMapMS,raySubmitMS;
static int rayRenderFrames;
static unsigned char *rayMapData[3];
static float *rayMapHeights;
static void rayPack(float value,unsigned char *p) {
  unsigned q=(unsigned)(clampf(value,0,.99999f)*16581375.f+.5f);
  p[0]=q/65025;p[1]=(q/255)%255;p[2]=q%255;p[3]=255;
}
static float rayMapCoord(int i) {
  float a=(2.f*i/(RAY_MAP_SIZE-1)-1)*(RAY_EXTENT/(RAY_EXTENT+RAY_WARP));
  return RAY_WARP*a/(1-fabsf(a));
}
static V3 rayLocal(V3 p) {return v3(dot(p,rayEast),dot(p,rayAnchorUp),dot(p,rayNorth));}
static void rayBakeMap(void) {
  V3 up=norm(cameraEye);
  float moved=RADIUS*sqrtf(dot(add(up,mul(rayAnchorUp,-1)),add(up,mul(rayAnchorUp,-1))));
  if(rayMapReady && moved<32 && (rayMapRevision==voxelPayloadRevision || frameNo-rayMapFrame<120))return;
  Uint64 start=SDL_GetPerformanceCounter();
  rayAnchorUp=up;rayEast=norm(cross(up,fabsf(up.z)<.9f?v3(0,0,1):v3(0,1,0)));rayNorth=cross(rayEast,up);
  for(int k=0;k<3;k++)if(!rayMapData[k]) {
    rayMapData[k]=malloc(RAY_MAP_SIZE*RAY_MAP_SIZE*4);
    if(!rayMapData[k])die("ray map allocation");
  }
  if(!rayMapHeights)rayMapHeights=malloc(RAY_MAP_SIZE*RAY_MAP_SIZE*sizeof(float));
  if(!rayMapHeights)die("ray height allocation");
  voxelCoverMaximum();memset(voxelBlocks,0,sizeof(voxelBlocks));
  SDL_LockMutex(mutex);
  int validCount=0;
  for(int y=0;y<RAY_MAP_SIZE;y++) {
    if(!(y&15))SDL_PumpEvents();
    VoxelCursor cursor={0};float z=rayMapCoord(y);
    for(int x=0;x<RAY_MAP_SIZE;x++) {
      float xx=rayMapCoord(x),rr=xx*xx+z*z;
      V3 tangent=add(mul(rayEast,xx),mul(rayNorth,z)),n=up;
      float h=0,u=0,v=0,localH=0;Node *node=NULL;
      /* Solve the radial field at a fixed tangent-chart coordinate. */
      for(int iteration=0;iteration<3;iteration++) {
        float vertical=sqrtf(fmaxf(1,(RADIUS+h)*(RADIUS+h)-rr));
        V3 radial=norm(add(tangent,mul(up,vertical)));
        node=voxelNode(radial,&u,&v,&cursor);
        if(!node)break;
        h=voxelHeightAt(node,u,v,&n);
      }
      size_t at=(size_t)y*RAY_MAP_SIZE+x;
      localH=sqrtf(fmaxf(1,(RADIUS+h)*(RADIUS+h)-rr))-RADIUS;
      /* Compand height: linear .5+h/65536 loses sub-metre precision in
       * the RV350 FP24 dot/subtract. Keep useful mantissa bits near ground. */
      float encoded=.5f+.5f*localH/(fabsf(localH)+64);
      rayMapHeights[at]=encoded;
      rayPack(encoded,rayMapData[0]+at*4);
      rayMapData[0][at*4+3]=node?255:0;
      if(node){voxelColor(node,u,v,rayMapData[1]+at*4);validCount++;}
      else memset(rayMapData[1]+at*4,0,4);
      n=norm(n);
      unsigned char *normal=rayMapData[2]+at*4;
      normal[0]=(unsigned char)(n.x*127+128);normal[1]=(unsigned char)(n.y*127+128);normal[2]=(unsigned char)(n.z*127+128);normal[3]=255;
    }
  }
  rayMapRevision=voxelPayloadRevision;SDL_UnlockMutex(mutex);
  /* Global Lipschitz bound for bilinear height samples after the nonlinear
   * chart warp. Include the maximum warp derivative within each interval. */
  float maxX=0,maxZ=0,da=2*(RAY_EXTENT/(RAY_EXTENT+RAY_WARP))/(RAY_MAP_SIZE-1);
  for(int y=1;y<RAY_MAP_SIZE;y++)for(int x=1;x<RAY_MAP_SIZE;x++) {
    int at=y*RAY_MAP_SIZE+x;
    float q[4]={rayMapHeights[at],rayMapHeights[at-1],rayMapHeights[at-RAY_MAP_SIZE],rayMapHeights[at-RAY_MAP_SIZE-1]};
    float extent=0;for(int k=0;k<4;k++)extent=fmaxf(extent,fabsf(2*q[k]-1));
    float inverseDerivative=128/powf(fmaxf(.00001f,1-extent),2);
    float a=rayMapCoord(x-1),b=rayMapCoord(x),closest=a*b<=0?0:fminf(fabsf(a),fabsf(b));
    float dx=fmaxf(fabsf(q[0]-q[1]),fabsf(q[2]-q[3]));
    maxX=fmaxf(maxX,dx*inverseDerivative/da*RAY_WARP/powf(RAY_WARP+closest,2));
    a=rayMapCoord(y-1);b=rayMapCoord(y);closest=a*b<=0?0:fminf(fabsf(a),fabsf(b));
    float dz=fmaxf(fabsf(q[0]-q[2]),fabsf(q[1]-q[3]));
    maxZ=fmaxf(maxZ,dz*inverseDerivative/da*RAY_WARP/powf(RAY_WARP+closest,2));
  }
  raySlope=fmaxf(.01f,sqrtf(maxX*maxX+maxZ*maxZ)*1.02f+.01f);
  if(!rayMap[0])glGenTextures(3,rayMap);
  for(int k=0;k<3;k++) {
    glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,rayMap[k]);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,RAY_MAP_SIZE,RAY_MAP_SIZE,0,GL_RGBA,GL_UNSIGNED_BYTE,rayMapData[k]);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
  }
  rayMapReady=1;rayMapFrame=frameNo;rayMapBuilds++;
  double ms=(SDL_GetPerformanceCounter()-start)*1000.0/SDL_GetPerformanceFrequency();rayMapMS+=ms;
  printf("GPU_RAY_MAP frame=%d build_ms=%.3f slope=%.3f valid=%d/%d\n",frameNo,ms,raySlope,validCount,RAY_MAP_SIZE*RAY_MAP_SIZE);
}
static void rayUniforms(GLuint p,int view) {
  glUseProgram(p);
  V3 local=rayLocal(add(cameraEye,mul(rayAnchorUp,-RADIUS)));
  u3(p,"eyeLocal",mul(local,1/RAY_UNITS));
  float scale=RAY_RANGE/RAY_UNITS;
  if(view) {
    u3(p,"rayForward",mul(rayLocal(viewForward),scale));u3(p,"rayRight",mul(rayLocal(viewRight),scale));u3(p,"rayUp",mul(rayLocal(viewUp),scale));
    u2(p,"lens",tanf(PI/6)*width/height,tanf(PI/6));
  }
  u2(p,"warp",RAY_WARP/RAY_UNITS,.5f*(RAY_EXTENT+RAY_WARP)/RAY_EXTENT*(RAY_MAP_SIZE-1)/RAY_MAP_SIZE);
}
static void rayDrawTargets(void) {
  GLint oldFbo;glGetIntegerv(GL_FRAMEBUFFER_BINDING,&oldFbo);
  glPushAttrib(GL_ALL_ATTRIB_BITS);
  glDisable(GL_POLYGON_STIPPLE);glDisable(GL_BLEND);glDisable(GL_DITHER);glDisable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);
  glDepthMask(GL_FALSE);glColorMask(1,1,1,1);
  size_t bytes=RAY_MAP_SIZE*RAY_MAP_SIZE*12u+(size_t)voxelWidth*voxelHeight*8;
  if(bytes>voxelRayGPUBytes && !vramReserve(bytes-voxelRayGPUBytes))die("GPU ray targets exceed VRAM budget");
  voxelRayGPUBytes=bytes;
  rayBakeMap();
  if(!rayStepP) {
    rayStepP=program("voxel-ray.vert","voxel-ray-step.frag");
    rayPayloadP=program("voxel-ray-resolve.vert","voxel-ray-payload.frag");
    rayDepthP=program("voxel-ray-resolve.vert","voxel-ray-depth.frag");
    glGenFramebuffers(1,&rayOutputFBO);
  }
  if(rayState[0].w!=voxelWidth || rayState[0].h!=voxelHeight) {
    for(int k=0;k<2;k++) {
      if(rayState[k].tex){glDeleteTextures(1,&rayState[k].tex);glDeleteFramebuffers(1,&rayState[k].fbo);}
      rayState[k]=target(voxelWidth,voxelHeight,0,0);
      glBindTexture(GL_TEXTURE_2D,rayState[k].tex);
      glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    }
  }
  Uint64 start=SDL_GetPerformanceCounter();
  glViewport(0,0,voxelWidth,voxelHeight);glBindFramebuffer(GL_FRAMEBUFFER,rayState[0].fbo);
  unsigned char initial[4];rayPack(clipNear/RAY_RANGE,initial);
  glClearColor(initial[0]/255.f,initial[1]/255.f,initial[2]/255.f,0);glClear(GL_COLOR_BUFFER_BIT);
  rayUniforms(rayStepP,1);tex(rayStepP,"heightTex",1,rayMap[0]);u1(rayStepP,"slope",raySlope);u2(rayStepP,"tolerance",.125f/RAY_UNITS,RAY_RANGE/RAY_UNITS*2*tanf(PI/6)/voxelHeight*rayPixelTolerance);
  int current=0;
  for(int i=0;i<rayPasses;i++) {
    glBindFramebuffer(GL_FRAMEBUFFER,rayState[1-current].fbo);tex(rayStepP,"stateTex",0,rayState[current].tex);quad();current=1-current;
  }
  glBindFramebuffer(GL_FRAMEBUFFER,rayOutputFBO);glViewport(0,0,voxelHeight,voxelWidth);glClearColor(0,0,0,0);
  for(int k=0;k<3;k++) {
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,voxelTextures[k],0);
    if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)die("GPU ray output incomplete");
    glClear(GL_COLOR_BUFFER_BIT);
    GLuint p=k==2?rayDepthP:rayPayloadP;rayUniforms(p,1);
    tex(p,"stateTex",0,rayState[current].tex);
    if(k<2)tex(p,"payloadTex",1,rayMap[k+1]);else u1(p,"nearRatio",clipNear/RAY_RANGE);
    quad();
  }
  glBindFramebuffer(GL_FRAMEBUFFER,oldFbo);glPopAttrib();glActiveTexture(GL_TEXTURE0);
  raySubmitMS+=(SDL_GetPerformanceCounter()-start)*1000.0/SDL_GetPerformanceFrequency();rayRenderFrames++;
  if(frameNo%120==119)printf("GPU_RAY frame=%d passes=%d map_builds=%d map_total_ms=%.3f submit_avg_ms=%.3f cpu_raster_builds=%llu\n",frameNo,rayPasses,rayMapBuilds,rayMapMS,raySubmitMS/rayRenderFrames,voxelRasterBuilds);
  checkGL("GPU ray targets");
  if(voxelRayAudit && frameNo%120==119) {
    size_t bytes=(size_t)voxelWidth*voxelHeight*4;
    unsigned char *gpu=malloc(bytes);if(!gpu)die("ray audit allocation");
    glBindTexture(GL_TEXTURE_2D,voxelTextures[2]);glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_UNSIGNED_BYTE,gpu);
    voxelRaster(1); /* Explicit diagnostic oracle only, excluded from benchmarks. */
    int hits=0,reference=0,both=0,missing=0,extra=0;double error=0;float maxError=0;
    for(size_t i=0;i<bytes;i+=4) {
      int a=gpu[i+3]!=0,b=voxelBuffers[2][i+3]!=0;hits+=a;reference+=b;missing+=b&&!a;extra+=a&&!b;
      if(a&&b) {
        unsigned ga=(unsigned)gpu[i]<<16|(unsigned)gpu[i+1]<<8|gpu[i+2];
        unsigned cb=(unsigned)voxelBuffers[2][i]<<16|(unsigned)voxelBuffers[2][i+1]<<8|voxelBuffers[2][i+2];
        float d=fabsf(clipNear*16777215.f/ga-clipNear*16777215.f/cb);error+=d;maxError=fmaxf(maxError,d);both++;
      }
    }
    printf("GPU_RAY_AUDIT frame=%d hits=%d reference=%d missing=%d extra=%d mean_z_error=%.3f max_z_error=%.3f\n",frameNo,hits,reference,missing,extra,both?error/both:0,maxError);
    free(gpu);
  }
}
static void rayClose(void) {
  for(int k=0;k<3;k++)free(rayMapData[k]);
  free(rayMapHeights);
  glDeleteTextures(3,rayMap);glDeleteFramebuffers(1,&rayOutputFBO);
  for(int k=0;k<2;k++){glDeleteTextures(1,&rayState[k].tex);glDeleteFramebuffers(1,&rayState[k].fbo);}
  glDeleteProgram(rayStepP);glDeleteProgram(rayPayloadP);glDeleteProgram(rayDepthP);
}
