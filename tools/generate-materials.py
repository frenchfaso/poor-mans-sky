#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Tileable 512x512 ground/rock materials; pure Python, no downloads or packages."""
import argparse, math, struct
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
def cellular(cells, seed):
    """Periodic Worley F2-F1: low values trace boundaries between mineral cells."""
    sites = [[(random_value(x,y,seed), random_value(x,y,seed+1))
              for x in range(cells)] for y in range(cells)]
    result = []
    for y in range(SIZE):
        py = y*cells/SIZE; iy = int(py)
        for x in range(SIZE):
            px = x*cells/SIZE; ix = int(px); first = second = 100.0
            # Jitter is constrained to the middle half of each cell, so a
            # 5x5 neighborhood safely contains the two nearest feature points.
            for dy in range(-2,3):
                for dx in range(-2,3):
                    ox,oy = sites[(iy+dy)%cells][(ix+dx)%cells]
                    d = (ix+dx+.25+.5*ox-px)**2 + (iy+dy+.25+.5*oy-py)**2
                    if d < first: first,second = d,first
                    elif d < second: second = d
            result.append(math.sqrt(second)-math.sqrt(first))
    return result

def generate(output_dir=ROOT, variant='classic'):
    output_dir.mkdir(parents=True,exist_ok=True)
    layers=[field(n,SEED+i*97) for i,n in enumerate((4,8,16,32,64,128,256))]
    cells=cellular(12,SEED+701) if variant=='cellular' else None
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
            if cells is not None:
                # Thin interrupted mineral boundaries instead of contour lines.
                fissure=max(0,1-cells[i]/.055)*max(0,d-.30)*45
            light=(.4*a+.25*c+.2*e+.1*f+.05*g-.5)*110+strata*7+(grain-.5)*12-fissure
            warm=(b-.5)*14
            rock.append(tuple(byte(v) for v in (131+light+warm,128+light,120+light-warm)))
    save_bmp(output_dir/'ground-procedural.bmp',ground)
    save_bmp(output_dir/'rock-procedural.bmp',rock)
    print('Procedural materials ready: 2 x 512x512, seed',SEED,'variant',variant)
if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--variant',choices=('classic','cellular'),default='classic')
    parser.add_argument('--output-dir',type=Path,default=ROOT)
    args=parser.parse_args()
    if args.variant!='classic' and args.output_dir.resolve()==ROOT.resolve():
        parser.error('experimental materials require a separate --output-dir')
    generate(args.output_dir,args.variant)
