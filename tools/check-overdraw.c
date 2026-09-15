// SPDX-License-Identifier: MPL-2.0
#define main poor_mans_sky_application_main
#include "../poor-mans-sky.c"
#undef main
int main(int argc,char **argv) {
 overdrawView=1;
 int result=poor_mans_sky_application_main(argc,argv);
 if(!result)puts("PASS overdraw GL pipeline and cleanup");
 return result;
}
