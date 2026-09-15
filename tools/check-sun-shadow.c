// SPDX-License-Identifier: MPL-2.0
#define main poor_mans_sky_application_main
#include "../src/poor-mans-sky.c"
#undef main
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %d %s\n",__LINE__,#x);return 1;}}while(0)
int main(void){
 CHECK(!SDL_Init(SDL_INIT_VIDEO));window=SDL_CreateWindow("shadow check",0,0,64,64,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);CHECK(window);context=SDL_GL_CreateContext(window);CHECK(context);
 GLuint cast=program("sun-cast.vert","sun-cast.frag"),receive=program("sun-ground.vert","sun-ground.frag");
 CHECK(cast && receive);Target t=target(256,256,1,0);
 GLuint white;unsigned char pixel[4]={255,255,255,255};glGenTextures(1,&white);glBindTexture(GL_TEXTURE_2D,white);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,pixel);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
 glBindFramebuffer(GL_FRAMEBUFFER,t.fbo);glViewport(0,0,256,256);glClearColor(1,1,1,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);glUseProgram(cast);tex(cast,"foliageTex",0,white);u1(cast,"cutoff",.4);
 glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrtho(-1,1,-1,1,-128,128);glMatrixMode(GL_MODELVIEW);glLoadIdentity();
 glBegin(GL_QUADS);glTexCoord2f(.5,.5);glVertex3f(-1,-1,32);glVertex3f(1,-1,32);glVertex3f(1,1,32);glVertex3f(-1,1,32);glEnd();
 glReadPixels(128,128,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);float depth=pixel[0]/255.f+pixel[1]/65025.f;CHECK(fabsf(depth-.375f)<.0001f);
 Target out=target(64,64,1,0);glBindFramebuffer(GL_FRAMEBUFFER,out.fbo);glViewport(0,0,64,64);glUseProgram(receive);tex(receive,"shadowTex",0,t.tex);u3(receive,"patchDelta",v3(0,0,0));u3(receive,"lightRight",v3(1,0,0));u3(receive,"lightUp",v3(0,1,0));u3(receive,"lightDir",v3(0,0,1));u1(receive,"strength",.38);
 for(int pass=0;pass<2;pass++){
 u3(receive,"patchDelta",v3(0,0,pass?40:0));glBegin(GL_QUADS);glVertex3f(-1,-1,0);glVertex3f(1,-1,0);glVertex3f(1,1,0);glVertex3f(-1,1,0);glEnd();glReadPixels(32,32,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
 CHECK(pass?pixel[0]>250:abs(pixel[0]-158)<3);
 }
 cameraEye=v3(0,RADIUS+5,0);sun=v3(0,-1,0);sunReady=1;sunShadowUpdate();CHECK(!sunReady);
 sunShadows=0;sunReady=1;sunShadowUpdate();CHECK(!sunReady);
 CHECK(glGetError()==GL_NO_ERROR);puts("PASS RG depth encoding, shadowed ground, receiver above caster stays lit, night/toggle disable, GLSL linkage");SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();return 0;
}
