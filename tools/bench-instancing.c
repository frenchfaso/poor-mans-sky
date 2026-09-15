// SPDX-License-Identifier: MPL-2.0
#define SDL_MAIN_HANDLED
#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct {
  float p[3], n[3], uv[2];
} Vertex;
static GLuint shader(GLenum type, const char *src) {
  GLuint s = glCreateShader(type);
  glShaderSource(s, 1, &src, 0);
  glCompileShader(s);
  GLint ok;
  glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    char msg[4096];
    glGetShaderInfoLog(s, 4096, 0, msg);
    puts(msg);
    exit(2);
  }
  return s;
}
static GLuint program(int instanced) {
  const char *v =
      instanced ? "#version 120\nattribute vec4 instanceData;varying vec3 "
                  "normal;varying vec2 uv;void "
                  "main(){normal=gl_Normal;uv=gl_MultiTexCoord0.xy;gl_Position="
                  "gl_ModelViewProjectionMatrix*vec4(gl_Vertex.xyz*"
                  "instanceData.w+instanceData.xyz,1);}"
                : "#version 120\nvarying vec3 normal;varying vec2 uv;void "
                  "main(){normal=gl_Normal;uv=gl_MultiTexCoord0.xy;gl_Position="
                  "gl_ModelViewProjectionMatrix*gl_Vertex;}";
  const char *f =
      "#version 120\nvarying vec3 normal;varying vec2 uv;uniform sampler2D "
      "tex;void main(){vec4 "
      "leaf=texture2D(tex,uv);if(leaf.a<.4)discard;gl_FragColor=vec4(leaf.rgb*("
      ".2+.8*max(dot(normal,vec3(.3,.9,.1)),0.0)),1);}";
  GLuint p = glCreateProgram();
  glAttachShader(p, shader(GL_VERTEX_SHADER, v));
  glAttachShader(p, shader(GL_FRAGMENT_SHADER, f));
  glBindAttribLocation(p, 6, "instanceData");
  glLinkProgram(p);
  GLint ok;
  glGetProgramiv(p, GL_LINK_STATUS, &ok);
  if (!ok)
    exit(3);
  return p;
}
int main(void) {
  SDL_Init(SDL_INIT_VIDEO);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
  SDL_Window *w = SDL_CreateWindow("Poor Man's Sky instancing benchmark", 0, 0, 512,
                                   384, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  SDL_GLContext ctx = SDL_GL_CreateContext(w);
  if (!ctx) {
    puts(SDL_GetError());
    return 2;
  }
  printf("GPU %s\n", glGetString(GL_RENDERER));
  if (!strstr((char *)glGetString(GL_EXTENSIONS), "GL_ARB_instanced_arrays"))
    return 3;
  PFNGLVERTEXATTRIBDIVISORARBPROC divisor =
      (PFNGLVERTEXATTRIBDIVISORARBPROC)SDL_GL_GetProcAddress(
          "glVertexAttribDivisorARB");
  PFNGLDRAWARRAYSINSTANCEDARBPROC draw =
      (PFNGLDRAWARRAYSINSTANCEDARBPROC)SDL_GL_GetProcAddress(
          "glDrawArraysInstancedARB");
  if (!divisor || !draw)
    return 4;
  GLuint fbo, color, depth;
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glGenTextures(1, &color);
  glBindTexture(GL_TEXTURE_2D, color);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 512, 512, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, 0);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         color, 0);
  glGenRenderbuffers(1, &depth);
  glBindRenderbuffer(GL_RENDERBUFFER, depth);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 512, 512);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                            GL_RENDERBUFFER, depth);
  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    return 5;
  GLuint prog[2] = {program(0), program(1)}, buffers[3];
  glGenBuffers(3, buffers);
  unsigned char pixels[64 * 64 * 4];
  for (int y = 0; y < 64; y++)
    for (int x = 0; x < 64; x++) {
      int i = (y * 64 + x) * 4;
      pixels[i] = 80;
      pixels[i + 1] = 150;
      pixels[i + 2] = 40;
      pixels[i + 3] = (abs(x - 32) < (64 - y) / 3 || (x + y) % 9 < 3) ? 255 : 0;
    }
  GLuint tex;
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               pixels);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  Vertex model[12];
  int ids[6] = {0, 1, 2, 0, 2, 3};
  for (int side = 0; side < 2; side++)
    for (int j = 0; j < 6; j++) {
      int k = ids[j];
      Vertex *v = &model[side * 6 + j];
      float x = (k == 1 || k == 2) ? .4f : -.4f;
      v->p[0] = side ? 0 : x;
      v->p[1] = k >= 2 ? 1 : 0;
      v->p[2] = side ? x : 0;
      v->n[0] = 0;
      v->n[1] = 1;
      v->n[2] = 0;
      v->uv[0] = (k == 1 || k == 2) ? 1 : 0;
      v->uv[1] = k >= 2 ? 1 : 0;
    }
  glBindBuffer(GL_ARRAY_BUFFER, buffers[0]);
  glBufferData(GL_ARRAY_BUFFER, sizeof(model), model, GL_STATIC_DRAW);
  glEnable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glViewport(0, 0, 512, 384);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(-35, 35, -5, 45, -100, 100);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glRotatef(35, 1, 0, 0);
  glEnableClientState(GL_VERTEX_ARRAY);
  glEnableClientState(GL_NORMAL_ARRAY);
  glEnableClientState(GL_TEXTURE_COORD_ARRAY);
  for (int count = 512; count <= 2048; count *= 4) {
    float *instances = malloc(count * 4 * sizeof(float));
    Vertex *batch = malloc(count * sizeof(model));
    for (int i = 0; i < count; i++) {
      float x = (i % 48 - 24) * 1.35f, z = (i / 48 - 20) * 1.1f,
            scale = .6f + (i % 9) * .08f;
      instances[i * 4] = x;
      instances[i * 4 + 1] = 0;
      instances[i * 4 + 2] = z;
      instances[i * 4 + 3] = scale;
      for (int j = 0; j < 12; j++) {
        Vertex v = model[j];
        v.p[0] = v.p[0] * scale + x;
        v.p[1] *= scale;
        v.p[2] = v.p[2] * scale + z;
        batch[i * 12 + j] = v;
      }
    }
    glBindBuffer(GL_ARRAY_BUFFER, buffers[1]);
    glBufferData(GL_ARRAY_BUFFER, count * sizeof(model), batch, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, buffers[2]);
    glBufferData(GL_ARRAY_BUFFER, count * 4 * sizeof(float), instances,
                 GL_STATIC_DRAW);
    for (int mode = 0; mode < 2; mode++) {
      glUseProgram(prog[mode]);
      glBindBuffer(GL_ARRAY_BUFFER, buffers[mode ? 0 : 1]);
      glVertexPointer(3, GL_FLOAT, sizeof(Vertex), (void *)0);
      glNormalPointer(GL_FLOAT, sizeof(Vertex), (void *)12);
      glTexCoordPointer(2, GL_FLOAT, sizeof(Vertex), (void *)24);
      if (mode) {
        glBindBuffer(GL_ARRAY_BUFFER, buffers[2]);
        glEnableVertexAttribArray(6);
        glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, 16, 0);
        divisor(6, 1);
      } else {
        glDisableVertexAttribArray(6);
        divisor(6, 0);
      }
      double sum = 0;
      for (int frame = 0; frame < 50; frame++) {
        Uint64 start = SDL_GetPerformanceCounter();
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        if (mode)
          draw(GL_TRIANGLES, 0, 12, count);
        else
          glDrawArrays(GL_TRIANGLES, 0, 12 * count);
        glFinish();
        if (frame >= 10)
          sum += (SDL_GetPerformanceCounter() - start) * 1000.0 /
                 SDL_GetPerformanceFrequency();
      }
      unsigned char *readback = malloc(512 * 384 * 4);
      glReadPixels(0, 0, 512, 384, GL_RGBA, GL_UNSIGNED_BYTE, readback);
      int covered = 0;
      for (int p = 0; p < 512 * 384; p++)
        covered += readback[p * 4 + 1] > 0;
      free(readback);
      printf("PIXELS covered=%d\n", covered);
      printf("BENCH instances=%d mode=%s ms=%.3f vertex_bytes=%zu error=%x\n",
             count, mode ? "instanced" : "batch", sum / 40,
             mode ? sizeof(model) + count * 16 : count * sizeof(model),
             glGetError());
    }
    free(batch);
    free(instances);
  }
  SDL_GL_DeleteContext(ctx);
  SDL_DestroyWindow(w);
  SDL_Quit();
  return 0;
}
