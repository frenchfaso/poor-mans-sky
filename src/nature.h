// SPDX-License-Identifier: MPL-2.0
/* Seeded spatial cells, generated off the GL thread and uploaded in bounded
 * batches. */
#define NATURE_CELLS 65536
#define NATURE_CANDIDATES 65536
#define NATURE_VIEW_DISTANCE 2500.0f
#define NATURE_STREAM_DISTANCE 2500.0f
#define NATURE_VERTS 65532
typedef struct {
  V3 p, n;
  float r, g, b, bend, u, v;
} NatureVertex;
typedef struct { V3 p; PackedNormal n; uint8_t r,g,b,bend; float u,v; } NaturePacked;
typedef struct {
  int face, x, y, state, wanted, count, solid;
  int lod, buildLod, pendingCount, pendingSolid, trees, rocks, grass;
  int pendingTrees, pendingRocks, pendingGrass;
  V3 pendingCenter;
  V3 center, boundCenter, boundHalf;
  NatureVertex *cpu;
  unsigned char *packed;
  int unique;
  unsigned char *resident;
  int residentUnique, group, nextGroup;
  V3 packedCenter, packedHalf;
  GLuint vbo, ebo;
  size_t gpuBytes;
  GLuint oldVbo,oldEbo;
  int fading,oldCount,oldLod;
  Uint32 fadeStart;
  size_t oldBytes;
  V3 oldCenter;
} NatureCell;
static NatureCell nature[NATURE_CELLS];
static int natureUsed;
static SDL_mutex *natureMutex;
static SDL_cond *natureCond;
static SDL_Thread *natureThread;
static int natureQuit, natureFrame, natureUploads, natureDraws;
static GLuint natureP, foliageTex,natureFadeP,natureFadeMask;
static int natureFading,natureFadeStarted;
static void natureFadeInit(void);
static uint32_t nrng;
static float nr(void) {
  nrng ^= nrng << 13;
  nrng ^= nrng >> 17;
  nrng ^= nrng << 5;
  return (nrng & 65535) / 65535.0f;
}
static NatureVertex *nv;
/* One nature worker: reserve worst-case scratch once in the address space,
 * instead of requesting several contiguous MiB for every new grass cell. */
static NatureVertex natureScratch[NATURE_VERTS];
static unsigned char naturePackedScratch[NATURE_VERTS*(sizeof(NaturePacked)+sizeof(uint16_t))];
static uint16_t natureIndexScratch[NATURE_VERTS];
static Uint32 natureRetryAt;
static int natureMemoryPressure,natureAllocationFailures;
static int nn, natureBuildLod;
static NatureVertex treeVariants[4][192];
static int treeCounts[4], broadLeaves;
static size_t natureGPUBytes;
static int natureVisibleTrees, natureVisibleRocks, natureVisibleGrass;
static void nt(V3 a, V3 b, V3 c, V3 color, float bend) {
  if (nn + 3 > NATURE_VERTS)
    return;
  V3 area = cross(add(b, mul(a, -1)), add(c, mul(a, -1)));
  if (dot(area, area) < 1e-10f)
    return;
  V3 normal = norm(area);
  nv[nn++] =
      (NatureVertex){a, normal, color.x, color.y, color.z, 0, .0075f, .0075f};
  nv[nn++] = (NatureVertex){b,       normal, color.x, color.y,
                            color.z, bend,   .0075f,  .0075f};
  nv[nn++] =
      (NatureVertex){c, normal, color.x, color.y, color.z, 0, .0075f, .0075f};
}
static V3 np(V3 base, V3 right, V3 up, V3 forward, float x, float y, float z) {
  return add(base, add(mul(right, x), add(mul(up, y), mul(forward, z))));
}
static void blob(V3 base, V3 right, V3 up, V3 forward, V3 size, V3 col,
                 int foliage) {
  float rough[8];
  for (int i = 0; i < 8; i++)
    rough[i] = .75f + nr() * .5f;
  int stacks = natureBuildLod ? 2 : 4, slices = natureBuildLod == 2 ? 4
                                                : natureBuildLod    ? 5
                                                                    : 7;
  for (int y = 0; y < stacks; y++)
    for (int x = 0; x < slices; x++) {
      V3 p[4];
      for (int j = 0; j < 4; j++) {
        float a = (x + (j == 1 || j == 2)) * 2 * PI / slices,
              b = -PI / 2 + (y + (j >= 2)) * PI / stacks;
        float q = rough[(x + (j == 1 || j == 2)) % slices];
        p[j] = np(base, right, up, forward, cosf(a) * cosf(b) * size.x * q,
                  sinf(b) * size.y, sinf(a) * cosf(b) * size.z * q);
      }
      V3 color = mul(col, .8f + nr() * .3f);
      int begin = nn;
      nt(p[0], p[2], p[1], color, foliage ? .15f : 0);
      nt(p[0], p[3], p[2], color, foliage ? .15f : 0);
      if (!foliage)
        for (int k = begin; k < nn; k++) {
          V3 rel = add(nv[k].p, mul(base, -1));
          float px = dot(rel, right) / size.x, py = dot(rel, up) / size.y,
                pz = dot(rel, forward) / size.z;
          V3 smooth =
              norm(add(mul(right, px / size.x),
                       add(mul(up, py / size.y), mul(forward, pz / size.z))));
          float ao = .68f + .32f * clampf(py * .5f + .5f, 0, 1);
          nv[k].r *= ao; nv[k].g *= ao; nv[k].b *= ao;
          float ax = fabsf(dot(nv[k].n, right)), ay = fabsf(dot(nv[k].n, up)),
                az = fabsf(dot(nv[k].n, forward));
          float u = ax > ay && ax > az ? pz : px,
                v = ay > ax && ay > az ? pz : py;
          nv[k].u = .02f + clampf(.5f + u * .35f, 0, 1) * .46f;
          nv[k].v = .52f + clampf(.5f + v * .35f, 0, 1) * .46f;
          nv[k].n = norm(add(mul(nv[k].n, .6f), mul(smooth, .4f)));
        }
    }
}
/* Leaf cards use radial normals: both sides receive a coherent canopy light.
 * Geometry stays batched per spatial cell, with bounded alpha overdraw. */
static void foliageCard(V3 base, V3 right, V3 up, float width, float height,
                        V3 color, int grass) {
  if (nn + 6 > NATURE_VERTS)
    return;
  V3 a = add(base, mul(right, -width)), b = add(base, mul(right, width)),
     c = add(b, mul(up, height)), d = add(a, mul(up, height));
  V3 points[6] = {a, b, c, a, c, d};
  int iu[6] = {0, 1, 1, 0, 1, 0}, iv[6] = {0, 0, 1, 0, 1, 1};
  for (int i = 0; i < 6; i++)
    nv[nn++] = (NatureVertex){points[i],
                              up,
                              color.x * (iv[i] ? 1.0f : .62f),
                              color.y * (iv[i] ? 1.0f : .62f),
                              color.z * (iv[i] ? 1.0f : .62f),
                              iv[i] * height * (grass ? .12f : .025f),
                              (.5f * grass + .03f + iu[i] * .44f) * .5f,
                              (.10f + iv[i] * .86f) * .5f};
}
static void crownCards(V3 base, V3 right, V3 up, V3 forward, float radius,
                       V3 color) {
  for (int j = 0; j < 8; j++) {
    float a = j * 2 * PI / 8 + nr() * .25f;
    V3 side = add(mul(right, cosf(a)), mul(forward, sinf(a)));
    V3 origin = add(base, add(mul(side, (nr() - .5f) * radius),
                              mul(up, (nr() - .5f) * radius * .8f)));
    int first = nn;
    foliageCard(origin, side, up, radius * (.6f + nr() * .25f), radius * 1.4f,
                color, 0);
    if (broadLeaves) for (int k=first;k<nn;k++) nv[k].u += .5f;
  }
}
static void initFoliage(void) {
  unsigned char *p = calloc(512 * 512, 4);
  uint32_t bytes = 0;
  if (!p)
    die("foliage texture");
  if (!cacheRead(4, 0, 0, 0, 0, p, 512 * 512 * 4, p, 0, &bytes)) {
    /* Procedural frond and grass masks with varied reflectance. Analytic needle
     * clusters and blades give actual silhouettes, not opaque ellipsoids. */
    for (int y = 0; y < 256; y++)
      for (int x = 0; x < 256; x++) {
        float u = (x % 128) / 128.0f, v = y / 256.0f, alpha = 0, shade = .85f;
        if (x < 128) {
          float t = (v - .10f) / .86f;
          for (int k = 0; k < 11; k++) {
            float row = .13f + k * .072f;
            float span = (1 - row) * .43f;
            float q = fabsf(u - .5f) / fmaxf(span, .02f);
            float center = row + fabsf(u - .5f) * .7f;
            float wave = sinf(u * 230 + k * 7) * .006f;
            if (q < 1 && fabsf(v - center + wave) < .012f * (1 - q) + .004f)
              alpha = 1;
          }
          if (fabsf(u - .5f) < .008f && t > 0 && t < .95f)
            alpha = 1;
        } else {
          for (int k = 0; k < 17; k++) {
            float bx = .025f + k * .059f, top = .54f + (k % 4) * .12f;
            float bend = sinf(k * 4.7f) * .15f;
            float t = (v - .1f) / top;
            float center = bx + bend * t * t;
            if (t >= 0 && t <= 1 && fabsf(u - center) < .028f * (1 - t))
              alpha = 1;
          }
        }
        shade = .65f + .35f * ((x * 17 + y * 29) % 31) / 30.0f;
        int i = (y * 512 + x) * 4;
        p[i] = p[i + 1] = p[i + 2] = (unsigned char)(shade * 255);
        p[i + 3] = (unsigned char)(alpha * 255);
        if (x < 12 && y < 12) {
          p[i] = p[i + 1] = p[i + 2] = p[i + 3] = 255;
        }
      }
    /* Broadleaf spray in the unused half of the atlas. Curved stems and
     * paired elliptical leaves, with midrib shading baked once on the CPU. */
    for (int y=0;y<256;y++) for (int x=0;x<128;x++) {
      float u=x/128.f,v=y/256.f, stem=.5f+.055f*sinf(v*5), shade=.7f;
      int covered=fabsf(u-stem)<.008f && v>.1f && v<.93f;
      for(int k=0;k<8;k++) for(int side=-1;side<=1;side+=2) {
        float cy=.19f+k*.091f,cx=.5f+.055f*sinf(cy*5)+side*(.12f+.02f*(k%3));
        float dx=u-cx,dy=v-cy, a=dx*.84f+side*dy*.54f,b=dy*.84f-side*dx*.54f;
        float q=a*a/(.145f*.145f)+b*b/(.039f*.039f);
        if(q<1) {covered=1;shade=.56f+.32f*(1-q)+.10f*cosf(b*160);}
      }
      int i=(y*512+x+256)*4;
      p[i]=p[i+1]=p[i+2]=(unsigned char)(255*shade);p[i+3]=covered?255:0;
    }
    for (int y = 0; y < 256; y++)
      for (int x = 0; x < 256; x++) {
        int i = ((y + 256) * 512 + x) * 4;
        unsigned char *material = materialMip[1][1] + (y * 256 + x) * 3;
        p[i] = material[0];
        p[i + 1] = material[1];
        p[i + 2] = material[2];
        p[i + 3] = 255;
      }
    cacheWrite(4, 0, 0, 0, 0, p, 512 * 512 * 4, p, 0);
  }
  glGenTextures(1, &foliageTex);
  glBindTexture(GL_TEXTURE_2D, foliageTex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 512, 512, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, p);
  glGenerateMipmap(GL_TEXTURE_2D);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 3);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                  GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  free(p);
}
#include "nature-models.h"
static V3 treeCandidate(int face, int x, int y, int i, uint32_t *rank) {
  uint32_t h = hash3(x, y, face + 731 + 1009 + i * 37) | 1;
  *rank = h;
  h ^= h << 13;
  h ^= h >> 17;
  h ^= h << 5;
  float a = (h & 65535) / 65535.f;
  h ^= h << 13;
  h ^= h >> 17;
  h ^= h << 5;
  float b = (h & 65535) / 65535.f;
  return direction(face, -1 + (x + a) / 4096, -1 + (y + b) / 4096);
}
static int treeSpacing(V3 d, uint32_t rank) {
  for (int face = 0; face < 6; face++) {
    float den = face < 2 ? d.x : face < 4 ? d.y : d.z;
    if (face & 1)
      den = -den;
    if (den < .4f)
      continue;
    float u = face == 0   ? -d.z / den
              : face == 1 ? d.z / den
              : face == 5 ? -d.x / den
                          : d.x / den;
    float v = face == 2 ? -d.z / den : face == 3 ? d.z / den : d.y / den;
    int cx = (int)floorf((u + 1) * 4096), cy = (int)floorf((v + 1) * 4096);
    for (int y = cy - 1; y <= cy + 1; y++)
      for (int x = cx - 1; x <= cx + 1; x++) {
        if (x < 0 || y < 0 || x >= 8192 || y >= 8192)
          continue;
        for (int k = 0; k < 7; k++) {
          uint32_t other;
          V3 q = treeCandidate(face, x, y, k, &other);
          if (other >= rank)
            continue;
          V3 delta = mul(add(d, mul(q, -1)), RADIUS);
          if (dot(delta, delta) < 36)
            return 0;
        }
      }
  }
  return 1;
}
static void buildNatureFreshInto(NatureCell *cell,NatureVertex *buffer) {
  nv = buffer;
  nn = 0;
  natureBuildLod = cell->buildLod;
  cell->trees = cell->rocks = cell->grass = 0;
  float tile = 2.0f / 8192;
  V3 centerDir = direction(cell->face, -1 + (cell->x + .5f) * tile,
                           -1 + (cell->y + .5f) * tile);
  cell->center = mul(centerDir, RADIUS + elevation(centerDir));
  const int rockCounts[3] = {7, 11, 16}, treeAttempts[3] = {3, 5, 7},
            grassAttempts[3] = {560, 975, 1575};
  for (int kind = 0; kind < 3; kind++) {
    if (kind == 2 && cell->buildLod > 0)
      break;
    int count = kind == 0   ? rockCounts[natureQuality]
                : kind == 1 ? treeAttempts[natureQuality]
                            : grassAttempts[natureQuality];
    float faceScale = fmaxf(fabsf(centerDir.x),
                            fmaxf(fabsf(centerDir.y), fabsf(centerDir.z)));
    count = (int)ceilf(count * faceScale * faceScale * faceScale);
    for (int i = 0; i < count; i++) {
      nrng =
          hash3(cell->x, cell->y, cell->face + 731 + kind * 1009 + i * 37) | 1;
      float jitterX = nr(), jitterY = nr();
      V3 d = direction(cell->face, -1 + (cell->x + jitterX) * tile,
                       -1 + (cell->y + jitterY) * tile);
      GeoCell climate;
      float h = geoElevation(d, &climate);
      if (geological && geology && kind > 0 &&
          (climate.temperature < 1 || climate.wet < .23f))
        continue;
      if (h < 2 || h > (kind == 0 ? 3400 : kind == 1 ? 2100 : 2600))
        continue;
      V3 right = norm(cross(d, v3(.01f, 1, .02f))), forward = cross(right, d);
      V3 base = add(mul(d, RADIUS + h - (kind == 0 ? .1f : 0)),
                    mul(cell->center, -1));
      V3 spawnDelta = add(add(base, cell->center), mul(spawnPoint, -1));
      if (kind != 2 && dot(spawnDelta, spawnDelta) < 144)
        continue;
      if (kind == 0) {
        if (cell->buildLod >= 3) continue;
        if (geological && geology && climate.wet > .35f &&
            climate.relief < .6f && i % 3)
          continue;
        float z = .2f + powf(nr(), 2) * 3.2f;
        if ((cell->buildLod == 1 && z < .4f) ||
            (cell->buildLod == 2 && z < 1.0f))
          continue;
        blob(base, right, d, forward, v3(z, z * (.5f + nr()), z * (.6f + nr())),
             v3(.8f, .8f, .8f), 0);
        cell->rocks++;
      } else if (kind == 1) {
        uint32_t rank;
        treeCandidate(cell->face, cell->x, cell->y, i, &rank);
        if (!treeSpacing(d, rank))
          continue;
        if (dot(surfaceNormal(d), d) < .78f ||
            noise3(mul(d, 1900)) <
                (geological ? .58f - climate.wet * .35f : .32f))
          continue;
        float height = 5 + nr() * 11, angle = nr() * 2 * PI,
              wide = .85f + nr() * .3f;
        int variant = (int)(nr() * 4) % 4;
        V3 r = add(mul(right, cosf(angle)), mul(forward, sinf(angle))),
           f = cross(r, d);
        placeTree(base, r, d, f, height, wide, variant, cell->buildLod);
        cell->trees++;
      } else {
        if (noise3(mul(d, RADIUS / 18)) < .32f)
          continue;
        float height = .24f + nr() * .65f;
        V3 color = mul(v3(.10f + nr() * .1f, .21f + nr() * .13f, .04f), .5f);
        float a = nr() * PI;
        for (int k = 0; k < 2; k++) {
          float angle = a + k * PI * .5f;
          foliageCard(base,
                      add(mul(right, cosf(angle)), mul(forward, sinf(angle))),
                      d, .48f, height, color, 1);
        }
        cell->grass++;
        /* Flower patches, not a regular every-Nth-position colour grid. */
        if (nr() < .18f) {
          V3 palette[5]={v3(.92,.79,.50),v3(.85,.28,.12),v3(.48,.31,.75),v3(.91,.82,.85),v3(.95,.65,.08)};
          int species=(int)(noise3(mul(d,RADIUS/35))*12)%5;
          V3 flower=palette[species],top=add(base,mul(d,height*.85f));
          int petals=species==2?5:8;
          for(int j=0;j<petals;j++) {
            float a=j*2*PI/petals;
            V3 axis=add(mul(right,cosf(a)),mul(forward,sinf(a))),side=cross(d,axis);
            V3 root=add(top,mul(axis,.025f)),tip=add(top,add(mul(axis,.13f),mul(d,.025f)));
            V3 mid=add(top,add(mul(axis,.085f),mul(d,-.015f)));
            nt(root,add(mid,mul(side,.032f)),tip,flower,0);
            nt(root,tip,add(mid,mul(side,-.032f)),mul(flower,.86f),0);
          }
          for(int j=0;j<6;j++) {
            float a=j*2*PI/6,b=(j+1)*2*PI/6;
            nt(add(top,mul(d,.01f)),np(top,right,d,forward,cosf(a)*.034f,.009f,sinf(a)*.034f),np(top,right,d,forward,cosf(b)*.034f,.009f,sinf(b)*.034f),v3(.45,.26,.035),0);
          }
          /* A tapered stem and paired leaves give flowers a readable body. */
          nt(add(base,mul(right,-.008f)),top,add(base,mul(right,.008f)),v3(.08,.19,.035),0);
          for(int side=-1;side<=1;side+=2) {
            V3 root=add(base,mul(d,height*.35f)),tip=add(root,add(mul(right,side*.18f),mul(d,.10f)));
            V3 mid=add(mul(add(root,tip),.5f),mul(forward,.045f));
            nt(root,mid,tip,v3(.075,.20,.04),0);
            nt(root,tip,add(mid,mul(forward,-.09f)),v3(.10,.25,.045),0);
          }
        }
        if (i % 28 == 0) {
          broadLeaves=i%56==0;
          crownCards(base,right,d,forward,.35f+nr()*.4f,v3(.055,.16,.035));
          broadLeaves=0;
        }
      }
    }
    if (kind == 1)
      cell->solid = nn;
  }
  for (int i = 0; i < nn; i++) {
    nv[i].u *= .5f;
    nv[i].v *= .5f;
  }
  cell->cpu = nv;
  cell->count = nn;
}
/* Owned fresh output for offline validation; runtime uses the reserved scratch. */
static int __attribute__((unused)) buildNatureFresh(NatureCell *cell) {
 NatureVertex *buffer=streamAlloc(NATURE_VERTS*sizeof(*buffer));
 if(!buffer){cell->cpu=NULL;return 0;}
 buildNatureFreshInto(cell,buffer);return 1;
}
/* Runtime packing only: cached procedural vertices retain their original ABI.
 */
static int indexNature(NaturePacked *vertices, int count,
                       unsigned short *indices) {
  int slots[131072] = {0};
  int unique = 0;
  for (int i = 0; i < count; i++) {
    NaturePacked value = vertices[i];
    unsigned slot = cacheHash(&value, sizeof(value), 2166136261u) & 131071;
    while (slots[slot] &&
           memcmp(&vertices[slots[slot] - 1], &value, sizeof(value)))
      slot = (slot + 1) & 131071;
    if (!slots[slot]) {
      vertices[unique] = value;
      slots[slot] = ++unique;
    }
    indices[i] = (unsigned short)(slots[slot] - 1);
  }
  return unique;
}
typedef struct {
  V3 center, boundCenter, boundHalf;
  uint32_t count, solid, trees, rocks, grass, unique;
} NatureDisk;
static int buildNature(NatureCell *cell) {
  cell->cpu=NULL;cell->packed=NULL;
  NatureDisk meta;
  const size_t capacity=NATURE_VERTS*(sizeof(NaturePacked)+sizeof(uint16_t));
  unsigned char *blob=naturePackedScratch;
  uint32_t bytes=0;
  int hit=cacheRead(2,cell->face,13+cell->buildLod+4*natureQuality,cell->x,cell->y,
    &meta,sizeof(meta),blob,capacity,&bytes);
  if(hit && meta.count<=NATURE_VERTS && meta.unique<=meta.count && meta.solid<=meta.count &&
     meta.trees<=7 && meta.rocks<=16 && meta.grass<=1575 && meta.count%3==0 && meta.solid%3==0 &&
     bytes==meta.unique*sizeof(NaturePacked)+meta.count*sizeof(uint16_t)) {
    uint16_t *indices=(uint16_t*)(blob+meta.unique*sizeof(NaturePacked));
    for(unsigned j=0;j<meta.count;j++)if(indices[j]>=meta.unique)hit=0;
  } else hit=0;
  if(!hit) {
    buildNatureFreshInto(cell,natureScratch);
    NaturePacked *v=(NaturePacked*)blob;
    V3 lo=v3(0,0,0),hi=lo;
    for(int i=0;i<cell->count;i++) {
      NatureVertex q=cell->cpu[i];
      v[i]=(NaturePacked){q.p,packNormal(q.n),(uint8_t)lrintf(clampf(q.r,0,1)*255),
        (uint8_t)lrintf(clampf(q.g,0,1)*255),(uint8_t)lrintf(clampf(q.b,0,1)*255),
        (uint8_t)lrintf(clampf(q.bend,0,1)*255),q.u,q.v};
      lo=v3(fminf(lo.x,q.p.x),fminf(lo.y,q.p.y),fminf(lo.z,q.p.z));
      hi=v3(fmaxf(hi.x,q.p.x),fmaxf(hi.y,q.p.y),fmaxf(hi.z,q.p.z));
    }
    uint16_t *indices=natureIndexScratch;
    int unique=indexNature(v,cell->count,indices);
    memcpy(blob+unique*sizeof(NaturePacked),indices,cell->count*sizeof(uint16_t));
    cell->cpu=NULL;
    meta=(NatureDisk){cell->center,add(cell->center,mul(add(lo,hi),.5f)),
      add(mul(add(hi,mul(lo,-1)),.5f),v3(.2f,.2f,.2f)),cell->count,cell->solid,
      cell->trees,cell->rocks,cell->grass,unique};
    bytes=unique*sizeof(NaturePacked)+cell->count*sizeof(uint16_t);
    cacheWrite(2,cell->face,13+cell->buildLod+4*natureQuality,cell->x,cell->y,&meta,sizeof(meta),blob,bytes);
  }
  /* Keep only the actual payload: distant/empty cells must not retain the
   * worst-case dense grass allocation in a 32-bit process. */
  size_t actualBytes=meta.unique*sizeof(NaturePacked)+meta.count*sizeof(uint16_t);
  unsigned char *compact=streamAlloc(actualBytes?actualBytes:1);
  if(!compact)return 0;
  memcpy(compact,blob,actualBytes);blob=compact;
  cell->packed=blob;cell->unique=meta.unique;cell->packedCenter=meta.boundCenter;cell->packedHalf=meta.boundHalf;
  cell->center=meta.center;cell->count=meta.count;cell->solid=meta.solid;
  cell->trees=meta.trees;cell->rocks=meta.rocks;cell->grass=meta.grass;
  return 1;
}
static int natureWork(void *arg) {
  (void)arg;
  SDL_SetThreadPriority(SDL_THREAD_PRIORITY_LOW);
  for (;;) {
    SDL_LockMutex(natureMutex);
    int id = -1;
    for (int i = 0; i < natureUsed; i++)
      if (nature[i].state == 1 &&
          (id<0 || natureRequestPriority[i]<natureRequestPriority[id]))
        id = i;
    if (natureQuit) {
      SDL_UnlockMutex(natureMutex);
      break;
    }
    Sint32 retryDelay=(Sint32)(natureRetryAt-SDL_GetTicks());
    if(retryDelay>0) {
      SDL_CondWaitTimeout(natureCond,natureMutex,(Uint32)retryDelay);
      SDL_UnlockMutex(natureMutex);continue;
    }
    if (id < 0) {
      SDL_CondWait(natureCond, natureMutex);
      SDL_UnlockMutex(natureMutex);
      continue;
    }
    nature[id].state = 2;
    NatureCell copy = nature[id];
    SDL_UnlockMutex(natureMutex);
    int ready=buildNature(&copy);
    SDL_LockMutex(natureMutex);
    if(!ready) {
      nature[id].state=1;natureMemoryPressure=1;natureRetryAt=SDL_GetTicks()+500;
      natureAllocationFailures++;
      if(natureAllocationFailures==1 || natureAllocationFailures%64==0) {
        fprintf(stderr,"NATURE_MEMORY_PRESSURE failures=%d cell=%d,%d,%d retry_ms=500; keeping existing mesh\n",natureAllocationFailures,copy.face,copy.x,copy.y);
        FILE *status=fopen("/proc/self/status","r");
        if(status){char line[256];while(fgets(line,sizeof(line),status))if(!strncmp(line,"Vm",2))fputs(line,stderr);fclose(status);}
      }
      SDL_UnlockMutex(natureMutex);continue;
    }
    nature[id].pendingCenter = copy.center;
    nature[id].cpu = copy.cpu;
    nature[id].packed=copy.packed;nature[id].unique=copy.unique;
    nature[id].packedCenter=copy.packedCenter;nature[id].packedHalf=copy.packedHalf;
    nature[id].pendingCount = copy.count;
    nature[id].pendingSolid = copy.solid;
    nature[id].pendingTrees = copy.trees;
    nature[id].pendingRocks = copy.rocks;
    nature[id].pendingGrass = copy.grass;
    nature[id].state = 3;
    SDL_UnlockMutex(natureMutex);
  }
  return 0;
}
static void natureInit(void) {
  natureP = program("nature.vert", "nature.frag");
  natureFadeInit();
  initTreeModels();
  initFoliage();
  initTreeAtlas();
  natureMutex = SDL_CreateMutex();
  natureCond = SDL_CreateCond();
  if (!natureMutex || !natureCond)
    die("nature synchronization");
  natureThread = SDL_CreateThread(natureWork, "ecosystem", NULL);
  if (!natureThread)
    die("nature worker");
}

typedef struct {
  int face, x, y;
  float distance, viewDistance;
  V3 center;
  int band;
} NatureCandidate;
static int natureNear(const void *a, const void *b) {
  const NatureCandidate *x = a, *y = b;
  return x->distance < y->distance ? -1 : x->distance > y->distance ? 1 : 0;
}
/* Enumerate actual cells on each intersected face. This also corrects the
 * smaller physical cell size at cube corners without duplicate identities. */
static int natureCandidates(V3 d, NatureCandidate *out) {
  int count = 0;
  for (int face = 0; face < 6; face++) {
    float den = face < 2 ? d.x : face < 4 ? d.y : d.z;
    if (face & 1)
      den = -den;
    if (den < .4f)
      continue;
    float u = face == 0   ? -d.z / den
              : face == 1 ? d.z / den
              : face == 5 ? -d.x / den
                          : d.x / den;
    float v = face == 2 ? -d.z / den : face == 3 ? d.z / den : d.y / den;
    int cx = (int)floorf((u + 1) * 4096), cy = (int)floorf((v + 1) * 4096);
    for (int y = cy - 160; y <= cy + 160; y++)
      for (int x = cx - 160; x <= cx + 160; x++) {
        if (x < 0 || y < 0 || x >= 8192 || y >= 8192)
          continue;
        V3 q = direction(face, -1 + (x + .5f) / 4096, -1 + (y + .5f) / 4096);
        V3 delta = mul(add(q, mul(d, -1)), RADIUS);
        float distance = sqrtf(dot(delta, delta));
        if (distance <= NATURE_STREAM_DISTANCE+(streamViewReady?128:0) && count < NATURE_CANDIDATES)
          out[count++] = (NatureCandidate){face, x, y, distance, distance, mul(q,RADIUS+elevation(q)), -1};
      }
  }
  qsort(out, count, sizeof(*out), natureNear);
  return count;
}
static int natureViewPriority(const void *a,const void *b) {
  const NatureCandidate *x=*(NatureCandidate*const*)a,*y=*(NatureCandidate*const*)b;
  if(x->band!=y->band)return x->band<y->band?-1:1;
  return x->viewDistance<y->viewDistance?-1:x->viewDistance>y->viewDistance;
}
#define NATURE_LOOKUP_SIZE (NATURE_CELLS * 4)
static int natureLookup[NATURE_LOOKUP_SIZE];
static unsigned natureBucket(int face, int x, int y) {
  return hash3(x, y, face + 917) & (NATURE_LOOKUP_SIZE - 1);
}
static void natureIndex(int id) {
  NatureCell *c = &nature[id];
  unsigned h = natureBucket(c->face, c->x, c->y);
  while (natureLookup[h]) h = (h + 1) & (NATURE_LOOKUP_SIZE - 1);
  natureLookup[h] = id + 1;
}
static int natureFind(int face, int x, int y) {
  unsigned h = natureBucket(face, x, y);
  while (natureLookup[h]) {
    int id = natureLookup[h] - 1;
    NatureCell *c = &nature[id];
    if (c->face == face && c->x == x && c->y == y) return id;
    h = (h + 1) & (NATURE_LOOKUP_SIZE - 1);
  }
  return -1;
}
#include "nature-batches.h"
#include "nature-transitions.h"
static int natureCollectPressure(void) {
 if(!natureMemoryPressure)return 0;
 int released=0;
 for(int i=0;i<natureUsed;i++) {
  NatureCell *c=&nature[i];
  if(!c->state || c->state==2 || c->state==5 || c->fading || c->wanted>=natureFrame-2)continue;
  natureDetach(i);free(c->resident);free(c->packed);free(c->cpu);
  natureGPUBytes-=c->gpuBytes;glDeleteBuffers(1,&c->vbo);glDeleteBuffers(1,&c->ebo);
  *c=(NatureCell){0};released++;
 }
 natureMemoryPressure=0;
 if(released){fprintf(stderr,"NATURE_MEMORY_RECOVERY released_cells=%d\n",released);SDL_CondSignal(natureCond);}
 return released;
}
static void natureUpdate(void) {
  natureFrame++;
  V3 d = norm(eye);
  if (sqrtf(dot(eye, eye)) - RADIUS - elevation(d) > NATURE_VIEW_DISTANCE)
    return;
  static NatureCandidate candidates[NATURE_CANDIDATES];
  static int candidateCount;
  static V3 lastCenter, lastViewEye, lastViewForward, lastViewRight;
  static int lastDirectional=-1, lastPreloading=-1;
  static float lastBubble;
  static int lastViewWidth, lastViewHeight;
  static NatureCandidate *wanted[NATURE_CANDIDATES];
  static int requestCount;
  V3 moved = mul(add(d, mul(lastCenter, -1)), RADIUS);
  /* The padded spatial list survives small movements; view filtering is
   * independent so turning in place updates the cone without re-enumeration. */
  int spatialChange=!candidateCount || dot(moved,moved)>(streamViewReady?1024:16) || lastDirectional!=streamViewReady;
  if (spatialChange) {
    candidateCount = natureCandidates(d, candidates);
    lastCenter = d;
  }
  int viewChange=spatialChange || lastPreloading!=preloading || lastBubble!=streamBubble ||
    lastViewWidth!=width || lastViewHeight!=height ||
    memcmp(&lastViewEye,&streamPriorityEye,sizeof(V3)) ||
    memcmp(&lastViewForward,&viewForward,sizeof(V3)) || memcmp(&lastViewRight,&viewRight,sizeof(V3));
  if(viewChange) {
    requestCount=0;
    for(int i=0;i<candidateCount;i++) {
      NatureCandidate *c=&candidates[i];
      if(preloading && c->distance>fmaxf(250,streamViewReady?streamBubble:0))continue;
      if(streamViewReady) {
        V3 delta=add(c->center,mul(streamPriorityEye,-1));
        c->viewDistance=sqrtf(dot(delta,delta));
        c->band=streamBand(c->center,55,c->band);
        if(c->band==5 || c->viewDistance>NATURE_STREAM_DISTANCE+55)continue;
      } else {c->band=0;c->viewDistance=c->distance;}
      wanted[requestCount++]=c;
    }
    if(streamViewReady)qsort(wanted,requestCount,sizeof(*wanted),natureViewPriority);
    lastViewEye=streamPriorityEye;lastViewForward=viewForward;lastViewRight=viewRight;
    lastPreloading=preloading;lastBubble=streamBubble;lastDirectional=streamViewReady;
    lastViewWidth=width;lastViewHeight=height;
  }

  int requested = 0;
  SDL_LockMutex(natureMutex);
  for(int i=0;i<natureUsed;i++)
    if(nature[i].fading && SDL_GetTicks()-nature[i].fadeStart>=NATURE_FADE_MS) natureFadeFinish(i);
  memset(natureLookup, 0, sizeof(natureLookup));
  for (int i = 0; i < natureUsed; i++)
    if (nature[i].state) natureIndex(i);
  for(int i=0;i<natureUsed;i++)natureRequestPriority[i]=1e20f;
  /* Protect wanted cells before replacement; off-cone cells remain cached. */
  for (int k = 0; k < requestCount; k++) {
    int id = natureFind(wanted[k]->face, wanted[k]->x, wanted[k]->y);
    if (id >= 0) {nature[id].wanted = natureFrame;natureRequestPriority[id]=(float)k;}
  }
  if(natureCollectPressure()) {
    memset(natureLookup,0,sizeof(natureLookup));
    for(int i=0;i<natureUsed;i++)if(nature[i].state)natureIndex(i);
  }
  int freeCursor = 0;
  for (int candidate = 0; candidate < requestCount; candidate++) {
    int face = wanted[candidate]->face, x = wanted[candidate]->x,
        y = wanted[candidate]->y;
    float distance = wanted[candidate]->viewDistance;
    int targetLod = distance < 65 ? 0 : distance < 125 ? 1 : distance < 400 ? 2 : 3;
    int id = natureFind(face, x, y);
    if (id < 0 && requested < 4)
      for (int i = freeCursor; i <= natureUsed && i < NATURE_CELLS; i++)
        if (!nature[i].state ||
            (nature[i].state == 4 && !nature[i].fading && nature[i].wanted < natureFrame - 90)) {
          id = i;
          freeCursor = i + 1;
          if (i == natureUsed) natureUsed++;
          natureDetach(i);free(nature[i].resident);
          natureGPUBytes -= nature[i].gpuBytes;
          glDeleteBuffers(1, &nature[i].ebo);
          glDeleteBuffers(1, &nature[i].vbo);
          nature[i] = (NatureCell){0};
          nature[i].face = face;
          nature[i].x = x;
          nature[i].y = y;
          nature[i].buildLod = targetLod;
          nature[i].state = 1;
          natureRequestPriority[i]=(float)candidate;
          requested++;
          SDL_CondSignal(natureCond);
          break;
        }
    if (id >= 0) {
      NatureCell *c = &nature[id];
      c->wanted = natureFrame;
      natureRequestPriority[id]=(float)candidate;
      if (c->state == 4) {
        int lod = c->lod;
        if (lod == 0 && distance > 75)
          lod = 1;
        else if (lod == 1 && distance < 55)
          lod = 0;
        else if (lod == 1 && distance > 140)
          lod = 2;
        else if (lod == 2 && distance < 110)
          lod = 1;
        else if (lod == 2 && distance > 440)
          lod = 3;
        else if (lod == 3 && distance < 360)
          lod = 2;
        if (lod != c->lod && !c->fading && requested < 4) {
          requested++;
          c->buildLod = lod;
          c->state = 1;
          SDL_CondSignal(natureCond);
        }
      }
    }
  }
  for (int i = 0; i < natureUsed; i++)
    if (nature[i].state == 3) {
      NatureCell *c = &nature[i];
      if(!preloading && natureFading>=NATURE_FADE_LIMIT)break;
      if(!vramReserve(c->unique*sizeof(NaturePacked)+(size_t)c->pendingCount*32))break;
      c->state = 5;
      SDL_UnlockMutex(natureMutex);
      natureDetach(i);natureFadeBegin(c);free(c->resident);
      natureGPUBytes -= c->gpuBytes;
      glDeleteBuffers(1, &c->ebo);
      glDeleteBuffers(1, &c->vbo);
      c->center = c->pendingCenter;
      c->count = c->pendingCount;
      c->solid = c->pendingSolid;
      c->trees = c->pendingTrees;
      c->rocks = c->pendingRocks;
      c->grass = c->pendingGrass;
      c->lod = c->buildLod;
      c->boundCenter=c->packedCenter;c->boundHalf=c->packedHalf;
      c->ebo = c->vbo = 0;
      if (c->count && (c->lod < 2 || c->fading)) {
      glGenBuffers(1,&c->ebo);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,c->ebo);
      glBufferData(GL_ELEMENT_ARRAY_BUFFER,c->count*sizeof(uint16_t),
        c->packed+c->unique*sizeof(NaturePacked),GL_STATIC_DRAW);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);
      c->gpuBytes=c->unique*sizeof(NaturePacked)+c->count*sizeof(uint16_t);
      glGenBuffers(1,&c->vbo);glBindBuffer(GL_ARRAY_BUFFER,c->vbo);
      glBufferData(GL_ARRAY_BUFFER,c->unique*sizeof(NaturePacked),c->packed,GL_STATIC_DRAW);
      } else c->gpuBytes = 0;
      c->resident=c->packed;c->residentUnique=c->unique;c->packed=NULL;
      if(!c->fading)natureAttach(i);
      SDL_LockMutex(natureMutex);
      c->state = 4;
      natureGPUBytes += c->gpuBytes;
      natureUploads++;
      break;
    }
  natureRebuildGroups();
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  SDL_UnlockMutex(natureMutex);
}
typedef struct {int id;float depth;} NatureDrawItem;
static NatureDrawItem natureDrawOrder[NATURE_CELLS];
static int natureDrawCompare(const void *aa,const void *bb) {
 const NatureDrawItem *a=aa,*b=bb;return (a->depth>b->depth)-(a->depth<b->depth);
}
static void natureDrawPass(int reflected) {
  for(int variant=0;variant<(reflected?1:2);variant++) {
    GLuint p=variant?natureFadeP:natureP;glUseProgram(p);
    u3(p,"sun",sun);u3(p,"fogColor",fogColor);u3(p,"ambientLight",ambientLight);
    u3(p,"sunLight",sunLight);u3(p,"ambientUp",norm(cameraEye));u1(p,"exposure",sceneExposure);
    tex(p,"foliageTex",0,foliageTex);
    if(variant)tex(p,"lodMask",1,natureFadeMask);
  }
  glActiveTexture(GL_TEXTURE0);
  glEnableClientState(GL_VERTEX_ARRAY);
  glEnableClientState(GL_NORMAL_ARRAY);
  glEnableClientState(GL_COLOR_ARRAY);
  glEnableClientState(GL_TEXTURE_COORD_ARRAY);
  glDisable(GL_CULL_FACE);
  if(overdrawView){glEnable(GL_BLEND);glBlendFunc(GL_ONE,GL_ONE);glDepthMask(GL_FALSE);glDisable(GL_DEPTH_TEST);}
  natureDraws = natureVisibleTrees = natureVisibleRocks = natureVisibleGrass =
      0;
  int drawCandidates=0;
  for(int i=0;i<natureUsed;i++) {
    NatureCell *c=&nature[i];if((!c->vbo && !c->oldVbo)||c->wanted<natureFrame-1)continue;
    V3 d=add(c->boundCenter,mul(cameraEye,-1));
    if(dot(d,d)>(reflected?700.f*700.f:(NATURE_VIEW_DISTANCE+200)*(NATURE_VIEW_DISTANCE+200)))continue;
    natureDrawOrder[drawCandidates++]=(NatureDrawItem){i,dot(d,viewForward)};
  }
  qsort(natureDrawOrder,drawCandidates,sizeof(*natureDrawOrder),natureDrawCompare);
  for (int item = 0; item < drawCandidates; item++) {
    int i=natureDrawOrder[item].id;

    NatureCell *c = &nature[i];
    if ((!c->vbo && !c->oldVbo) || c->wanted < natureFrame - 1)
      continue;
    V3 offset = add(c->center, mul(cameraEye, -1));
    float dist = sqrtf(dot(offset, offset));
    if (dist > (reflected ? 600 : NATURE_VIEW_DISTANCE) ||
        (!c->fading && (cullingMode ? !boxInFrustum(c->boundCenter, c->boundHalf)
                     : dot(offset, cullForward) < dist * .55f - 65)))
      continue;
    float t=clampf((SDL_GetTicks()-c->fadeStart)/(float)NATURE_FADE_MS,0,1);t=t*t*(3-2*t);
    GLuint p=overdrawView?overdrawP:c->fading&&!reflected?natureFadeP:natureP;glUseProgram(p);
    for(int old=0;old<(c->fading&&!reflected?2:1);old++) {
      GLuint vb=old?c->oldVbo:c->vbo,ib=old?c->oldEbo:c->ebo;
      int count=old?c->oldCount:reflected?c->solid:c->count;
      if(!vb || !count)continue;
      V3 delta=old?add(c->oldCenter,mul(cameraEye,-1)):offset;
      glPushMatrix();glTranslatef(delta.x,delta.y,delta.z);u3(p,"offset",delta);
      naturePointers(vb);u1(p,"alphaCutoff",(old?c->oldLod:c->lod)>=2?.25f:.4f);
      if(c->fading&&!reflected)u2(p,"fadeRange",old?t:0,old?1:t);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,ib);glDrawElements(GL_TRIANGLES,count,GL_UNSIGNED_SHORT,0);
      glPopMatrix();triangles+=count/3;natureDraws++;
    }
    natureVisibleTrees+=c->trees;natureVisibleRocks+=c->rocks;
    if(!reflected)natureVisibleGrass+=c->grass;
  }
  glUseProgram(natureP);
  natureGroupDraws=0;
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);
  u1(natureP,"alphaCutoff",.25f);
  for(int k=0;k<natureGroupUsed;k++) {
    NatureGroup *g=&natureGroups[k];
    if(!g->vbo || !g->count) continue;
    V3 delta=add(g->boundCenter,mul(cameraEye,-1));
    float radius=sqrtf(dot(g->boundHalf,g->boundHalf));
    float distance=sqrtf(dot(delta,delta));
    if(distance-radius>(reflected?600:NATURE_VIEW_DISTANCE) ||
       !boxInFrustum(g->boundCenter,g->boundHalf)) continue;
    V3 offset=add(g->center,mul(cameraEye,-1));
    glPushMatrix();glTranslatef(offset.x,offset.y,offset.z);
    u3(natureP,"offset",offset);naturePointers(g->vbo);
    if(overdrawView)glUseProgram(overdrawP);
    glDrawArrays(GL_TRIANGLES,0,g->count);
    if(overdrawView)glUseProgram(natureP);
    glPopMatrix();triangles+=g->count/3;natureDraws++;natureGroupDraws++;
    natureVisibleTrees+=g->trees;natureVisibleRocks+=g->rocks;
  }
  glDisableClientState(GL_VERTEX_ARRAY);
  glDisableClientState(GL_NORMAL_ARRAY);
  glDisableClientState(GL_COLOR_ARRAY);
  glDisableClientState(GL_TEXTURE_COORD_ARRAY);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
  glEnable(GL_CULL_FACE);
  if(overdrawView){glDisable(GL_BLEND);glDepthMask(GL_TRUE);glEnable(GL_DEPTH_TEST);}
}
static void natureDraw(void) { natureDrawPass(0); }
static void natureClose(void) {
  SDL_LockMutex(natureMutex);
  natureQuit = 1;
  SDL_CondSignal(natureCond);
  SDL_UnlockMutex(natureMutex);
  SDL_WaitThread(natureThread, NULL);
  for (int i = 0; i < natureUsed; i++) {
    glDeleteBuffers(1,&nature[i].oldVbo);glDeleteBuffers(1,&nature[i].oldEbo);
    free(nature[i].resident);
    free(nature[i].cpu);
    free(nature[i].packed);
    glDeleteBuffers(1, &nature[i].vbo);
    glDeleteBuffers(1, &nature[i].ebo);
  }
  for(int k=0;k<natureGroupUsed;k++) glDeleteBuffers(1,&natureGroups[k].vbo);
  SDL_DestroyMutex(natureMutex);
  glDeleteTextures(1, &foliageTex);
  glDeleteTextures(1,&natureFadeMask);glDeleteProgram(natureFadeP);
  SDL_DestroyCond(natureCond);
}
