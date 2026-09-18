// SPDX-License-Identifier: MPL-2.0
/* Keep elementary vector operations inline even in the caster's ordered-math
 * kernel; GCC otherwise emits struct-return calls for every ray sample. */
static inline __attribute__((always_inline)) V3 v3(float x, float y, float z) {
  V3 v = {x, y, z};
  return v;
}
static inline __attribute__((always_inline)) V3 add(V3 a, V3 b) { return v3(a.x + b.x, a.y + b.y, a.z + b.z); }
static inline __attribute__((always_inline)) V3 mul(V3 a, float b) { return v3(a.x * b, a.y * b, a.z * b); }
static inline __attribute__((always_inline)) float dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static V3 cross(V3 a, V3 b) {
  return v3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x);
}
static V3 norm(V3 a) { return mul(a, 1.0f / sqrtf(fmaxf(dot(a, a), 1e-12f))); }
static float clampf(float x, float a, float b) { return fminf(fmaxf(x, a), b); }
static PackedNormal packNormal(V3 n) {
  return (PackedNormal){(int8_t)lrintf(clampf(n.x,-1,1)*127),
    (int8_t)lrintf(clampf(n.y,-1,1)*127),(int8_t)lrintf(clampf(n.z,-1,1)*127),0};
}
static V3 unpackNormal(PackedNormal n) { return v3(n.x/127.f,n.y/127.f,n.z/127.f); }
static void die(const char *s) {
  fprintf(stderr, "FATAL: %s\n", s);
  exit(1);
}
/* Assets follow the executable; cache and screenshots follow the launch cwd. */
static char resourceRoot[4096];
static void resourceInit(void) {
  char *base=SDL_GetBasePath();
  if(base) {
    snprintf(resourceRoot,sizeof(resourceRoot),"%s../",base);
    SDL_free(base);
    char probe[8192];
    snprintf(probe,sizeof(probe),"%sassets/ground-procedural.bmp",resourceRoot);
    if(access(probe,R_OK)) resourceRoot[0]=0;
  }
}
static const char *resourcePath(const char *relative) {
  static char path[8192]; /* Resource loads happen only on the main startup thread. */
  if(!resourceRoot[0])return relative;
  if(snprintf(path,sizeof(path),"%s%s",resourceRoot,relative)>=(int)sizeof(path))
    die("resource path too long");
  return path;
}
static void checkGL(const char *s) {
  GLenum e = glGetError();
  if (e) {
    fprintf(stderr, "GL error at %s: 0x%x\n", s, e);
    exit(2);
  }
}
/* Opt-in crash investigation: durable checkpoints have a measurable I/O cost. */
static FILE *gpuTrace;
static void gpuCheckpoint(const char *stage) {
 if(!gpuTrace)return;
 fprintf(gpuTrace,"%u frame=%d stage=%s eye=%.1f,%.1f,%.1f near=%.1f\n",SDL_GetTicks(),frameNo,stage,eye.x,eye.y,eye.z,clipNear);
 fflush(gpuTrace);fsync(fileno(gpuTrace));
}
static char *readText(const char *path) {
  FILE *f = fopen(resourcePath(path), "rb");
  if (!f)
    die(path);
  if (fseek(f, 0, SEEK_END))
    die("shader seek");
  long n = ftell(f);
  if (n < 0 || n > 16 * 1024 * 1024)
    die("shader size");
  if (fseek(f, 0, SEEK_SET))
    die("shader rewind");
  char *s = malloc(n + 1);
  if (!s)
    die("shader allocation");
  if (fread(s, 1, n, f) != (size_t)n)
    die("shader read");
  s[n] = 0;
  fclose(f);
  return s;
}
static GLuint shader(GLenum type, const char *path) {
  char *s = readText(path);
  GLuint id = glCreateShader(type);
  const GLchar *p = s;
  if(strstr(path,"globe-sky.frag") || strstr(path,"moon-air.vert")) {
    char *common=readText("shaders/atmosphere-common.glsl");
    const GLchar *parts[]={"#version 120\n",common,strchr(s,'\n')+1};
    glShaderSource(id,3,parts,NULL);free(common);
  } else glShaderSource(id, 1, &p, NULL);
  glCompileShader(id);
  free(s);
  GLint ok;
  glGetShaderiv(id, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    char log[8192];
    glGetShaderInfoLog(id, sizeof(log), NULL, log);
    fprintf(stderr, "%s\n%s\n", path, log);
    exit(1);
  }
  return id;
}
static int uniformCount;
static GLuint linkedPrograms[128];
static int linkedProgramCount;
static GLuint program(const char *v, const char *f) {
  gpuCheckpoint(f);
  char vp[256], fp[256];
  snprintf(vp, sizeof(vp), "shaders/%s", v);
  snprintf(fp, sizeof(fp), "shaders/%s", f);
  GLuint vs = shader(GL_VERTEX_SHADER, vp), fs = shader(GL_FRAGMENT_SHADER, fp),
         p = glCreateProgram();
  glAttachShader(p, vs);
  glAttachShader(p, fs);
  glLinkProgram(p);
  GLint ok;
  glGetProgramiv(p, GL_LINK_STATUS, &ok);
  if (!ok) {
    char log[8192];
    glGetProgramInfoLog(p, sizeof(log), NULL, log);
    fprintf(stderr, "%s: %s\n", f, log);
    exit(1);
  }
  glDeleteShader(vs);
  glDeleteShader(fs);
  if(linkedProgramCount<128)linkedPrograms[linkedProgramCount++]=p;
  printf("Shader linked: %s\n", f);
  uniformCount = 0; /* Linked program names may be recycled by GL. */
  return p;
}
static GLint uniformLocation(GLuint p, const char *name) {
  static struct {
    GLuint program;
    GLint location;
    char name[64];
  } entries[256];
  for (int i = 0; i < uniformCount; i++)
    if (entries[i].program == p && !strcmp(entries[i].name, name))
      return entries[i].location;
  GLint location = glGetUniformLocation(p, name);
  if (uniformCount < 256 && strlen(name) < sizeof(entries[0].name)) {
    entries[uniformCount].program = p;
    entries[uniformCount].location = location;
    strcpy(entries[uniformCount++].name, name);
  }
  return location;
}
static void u1(GLuint p, const char *n, float v) {
  glUniform1f(uniformLocation(p, n), v);
}
static void u2(GLuint p, const char *n, float a, float b) {
  glUniform2f(uniformLocation(p, n), a, b);
}
static void u3(GLuint p, const char *n, V3 v) {
  glUniform3f(uniformLocation(p, n), v.x, v.y, v.z);
}
static void tex(GLuint p, const char *n, int unit, GLuint id) {
  glActiveTexture(GL_TEXTURE0 + unit);
  glBindTexture(GL_TEXTURE_2D, id);
  glUniform1i(uniformLocation(p, n), unit);
  glActiveTexture(GL_TEXTURE0);
}
static Target target(int w, int h, int depth, int repeat) {
  Target t = {0};
  t.w = w;
  t.h = h;
  glGenTextures(1, &t.tex);
  glBindTexture(GL_TEXTURE_2D, t.tex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                  repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                  repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE);
  glGenFramebuffers(1, &t.fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, t.fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         t.tex, 0);
  if (depth) {
    glGenRenderbuffers(1, &t.depth);
    glBindRenderbuffer(GL_RENDERBUFFER, t.depth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                              GL_RENDERBUFFER, t.depth);
  }
  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    die("FBO incomplete");
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  return t;
}
static void quad(void) {
  glBegin(GL_QUADS);
  glTexCoord2f(0, 0);
  glVertex2f(-1, -1);
  glTexCoord2f(1, 0);
  glVertex2f(1, -1);
  glTexCoord2f(1, 1);
  glVertex2f(1, 1);
  glTexCoord2f(0, 1);
  glVertex2f(-1, 1);
  glEnd();
}
static void bake(Target t, GLuint p) {
  glBindFramebuffer(GL_FRAMEBUFFER, t.fbo);
  glViewport(0, 0, t.w, t.h);
  glUseProgram(p);
  quad();
  glFinish();
  checkGL("GPU bake");
}
static void mipmaps(Target t) {
  glBindTexture(GL_TEXTURE_2D, t.tex);
  glGenerateMipmap(GL_TEXTURE_2D);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                  GL_LINEAR_MIPMAP_LINEAR);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 4.0f);
}
static const unsigned char glyphs[43][5] = {
    {62, 81, 73, 69, 62},  {0, 66, 127, 64, 0},    {66, 97, 81, 73, 70},
    {33, 65, 69, 75, 49},  {24, 20, 18, 127, 16},  {39, 69, 69, 69, 57},
    {60, 74, 73, 73, 48},  {1, 113, 9, 5, 3},      {54, 73, 73, 73, 54},
    {6, 73, 73, 41, 30},   {126, 17, 17, 17, 126}, {127, 73, 73, 73, 54},
    {62, 65, 65, 65, 34},  {127, 65, 65, 34, 28},  {127, 73, 73, 73, 65},
    {127, 9, 9, 9, 1},     {62, 65, 73, 73, 122},  {127, 8, 8, 8, 127},
    {0, 65, 127, 65, 0},   {32, 64, 65, 63, 1},    {127, 8, 20, 34, 65},
    {127, 64, 64, 64, 64}, {127, 2, 12, 2, 127},   {127, 4, 8, 16, 127},
    {62, 65, 65, 65, 62},  {127, 9, 9, 9, 6},      {62, 65, 81, 33, 94},
    {127, 9, 25, 41, 70},  {70, 73, 73, 73, 49},   {1, 1, 127, 1, 1},
    {63, 64, 64, 64, 63},  {31, 32, 64, 32, 31},   {63, 64, 56, 64, 63},
    {99, 20, 8, 20, 99},   {3, 4, 120, 4, 3},      {97, 81, 73, 69, 67},
    {0, 0, 96, 96, 0},     {0, 0, 54, 54, 0},      {8, 8, 8, 8, 8},
    {64, 32, 16, 8, 4},    {0, 0, 127, 0, 0},      {8, 8, 62, 8, 8},
    {0, 0, 0, 0, 0}};
static void label(float x, float y, const char *s, float scale) {
  glBegin(GL_QUADS);
  for (; *s; s++, x += 6 * scale) {
    int g = *s >= '0' && *s <= '9'   ? *s - '0'
            : *s >= 'A' && *s <= 'Z' ? *s - 'A' + 10
            : *s == '.'              ? 36
            : *s == ':'              ? 37
            : *s == '-'              ? 38
            : *s == '/'              ? 39
            : *s == '|'              ? 40
            : *s == '+'              ? 41
                                     : 42;
    for (int a = 0; a < 5; a++)
      for (int b = 0; b < 7; b++)
        if (glyphs[g][a] & (1 << b)) {
          float px = x + a * scale, py = y + b * scale;
          glVertex2f(px, py);
          glVertex2f(px + scale, py);
          glVertex2f(px + scale, py + scale);
          glVertex2f(px, py + scale);
        }
  }
  glEnd();
}
