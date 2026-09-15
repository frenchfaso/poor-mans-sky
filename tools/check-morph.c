// SPDX-License-Identifier: MPL-2.0
#define main poor_mans_sky_application_main
#include "../src/poor-mans-sky.c"
#undef main
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
int main(void) {
 CHECK(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
 window=SDL_CreateWindow("Morph test",0,0,64,64,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
 CHECK(window); context=SDL_GL_CreateContext(window); CHECK(context);
 Node *n=&nodes[0]; n->vertices=calloc(NV,sizeof(Vertex)); CHECK(n->vertices);
 n->center=v3(0,0,RADIUS); n->size=24; n->slot=0; n->face=4;
 for(int y=0;y<=PATCH;y++) for(int x=0;x<=PATCH;x++) {
  Vertex *v=&n->vertices[y*(PATCH+1)+x];
  v->p=v3(x,(x==12&&y==12)?5:0,y); v->h=v->p.y; v->n=packNormal(v3(0,1,0));
  v->u=x/(float)PATCH;v->v=y/(float)PATCH;
 }
 measureBounds(n); V3 originalHalf=n->boundHalf;
 glGenBuffers(1,&n->vbo);glBindBuffer(GL_ARRAY_BUFFER,n->vbo);
 glBufferData(GL_ARRAY_BUFFER,NV*sizeof(Vertex),n->vertices,GL_DYNAMIC_DRAW);
 cullingMode=0;cameraEye=v3(0,0,RADIUS+50);cullForward=v3(0,0,-1);
 selected[0]=0;selectedCount=1;n->meshLevel=3;n->used=frameNo=0;
 prepareMorphs();
 n->meshLevel=0;n->used=frameNo=1;prepareMorphs();
 int m=morphFor(0), center=12*(PATCH+1)+12;
 CHECK(m>=0);CHECK(fabsf(lodMorphs[m].current[center].p.y)<.001);
 lodMorphs[m].start-=225;n->used=frameNo=2;prepareMorphs();
 CHECK(lodMorphs[m].current[center].p.y>2.3 && lodMorphs[m].current[center].p.y<2.8);
 n->edgeKey=123;
 lodMorphs[m].start-=450;n->used=frameNo=3;prepareMorphs();
 CHECK(n->edgeKey==0);
 CHECK(morphFor(0)<0 && n->meshLevel==0);
 CHECK(fabsf(n->boundHalf.y-originalHalf.y)<.001);
 n->meshLevel=3;n->used=frameNo=4;prepareMorphs();m=morphFor(0);
 CHECK(m>=0 && n->meshLevel==0 && lodMorphs[m].reverse);
 CHECK(fabsf(lodMorphs[m].current[center].p.y-5)<.001);
 lodMorphs[m].start-=225;n->used=frameNo=5;prepareMorphs();
 CHECK(lodMorphs[m].current[center].p.y>2.2 && lodMorphs[m].current[center].p.y<2.7);
 lodMorphs[m].start-=450;n->used=frameNo=6;prepareMorphs();
 CHECK(morphFor(0)<0 && n->meshLevel==3);
 Node *child=&nodes[1]; *child=*n;
 child->level=1;child->size=12;child->used=frameNo=7;
 child->vertices=malloc(NV*sizeof(Vertex));CHECK(child->vertices);
 memcpy(child->vertices,n->vertices,NV*sizeof(Vertex));
 for(int j=0;j<NV;j++){child->vertices[j].p.x*=.5f;child->vertices[j].p.z*=.5f;}
 glGenBuffers(1,&child->vbo);glBindBuffer(GL_ARRAY_BUFFER,child->vbo);
 glBufferData(GL_ARRAY_BUFFER,NV*sizeof(Vertex),child->vertices,GL_DYNAMIC_DRAW);
 child->meshLevel=0;selected[0]=1;prepareMorphs();m=morphFor(1);CHECK(m>=0);
 Vertex gpuVertex;glBindBuffer(GL_ARRAY_BUFFER,child->vbo);
 glGetBufferSubData(GL_ARRAY_BUFFER,center*sizeof(Vertex),sizeof(Vertex),&gpuVertex);
 CHECK(fabsf(gpuVertex.p.y)<.001);
 CHECK(fabsf(gpuVertex.p.x-6)<.001 && fabsf(gpuVertex.p.z-6)<.001);
 V3 v=v3(.2f,.5f,.7f), normal=v3(0,1,0), mirrored=mirrorDirection(v,normal);
 CHECK(fabsf(dot(v,v)-dot(mirrored,mirrored))<.0001);
 CHECK(fabsf(mirrored.y+v.y)<.0001);
 CHECK(glGetError()==GL_NO_ERROR);
 puts("PASS real VBO refinement/coarsening endpoints and midpoint, bound restoration, child starts on parent surface (GPU readback), reflection direction");
 SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();return 0;
}
