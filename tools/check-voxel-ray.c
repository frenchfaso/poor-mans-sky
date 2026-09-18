// SPDX-License-Identifier: MPL-2.0
#define SDL_MAIN_HANDLED
#define main poor_mans_sky_application_main
#include "../src/poor-mans-sky.c"
#undef main
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void) {
  CHECK(!SDL_Init(SDL_INIT_VIDEO));
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);
  window=SDL_CreateWindow("GPU ray check",0,0,64,64,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);CHECK(window);
  context=SDL_GL_CreateContext(window);CHECK(context);
  rw=rh=width=height=64;clipNear=1;clipFar=RAY_RANGE;
  voxelResize();cameraEye=v3(0,RADIUS+10,0);
  rayAnchorUp=v3(0,1,0);rayEast=v3(1,0,0);rayNorth=v3(0,0,1);
  viewForward=norm(v3(0,-.3f,-1));viewRight=v3(1,0,0);viewUp=cross(viewRight,viewForward);
  rayMapReady=1;raySlope=.01f;rayPixelTolerance=0;
  glGenTextures(3,rayMap);unsigned char flat[4];rayPack(.5f,flat);
  for(int k=0;k<3;k++) {
    glBindTexture(GL_TEXTURE_2D,rayMap[k]);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,flat);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
  }
  unsigned char pixels[64*64*4];
  for(int passes=16;passes<=64;passes*=4) {
    rayPasses=passes;rayDrawTargets();
    glBindTexture(GL_TEXTURE_2D,voxelTextures[2]);glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
    int expected=0,hits=0;float maxGap=0;
    for(int x=0;x<64;x++)for(int y=0;y<64;y++) {
      V3 ray=add(viewForward,add(mul(viewRight,(2*(x+.5f)/64-1)*tanf(PI/6)),mul(viewUp,(2*(y+.5f)/64-1)*tanf(PI/6))));
      unsigned char *p=pixels+(x*64+y)*4;
      int eligible=ray.y<0 && -10/ray.y<RAY_RANGE*.99f;
      float distance=clipNear;int converged=0;
      for(int j=0;j<passes;j++) {
        float gap=10+ray.y*distance;
        if(gap<=.125f && distance<RAY_RANGE*.99998f)converged=1;
        if(!converged)distance=fminf(RAY_RANGE*.99999f,distance+.95f*fmaxf(gap,0)/(fabsf(ray.y)+raySlope*sqrtf(ray.x*ray.x+ray.z*ray.z)+.000002f));
      }
      eligible=eligible&&converged;expected+=eligible;
      CHECK((p[3]!=0)==eligible);
      if(eligible) {
        hits++;unsigned packed=(unsigned)p[0]<<16|(unsigned)p[1]<<8|p[2];CHECK(packed);
        float z=16777215.f/packed,gap=10+ray.y*z;maxGap=fmaxf(maxGap,fabsf(gap));
        CHECK(gap>=-.05f && gap<.15f);
      }
    }
    CHECK(glGetError()==GL_NO_ERROR);CHECK(voxelRasterBuilds==0);
    printf("GPU ray plane passes=%d hits=%d expected=%d max_gap=%.6f cpu_casts=%llu OK\n",passes,hits,expected,maxGap,voxelRasterBuilds);
  }
  rayClose();SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();return 0;
}
