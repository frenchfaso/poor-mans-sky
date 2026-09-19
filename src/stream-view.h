// SPDX-License-Identifier: MPL-2.0
/* Published under the terrain mutex; workers only read this camera snapshot.
 * Streaming planes are wider than drawing planes and never cull coverage. */
static int directionalStreaming=1, streamViewReady;
static V3 streamPriorityEye, streamPlanes[2][4];
static float streamBubble,streamDetailRange;
static void streamViewSetup(void) {
  streamPriorityEye=cameraEye;
  streamBubble=flying?quality()->flyBubble:quality()->walkBubble;
  streamDetailRange=flying?quality()->flyDetail:quality()->walkDetail;
  streamViewReady=directionalStreaming;
  if(!streamViewReady)return;
  static int planeWidth,planeHeight,planePreset=-1;
  static V3 lastForward,lastRight,lastUp;
  static float fx[2],rx[2],fy[2],uy[2];
  int projectionChanged=planeWidth!=width || planeHeight!=height || planePreset!=qualityPreset;
  if(projectionChanged) {
    for(int retained=0;retained<2;retained++) {
      float margin=(quality()->streamMargin+(retained?6:0))*PI/180;
      float ty=tanf(PI/6+margin);
      float tx=tanf(fminf(1.53f,atanf(tanf(PI/6)*width/height)+margin));
      rx[retained]=1/sqrtf(1+tx*tx);fx[retained]=tx*rx[retained];
      uy[retained]=1/sqrtf(1+ty*ty);fy[retained]=ty*uy[retained];
    }
    planeWidth=width;planeHeight=height;planePreset=qualityPreset;
  }
  if(!projectionChanged && !memcmp(&lastForward,&viewForward,sizeof(V3)) &&
     !memcmp(&lastRight,&viewRight,sizeof(V3)) && !memcmp(&lastUp,&viewUp,sizeof(V3)))return;
  for(int retained=0;retained<2;retained++) {
    /* Camera axes are orthonormal: normalization factors depend only on FOV. */
    V3 horizontal=mul(viewForward,fx[retained]),right=mul(viewRight,rx[retained]);
    V3 vertical=mul(viewForward,fy[retained]),up=mul(viewUp,uy[retained]);
    streamPlanes[retained][0]=add(horizontal,right);
    streamPlanes[retained][1]=add(horizontal,mul(right,-1));
    streamPlanes[retained][2]=add(vertical,up);
    streamPlanes[retained][3]=add(vertical,mul(up,-1));
  }
  lastForward=viewForward;lastRight=viewRight;lastUp=viewUp;
}
static int streamBandDelta(V3 d,float distance2,float radius,int previous) {
  /* sqrt(distance2)-radius <= boundary, expressed without a square root. */
  float limit=streamBubble*(previous==0?1.2f:1)+radius;
  if(distance2<=limit*limit)return 0;
  limit=streamDetailRange*(previous>=0 && previous<5?1.15f:1)+radius;
  if(distance2>limit*limit)return 5;
  int retained=previous>=0 && previous<5;
  for(int p=0;p<4;p++)if(dot(d,streamPlanes[retained][p]) < -radius)return 5;
  float boundary=streamBubble*4;
  for(int band=1;band<4;band++,boundary*=4) {
    limit=boundary*(previous==band?1.15f:previous==band+1?.9f:1)+radius;
    if(distance2<=limit*limit)return band;
  }
  return 4;
}
static int streamBand(V3 center,float radius,int previous) {
  V3 d=add(center,mul(streamPriorityEye,-1));
  return streamBandDelta(d,dot(d,d),radius,previous);
}
static float streamRadius(const Node *n) {
  /* Unknown pages use a generous bound until measured metadata is uploaded. */
  return n->boundRadius>0?n->boundRadius:n->size*1.5f;
}
static V3 streamCenter(const Node *n) {
  return n->boundRadius>0?n->boundCenter:n->center;
}
