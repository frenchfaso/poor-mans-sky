// SPDX-License-Identifier: MPL-2.0
/* Real tiny draws on the current driver, including variants not yet visible.
 * No shader binaries are transferred between the Mac and the RV350. */
static void warmShaders(void) {
 Uint32 start=SDL_GetTicks();
 moonInit();
 if(cloudsEnabled){cloudInit();cloudPrepareCatalog();}
 if(sunShadows){sunCastP=program("sun-cast.vert","sun-cast.frag");sunGroundP=program("sun-ground.vert","sun-ground.frag");}
 if(reflectionEnabled)reflectionBlurP=program("bake.vert","reflection-blur.frag");
 overdrawP=program("nature.vert","overdraw.frag");
 GLint oldFbo,oldProgram;glGetIntegerv(GL_FRAMEBUFFER_BINDING,&oldFbo);glGetIntegerv(GL_CURRENT_PROGRAM,&oldProgram);
 glPushAttrib(GL_ALL_ATTRIB_BITS);glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT);
 Target scratch=target(8,8,1,0);glBindFramebuffer(GL_FRAMEBUFFER,scratch.fbo);glViewport(0,0,8,8);
 GLuint white;unsigned char pixel[4]={255,255,255,255};glActiveTexture(GL_TEXTURE0);glGenTextures(1,&white);glBindTexture(GL_TEXTURE_2D,white);
 glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,pixel);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
 glBindTexture(GL_TEXTURE_CUBE_MAP,skyCube);
 glDisable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);glDisable(GL_BLEND);glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
 glColorMask(1,1,1,1);glBindBuffer(GL_ARRAY_BUFFER,0);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);
 glMatrixMode(GL_PROJECTION);glPushMatrix();glLoadIdentity();glMatrixMode(GL_MODELVIEW);glPushMatrix();glLoadIdentity();
 int count=0;
 for(int i=0;i<linkedProgramCount;i++) {
  GLuint p=linkedPrograms[i];if(!glIsProgram(p))continue;
  glUseProgram(p);u1(p,"exposure",1);u1(p,"morph",1);u1(p,"transitionMix",1);
  u2(p,"fadeRange",0,1);u2(p,"scale",1,1);u3(p,"sun",v3(0,0,1));
  u3(p,"eye",v3(0,0,2));u3(p,"ambientUp",v3(0,0,1));
  /* All active samplers start at unit zero; no current program mixes 2D/cube. */
  GLint uniforms=0;glGetProgramiv(p,GL_ACTIVE_UNIFORMS,&uniforms);
  for(int j=0;j<uniforms;j++){char name[128];GLsizei length;GLint size;GLenum type;
   glGetActiveUniform(p,j,sizeof(name),&length,&size,&type,name);
   if(type==GL_SAMPLER_2D || type==GL_SAMPLER_CUBE)glUniform1i(glGetUniformLocation(p,name),0);
  }
  for(int mode=0;mode<2;mode++)for(int fog=0;fog<2;fog++)
   if(p==landPrograms[mode][fog] || p==fadePrograms[mode][fog]) {
    tex(p,"atlas",0,atlasTex[0]);tex(p,"detailTex",1,detailMap.tex);tex(p,"transitionAtlas",2,transitionAtlas?transitionAtlas:atlasTex[0]);
   }
  if(p==reflectionLandP) {
   tex(p,"atlas",0,atlasTex[0]);
   glUniform4f(uniformLocation(p,"reflectionPlane"),0,0,1,1);
  }
  if(p==actorP)tex(p,"materialTex",0,actorTex);
  if(p==natureP || p==natureFadeP || p==sunCastP){tex(p,"foliageTex",0,foliageTex);tex(p,"lodMask",1,natureFadeMask);}
  if(p==moonP || p==moonAirP)tex(p,"rockTex",0,moonRock);
  if(p==cloudP)tex(p,"cloudTex",0,cloudTex);
  if(p==waterP){tex(p,"detailTex",0,detailMap.tex);tex(p,"reflectionTex",1,white);}
  if(p==postQualityP || p==postPerformanceP || p==postLowP || p==brightP || p==blurP || p==reflectionBlurP){tex(p,"sceneTex",0,scene.tex);tex(p,"bloomTex",1,glow[0].tex);}
  glBegin(GL_TRIANGLES);glColor4f(1,1,1,1);glNormal3f(0,0,1);
  glTexCoord3f(0,0,-1);glVertex3f(-1,-1,-.2f);glTexCoord3f(1,0,-1);glVertex3f(1,-1,-.2f);glTexCoord3f(.5,1,-1);glVertex3f(0,1,-.2f);glEnd();
  gpuCheckpoint("shader-warmup-submit");glFinish();gpuCheckpoint("shader-warmup-finished");checkGL("shader warmup");count++;
 }
 glMatrixMode(GL_MODELVIEW);glPopMatrix();glMatrixMode(GL_PROJECTION);glPopMatrix();glMatrixMode(GL_MODELVIEW);
 glBindFramebuffer(GL_FRAMEBUFFER,oldFbo);glUseProgram(glIsProgram(oldProgram)?oldProgram:0);glPopClientAttrib();glPopAttrib();
 glDeleteTextures(1,&white);glDeleteTextures(1,&scratch.tex);glDeleteFramebuffers(1,&scratch.fbo);glDeleteRenderbuffers(1,&scratch.depth);
 printf("SHADER_WARMUP programs=%d milliseconds=%u\n",count,SDL_GetTicks()-start);fflush(stdout);
}
