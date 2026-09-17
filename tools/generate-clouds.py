#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Bake four seeded cloud volumes into the existing 128-square RGBA atlas.

Pure Python; density uses ellipsoid guides and three octaves of value noise.
Front-to-back integration stores opacity and neutral top-light shading. Runtime
still uses one texture sample and its existing day/night tint. No book code.
"""
import argparse
import math
import struct
from pathlib import Path

SIZE = 128
TILE = SIZE // 2
SEED = 20260915
ROOT = Path(__file__).resolve().parent.parent

def noise(x, y, z, seed):
    ix, iy, iz = math.floor(x), math.floor(y), math.floor(z)
    def smooth(v):
        return v*v*(3-2*v)
    fx, fy, fz = smooth(x-ix), smooth(y-iy), smooth(z-iz)
    value = 0.0
    for k in range(2):
        for j in range(2):
            for i in range(2):
                n = ((ix+i)*374761393 + (iy+j)*668265263 + (iz+k)*2147483647 + seed*2246822519) & 0xffffffff
                n = ((n ^ (n >> 13))*1274126177) & 0xffffffff
                r = (n ^ (n >> 16))/4294967295.0
                value += r*(fx if i else 1-fx)*(fy if j else 1-fy)*(fz if k else 1-fz)
    return value

def guides(variant):
    # Broad base plus asymmetric rising lobes; each is a soft ellipsoid.
    return [(-.35,-.16,0,.39,.29,.42),(.08,-.12,0,.49,.32,.49),
            (.40,-.08,.04,.29,.29,.34),
            (-.24+.06*variant,.16,.02,.31,.40,.39),
            (.18-.045*variant,.29,-.04,.28,.32,.34)]

def density(x, y, z, shapes, seed):
    support = max(1-((x-a)/rx)**2-((y-b)/ry)**2-((z-c)/rz)**2
                  for a,b,c,rx,ry,rz in shapes)
    if support < -.34:
        return 0.0
    f = .0
    for frequency, amplitude in ((3.7,.62),(7.7,.27),(15.9,.11)):
        f += amplitude*noise(x*frequency+11,y*frequency+19,z*frequency+7,seed)
    return max(0.0, min(1.0, (support + (f-.5)*.68)*2.6))

def generate():
    pixels = bytearray(SIZE*SIZE*4)
    for variant in range(4):
        shapes, seed = guides(variant), SEED+variant*97
        for y in range(TILE):
            v = (y-31.5)/31.5
            for x in range(TILE):
                u = (x-31.5)/31.5
                # Guaranteed transparent padding inside the runtime octagon.
                edge = min(.94-abs(u), .94-abs(v), 1.43-abs(u)-abs(v))
                if edge <= 0:
                    continue
                transmittance, radiance = 1.0, 0.0
                for k in range(28):
                    z = .75-(k+.5)*1.5/28
                    d = density(u,v,z,shapes,seed)
                    if d <= 0:
                        continue
                    light_depth = 0.0
                    for step in (1,2,3,4):
                        light_depth += density(u-.025*step,v+.12*step,z+.06*step,shapes,seed)
                    light = .57+.40*math.exp(-light_depth*.55)
                    opacity = 1-math.exp(-d*1.5/28*1.25)
                    radiance += transmittance*opacity*light
                    transmittance *= 1-opacity
                opacity = (1-transmittance)*min(1.0,edge/.06)
                shade = radiance/max(1e-9,1-transmittance)
                i = (((variant//2)*TILE+y)*SIZE+(variant%2)*TILE+x)*4
                grey = round(max(0,min(1,shade))*255)
                pixels[i:i+4] = bytes((grey,grey,grey,round(opacity*255)))
    return pixels

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=ROOT/'assets/cloud-procedural.rgba')
    args=parser.parse_args()
    content=b'PMSCLOUD'+struct.pack('<II',SIZE,SIZE)+generate()
    args.output.parent.mkdir(parents=True,exist_ok=True)
    if not args.output.exists() or args.output.read_bytes()!=content:
        args.output.write_bytes(content)
    print(f'Cloud atlas ready: {SIZE}x{SIZE} RGBA, four volumes, seed {SEED}')

if __name__=='__main__':
    main()
