// SPDX-License-Identifier: MPL-2.0
#define main poor_mans_sky_application_main
#include "../src/poor-mans-sky.c"
#undef main
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL detail %d: %s\n",__LINE__,#x); return 1; } } while (0)
static GLuint testTexture(int seed) {
  unsigned char pixels[16*16*3];
  for (unsigned i=0;i<sizeof(pixels);i++) pixels[i]=32+(i*37+seed*53)%192;
  GLuint t;glGenTextures(1,&t);glBindTexture(GL_TEXTURE_2D,t);
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGB8,16,16,0,GL_RGB,GL_UNSIGNED_BYTE,pixels);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
  return t;
}
int main(void) {
  Node n={0};n.boundRadius=10;
  cameraEye=v3(0,0,110);CHECK(!terrainFastMode(&n));
  cameraEye=v3(0,0,111);CHECK(!terrainFastMode(&n));
  cameraEye=v3(0,0,112);CHECK(terrainFastMode(&n));
  n.boundRadius=200;CHECK(!terrainFastMode(&n)); /* Large patch crossing detail range. */
  performanceMode=1;CHECK(terrainFastMode(&n));performanceMode=0;
  CHECK(!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER));
  window=SDL_CreateWindow("Terrain detail equivalence",0,0,64,64,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
  CHECK(window);context=SDL_GL_CreateContext(window);CHECK(context);
  GLuint atlas=testTexture(1),detail=testTexture(2),transition=testTexture(3);
  glViewport(0,0,64,64);glDisable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);glDisable(GL_BLEND);glDisable(GL_DITHER);
  glMatrixMode(GL_PROJECTION);glLoadIdentity();glMatrixMode(GL_MODELVIEW);glLoadIdentity();
  unsigned char pixels[2][64*64*3];
  for(int fogMode=0;fogMode<2;fogMode++)for(int fade=0;fade<2;fade++) {
    GLuint programs[2];
    for(int mode=0;mode<2;mode++)
      programs[mode]=program(fogMode?"globe-cpu-fog.vert":"globe.vert",fade?(mode?"globe-fade-fast.frag":"globe-fade.frag"):(mode?"globe-fast.frag":"globe.frag"));
    for(int pose=0;pose<3;pose++) {
      for(int mode=0;mode<2;mode++) {
        GLuint p=programs[mode];glUseProgram(p);
        tex(p,"atlas",0,atlas);tex(p,"detailTex",1,detail);tex(p,"transitionAtlas",2,transition);
        u2(p,"page",.2f,.3f);u2(p,"transitionPage",.1f,.4f);u1(p,"transitionMix",pose*.5f);
        u3(p,"eye",v3(0,0,102+pose*1000));u3(p,"detailOrigin",v3(.7f,.1f,.8f));
        u3(p,"sun",norm(v3(.3f,.2f,1)));u3(p,"ambientUp",v3(0,0,1));
        u3(p,"ambientLight",v3(.2f,.15f,.1f));u3(p,"sunLight",v3(.7f,.6f,.5f));
        u3(p,"fogColor",v3(.2f,.3f,.4f));u1(p,"exposure",1.1f);u1(p,"fogHeightFactor",.8f);
        glUniform4f(uniformLocation(p,"fogPlane"),.02f,.01f,0,.1f);
        u1(p,"reflectionPass",0);
        glClear(GL_COLOR_BUFFER_BIT);
        glBegin(GL_QUADS);
        glNormal3f(0,0,1);glTexCoord2f(0,0);glVertex3f(-1,-1,0);
        glNormal3f(.6f,0,.8f);glTexCoord2f(1,0);glVertex3f(1,-1,0);
        glNormal3f(0,.6f,.8f);glTexCoord2f(1,1);glVertex3f(1,1,0);
        glNormal3f(-.6f,0,.8f);glTexCoord2f(0,1);glVertex3f(-1,1,0);
        glEnd();glReadPixels(0,0,64,64,GL_RGB,GL_UNSIGNED_BYTE,pixels[mode]);
        CHECK(glGetError()==GL_NO_ERROR);
      }
      unsigned sum=0,max=0,lit=0;
      for(unsigned i=0;i<sizeof(pixels[0]);i++) {
        unsigned d=abs((int)pixels[0][i]-(int)pixels[1][i]);sum+=d;if(d>max)max=d;lit+=pixels[0][i];
      }
      printf("DETAIL fog=%d fade=%d pose=%d max_error=%u mean_error=%.5f\n",fogMode,fade,pose,max,sum/(double)sizeof(pixels[0]));
      CHECK(lit>1000 && max<=3 && sum<sizeof(pixels[0])/2);
    }
    for(int mode=0;mode<2;mode++)glDeleteProgram(programs[mode]);
  }
  puts("PASS conservative distance selection and 12 shader comparisons (8-bit framebuffer)");
  SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();return 0;
}
