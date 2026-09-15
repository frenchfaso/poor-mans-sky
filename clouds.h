// SPDX-License-Identifier: MPL-2.0
/* Static seeded cloud volumes, represented by three offset translucent puffs.
 * One texture, one draw, bounded to 96 nearest visible puffs. No ray marching. */
static GLuint cloudP,cloudTex,cloudVbo;
typedef struct {V3 p;float u,v;V3 color;float alpha;} CloudVertex;
typedef struct {V3 p;float size,dist,alpha;} CloudPuff;
static CloudPuff cloudCatalog[2304];
static uint32_t cloudSeed;static int cloudCatalogReady,cloudDrawCount;
static float cloudScreenArea;
static void cloudPrepareCatalog(void) {
 if(cloudCatalogReady && cloudSeed==worldSeed)return;
 for(int i=0;i<768;i++) {
  uint32_t a=hash3(i,418,worldSeed),b=hash3(i,953,worldSeed);
  float z=(a&65535)/32767.5f-1,t=(b&65535)*2*PI/65536.f;
  V3 d=v3(sqrtf(fmaxf(0,1-z*z))*cosf(t),z,sqrtf(fmaxf(0,1-z*z))*sinf(t));
  V3 p=mul(d,RADIUS+4500+(a>>16)/65535.f*2200),tangent=norm(cross(d,v3(.01f,1,.02f)));
  float size=2400+(b>>16)/65535.f*1800;
  for(int k=0;k<3;k++)cloudCatalog[i*3+k]=(CloudPuff){add(p,add(mul(tangent,(k-1)*size*.5f),mul(d,k%2*size*.15f))),size*(.78f+.11f*((a>>(k*3))&3)),0,1};
 }
 cloudSeed=worldSeed;cloudCatalogReady=1;
}
static void cloudInit(void) {
 if(!cloudTex) {
  if(!cloudP)cloudP=program("cloud.vert","cloud.frag");
  /* Four baked cumulus silhouettes: same sample count at runtime. */
  unsigned char pixels[128*128*4];
  for(int variant=0;variant<4;variant++)for(int y=0;y<64;y++)for(int x=0;x<64;x++) {
    float u=(x-31.5f)/31.5f,v=(y-31.5f)/31.5f;
    float angle=atan2f(v,u),radius=sqrtf(u*u+v*v);
    float rim=.79f+.07f*sinf(angle*5+variant*1.7f)+.035f*sinf(angle*9-variant);
    float edge=clampf((rim-radius)/.21f,0,1);
    edge=edge*edge*(3-2*edge);
    float base=clampf((v+.72f)/.24f,0,1);base=base*base*(3-2*base);
    float billow=.5f+.5f*sinf(u*9+variant)*sinf(v*8+u*3);
    float density=(.30f+.17f*billow)*edge*base;
    float shade=clampf(.70f+.20f*v+.10f*billow,.48f,.98f);
    int i=(((variant/2)*64+y)*128+(variant%2)*64+x)*4;
    pixels[i]=pixels[i+1]=pixels[i+2]=(unsigned char)(shade*255);pixels[i+3]=(unsigned char)(density*255);
  }
  glGenTextures(1,&cloudTex);glBindTexture(GL_TEXTURE_2D,cloudTex);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,128,128,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);glGenBuffers(1,&cloudVbo);
 }
}
static void cloudsDraw(void) {
 cloudDrawCount=0;cloudScreenArea=0;
 if(!cloudsEnabled)return;
 cloudPrepareCatalog();
 cloudInit();
 CloudPuff puffs[96]={0};int count=0;
 const float ty=.5773503f,tx=ty*width/height;
 for(int i=0;i<2304;i++) {
  CloudPuff q=cloudCatalog[i];V3 delta=add(q.p,mul(cameraEye,-1));
  float z=dot(delta,viewForward),distance=sqrtf(dot(delta,delta));
  if(dot(norm(q.p),cameraEye)<RADIUS-10000 || z < -q.size ||
     fabsf(dot(delta,viewRight))>z*tx+q.size*sqrtf(1+tx*tx) ||
     fabsf(dot(delta,viewUp))>z*ty+q.size*sqrtf(1+ty*ty))continue;
  q.dist=distance*distance;
  float fade=clampf((distance/q.size-.12f)/.68f,0,1);q.alpha=fade*fade*(3-2*fade);
  if(q.alpha<.01f)continue;
  if(count==96 && q.dist>=puffs[95].dist)continue;
  int slot=count<96?count++:95;
  while(slot>0 && puffs[slot-1].dist>q.dist){puffs[slot]=puffs[slot-1];slot--;}
  puffs[slot]=q;
 }
 /* Budget projected area, not just sprite count. Taper the final puff so
  * approaching a cloud does not cause an abrupt density boundary. */
 float budget=2.5f;
 for(int i=0;i<count;i++) {
  V3 delta=add(puffs[i].p,mul(cameraEye,-1));float z=fmaxf(puffs[i].size*.25f,dot(delta,viewForward));
  float area=fminf(1,puffs[i].size*puffs[i].size*.52f*.875f/(z*z*tx*ty));
  float weight=clampf(budget/fmaxf(area,.001f),0,1);puffs[i].alpha*=weight;
  budget=fmaxf(0,budget-area);if(weight>0)cloudScreenArea+=area;
 }
 CloudVertex vertices[96*18];int n=0;
 /* The octagon encloses the alpha footprint and removes transparent corners. */
 static const float corners[8][2]={{-1,-.5},{-.5,-1},{.5,-1},{1,-.5},{1,.5},{.5,1},{-.5,1},{-1,.5}};
 for(int i=count-1;i>=0;i--) {
  CloudPuff p=puffs[i];if(p.alpha<.01f)continue;cloudDrawCount++;
  V3 d=norm(p.p);float solar=dot(d,sun),day=clampf((solar+.09f)/.32f,0,1);day=day*day*(3-2*day);
  float warm=clampf((solar+.03f)/.30f,0,1);
  V3 tint=add(mul(v3(1,.53f,.30f),1-warm),mul(v3(.98f,.99f,1),warm));
  float forward=fmaxf(0,dot(norm(add(p.p,mul(cameraEye,-1))),sun));forward*=forward;forward*=forward;
  V3 color=add(mul(v3(.19f,.24f,.34f),.04f+.20f*day),mul(tint,day*(.80f+.16f*forward)));
  int variant=(int)(fabsf(p.p.x*.013f)+fabsf(p.p.z*.007f))%4;
  for(int triangle=1;triangle<7;triangle++)for(int k=0;k<3;k++) {
   int index=k==0?0:k==1?triangle:triangle+1;float x=corners[index][0],y=corners[index][1];
   V3 v=add(add(p.p,mul(cameraEye,-1)),add(mul(viewRight,x*p.size),mul(viewUp,y*p.size*.52f)));
   vertices[n++]=(CloudVertex){v,((variant%2)*64+.5f+(x*.5f+.5f)*63)/128.f,((variant/2)*64+.5f+(y*.5f+.5f)*63)/128.f,color,p.alpha};
  }
 }
 glUseProgram(cloudP);tex(cloudP,"cloudTex",0,cloudTex);if(overdrawView)glUseProgram(overdrawP);glDisable(GL_CULL_FACE);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glDepthMask(GL_FALSE);
 if(overdrawView){glBlendFunc(GL_ONE,GL_ONE);glDisable(GL_DEPTH_TEST);}
 glBindBuffer(GL_ARRAY_BUFFER,cloudVbo);glBufferData(GL_ARRAY_BUFFER,n*sizeof(*vertices),vertices,GL_STREAM_DRAW);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);
 glEnableClientState(GL_VERTEX_ARRAY);glEnableClientState(GL_TEXTURE_COORD_ARRAY);glEnableClientState(GL_COLOR_ARRAY);
 glVertexPointer(3,GL_FLOAT,sizeof(CloudVertex),(void*)offsetof(CloudVertex,p));glTexCoordPointer(2,GL_FLOAT,sizeof(CloudVertex),(void*)offsetof(CloudVertex,u));glColorPointer(4,GL_FLOAT,sizeof(CloudVertex),(void*)offsetof(CloudVertex,color));glDrawArrays(GL_TRIANGLES,0,n);
 glDisableClientState(GL_VERTEX_ARRAY);glDisableClientState(GL_TEXTURE_COORD_ARRAY);glDisableClientState(GL_COLOR_ARRAY);glBindBuffer(GL_ARRAY_BUFFER,0);glDepthMask(GL_TRUE);glDisable(GL_BLEND);glEnable(GL_CULL_FACE);glEnable(GL_DEPTH_TEST);
}
static void cloudsClose(void){glDeleteBuffers(1,&cloudVbo);glDeleteTextures(1,&cloudTex);glDeleteProgram(cloudP);}
