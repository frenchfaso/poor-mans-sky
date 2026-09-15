// SPDX-License-Identifier: MPL-2.0
#define main poor_mans_sky_application_main
#include "../src/poor-mans-sky.c"
#undef main
#define CHECK(x) do { if(!(x)){fprintf(stderr,"FAIL seam %d: %s\n",__LINE__,#x);return 1;} }while(0)
static const int orbital[][5]={
{2,2,3,0,1},
{4,2,3,3,1},
{0,2,0,3,1},
{2,2,2,0,3},
{4,2,2,3,1},
{2,2,3,1,1},
{4,2,3,2,3},
{0,2,1,3,1},
{0,2,0,2,1},
{4,2,2,2,1},
{2,2,2,1,3},
{0,2,1,2,1},
{4,1,0,1,1},
{2,1,0,0,1},
{4,1,1,0,1},
{2,1,1,1,2},
{0,1,0,0,1},
{0,1,1,1,1},
{1,1,1,1,1},
{4,1,0,0,1},
{2,1,0,1,2},
{3,1,1,1,1},
{5,1,0,1,1},
{0,1,1,0,1},
{1,1,1,0,1},
{1,1,0,1,2},
{3,1,0,1,1},
{5,1,1,1,2},
{3,1,1,0,1},
{5,1,0,0,1},
{1,1,0,0,1},
{3,1,0,0,1},
{5,1,1,0,1}
};
static const int coast[][5]={{0,3,1,3,1},{0,3,1,2,1},{0,3,0,2,1},{0,3,0,3,1},{0,3,2,3,3},{0,3,2,2,3},{4,3,7,2,3},{4,3,7,3,3},{0,3,1,4,1},{0,3,0,1,1},{0,3,1,1,1},{0,3,0,4,3},{0,3,0,0,3},{4,3,6,3,3},{4,3,6,2,3},{0,3,3,3,3},{0,3,1,0,1},{0,3,3,2,3},{0,3,0,5,3},{0,3,1,5,3},{4,2,3,0,3},{4,2,3,2,3},{0,2,1,2,1},{0,2,1,0,1},{3,2,3,3,3},{0,2,0,3,1},{4,2,2,1,1},{3,2,3,2,3},{4,2,3,3,1},{0,2,2,1,1},{4,2,2,0,1},{4,2,2,2,1},{0,2,1,3,1},{0,2,2,0,1},{3,2,2,3,1},{0,2,2,2,1},{2,2,3,0,1},{3,2,2,2,1},{4,2,2,3,1},{2,2,3,1,1},{0,2,2,3,1},{0,2,3,0,1},{0,2,3,1,3},{2,2,2,0,3},{0,2,3,2,3},{3,1,1,0,1},{0,2,3,3,1},{4,1,0,0,1},{2,2,2,1,3},{4,1,0,1,1},{3,1,0,1,1},{2,1,1,1,2},{5,1,0,0,1},{5,1,0,1,1},{2,1,0,0,2},{1,1,1,0,1},{3,1,0,0,1},{1,1,1,1,1},{5,1,1,0,1},{2,1,0,1,2},{1,1,0,0,1},{5,1,1,1,2},{1,1,0,1,2}};
static int sceneCheck(const int (*cover)[5],int count) {
  countNode=freeCount=selectedCount=0;
  for(int i=0;i<MAXNODE;i++)previousIndex[i]=-1;
  for(int i=0;i<MORPH_SLOTS;i++)lodMorphs[i].id=-1;
  for(int f=0;f<6;f++)newNode(f,0,0,0);
  float maximum=0;int corners=0,flipped=0,collapsed=0;
  for(int i=0;i<count;i++) {
    int f=cover[i][0],level=cover[i][1],x=cover[i][2],y=cover[i][3],id=f;
    for(int l=1;l<=level;l++) {
      Node *n=&nodes[id];
      if(n->child[0]<0)for(int k=0;k<4;k++)n->child[k]=newNode(f,l,n->x*2+(k&1),n->y*2+(k>>1));
      id=n->child[((x>>(level-l))&1)+2*((y>>(level-l))&1)];
    }
    Node *n=&nodes[id];generate(n,&n->pixels,&n->vertices);
    n->meshLevel=cover[i][4];n->slot=i;n->used=frameNo=100;
    glGenBuffers(1,&n->vbo);glBindBuffer(GL_ARRAY_BUFFER,n->vbo);
    glBufferData(GL_ARRAY_BUFFER,NV*sizeof(Vertex),n->vertices,GL_STATIC_DRAW);
    selected[selectedCount]=id;previousIndex[id]=selectedCount++;
  }
  stitchTerrainEdges();
  for(int i=0;i<count;i++) {
    Node *n=&nodes[selected[i]];if(!n->stitched)continue;
    Vertex gpu[NV];glBindBuffer(GL_ARRAY_BUFFER,n->vbo);glGetBufferSubData(GL_ARRAY_BUFFER,0,sizeof(gpu),gpu);
    CHECK(!memcmp(gpu,n->stitched,sizeof(gpu)));
    int step=1<<n->meshLevel;
    for(int y=0;y<PATCH;y+=step)for(int x=0;x<PATCH;x+=step) {
      int a=y*(PATCH+1)+x,b=a+step*(PATCH+1);
      int tri[2][3]={{a,a+step,b},{a+step,b+step,b}};
      for(int t=0;t<2;t++) {
        V3 p0=n->stitched[tri[t][0]].p,p1=n->stitched[tri[t][1]].p,p2=n->stitched[tri[t][2]].p;
        V3 normal=cross(add(p1,mul(p0,-1)),add(p2,mul(p0,-1)));
        flipped+=dot(normal,add(n->center,p0)) < -1;
        collapsed+=dot(normal,normal)<1e-8f;
      }
    }
    for(int e=0;e<4;e++)for(int k=0;k<=PATCH;k++) {
      Vertex *v=&n->stitched[edgeVertex(e,k)],*skirt=&n->stitched[(PATCH+1)*(PATCH+1)+e*(PATCH+1)+k];
      V3 world=add(n->center,v->p),radial=norm(world);
      V3 expected=add(v->p,mul(radial,-fmaxf(1,n->size*.06f)));
      V3 error=add(skirt->p,mul(expected,-1));float length=sqrtf(dot(error,error));
      maximum=fmaxf(maximum,length);if(k==0||k==PATCH)corners++;
    }
  }
  printf("SEAM_CHECK patches=%d corners=%d maximum_skirt_error=%.6f m\n",count,corners,maximum);
  printf("SURFACE_CHECK flipped=%d collapsed=%d\n",flipped,collapsed);
  CHECK(maximum<.1f && flipped==0 && collapsed==0);
  CHECK(glGetError()==GL_NO_ERROR);
  for(int i=0;i<countNode;i++){free(nodes[i].vertices);free(nodes[i].pixels);free(nodes[i].stitched);if(nodes[i].vbo)glDeleteBuffers(1,&nodes[i].vbo);}
  return 0;
}
int main(void) {
  CHECK(!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER));
  window=SDL_CreateWindow("Poor Man's Sky corner regression",0,0,64,64,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
  CHECK(window);context=SDL_GL_CreateContext(window);CHECK(context);
  /* Loading geology needs no graphics progress screen in this test. */
  SDL_Window *saved=window;window=NULL;cacheEnabled=0;geologyInit();materialInit();window=saved;
  mutex=SDL_CreateMutex();
  CHECK(!sceneCheck(orbital,sizeof(orbital)/sizeof(orbital[0])));
  CHECK(!sceneCheck(coast,sizeof(coast)/sizeof(coast[0])));
  const int nearCover[][5]={{4,14,8192,8192,1},{4,14,8193,8192,3},{4,14,8192,8193,3},{4,14,8193,8193,1}};
  CHECK(!sceneCheck(nearCover,4));
  puts("PASS orbital and near-ground shared corners remain radial and point inward; GPU readback matches");
  SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();return 0;
}
