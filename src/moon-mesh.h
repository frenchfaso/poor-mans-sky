// SPDX-License-Identifier: MPL-2.0
/* GPU-ready mesh generation, shared by runtime and offline baker. */
#define MOON_GRID 12
#define MOON_VERTS (MOON_GRID*MOON_GRID*6+MOON_GRID*4*6)
typedef struct {V3 p,n;float u,v;V3 coarse;} MoonVertex;
static V3 moonPoint(int face,float u,float v) {V3 d=direction(face,u,v);return mul(d,MOON_RADIUS+moonHeight(d));}
static void moonFineMesh(int face,int level,int x,int y,V3 *center,MoonVertex *v) {
 float span=2.f/(1<<level),u=-1+x*span,w=-1+y*span;
 *center=moonPoint(face,u+span*.5f,w+span*.5f);
 V3 points[MOON_GRID+1][MOON_GRID+1],normals[MOON_GRID+1][MOON_GRID+1];
 for(int j=0;j<=MOON_GRID;j++)for(int i=0;i<=MOON_GRID;i++) {
  float a=u+span*i/MOON_GRID,b=w+span*j/MOON_GRID,e=fmaxf(2,span*MOON_RADIUS/MOON_GRID*.5f)/MOON_RADIUS;
  points[j][i]=moonPoint(face,a,b);
  V3 da=add(moonPoint(face,a+e,b),mul(moonPoint(face,a-e,b),-1)),db=add(moonPoint(face,a,b+e),mul(moonPoint(face,a,b-e),-1));
  V3 n=norm(cross(da,db));if(dot(n,points[j][i])<0)n=mul(n,-1);normals[j][i]=n;
 }
 int out=0,ix[6]={0,1,1,0,1,0},iy[6]={0,0,1,0,1,1};
 for(int j=0;j<MOON_GRID;j++)for(int i=0;i<MOON_GRID;i++)for(int k=0;k<6;k++) {
  int a=i+ix[k],b=j+iy[k];V3 p=points[b][a];
  v[out++]=(MoonVertex){add(p,mul(*center,-1)),normals[b][a],fmodf(u*MOON_RADIUS/24,1)+span*a/MOON_GRID*MOON_RADIUS/24,fmodf(w*MOON_RADIUS/24,1)+span*b/MOON_GRID*MOON_RADIUS/24,{0,0,0}};
 }
 for(int side=0;side<4;side++)for(int j=0;j<MOON_GRID;j++) {
  int ax=side<2?(side?MOON_GRID:0):j,ay=side<2?j:(side==3?MOON_GRID:0);
  int bx=side<2?ax:j+1,by=side<2?j+1:ay;
  V3 a=points[ay][ax],b=points[by][bx];float depth=fmaxf(2,span*MOON_RADIUS*.035f);
  V3 p[4]={a,b,add(b,mul(norm(b),-depth)),add(a,mul(norm(a),-depth))};int ids[6]={0,1,2,0,2,3};
  for(int k=0;k<6;k++){int id=ids[k];v[out++]=(MoonVertex){add(p[id],mul(*center,-1)),norm(p[id]),fmodf(u*MOON_RADIUS/24,1)+span*ax/MOON_GRID*MOON_RADIUS/24,fmodf(w*MOON_RADIUS/24,1)+span*ay/MOON_GRID*MOON_RADIUS/24,{0,0,0}};}
 }
}

/* Exact barycentric sample of the parent's triangle mesh (same diagonal).
 * Stored in disk/RAM payloads, so morphing adds no per-frame CPU mesh uploads. */
static V3 moonParentSample(V3 grid[13][13],float u,float v) {
 float x=clampf(u,0,1)*MOON_GRID,y=clampf(v,0,1)*MOON_GRID;
 int ix=(int)fminf(x,MOON_GRID-1),iy=(int)fminf(y,MOON_GRID-1);
 float a=x-ix,b=y-iy;
 if(a>=b)return add(mul(grid[iy][ix],1-a),add(mul(grid[iy][ix+1],a-b),mul(grid[iy+1][ix+1],b)));
 return add(mul(grid[iy][ix],1-b),add(mul(grid[iy+1][ix+1],a),mul(grid[iy+1][ix],b-a)));
}
static void moonCoarseMesh(int face,int level,int x,int y,V3 center,MoonVertex *vertices) {
 if(!level){for(int i=0;i<MOON_VERTS;i++)vertices[i].coarse=vertices[i].p;return;}
 float span=2.f/(1<<(level-1)),u=-1+(x/2)*span,v=-1+(y/2)*span;
 V3 grid[13][13];for(int j=0;j<=MOON_GRID;j++)for(int i=0;i<=MOON_GRID;i++)grid[j][i]=moonPoint(face,u+span*i/MOON_GRID,v+span*j/MOON_GRID);
 V3 points[13][13];
 for(int j=0;j<=MOON_GRID;j++)for(int i=0;i<=MOON_GRID;i++)points[j][i]=moonParentSample(grid,((x&1)+i/(float)MOON_GRID)*.5f,((y&1)+j/(float)MOON_GRID)*.5f);
 int out=0,ix[6]={0,1,1,0,1,0},iy[6]={0,0,1,0,1,1};
 for(int j=0;j<MOON_GRID;j++)for(int i=0;i<MOON_GRID;i++)for(int k=0;k<6;k++)vertices[out++].coarse=add(points[j+iy[k]][i+ix[k]],mul(center,-1));
 for(int side=0;side<4;side++)for(int j=0;j<MOON_GRID;j++) {
  int ax=side<2?(side?MOON_GRID:0):j,ay=side<2?j:(side==3?MOON_GRID:0),bx=side<2?ax:j+1,by=side<2?j+1:ay;
  V3 a=points[ay][ax],b=points[by][bx];float depth=fmaxf(2,span*.5f*MOON_RADIUS*.035f);
  V3 p[4]={a,b,add(b,mul(norm(b),-depth)),add(a,mul(norm(a),-depth))};int ids[6]={0,1,2,0,2,3};
  for(int k=0;k<6;k++)vertices[out++].coarse=add(p[ids[k]],mul(center,-1));
 }
}
static void moonMesh(int face,int level,int x,int y,V3 *center,MoonVertex *vertices) {
 moonFineMesh(face,level,x,y,center,vertices);moonCoarseMesh(face,level,x,y,*center,vertices);
}
