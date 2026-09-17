// SPDX-License-Identifier: MPL-2.0
#define main poor_mans_sky_application_main
#include "../src/poor-mans-sky.c"
#undef main
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);return 1; } } while(0)
static Node *referenceNode(V3 p,float *u,float *v) {
  int face;voxelFaceUV(p,&face,u,v);int id=face;
  while(!voxelCover[id] && nodes[id].child[0]>=0) {
    int x=*u>=.5f,y=*v>=.5f,next=nodes[id].child[x+2*y];if(next<0)break;
    *u=*u*2-x;*v=*v*2-y;id=next;
  }
  return voxelCover[id]?&nodes[id]:NULL;
}
int main(void) {
  V3 forward=v3(1,0,0),right=v3(0,0,1),up=v3(0,1,0);
  CHECK(voxelViewSupported(v3(0,RADIUS+35,0),forward,right,up));
  CHECK(voxelViewSupported(v3(0,RADIUS+1750,0),forward,right,up));
  CHECK(!voxelViewSupported(v3(0,RADIUS+2000,0),forward,right,up));
  CHECK(!voxelViewSupported(v3(0,RADIUS+400000,0),forward,right,up));
  CHECK(voxelViewSupported(v3(0,RADIUS+1499,0),forward,right,up));
  for(int angle=0;angle<=89;angle++) {
    float a=angle*PI/180;V3 f=v3(cosf(a),-sinf(a),0),u=v3(sinf(a),cosf(a),0);
    if(angle<=54)CHECK(voxelViewSupported(v3(0,RADIUS+35,0),f,right,u));
    if(angle==56) {
      CHECK(voxelViewSupported(v3(0,RADIUS+35,0),f,right,u));
      float mix=voxelViewMix(v3(0,RADIUS+35,0),f,right,u);CHECK(mix>0 && mix<1);
    }
    if(angle>=59)CHECK(!voxelViewSupported(v3(0,RADIUS+35,0),f,right,u));
  }
  CHECK(!voxelViewSupported(v3(0,RADIUS+35,0),forward,norm(v3(0,.01f,1)),up));
  CHECK(voxelViewMix(v3(0,RADIUS+1500,0),forward,right,up)==1);
  CHECK(voxelViewMix(v3(0,RADIUS+2000,0),forward,right,up)==0);
  CHECK(fabsf(voxelViewMix(v3(0,RADIUS+1750,0),forward,right,up)-.5f)<.001f);
  float last=1;
  for(int altitude=1500;altitude<=2000;altitude++) {
    float mix=voxelViewMix(v3(0,RADIUS+altitude,0),forward,right,up);
    CHECK(mix>=0 && mix<=last);last=mix;
  }
  static Vertex vertices[NV];static unsigned char pixels[PAGE_BYTES];
  int next=6;
  for(int face=0;face<6;face++) {
    for(int k=0;k<4;k++) {
      int id=next++;nodes[face].child[k]=id;
      for(int j=0;j<4;j++) {
        int leaf=next++;nodes[id].child[j]=leaf;voxelCover[leaf]=1;
        nodes[leaf].vertices=vertices;nodes[leaf].pixels=pixels;
      }
    }
  }
  VoxelCursor cursor={0};
  /* Traverse all faces and boundaries in both directions, including reuse
   * after crossing into a different face with otherwise identical UVs. */
  for(int pass=0;pass<2;pass++)for(int f=0;f<6;f++)for(int y=0;y<129;y++)for(int x=0;x<129;x++) {
    float u,v,a,b;V3 p=direction(f,(pass?128-x:x)/64.f-1,y/64.f-1);
    Node *n=voxelNode(p,&u,&v,&cursor),*ref=referenceNode(p,&a,&b);
    CHECK(n==ref);CHECK(fabsf(u-a)<1e-6f && fabsf(v-b)<1e-6f);
    int face;float fu,fv;voxelFaceUV(p,&face,&fu,&fv);
    CHECK(dot(p,direction(face,fu*2-1,fv*2-1))>.99999f);
  }
  CHECK(voxelLookupHits>10000);
  /* BC1 selectors exercise both endpoints and interpolants in every block,
   * then mutate storage to verify the per-frame cache invalidation contract. */
  for(int pass=0;pass<2;pass++) {
    for(int i=0;i<PAGE_BYTES;i++)pixels[i]=(unsigned char)(i*73+pass*29);
    memset(voxelBlocks,0,sizeof(voxelBlocks));
    for(int y=0;y<PAGE;y++)for(int x=0;x<PAGE;x++) {
      float rgb[3];voxelTexel(&nodes[7],x,y,rgb);
      const unsigned char *b=pixels+((y/4)*(PAGE/4)+x/4)*8;
      int c[4][3];unpack565(b[0]|b[1]<<8,c[0]);unpack565(b[2]|b[3]<<8,c[1]);
      for(int k=0;k<3;k++){c[2][k]=(2*c[0][k]+c[1][k])/3;c[3][k]=(c[0][k]+2*c[1][k])/3;}
      unsigned bits=(unsigned)b[4]|(unsigned)b[5]<<8|(unsigned)b[6]<<16|(unsigned)b[7]<<24;
      int index=(bits>>(2*((y&3)*4+(x&3))))&3;
      for(int k=0;k<3;k++)CHECK(rgb[k]==c[index][k]);
    }
  }
  for(int y=0;y<=PATCH;y++)for(int x=0;x<=PATCH;x++) {
    Vertex *p=&vertices[y*(PATCH+1)+x];p->h=3*x+5*y-20;p->n=(PackedNormal){0,127,0,0};
  }
  for(int y=0;y<100;y++)for(int x=0;x<100;x++) {
    V3 n;float u=x/100.f,v=y/100.f;
    float h=voxelHeightAt(&nodes[7],u,v,&n);
    CHECK(h==voxelHeightAt(&nodes[7],u,v,NULL));
    CHECK(fabsf(h-(3*u*PATCH+5*v*PATCH-20))<.0001f);
    CHECK(fabsf(n.y-1)<1e-6f);
  }
  /* Compact data must agree with the legacy payload, and slot reuse must
   * invalidate heights, normals and decoded colors even at identical addresses. */
  voxelScene=1;nodes[7].slot=0;
  memset(voxelBlocks,0,sizeof(voxelBlocks));
  for(int y=0;y<20;y++)for(int x=0;x<20;x++) {
    float u=x/20.f,v=y/20.f;V3 a,b;unsigned char ca[4],cb[4];
    voxelCompact=0;float h=voxelHeightAt(&nodes[7],u,v,&a);voxelColor(&nodes[7],u,v,ca);
    voxelCompact=1;CHECK(h==voxelHeightAt(&nodes[7],u,v,&b));voxelColor(&nodes[7],u,v,cb);
    CHECK(!memcmp(&a,&b,sizeof(a)));CHECK(!memcmp(ca,cb,4));
  }
  vertices[0].h=1234;memset(pixels,0,sizeof(pixels));voxelInvalidateSlot(0);
  memset(voxelBlocks,0,sizeof(voxelBlocks));
  CHECK(voxelHeightAt(&nodes[7],0,0,NULL)==1234);
  unsigned char color[4];voxelColor(&nodes[7],0,0,color);CHECK(color[0]==0 && color[1]==0 && color[2]==0);
  Vertex proxy[VOXEL_PROXY_VERTS];voxelProxyVertices(&nodes[7],proxy);
  for(int y=0;y<4;y++)for(int x=0;x<4;x++)CHECK(!memcmp(&proxy[y*4+x],&vertices[y*8*(PATCH+1)+x*8],sizeof(Vertex)));
  free(voxelPages[0]);
  puts("voxel terrain: camera/altitude crossfade, cube boundaries, coherent lookup, BC1 cache, compact payload/invalidation, proxy sampling OK");return 0;
}
