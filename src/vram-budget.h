// SPDX-License-Identifier: MPL-2.0
/* App-side estimate, not a driver residency query. Eight MiB remain outside
 * this budget for scanout/driver allocations and transfer transients. */
#define VRAM_BUDGET (56u*1048576u)
static size_t vramTargetBytes(Target t){return (size_t)t.w*t.h*(t.depth?8:4);}
static size_t vramEstimate(void) {
 size_t bytes=(waterSeamVBO?sizeof(waterGrids[0]):0)+8u*1048576u+STAR_BYTES+bcMipBytes(1024,4)+2u*1048576u+256u*1024u;
 bytes+=vramTargetBytes(scene)+vramTargetBytes(glow[0])+vramTargetBytes(glow[1]);
 bytes+=vramTargetBytes(noiseMap)+vramTargetBytes(detailMap)+vramTargetBytes(sunMap);
 bytes+=vramTargetBytes(reflectionMap)+vramTargetBytes(reflectionBlur);
 bytes+=(size_t)resident*NV*sizeof(Vertex)+natureGPUBytes;
 for(int i=0;i<MOON_SLOTS;i++)if(moonPatches[i].vbo)bytes+=MOON_VERTS*sizeof(MoonVertex);
 return bytes;
}
static int vramReserve(size_t incoming) {
 if(vramEstimate()+incoming<=VRAM_BUDGET)return 1;
 if(!nearMoon(eye))for(int i=0;i<MOON_SLOTS && vramEstimate()+incoming>VRAM_BUDGET;i++) {
  MoonPatch *p=&moonPatches[i];if(p->vbo && p->level>1 && p->stamp<frameNo) {
   glDeleteBuffers(1,&p->vbo);p->vbo=0;p->valid=0;
  }
 }
 SDL_LockMutex(mutex);
 while(vramEstimate()+incoming>VRAM_BUDGET) {
  int victim=-1;float best=-1;
  for(int slot=6;slot<SLOTS;slot++) {
   int id=owners[slot];if(id<6 || nodes[id].wanted>=frameNo-2)continue;
   Node *n=&nodes[id];V3 delta=add(n->center,mul(eye,-1));
   /* Visibility wins over age; RAM/disk payloads are never discarded here. */
   float score=(n->visible?0:1e12f)+(frameNo-n->wanted)*10000.f+sqrtf(dot(delta,delta));
   if(score>best){best=score;victim=slot;}
  }
  if(victim<0)break;
  Node *n=&nodes[owners[victim]];glDeleteBuffers(1,&n->vbo);glDeleteQueries(1,&n->query);
  n->vbo=n->query=0;n->queryPending=0;n->testedEpoch=0;n->slot=-1;n->state=n->pixels?2:0;
  owners[victim]=-1;resident--;evicted++;
 }
 SDL_UnlockMutex(mutex);return vramEstimate()+incoming<=VRAM_BUDGET;
}
