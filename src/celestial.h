// SPDX-License-Identifier: MPL-2.0
/* Kinematic ephemeris in the rotating planet frame. Cached lunar vertices
 * remain in the original body frame. Eight game days per orbit, circular. */
typedef struct {V3 center,x,y,z,sun;} CelestialFrame;
static CelestialFrame celestial={{480000,280000,560000},{1,0,0},{0,1,0},{0,0,1},{.6f,.5f,.6f}};
static float lunarPhaseOffset=.18f;
/* Fixed orientation of the orbit, chosen once from the unchanged spawn view. */
static V3 lunarOrbitAxis={0,0,1};
static float lunarOrbitCos=1, lunarOrbitSin;
static V3 celestialVector(const CelestialFrame *f,V3 p){return add(mul(f->x,p.x),add(mul(f->y,p.y),mul(f->z,p.z)));}
static V3 celestialInverse(const CelestialFrame *f,V3 p){return v3(dot(p,f->x),dot(p,f->y),dot(p,f->z));}
static V3 moonVector(V3 p){return celestialVector(&celestial,p);}
static V3 moonLocalVector(V3 p){return celestialInverse(&celestial,p);}
static V3 moonLocalPoint(V3 p){return moonLocalVector(add(p,mul(celestial.center,-1)));}
static V3 moonWorldPoint(V3 p){return add(celestial.center,moonVector(p));}
static V3 celestialRotateZ(V3 p,double a){float c=cos(a),s=sin(a);return v3(c*p.x-s*p.y,s*p.x+c*p.y,p.z);}
static V3 celestialOrbitVector(V3 p,double orbit,double spin) {
 V3 radial=norm(v3(480000,280000,560000)),tangent=norm(cross(v3(0,0,1),radial)),north=cross(radial,tangent);
 V3 q=celestialRotateZ(v3(dot(p,radial),dot(p,tangent),dot(p,north)),orbit);
 float c=cosf(5*PI/180),s=sinf(5*PI/180);
 q=v3(q.x,c*q.y-s*q.z,s*q.y+c*q.z);
 q=add(mul(q,lunarOrbitCos),add(mul(cross(lunarOrbitAxis,q),lunarOrbitSin),
       mul(lunarOrbitAxis,dot(lunarOrbitAxis,q)*(1-lunarOrbitCos))));
 return celestialRotateZ(q,spin);
}
static CelestialFrame celestialAt(double seconds,float dayPhase,float monthPhase) {
 double days=seconds/dayLength,spin=2.0*3.141592653589793*(days+dayPhase),orbit=2.0*3.141592653589793*(days/8.0+monthPhase);
 CelestialFrame f;
 f.x=celestialOrbitVector(v3(1,0,0),orbit,spin);f.y=celestialOrbitVector(v3(0,1,0),orbit,spin);f.z=celestialOrbitVector(v3(0,0,1),orbit,spin);
 f.center=celestialVector(&f,v3(480000,280000,560000));
 /* Slow seasonal revolution makes the solar direction independent of the
  * eight-day lunar orbit. Planet coordinates account for daily rotation. */
 f.sun=celestialRotateZ(v3(1,0,0),spin+2.0*3.141592653589793*days/96.0);
 return f;
}
/* Place the default-time Moon above/right of the initial horizon. Rotate
 * its entire body frame as well as its center, preserving synchronous rotation,
 * distance and cached local terrain. Explicit time/phase offsets still apply. */
static void celestialFrameSpawnMoon(V3 observer,V3 forward) {
 V3 up=norm(observer),right=norm(cross(forward,up));
 V3 ray=norm(add(forward,add(mul(right,.60f),mul(up,.14f))));
 const V3 original={480000,280000,560000};
 float along=dot(observer,ray);
 float travel=-along+sqrtf(along*along+dot(original,original)-dot(observer,observer));
 V3 target=norm(celestialRotateZ(add(observer,mul(ray,travel)),-2.0*PI*.12));
 lunarOrbitCos=1;lunarOrbitSin=0;
 V3 source=norm(celestialOrbitVector(original,2.0*PI*.18,0));
 lunarOrbitCos=clampf(dot(source,target),-1,1);
 V3 axis=cross(source,target);float length=sqrtf(dot(axis,axis));
 if(length>1e-6f) {
  lunarOrbitAxis=mul(axis,1/length);lunarOrbitSin=length;
 } else {
  lunarOrbitAxis=norm(cross(source,fabsf(source.y)<.9f?v3(0,1,0):v3(1,0,0)));
  lunarOrbitSin=0;
 }
}
static float lunarIlluminatedFraction(V3 observer){return clampf(.5f+.5f*dot(norm(add(observer,mul(celestial.center,-1))),celestial.sun),0,1);}
static float lunarPlanetshine(void) {
 float cosine=clampf(dot(norm(celestial.center),celestial.sun),-1,1),a=acosf(cosine);
 float phase=(sinf(a)+(PI-a)*cosine)/PI;
 return .30f*RADIUS*RADIUS/dot(celestial.center,celestial.center)*phase;
}
