// SPDX-License-Identifier: MPL-2.0
/* Static seeded clouds: three offset puffs, each with three planet-fixed cards.
 * One texture, one draw, bounded to 96 nearest visible puffs. No ray marching. */
static GLuint cloudP,cloudTex,cloudVbo;
typedef struct {V3 p;float u,v;V3 color;float alpha;} CloudVertex;
typedef struct {V3 p;float size,dist,alpha;} CloudPuff;
static CloudPuff cloudCatalog[2304];
static uint32_t cloudSeed;static int cloudCatalogReady,cloudDrawCount;
static float cloudScreenArea;
/* Three fixed orthogonal cards per puff. Their axes belong to the planet,
 * never the camera or its history. View weights fade edge-on cards smoothly. */
static void cloudAxes(V3 position,V3 axes[3]) {
 axes[1]=norm(position);
 axes[0]=norm(cross(axes[1],fabsf(axes[1].y)<.9f?v3(0,1,0):v3(1,0,0)));
 axes[2]=cross(axes[0],axes[1]);
}
static void cloudWeights(V3 delta,V3 axes[3],float weights[3]) {
 V3 direction=norm(delta);float sum=0;
 for(int i=0;i<3;i++) {
  float facing=dot(direction,axes[2-i]);
  weights[i]=facing*facing;sum+=weights[i];
 }
 for(int i=0;i<3;i++)weights[i]/=fmaxf(sum,1e-6f);
}
static const float cloudCorners[8][2]={{-1,-.5},{-.5,-1},{.5,-1},{1,-.5},{1,.5},{.5,1},{-.5,1},{-1,.5}};
static const int cloudHorizontal[3]={0,0,2},cloudVertical[3]={1,2,1};
static const float cloudScale[3]={1,.52f,.72f};
/* Homogeneous camera coordinates: sides are x/y = +/-z. Clip only crossed
 * planes; most cards need no clipping. Offscreen pixels spend no fill budget. */
static float cloudPlaneDistance(V3 p,int plane) {
 switch(plane) {
  case 0:return p.x+p.z;case 1:return p.z-p.x;
  case 2:return p.y+p.z;case 3:return p.z-p.y;
  case 4:return p.z-clipNear;default:return clipFar-p.z;
 }
}
static float cloudCardArea(V3 center,V3 right,V3 up) {
 V3 polygons[2][16];int count=8,source=0,any=0,all=63;
 for(int i=0;i<8;i++) {
  V3 p=add(center,add(mul(right,cloudCorners[i][0]),mul(up,cloudCorners[i][1])));
  polygons[0][i]=p;int outside=0;
  for(int plane=0;plane<6;plane++)if(cloudPlaneDistance(p,plane)<0)outside|=1<<plane;
  any|=outside;all&=outside;
 }
 if(all)return 0;
 for(int plane=0;plane<6;plane++)if(any&(1<<plane)) {
  V3 *in=polygons[source],*out=polygons[1-source];int written=0;
  V3 previous=in[count-1];float a=cloudPlaneDistance(previous,plane);
  for(int i=0;i<count;i++) {
   V3 current=in[i];float b=cloudPlaneDistance(current,plane);
   if((a<0)!=(b<0))out[written++]=add(previous,mul(add(current,mul(previous,-1)),a/(a-b)));
   if(b>=0)out[written++]=current;
   previous=current;a=b;
  }
  if(written<3)return 0;
  count=written;source=1-source;
 }
 V3 *p=polygons[source];float area=0;
 float lastX=p[count-1].x/p[count-1].z,lastY=p[count-1].y/p[count-1].z;
 for(int i=0;i<count;i++) {
  float x=p[i].x/p[i].z,y=p[i].y/p[i].z;
  area+=lastX*y-x*lastY;lastX=x;lastY=y;
 }
 return fabsf(area)*.125f; /* Shoelace area / the four-square-unit viewport. */
}
static float cloudProjectedArea(V3 delta,float size,V3 axes[3],float tx,float ty) {
 V3 center=v3(dot(delta,viewRight)/tx,dot(delta,viewUp)/ty,dot(delta,viewForward)),projected[3];
 for(int i=0;i<3;i++) {
  V3 axis=mul(axes[i],size*cloudScale[i]);
  projected[i]=v3(dot(axis,viewRight)/tx,dot(axis,viewUp)/ty,dot(axis,viewForward));
 }
 float area=0;
 for(int face=0;face<3;face++)area+=cloudCardArea(center,projected[cloudHorizontal[face]],projected[cloudVertical[face]]);
 return area;
}
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
  /* Offline density/lighting integration; one texture sample per fragment. */
  unsigned char pixels[256*256*4], header[16];
  FILE *f = fopen(resourcePath("assets/cloud-procedural.rgba"), "rb");
  if (!f) die("cloud atlas missing: run make clouds");
  const unsigned char expected[16] = {'P','M','S','C','L','O','U','D',0,1,0,0,0,1,0,0};
  int valid = fread(header, 1, sizeof(header), f) == sizeof(header) &&
              !memcmp(header, expected, sizeof(header)) &&
              fread(pixels, 1, sizeof(pixels), f) == sizeof(pixels) && fgetc(f) == EOF;
  fclose(f);
  if (!valid) die("invalid cloud atlas");
  glGenTextures(1,&cloudTex);glBindTexture(GL_TEXTURE_2D,cloudTex);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,256,256,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);glGenBuffers(1,&cloudVbo);
 }
}
static void cloudsDraw(void) {
 cloudDrawCount=0;cloudScreenArea=0;
 if(!cloudsEnabled)return;
 cloudPrepareCatalog();
 cloudInit();
 CloudPuff puffs[96]={0};int count=0,limit=quality()->cloudLimit;
 const float ty=.5773503f,tx=ty*width/height;
 for(int i=0;i<2304;i++) {
  CloudPuff q=cloudCatalog[i];V3 delta=add(q.p,mul(cameraEye,-1));
  float z=dot(delta,viewForward);
  float bound=q.size*1.08f; /* Circumscribe the octagon at any screen roll. */
  if(z+bound<clipNear || z-bound>clipFar ||
     fabsf(dot(delta,viewRight))>z*tx+bound*sqrtf(1+tx*tx) ||
     fabsf(dot(delta,viewUp))>z*ty+bound*sqrtf(1+ty*ty) || behindPlanet(q.p,bound))continue;
  q.dist=dot(delta,delta);float distance=sqrtf(q.dist);
  float fade=clampf((distance/q.size-.12f)/.68f,0,1);q.alpha=fade*fade*(3-2*fade);
  if(q.alpha<.01f)continue;
  if(count==limit && q.dist>=puffs[limit-1].dist)continue;
  int slot=count<limit?count++:limit-1;
  while(slot>0 && puffs[slot-1].dist>q.dist){puffs[slot]=puffs[slot-1];slot--;}
  puffs[slot]=q;
 }
 /* Budget projected area, not just sprite count. Taper the final puff so
  * approaching a cloud does not cause an abrupt density boundary. */
 float budget=quality()->cloudArea;
 V3 puffAxes[96][3];float puffWeights[96][3];
 for(int i=0;i<count;i++) {
  if(budget<=0){for(int j=i;j<count;j++)puffs[j].alpha=0;break;}
  V3 delta=add(puffs[i].p,mul(cameraEye,-1));
  cloudAxes(puffs[i].p,puffAxes[i]);cloudWeights(delta,puffAxes[i],puffWeights[i]);
  float area=cloudProjectedArea(delta,puffs[i].size,puffAxes[i],tx,ty);
  if(area<=1e-6f){puffs[i].alpha=0;continue;}
  float weight=clampf(budget/area,0,1);puffs[i].alpha*=weight;
  budget=fmaxf(0,budget-area);if(weight>0)cloudScreenArea+=area;
 }
 CloudVertex vertices[96*3*18];int n=0;
 /* The octagon encloses the alpha footprint and removes transparent corners. */
 for(int i=count-1;i>=0;i--) {
  CloudPuff p=puffs[i];if(p.alpha<.01f)continue;cloudDrawCount++;
  V3 d=norm(p.p);float solar=dot(d,sun),day=clampf((solar+.09f)/.32f,0,1);day=day*day*(3-2*day);
  float warm=clampf((solar+.03f)/.30f,0,1);
  V3 tint=add(mul(v3(1,.53f,.30f),1-warm),mul(v3(.98f,.99f,1),warm));
  float forward=fmaxf(0,dot(norm(add(p.p,mul(cameraEye,-1))),sun));forward*=forward;forward*=forward;
  V3 color=add(mul(v3(.19f,.24f,.34f),.04f+.20f*day),mul(tint,day*(.80f+.16f*forward)));
  int variant=(int)(fabsf(p.p.x*.013f)+fabsf(p.p.z*.007f))%4;
  V3 *axes=puffAxes[i],delta=add(p.p,mul(cameraEye,-1));float *weights=puffWeights[i];
  /* Front XY, top XZ, side ZY: atlas projections of the same density field. */
  for(int face=0;face<3;face++) {
   if(weights[face]<=.01f)continue;
   V3 right=mul(axes[cloudHorizontal[face]],p.size*cloudScale[cloudHorizontal[face]]);
   V3 up=mul(axes[cloudVertical[face]],p.size*cloudScale[cloudVertical[face]]);
   int tile=variant*3+face;
   for(int triangle=1;triangle<7;triangle++)for(int k=0;k<3;k++) {
    int index=k==0?0:k==1?triangle:triangle+1;float x=cloudCorners[index][0],y=cloudCorners[index][1];
    V3 v=add(delta,add(mul(right,x),mul(up,y)));
    vertices[n++]=(CloudVertex){v,((tile%4)*64+.5f+(x*.5f+.5f)*63)/256.f,((tile/4)*64+.5f+(y*.5f+.5f)*63)/256.f,color,p.alpha*weights[face]};
   }
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
