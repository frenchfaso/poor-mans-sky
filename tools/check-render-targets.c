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
  resourceInit();
  int drawableWidth,drawableHeight;
  SDL_GL_GetDrawableSize(window,&drawableWidth,&drawableHeight);
  for(int p=0;p<3;p++)for(int i=0;i<RENDER_SIZE_COUNT;i++) {
    renderSizeIndex=i;resolution();
    GLuint texture=scene.tex;setQualityPreset(p);
    CHECK(scene.tex==texture); /* Presets must not reallocate the scene target. */
    CHECK(scene.w>=rw && scene.h>=rh && scene.w<2*rw && scene.h<2*rh);
    CHECK(glow[0].w==quality()->bloomSize);
    CHECK(rw==renderSizes[i][0] && rh==renderSizes[i][1] && rw*3==rh*4);
    CHECK(width==drawableWidth && height==drawableHeight);
    glBindFramebuffer(GL_FRAMEBUFFER,scene.fbo);
    CHECK(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);
    CHECK(glGetError()==GL_NO_ERROR);
  }
  renderSizeIndex=0;resolution();changeRenderSize(-1);CHECK(renderSizeIndex==0);
  renderSizeIndex=RENDER_SIZE_COUNT-1;resolution();changeRenderSize(1);
  CHECK(renderSizeIndex==RENDER_SIZE_COUNT-1);
  puts("PASS 18 internal resolution/preset combinations, complete GPU targets, fixed drawable and bounded controls");
  SDL_GL_DeleteContext(context);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
