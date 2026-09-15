// SPDX-License-Identifier: MPL-2.0
/* Four seed-stable tree prototypes. Their distant crossed impostors are baked
 * once with the same silhouettes and albedo, then lit at runtime. */
static void initTreeModels(void) {
  for (int variant = 0; variant < 4; variant++) {
    nv = treeVariants[variant];
    nn = 0;
    nrng = hash3(variant, 917, 316) | 1;
    V3 base = v3(0, 0, 0), right = v3(1, 0, 0), up = v3(0, 1, 0),
       forward = v3(0, 0, 1);
    float height = 12, radius = .12f + height * .018f;
    for (int k = 0; k < 6; k++) {
      float a = k * 2 * PI / 6, b = (k + 1) * 2 * PI / 6;
      nt(np(base, right, up, forward, cosf(a) * radius, 0, sinf(a) * radius),
         mul(up, height),
         np(base, right, up, forward, cosf(b) * radius, 0, sinf(b) * radius),
         v3(.22, .14, .075), 0);
    }
    broadLeaves = variant != 0;
    for (int k = 0; k < 3; k++) {
      /* Pine, spreading oak, slender birch, layered rounded crown.
       * Same card budget, distinct branch placement and crown proportions. */
      float radii[4][3]={{.27f,.23f,.19f},{.28f,.29f,.23f},{.17f,.18f,.14f},{.25f,.22f,.26f}};
      float levels[4][3]={{.56f,.73f,.90f},{.48f,.64f,.78f},{.48f,.70f,.91f},{.48f,.66f,.82f}};
      V3 colors[4]={v3(.045,.12,.035),v3(.10,.20,.045),v3(.16,.27,.065),v3(.07,.17,.075)};
      V3 crown=add(mul(up,height*levels[variant][k]),add(mul(right,(nr()-.5f)*height*(variant==1?.25f:.12f)),mul(forward,(nr()-.5f)*height*.15f)));
      crownCards(crown,right,up,forward,height*radii[variant][k],mul(colors[variant],.88f+nr()*.24f));
    }
    broadLeaves=0;
    treeCounts[variant] = nn;
  }
  nv = NULL;
  nn = 0;
}
static void placeTree(V3 base, V3 right, V3 up, V3 forward, float height,
                      float wide, int variant, int lod) {
  float scale = height / 12;
  if (lod < 2) {
    for (int j = 0; j < treeCounts[variant] && nn < NATURE_VERTS; j++) {
      /* Keep trunk and alternate complete crown cards in the medium LOD. */
      if (lod == 1 && j >= 18 && ((j - 18) / 6) % 2) continue;
      NatureVertex v = treeVariants[variant][j];
      if (j < 18) { float ao=.65f+.35f*clampf(v.p.y/3,0,1);
        v.r*=ao;v.g*=ao;v.b*=ao; }
      v.p = np(base, right, up, forward, v.p.x * scale * wide, v.p.y * scale,
               v.p.z * scale * wide);
      v.n = norm(add(mul(right, v.n.x / wide),
                     add(mul(up, v.n.y), mul(forward, v.n.z / wide))));
      v.bend *= scale;
      nv[nn++] = v;
    }
  } else {
    int ids[6] = {0, 1, 2, 0, 2, 3};
    for (int view = 0; view < 2; view++) {
      V3 side = view ? forward : right;
      V3 a = add(base, mul(side, -6 * scale * wide)),
         b = add(base, mul(side, 6 * scale * wide));
      V3 corners[4] = {a, b, add(b, mul(up, 16 * scale)),
                       add(a, mul(up, 16 * scale))};
      for (int j = 0; j < 6 && nn < NATURE_VERTS; j++) {
        int k = ids[j];
        float u = (k == 1 || k == 2) ? 1 : 0, v = k >= 2 ? 1 : 0;
        /* Coordinates in 512px units; the caller scales all UVs by 1/2. */
        nv[nn++] = (NatureVertex){corners[k],
                                  up,
                                  1,
                                  1,
                                  1,
                                  0,
                                  (variant + (u * .992f + .004f)) * .5f,
                                  (2 + view + (v * .992f + .004f)) * .5f};
      }
    }
  }
}
static void initTreeAtlas(void) {
  const size_t bytes = 1024 * 1024 * 4;
  const size_t packedBytes=bcMipBytes(1024,4);
  unsigned char *packed=malloc(packedBytes);if(!packed)die("BC3 atlas");
  unsigned char *pixels = malloc(bytes);
  uint32_t used = 0;
  if (!pixels)
    die("tree atlas allocation");
  if (!cacheRead(4, 1, 1, 0, 0, packed, packedBytes, packed, 0, &used)) {
    Target t = target(1024, 1024, 1, 0);
    GLuint copyP = program("bake.vert", "atlas-copy.frag");
    GLuint bakeP = program("nature-bake.vert", "nature-bake.frag");
    glBindFramebuffer(GL_FRAMEBUFFER, t.fbo);
    glViewport(0, 0, 1024, 1024);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glUseProgram(copyP);
    tex(copyP, "sourceTex", 0, foliageTex);
    glViewport(0, 0, 512, 512);
    quad();
    glUseProgram(bakeP);
    tex(bakeP, "foliageTex", 0, foliageTex);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    for (int view = 0; view < 2; view++)
      for (int variant = 0; variant < 4; variant++) {
        glViewport(variant * 256, 512 + view * 256, 256, 256);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(-6, 6, 0, 16, -24, 24);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glRotatef(view * 90, 0, 1, 0);
        NatureVertex *v = treeVariants[variant];
        glVertexPointer(3, GL_FLOAT, sizeof(*v), &v->p);
        glColorPointer(4, GL_FLOAT, sizeof(*v), &v->r);
        glTexCoordPointer(2, GL_FLOAT, sizeof(*v), &v->u);
        glDrawArrays(GL_TRIANGLES, 0, treeCounts[variant]);
      }
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glReadPixels(0, 0, 1024, 1024, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &t.fbo);
    glDeleteRenderbuffers(1, &t.depth);
    glDeleteTextures(1, &t.tex);
    glDeleteProgram(copyP);
    glDeleteProgram(bakeP);
    /* Fill transparent edge RGB so bilinear filtering does not create dark
     * fringes. */
    for (int pass = 0; pass < 3; pass++) {
      unsigned char *copy = malloc(bytes);
      if (!copy)
        die("tree atlas edge buffer");
      memcpy(copy, pixels, bytes);
      for (int y = 513; y < 1023; y++)
        for (int x = 1; x < 1023; x++) {
          int i = (y * 1024 + x) * 4;
          if (copy[i + 3] || copy[i] || copy[i + 1] || copy[i + 2])
            continue;
          int neighbors[4] = {i - 4, i + 4, i - 4096, i + 4096};
          for (int n = 0; n < 4; n++) {
            int j = neighbors[n];
            if (copy[j] || copy[j + 1] || copy[j + 2]) {
              memcpy(pixels + i, copy + j, 3);
              break;
            }
          }
        }
      free(copy);
    }
    bcMipEncode(pixels,1024,4,packed);
    cacheWrite(4,1,1,0,0,packed,packedBytes,packed,0);
  }
  glBindTexture(GL_TEXTURE_2D, foliageTex);
  bcMipUpload(packed,1024,4);
  free(packed);
  GLint compressed=0;glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_COMPRESSED,&compressed);
  printf("FOLIAGE BC3 compressed=%d bytes=%zu\n",compressed,packedBytes);
  GLint mipWidth=0;
  glGetTexLevelParameteriv(GL_TEXTURE_2D,4,GL_TEXTURE_WIDTH,&mipWidth);
  if(mipWidth!=64)die("incomplete tree atlas mip chain");
  free(pixels);
  checkGL("cached tree impostor atlas");
}
