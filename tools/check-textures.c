// SPDX-License-Identifier: MPL-2.0
#define main poor_mans_sky_application_main
#include "../src/poor-mans-sky.c"
#undef main
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL texture %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void) {
  CHECK(!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER));
  window=SDL_CreateWindow("Texture transition",0,0,64,64,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
  CHECK(window);context=SDL_GL_CreateContext(window);CHECK(context);
  unsigned char rgba[PAGE*PAGE*4],page[PAGE_BYTES],atlas[1024*512/2];
  unsigned char colors[4][3]={{255,0,0},{0,255,0},{0,0,255},{255,255,255}};
  for(int i=0;i<MAXNODE;i++)previousIndex[i]=-1;
  for(int i=0;i<5;i++) {
    Node *n=&nodes[i];n->face=4;n->level=i?1:0;n->x=i?(i-1)%2:0;n->y=i?(i-1)/2:0;
    n->size=RADIUS*2/(1<<n->level);n->center=v3(0,0,RADIUS);n->used=frameNo=100;
    for(int j=0;j<PAGE*PAGE;j++){memcpy(rgba+j*4,colors[i?i-1:0],3);rgba[j*4+3]=255;}
    n->pixels=compressPage(rgba);
  }
  selectedCount=1;selected[0]=0;previousCount=4;
  for(int i=0;i<4;i++)previousCover[i]=i+1;
  cullingMode=0;cameraEye=v3(0,0,RADIUS+50);cullForward=v3(0,0,-1);
  prepareTextureFades();CHECK(textureFadeFor(0)==0);
  glBindTexture(GL_TEXTURE_2D,transitionAtlas);glGetCompressedTexImage(GL_TEXTURE_2D,0,atlas);
  for(int row=0;row<32;row++)memcpy(page+row*32*8,atlas+row*256*8,32*8);
  for(int i=0;i<4;i++) {
    unsigned char pixel[4];pagePixel(page,.25f+(i%2)*.5f,.25f+(i/2)*.5f,pixel);
    for(int c=0;c<3;c++)CHECK(abs(pixel[c]-colors[i][c])<=8);
  }
  for(int i=0;i<TEXTURE_FADES;i++)textureFades[i].id=-1;
  nodes[0].level=2;nodes[0].size=RADIUS*.5f;nodes[1].level=0;nodes[1].x=nodes[1].y=0;
  previousCount=1;previousCover[0]=1;prepareTextureFades();CHECK(textureFadeFor(0)==0);
  glBindTexture(GL_TEXTURE_2D,transitionAtlas);glGetCompressedTexImage(GL_TEXTURE_2D,0,atlas);
  for(int row=0;row<32;row++)memcpy(page+row*32*8,atlas+row*256*8,32*8);
  unsigned char pixel[4];pagePixel(page,.5f,.5f,pixel);CHECK(pixel[0]>245&&pixel[1]<8&&pixel[2]<8);
  CHECK(glGetError()==GL_NO_ERROR);
  puts("PASS texture coarsening mosaic and refinement snapshot, BC1 GPU readback");
  SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();return 0;
}
