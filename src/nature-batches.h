// SPDX-License-Identifier: MPL-2.0
/* Persistent spatial batches of far impostors. CPU assembly happens only when
 * streamed membership changes; no per-frame geometry expansion/upload. */
#define NATURE_GROUPS 2048
typedef struct {
  int head, face, x, y, dirty, count, trees, rocks;
  GLuint vbo;
  size_t bytes;
  V3 center, boundCenter, boundHalf;
} NatureGroup;
static NatureGroup natureGroups[NATURE_GROUPS];
static int natureGroupUsed, natureGroupDraws, natureGroupRebuilds;
static void natureDetach(int id) {
  NatureCell *c=&nature[id];
  if (!c->group) return;
  NatureGroup *g=&natureGroups[c->group-1];
  int *link=&g->head;
  while (*link && *link!=id+1) link=&nature[*link-1].nextGroup;
  if (*link) *link=c->nextGroup;
  c->group=c->nextGroup=0;g->dirty=1;
}
static void natureAttach(int id) {
  NatureCell *c=&nature[id];
  if (c->lod<2 || !c->count) return;
  int empty=-1, k;
  for(k=0;k<natureGroupUsed;k++) {
    NatureGroup *g=&natureGroups[k];
    if(g->head && g->face==c->face && g->x==(c->x>>3) && g->y==(c->y>>3)) break;
    if(!g->head && empty<0) empty=k;
  }
  if(k==natureGroupUsed) {
    if(empty>=0) k=empty;
    else if(natureGroupUsed<NATURE_GROUPS) natureGroupUsed++;
    else die("nature group capacity");
    NatureGroup *g=&natureGroups[k];
    g->face=c->face;g->x=c->x>>3;g->y=c->y>>3;
  }
  NatureGroup *g=&natureGroups[k];
  c->group=k+1;c->nextGroup=g->head;g->head=id+1;g->dirty=1;
}
static void natureRebuildGroups(void) {
  static Uint32 retryAt;
  if((Sint32)(retryAt-SDL_GetTicks())>0)return;
  for(int k=0;k<natureGroupUsed;k++) {
    NatureGroup *g=&natureGroups[k];
    if(!g->dirty) continue;
    size_t required=0;for(int link=g->head;link;link=nature[link-1].nextGroup)required+=(size_t)nature[link-1].count*sizeof(NaturePacked);
    if(required>g->bytes && !vramReserve(required-g->bytes))continue;
    NaturePacked *v=required?streamAlloc(required):NULL;
    if(required && !v){natureMemoryPressure=1;retryAt=SDL_GetTicks()+500;return;}
    g->dirty=0;g->count=g->trees=g->rocks=0;
    for(int link=g->head;link;link=nature[link-1].nextGroup) {
      NatureCell *c=&nature[link-1];
      g->count+=c->count;g->trees+=c->trees;g->rocks+=c->rocks;
    }
    natureGPUBytes-=g->bytes;g->bytes=0;
    if(!g->count) { glDeleteBuffers(1,&g->vbo);g->vbo=0;continue; }
    g->center=nature[g->head-1].center;

    V3 lo=v3(1e30f,1e30f,1e30f),hi=mul(lo,-1);int n=0;
    for(int link=g->head;link;link=nature[link-1].nextGroup) {
      NatureCell *c=&nature[link-1];
      NaturePacked *src=(NaturePacked*)c->resident;
      const uint16_t *ix=(const uint16_t*)(c->resident+c->residentUnique*sizeof(*src));
      V3 delta=add(c->center,mul(g->center,-1));
      for(int i=0;i<c->count;i++) {
        NaturePacked q=src[ix[i]];q.p=add(q.p,delta);v[n++]=q;
        lo=v3(fminf(lo.x,q.p.x),fminf(lo.y,q.p.y),fminf(lo.z,q.p.z));
        hi=v3(fmaxf(hi.x,q.p.x),fmaxf(hi.y,q.p.y),fmaxf(hi.z,q.p.z));
      }
    }
    g->boundCenter=add(g->center,mul(add(lo,hi),.5f));
    g->boundHalf=add(mul(add(hi,mul(lo,-1)),.5f),v3(.2f,.2f,.2f));
    if(!g->vbo) glGenBuffers(1,&g->vbo);
    glBindBuffer(GL_ARRAY_BUFFER,g->vbo);
    g->bytes=g->count*sizeof(*v);
    glBufferData(GL_ARRAY_BUFFER,g->bytes,v,GL_STATIC_DRAW);
    natureGPUBytes+=g->bytes;free(v);natureGroupRebuilds++;
  }
}
static void naturePointers(GLuint vbo) {
  glBindBuffer(GL_ARRAY_BUFFER,vbo);
  glVertexPointer(3,GL_FLOAT,sizeof(NaturePacked),(void*)offsetof(NaturePacked,p));
  glNormalPointer(GL_BYTE,sizeof(NaturePacked),(void*)offsetof(NaturePacked,n));
  glColorPointer(4,GL_UNSIGNED_BYTE,sizeof(NaturePacked),(void*)offsetof(NaturePacked,r));
  glTexCoordPointer(2,GL_FLOAT,sizeof(NaturePacked),(void*)offsetof(NaturePacked,u));
}
