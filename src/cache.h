// SPDX-License-Identifier: MPL-2.0
/* Version 3: little-endian IEEE754 payloads in bounded regional packs.
 * Index is append-only and checksummed; incomplete tails are discarded. Workers
 * serialize I/O, so a pack cannot be evicted while another worker reads it.
 * Only one process may write a cache directory (advisory lifetime lock). */
#include "cache-build.h"
#include <dirent.h>
#include <fcntl.h>
#include <sys/file.h>
#include <time.h>
#include <unistd.h>
static void die(const char *s);
#define DISK_LIMIT (4096ull * 1024 * 1024)
#define PACK_LIMIT (64u * 1024 * 1024)
#define CACHE_TABLE 262144
#define CACHE_PACKS 512
static const char *cacheDirectory = "./cache";
static int cacheEnabled = 1;
static SDL_mutex *diskMutex;
static uint64_t diskBytes, diskHits[7], diskMisses[7], diskWrites, diskBad,
    diskSkipped, diskEvicted;
static uint64_t diskLimit = DISK_LIMIT;
static int cacheLock = -1;
typedef struct {
  uint32_t key[5], offset, na, nb, sum;
  int pack, next;
} CacheEntry;
typedef struct {
  char base[240];
  uint32_t bytes, indexBytes;
  int kind, face, rx, ry, segment, fd;
  uint64_t used;
} CachePack;
static CachePack packs[CACHE_PACKS];
static int packCount, entryCount, *cacheBuckets;
static CacheEntry *cacheEntries;
static uint64_t cacheClock;
static uint32_t cacheHash(const void *data, size_t size, uint32_t h) {
  const unsigned char *p = data;
  while (size--)
    h = (h ^ *p++) * 16777619u;
  return h;
}
static const char *cacheDomain(int kind) {
  static const char *d[] = {CACHE_TERRAIN_ID, CACHE_TERRAIN_ID,
                            CACHE_NATURE_ID,  CACHE_SKY_ID,
                            CACHE_FOLIAGE_ID, CACHE_GEOLOGY_ID, CACHE_MOON_ID};
  return d[kind];
}
static void packPath(char *path, size_t n, int id, const char *ext) {
  snprintf(path, n, "%s/%s.%s", cacheDirectory, packs[id].base, ext);
}
static int entrySlot(const uint32_t *key) {
  return cacheHash(key, 20, 2166136261u) & (CACHE_TABLE - 1);
}
static CacheEntry *entryFind(const uint32_t *key) {
  for (int i = cacheBuckets[entrySlot(key)]; i >= 0; i = cacheEntries[i].next)
    if (!memcmp(cacheEntries[i].key, key, 20))
      return &cacheEntries[i];
  return NULL;
}
static void entryPut(int pack, const uint32_t *h) {
  CacheEntry *e = entryFind(h);
  if (!e) {
    if (entryCount == CACHE_TABLE) {
      /* Reclaim entries belonging to evicted packs during long explorations. */
      int live = 0;
      for (int i = 0; i < entryCount; i++)
        if (cacheEntries[i].pack >= 0) cacheEntries[live++] = cacheEntries[i];
      entryCount = live;
      for (int i = 0; i < CACHE_TABLE; i++) cacheBuckets[i] = -1;
      for (int i = 0; i < live; i++) {
        int bucket = entrySlot(cacheEntries[i].key);
        cacheEntries[i].next = cacheBuckets[bucket]; cacheBuckets[bucket] = i;
      }
      if (entryCount == CACHE_TABLE) return;
    }
    int slot = entrySlot(h), i = entryCount++;
    e = &cacheEntries[i];
    e->next = cacheBuckets[slot];
    cacheBuckets[slot] = i;
    memcpy(e->key, h, 20);
  }
  e->pack = pack;
  e->offset = h[5];
  e->na = h[6];
  e->nb = h[7];
  e->sum = h[8];
}
static int packFD(int id) {
  CachePack *p = &packs[id];
  p->used = ++cacheClock;
  if (p->fd >= 0)
    return p->fd;
  int openCount = 0, old = -1;
  for (int i = 0; i < packCount; i++)
    if (packs[i].fd >= 0) {
      openCount++;
      if (old < 0 || packs[i].used < packs[old].used)
        old = i;
    }
  if (openCount >= 24) {
    close(packs[old].fd);
    packs[old].fd = -1;
  }
  char path[1024];
  packPath(path, sizeof(path), id, "dat");
  p->fd = open(path, O_RDWR | O_CREAT, 0644);
  return p->fd;
}
static int readAt(int fd, void *data, size_t n, off_t off) {
  unsigned char *p = data;
  while (n) {
    ssize_t r = pread(fd, p, n, off);
    if (r <= 0)
      return 0;
    p += r;
    n -= r;
    off += r;
  }
  return 1;
}
static int writeAt(int fd, const void *data, size_t n, off_t off) {
  const unsigned char *p = data;
  while (n) {
    ssize_t r = pwrite(fd, p, n, off);
    if (r <= 0)
      return 0;
    p += r;
    n -= r;
    off += r;
  }
  return 1;
}
static int packRoom(uint64_t incoming, int keep) {
  while (diskBytes + incoming > diskLimit) {
    int old = -1;
    for (int i = 0; i < packCount; i++)
      if (i != keep && packs[i].bytes &&
          (old < 0 || packs[i].used < packs[old].used))
        old = i;
    if (old < 0)
      return 0;
    CachePack *p = &packs[old];
    char path[1024];
    if (p->fd >= 0) {
      close(p->fd);
      p->fd = -1;
    }
    packPath(path, sizeof(path), old, "dat");
    if (unlink(path))
      return 0;
    packPath(path, sizeof(path), old, "idx");
    unlink(path);
    diskBytes -= p->bytes + p->indexBytes;
    p->bytes = p->indexBytes = 0;
    for (int i = 0; i < entryCount; i++)
      if (cacheEntries[i].pack == old)
        cacheEntries[i].pack = -1;
    diskEvicted++;
  }
  return 1;
}
static void cacheInit(void) {
  diskMutex = SDL_CreateMutex();
  if (!diskMutex)
    die("disk mutex");
  if (!cacheEnabled)
    return;
  uint32_t endian = 1;
  float one = 1;
  uint32_t bits;
  memcpy(&bits, &one, 4);
  if (*(unsigned char *)&endian != 1 || sizeof(float) != 4 ||
      bits != 0x3f800000)
    die("cache requires LE IEEE754");
  mkdir(cacheDirectory, 0755);
  char path[1024];
  snprintf(path, sizeof(path), "%s/writer.lock", cacheDirectory);
  cacheLock = open(path, O_CREAT | O_RDWR, 0644);
  if (cacheLock < 0 || flock(cacheLock, LOCK_EX | LOCK_NB)) {
    cacheEnabled = 0;
    fprintf(stderr, "CACHE unavailable or in use; generation fallback\n");
    return;
  }
  cacheBuckets = malloc(CACHE_TABLE * sizeof(int));
  cacheEntries = calloc(CACHE_TABLE, sizeof(CacheEntry));
  if (!cacheBuckets || !cacheEntries)
    die("cache index allocation");
  for (int i = 0; i < CACHE_TABLE; i++)
    cacheBuckets[i] = -1;
  DIR *dir = opendir(cacheDirectory);
  if (!dir) {
    cacheEnabled = 0;
    return;
  }
  struct dirent *de;
  while ((de = readdir(dir)) && packCount < CACHE_PACKS) {
    char hash[33];
    unsigned seed;
    int g, k, f, x, y, seg, n = 0;
    if (sscanf(de->d_name,
               "r3-%32[0123456789abcdef]-%u-%d-%d-%d-%d-%d-%d.idx%n", hash,
               &seed, &g, &k, &f, &x, &y, &seg, &n) != 8 ||
        n != (int)strlen(de->d_name) || k < 1 || k > 6 || seed != worldSeed ||
        g != geological || strcmp(hash, cacheDomain(k)))
      continue;
    int id = packCount++;
    CachePack *p = &packs[id];
    snprintf(p->base, sizeof(p->base), "%.*s", n - 4, de->d_name);
    p->kind = k;
    p->face = f;
    p->rx = x;
    p->ry = y;
    p->segment = seg;
    p->fd = -1;
    packPath(path, sizeof(path), id, "dat");
    struct stat st;
    if (stat(path, &st) || !S_ISREG(st.st_mode) || st.st_size < 0 ||
        (uint64_t)st.st_size > PACK_LIMIT) {
      packCount--;
      continue;
    }
    p->bytes = st.st_size;
    p->used = ++cacheClock;
    packPath(path, sizeof(path), id, "idx");
    FILE *idx = fopen(path, "rb");
    if (!idx) {
      packCount--;
      continue;
    }
    uint32_t h[10];
    while (fread(h, sizeof(h), 1, idx) == 1) {
      if (h[9] != cacheHash(h, 36, 2166136261u) || h[0] != (uint32_t)k ||
          h[1] != (uint32_t)f || (uint64_t)h[5] + h[6] + h[7] > p->bytes) {
        diskBad++;
        break;
      }
      entryPut(id, h);
      p->indexBytes += sizeof(h);
    }
    fclose(idx);
    truncate(path, p->indexBytes);
    diskBytes += p->bytes + p->indexBytes;
  }
  closedir(dir);
  printf("DISK_CACHE indexed packs=%d entries=%d build=%s size=%.1fMiB\n",
         packCount, entryCount, CACHE_BUILD_ID, diskBytes / 1048576.);
}
static int cacheRead(int kind, int face, int level, int x, int y, void *a,
                     uint32_t na, void *b, uint32_t maxb, uint32_t *nb) {
  if (!cacheEnabled)
    return 0;
  uint32_t key[5] = {kind, face, level, x, y};
  int ok = 0;
  SDL_LockMutex(diskMutex);
  CacheEntry *e = entryFind(key);
  if (e && e->pack >= 0 && e->na == na && e->nb <= maxb) {
    int fd = packFD(e->pack);
    ok = fd >= 0 && readAt(fd, a, na, e->offset) &&
         readAt(fd, b, e->nb, (off_t)e->offset + na);
    if (ok)
      ok = cacheHash(b, e->nb, cacheHash(a, na, 2166136261u)) == e->sum;
    if (ok)
      *nb = e->nb;
    else {
      diskBad++;
      e->pack = -1;
    }
  }
  if (ok)
    diskHits[kind]++;
  else
    diskMisses[kind]++;
  SDL_UnlockMutex(diskMutex);
  return ok;
}
static void cacheWrite(int kind, int face, int level, int x, int y,
                       const void *a, uint32_t na, const void *b, uint32_t nb) {
  if (!cacheEnabled)
    return;
  uint64_t bytes = (uint64_t)na + nb;
  if (bytes > PACK_LIMIT)
    return;
  int shift = level > 1 ? level - 1 : 0; /* four quadrants per cube face */
  int rx = level < 30 ? x >> shift : 0, ry = level < 30 ? y >> shift : 0;
  if (kind != 1) {
    rx = ry = 0;
  }
  SDL_LockMutex(diskMutex);
  int id = -1, seg = 0;
  for (int i = 0; i < packCount; i++)
    if (packs[i].kind == kind && packs[i].face == face && packs[i].rx == rx &&
        packs[i].ry == ry) {
      if (packs[i].segment >= seg)
        seg = packs[i].segment + 1;
      if (packs[i].bytes + bytes <= PACK_LIMIT)
        id = i;
    }
  if (id < 0 && packCount < CACHE_PACKS) {
    id = packCount++;
    CachePack *p = &packs[id];
    p->kind = kind;
    p->face = face;
    p->rx = rx;
    p->ry = ry;
    p->segment = seg;
    p->fd = -1;
    snprintf(p->base, sizeof(p->base), "r3-%s-%u-%d-%d-%d-%d-%d-%d",
             cacheDomain(kind), worldSeed, geological, kind, face, rx, ry, seg);
  }
  if (id < 0 || !packRoom(bytes + 40, id)) {
    diskSkipped++;
    SDL_UnlockMutex(diskMutex);
    return;
  }
  CachePack *p = &packs[id];
  int fd = packFD(id);
  uint32_t h[10] = {kind, face, level,
                    x,    y,    p->bytes,
                    na,   nb,   cacheHash(b, nb, cacheHash(a, na, 2166136261u)),
                    0};
  h[9] = cacheHash(h, 36, 2166136261u);
  int ok = fd >= 0 && writeAt(fd, a, na, p->bytes) &&
           writeAt(fd, b, nb, (off_t)p->bytes + na);
  if (ok) {
    char path[1024];
    packPath(path, sizeof(path), id, "idx");
    int idx = open(path, O_WRONLY | O_CREAT, 0644);
    ok = idx >= 0 && writeAt(idx, h, sizeof(h), p->indexBytes);
    if (idx >= 0)
      close(idx);
  }
  if (ok) {
    entryPut(id, h);
    p->bytes += bytes;
    p->indexBytes += 40;
    diskBytes += bytes + 40;
    diskWrites++;
  } else
    diskSkipped++;
  SDL_UnlockMutex(diskMutex);
}
static void cacheClose(void) {
  printf(
      "DISK_CACHE terrain_hits=%llu terrain_misses=%llu nature_hits=%llu "
      "nature_misses=%llu writes=%llu corrupt=%llu skipped=%llu size=%.1fMiB\n",
      (unsigned long long)diskHits[1], (unsigned long long)diskMisses[1],
      (unsigned long long)diskHits[2], (unsigned long long)diskMisses[2],
      (unsigned long long)diskWrites, (unsigned long long)diskBad,
      (unsigned long long)diskSkipped, diskBytes / 1048576.);
  for (int i = 0; i < packCount; i++)
    if (packs[i].fd >= 0) {
      fsync(packs[i].fd);
      close(packs[i].fd);
    }
  if (cacheLock >= 0)
    close(cacheLock);
  free(cacheBuckets);
  free(cacheEntries);
  SDL_DestroyMutex(diskMutex);
}
