// SPDX-License-Identifier: MPL-2.0
static uint32_t hash3(int x, int y, int z) {
  uint32_t n = (uint32_t)x * 73856093u ^ (uint32_t)y * 19349663u ^
               (uint32_t)z * 83492791u;
  n ^= worldSeed;
  n ^= n >> 13;
  n *= 1274126177u;
  return n ^ (n >> 16);
}
static float mixf(float a, float b, float t) { return a + (b - a) * t; }
static float noise3(V3 p) {
  int x = (int)floorf(p.x), y = (int)floorf(p.y), z = (int)floorf(p.z);
  float a = p.x - x, b = p.y - y, c = p.z - z;
  a = a * a * (3 - 2 * a);
  b = b * b * (3 - 2 * b);
  c = c * c * (3 - 2 * c);
  float v[8];
  for (int k = 0; k < 8; k++)
    v[k] =
        (hash3(x + (k & 1), y + ((k >> 1) & 1), z + ((k >> 2) & 1)) & 65535) /
        65535.0f;
  return mixf(mixf(mixf(v[0], v[1], a), mixf(v[2], v[3], a), b),
              mixf(mixf(v[4], v[5], a), mixf(v[6], v[7], a), b), c);
}
static float legacyElevation(V3 d) {
  float continent = noise3(add(mul(d, 3.1f), v3(7, 11, 3)));
  float ridge = 1 - fabsf(noise3(mul(d, 85)) * 2 - 1);
  float mountain = clampf((noise3(mul(d, 21)) - .35f) * 2.5f, 0, 1);
  float hills = noise3(mul(d, RADIUS / 650.0f));
  float local = noise3(mul(d, RADIUS / 95.0f));
  float rock = noise3(mul(d, RADIUS / 14.0f));
  return (continent - .51f) * 12000 + ridge * ridge * mountain * 3400 +
         (hills - .5f) * 420 + (local - .5f) * 65 + (rock - .5f) * 5 - 800;
}
