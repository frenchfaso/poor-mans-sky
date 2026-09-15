// SPDX-License-Identifier: MPL-2.0
#define main poor_mans_sky_application_main
#include "../poor-mans-sky.c"
#undef main
int main(void) {
  cameraEye = v3(0, 0, 0);
  int accepted = 0, rejected = 0;
  float worst = 0;
  for (int d = 0; d < 9; d++)
    for (int r = 0; r < 8; r++)
      for (int h = 0; h < 3; h++) {
        float distance = 10 * powf(3, d), radius = .5f * powf(3, r),
              factor = h == 0   ? 1
                       : h == 1 ? .5f
                                : .01f;
        Node n = {0};
        n.center = v3(distance * .6f, 0, distance * .8f);
        n.boundCenter = n.center;
        n.boundRadius = radius;
        float plane[4];
        if (!fogCoefficients(&n, factor, plane)) {
          rejected++;
          continue;
        }
        accepted++;
        for (int i = 0; i < 500; i++) {
          float z = 1 - 2 * (i + .5f) / 500.0f, a = i * 2.39996323f;
          V3 p = v3(radius * sqrtf(1 - z * z) * cosf(a), radius * z,
                    radius * sqrtf(1 - z * z) * sinf(a));
          V3 delta = add(n.center, p);
          float exact =
              factor * (1 - exp2f(-sqrtf(dot(delta, delta)) * .00009f));
          float approx = clampf(p.x * plane[0] + p.y * plane[1] +
                                    p.z * plane[2] + plane[3],
                                0, factor);
          float error = fabsf(exact - approx);
          worst = fmaxf(worst, error);
          if (error > .00302f) {
            fprintf(stderr, "FAIL fog error %.6f distance %.1f radius %.1f\n",
                    error, distance, radius);
            return 1;
          }
        }
      }
  if (!accepted || !rejected)
    return 2;
  printf("PASS fog error bound: %d accepted, %d fallback, worst sampled error "
         "%.6f\n",
         accepted, rejected, worst);
  return 0;
}
