// SPDX-License-Identifier: MPL-2.0
/* A static 3x3-cell patch for the small reflection target. No skirts,
 * morphing, stitching or full-resolution terrain buffer uploads. */
#define VOXEL_PROXY_SIDE 4
#define VOXEL_PROXY_VERTS (VOXEL_PROXY_SIDE*VOXEL_PROXY_SIDE)
static GLuint voxelProxyEBO;
static int voxelProxyUploads,voxelFallbackUploads;
static void voxelUploadAtlas(Node *n) {
  if(n->atlasReady)return;
  int slot=n->slot;
  glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,atlasTex[slot/256]);
  glCompressedTexSubImage2D(GL_TEXTURE_2D,0,(slot%16)*PAGE,((slot%256)/16)*PAGE,
    PAGE,PAGE,GL_COMPRESSED_RGB_S3TC_DXT1_EXT,PAGE_BYTES,n->pixels);
  n->atlasReady=1;
}
static void voxelProxyVertices(const Node *n,Vertex *out) {
  for(int y=0;y<VOXEL_PROXY_SIDE;y++)for(int x=0;x<VOXEL_PROXY_SIDE;x++)
    out[y*VOXEL_PROXY_SIDE+x]=n->vertices[(y*8)*(PATCH+1)+x*8];
}
static void voxelSetVBO(Node *n,const Vertex *vertices,size_t bytes) {
  if(bytes>n->vboBytes && !vramReserve(bytes-n->vboBytes))die("terrain proxy/fallback VRAM budget");
  if(!n->vbo)glGenBuffers(1,&n->vbo);
  glBindBuffer(GL_ARRAY_BUFFER,n->vbo);
  glBufferData(GL_ARRAY_BUFFER,bytes,vertices,GL_STATIC_DRAW);
  terrainGPUBytes=terrainGPUBytes-n->vboBytes+bytes;n->vboBytes=bytes;
}
static void voxelProxyDraw(Node *n) {
  voxelUploadAtlas(n);
  if(n->vboBytes!=VOXEL_PROXY_VERTS*sizeof(Vertex)) {
    Vertex vertices[VOXEL_PROXY_VERTS];voxelProxyVertices(n,vertices);
    voxelSetVBO(n,vertices,sizeof(vertices));voxelProxyUploads++;
  }
  if(!voxelProxyEBO) {
    unsigned short ix[54];int k=0;
    for(int y=0;y<3;y++)for(int x=0;x<3;x++) {
      int a=y*4+x,b=a+1,c=a+4,d=c+1;
      ix[k++]=a;ix[k++]=b;ix[k++]=c;ix[k++]=b;ix[k++]=d;ix[k++]=c;
    }
    glGenBuffers(1,&voxelProxyEBO);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,voxelProxyEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof(ix),ix,GL_STATIC_DRAW);
  }
  glBindBuffer(GL_ARRAY_BUFFER,n->vbo);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,voxelProxyEBO);
  glVertexPointer(3,GL_FLOAT,sizeof(Vertex),(void*)offsetof(Vertex,p));
  glNormalPointer(GL_BYTE,sizeof(Vertex),(void*)offsetof(Vertex,n));
  glTexCoordPointer(3,GL_FLOAT,sizeof(Vertex),(void*)offsetof(Vertex,u));
  glDrawElements(GL_TRIANGLES,54,GL_UNSIGNED_SHORT,0);triangles+=18;
}
