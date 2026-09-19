// SPDX-License-Identifier: MPL-2.0
#define main poor_mans_sky_application_main
#include "../src/poor-mans-sky.c"
#undef main
#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x)) {                                                                \
      fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x);                          \
      return 1;                                                                \
    }                                                                          \
  } while (0)
int main(void) {
  CHECK(SDL_Init(SDL_INIT_VIDEO) == 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
  window = SDL_CreateWindow("Poor Man's Sky resource test", 0, 0, 64, 64,
                            SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  CHECK(window);
  context = SDL_GL_CreateContext(window);
  CHECK(context);
  scene = target(1024, 1024, 1, 0);
  for (int k = 0; k < 2; k++)
    glow[k] = target(256, 256, 0, 0);
  width = 1024;
  height = 768;
  for (int p = 0; p < 3; p++)
    for (int i = 0; i < 5; i++) {
      setQualityPreset(p);
      int sizes[5][2]={{640,480},{720,480},{848,480},{800,600},{1024,768}};
      SDL_SetWindowSize(window,sizes[i][0],sizes[i][1]);
      resolution();
      CHECK(scene.w >= rw && scene.h >= rh && scene.w < 2 * rw &&
            scene.h < 2 * rh);
      CHECK(glow[0].w == quality()->bloomSize);
      CHECK(rw == (int)(width*quality()->renderScale) && rh == (int)(height*quality()->renderScale));
      checkGL("resize");
    }
  puts("PASS all 15 resolution/preset combinations and complete GPU targets");
  SDL_GL_DeleteContext(context);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
