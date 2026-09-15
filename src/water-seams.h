// SPDX-License-Identifier: MPL-2.0
/* Stitch the rendered sea surface, not the displaced terrain underneath it.
 * One small streaming VBO; CPU grids are bounded by terrain residency slots. */
#define WATER_NV ((PATCH+1)*(PATCH+1))
static Vertex waterGrids[SLOTS][WATER_NV];
static GLuint waterSeamVBO;
static int waterCoarseFirst(const void *a,const void *b) {
 int order=coarseFirst(a,b);return order?order:*(const int*)a-*(const int*)b;
}
static void prepareWaterSeams(void) {
 int visible=0;for(int i=0;i<selectedCount;i++)if(nodes[selected[i]].visible && nodes[selected[i]].minHeight<0){visible=1;break;}
 if(!visible)return;
 int order[1024];memcpy(order,selected,selectedCount*sizeof(int));
 qsort(order,selectedCount,sizeof(int),waterCoarseFirst);
 uint32_t tx=0,ts=(uint32_t)selectedCount;
 for(int i=0;i<selectedCount;i++) {
  Node *n=&nodes[selected[i]];uint32_t data[]={selected[i],n->face,n->level,n->x,n->y};
  uint32_t h=cacheHash(data,sizeof(data),2166136261u);tx^=h;ts+=h;
  if(n->slot<0 || !n->vertices)continue;
  int m=morphFor(selected[i]);const Vertex *src=n->stitched?n->stitched:m>=0?lodMorphs[m].current:n->vertices;
  for(int j=0;j<WATER_NV;j++) {
   Vertex q=src[j];V3 world=add(n->center,q.p);double len=sqrt((double)world.x*world.x+(double)world.y*world.y+(double)world.z*world.z);
   q.p=v3((float)(world.x*(double)RADIUS/len-n->center.x),(float)(world.y*(double)RADIUS/len-n->center.y),(float)(world.z*(double)RADIUS/len-n->center.z));
   waterGrids[n->slot][j]=q;
  }
 }
 for(int i=0;i<selectedCount;i++) {
  Node *n=&nodes[order[i]];if(n->slot<0 || !n->vertices)continue;
  const SeamNeighbors *neighbors=findSeamNeighbors(order[i],tx,ts);
  for(int r=0;r<neighbors->count;r++) {
   const SeamRun *run=&neighbors->runs[r];Node *o=&nodes[run->id];
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
}
static GLuint waterGeometry(Node *n) {
 if(!waterSeamVBO)glGenBuffers(1,&waterSeamVBO);
 glBindBuffer(GL_ARRAY_BUFFER,waterSeamVBO);
 glBufferData(GL_ARRAY_BUFFER,sizeof(waterGrids[0]),waterGrids[n->slot],GL_STREAM_DRAW);
 return waterSeamVBO;
}
