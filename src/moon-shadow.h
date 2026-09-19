// SPDX-License-Identifier: MPL-2.0
/* Reuse the ship shadow map on the stitched lunar mesh. Receiver coordinates
 * and light axes are both body-local; GL still projects the same world mesh. */
static void moonShadowGround(void) {
 if(!sunReady || !sunBodyMoon || wire)return;
 Uint64 start=SDL_GetPerformanceCounter();
 glUseProgram(sunGroundP);tex(sunGroundP,"shadowTex",0,sunMap.tex);
 u1(sunGroundP,"shadowTexel",1.f/sunMap.w);u1(sunGroundP,"strength",sunStrength);
 u3(sunGroundP,"lightRight",moonLocalVector(sunRight));u3(sunGroundP,"lightUp",moonLocalVector(sunUp));u3(sunGroundP,"lightDir",moonLocalVector(sunDirection));
 glEnable(GL_BLEND);glBlendFunc(GL_ZERO,GL_SRC_COLOR);glDepthMask(GL_FALSE);glDepthFunc(GL_LEQUAL);
 glEnable(GL_POLYGON_OFFSET_FILL);glPolygonOffset(-1,-1);glDisable(GL_CULL_FACE);
 glEnableClientState(GL_VERTEX_ARRAY);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);
 V3 anchor=moonLocalPoint(sunAnchor);
 float rotation[16]={celestial.x.x,celestial.x.y,celestial.x.z,0,celestial.y.x,celestial.y.y,celestial.y.z,0,celestial.z.x,celestial.z.y,celestial.z.z,0,0,0,0,1};
 for(int i=0;i<moonSelectedCount;i++) {
  MoonPatch *p=moonSelected[i];if(!p->visible)continue;
  V3 delta=add(p->boundCenter,mul(anchor,-1));
  V3 gap=v3(fmaxf(fabsf(delta.x)-p->boundHalf.x,0),fmaxf(fabsf(delta.y)-p->boundHalf.y,0),fmaxf(fabsf(delta.z)-p->boundHalf.z,0));
  if(dot(gap,gap)>14*14)continue;
  u3(sunGroundP,"patchDelta",add(p->center,mul(anchor,-1)));
  V3 offset=add(moonWorldPoint(p->center),mul(cameraEye,-1));
  glPushMatrix();glTranslatef(offset.x,offset.y,offset.z);glMultMatrixf(rotation);
  glBindBuffer(GL_ARRAY_BUFFER,p->vbo);glVertexPointer(3,GL_FLOAT,sizeof(MoonVertex),(void*)offsetof(MoonVertex,p));
  /* Skirts close cracks; they must not multiply the ground shadow again. */
  glDrawArrays(GL_TRIANGLES,0,MOON_GRID*MOON_GRID*6);glPopMatrix();sunReceiverDraws++;
 }
 glBindBuffer(GL_ARRAY_BUFFER,0);glDisableClientState(GL_VERTEX_ARRAY);
 glDisable(GL_POLYGON_OFFSET_FILL);glEnable(GL_CULL_FACE);
 glDepthMask(GL_TRUE);glDisable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
 sunShadowMS+=(SDL_GetPerformanceCounter()-start)*1000.0/SDL_GetPerformanceFrequency();
}
