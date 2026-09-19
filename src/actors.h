// SPDX-License-Identifier: MPL-2.0
typedef struct {
  V3 p, n;
  float u, v;
} ActorVertex;
/* Original lightweight explorer and courier. Primitive VBOs, rigid GPU
 * transforms. */
static GLuint actorP, shapeVBO[6], shapeEBO[6], actorTex;
static int shapeCount[6], landing, actorMaterial;
static V3 shipPos, shipHeading;
static GLint paintLoc, shineLoc, tileLoc, texturedLoc;
/* Share equal position/normal/UV tuples, keeping material seams intact.
 * All model parts use positive scales and closed outward-facing primitives. */
static int indexActorMesh(ActorVertex *v, int count, unsigned short *indices,
                          int *indexTotal) {
  ActorVertex unique[24 * 16 * 6];
  int vertices = 0, elements = 0;
  for (int i = 0; i < count; i += 3) {
    V3 area = cross(add(v[i + 1].p, mul(v[i].p, -1)),
                    add(v[i + 2].p, mul(v[i].p, -1)));
    if (dot(area, area) < 1e-12f)
      continue;
    V3 normal = add(v[i].n, add(v[i + 1].n, v[i + 2].n));
    if (dot(area, normal) < 0) {
      ActorVertex temp = v[i + 1];
      v[i + 1] = v[i + 2];
      v[i + 2] = temp;
    }
    for (int j = 0; j < 3; j++) {
      int k = 0;
      for (; k < vertices; k++)
        if (!memcmp(&unique[k], &v[i + j], sizeof(*v)))
          break;
      if (k == vertices)
        unique[vertices++] = v[i + j];
      indices[elements++] = (unsigned short)k;
    }
  }
  memcpy(v, unique, vertices * sizeof(*v));
  *indexTotal = elements;
  return vertices;
}
static void actorInit(void) {
  actorP = program("actor.vert", "actor.frag");
  paintLoc = glGetUniformLocation(actorP, "paint");
  shineLoc = glGetUniformLocation(actorP, "shine");
  tileLoc = glGetUniformLocation(actorP, "tile");
  texturedLoc = glGetUniformLocation(actorP, "textured");
  SDL_Surface *bmp = SDL_LoadBMP(resourcePath("assets/actor-materials.bmp"));
  if (!bmp)
    die(SDL_GetError());
  SDL_Surface *rgba =
      SDL_ConvertSurfaceFormat(bmp, SDL_PIXELFORMAT_ABGR8888, 0);
  SDL_FreeSurface(bmp);
  if (!rgba)
    die(SDL_GetError());
  glGenTextures(1, &actorTex);
  glBindTexture(GL_TEXTURE_2D, actorTex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGB_S3TC_DXT1_EXT, rgba->w,
               rgba->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba->pixels);
  glGenerateMipmap(GL_TEXTURE_2D);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                  GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  SDL_FreeSurface(rgba);
  ActorVertex v[24 * 16 * 6];
  for (int shape = 0; shape < 6; shape++) {
    int n = 0;
    if (shape == 0 || shape == 3 || shape == 5) {
      int slices = shape == 5   ? 10
                   : shape == 3 ? 12
                                : 24,
          stacks = shape == 5   ? 6
                   : shape == 3 ? 8
                                : 16;
      for (int y = 0; y < stacks; y++)
        for (int x = 0; x < slices; x++) {
          int dx[6] = {0, 1, 0, 1, 1, 0}, dy[6] = {0, 0, 1, 0, 1, 1};
          for (int j = 0; j < 6; j++) {
            float a = (x + dx[j]) * 2 * PI / slices,
                  b = -PI / 2 + (y + dy[j]) * PI / stacks;
            V3 p = v3(cosf(b) * cosf(a), sinf(b), cosf(b) * sinf(a));
            v[n++] = (ActorVertex){p, p, (x + dx[j]) / (float)slices,
                                   (y + dy[j]) / (float)stacks};
          }
        }
    } else if (shape == 2 || shape == 4) {
      int slices = shape == 4 ? 12 : 24, stacks = shape == 4 ? 4 : 8;
      for (int x = 0; x < slices; x++)
        for (int y = 0; y < stacks; y++) {
          int dx[6] = {0, 1, 0, 1, 1, 0}, dy[6] = {0, 0, 1, 0, 1, 1};
          for (int j = 0; j < 6; j++) {
            float a = (x + dx[j]) * 2 * PI / slices,
                  b = (y + dy[j]) * 2 * PI / stacks;
            V3 normal = v3(cosf(a) * cosf(b), sinf(a) * cosf(b), sinf(b));
            V3 pos = v3(cosf(a) * (1 + .14f * cosf(b)),
                        sinf(a) * (1 + .14f * cosf(b)), .14f * sinf(b));
            v[n++] = (ActorVertex){pos, normal, (x + dx[j]) / (float)slices,
                                   (y + dy[j]) / (float)stacks};
          }
        }
    } else {
      V3 corners[8] = {{-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1},
                       {-1, -1, 1},  {1, -1, 1},  {1, 1, 1},  {-1, 1, 1}};
      int faces[6][4] = {{0, 3, 2, 1}, {4, 5, 6, 7}, {0, 4, 7, 3},
                         {1, 2, 6, 5}, {0, 1, 5, 4}, {3, 7, 6, 2}};
      for (int f = 0; f < 6; f++) {
        int ix[6] = {0, 1, 2, 0, 2, 3};
        V3 normal = norm(
            cross(add(corners[faces[f][1]], mul(corners[faces[f][0]], -1)),
                  add(corners[faces[f][2]], mul(corners[faces[f][0]], -1))));
        for (int j = 0; j < 6; j++)
          v[n++] = (ActorVertex){corners[faces[f][ix[j]]], normal,
                                 (ix[j] == 1 || ix[j] == 2) ? 1 : 0,
                                 ix[j] >= 2 ? 1 : 0};
      }
    }
    unsigned short indices[24 * 16 * 6];
    n = indexActorMesh(v, n, indices, &shapeCount[shape]);
    glGenBuffers(1, &shapeEBO[shape]);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, shapeEBO[shape]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, shapeCount[shape] * sizeof(*indices),
                 indices, GL_STATIC_DRAW);
    glGenBuffers(1, &shapeVBO[shape]);
    glBindBuffer(GL_ARRAY_BUFFER, shapeVBO[shape]);
    glBufferData(GL_ARRAY_BUFFER, n * sizeof(ActorVertex), v, GL_STATIC_DRAW);
  }
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}
static void part(int shape, V3 p, V3 size, V3 color, float shine, float tilt) {
  if (shape == 0 &&
      (performanceMode || fmaxf(size.x, fmaxf(size.y, size.z)) < .5f))
    shape = 3;
  if (performanceMode && shape == 2)
    shape = 4;
  if (performanceMode && shape == 3 &&
      fmaxf(size.x, fmaxf(size.y, size.z)) < .2f)
    shape = 5;
  glPushMatrix();
  glTranslatef(p.x, p.y, p.z);
  glRotatef(tilt, 1, 0, 0);
  glScalef(size.x, size.y, size.z);
  glUniform3f(paintLoc, color.x, color.y, color.z);
  glUniform1f(shineLoc, shine);
  int material = shine > .4f && shine < .8f ? 2 : actorMaterial;
  glUniform2f(tileLoc, (material % 2) * .5f, (material / 2) * .5f);
  glUniform1f(texturedLoc, shine >= .85f ? 0 : 1);
  glBindBuffer(GL_ARRAY_BUFFER, shapeVBO[shape]);
  glVertexPointer(3, GL_FLOAT, sizeof(ActorVertex),
                  (void *)offsetof(ActorVertex, p));
  glNormalPointer(GL_FLOAT, sizeof(ActorVertex),
                  (void *)offsetof(ActorVertex, n));
  glTexCoordPointer(2, GL_FLOAT, sizeof(ActorVertex),
                    (void *)offsetof(ActorVertex, u));
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, shapeEBO[shape]);
  glDrawElements(GL_TRIANGLES, shapeCount[shape], GL_UNSIGNED_SHORT, (void *)0);
  triangles += shapeCount[shape] / 3;
  glPopMatrix();
}
static void actorFrame(V3 pos, V3 forward, V3 up, float bank) {
  forward = norm(add(forward, mul(up, -dot(forward, up))));
  V3 right = norm(cross(forward, up));
  float m[16] = {right.x,    right.y,    right.z,    0, up.x,  up.y,  up.z,  0,
                 -forward.x, -forward.y, -forward.z, 0, pos.x, pos.y, pos.z, 1};
  m[12] -= cameraEye.x;
  m[13] -= cameraEye.y;
  m[14] -= cameraEye.z;
  glMultMatrixf(m);
  glRotatef(bank * 180 / PI, 0, 0, -1);
}
static void drawActors(void) {
  V3 f = viewForward, r = viewRight, u = viewUp;
  glUseProgram(actorP);
  u3(actorP, "ambientLight", ambientLight);
  V3 radialAmbient=bodyUp(eye);
  u3(actorP,"ambientUp",v3(dot(radialAmbient,r),dot(radialAmbient,u),-dot(radialAmbient,f)));
  V3 lightPosition=flying?add(eye,mul(bodyUp(eye),-1.3f)):shipPos;
  u3(actorP, "sunLight", directSunlightAt(lightPosition));
  u1(actorP, "exposure", sceneExposure);
  u3(actorP, "fogColor", fogColor);
  tex(actorP, "materialTex", 0, actorTex);
  actorMaterial = 0;
  u3(actorP, "lightDir", v3(dot(sun, r), dot(sun, u), -dot(sun, f)));
  glEnableClientState(GL_VERTEX_ARRAY);
  glEnableClientState(GL_NORMAL_ARRAY);
  glEnableClientState(GL_TEXTURE_COORD_ARRAY);
  glEnable(GL_CULL_FACE);
  V3 ivory = v3(.78, .82, .77), orange = v3(.85, .23, .06),
     dark = v3(.055, .075, .09), metal = v3(.25, .30, .33),
     glass = v3(.035, .17, .22), blue = v3(.15, .75, 1);
  glPushMatrix();
  if (flying) {
    V3 radial = bodyUp(eye);
    V3 right = norm(cross(flightForward, flightUp)),
       up = cross(right, flightForward),
       pos = add(add(eye, mul(radial, -1.3f)), mul(cameraEye, -1));
    float m[16] = {right.x,
                   right.y,
                   right.z,
                   0,
                   up.x,
                   up.y,
                   up.z,
                   0,
                   -flightForward.x,
                   -flightForward.y,
                   -flightForward.z,
                   0,
                   pos.x,
                   pos.y,
                   pos.z,
                   1};
    glMultMatrixf(m);
  } else
    actorFrame(shipPos, shipHeading, bodyUp(shipPos), 0);
  /* Compact ivory courier: long nose, glazed canopy, swept outriggers and twin
   * engines. */
  part(0, v3(0, .35, -.3), v3(1.1, .65, 3.6), ivory, .3, 0);
  part(0, v3(0, .87, -1), v3(.76, .58, 1.45), glass, .9, 0);
  part(1, v3(0, .24, -3.15), v3(.5, .13, .9), orange, .25, 0);
  part(1, v3(0, .40, 1.7), v3(.6, .22, 1.0), orange, .3, 0);
  for (int side = -1; side <= 1; side += 2) {
    glPushMatrix();
    glTranslatef(side * 1.6f, 0, .8);
    glRotatef(side * 22, 0, 1, 0);
    part(1, v3(0, 0, 0), v3(1.45, .12, .70), ivory, .3, 0);
    glPopMatrix();
    part(0, v3(side * 2.7f, .1, 1.15), v3(.47, .47, 1.7), metal, .6, 0);
    part(1, v3(side * 2.7f, .4, 1.7), v3(.22, .52, .68), orange, .25, -18);
    part(0, v3(side * 2.7f, .1, 2.78), v3(.34, .34, .08), blue, 2, 0);
    if (flying)
      part(0, v3(side * 2.7f, .1, 3.25),
           v3(.21, .21, .35f + sqrtf(dot(velocity, velocity)) * .003f), blue, 2,
           0);
    if (!flying) {
      part(1, v3(side * .85f, -.67, 1.3), v3(.10, .48, .10), metal, .3, 0);
      part(1, v3(side * .85f, -1.08, 1.3), v3(.35, .09, .45), dark, .2, 0);
    }
    part(1, v3(side * 1.06f, .35, .2), v3(.025, .18, .75), dark, .2, 0);
  }
  /* Actual nozzle rims, heat vanes, canopy frames, avionics and service
   * hardware. */
  for (int side = -1; side <= 1; side += 2) {
    for (int ring = 0; ring < 4; ring++)
      part(2, v3(side * 2.7f, .1, 2.25f + ring * .16f), v3(.44, .44, .7), metal,
           .6, 0);
    for (int fin = 0; fin < 8; fin++) {
      float a = fin * 2 * PI / 8;
      part(1, v3(side * 2.7f + cosf(a) * .43f, .1f + sinf(a) * .43f, 1.75f),
           v3(.04, .04, .7), dark, .6, 0);
    }
    part(2, v3(side * 2.7f, .1, -.42), v3(.44, .44, .7), ivory, .3, 0);
    part(0, v3(side * 2.7f, .1, -.49), v3(.34, .34, .04), dark, .4, 0);
    part(1, v3(side * .69f, 1.07, -1), v3(.035, .055, 1.0), ivory, .3, 0);
    part(1, v3(side * .57f, 1.14, -1.87), v3(.035, .075, .38), metal, .6, 24);
    part(1, v3(side * 1.35f, .16, 1.6), v3(.24, .15, .52), metal, .6, 0);
    part(1, v3(side * 2.9f, .18, .8), v3(.20, .045, .11),
         side < 0 ? v3(1, .08, .02) : v3(.1, 1, .28), 2, 0);
    part(1, v3(side * .7f, .50, 2.35), v3(.20, .16, .4), metal, .6, 0);
    for (int vent = 0; vent < 5; vent++)
      part(1, v3(side * .64f, .82, 1.0f + vent * .18f), v3(.19, .035, .025),
           dark, .4, 0);
  }
  part(1, v3(0, 1.42, -1), v3(.025, .035, 1.0), ivory, .3, 0);
  part(1, v3(0, .91, 2.05), v3(.035, .48, .035), metal, .6, -12);
  part(0, v3(0, 1.42, 2.15), v3(.09, .08, .09), blue, 2, 0);
  part(1, v3(0, -.2, -3.05), v3(.35, .18, .35), metal, .6, 0);
  glPopMatrix();

  glDisableClientState(GL_VERTEX_ARRAY);
  glDisableClientState(GL_NORMAL_ARRAY);
  glDisableClientState(GL_TEXTURE_COORD_ARRAY);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
  glEnable(GL_CULL_FACE);
}
static void parkShip(void) {
  shipPos = bodyFloor(shipPos,1.2f);
}
static void exitShip(void) {
  V3 d = bodyUp(eye);
  shipPos = eye;
  shipHeading = heading;
  parkShip();
  V3 right = norm(cross(heading, d));
  eye = bodyFloor(add(shipPos,mul(right,4.8f)),2.5f);
  flying = landing = 0;
  roll = lookX = lookY = 0;
  velocity = v3(0, 0, 0);
  pitch = -.12f;
  printf("SHIP landed / on foot\n");
}
static void interactShip(void) {
  if (flying) {
    landing = !landing;
    printf("SHIP landing %s\n", landing ? "requested" : "cancelled");
    return;
  }
  V3 delta = add(eye, mul(shipPos, -1));
  if (dot(delta, delta) > 100) {
    printf("SHIP approach within 10 metres\n");
    return;
  }
  eye = add(shipPos, mul(bodyUp(shipPos), 1.3f));
  heading = shipHeading;
  flying = 1;
  landing = 0;
  pitch = .12f;
  roll = lookX = lookY = 0;
  velocity = v3(0, 0, 0);
  flightForward =
      norm(add(mul(heading, cosf(pitch)), mul(bodyUp(eye), sinf(pitch))));
  flightUp = bodyUp(eye);
  flightRates = v3(0, 0, 0);
  throttle = 0;
  printf("SHIP boarded\n");
}
