// SPDX-License-Identifier: MPL-2.0
/* Compare the formerly shared shader with the two specialized programs. */
#define main poor_mans_sky_application_main
#include "../src/poor-mans-sky.c"
#undef main
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL reflection %d: %s\n",__LINE__,#x); return 1; } } while (0)
int main(void) {
  CHECK(!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER));
  window=SDL_CreateWindow("Reflection specialization",0,0,64,64,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
  CHECK(window);context=SDL_GL_CreateContext(window);CHECK(context);
  GLuint reference=program("globe.vert","../tools/reflection-reference.frag");
  GLuint opaque=program("globe.vert","globe-fast.frag");
  GLuint reflection=program("globe.vert","globe-reflection.frag");
  unsigned char texture[16*16*3],pixels[2][64*64*3];
  for(unsigned i=0;i<sizeof(texture);i++)texture[i]=32+(i*37)%192;
  GLuint atlas;glGenTextures(1,&atlas);glBindTexture(GL_TEXTURE_2D,atlas);
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGB8,16,16,0,GL_RGB,GL_UNSIGNED_BYTE,texture);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
  glViewport(0,0,64,64);glDisable(GL_DEPTH_TEST);glDisable(GL_BLEND);glDisable(GL_DITHER);glDisable(GL_CULL_FACE);
  glMatrixMode(GL_PROJECTION);glLoadIdentity();glMatrixMode(GL_MODELVIEW);glLoadIdentity();
  unsigned cases=0;
  for(int reflected=0;reflected<2;reflected++)for(int plane=0;plane<3;plane++)for(int light=0;light<2;light++) {
    for(int mode=0;mode<2;mode++) {
      GLuint p=mode?(reflected?reflection:opaque):reference;glUseProgram(p);tex(p,"atlas",0,atlas);
      u1(p,"reflectionPass",reflected);u1(p,"fogHeightFactor",reflected?0:.8f);
      glUniform4f(uniformLocation(p,"reflectionPlane"),1,0,0,(plane-1)*2);
      u2(p,"page",.2f,.3f);u3(p,"eye",v3(0,0,400));
      u3(p,"sun",norm(v3(.3f,.2f,1)));u3(p,"ambientUp",v3(0,0,1));
      u3(p,"ambientLight",v3(.12f,.16f,.2f));u3(p,"sunLight",mul(v3(.7f,.6f,.5f),light?.1f:1));
      u3(p,"fogColor",v3(.3f,.2f,.1f));u1(p,"exposure",1.1f);
      glClearColor(.1f,.2f,.3f,0);glClear(GL_COLOR_BUFFER_BIT);
      glBegin(GL_QUADS);
      glNormal3f(0,0,1);glTexCoord2f(0,0);glVertex3f(-1,-1,0);
      glNormal3f(.6f,0,.8f);glTexCoord2f(1,0);glVertex3f(1,-1,0);
      glNormal3f(0,.6f,.8f);glTexCoord2f(1,1);glVertex3f(1,1,0);
      glNormal3f(-.6f,0,.8f);glTexCoord2f(0,1);glVertex3f(-1,1,0);
      glEnd();glReadPixels(0,0,64,64,GL_RGB,GL_UNSIGNED_BYTE,pixels[mode]);
    }
    unsigned sum=0,max=0;for(unsigned i=0;i<sizeof(pixels[0]);i++){unsigned d=abs((int)pixels[0][i]-(int)pixels[1][i]);sum+=d;if(d>max)max=d;}
    printf("REFLECTION reflected=%d plane=%d light=%d max=%u mean=%.6f\n",reflected,plane,light,max,sum/(double)sizeof(pixels[0]));
    CHECK(max<=3 && sum<sizeof(pixels[0])/2);cases++;
  }
  CHECK(glGetError()==GL_NO_ERROR);
  printf("PASS %u specialized shader comparisons, including fully clipped/crossing/visible planes\n",cases);
  SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();return 0;
}
