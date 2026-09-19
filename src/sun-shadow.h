// SPDX-License-Identifier: MPL-2.0
/* Local directional shadow map: RG8 packed depth, no vendor depth-compare
 * extension. Only near solid vegetation and a cheap ship proxy cast shadows.
 * Ground receives via an isolated multiplicative pass; water shaders unchanged. */
static Target sunMap;
static GLuint sunCastP,sunGroundP;
static int sunShadows=1,sunReady,sunLast=-100,sunUpdates,sunCasterDraws,sunReceiverDraws;
static V3 sunAnchor,sunRight,sunUp,sunDirection,sunLastEye,sunLastShip,sunLastForward,sunLastUp;
static int sunLastFlying=-1;
static double sunShadowMS;
static void sunProxy(int shape,V3 pos,V3 size) {
  glPushMatrix();glTranslatef(pos.x,pos.y,pos.z);glScalef(size.x,size.y,size.z);
  glBindBuffer(GL_ARRAY_BUFFER,shapeVBO[shape]);
  glVertexPointer(3,GL_FLOAT,sizeof(ActorVertex),(void*)offsetof(ActorVertex,p));
  glTexCoordPointer(2,GL_FLOAT,sizeof(ActorVertex),(void*)offsetof(ActorVertex,u));
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,shapeEBO[shape]);
  glDrawElements(GL_TRIANGLES,shapeCount[shape],GL_UNSIGNED_SHORT,0);
  glPopMatrix();sunCasterDraws++;
}
static void sunShadowUpdate(void) {
  float height=dot(norm(cameraEye),sun);
  if(!sunShadows || height<.12f || sqrtf(dot(cameraEye,cameraEye))-RADIUS-elevation(norm(cameraEye))>100) {sunReady=0;return;}
  V3 movingEye=add(cameraEye,mul(sunLastEye,-1));
  V3 currentShip=flying?eye:shipPos,currentForward=flying?flightForward:shipHeading;
  V3 currentUp=flying?flightUp:bodyUp(shipPos);
  V3 movingShip=add(currentShip,mul(sunLastShip,-1));
  V3 movingSun=add(sun,mul(sunDirection,-1));
  if(sunReady && frameNo-sunLast<4 && dot(movingSun,movingSun)<4e-7f && dot(movingEye,movingEye)<.09f &&
     dot(movingShip,movingShip)<.0625f && dot(currentForward,sunLastForward)>.9999f &&
     dot(currentUp,sunLastUp)>.9999f && flying==sunLastFlying) return;
  Uint64 start=SDL_GetPerformanceCounter();
  if(!sunMap.tex) {
    sunMap=target(512,512,1,0);
    glBindTexture(GL_TEXTURE_2D,sunMap.tex);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    if(!sunCastP)sunCastP=program("sun-cast.vert","sun-cast.frag");
    if(!sunGroundP)sunGroundP=program("sun-ground.vert","sun-ground.frag");
  }
  sunDirection=sun;
  sunRight=norm(cross(fabsf(sun.y)<.9f?v3(0,1,0):v3(1,0,0),sun));
  sunUp=cross(sun,sunRight);
  V3 d=norm(cameraEye);sunAnchor=mul(d,RADIUS+fmaxf(elevation(d),0));
  /* Snap the orthographic footprint in light space, not to camera orientation. */
  float texel=32.0f/512;
  float x=dot(sunAnchor,sunRight),y=dot(sunAnchor,sunUp);
  sunAnchor=add(sunAnchor,add(mul(sunRight,roundf(x/texel)*texel-x),mul(sunUp,roundf(y/texel)*texel-y)));
  V3 delta=add(cameraEye,mul(sunAnchor,-1));
  float m[16]={sunRight.x,sunUp.x,sun.x,0,sunRight.y,sunUp.y,sun.y,0,sunRight.z,sunUp.z,sun.z,0,dot(delta,sunRight),dot(delta,sunUp),dot(delta,sun),1};
  GLfloat clear[4];glGetFloatv(GL_COLOR_CLEAR_VALUE,clear);
  GLboolean dither=glIsEnabled(GL_DITHER);glDisable(GL_DITHER);
  glBindFramebuffer(GL_FRAMEBUFFER,sunMap.fbo);glViewport(0,0,512,512);
  glDepthRange(0,1);glDepthMask(GL_TRUE);glDepthFunc(GL_LESS);
  glClearColor(1,1,1,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
  glClearColor(clear[0],clear[1],clear[2],clear[3]);
  glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrtho(-16,16,-16,16,-128,128);
  glMatrixMode(GL_MODELVIEW);glLoadMatrixf(m);
  glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);glDisable(GL_CULL_FACE);glDisable(GL_BLEND);
  glEnableClientState(GL_VERTEX_ARRAY);glEnableClientState(GL_TEXTURE_COORD_ARRAY);
  glUseProgram(sunCastP);tex(sunCastP,"foliageTex",0,foliageTex);u1(sunCastP,"cutoff",.4f);
  sunCasterDraws=0;
  for(int i=0;i<natureUsed;i++) {
    NatureCell *c=&nature[i];if(!c->vbo || !c->solid || c->wanted<natureFrame-1)continue;
    V3 delta=add(c->boundCenter,mul(sunAnchor,-1));
    if(sqrtf(dot(delta,delta))-sqrtf(dot(c->boundHalf,c->boundHalf))>80)continue;
    V3 offset=add(c->center,mul(cameraEye,-1));
    glPushMatrix();glTranslatef(offset.x,offset.y,offset.z);naturePointers(c->vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,c->ebo);glDrawElements(GL_TRIANGLES,c->solid,GL_UNSIGNED_SHORT,0);
    glPopMatrix();sunCasterDraws++;
  }
  V3 ship=flying?add(eye,mul(bodyUp(eye),-1.3f)):shipPos;
  delta=add(ship,mul(sunAnchor,-1));
  if(dot(delta,delta)<100*100) {
    u1(sunCastP,"cutoff",0);glPushMatrix();
    if(flying) {
      V3 right=norm(cross(flightForward,flightUp)),up=cross(right,flightForward),p=add(ship,mul(cameraEye,-1));
      float frame[16]={right.x,right.y,right.z,0,up.x,up.y,up.z,0,-flightForward.x,-flightForward.y,-flightForward.z,0,p.x,p.y,p.z,1};glMultMatrixf(frame);
    } else actorFrame(shipPos,shipHeading,bodyUp(shipPos),0);
    sunProxy(3,v3(0,.35,-.3),v3(1.1,.65,3.6));
    sunProxy(3,v3(0,.87,-1),v3(.76,.58,1.45));
    for(int side=-1;side<=1;side+=2) {
      glPushMatrix();glTranslatef(side*1.6f,0,.8f);glRotatef(side*22,0,1,0);
      sunProxy(1,v3(0,0,0),v3(1.45,.12,.70));glPopMatrix();
      sunProxy(3,v3(side*2.7f,.1,1.15),v3(.47,.47,1.7));
      sunProxy(1,v3(side*2.7f,.4,1.7),v3(.22,.52,.68));
      if(!flying){sunProxy(1,v3(side*.85f,-.67,1.3),v3(.10,.48,.10));sunProxy(1,v3(side*.85f,-1.08,1.3),v3(.35,.09,.45));}
    }
    sunProxy(1,v3(0,.24,-3.15),v3(.5,.13,.9));
    sunProxy(1,v3(0,.91,2.05),v3(.035,.48,.035));
    glPopMatrix();
  }
  if(dither)glEnable(GL_DITHER);
  glBindFramebuffer(GL_FRAMEBUFFER,scene.fbo);glViewport(0,0,rw,rh);
  glDepthFunc(GL_LEQUAL);glDepthRange(worldDepthLo,worldDepthHi);glEnable(GL_CULL_FACE);camera();
  glBindBuffer(GL_ARRAY_BUFFER,0);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);
  if(wire)glPolygonMode(GL_FRONT_AND_BACK,GL_LINE);
  sunReady=1;sunLast=frameNo;sunUpdates++;
  sunLastEye=cameraEye;sunLastShip=currentShip;sunLastForward=currentForward;sunLastUp=currentUp;sunLastFlying=flying;
  sunShadowMS+=(SDL_GetPerformanceCounter()-start)*1000.0/SDL_GetPerformanceFrequency();
}
static void sunShadowGround(void) {
  sunReceiverDraws=0;if(!sunReady || wire)return;
  Uint64 start=SDL_GetPerformanceCounter();
  glUseProgram(sunGroundP);tex(sunGroundP,"shadowTex",0,sunMap.tex);
  u3(sunGroundP,"lightRight",sunRight);u3(sunGroundP,"lightUp",sunUp);u3(sunGroundP,"lightDir",sunDirection);
  u1(sunGroundP,"strength",.38f*clampf((dot(norm(cameraEye),sun)-.12f)/.18f,0,1));
  glEnable(GL_BLEND);glBlendFunc(GL_ZERO,GL_SRC_COLOR);glDepthMask(GL_FALSE);glDepthFunc(GL_LEQUAL);
  glEnable(GL_POLYGON_OFFSET_FILL);glPolygonOffset(-1,-1);
  if(voxelTerrain) {
    u3(sunGroundP,"patchDelta",mul(sunAnchor,-1));
    glPushMatrix();glTranslatef(-cameraEye.x,-cameraEye.y,-cameraEye.z);
    staticPlanetGeometry();glPopMatrix();sunReceiverDraws=1;
  }
  for(int i=0;!voxelTerrain && i<selectedCount;i++) {
    Node *n=&nodes[selected[i]];if(!n->visible || n->maxHeight<0)continue;
    V3 delta=add(n->boundCenter,mul(sunAnchor,-1));
    V3 gap=v3(fmaxf(fabsf(delta.x)-n->boundHalf.x,0),fmaxf(fabsf(delta.y)-n->boundHalf.y,0),fmaxf(fabsf(delta.z)-n->boundHalf.z,0));
    if(dot(gap,gap)>14*14)continue;
    u3(sunGroundP,"patchDelta",add(n->center,mul(sunAnchor,-1)));
    V3 offset=add(n->center,mul(cameraEye,-1));glPushMatrix();glTranslatef(offset.x,offset.y,offset.z);
    geometry(n->vbo,meshLOD(n));glPopMatrix();sunReceiverDraws++;
  }
  glDisable(GL_POLYGON_OFFSET_FILL);
  glDepthFunc(GL_LEQUAL);glDepthMask(GL_TRUE);glDisable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
  sunShadowMS+=(SDL_GetPerformanceCounter()-start)*1000.0/SDL_GetPerformanceFrequency();
}
static void sunShadowClose(void) {
  glDeleteTextures(1,&sunMap.tex);glDeleteFramebuffers(1,&sunMap.fbo);glDeleteRenderbuffers(1,&sunMap.depth);
  glDeleteProgram(sunCastP);glDeleteProgram(sunGroundP);
}
