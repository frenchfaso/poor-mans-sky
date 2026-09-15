// SPDX-License-Identifier: MPL-2.0
#define main poor_mans_sky_application_main
#include "../src/poor-mans-sky.c"
#undef main
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void) {
  CHECK(!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER));
  window=SDL_CreateWindow("Nature batches",0,0,64,64,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
  CHECK(window);context=SDL_GL_CreateContext(window);CHECK(context);
  for(int i=0;i<16;i++) {
    NatureCell *c=&nature[i];
    *c=(NatureCell){.face=4,.x=8+i%4,.y=8+i/4,.lod=2,.count=3,.residentUnique=3};
    c->center=v3(i*24,200000,0);c->resident=calloc(1,3*sizeof(NaturePacked)+6);
    NaturePacked *v=(NaturePacked*)c->resident;uint16_t *ix=(uint16_t*)(v+3);
    for(int j=0;j<3;j++){v[j].p=v3(j==1,j==2,0);ix[j]=j;}
    natureAttach(i);
  }
  natureRebuildGroups();CHECK(natureGroupUsed==1 && natureGroups[0].count==48);
  NaturePacked out[48];glBindBuffer(GL_ARRAY_BUFFER,natureGroups[0].vbo);
  glGetBufferSubData(GL_ARRAY_BUFFER,0,sizeof(out),out);
  int n=0;
  for(int link=natureGroups[0].head;link;link=nature[link-1].nextGroup) {
    NatureCell *c=&nature[link-1];NaturePacked *v=(NaturePacked*)c->resident;
    for(int j=0;j<3;j++) {
      V3 expected=add(v[j].p,add(c->center,mul(natureGroups[0].center,-1)));
      CHECK(!memcmp(&out[n++].p,&expected,sizeof(V3)));
    }
  }
  int revision=natureGroupRebuilds;natureRebuildGroups();CHECK(revision==natureGroupRebuilds);
  natureDetach(7);nature[7].lod=0;natureRebuildGroups();CHECK(natureGroups[0].count==45);
  for(int i=0;i<16;i++){natureDetach(i);free(nature[i].resident);}
  natureRebuildGroups();CHECK(natureGPUBytes==0 && !natureGroups[0].vbo);
  CHECK(glGetError()==GL_NO_ERROR);
  puts("PASS 16 cells -> 1 draw, exact GPU positions, stable reuse, LOD removal, empty group cleanup");
  SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();return 0;
}
