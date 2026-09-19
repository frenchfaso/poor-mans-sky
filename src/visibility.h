// SPDX-License-Identifier: MPL-2.0
/* Bounds contain every submitted terrain/skirt vertex and the displaced sea.
 * Drawing culling is separate from the wider streaming cone and near bubble. */
static V3 frustumPlanes[6];
static float frustumOffsets[6];
static int visibleCount, frustumRejected, horizonRejected;
static void profileMark(int stage) {
  if (!profileFrames || frameNo < 30)
    return;
  glFinish();
  Uint64 now = SDL_GetPerformanceCounter();
  profileTimes[stage] +=
      (now - profileStamp) * 1000.0 / SDL_GetPerformanceFrequency();
  profileStamp = now;
}
static void measureBounds(Node *n) {
  n->minHeight=1e9f;n->maxHeight=-1e9f;
  V3 lo = v3(1e30f, 1e30f, 1e30f), hi = mul(lo, -1);
  for (int i = 0; i < NV; i++) {
    n->minHeight=fminf(n->minHeight,n->vertices[i].h);
    n->maxHeight=fmaxf(n->maxHeight,n->vertices[i].h);
    V3 points[2];
    points[0] = n->vertices[i].p;
    V3 radial = norm(add(n->center, points[0]));
    points[1] = add(points[0], mul(radial, -n->vertices[i].h));
    for (int j = 0; j < 2; j++) {
      V3 p = points[j];
      lo = v3(fminf(lo.x, p.x), fminf(lo.y, p.y), fminf(lo.z, p.z));
      hi = v3(fmaxf(hi.x, p.x), fmaxf(hi.y, p.y), fmaxf(hi.z, p.z));
    }
  }
  n->boundCenter = add(n->center, mul(add(lo, hi), .5f));
  n->boundHalf = add(mul(add(hi, mul(lo, -1)), .5f), v3(1, 1, 1));
  n->boundRadius = sqrtf(dot(n->boundHalf, n->boundHalf));
}
static void visibilitySetup(void) {
  float ty = tanf(PI / 6), tx = ty * width / height;
  frustumPlanes[0] = norm(add(mul(viewForward, tx), viewRight));
  frustumPlanes[1] = norm(add(mul(viewForward, tx), mul(viewRight, -1)));
  frustumPlanes[2] = norm(add(mul(viewForward, ty), viewUp));
  frustumPlanes[3] = norm(add(mul(viewForward, ty), mul(viewUp, -1)));
  frustumPlanes[4] = viewForward;
  frustumPlanes[5] = mul(viewForward, -1);
  for (int i = 0; i < 4; i++)
    frustumOffsets[i] = 0;
  frustumOffsets[4] = -clipNear;
  frustumOffsets[5] = clipFar;
}
static int boxInFrustum(V3 center, V3 half) {
  V3 d = add(center, mul(cameraEye, -1));
  for (int i = 0; i < 6; i++) {
    V3 p = frustumPlanes[i];
    float support =
        fabsf(p.x) * half.x + fabsf(p.y) * half.y + fabsf(p.z) * half.z;
    if (dot(p, d) + frustumOffsets[i] + support < 0)
      return 0;
  }
  return 1;
}
static int behindPlanet(V3 center, float radius) {
  /* Below the analytic minimum elevation (-7165m), with a generous
   * allowance for chord sag at the coarsest index LOD on root patches. Never use sea level
   * as an occluder: shoreline discard and underwater cameras need safety. */
  const float core = RADIUS * .8f;
  float camera2 = dot(cameraEye, cameraEye);
  if (camera2 <= core * core)
    return 0;
  V3 delta = add(center, mul(cameraEye, -1));
  float distance = sqrtf(dot(delta, delta));
  float tangent = sqrtf(camera2 - core * core);
  if (distance - radius <= tangent || distance <= radius)
    return 0;
  float cameraDistance = sqrtf(camera2), s = radius / distance;
  float coneSin = core / cameraDistance, coneCos = tangent / cameraDistance;
  if (s >= coneSin)
    return 0;
  float threshold = coneCos * sqrtf(fmaxf(0, 1 - s * s)) + coneSin * s;
  float alignment = -dot(delta, cameraEye) / (distance * cameraDistance);
  return alignment > threshold + 1e-5f;
}
static int nodeVisible(const Node *n) {
  if (!cullingMode)
    return legacyVisible(n);
  if (!boxInFrustum(n->boundCenter, n->boundHalf)) {
    frustumRejected++;
    return 0;
  }
  if (behindPlanet(n->boundCenter, n->boundRadius)) {
    horizonRejected++;
    return 0;
  }
  return 1;
}
/* Asynchronous zero-sample box queries against opaque terrain only.
 * A camera, projection or cover change invalidates EVERY hidden result.
 * Moving views therefore fail open, never borrowing visibility from the past.
 */
static unsigned occlusionEpoch = 1;
static uint32_t occlusionSignature;
static int occlusionStable, occlusionTested, occlusionHidden, occlusionIssued;
static int occlusionActive,occlusionCooldown,occlusionSamples,occlusionSavings;
static int occlusionPolicy(float altitude,int candidates,int frame) {
 return occlusionEnabled && !wire && altitude<8000 && candidates>=24 && frame>=occlusionCooldown;
}
static void occlusionPrepare(void) {
  uint32_t h = 2166136261u;
  h = cacheHash(&cameraEye, sizeof(cameraEye), h);
  h = cacheHash(&viewForward, sizeof(viewForward), h);
  h = cacheHash(&viewRight, sizeof(viewRight), h);
  h = cacheHash(&viewUp, sizeof(viewUp), h);
  h = cacheHash(&clipNear, sizeof(clipNear), h);
  h = cacheHash(&clipFar, sizeof(clipFar), h);
  h = cacheHash(&rw, sizeof(rw), h);
  h = cacheHash(&rh, sizeof(rh), h);
  h = cacheHash(&wire, sizeof(wire), h);
  h = cacheHash(&lodRevision, sizeof(lodRevision), h);
  for (int i = 0; i < selectedCount; i++) {
    Node *n = &nodes[selected[i]];
    h = cacheHash(&selected[i], sizeof(selected[i]), h);
    h = cacheHash(&n->meshLevel, sizeof(n->meshLevel), h);
    h = cacheHash(&n->visible, sizeof(n->visible), h);
  }
  if (h != occlusionSignature) {
    occlusionSignature = h;
    occlusionEpoch++;
    occlusionStable = 0;
  } else
    occlusionStable++;
  occlusionTested = occlusionHidden = occlusionIssued = 0;
  occlusionActive=occlusionPolicy(sqrtf(dot(cameraEye,cameraEye))-RADIUS,visibleCount,frameNo);
  if (!occlusionActive)return;
  for (int i = 0; i < selectedCount; i++) {
    Node *n = &nodes[selected[i]];
    if (n->queryPending) {
      GLint ready = 0;
      glGetQueryObjectiv(n->query, GL_QUERY_RESULT_AVAILABLE, &ready);
      if (ready) {
        GLuint samples = 1;
        glGetQueryObjectuiv(n->query, GL_QUERY_RESULT, &samples);
        n->queryPending = 0;
        if (n->queryEpoch == occlusionEpoch) {
          occlusionSamples++;occlusionSavings+=(samples==0);
          n->testedEpoch = occlusionEpoch;
          n->occluded = (samples == 0);
        }
      }
    }
    if (n->visible && n->testedEpoch == occlusionEpoch) {
      occlusionTested++;
      if (n->occluded) {
        n->visible = 0;
        occlusionHidden++;
      }
    }
  }
  if(occlusionSamples>=96) {
    if(occlusionSavings*20<occlusionSamples)occlusionCooldown=frameNo+240;
    occlusionSamples=occlusionSavings=0;
  }
}
static void occlusionIssue(void) {
  if (!occlusionActive || occlusionStable < 3)
    return;
  glUseProgram(occlusionP);
  glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
  glDepthMask(GL_FALSE);
  glDisable(GL_CULL_FACE);
  static const unsigned char faces[24] = {0, 1, 3, 2, 4, 6, 7, 5, 0, 4, 5, 1,
                                          2, 3, 7, 6, 0, 2, 6, 4, 1, 5, 7, 3};
  for (int i = selectedCount - 1; i >= 0 && occlusionIssued < quality()->queryBudget; i--) {
    Node *n = &nodes[selected[i]];
    if (!n->visible || n->queryPending || n->testedEpoch == occlusionEpoch)
      continue;
    V3 d = add(n->boundCenter, mul(cameraEye, -1));
    if(n->boundRadius*height/fmaxf(1,dot(d,viewForward))<16)continue;
    /* Boxes crossing the near plane must not be queried: clipping could
     * remove their front faces and falsely report an invisible volume. */
    float support = fabsf(viewForward.x) * n->boundHalf.x +
                    fabsf(viewForward.y) * n->boundHalf.y +
                    fabsf(viewForward.z) * n->boundHalf.z;
    if (dot(d, viewForward) - support <= clipNear) {
      n->testedEpoch = occlusionEpoch;
      n->occluded = 0;
      continue;
    }
    if (!n->query)
      glGenQueries(1, &n->query);
    glBeginQuery(GL_SAMPLES_PASSED, n->query);
    glBegin(GL_QUADS);
    for (int k = 0; k < 24; k++) {
      int corner = faces[k];
      glVertex3f(d.x + (corner & 1 ? n->boundHalf.x : -n->boundHalf.x),
                 d.y + (corner & 2 ? n->boundHalf.y : -n->boundHalf.y),
                 d.z + (corner & 4 ? n->boundHalf.z : -n->boundHalf.z));
    }
    glEnd();
    glEndQuery(GL_SAMPLES_PASSED);
    n->queryEpoch = occlusionEpoch;
    n->queryPending = 1;
    occlusionIssued++;
  }
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  glDepthMask(GL_TRUE);
  glEnable(GL_CULL_FACE);
}
