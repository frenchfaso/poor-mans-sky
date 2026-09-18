// SPDX-License-Identifier: MPL-2.0
/* CPU process time includes workers, excludes sleeping/driver waits. GPU elapsed
 * queries are polled asynchronously, never glFinish. RSS is current process
 * memory. Radeon VRAM is global driver accounting; other drivers use the engine
 * allocation estimate. No GPU synchronization or framebuffer readback. */
#ifdef __linux__
#include <fcntl.h>
#include <sys/ioctl.h>
/* Stable Linux Radeon DRM UAPI, without a libdrm build dependency:
 * include/uapi/drm/radeon_drm.h (GEM_INFO, INFO / VRAM_USAGE).
 * VRAM_USAGE reads TTM accounting, not hardware registers or a GPU query. */
typedef struct { uint32_t request,pad; uint64_t value; } PerfRadeonInfo;
typedef struct { uint64_t gartSize,vramSize,vramVisible; } PerfRadeonGemInfo;
static int perfRadeonFD=-1;
#endif
static int perfVRAMActual;
static float perfVRAMMiB,perfVRAMCapacity=56;
static double perfVRAMNext;
static void perfMemoryClose(void) {
#ifdef __linux__
  if(perfRadeonFD>=0)close(perfRadeonFD);
  perfRadeonFD=-1;
#endif
  perfVRAMActual=0;perfVRAMCapacity=56;
}
static int perfMemoryRead(void) {
#ifdef __linux__
  uint64_t bytes=0;
  PerfRadeonInfo info={0x1e,0,(uint64_t)(uintptr_t)&bytes};
  if(perfRadeonFD>=0 && !ioctl(perfRadeonFD,_IOWR('d',0x67,PerfRadeonInfo),&info)) {
    perfVRAMMiB=bytes/1048576.f;return 1;
  }
#endif
  return 0;
}
static void perfMemoryInit(void) {
#ifdef __linux__
  const char *vendor=(const char*)glGetString(GL_VENDOR);
  /* Only the target R300 driver, with one render node: do not attribute an
   * unrelated GPU's allocations to this context on multi-GPU machines. */
  if(!vendor || strcmp(vendor,"X.Org R300 Project"))return;
  int node=-1;char path[128],driver[256];struct stat st;
  for(int i=128;i<192;i++) {
    snprintf(path,sizeof(path),"/sys/class/drm/renderD%d",i);
    if(!stat(path,&st)){if(node>=0)return;node=i;}
  }
  if(node<0)return;
  snprintf(path,sizeof(path),"/sys/class/drm/renderD%d/device/driver",node);
  ssize_t length=readlink(path,driver,sizeof(driver)-1);
  if(length<0)return;
  driver[length]=0;const char *name=strrchr(driver,'/');
  if(!name || strcmp(name+1,"radeon"))return;
  snprintf(path,sizeof(path),"/dev/dri/renderD%d",node);
  perfRadeonFD=open(path,O_RDONLY|O_CLOEXEC);
  PerfRadeonGemInfo info={0};
  if(perfRadeonFD<0 || ioctl(perfRadeonFD,_IOWR('d',0x5c,PerfRadeonGemInfo),&info) || !info.vramSize || !perfMemoryRead()) {
    perfMemoryClose();return;
  }
  perfVRAMActual=1;perfVRAMCapacity=info.vramSize/1048576.f;perfVRAMNext=.5;
#endif
}
#define PERF_HISTORY 200
#define PERF_QUERIES 8
#ifndef GL_TIME_ELAPSED
#define GL_TIME_ELAPSED 0x88BF
#endif
typedef void (APIENTRY *PerfQueryResult64)(GLuint,GLenum,Uint64*);
typedef struct { float cpu,gpu,ram,vram; double time; } PerfPoint;
static PerfPoint perfHistory[PERF_HISTORY];
static GLuint perfQueries[PERF_QUERIES];
static unsigned char perfPending[PERF_QUERIES];
static PerfQueryResult64 perfResult64;
static int perfActive=-1,perfHead,perfCount,perfFrames,perfGPUFrames;
static double perfLastCPU,perfCPUTime,perfGPUTime,perfWall,perfElapsed;
static float perfFPS;
static double perfCPUClock(void) {
  struct timespec t;if(clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&t))return 0;
  return t.tv_sec*1000.0+t.tv_nsec*.000001;
}
static float perfResidentMiB(void) {
#ifdef __APPLE__
  mach_task_basic_info_data_t info;mach_msg_type_number_t count=MACH_TASK_BASIC_INFO_COUNT;
  if(task_info(mach_task_self(),MACH_TASK_BASIC_INFO,(task_info_t)&info,&count)==KERN_SUCCESS)
    return info.resident_size/1048576.f;
#else
  FILE *f=fopen("/proc/self/statm","r");unsigned long total,residentPages;
  if(f){int ok=fscanf(f,"%lu %lu",&total,&residentPages)==2;fclose(f);if(ok)return residentPages*(double)sysconf(_SC_PAGESIZE)/1048576.;}
#endif
  return -1;
}
static void perfInit(void) {
  const char *extensions=(const char*)glGetString(GL_EXTENSIONS);
  if(extensions && (strstr(extensions,"GL_ARB_timer_query") || strstr(extensions,"GL_EXT_timer_query"))) {
    perfResult64=(PerfQueryResult64)SDL_GL_GetProcAddress("glGetQueryObjectui64v");
    if(!perfResult64)perfResult64=(PerfQueryResult64)SDL_GL_GetProcAddress("glGetQueryObjectui64vEXT");
    GLint bits=0;if(perfResult64)glGetQueryiv(GL_TIME_ELAPSED,GL_QUERY_COUNTER_BITS,&bits);
    if(bits)glGenQueries(PERF_QUERIES,perfQueries);else perfResult64=NULL;
  }
  perfLastCPU=perfCPUClock();
  perfMemoryInit();
  printf("PERF_OVERLAY gpu_timer=%s cpu=process_time ram=current_rss gpu_memory=%s capacity_mib=%.0f\n",perfResult64?"asynchronous":"unavailable",perfVRAMActual?"radeon_global_vram_2hz":"allocation_estimate",perfVRAMCapacity);
}
static void perfBegin(void) {
  if(!perfResult64)return;
  for(int i=0;i<PERF_QUERIES;i++)if(perfPending[i]) {
    GLuint available=0;glGetQueryObjectuiv(perfQueries[i],GL_QUERY_RESULT_AVAILABLE,&available);
    if(available) {
      Uint64 ns=0;perfResult64(perfQueries[i],GL_QUERY_RESULT,&ns);
      perfGPUTime+=ns*.000001;perfGPUFrames++;perfPending[i]=0;
    }
  }
  for(int i=0;i<PERF_QUERIES;i++)if(!perfPending[i]) {
    perfActive=i;glBeginQuery(GL_TIME_ELAPSED,perfQueries[i]);break;
  }
}
static void perfEnd(void) {
  if(perfActive<0)return;
  glEndQuery(GL_TIME_ELAPSED);perfPending[perfActive]=1;perfActive=-1;
}
static void perfFrame(double wallMS) {
  double now=perfCPUClock();perfCPUTime+=fmax(0,now-perfLastCPU);perfLastCPU=now;
  perfWall+=wallMS;perfElapsed+=wallMS*.001;perfFrames++;
  if(perfWall<100)return;
  if(perfVRAMActual && perfElapsed>=perfVRAMNext) {
    perfVRAMNext=perfElapsed+.5;
    if(!perfMemoryRead()) {
      perfMemoryClose();
      /* Do not connect global allocations to the process estimate in a graph. */
      for(int i=0;i<PERF_HISTORY;i++)perfHistory[i].vram=-1;
    }
  }
  float vram=perfVRAMActual?perfVRAMMiB:vramEstimate()/1048576.f;
  perfHistory[perfHead]=(PerfPoint){perfCPUTime/perfFrames,perfGPUFrames?perfGPUTime/perfGPUFrames:-1,perfResidentMiB(),vram,perfElapsed};
  float instant=1000*perfFrames/perfWall,weight=1-expf(-perfWall/500);
  perfFPS=perfFPS>0?perfFPS+(instant-perfFPS)*weight:instant;
  perfHead=(perfHead+1)%PERF_HISTORY;if(perfCount<PERF_HISTORY)perfCount++;
  perfCPUTime=perfGPUTime=perfWall=0;perfFrames=perfGPUFrames=0;
}
static void perfRect(float x,float y,float w,float h) {
  glBegin(GL_QUADS);glVertex2f(x,y);glVertex2f(x+w,y);glVertex2f(x+w,y+h);glVertex2f(x,y+h);glEnd();
}
static float perfValue(PerfPoint p,int field) {
  return field==0?p.cpu:field==1?p.gpu:field==2?p.ram:p.vram;
}
static void perfGraph(float x,float y,float w,float h,int field,float maximum) {
  int open=0;
  for(int i=0;i<perfCount;i++) {
    PerfPoint point=perfHistory[(perfHead-perfCount+i+PERF_HISTORY)%PERF_HISTORY];
    double age=perfElapsed-point.time;float v=perfValue(point,field);
    if(age>20)continue;
    if(v<0){if(open)glEnd();open=0;continue;}
    if(!open){glBegin(GL_LINE_STRIP);open=1;}
    glVertex2f(x+w*(1-age/20),y+h*(1-clampf(v/maximum,0,1)));
  }
  if(open)glEnd();
}
static void overlay(void) {
  glUseProgram(0);glDisable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);
  glActiveTexture(GL_TEXTURE0);glDisable(GL_TEXTURE_2D);
  glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrtho(0,width,height,0,-1,1);
  glMatrixMode(GL_MODELVIEW);glLoadIdentity();
  glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
  glColor4f(.01f,.018f,.03f,.88f);perfRect(16,16,212,128);
  glColor4f(.91f,.96f,1,1);char text[100];
  snprintf(text,sizeof(text),"%.0F FPS",perfFPS);label(24,24,text,2.5f);
  PerfPoint latest=perfCount?perfHistory[(perfHead+PERF_HISTORY-1)%PERF_HISTORY]:(PerfPoint){0,-1,-1,0,0};
  float computeMax=10,ramMax=64;
  for(int i=0;i<perfCount;i++) {
    if(perfElapsed-perfHistory[i].time>20)continue;
    computeMax=fmaxf(computeMax,fmaxf(perfHistory[i].cpu,perfHistory[i].gpu));ramMax=fmaxf(ramMax,perfHistory[i].ram);
  }
  computeMax=ceilf(computeMax/10)*10;ramMax=ceilf(ramMax/64)*64;
  glColor4f(.3f,.85f,1,1);snprintf(text,sizeof(text),"CPU %.1F MS",latest.cpu);label(24,50,text,1);
  glColor4f(1,.68f,.27f,1);if(latest.gpu<0)snprintf(text,sizeof(text),"GPU N/A");else snprintf(text,sizeof(text),"GPU %.1F MS",latest.gpu);label(126,50,text,1);
  glColor4f(.16f,.23f,.29f,1);perfRect(24,62,196,23);
  glColor4f(.3f,.85f,1,1);perfGraph(24,62,196,23,0,computeMax);
  glColor4f(1,.68f,.27f,1);perfGraph(24,62,196,23,1,computeMax);
  glColor4f(.65f,.73f,.8f,1);snprintf(text,sizeof(text),"0-%.0F MS / FRAME   20 S",computeMax);label(24,90,text,1);
  glColor4f(.3f,.85f,1,1);if(latest.ram<0)snprintf(text,sizeof(text),"RAM N/A");else snprintf(text,sizeof(text),"RAM %.0F MIB",latest.ram);label(24,105,text,1);
  glColor4f(1,.68f,.27f,1);snprintf(text,sizeof(text),"%s %.1F MIB",perfVRAMActual?"VRAM":"EST",latest.vram);label(126,105,text,1);
  glColor4f(.16f,.23f,.29f,1);perfRect(24,118,94,18);perfRect(126,118,94,18);
  glColor4f(.3f,.85f,1,1);perfGraph(24,118,94,18,2,ramMax);
  glColor4f(1,.68f,.27f,1);perfGraph(126,118,94,18,3,perfVRAMCapacity);

  glDisable(GL_BLEND);
}
static void perfClose(void) {if(perfResult64)glDeleteQueries(PERF_QUERIES,perfQueries);perfMemoryClose();}
