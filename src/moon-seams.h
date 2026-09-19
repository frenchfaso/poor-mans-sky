// SPDX-License-Identifier: MPL-2.0
/* Render-time stitching; disk payloads remain immutable and compatible. */
#define MOON_POINTS ((MOON_GRID+1)*(MOON_GRID+1))
static MoonVertex moonBase[MOON_SLOTS][MOON_POINTS],moonGrid[MOON_SLOTS][MOON_POINTS];
static uint32_t moonUploadedHash[MOON_SLOTS],moonGridKey[MOON_SLOTS];
typedef struct {int slot;float u,v;} MoonLink;
static MoonLink moonLinks[MOON_SLOTS][MOON_POINTS];
static uint32_t moonTopology;
static int moonSeamUploads;
static int moonRank(const MoonPatch *a,const MoonPatch *b) {
 if(a->level!=b->level)return a->level-b->level;
 if(a->face!=b->face)return a->face-b->face;
 if(a->y!=b->y)return a->y-b->y;
 return a->x-b->x;
}
static int moonOrder(const void *a,const void *b){return moonRank(*(MoonPatch*const*)a,*(MoonPatch*const*)b);}
static MoonVertex moonGridSample(const MoonVertex *grid,float u,float v) {
 float x=clampf(u,0,1)*MOON_GRID,y=clampf(v,0,1)*MOON_GRID;
 int ix=(int)fminf(x,MOON_GRID-1),iy=(int)fminf(y,MOON_GRID-1),a=iy*(MOON_GRID+1)+ix,b,c=a+MOON_GRID+2;
 float fx=x-ix,fy=y-iy,wa,wb,wc;
 if(fx>=fy){b=a+1;wa=1-fx;wb=fx-fy;wc=fy;}else{b=a+MOON_GRID+1;wa=1-fy;wb=fy-fx;wc=fx;}
 MoonVertex q=grid[a];q.p=add(mul(grid[a].p,wa),add(mul(grid[b].p,wb),mul(grid[c].p,wc)));
 q.n=norm(add(mul(grid[a].n,wa),add(mul(grid[b].n,wb),mul(grid[c].n,wc))));return q;
}
static void moonRememberGrid(int slot,const MoonVertex *v) {
 int ix[6]={0,1,1,0,1,0},iy[6]={0,0,1,0,1,1},out=0;
 for(int j=0;j<MOON_GRID;j++)for(int i=0;i<MOON_GRID;i++)for(int k=0;k<6;k++)moonBase[slot][(j+iy[k])*(MOON_GRID+1)+i+ix[k]]=v[out++];
 moonUploadedHash[slot]=0;moonGridKey[slot]=0;moonTopology=0;
}
static void moonStitch(void) {
 MoonPatch *order[MOON_SLOTS];memcpy(order,moonSelected,moonSelectedCount*sizeof(*order));qsort(order,moonSelectedCount,sizeof(*order),moonOrder);
 uint32_t topology=2166136261u;
 for(int i=0;i<moonSelectedCount;i++){MoonPatch *p=order[i];int data[]={p-moonPatches,p->face,p->level,p->x,p->y};topology=cacheHash(data,sizeof(data),topology);}
 if(topology!=moonTopology) {
  for(int i=0;i<moonSelectedCount;i++) {
   MoonPatch *p=order[i];int slot=p-moonPatches;Node chart={0};chart.face=p->face;chart.level=p->level;chart.x=p->x;chart.y=p->y;
   for(int y=0;y<=MOON_GRID;y++)for(int x=0;x<=MOON_GRID;x++) {
    MoonLink *link=&moonLinks[slot][y*(MOON_GRID+1)+x];link->slot=-1;
    if(x && y && x<MOON_GRID && y<MOON_GRID)continue;
    double d[3];seamDirection(&chart,x/(double)MOON_GRID,y/(double)MOON_GRID,d);
    for(int j=0;j<i;j++) {
     MoonPatch *o=order[j];double den=d[o->face/2]*(o->face&1?-1:1);if(den<=0)continue;
     double u,v;seamChart(d,o->face,&u,&v);double scale=(double)(1<<o->level);
     u=(u+1)*.5*scale-o->x;v=(v+1)*.5*scale-o->y;
     if(u< -1e-7 || v< -1e-7 || u>1+1e-7 || v>1+1e-7)continue;
     link->slot=o-moonPatches;link->u=u;link->v=v;break;
    }
   }
  }moonTopology=topology;
 }
 moonSeamUploads=0;
 for(int i=0;i<moonSelectedCount;i++) {
  MoonPatch *p=order[i];int slot=p-moonPatches;float t=moonMorphFactor(p);
  uint32_t key=cacheHash(&t,sizeof(t),topology);
  for(int j=0;j<MOON_POINTS;j++){int other=moonLinks[slot][j].slot;if(other>=0)key=cacheHash(&moonUploadedHash[other],sizeof(uint32_t),key);}
  if(moonGridKey[slot]==key && moonUploadedHash[slot])continue;
  moonGridKey[slot]=key;
  for(int j=0;j<MOON_POINTS;j++) {
   MoonVertex q=moonBase[slot][j];q.p=add(mul(q.coarse,1-t),mul(q.p,t));
   MoonLink link=moonLinks[slot][j];if(link.slot>=0) {
    MoonVertex sample=moonGridSample(moonGrid[link.slot],link.u,link.v);
    q.p=add(sample.p,add(moonPatches[link.slot].center,mul(p->center,-1)));q.n=sample.n;
   }
   q.coarse=q.p;moonGrid[slot][j]=q;
  }
  uint32_t hash=cacheHash(moonGrid[slot],sizeof(moonGrid[slot]),2166136261u);
  if(hash==moonUploadedHash[slot])continue;
  MoonVertex out[MOON_VERTS];int n=0,ix[6]={0,1,1,0,1,0},iy[6]={0,0,1,0,1,1};
  for(int y=0;y<MOON_GRID;y++)for(int x=0;x<MOON_GRID;x++)for(int k=0;k<6;k++)out[n++]=moonGrid[slot][(y+iy[k])*(MOON_GRID+1)+x+ix[k]];
  float depth=fmaxf(2,2.f/(1<<p->level)*MOON_RADIUS*.035f);
  for(int side=0;side<4;side++)for(int j=0;j<MOON_GRID;j++) {
   int ax=side<2?(side?MOON_GRID:0):j,ay=side<2?j:(side==3?MOON_GRID:0),bx=side<2?ax:j+1,by=side<2?j+1:ay;
   MoonVertex q[4]={moonGrid[slot][ay*(MOON_GRID+1)+ax],moonGrid[slot][by*(MOON_GRID+1)+bx]};q[2]=q[1];q[3]=q[0];
   for(int k=2;k<4;k++){q[k].p=add(q[k].p,mul(norm(add(p->center,q[k].p)),-depth));q[k].coarse=q[k].p;}
   int ids[6]={0,1,2,0,2,3};for(int k=0;k<6;k++)out[n++]=q[ids[k]];
  }
  moonMeasureBounds(p,out);
  glBindBuffer(GL_ARRAY_BUFFER,p->vbo);glBufferSubData(GL_ARRAY_BUFFER,0,sizeof(out),out);moonUploadedHash[slot]=hash;moonSeamUploads++;
 }
}
