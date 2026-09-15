// SPDX-License-Identifier: MPL-2.0
/* Both worlds share disk packs, allocation policy, worker and VRAM budget. */
#include "moon-mesh.h"
#define MOON_SLOTS 256
 typedef struct {int face,level,x,y,stamp,valid;V3 center;GLuint vbo;} MoonPatch;
static MoonPatch moonPatches[MOON_SLOTS];
static GLuint moonP,moonAirP,moonDrawP,moonRock,homeGlobe;
static int moonDraws,moonBuilds,moonPlanning,moonBaking,moonUploads,moonRequestOnly;
static float moonLodScale=.9f;
static MoonPatch *moonSelected[MOON_SLOTS];
static int moonSelectedCount;
#include "moon-stream.h"
static void moonRememberGrid(int slot,const MoonVertex *v);
static MoonPatch *moonPatch(int face,int level,int x,int y) {
 int slot=-1;
 for(int i=0;i<MOON_SLOTS;i++) {
  MoonPatch *p=&moonPatches[i];
  if(p->valid && p->face==face && p->level==level && p->x==x && p->y==y){p->stamp=frameNo;return p;}
  if(!p->valid || (p->stamp<frameNo && (slot<0 || p->stamp<moonPatches[slot].stamp)))slot=i;
 }
 if(slot<0)return NULL;
 MoonPatch *p=&moonPatches[slot];
 /* Requests never perform disk access or generation on the GL thread. */
 SDL_LockMutex(mutex);
 MoonRAM *r=moonRequest(face,level,x,y);
 if(!r || r->state!=3 || moonUploads>=4){SDL_UnlockMutex(mutex);return NULL;}
 r->pins++;V3 center=r->center;MoonVertex *vertices=r->vertices;
 SDL_UnlockMutex(mutex);
 if(!p->vbo){if(!vramReserve(MOON_VERTS*sizeof(MoonVertex))){SDL_LockMutex(mutex);r->pins--;SDL_UnlockMutex(mutex);return NULL;}glGenBuffers(1,&p->vbo);}
 glBindBuffer(GL_ARRAY_BUFFER,p->vbo);glBufferData(GL_ARRAY_BUFFER,MOON_VERTS*sizeof(MoonVertex),vertices,GL_STATIC_DRAW);
 moonRememberGrid(slot,vertices);
 SDL_LockMutex(mutex);r->pins--;SDL_UnlockMutex(mutex);
 moonUploads++;p->center=center;
 p->face=face;p->level=level;p->x=x;p->y=y;p->stamp=frameNo;p->valid=1;return p;
}
/* Collect a disjoint cover before drawing: unfinished children retain their
 * parent, never a partly refined mesh with holes or overlapping surfaces. */
static int moonNode(int face,int level,int x,int y) {
 float span=2.f/(1<<level);V3 d=direction(face,-1+(x+.5f)*span,-1+(y+.5f)*span);
 V3 localEye=moonLocalPoint(cameraEye),c=mul(d,MOON_RADIUS+moonHeight(d));
 float extent=span*MOON_RADIUS*1.6f,dist=sqrtf(dot(add(localEye,mul(c,-1)),add(localEye,mul(c,-1))));
 if(dot(d,localEye)<MOON_RADIUS-2500-extent)return 1;
 if(moonRequestOnly){SDL_LockMutex(mutex);moonRequest(face,level,x,y);SDL_UnlockMutex(mutex);}
 if(!moonPlanning && !moonBaking && !moonRequestOnly) {
  V3 relative=moonVector(add(c,mul(localEye,-1)));float z=dot(relative,viewForward),tx=.5773503f*width/height,ty=.5773503f;
  if(z < -extent || fabsf(dot(relative,viewRight))>z*tx+extent*sqrtf(1+tx*tx) || fabsf(dot(relative,viewUp))>z*ty+extent*sqrtf(1+ty*ty))return 1;
 }
 if(level<13 && dist<extent*moonLodScale) {
  int order[4]={0,1,2,3};float distances[4];
  for(int k=0;k<4;k++){V3 q=direction(face,-1+(x*2+(k&1)+.5f)*span*.5f,-1+(y*2+(k>>1)+.5f)*span*.5f);distances[k]=-dot(q,localEye);}
  for(int i=1;i<4;i++){int k=order[i],j=i;while(j>0 && distances[order[j-1]]>distances[k]){order[j]=order[j-1];j--;}order[j]=k;}
  int before=moonSelectedCount,complete=1;
  for(int i=0;i<4;i++){int k=order[i];if(!moonNode(face,level+1,x*2+(k&1),y*2+(k>>1)))complete=0;}
  if(complete || moonPlanning || moonBaking || moonRequestOnly)return 1;
  moonSelectedCount=before; /* Roll back children before selecting their parent. */
 }
 if(moonPlanning){moonDraws++;return 1;}
 if(moonRequestOnly)return 1;
 if(moonBaking){
  MoonVertex vertices[MOON_VERTS];V3 center;uint32_t bytes;
  if(!cacheRead(6,face,level,x,y,&center,sizeof(center),vertices,sizeof(vertices),&bytes) || bytes!=sizeof(vertices)) {
    moonMesh(face,level,x,y,&center,vertices);cacheWrite(6,face,level,x,y,&center,sizeof(center),vertices,sizeof(vertices));moonBuilds++;
  }moonDraws++;return 1;
 }
 MoonPatch *p=moonPatch(face,level,x,y);if(!p || moonSelectedCount==MOON_SLOTS)return 0;
 moonSelected[moonSelectedCount++]=p;return 1;
}
static float moonMorphFactor(const MoonPatch *p) {
 if(!p->level)return 1;
 float span=2.f/(1<<(p->level-1));V3 c=moonPoint(p->face,-1+(p->x/2+.5f)*span,-1+(p->y/2+.5f)*span);
 V3 delta=add(c,mul(moonLocalPoint(cameraEye),-1));
 float threshold=span*MOON_RADIUS*1.6f*moonLodScale;
 float t=clampf((threshold-sqrtf(dot(delta,delta)))/fmaxf(1,threshold*.25f),0,1);
 return t*t*(3-2*t);
}
#include "moon-seams.h"
static void moonDrawPatch(const MoonPatch *p) {
 u1(moonDrawP,"morph",1);
 V3 radial=norm(p->center),tu=p->face==0?v3(0,0,-1):p->face==1?v3(0,0,1):p->face==5?v3(-1,0,0):v3(1,0,0);
 V3 tv=p->face==2?v3(0,0,-1):p->face==3?v3(0,0,1):v3(0,1,0);
 tu=norm(add(tu,mul(radial,-dot(tu,radial))));tv=norm(add(tv,mul(radial,-dot(tv,radial))));
 glUniform2f(uniformLocation(moonDrawP,"bumpSun"),dot(tu,moonLocalVector(sun)),dot(tv,moonLocalVector(sun)));
 u3(moonDrawP,"localEye",add(moonLocalPoint(cameraEye),mul(p->center,-1)));
 V3 offset=add(moonWorldPoint(p->center),mul(cameraEye,-1));
 u3(moonDrawP,"patchCenter",p->center);
 glPushMatrix();glTranslatef(offset.x,offset.y,offset.z);float rotation[16]={celestial.x.x,celestial.x.y,celestial.x.z,0,celestial.y.x,celestial.y.y,celestial.y.z,0,celestial.z.x,celestial.z.y,celestial.z.z,0,0,0,0,1};glMultMatrixf(rotation);glBindBuffer(GL_ARRAY_BUFFER,p->vbo);
 glVertexPointer(3,GL_FLOAT,sizeof(MoonVertex),(void*)offsetof(MoonVertex,p));
 glNormalPointer(GL_FLOAT,sizeof(MoonVertex),(void*)offsetof(MoonVertex,n));
 glTexCoordPointer(2,GL_FLOAT,sizeof(MoonVertex),(void*)offsetof(MoonVertex,u));
 glClientActiveTexture(GL_TEXTURE1);glEnableClientState(GL_TEXTURE_COORD_ARRAY);
 glTexCoordPointer(3,GL_FLOAT,sizeof(MoonVertex),(void*)offsetof(MoonVertex,coarse));
 glClientActiveTexture(GL_TEXTURE0);
 glDrawArrays(GL_TRIANGLES,0,MOON_VERTS);
 glClientActiveTexture(GL_TEXTURE1);glDisableClientState(GL_TEXTURE_COORD_ARRAY);glClientActiveTexture(GL_TEXTURE0);
 glPopMatrix();moonDraws++;triangles+=MOON_VERTS/3;
}
static float moonRockHeight(int x,int y) {
 unsigned char *p=materialMip[1][1]+(((y+256)%256)*256+(x+256)%256)*3;
 return (p[0]*.299f+p[1]*.587f+p[2]*.114f)/255.f;
}
static void moonInit(void) {
 if(!moonRock) {
  if(!moonP)moonP=program("moon.vert","moon.frag");
  if(!moonAirP)moonAirP=program("moon-air.vert","moon-air.frag");
  glGenTextures(1,&moonRock);glBindTexture(GL_TEXTURE_2D,moonRock);
  unsigned char pixels[256*256*3];
  for(int y=0;y<256;y++)for(int x=0;x<256;x++) {
   int i=(y*256+x)*3;pixels[i]=(unsigned char)(moonRockHeight(x,y)*255);
   pixels[i+1]=(unsigned char)(clampf(.5f+(moonRockHeight(x+1,y)-moonRockHeight(x-1,y))*2,0,1)*255);
   pixels[i+2]=(unsigned char)(clampf(.5f+(moonRockHeight(x,y+1)-moonRockHeight(x,y-1))*2,0,1)*255);
  }
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,256,256,0,GL_RGB,GL_UNSIGNED_BYTE,pixels);glGenerateMipmap(GL_TEXTURE_2D);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
 }
}
#include "moon-atmosphere.h"
static void moonDraw(void) {
 SDL_LockMutex(mutex);streamPriorityEye=eye;streamPriorityFrame=frameNo;SDL_UnlockMutex(mutex);
 moonInit();
 moonDraws=0;moonUploads=0;moonDrawP=moonAtmosphereNeeded()?moonAirP:moonP;glUseProgram(moonDrawP);glColor3f(.48,.49,.51);tex(moonDrawP,"rockTex",0,moonRock);u3(moonDrawP,"sun",moonLocalVector(sun));
 u3(moonDrawP,"planetDir",moonLocalVector(mul(norm(moonCenter()),-1)));u1(moonDrawP,"planetshine",lunarPlanetshine());
 u3(moonDrawP,"bodyCenter",moonCenter());u3(moonDrawP,"worldSun",sun);
 float rotation[9]={celestial.x.x,celestial.x.y,celestial.x.z,celestial.y.x,celestial.y.y,celestial.y.z,celestial.z.x,celestial.z.y,celestial.z.z};
 glUniformMatrix3fv(uniformLocation(moonDrawP,"bodyRotation"),1,GL_FALSE,rotation);
 if(moonDrawP==moonAirP)moonAtmosphereUniforms(moonDrawP);
 glEnable(GL_DEPTH_TEST);glDepthMask(GL_TRUE);glDisable(GL_BLEND);glDisable(GL_CULL_FACE);
 glEnableClientState(GL_VERTEX_ARRAY);glEnableClientState(GL_NORMAL_ARRAY);glEnableClientState(GL_TEXTURE_COORD_ARRAY);
 glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);
 int faces[6]={0,1,2,3,4,5};V3 me=moonLocalPoint(cameraEye);
 for(int i=1;i<6;i++){int f=faces[i],j=i;while(j>0 && dot(direction(f,0,0),me)>dot(direction(faces[j-1],0,0),me)){faces[j]=faces[j-1];j--;}faces[j]=f;}
 size_t meshBytes=MOON_VERTS*sizeof(MoonVertex),held=0;
 for(int i=0;i<MOON_SLOTS;i++)if(moonPatches[i].vbo)held+=meshBytes;
 size_t wanted=220*meshBytes;
 vramReserve(wanted>held?wanted-held:0);
 size_t estimated=vramEstimate(),available=estimated<56u*1048576u?56u*1048576u-estimated:0;
 int patchBudget=(int)((available+held)/meshBytes);if(patchBudget>220)patchBudget=220;if(patchBudget<6)patchBudget=6;
 moonPlanning=1;moonLodScale=.9f;
 do {moonDraws=0;for(int i=0;i<6;i++)moonNode(faces[i],0,0,0);if(moonDraws<=patchBudget)break;moonLodScale*=.85f;}while(moonLodScale>.1f);
 moonPlanning=0;moonDraws=0;moonSelectedCount=0;
 /* Prefetch the complete radial cover, including behind the camera. */
 moonRequestOnly=1;for(int i=0;i<6;i++)moonNode(faces[i],0,0,0);moonRequestOnly=0;
 for(int i=0;i<6;i++)moonNode(faces[i],0,0,0);
 moonStitch();
 for(int i=0;i<moonSelectedCount;i++)moonDrawPatch(moonSelected[i]);
 glDisableClientState(GL_VERTEX_ARRAY);glDisableClientState(GL_NORMAL_ARRAY);glDisableClientState(GL_TEXTURE_COORD_ARRAY);glBindBuffer(GL_ARRAY_BUFFER,0);glEnable(GL_CULL_FACE);
}
static void moonClose(void){moonClearRAM();for(int i=0;i<MOON_SLOTS;i++)glDeleteBuffers(1,&moonPatches[i].vbo);glDeleteBuffers(1,&homeGlobe);glDeleteTextures(1,&moonRock);glDeleteProgram(moonP);glDeleteProgram(moonAirP);}
