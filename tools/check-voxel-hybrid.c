// SPDX-License-Identifier: MPL-2.0
#define SDL_MAIN_HANDLED
#define main poor_mans_sky_application_main
#include "../src/poor-mans-sky.c"
#undef main
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void) {
  resourceInit();CHECK(!SDL_Init(SDL_INIT_VIDEO));
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,24);
  window=SDL_CreateWindow("hybrid plane check",0,0,64,64,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);CHECK(window);
  context=SDL_GL_CreateContext(window);CHECK(context);mutex=SDL_CreateMutex();CHECK(mutex);
  rw=rh=width=height=64;clipNear=1;clipFar=1000;worldDepthLo=.01;worldDepthHi=.5;
  viewForward=v3(0,0,-1);viewRight=v3(1,0,0);viewUp=v3(0,1,0);cameraEye=v3(0,0,0);camera();
  Node *n=&nodes[0];n->slot=0;n->size=24;n->vertices=calloc(NV,sizeof(Vertex));n->pixels=calloc(1,TERRAIN_BYTES);
  CHECK(n->vertices && n->pixels);
  for(int y=0;y<=PATCH;y++)for(int x=0;x<=PATCH;x++) {
    n->vertices[y*(PATCH+1)+x].p=v3(x-12,-2,-2-y);
    n->vertices[y*(PATCH+1)+x].n=packNormal(viewUp);
  }
  hybridInit();size_t indexBytes=hybridGPUBytes;
  unsigned short indices[4*PATCH*(PATCH+3)];
  for(int lod=0;lod<4;lod++)for(int axis=0;axis<2;axis++)for(int reverse=0;reverse<2;reverse++) {
    int count=hybridMakeIndices(indices,1<<lod,axis,reverse);CHECK(count==hybridIndexCount[lod]);
    for(int i=0;i<count;i++)CHECK(indices[i]<HYBRID_VERTS);
  }
  glEnableClientState(GL_VERTEX_ARRAY);glEnableClientState(GL_NORMAL_ARRAY);glEnableClientState(GL_COLOR_ARRAY);
  glViewport(0,0,64,64);glEnable(GL_DEPTH_TEST);glDepthRange(worldDepthLo,worldDepthHi);glDepthFunc(GL_LESS);
  glDisable(GL_CULL_FACE);glClearDepth(1);
  hybridUniforms(hybridP);u3(hybridP,"ambientLight",v3(1,1,1));u1(hybridP,"exposure",1);u1(hybridP,"fastMode",1);
  GLuint white;unsigned char texel[4]={128,128,128,255};glGenTextures(1,&white);glBindTexture(GL_TEXTURE_2D,white);
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,texel);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);tex(hybridP,"detailTex",0,white);
  float depth[4096],saved[4096];int tested=0;
  for(int reverse=0;reverse<2;reverse++) {
    HybridDraw d={0,0,0,reverse,0};glClear(GL_DEPTH_BUFFER_BIT);hybridGeometry(hybridP,&d);
    glReadPixels(0,0,64,64,GL_DEPTH_COMPONENT,GL_FLOAT,depth);
    for(int y=0;y<64;y++)for(int x=0;x<64;x++) {
      float sy=(2*(y+.5f)/64-1)*tanf(PI/6),sx=(2*(x+.5f)/64-1)*tanf(PI/6);
      float hit=sy<0?-2/sy:1e9f;
      if(hit>3 && hit<25 && fabsf(sx*hit)<11) {
        CHECK(depth[y*64+x]<worldDepthHi);
        float nd=(depth[y*64+x]-worldDepthLo)/(worldDepthHi-worldDepthLo);
        float z=clipNear/(1-nd*(clipFar-clipNear)/clipFar);
        CHECK(z>=hit-.002f && z-hit<1.01f);tested++;
      }
      if(sy>=0)CHECK(depth[y*64+x]==1);
    }
    if(!reverse)memcpy(saved,depth,sizeof(depth));else CHECK(!memcmp(saved,depth,sizeof(depth)));
  }
  CHECK(tested>2000 && hybridBuilds==1 && voxelRasterBuilds==0 && voxelSamples==0);
  HybridVertex actual[HYBRID_VERTS];glBindBuffer(GL_ARRAY_BUFFER,hybridBlocks[0].vbo);
  glGetBufferSubData(GL_ARRAY_BUFFER,0,sizeof(actual),actual);
  for(int i=0;i<VOXEL_GRID;i++) {
    V3 scale=hybridBlocks[0].scale,p=n->vertices[i].p;
    CHECK(fabsf(actual[2*i].p[0]*scale.x-p.x)<=scale.x*.51f);
    CHECK(fabsf(actual[2*i].p[1]*scale.y-p.y)<=scale.y*.51f);
    CHECK(fabsf(actual[2*i].p[2]*scale.z-p.z)<=scale.z*.51f);
    CHECK(actual[2*i].bottom==0 && actual[2*i+1].bottom==1);
  }
  frameNo++;cameraEye.x=.3f;HybridDraw d={0,1,1,0,0};hybridGeometry(hybridP,&d);CHECK(hybridBuilds==1);
  voxelInvalidateSlot(0);CHECK(hybridGPUBytes==indexBytes && !hybridBlocks[0].vbo);
  n->vertices[0].p.y=-3;hybridGeometry(hybridP,&d);CHECK(hybridBuilds==2);
  size_t incoming=VRAM_BUDGET-vramEstimate()+sizeof(actual)/2;
  CHECK(vramReserve(incoming) && !hybridBlocks[0].vbo && hybridGPUBytes==indexBytes);
  CHECK(glGetError()==GL_NO_ERROR);hybridClose();CHECK(hybridGPUBytes==0);
  glDeleteTextures(1,&white);free(n->vertices);free(n->pixels);SDL_DestroyMutex(mutex);
  SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();
  puts("hybrid plane: coverage, depth, order, persistent data, movement/stride reuse, slot invalidation and accounting OK");return 0;
}
