#!/usr/bin/env python3
"""Derive the 47-tile CTM layout from real tile PNGs (no guessing).

A tile of the OptiFine "ctm" method is fully described by which of its four
edges are connected and, only when both edges next to a corner are connected,
whether that diagonal neighbour is connected too. That gives exactly
1 + 4 + 2 + 8 + 16 + 16 = 47 states, one per tile.

We read each tile image, measure the outer pixel rows (edge border present =>
edge NOT connected) and the four corner pixels (opaque => diagonal missing),
and check that the 47 tiles give 47 distinct, valid states. The result is the
state -> tile index table used by src/CtmCore.hpp.

usage: derive_ctm_table.py DIR_WITH_0..46.png [DIR2 ...]
All directories given must yield the identical table (consistency check).
"""
import sys
from pathlib import Path
from PIL import Image

U, R, D, L = 1, 2, 4, 8          # edge connected
UL, UR, DR, DL = 1, 2, 4, 8      # diagonal connected


def tile_state(path):
    im = Image.open(path).convert("RGBA")
    s = im.size[0]
    # Stained glass is translucent, so "opaque" is measured against the tile's
    # own interior (median alpha of the inner pixels), not against zero.
    inner = sorted(im.getpixel((x, y))[3] for x in range(2, s - 2) for y in range(2, s - 2))
    base = inner[len(inner) // 2]
    op = lambda x, y: im.getpixel((x, y))[3] > base + 20
    thr = max(3, (s - 2) // 3)
    top = sum(op(x, 0) for x in range(1, s - 1))
    bot = sum(op(x, s - 1) for x in range(1, s - 1))
    lef = sum(op(0, y) for y in range(1, s - 1))
    rig = sum(op(s - 1, y) for y in range(1, s - 1))
    edges = 0
    if top < thr: edges |= U
    if rig < thr: edges |= R
    if bot < thr: edges |= D
    if lef < thr: edges |= L
    notch = {UL: op(0, 0), UR: op(s - 1, 0), DR: op(s - 1, s - 1), DL: op(0, s - 1)}
    diag = 0
    # a diagonal only exists when both neighbouring edges are connected
    for bit, need in ((UL, U | L), (UR, U | R), (DR, D | R), (DL, D | L)):
        if edges & need == need and not notch[bit]:
            diag |= bit
    return edges, diag


def expected_states():
    out = set()
    for e in range(16):
        pairs = [(UL, U | L), (UR, U | R), (DR, D | R), (DL, D | L)]
        free = [b for b, need in pairs if e & need == need]
        for k in range(1 << len(free)):
            d = 0
            for i, b in enumerate(free):
                if k >> i & 1: d |= b
            out.add((e, d))
    return out


def table_for(directory):
    d = Path(directory)
    states = {}
    for i in range(47):
        st = tile_state(d / f"{i}.png")
        if st in states:
            raise SystemExit(f"{d}: tiles {states[st]} and {i} give the same state {st}")
        states[st] = i
    exp = expected_states()
    if set(states) != exp:
        raise SystemExit(f"{d}: states do not cover the 47 valid ones: "
                         f"missing={sorted(exp - set(states))} extra={sorted(set(states) - exp)}")
    return states


def main(argv):
    if not argv:
        raise SystemExit(__doc__)
    ref = None
    for a in argv:
        t = table_for(a)
        if ref is None:
            ref = t
        elif t != ref:
            raise SystemExit(f"{a}: layout differs from {argv[0]}")
    print(f"// {len(argv)} tile set(s) analysed, identical layout, 47 distinct valid states")
    print("// { edges(U=1,R=2,D=4,L=8), diagonals(UL=1,UR=2,DR=4,DL=8), tile }")
    by_tile = sorted((i, st) for st, i in ref.items())
    for i, (e, d) in by_tile:
        print(f"    {{{e:2d}, {d:2d}, {i:2d}}},")


if __name__ == "__main__":
    main(sys.argv[1:])
