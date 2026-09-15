#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Tileable 512x512 ground/rock materials; pure Python, no downloads or packages."""
import math, struct
from pathlib import Path
SIZE=512
SEED=20260911
ROOT=Path(__file__).resolve().parent.parent/'assets'
def random_value(x,y,seed):
    n=(x*374761393+y*668265263+seed*2246822519)&0xffffffff
    n=((n^(n>>13))*1274126177)&0xffffffff
    return (n^(n>>16))/4294967295.0
def field(cells,seed):
    lattice=[[random_value(x,y,seed) for x in range(cells)] for y in range(cells)]
    table=[]
    for i in range(SIZE):
        t=i*cells/SIZE; k=int(t); f=t-k
        table.append((k%cells,(k+1)%cells,f*f*(3-2*f)))
    result=[]
    for y0,y1,fy in table:
        for x0,x1,fx in table:
            a=lattice[y0][x0]*(1-fx)+lattice[y0][x1]*fx
            b=lattice[y1][x0]*(1-fx)+lattice[y1][x1]*fx
            result.append(a*(1-fy)+b*fy)
    return result
def save_bmp(path,pixels):
    data=bytes(v for r,g,b in pixels for v in (b,g,r))
    header=struct.pack('<2sIHHI',b'BM',54+len(data),0,0,54)
    header+=struct.pack('<IiiHHIIiiII',40,SIZE,-SIZE,1,24,0,len(data),2835,2835,0,0)
    content=header+data
    if not path.exists() or path.read_bytes()!=content:path.write_bytes(content)
def byte(v):return max(0,min(255,round(v)))
def generate():
    ROOT.mkdir(exist_ok=True)
    layers=[field(n,SEED+i*97) for i,n in enumerate((4,8,16,32,64,128,256))]
    ground=[];rock=[]
    for y in range(SIZE):
        for x in range(SIZE):
            i=y*SIZE+x;a,b,c,d,e,f,g=(v[i] for v in layers)
            grain=random_value(x,y,SEED+13)
            # Multiscale soil, damp humus patches, tiny bright mineral grains.
            soil=.28*a+.22*b+.18*c+.14*d+.1*e+.05*f+.03*g
            grit=max(0,(grain-.92)/.08)*max(0,(e-.35))*38
            damp=max(0,(b-.52))*45
            shade=(soil-.5)*95+(grain-.5)*14+grit-damp
            moss=max(0,(a-.55))*45
            ground.append(tuple(byte(v) for v in (91+shade-moss*.3,74+shade*.85+moss,49+shade*.68)))
            # Warped, periodic stratification, mineral mottling and dark fissures.
            strata=math.sin(2*math.pi*(y*12/SIZE+1.7*b+.45*d))
            fissure=max(0,1-abs(c-.5)*55)*max(0,d-.35)*60
            light=(.4*a+.25*c+.2*e+.1*f+.05*g-.5)*110+strata*7+(grain-.5)*12-fissure
            warm=(b-.5)*14
            rock.append(tuple(byte(v) for v in (131+light+warm,128+light,120+light-warm)))
    save_bmp(ROOT/'ground-procedural.bmp',ground)
    save_bmp(ROOT/'rock-procedural.bmp',rock)
    print('Procedural materials ready: 2 x 512x512, seed',SEED)
if __name__=='__main__':generate()
