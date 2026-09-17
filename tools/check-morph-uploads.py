#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Compare deterministic GPU readbacks from the compiled C regression test."""
import subprocess
import sys
from pathlib import Path

binary = str(Path(sys.argv[1]).resolve())
states = []
for arguments in (["--immediate"], []):
    result = subprocess.run([binary, *arguments], check=True, text=True,
                            stdout=subprocess.PIPE)
    lines = result.stdout.splitlines()
    states.append([line for line in lines if line.startswith("STATE ")])
    print(next(line for line in lines if line.startswith("PASS ")))
if len(states[0]) != 14 or states[0] != states[1]:
    raise SystemExit("FAIL: immediate/deferred GPU or bounds mismatch")
print("PASS: identical GPU readbacks, bounds and LOD across all 14 poses")
