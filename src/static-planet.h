// SPDX-License-Identifier: MPL-2.0
/* One welded cube sphere, generated once per world. No patch LOD or morph. */
#define STATIC_PLANET_SIDE 32
#define STATIC_PLANET_VERTS (6*STATIC_PLANET_SIDE*STATIC_PLANET_SIDE+2)
#define STATIC_PLANET_INDICES (36*STATIC_PLANET_SIDE*STATIC_PLANET_SIDE)
typedef struct { V3 p; PackedNormal n; unsigned char color[4]; } StaticPlanetVertex;
static GLuint staticPlanetVBO,staticPlanetEBO,staticPlanetP;
static int staticPlanetBuilds;
static size_t staticPlanetBytes;
static void staticPlanetBuild(StaticPlanetVertex *vertices,unsigned short *indices) {
  int side=STATIC_PLANET_SIDE,span=side+1;
  int *ids=malloc((size_t)span*span*span*sizeof(int));if(!ids)die("static planet topology allocation");
  for(int i=0;i<span*span*span;i++)ids[i]=-1;
  int count=0,out=0;
  for(int face=0;face<6;face++) {
    unsigned short grid[STATIC_PLANET_SIDE+1][STATIC_PLANET_SIDE+1];
    for(int y=0;y<=side;y++)for(int x=0;x<=side;x++) {
      int a,b,c;
      switch(face) {
        case 0:a=side;b=y;c=side-x;break;
        case 1:a=0;b=y;c=x;break;
        case 2:a=x;b=side;c=side-y;break;
        case 3:a=x;b=0;c=y;break;
        case 4:a=x;b=y;c=side;break;
        default:a=side-x;b=y;c=0;break;
      }
      int key=(a*span+b)*span+c,id=ids[key];
      if(id<0) {
        id=ids[key]=count++;V3 d=norm(v3(2.f*a/side-1,2.f*b/side-1,2.f*c/side-1));
        float h=elevation(d);V3 n=surfaceNormal(d),pos=mul(d,RADIUS+h);
        float slope=clampf(dot(n,d),0,1);
        float grass=clampf((h-3)*.09f,0,1)*clampf((2300-h)*.0015f,0,1)*clampf((slope-.64f)*4,0,1);
        float snow=clampf((h-3200)*.0018f,0,.92f)*clampf((slope-.45f)*3,0,1);
        V3 rock=materialTriplanar(1,pos,n,RADIUS/side),soil=materialTriplanar(0,pos,n,RADIUS/side);
        if(geological && geology) {
          GeoCell climate=geoSample(d,NULL);
          grass*=clampf(climate.wet*1.8f-.2f,0,1)*clampf((climate.temperature+3)/15,0,1);
          snow=clampf((1-climate.temperature)*.15f,0,.92f)*clampf((slope-.45f)*3,0,1);
        }
        V3 col=add(mul(rock,1-grass),mul(add(mul(soil,.7f),v3(.028f,.047f,.012f)),grass));
        col=add(mul(col,1-snow),mul(v3(.52f,.57f,.61f),snow));
        vertices[id]=(StaticPlanetVertex){pos,packNormal(n),{(unsigned char)(clampf(col.x,0,1)*255),(unsigned char)(clampf(col.y,0,1)*255),(unsigned char)(clampf(col.z,0,1)*255),255}};
      }
      grid[y][x]=(unsigned short)id;
    }
    for(int y=0;y<side;y++)for(int x=0;x<side;x++) {
      unsigned short a=grid[y][x],b=grid[y][x+1],c=grid[y+1][x],d=grid[y+1][x+1];
      indices[out++]=a;indices[out++]=b;indices[out++]=c;
      indices[out++]=b;indices[out++]=d;indices[out++]=c;
    }
  }
  free(ids);if(count!=STATIC_PLANET_VERTS || out!=STATIC_PLANET_INDICES)die("static planet topology size");
}
static void staticPlanetInit(void) {
  if(staticPlanetVBO)return;
  size_t vb=STATIC_PLANET_VERTS*sizeof(StaticPlanetVertex),ib=STATIC_PLANET_INDICES*sizeof(unsigned short);
  if(!vramReserve(vb+ib))die("static planet VRAM budget");
  StaticPlanetVertex *v=malloc(vb);unsigned short *ix=malloc(ib);if(!v || !ix)die("static planet allocation");
  staticPlanetBuild(v,ix);
  glGenBuffers(1,&staticPlanetVBO);glBindBuffer(GL_ARRAY_BUFFER,staticPlanetVBO);glBufferData(GL_ARRAY_BUFFER,vb,v,GL_STATIC_DRAW);
  glGenBuffers(1,&staticPlanetEBO);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,staticPlanetEBO);glBufferData(GL_ELEMENT_ARRAY_BUFFER,ib,ix,GL_STATIC_DRAW);
  free(v);free(ix);staticPlanetBytes=vb+ib;terrainGPUBytes+=staticPlanetBytes;staticPlanetBuilds++;
  printf("STATIC_PLANET vertices=%d triangles=%d gpu_bytes=%zu builds=%d\n",STATIC_PLANET_VERTS,STATIC_PLANET_INDICES/3,staticPlanetBytes,staticPlanetBuilds);
}
static void staticPlanetGeometry(void) {
  glBindBuffer(GL_ARRAY_BUFFER,staticPlanetVBO);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,staticPlanetEBO);
  glVertexPointer(3,GL_FLOAT,sizeof(StaticPlanetVertex),(void*)offsetof(StaticPlanetVertex,p));
  glNormalPointer(GL_BYTE,sizeof(StaticPlanetVertex),(void*)offsetof(StaticPlanetVertex,n));
  glColorPointer(4,GL_UNSIGNED_BYTE,sizeof(StaticPlanetVertex),(void*)offsetof(StaticPlanetVertex,color));
  glDisableClientState(GL_TEXTURE_COORD_ARRAY);glEnableClientState(GL_COLOR_ARRAY);
  glDrawElements(GL_TRIANGLES,STATIC_PLANET_INDICES,GL_UNSIGNED_SHORT,0);triangles+=STATIC_PLANET_INDICES/3;
  glDisableClientState(GL_COLOR_ARRAY);glEnableClientState(GL_TEXTURE_COORD_ARRAY);
}
static void staticPlanetDraw(int reflection,V3 radial) {
  staticPlanetInit();if(!staticPlanetP)staticPlanetP=program("static-planet.vert","static-planet.frag");
  GLuint p=staticPlanetP;glUseProgram(p);
  u3(p,"eye",cameraEye);u3(p,"sun",sun);u3(p,"sunLight",sunLight);u3(p,"ambientLight",ambientLight);
  u3(p,"ambientUp",radial);u3(p,"fogColor",fogColor);u1(p,"exposure",sceneExposure);
  u1(p,"fogHeightFactor",reflection?0:exp2f(-fmaxf(sqrtf(dot(cameraEye,cameraEye))-RADIUS,0)/13000));
  glUniform4f(uniformLocation(p,"waterPlane"),radial.x,radial.y,radial.z,reflection?RADIUS:-1e20f);
  staticPlanetGeometry();drawn++;
}
static void staticPlanetClose(void) {
  glDeleteBuffers(1,&staticPlanetVBO);glDeleteBuffers(1,&staticPlanetEBO);glDeleteProgram(staticPlanetP);
  terrainGPUBytes-=staticPlanetBytes;staticPlanetBytes=0;
  staticPlanetVBO=staticPlanetEBO=staticPlanetP=0;
}
