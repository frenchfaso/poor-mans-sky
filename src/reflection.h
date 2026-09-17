// SPDX-License-Identifier: MPL-2.0
/* Local tangent-plane reflection, refreshed every frame (optionally every two). */
static Target reflectionMap, reflectionBlur;
static GLuint reflectionBlurP, reflectionLandP;
static int reflectionEnabled=1;
static int reflectionInterval = 1, reflectionUpdates, reflectionMaxGap;
static V3 reflectionEye;
static float reflectionVP[16];
static int reflectionReady, reflectionLastFrame = -100, reflectionDraws;
static V3 mirrorDirection(V3 v, V3 normal) {
  return add(v, mul(normal, -2 * dot(v, normal)));
}
static void updateReflection(void) {
  if(!reflectionEnabled){reflectionReady=0;return;}
  float altitude = sqrtf(dot(cameraEye, cameraEye)) - RADIUS;
  int waterVisible = 0;
  for (int i = 0; i < selectedCount; i++)
    waterVisible |=
        nodes[selected[i]].visible && nodes[selected[i]].minHeight < 0;
  if (!waterVisible || altitude > 800 || altitude < .1f) {
    reflectionReady = 0;
    return;
  }
  if (reflectionReady && frameNo - reflectionLastFrame < reflectionInterval)
    return;
  if (reflectionReady && frameNo - reflectionLastFrame > reflectionMaxGap)
    reflectionMaxGap = frameNo - reflectionLastFrame;
  if (!reflectionMap.tex) {
    reflectionMap = target(128, 128, 1, 0);
    reflectionBlur = target(128, 128, 0, 0);
    if(!reflectionBlurP)reflectionBlurP = program("bake.vert", "reflection-blur.frag");
  }
  V3 savedEye = cameraEye, savedF = viewForward, savedR = viewRight,
     savedU = viewUp;
  float savedNear = clipNear, savedFar = clipFar;
  V3 radial = norm(cameraEye);
  cameraEye = add(cameraEye, mul(radial, -2 * altitude));
  reflectionEye = cameraEye;
  viewForward = mirrorDirection(viewForward, radial);
  viewRight = mirrorDirection(viewRight, radial);
  viewUp = mirrorDirection(viewUp, radial);
  clipNear = .5f;
  clipFar = 6000;
  camera();
  visibilitySetup();
  float projection[16], model[16];
  glGetFloatv(GL_PROJECTION_MATRIX, projection);
  glGetFloatv(GL_MODELVIEW_MATRIX, model);
  for (int c = 0; c < 4; c++)
    for (int r = 0; r < 4; r++) {
      reflectionVP[c * 4 + r] = 0;
      for (int k = 0; k < 4; k++)
        reflectionVP[c * 4 + r] += projection[k * 4 + r] * model[c * 4 + k];
    }
  glBindFramebuffer(GL_FRAMEBUFFER, reflectionMap.fbo);
  glViewport(0, 0, 128, 128);
  glDepthRange(0, 1);
  glClearColor(0, 0, 0, 0);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_CULL_FACE);
  glFrontFace(GL_CW);
  glEnableClientState(GL_VERTEX_ARRAY);
  glEnableClientState(GL_NORMAL_ARRAY);
  glEnableClientState(GL_TEXTURE_COORD_ARRAY);
  GLuint p = reflectionLandP;
  glUseProgram(p);
  u1(p, "fogHeightFactor", 0);
  u3(p, "sun", sun);
  u3(p, "fogColor", fogColor);
  u3(p, "ambientLight", ambientLight);
  u3(p, "ambientUp", norm(savedEye));
  u3(p, "sunLight", sunLight);
  u1(p, "exposure", sceneExposure);
  tex(p, "atlas", 0, atlasTex[0]);
  reflectionDraws = 0;
  for (int i = 0; i < selectedCount; i++) {
    Node *n = &nodes[selected[i]];
    if (n->maxHeight < 0 || !boxInFrustum(n->boundCenter, n->boundHalf))
      continue;
    V3 offset = add(n->center, mul(cameraEye, -1));
    if (sqrtf(dot(offset, offset)) - n->boundRadius > 5000)
      continue;
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, atlasTex[n->slot / 256]);
    u2(p, "page", (n->slot % 16) / 16.0f, ((n->slot % 256) / 16) / 16.0f);
    u3(p, "eye", mul(offset, -1));
    glUniform4f(uniformLocation(p, "reflectionPlane"), radial.x, radial.y,
                radial.z, dot(n->center, radial) - RADIUS);
    glPushMatrix();
    glTranslatef(offset.x, offset.y, offset.z);
    int lod = n->meshLevel < 3 ? n->meshLevel + 1 : 3;
    geometry(n->vbo, lod);
    glPopMatrix();
    reflectionDraws++;
  }
  natureDrawPass(1);
  glFrontFace(GL_CCW);
  /* Blur only the 128-square reflection, not each full-resolution water pixel.
   * Preserve coverage alpha so cleared pixels cannot produce dark halos. */
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glDisable(GL_BLEND);
  glUseProgram(reflectionBlurP);
  glBindFramebuffer(GL_FRAMEBUFFER, reflectionBlur.fbo);
  tex(reflectionBlurP, "sceneTex", 0, reflectionMap.tex);
  u2(reflectionBlurP, "direction", 1.0f / 128, 0);
  u1(reflectionBlurP, "finalPass", 0);
  quad();
  glBindFramebuffer(GL_FRAMEBUFFER, reflectionMap.fbo);
  tex(reflectionBlurP, "sceneTex", 0, reflectionBlur.tex);
  u2(reflectionBlurP, "direction", 0, 1.0f / 128);
  u1(reflectionBlurP, "finalPass", 1);
  quad();
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_CULL_FACE);
  cameraEye = savedEye;
  viewForward = savedF;
  viewRight = savedR;
  viewUp = savedU;
  clipNear = savedNear;
  clipFar = savedFar;
  camera();
  visibilitySetup();
  glBindFramebuffer(GL_FRAMEBUFFER, scene.fbo);
  glViewport(0, 0, rw, rh);
  glDepthRange(worldDepthLo,worldDepthHi);
  glEnableClientState(GL_VERTEX_ARRAY);
  glEnableClientState(GL_NORMAL_ARRAY);
  glEnableClientState(GL_TEXTURE_COORD_ARRAY);
  reflectionReady = 1;
  reflectionLastFrame = frameNo;
  reflectionUpdates++;
}
