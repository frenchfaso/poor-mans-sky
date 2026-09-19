// SPDX-License-Identifier: MPL-2.0
/* Published under the terrain mutex; workers only read this camera snapshot.
 * Streaming planes are wider than drawing planes and never cull coverage. */
static int directionalStreaming=1, streamViewReady;
static V3 streamPriorityEye, streamPlanes[2][4];
static float streamBubble;
static void streamViewSetup(void) {
  streamPriorityEye=cameraEye;
  streamBubble=flying?300:100;
  for(int retained=0;retained<2;retained++) {
    float margin=(retained?16:10)*PI/180;
    float ty=tanf(PI/6+margin);
    float tx=tanf(fminf(1.53f,atanf(tanf(PI/6)*width/height)+margin));
    streamPlanes[retained][0]=norm(add(mul(viewForward,tx),viewRight));
    streamPlanes[retained][1]=norm(add(mul(viewForward,tx),mul(viewRight,-1)));
    streamPlanes[retained][2]=norm(add(mul(viewForward,ty),viewUp));
    streamPlanes[retained][3]=norm(add(mul(viewForward,ty),mul(viewUp,-1)));
  }
  streamViewReady=directionalStreaming;
}
static int streamBand(V3 center,float radius,int previous) {
  V3 d=add(center,mul(streamPriorityEye,-1));
  float distance=fmaxf(0,sqrtf(dot(d,d))-radius);
  if(distance<=streamBubble*(previous==0?1.2f:1))return 0;
  int retained=previous>=0 && previous<5;
  for(int p=0;p<4;p++)if(dot(d,streamPlanes[retained][p]) < -radius)return 5;
  float boundary=streamBubble*4;
  for(int band=1;band<4;band++,boundary*=4) {
    float limit=boundary*(previous==band?1.15f:previous==band+1?.9f:1);
    if(distance<=limit)return band;
  }
  return 4;
}
static float streamRadius(const Node *n) {
  /* Unknown pages use a generous bound until measured metadata is uploaded. */
  return n->boundRadius>0?n->boundRadius:n->size*1.5f;
}
static V3 streamCenter(const Node *n) {
  return n->boundRadius>0?n->boundCenter:n->center;
}
