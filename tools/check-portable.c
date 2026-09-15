// SPDX-License-Identifier: MPL-2.0
#define main poor_mans_sky_application_main
#include "../poor-mans-sky.c"
#undef main
int main(int argc,char **argv) {
  if(argc!=2)return 2;
  SDL_Init(SDL_INIT_TIMER);cacheDirectory=argv[1];cacheInit();geologyInit();materialInit();view(1);
  V3 d=norm(eye);int f=0;float u=0,v=0;
  float best=-1;for(int i=0;i<6;i++){V3 q=direction(i,0,0);float t=dot(q,d);if(t>best){best=t;f=i;}}
  float den=f<2?d.x:f<4?d.y:d.z;if(f&1)den=-den;
  u=f==0?-d.z/den:f==1?d.z/den:f==5?-d.x/den:d.x/den;
  v=f==2?-d.z/den:f==3?d.z/den:d.y/den;
  float maxHeight=0,maxPosition=0;int hits=0;
  for(int level=0;level<18;level++) {
    int id=newNode(f,level,(int)((u+1)*.5f*(1<<level)),(int)((v+1)*.5f*(1<<level)));Node *n=&nodes[id];
    unsigned char *p=malloc(TERRAIN_BYTES),*fresh;Vertex *cached=malloc(NV*sizeof(Vertex)),*vertices;uint32_t bytes=0;
    if(!cacheRead(1,n->face,n->level,n->x,n->y,p,TERRAIN_BYTES,cached,NV*sizeof(Vertex),&bytes)){free(p);free(cached);continue;}
    hits++;TerrainMeta m;memcpy(&m,p+PAGE_BYTES,sizeof(m));n->center=m.center;
    generateFresh(n,&fresh,&vertices);
    for(int j=0;j<NV;j++) {float h=fabsf(vertices[j].h-cached[j].h);V3 q=add(vertices[j].p,mul(cached[j].p,-1));
      maxHeight=fmaxf(maxHeight,h);maxPosition=fmaxf(maxPosition,sqrtf(dot(q,q)));}
    free(p);free(cached);free(fresh);free(vertices);
  }
  printf("PORTABLE hits=%d max_height_error=%.8g max_position_error=%.8g vertex=%zu nature=%zu\n",hits,maxHeight,maxPosition,sizeof(Vertex),sizeof(NaturePacked));
  cacheClose();return hits>=15&&maxHeight<.5f&&maxPosition<1.f?0:1;
}
