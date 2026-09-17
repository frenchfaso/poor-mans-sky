// SPDX-License-Identifier: MPL-2.0
/* Stitch the rendered sea surface, not the displaced terrain underneath it.
 * One small streaming VBO; CPU grids are bounded by terrain residency slots. */
#define WATER_NV ((PATCH+1)*(PATCH+1))
static Vertex waterGrids[SLOTS][WATER_NV];
static GLuint waterSeamVBO;
typedef struct { int id, slot, lod, wet; } WaterState;
static WaterState waterState[1024];
static uint64_t waterRevision;
static int waterStateCount = -1, waterPrepared, waterReused;
static int waterIdFirst(const void *a,const void *b) {
 return *(const int*)a-*(const int*)b;
}
static int waterCoarseFirst(const void *a,const void *b) {
 int order=coarseFirst(a,b);return order?order:*(const int*)a-*(const int*)b;
}
static void prepareWaterSeams(void) {
 waterPrepared=waterReused=0;
 int visible=0;for(int i=0;i<selectedCount;i++)if(nodes[selected[i]].visible && nodes[selected[i]].minHeight<0){visible=1;break;}
 if(!visible)return;
 int order[1024];memcpy(order,selected,selectedCount*sizeof(int));
 /* Compare exact identities rather than a hash: slot reuse and LOD changes
  * must invalidate even if the camera still sees the same region. */
 qsort(order,selectedCount,sizeof(int),waterIdFirst);
 WaterState state[1024];
 for(int i=0;i<selectedCount;i++) {
  Node *n=&nodes[order[i]];
  state[i]=(WaterState){order[i],n->slot,n->meshLevel,n->minHeight<0};
 }
 if(waterStateCount==selectedCount && waterRevision==terrainGeometryRevision &&
    !memcmp(state,waterState,selectedCount*sizeof(*state))) {
  waterReused=1;return;
 }
 qsort(order,selectedCount,sizeof(int),waterCoarseFirst);
 uint32_t tx=0,ts=(uint32_t)selectedCount;
 for(int i=0;i<selectedCount;i++) {
  Node *n=&nodes[selected[i]];uint32_t data[]={selected[i],n->face,n->level,n->x,n->y};
  uint32_t h=cacheHash(data,sizeof(data),2166136261u);tx^=h;ts+=h;
 }
 /* Keep wet patches and their transitive coarse-edge dependencies. A dry
  * neighbor may still define a coast boundary, so cannot simply be dropped. */
 unsigned char needed[SLOTS]={0};
 for(int i=0;i<selectedCount;i++) {
  Node *n=&nodes[selected[i]];
  if(n->slot>=0 && n->vertices && n->minHeight<0)needed[n->slot]=1;
 }
 for(int i=selectedCount-1;i>=0;i--) {
  Node *n=&nodes[order[i]];
  if(n->slot<0 || !n->vertices || !needed[n->slot])continue;
  const SeamNeighbors *neighbors=findSeamNeighbors(order[i],tx,ts);
  for(int r=0;r<neighbors->count;r++) {
   int id=neighbors->runs[r].id;
   if(id<0 || id==order[i])continue;
   Node *o=&nodes[id];
   if(o->slot>=0 && o->vertices && waterCoarseFirst(&id,&order[i])<0)
    needed[o->slot]=1;
  }
 }
 for(int i=0;i<selectedCount;i++) {
  Node *n=&nodes[selected[i]];
  if(n->slot<0 || !n->vertices)continue;
  if(!needed[n->slot])continue;
  waterPrepared++;
  int m=morphFor(selected[i]);const Vertex *src=n->stitched?n->stitched:m>=0?lodMorphs[m].current:n->vertices;
  for(int j=0;j<WATER_NV;j++) {
   Vertex q=src[j];V3 world=add(n->center,q.p);double len=sqrt((double)world.x*world.x+(double)world.y*world.y+(double)world.z*world.z);
   q.p=v3((float)(world.x*(double)RADIUS/len-n->center.x),(float)(world.y*(double)RADIUS/len-n->center.y),(float)(world.z*(double)RADIUS/len-n->center.z));
   waterGrids[n->slot][j]=q;
  }
 }
 for(int i=0;i<selectedCount;i++) {
  Node *n=&nodes[order[i]];if(n->slot<0 || !n->vertices || !needed[n->slot])continue;
  const SeamNeighbors *neighbors=findSeamNeighbors(order[i],tx,ts);
  for(int r=0;r<neighbors->count;r++) {
   const SeamRun *run=&neighbors->runs[r];if(run->id<0)continue;Node *o=&nodes[run->id];
   if(o==n || o->slot<0 || !o->vertices || waterCoarseFirst(&run->id,&order[i])>=0)continue;
   for(int k=run->first;k<=run->last;k++) {
    Vertex *out=&waterGrids[n->slot][edgeVertex(run->edge,k)];double d[3],u,v,scale=1.0/(1u<<o->level);
    seamDirection(n,out->u,out->v,d);seamChart(d,o->face,&u,&v);
    u=((u+1)*.5-o->x*scale)/scale;v=((v+1)*.5-o->y*scale)/scale;
    if(u<-.0001 || u>1.0001 || v<-.0001 || v>1.0001)continue;
    Vertex sample=sampleMesh(waterGrids[o->slot],u,v,o->meshLevel);
    out->p=add(sample.p,add(o->center,mul(n->center,-1)));out->h=sample.h;
   }
  }
 }
 memcpy(waterState,state,selectedCount*sizeof(*state));
 waterStateCount=selectedCount;waterRevision=terrainGeometryRevision;
}
static GLuint waterGeometry(Node *n) {
 if(!waterSeamVBO)glGenBuffers(1,&waterSeamVBO);
 glBindBuffer(GL_ARRAY_BUFFER,waterSeamVBO);
 glBufferData(GL_ARRAY_BUFFER,sizeof(waterGrids[0]),waterGrids[n->slot],GL_STREAM_DRAW);
 return waterSeamVBO;
}
