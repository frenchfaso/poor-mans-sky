// SPDX-License-Identifier: MPL-2.0
#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
static int pipelinePoll(SDL_Event *event);
#define SDL_PollEvent pipelinePoll
#define main poor_mans_sky_application_main
#include "../src/poor-mans-sky.c"
#undef main
#undef SDL_PollEvent
static int liveCheck;
static int pipelinePoll(SDL_Event *event) {
  if(liveCheck) {
    if(voxelScene && voxelMix==1 && staticPlanetMainDraws)die("static mesh underneath caster");
    if(frameNo==60)eye=mul(norm(eye),RADIUS+3000);
    if(frameNo==100) {
      if(voxelScene)die("altitude did not select static fallback");
      eye=mul(norm(eye),RADIUS+1750);
    }
    if(frameNo==140) {
      if(!voxelScene || voxelMix<=0 || voxelMix>=1)
        die("altitude band did not render both paths");
      eye=mul(norm(eye),RADIUS+1000);
    }
    if(frameNo==180) {
      if(!voxelScene || voxelMix<1)die("descent did not restore caster");
      pitch=-1.56f;
    }
    if(frameNo==220) {
      if(voxelScene)die("vertical view did not select static fallback");
      pitch=-.24f;
    }
  }
  return SDL_PollEvent(event);
}
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);return 1; } } while(0)
int main(int argc,char **argv) {
  if(argc>1 && (!strcmp(argv[1],"--live") || !strcmp(argv[1],"--live-hybrid"))) {
    liveCheck=1;
    char *args[]={argv[0],"--windowed",!strcmp(argv[1],"--live-hybrid")?"--voxel-hybrid":"--voxel-terrain","--frames","260","--still","--no-preload","--no-vsync"};
    CHECK(!poor_mans_sky_application_main(sizeof(args)/sizeof(args[0]),args));
    CHECK(staticPlanetBuilds==1 && voxelFallbackUploads==0 && voxelProxyUploads==0);
    CHECK(morphStarted==0 && textureTransitions==0 && batchBytes==0);
    for(int i=0;i<countNode;i++)CHECK(nodes[i].vboBytes==0);
    puts("voxel live pipeline: actual renderer entered mesh fallback and returned to caster OK");return 0;
  }
  CHECK(!SDL_Init(SDL_INIT_VIDEO));
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);
  window=SDL_CreateWindow("voxel pipeline check",0,0,64,64,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);CHECK(window);
  context=SDL_GL_CreateContext(window);CHECK(context);mutex=SDL_CreateMutex();CHECK(mutex);
  countNode=selectedCount=1;selected[0]=0;voxelScene=voxelTerrain=1;
  Node *n=&nodes[0];n->slot=0;n->atlasReady=1;n->size=10;
  n->vertices=calloc(NV,sizeof(Vertex));n->pixels=calloc(1,TERRAIN_BYTES);CHECK(n->vertices && n->pixels);
  for(int i=0;i<NV;i++){n->vertices[i].p=v3(i*.01f,0,0);n->vertices[i].h=i*.1f;}
  TerrainMeta meta={0};memcpy(n->pixels+PAGE_BYTES,&meta,sizeof(meta));
  glEnableClientState(GL_VERTEX_ARRAY);glEnableClientState(GL_NORMAL_ARRAY);glEnableClientState(GL_TEXTURE_COORD_ARRAY);
  voxelPrepareTerrain();CHECK(previousIndex[0]==0 && previousCount==1);
  for(int iteration=0;iteration<3;iteration++) {
    voxelProxyDraw(n);CHECK(n->vboBytes==VOXEL_PROXY_VERTS*sizeof(Vertex));
    CHECK(terrainGPUBytes==n->vboBytes);
    Vertex actual[VOXEL_PROXY_VERTS],expected[VOXEL_PROXY_VERTS];voxelProxyVertices(n,expected);
    glGetBufferSubData(GL_ARRAY_BUFFER,0,sizeof(actual),actual);CHECK(!memcmp(actual,expected,sizeof(actual)));
    GLuint saved=n->vbo;int uploads=voxelProxyUploads;voxelProxyDraw(n);
    CHECK(n->vbo==saved && voxelProxyUploads==uploads); /* immutable proxy reused */
    voxelSetVBO(n,n->vertices,NV*sizeof(Vertex));voxelWasActive=0;CHECK(n->vboBytes==NV*sizeof(Vertex));CHECK(terrainGPUBytes==n->vboBytes);
    GLint bytes;glGetBufferParameteriv(GL_ARRAY_BUFFER,GL_BUFFER_SIZE,&bytes);CHECK(bytes==NV*sizeof(Vertex));
    Vertex all[NV];glGetBufferSubData(GL_ARRAY_BUFFER,0,sizeof(all),all);CHECK(!memcmp(all,n->vertices,sizeof(all)));
    n->stitched=calloc(NV,sizeof(Vertex));morphActive=1;lodMorphs[0].id=0;
    voxelPrepareTerrain();CHECK(n->vbo==0 && n->vboBytes==0 && terrainGPUBytes==0);
    CHECK(!n->stitched && !morphActive && lodMorphs[0].id==-1 && previousIndex[0]==0);
  }
  CHECK(glGetError()==GL_NO_ERROR);CHECK(voxelFallbackUploads==0 && voxelProxyUploads==3);
  resourceInit();materialInit();staticPlanetInit();
  GLuint fixed=staticPlanetVBO;size_t fixedBytes=terrainGPUBytes;staticPlanetInit();
  CHECK(staticPlanetBuilds==1 && staticPlanetVBO==fixed && terrainGPUBytes==fixedBytes);
  voxelWasActive=0;voxelPrepareTerrain();CHECK(staticPlanetVBO==fixed && terrainGPUBytes==fixedBytes);
  /* Verify stipple with GLSL and explicit depth writes, as in the caster.
   * Every pixel must belong to exactly one path with that path's depth. */
  const char *vs="#version 120\nvoid main(){gl_Position=gl_Vertex;gl_FrontColor=gl_Color;}";
  const char *fs="#version 120\nuniform float z;void main(){gl_FragColor=gl_Color;gl_FragDepth=z;}";
  GLuint v=glCreateShader(GL_VERTEX_SHADER),f=glCreateShader(GL_FRAGMENT_SHADER),p=glCreateProgram();
  glShaderSource(v,1,&vs,NULL);glCompileShader(v);glShaderSource(f,1,&fs,NULL);glCompileShader(f);
  glAttachShader(p,v);glAttachShader(p,f);glLinkProgram(p);GLint linked;glGetProgramiv(p,GL_LINK_STATUS,&linked);CHECK(linked);
  glUseProgram(p);glViewport(0,0,64,64);glEnable(GL_DEPTH_TEST);glDepthFunc(GL_ALWAYS);glDepthRange(0,1);
  glDisable(GL_CULL_FACE);glDisable(GL_BLEND);glClearColor(0,0,1,1);glClearDepth(1);
  unsigned char colors[64*64*4];float depths[64*64];
  for(int step=1;step<4;step++) {
    voxelMix=step*.25f;glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    voxelStipple(1);glColor3f(1,0,0);u1(p,"z",.25f);quad();
    voxelStipple(0);glColor3f(0,1,0);u1(p,"z",.75f);quad();glDisable(GL_POLYGON_STIPPLE);
    glReadPixels(0,0,64,64,GL_RGBA,GL_UNSIGNED_BYTE,colors);glReadPixels(0,0,64,64,GL_DEPTH_COMPONENT,GL_FLOAT,depths);
    int red=0;
    for(int i=0;i<64*64;i++) {
      CHECK(colors[i*4+2]==0);int caster=colors[i*4]>127;red+=caster;
      CHECK(caster || colors[i*4+1]>127);CHECK(fabsf(depths[i]-(caster?.25f:.75f))<.0001f);
    }
    CHECK(red==step*1024);
  }
  voxelMix=1;glUseProgram(0);glDeleteProgram(p);glDeleteShader(v);glDeleteShader(f);
  /* Real depth-test audit: a flat opaque surface at 10 m must hide a box
   * at 20 m, but not a near box, a sky hole or data from a previous frame. */
  width=height=voxelWidth=voxelHeight=64;clipNear=1;clipFar=1000;
  cameraEye=v3(0,0,0);viewForward=v3(0,0,-1);viewRight=v3(1,0,0);viewUp=v3(0,1,0);
  voxelBuffers[2]=calloc(64*64,4);CHECK(voxelBuffers[2]);
  unsigned inv=(unsigned)(16777215.f/10);
  for(int i=0;i<64*64;i++) {unsigned char *p=voxelBuffers[2]+i*4;p[0]=inv>>16;p[1]=inv>>8;p[2]=inv;p[3]=255;}
  voxelHiZ=voxelHiZCheck=1;voxelHiZBuild();
  worldDepthLo=.01;worldDepthHi=.5;glDepthRange(worldDepthLo,worldDepthHi);
  glClearDepth(worldDepthLo+(worldDepthHi-worldDepthLo)*(1000./999.)*(1-inv/16777215.));
  glDepthMask(GL_TRUE);glClear(GL_DEPTH_BUFFER_BIT);glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);glDisable(GL_CULL_FACE);camera();
  CHECK(voxelHiddenBox(v3(0,0,-20),v3(1,1,1)));CHECK(voxelHiZVerified==1);
  CHECK(!voxelHiddenBox(v3(0,0,-5),v3(1,1,1)));
  CHECK(!voxelHiddenBox(v3(0,0,-1),v3(1,1,1)));
  voxelBuffers[2][(32*64+32)*4+3]=0;voxelHiZBuild();
  CHECK(!voxelHiddenBox(v3(0,0,-20),v3(1,1,1)));
  frameNo++;CHECK(!voxelHiddenBox(v3(0,0,-20),v3(1,1,1)));
  CHECK(!voxelHiZRect(-1,0,20,20,100));
  voxelHiZFree();free(voxelBuffers[2]);glDeleteQueries(1,&voxelHiZQuery);
  staticPlanetClose();CHECK(terrainGPUBytes==0);free(n->vertices);free(n->pixels);glDeleteBuffers(1,&voxelProxyEBO);
  SDL_DestroyMutex(mutex);SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();
  puts("voxel pipeline: proxy reuse, VBO contents/accounting, mesh fallback, caster re-entry, stipple color/depth and conservative Hi-Z/GPU audit OK");return 0;
}
