# SPDX-License-Identifier: MPL-2.0
"""Runtime-only edits preserve existing packs; generator edits invalidate them."""
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parent.parent
nature=root/'src/nature.h';visibility=root/'src/visibility.h'
original=nature.read_text();original_visibility=visibility.read_text()
def ids():
 subprocess.run(['python3',str(root/'tools/cache-fingerprint.py')],check=True)
 return (root/'src/cache-build.h').read_text()
base=ids()
try:
 nature.write_text(original.replace('static void natureDrawPass(int reflected) {','static void natureDrawPass(int reflected) { /* runtime-only test */',1))
 visibility.write_text(original_visibility.replace('static void occlusionPrepare(void) {','static void occlusionPrepare(void) { /* runtime-only test */',1))
 assert ids()==base,'render changes must not invalidate cache'
 nature.write_text(original.replace('nrng ^= nrng << 13;','nrng ^= nrng << 14;',1))
 assert ids()!=base,'generator changes must invalidate cache'
finally:
 nature.write_text(original);visibility.write_text(original_visibility)
 assert ids()==base
print('PASS runtime cache isolation, generator invalidation, exact restoration')
