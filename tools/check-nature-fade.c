// SPDX-License-Identifier: MPL-2.0
#define main poor_mans_sky_application_main
#include "../poor-mans-sky.c"
#undef main
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %d %s\n",__LINE__,#x);return 1;}}while(0)
static void testQuad(void){glBegin(GL_QUADS);glNormal3f(0,1,0);glTexCoord2f(.05,.05);glVertex3f(-1,-1,0);glVertex3f(1,-1,0);glVertex3f(1,1,0);glVertex3f(-1,1,0);glEnd();}
int main(void){
 CHECK(!SDL_Init(SDL_INIT_VIDEO));window=SDL_CreateWindow("LOD fade test",0,0,64,64,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);CHECK(window);context=SDL_GL_CreateContext(window);CHECK(context);
 natureFadeInit();Target t=target(8,8,0,0);glBindFramebuffer(GL_FRAMEBUFFER,t.fbo);glViewport(0,0,8,8);
 GLuint white;unsigned char rgb[4]={255,255,255,255};glGenTextures(1,&white);glBindTexture(GL_TEXTURE_2D,white);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,rgb);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
 glUseProgram(natureFadeP);tex(natureFadeP,"foliageTex",0,white);tex(natureFadeP,"lodMask",1,natureFadeMask);glActiveTexture(GL_TEXTURE0);
 u3(natureFadeP,"ambientLight",v3(1,1,1));u3(natureFadeP,"sunLight",v3(0,0,0));u3(natureFadeP,"offset",v3(0,0,0));u1(natureFadeP,"exposure",1);u1(natureFadeP,"alphaCutoff",.4);
 glMatrixMode(GL_PROJECTION);glLoadIdentity();glMatrixMode(GL_MODELVIEW);glLoadIdentity();glDisable(GL_DEPTH_TEST);glDisable(GL_BLEND);
 unsigned char pixels[8*8*4];
 for(int step=0;step<=4;step++){
 float phase=step*.25f;glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);
 glColor3f(1,0,0);u2(natureFadeP,"fadeRange",phase,1);testQuad();
 glColor3f(0,0,1);u2(natureFadeP,"fadeRange",0,phase);testQuad();
 glReadPixels(0,0,8,8,GL_RGBA,GL_UNSIGNED_BYTE,pixels);int red=0,blue=0;
 for(int i=0;i<64;i++){red+=pixels[i*4]>240;blue+=pixels[i*4+2]>240;}
 CHECK(red+blue==64 && blue==step*16);
 }
 NatureCell *c=&nature[0];*c=(NatureCell){.face=4,.x=8,.y=8,.lod=0,.count=3,.residentUnique=3,.pendingCount=3};
 c->resident=calloc(1,3*sizeof(NaturePacked)+6);uint16_t *ix=(uint16_t*)(c->resident+3*sizeof(NaturePacked));ix[0]=0;ix[1]=1;ix[2]=2;
 glGenBuffers(1,&c->vbo);glBindBuffer(GL_ARRAY_BUFFER,c->vbo);glBufferData(GL_ARRAY_BUFFER,3*sizeof(NaturePacked),c->resident,GL_STATIC_DRAW);
 glGenBuffers(1,&c->ebo);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,c->ebo);glBufferData(GL_ELEMENT_ARRAY_BUFFER,6,ix,GL_STATIC_DRAW);
 c->gpuBytes=3*sizeof(NaturePacked)+6;natureGPUBytes=c->gpuBytes;
 natureFadeBegin(c);CHECK(natureFading==1 && c->oldVbo && !c->vbo);
 c->lod=2;natureFadeFinish(0);natureRebuildGroups();CHECK(natureFading==0 && c->group && natureGPUBytes==3*sizeof(NaturePacked));
 natureDetach(0);natureRebuildGroups();CHECK(natureGPUBytes==0);free(c->resident);c->resident=NULL;
 CHECK(glGetError()==GL_NO_ERROR);puts("PASS complementary LOD coverage at 0/25/50/75/100 percent: no holes or overlap; retained mesh and batch cleanup");SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();return 0;
}
