#!/usr/bin/env python3
"""pmfg: lay the files of data/rawfs/<narc path>/ over that NARC's members, each named by its member index.

data/rawfs holds the ROM files hg-engine has no source form of (maps, matrices, textures, ...), written by PMFG's
tools/dspre_sync.py from DSPRE edits. An index one past the last member appends it, so new maps can be added in order.

    python scripts/rawfs_overlay.py base/root/a/0/6/5 data/rawfs/a/0/6/5
"""

import os
import sys

import ndspy.narc

target, members = sys.argv[1:]
narc = ndspy.narc.NARC.fromFile(target)

for name in sorted(os.listdir(members), key = int):
    idx = int(name)
    with open(os.path.join(members, name), "rb") as fp:
        data = fp.read()
    if idx < len(narc.files):
        narc.files[idx] = data
    elif idx == len(narc.files):
        narc.files.append(data)
    else:
        sys.exit(f"{members}/{name}: {target} has {len(narc.files)} members, so the next new one must be {len(narc.files)}")

narc.saveToFile(target)
print(f"rawfs: {target} <- {len(os.listdir(members))} member(s)")
