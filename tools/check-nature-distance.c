// SPDX-License-Identifier: MPL-2.0
#define main poor_mans_sky_application_main
#include "../src/poor-mans-sky.c"
#undef main
int main(void) {
  static NatureCandidate candidates[NATURE_CANDIDATES];
  for (int i=0;i<10;i++) {
    V3 d=i<8?norm(v3(i&1?1:-1,i&2?1:-1,i&4?1:-1)):norm(v3(1,i==8?0:1,0));
    int n=natureCandidates(d,candidates),faces=0;
    printf("cover=%d n=%d far=%.2f\n",i,n,candidates[n-1].distance);
    if(n<=1024 || n>=NATURE_CELLS || candidates[n-1].distance<2490) return 1;
    memset(natureLookup,0,sizeof(natureLookup));
    for(int k=0;k<n;k++) {
      NatureCandidate c=candidates[k];
      if(k && c.distance<candidates[k-1].distance) return 2;
      if(natureFind(c.face,c.x,c.y)>=0) return 3;
      nature[k]=(NatureCell){.face=c.face,.x=c.x,.y=c.y,.state=4};natureIndex(k);
      if(natureFind(c.face,c.x,c.y)!=k) return 4;
      faces|=1<<c.face;
    }
    if(i<8 && __builtin_popcount((unsigned)faces)!=3) return 5;
    printf("PASS position=%d cells=%d radius=%.2f\n",i,n,candidates[n-1].distance);
  }
  return 0;
}
