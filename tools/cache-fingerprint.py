#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Platform independent generator identities; payload ABI is cache-schema.h."""
import hashlib,re,pathlib,json
root=pathlib.Path(__file__).resolve().parent.parent
src=root/"src"
def clean(data):
    return re.sub(rb'^// SPDX-License-Identifier: MPL-2.0\n', b'', data, flags=re.M)
main=clean((src/'poor-mans-sky.c').read_bytes()).decode()
shared=clean((src/'engine-common.h').read_bytes()).decode().split('static void die')[0]
def function(name):
    m=re.search(r'^static .*?\b'+name+r'\([^;{}]*\)\s*\{',main,re.M)
    if not m: raise ValueError(name)
    start=main.index('{',m.start());depth=1;i=start+1
    while depth:
        depth+=(main[i]=='{')-(main[i]=='}');i+=1
    return main[m.start():i]
def payload_source(name):
    source=clean(((root if "/" in name else src)/name).read_bytes()).replace(b"streamAlloc(",b"malloc(")
    if name=='moon-field.h':
        # Only the immutable local height generator affects lunar payloads.
        text=source.decode();start=text.index('static float moonHeight(')
        begin=text.index('{',start);depth=1;end=begin+1
        while depth:
            depth+=(text[end]=='{')-(text[end]=='}');end+=1
        radius=re.search(r'^#define MOON_RADIUS .*$',text,re.M).group(0)
        source=(radius+'\n'+text[start:end]).encode()
    if name=='nature.h':
        source=source.split(b'static int natureWork')[0]
    if name=='visibility.h':
        text=source.decode();start=text.index('static void measureBounds(')
        begin=text.index('{',start);depth=1;end=begin+1
        while depth:
            depth+=(text[end]=='{')-(text[end]=='}');end+=1
        source=text[start:end].encode()
    return source
compat_path=src/'cache-compat.json'
compat=json.loads(compat_path.read_text()) if compat_path.exists() else {}
base=['cache-schema.h']
domains={
'GEOLOGY':['geology.h','terrain-field.h'],
'TERRAIN':['geology.h','terrain-field.h','terrain-bake.h','materials.h','bc-codec.h','visibility.h'],
'NATURE':['geology.h','terrain-field.h','nature.h','nature-models.h','materials.h'],
'MOON':['moon-field.h','moon-mesh.h'],
'SKY':['stars.h'],
'FOLIAGE':['nature.h','nature-models.h','materials.h','bc-codec.h','shaders/nature-bake.vert','shaders/nature-bake.frag']}
lines=[]
for domain,files in domains.items():
    h=hashlib.sha256(shared.encode())
    for name in base+files:h.update(payload_source(name))
    if domain in ('TERRAIN','NATURE','FOLIAGE'):
        for name in ('assets/ground-procedural.bmp','assets/rock-procedural.bmp'):h.update(payload_source(name))
    if domain in ('TERRAIN','NATURE'):
        for name in ('direction','surfaceNormal','nodeDir','rgb565','unpack565','compressPage'):h.update(function(name).encode())
    if domain=='TERRAIN':
        h.update(function('measureGeometry').encode())
        h.update(main[main.index('typedef struct {\n  V3 center, boundCenter'):main.index('/* Called under the streaming')].replace('streamAlloc(', 'malloc(').encode())
    if domain=='NATURE':h.update(function('homeDirection').encode())
    for line in main.splitlines():
        if re.match(r'#define (RADIUS|PAGE|PATCH|NV|MAXLEVEL) ',line):h.update(line.encode())
    digest=h.hexdigest()[:32]
    identity=compat.get(domain,{}).get(digest,digest)
    lines.append(f'#define CACHE_{domain}_ID "{identity}"')
lines.append('#define CACHE_BUILD_ID CACHE_TERRAIN_ID')
p=src/'cache-build.h';text='\n'.join(lines)+'\n'
if not p.exists() or p.read_text()!=text:p.write_text(text)
