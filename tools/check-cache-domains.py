# SPDX-License-Identifier: MPL-2.0
from pathlib import Path
import subprocess
p=Path('src/poor-mans-sky.c'); original=p.read_text()
f=Path('src/terrain-field.h'); field=f.read_text()
def build_ids():
    subprocess.run(['make','src/cache-build.h'],check=True,stdout=subprocess.DEVNULL)
    return Path('src/cache-build.h').read_text()
base=build_ids()
try:
    changed=original.replace('* 1.8f * dt','* 1.81f * dt',1)
    assert changed!=original
    p.write_text(changed)
    assert build_ids()==base, 'input edit invalidated generated assets'
    p.write_text(original)
    changed=field.replace('continent - .51f','continent - .52f',1)
    assert changed!=field
    f.write_text(changed)
    modified=build_ids()
    for domain in ['TERRAIN','NATURE','GEOLOGY']:
        def line(s): return next(x for x in s.splitlines() if x.startswith('#define CACHE_'+domain+'_ID'))
        assert line(base)!=line(modified),domain
finally:
    p.write_text(original);f.write_text(field)
    assert build_ids()==base
print('PASS input edits preserve cache; field edits invalidate terrain/nature/geology; restored original domains')
subprocess.run(['make','-j1'],check=True,stdout=subprocess.DEVNULL)
mtime=Path('bin/poor-mans-sky').stat().st_mtime_ns
subprocess.run(['make','-j1'],check=True,stdout=subprocess.DEVNULL)
assert Path('bin/poor-mans-sky').stat().st_mtime_ns==mtime, 'unchanged launch rebuilds binary'
print('PASS unchanged make does not rebuild the executable')
