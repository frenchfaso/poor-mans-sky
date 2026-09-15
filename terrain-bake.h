// SPDX-License-Identifier: MPL-2.0
static void generateFresh(const Node *n, unsigned char **pixels,
                          Vertex **vertices) {
  unsigned char *p = streamAlloc(PAGE * PAGE * 4);
  Vertex *v = streamAlloc(NV * sizeof(Vertex));
  float occlusion[NV];
  GeoCell region[17 * 17];
  int grid = 0;
  if (geological && geology && n->size < 8000) {
    grid = (int)clampf(ceilf(n->size / 500) + 1, 2, 17);
    for (int y = 0; y < grid; y++)
      for (int x = 0; x < grid; x++)
        region[y * grid + x] =
            geoSample(nodeDir(n, (-3.5f + x * 127.f / (grid - 1)) / 120,
                              (-3.5f + y * 127.f / (grid - 1)) / 120),
                      NULL);
  }
  if (!p || !v)
    die("asset cache allocation");
  for (int y = 0; y <= PATCH; y++)
    for (int x = 0; x <= PATCH; x++) {
      int k = y * (PATCH + 1) + x;
      float u = x / (float)PATCH, w = y / (float)PATCH;
      V3 d = nodeDir(n, u, w);
      v[k].h = elevation(d);
      v[k].p = mul(d, RADIUS + v[k].h);
      v[k].n = packNormal(surfaceNormal(d));
      V3 tangent = norm(cross(d, v3(.01f, 1, .02f))),
         bitangent = cross(tangent, d);
      float obscured = 0;
      for (int j = 0; j < 4; j++) {
        V3 ray = mul(j < 2 ? tangent : bitangent, (j & 1) ? -1 : 1);
        float horizon =
            ((geological && geology
                  ? geoSample(norm(add(d, mul(ray, 180.0f / RADIUS))), NULL).h
                  : elevation(norm(add(d, mul(ray, 180.0f / RADIUS))))) -
             v[k].h) /
            180;
        obscured += clampf(horizon, 0, 1);
      }
      occlusion[k] = 1 - .55f * obscured * .25f;

      v[k].u = u;
      v[k].v = w;
    }
  for (int y = 0; y < PAGE; y++)
    for (int x = 0; x < PAGE; x++) {
      V3 d = nodeDir(n, (x - 3.5f) / 120.0f, (y - 3.5f) / 120.0f);
      GeoCell climate;
      float h;
      if (grid) {
        float gx = x * (grid - 1) / 127.f, gy = y * (grid - 1) / 127.f;
        int ix = (int)fminf(grid - 2, gx), iy = (int)fminf(grid - 2, gy);
        float a = gx - ix, b = gy - iy;
        climate = (GeoCell){0};
        for (int j = 0; j < 2; j++)
          for (int i = 0; i < 2; i++) {
            GeoCell *c = &region[(iy + j) * grid + ix + i];
            float w = (i ? a : 1 - a) * (j ? b : 1 - b);
            climate.h += c->h * w;
            climate.wet += c->wet * w;
            climate.temperature += c->temperature * w;
            climate.flow += c->flow * w;
            climate.relief += c->relief * w;
            climate.rock += c->rock * w;
          }
        h = geoResidual(d, climate);
      } else
        h = geoElevation(d, &climate);
      float a = noise3(mul(d, 930)), b = noise3(mul(d, 4310));
      /* Band-limit unique grain to the page footprint to avoid distant
       * aliasing. */
      float footprint = n->size / 120.0f;
      float grain = noise3(mul(d, RADIUS / fmaxf(.03f, footprint * 2)));
      float flecks = noise3(mul(d, RADIUS / fmaxf(.15f, footprint * 3)));
      float gx = clampf((x - 3.5f) * PATCH / 120.0f, 0, PATCH),
            gy = clampf((y - 3.5f) * PATCH / 120.0f, 0, PATCH);
      int ix = (int)gx, iy = (int)gy, nx = ix < PATCH ? ix + 1 : ix,
          ny = iy < PATCH ? iy + 1 : iy;
      float fx = gx - ix, fy = gy - iy;
      V3 normal = norm(add(mul(add(mul(unpackNormal(v[iy * (PATCH + 1) + ix].n), 1 - fx),
                                   mul(unpackNormal(v[iy * (PATCH + 1) + nx].n), fx)),
                               1 - fy),
                           mul(add(mul(unpackNormal(v[ny * (PATCH + 1) + ix].n), 1 - fx),
                                   mul(unpackNormal(v[ny * (PATCH + 1) + nx].n), fx)),
                               fy)));
      float slope = clampf(dot(normal, d), 0, 1);
      float grass = clampf((h - 3) * .09f, 0, 1) *
                    clampf((2300 - h) * .0015f, 0, 1) *
                    clampf((slope - .64f) * 4, 0, 1);
      float snow = clampf((h - 3200 + (a - .5f) * 320) * .0018f, 0, .92f) *
                   clampf((slope - .45f) * 3, 0, 1);
      if (geological && geology) {
        grass *= clampf(climate.wet * 1.8f - .2f, 0, 1) *
                 clampf((climate.temperature + 3) / 15, 0, 1);
        snow = clampf((1 - climate.temperature) * .15f, 0, .92f) *
               clampf((slope - .45f) * 3, 0, 1);
      }
      V3 pos = mul(d, RADIUS + h);
      V3 rock = materialTriplanar(1, pos, normal, footprint);
      V3 soil = materialTriplanar(0, pos, normal, footprint);
      V3 meadow = add(mul(soil, .7f), v3(.028f, .047f, .012f));
      if (geological && geology) {
        float strata = .93f + .07f * sinf(h * .24f + climate.rock * 6);
        rock = mul(rock, strata);
        rock.x *= .85f + .3f * climate.rock;
        rock.z *= 1.1f - .25f * climate.rock;
        soil = mul(soil, 1 - .22f * climate.wet);
        meadow = add(mul(soil, .7f), v3(.028f, .047f, .012f));
        float gravel = clampf(log2f(1 + climate.flow) / 14 - .45f, 0, .5f);
        grass *= 1 - gravel;
      }
      V3 col = add(mul(rock, 1 - grass), mul(meadow, grass));
      col = mul(col, .80f + a * .20f + b * .12f + (grain - .5f) * .08f);
      col = add(mul(col, 1 - snow), mul(v3(.52f, .57f, .61f), snow));
      col = mul(col, .94f + flecks * .12f);
      float ao = (occlusion[iy * (PATCH + 1) + ix] * (1 - fx) +
                  occlusion[iy * (PATCH + 1) + nx] * fx) *
                     (1 - fy) +
                 (occlusion[ny * (PATCH + 1) + ix] * (1 - fx) +
                  occlusion[ny * (PATCH + 1) + nx] * fx) *
                     fy;
      col = mul(col, ao);

      int k = (y * PAGE + x) * 4;
      p[k] = (unsigned char)(sqrtf(clampf(col.x, 0, 1)) * 255);
      p[k + 1] = (unsigned char)(sqrtf(clampf(col.y, 0, 1)) * 255);
      p[k + 2] = (unsigned char)(sqrtf(clampf(col.z, 0, 1)) * 255);
      p[k + 3] = 255;
    }
  int base = (PATCH + 1) * (PATCH + 1);
  for (int e = 0; e < 4; e++)
    for (int j = 0; j <= PATCH; j++) {
      int a = e == 0   ? j
              : e == 1 ? j * (PATCH + 1) + PATCH
              : e == 2 ? PATCH * (PATCH + 1) + PATCH - j
                       : (PATCH - j) * (PATCH + 1);
      v[base + e * (PATCH + 1) + j] = v[a];
      v[base + e * (PATCH + 1) + j].p =
          add(v[a].p, mul(norm(v[a].p), -fmaxf(1, n->size * .06f)));
    }
  for (int j = 0; j < NV; j++)
    v[j].p = add(v[j].p, mul(n->center, -1));
  *pixels = compressPage(p);
  free(p);
  *vertices = v;
}
