#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Check the runtime cloud atlas contract and its octagonal billboard bounds."""
import struct
import sys
from pathlib import Path

path = Path(sys.argv[1]) if len(sys.argv)>1 else Path(__file__).resolve().parent.parent/'assets/cloud-procedural.rgba'
data = path.read_bytes()
assert len(data)==16+128*128*4, 'wrong payload size'
assert data[:8]==b'PMSCLOUD' and struct.unpack('<II',data[8:16])==(128,128), 'wrong header'
tiles=[]
for variant in range(4):
    alpha=[]
    for y in range(64):
        for x in range(64):
            i=16+(((variant//2)*64+y)*128+(variant%2)*64+x)*4
            r,g,b,a=data[i:i+4]
            assert r==g==b, 'runtime lighting requires neutral baked shading'
            # Include padding for bilinear filtering inside the octagonal mesh.
            u,v=(x-31.5)/31.5,(y-31.5)/31.5
            if abs(u)+abs(v)>=1.43 or x in (0,63) or y in (0,63):
                assert a==0, 'visible alpha would be clipped or bleed across tiles'
            alpha.append(a)
    assert 60<max(alpha)<230 and 5<sum(alpha)/4096<70, 'empty or excessively opaque tile'
    tiles.append(bytes(alpha))
assert len(set(tiles))==4, 'cloud variants must differ'
print('PASS cloud atlas: 64 KiB RGBA, four distinct neutral tiles, transparent borders and octagon padding')
