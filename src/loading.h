// SPDX-License-Identifier: MPL-2.0
/* One monotonic, phase-weighted startup bar, shared by cached and generated
 * paths. Phase weights describe completed work, not a time-remaining estimate. */
static float bootProgressValue;
static void bootProgress(float value,const char *stage,const char *detail) {
  if(!window)return;
  bootProgressValue=fmaxf(bootProgressValue,clampf(value,0,1));
  SDL_PumpEvents();SDL_Event quit;
  if(SDL_PeepEvents(&quit,1,SDL_GETEVENT,SDL_QUIT,SDL_QUIT)>0){SDL_Quit();exit(0);}
  GLint oldFbo,oldProgram,oldMode;
  glGetIntegerv(GL_FRAMEBUFFER_BINDING,&oldFbo);glGetIntegerv(GL_CURRENT_PROGRAM,&oldProgram);glGetIntegerv(GL_MATRIX_MODE,&oldMode);
  glPushAttrib(GL_ALL_ATTRIB_BITS);
  glBindFramebuffer(GL_FRAMEBUFFER,0);glViewport(0,0,width,height);glUseProgram(0);
  glDisable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);glDisable(GL_BLEND);glDisable(GL_SCISSOR_TEST);
  for(int i=0;i<4;i++){glActiveTexture(GL_TEXTURE0+i);glDisable(GL_TEXTURE_2D);glDisable(GL_TEXTURE_CUBE_MAP);}glActiveTexture(GL_TEXTURE0);
  glColorMask(1,1,1,1);glClearColor(.015f,.025f,.04f,1);glClear(GL_COLOR_BUFFER_BIT);
  glMatrixMode(GL_PROJECTION);glPushMatrix();glLoadIdentity();glOrtho(0,width,height,0,-1,1);
  glMatrixMode(GL_MODELVIEW);glPushMatrix();glLoadIdentity();
  float left=width*.15f,top=height*.48f,span=width*.7f;
  glColor3f(.13f,.18f,.22f);glBegin(GL_QUADS);
  glVertex2f(left,top);glVertex2f(left+span,top);glVertex2f(left+span,top+16);glVertex2f(left,top+16);glEnd();
  glColor3f(.25f,.75f,.85f);glBegin(GL_QUADS);
  glVertex2f(left,top);glVertex2f(left+span*bootProgressValue,top);glVertex2f(left+span*bootProgressValue,top+16);glVertex2f(left,top+16);glEnd();
  glColor3f(.86f,.94f,.97f);label(left,top-60,"POOR MAN'S SKY",2);
  char text[160];snprintf(text,sizeof(text),"%s",stage);label(left,top-25,text,1.2f);
  snprintf(text,sizeof(text),"%.0F / 100",bootProgressValue*100);label(left+span-72,top+32,text,1);
  if(detail)label(left,top+32,detail,1);
  SDL_GL_SwapWindow(window);
  glMatrixMode(GL_MODELVIEW);glPopMatrix();glMatrixMode(GL_PROJECTION);glPopMatrix();glMatrixMode(oldMode);
  glBindFramebuffer(GL_FRAMEBUFFER,oldFbo);glUseProgram(glIsProgram(oldProgram)?oldProgram:0);glPopAttrib();
}
