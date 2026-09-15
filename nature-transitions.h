// SPDX-License-Identifier: MPL-2.0
/* Complementary screen-door coverage, only on meshes currently changing LOD.
 * Stable vegetation keeps its original shader and persistent batches. */
#define NATURE_FADE_MS 650
#define NATURE_FADE_LIMIT 24
static void natureFadeInit(void) {
  natureFadeP=program("nature.vert","nature-fade.frag");
  unsigned char mask[64];
  for(int y=0;y<8;y++)for(int x=0;x<8;x++) {
    int rank=0;
    for(int bit=0;bit<3;bit++)rank|=((((x>>bit)^(y>>bit))&1)*2+((y>>bit)&1))<<(2*(2-bit));
    mask[y*8+x]=(unsigned char)(rank*4+2);
  }
  glGenTextures(1,&natureFadeMask);glBindTexture(GL_TEXTURE_2D,natureFadeMask);
  glTexImage2D(GL_TEXTURE_2D,0,GL_LUMINANCE8,8,8,0,GL_LUMINANCE,GL_UNSIGNED_BYTE,mask);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
}
static void natureFadeBegin(NatureCell *c) {
  if(preloading || natureFading>=NATURE_FADE_LIMIT || (!c->count && !c->pendingCount))return;
  c->oldVbo=c->vbo;c->oldEbo=c->ebo;c->oldBytes=c->gpuBytes;
  c->oldCount=c->count;c->oldLod=c->lod;c->oldCenter=c->center;
  if(c->count && !c->oldVbo) {
    glGenBuffers(1,&c->oldVbo);glBindBuffer(GL_ARRAY_BUFFER,c->oldVbo);
    glBufferData(GL_ARRAY_BUFFER,c->residentUnique*sizeof(NaturePacked),c->resident,GL_STATIC_DRAW);
    glGenBuffers(1,&c->oldEbo);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,c->oldEbo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,c->count*sizeof(uint16_t),c->resident+c->residentUnique*sizeof(NaturePacked),GL_STATIC_DRAW);
    c->oldBytes=c->residentUnique*sizeof(NaturePacked)+c->count*sizeof(uint16_t);
    natureGPUBytes+=c->oldBytes;
  }
  c->vbo=c->ebo=0;c->gpuBytes=0;
  c->fading=1;c->fadeStart=SDL_GetTicks();natureFading++;natureFadeStarted++;
}
static void natureFadeFinish(int id) {
  NatureCell *c=&nature[id];if(!c->fading)return;
  glDeleteBuffers(1,&c->oldVbo);glDeleteBuffers(1,&c->oldEbo);
  natureGPUBytes-=c->oldBytes;c->oldBytes=0;c->oldVbo=c->oldEbo=0;
  c->fading=0;natureFading--;
  if(c->lod>=2) {
    glDeleteBuffers(1,&c->vbo);glDeleteBuffers(1,&c->ebo);
    natureGPUBytes-=c->gpuBytes;c->gpuBytes=0;c->vbo=c->ebo=0;
    natureAttach(id);
  }
}
