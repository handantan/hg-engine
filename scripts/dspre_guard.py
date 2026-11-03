#!/usr/bin/env python3
"""pmfg: stop the build before it overwrites a DSPRE edit to a NARC member the build makes from engine/data.

The scr_seq, zone_event and msg_data rules unpack base/root's archive and copy the members built from source over it.
DSPRE saves its edits into base/root, so an edit to one of those members (a map whose script, events or text are
already in engine/data) would be lost silently. The previous build output is what the build last copied to base/root:
a member that differs from it there was changed since, by DSPRE. PMFG's tools/dspre_sync.py folds it into engine/data
and refreshes the previous output, after which the build goes through. DSPRE_DISCARD=1 drops the edit instead.

    python scripts/dspre_guard.py base/root/a/0/3/2 build/narc/zone_event.narc build/zone_event/2_028 data/text/369.txt ...
"""

import os
import re
import sys

import ndspy.narc

target, previous, *built = sys.argv[1:]
if os.environ.get("DSPRE_DISCARD") == "1" or not (os.path.exists(target) and os.path.exists(previous)):
    sys.exit(0)

current, last = ndspy.narc.NARC.fromFile(target).files, ndspy.narc.NARC.fromFile(previous).files
# 2_028, 369.txt -> the member index
members = sorted({int(re.search(r"(\d+)\D*$", os.path.basename(f)).group(1)) for f in built})
edited = [i for i in members if i < len(current) and i < len(last) and current[i] != last[i]]
if edited:
    sys.exit(f"{target}: member(s) {', '.join(map(str, edited))} were edited in DSPRE, and the build makes them from "
             "engine/data, so it would overwrite the edit.\n"
             "Run tools/dspre_sync.py in the PMFG workspace, then build again (DSPRE_DISCARD=1 make drops the edit).")
