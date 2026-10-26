#!/usr/bin/env python3
"""Checks that the shipped data is consistent with the C++ engine.

 - every texture key a rule uses exists in terrain_texture.json and on disk
 - all tiles share one size, every 'ctm' rule has its 47 tiles
 - the 47-tile table compiled into src/CtmCore.hpp equals what is measured from
   the shipped PNGs of every 47-tile set (so data and engine cannot drift apart)
"""
import json
import re
import sys
import tempfile
import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
from PIL import Image  # noqa: E402
import derive_ctm_table as d  # noqa: E402

rules = json.loads((ROOT / "config" / "ctm_rules.json").read_text())["rules"]
tex = json.loads((ROOT / "pack" / "textures" / "terrain_texture.json").read_text())["texture_data"]
tdir = ROOT / "pack" / "textures" / "blocks" / "continuity"

errors = []
sizes = set()
for r in rules:
    need = {"ctm": 47, "horizontal": 4, "vertical": 4, "horizontal+vertical": 7,
            "vertical+horizontal": 7, "top": 1, "fixed": 1}[r["method"]]
    if len(r["tiles"]) < need:
        errors.append(f"{r['name']}: {len(r['tiles'])} tiles, needs {need}")
    for k in r["tiles"]:
        if not k:
            continue
        if k not in tex:
            errors.append(f"{r['name']}: key {k} not in terrain_texture.json")
        f = tdir / f"{k}.png"
        if not f.is_file():
            errors.append(f"{r['name']}: file {f.name} missing")
        else:
            sizes.add(Image.open(f).size)
for k, v in tex.items():
    if not (ROOT / "pack" / (v["textures"] + ".png")).is_file():
        errors.append(f"terrain_texture key {k} points to a missing file")
if len(sizes) != 1:
    errors.append(f"tile sizes differ: {sorted(sizes)}")

# compare the compiled table with the measured one, for every 47-tile set
src = (ROOT / "src" / "CtmCore.hpp").read_text()
block = src[src.index("kCtmTiles"):]
block = block[:block.index("}};")]
compiled = {(int(e), int(g)): int(t) for e, g, t in re.findall(r"\{\s*(\d+),\s*(\d+),\s*(\d+)\}", block)}
if len(compiled) != 47:
    errors.append(f"CtmCore.hpp table has {len(compiled)} entries, expected 47")

checked = 0
for r in rules:
    if r["method"] != "ctm":
        continue
    tmp = Path(tempfile.mkdtemp())
    try:
        for i, k in enumerate(r["tiles"][:47]):
            shutil.copy(tdir / f"{k}.png", tmp / f"{i}.png")
        measured = d.table_for(tmp)
        if measured != compiled:
            errors.append(f"{r['name']}: measured tile layout differs from CtmCore.hpp")
        checked += 1
    except SystemExit as e:
        errors.append(f"{r['name']}: {e}")
    finally:
        shutil.rmtree(tmp)

if errors:
    print("\n".join(errors[:30]))
    print(f"{len(errors)} problem(s)")
    sys.exit(1)
print(f"ok: {len(rules)} rules, {len(tex)} textures, {checked} tile sets match CtmCore.hpp")
