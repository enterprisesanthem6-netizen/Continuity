#!/usr/bin/env python3
"""Convert an OptiFine/Continuity CTM resource pack into what the Bedrock mod needs.

Input  : a Continuity .jar, any resource pack .zip, or an unpacked pack folder.
         Everything under  assets/*/optifine/ctm/  (or mcpatcher/ctm/) is read.
Output : <out>/pack/                      a Bedrock resource pack with the tile PNGs
                                          and a terrain_texture.json that registers them
         <out>/config/ctm_rules.json      the rules the native mod loads at runtime

Supported methods: ctm, horizontal, vertical, horizontal+vertical,
vertical+horizontal, top, fixed.  repeat / random / overlay* are reported and
skipped.  Rules are keyed by block id only: matchTiles / state properties /
resourceCondition are ignored (reported), because Bedrock has no Java tile names.

usage: convert_continuity.py INPUT OUT_DIR [--block-map map.json]
"""
import argparse
import io
import json
import re
import sys
import uuid
import zipfile
from pathlib import Path, PurePosixPath

from PIL import Image

METHODS = {
    "ctm": "ctm", "glass": "ctm", "horizontal": "horizontal", "vertical": "vertical",
    "horizontal+vertical": "horizontal+vertical", "h+v": "horizontal+vertical",
    "vertical+horizontal": "vertical+horizontal", "v+h": "vertical+horizontal",
    "top": "top", "fixed": "fixed",
}
REQUIRED = {"ctm": 47, "horizontal": 4, "vertical": 4, "horizontal+vertical": 7,
            "vertical+horizontal": 7, "top": 1, "fixed": 1}
FACE_NAMES = {"bottom": "down", "top": "up", "north": "north", "south": "south",
              "east": "east", "west": "west"}
ALL_FACES = ["down", "up", "north", "south", "west", "east"]
SIDE_FACES = ["north", "south", "west", "east"]
CTM_DIR = re.compile(r"(?:^|/)assets/[^/]+/(?:optifine|mcpatcher)/ctm/(.+)$")


class Source:
    """Read-only view over a zip/jar or a folder."""

    def __init__(self, path):
        self.path = Path(path)
        if self.path.is_dir():
            self.files = {p.relative_to(self.path).as_posix(): p for p in self.path.rglob("*") if p.is_file()}
            self._zip = None
        else:
            self._zip = zipfile.ZipFile(self.path)
            self.files = {n: n for n in self._zip.namelist() if not n.endswith("/")}

    def read(self, name):
        if self._zip:
            return self._zip.read(name)
        return self.files[name].read_bytes()


def parse_properties(text):
    props = {}
    for raw in text.splitlines():
        line = raw.strip()
        if not line or line[0] in "#!":
            continue
        m = re.match(r"([^=:\s]+)\s*[=:]\s*(.*)$", line)
        if m:
            props[m.group(1)] = m.group(2).strip()
    return props


def expand_tiles(value, base_dir, warn):
    """Returns a list whose items are a PNG path (posix, inside the source) or None (skip/default)."""
    out = []
    for tok in value.split():
        if tok in ("<skip>", "<default>"):
            out.append(None)
        elif re.fullmatch(r"\d+-\d+", tok):
            a, b = map(int, tok.split("-"))
            out.extend(f"{base_dir}/{i}.png" for i in range(a, b + 1))
        elif re.fullmatch(r"\d+", tok):
            out.append(f"{base_dir}/{tok}.png")
        elif ":" in tok or tok.startswith("assets/") or tok.startswith("/"):
            warn(f"absolute tile reference '{tok}' not supported")
            return None
        else:
            name = tok[2:] if tok.startswith("./") else tok
            name = name if name.endswith(".png") else name + ".png"
            out.append(str(PurePosixPath(base_dir) / name))
    return out


def parse_faces(value):
    if value is None:
        return list(ALL_FACES)
    faces = []
    for tok in re.split(r"[,\s]+", value.strip()):
        if tok == "all":
            faces += ALL_FACES
        elif tok == "sides":
            faces += SIDE_FACES
        elif tok in FACE_NAMES:
            faces.append(FACE_NAMES[tok])
    return [f for f in ALL_FACES if f in faces]


def java_block_to_bedrock(tok, block_map, warn):
    if ":" in tok and not tok.startswith(("minecraft:",)) and "=" not in tok:
        ns, name = tok.split(":", 1)
    elif tok.startswith("minecraft:"):
        ns, name = "minecraft", tok[len("minecraft:"):]
    else:
        ns, name = "minecraft", tok
    if "=" in name or ":" in name:
        warn(f"block '{tok}' has state properties; the state filter is dropped")
        name = name.split(":")[0]
    full = f"{ns}:{name}"
    return block_map.get(full, full)


def tile_key(png_path, ctm_root_rel):
    rel = PurePosixPath(png_path).relative_to(ctm_root_rel)
    parts = list(rel.with_suffix("").parts)
    if len(parts) > 1 and parts[0] in ("programmer_art", "default"):
        parts = parts[1:]
    return "ctm_" + re.sub(r"[^a-z0-9_]", "_", "_".join(parts).lower())


def convert(src_path, out_dir, block_map, variant="default"):
    src = Source(src_path)
    report = {"rules": 0, "skipped": [], "notes": [], "textures": 0}
    rules, used_png = [], {}

    props_files = []
    for name in src.files:
        m = CTM_DIR.search(name)
        if m and name.endswith(".properties"):
            props_files.append((name, name[: len(name) - len(m.group(1))].rstrip("/")))
    props_files.sort()  # OptiFine: processed alphabetically by path
    # Continuity ships one rule set per texture style (default / programmer_art).
    # Bedrock uses the modern default look, so keep only the requested variant
    # when the pack has such a folder; plain OptiFine packs have none and are kept whole.
    def first_dir(entry):
        rel = PurePosixPath(entry[0]).relative_to(entry[1]).parts
        return rel[0] if len(rel) > 1 else None
    if variant and any(first_dir(e) == variant for e in props_files):
        props_files = [e for e in props_files if first_dir(e) == variant]
        report["notes"].append(f"all: using variant '{variant}'")

    for name, ctm_root in props_files:
        label = PurePosixPath(name).relative_to(ctm_root).as_posix()
        warns = []
        warn = warns.append
        p = parse_properties(src.read(name).decode("utf-8", "replace"))
        method = METHODS.get(p.get("method", "ctm").lower())
        if method is None:
            report["skipped"].append(f"{label}: method '{p.get('method')}' not supported")
            continue
        blocks_value = p.get("matchBlocks") or p.get("matchBlock")
        if not blocks_value:
            m = re.match(r"block_(.+)\.properties$", PurePosixPath(name).name)
            blocks_value = m.group(1) if m else None
        if not blocks_value and p.get("matchTiles") and len(p["matchTiles"].split()) == 1:
            # tile-only rule: assume the block is named like its tile (true for e.g. sandstone)
            blocks_value = p["matchTiles"].split()[0]
            warn(f"block inferred from tile name '{blocks_value}'")
        if not blocks_value:
            report["skipped"].append(f"{label}: no matchBlocks (tile-only rules cannot be mapped to Bedrock blocks)")
            continue
        base_dir = str(PurePosixPath(name).parent)
        tiles_value = p.get("tiles")
        if tiles_value is None:
            report["skipped"].append(f"{label}: no tiles")
            continue
        pngs = expand_tiles(tiles_value, base_dir, warn)
        if pngs is None or len(pngs) < REQUIRED[method]:
            report["skipped"].append(f"{label}: needs {REQUIRED[method]} tiles, got {0 if pngs is None else len(pngs)}")
            continue
        missing = [t for t in pngs if t and t not in src.files]
        if missing:
            report["skipped"].append(f"{label}: missing tile image {missing[0]}")
            continue
        keys = []
        for t in pngs:
            if t is None:
                keys.append(None)
                continue
            k = tile_key(t, ctm_root)
            used_png[k] = t
            keys.append(k)
        blocks = [java_block_to_bedrock(b, block_map, warn) for b in blocks_value.split()]
        connect = p.get("connect") or "block"
        if connect not in ("block", "tile", "state"):
            warn(f"unknown connect '{connect}', using block")
            connect = "block"
        if "matchTiles" in p and (p.get("matchBlocks") or p.get("matchBlock")):
            warn("matchTiles ignored (Bedrock has no Java tile names)")
        if "resourceCondition" in p:
            warn("resourceCondition ignored")
        rules.append({
            "name": label[: -len(".properties")],
            "method": method,
            "blocks": blocks,
            "faces": parse_faces(p.get("faces")),
            "connect": connect,
            "innerSeams": p.get("innerSeams", "false").lower() == "true",
            "tiles": keys[: REQUIRED[method]] if method != "ctm" else keys[:47],
        })
        for w in warns:
            report["notes"].append(f"{label}: {w}")

    out = Path(out_dir)
    pack = out / "pack"
    tex_dir = pack / "textures" / "blocks" / "continuity"
    tex_dir.mkdir(parents=True, exist_ok=True)
    (out / "config").mkdir(parents=True, exist_ok=True)

    texture_data = {}
    sizes = set()
    for k, png in sorted(used_png.items()):
        im = Image.open(io.BytesIO(src.read(png))).convert("RGBA")
        sizes.add(im.size)
        im.save(tex_dir / f"{k}.png")
        texture_data[k] = {"textures": f"textures/blocks/continuity/{k}"}
    report["textures"] = len(texture_data)
    if len(sizes) > 1:
        report["notes"].append(f"tile images have different sizes: {sorted(sizes)}")

    (pack / "textures" / "terrain_texture.json").write_text(json.dumps({
        "resource_pack_name": "continuity_be", "texture_name": "atlas.terrain",
        "padding": 8, "num_mip_levels": 4, "texture_data": texture_data}, indent=1))

    pack_uuid = str(uuid.uuid5(uuid.NAMESPACE_URL, "continuity-be/pack"))
    mod_uuid = str(uuid.uuid5(uuid.NAMESPACE_URL, "continuity-be/module"))
    (pack / "manifest.json").write_text(json.dumps({
        "format_version": 2,
        "header": {"name": "Continuity BE Tiles", "description": "Connected-texture tiles for the Continuity BE native mod",
                   "uuid": pack_uuid, "version": [1, 0, 0], "min_engine_version": [1, 21, 0]},
        "modules": [{"type": "resources", "uuid": mod_uuid, "version": [1, 0, 0]}],
    }, indent=2))
    icon = Path(__file__).resolve().parent.parent / "icon.png"
    if icon.is_file():
        (pack / "pack_icon.png").write_bytes(icon.read_bytes())

    rules_doc = {"format": 1, "orientation": {"topUp": "north", "bottomUp": "south"}, "rules": rules}
    (out / "config" / "ctm_rules.json").write_text(json.dumps(rules_doc, indent=1))
    report["rules"] = len(rules)
    return report


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("input")
    ap.add_argument("out_dir")
    ap.add_argument("--block-map", help="JSON {java_id: bedrock_id} for blocks whose ids differ")
    ap.add_argument("--variant", default="default", help="rule-set folder to use when the pack has several (default: default)")
    a = ap.parse_args()
    bm = json.loads(Path(a.block_map).read_text()) if a.block_map else {}
    r = convert(a.input, a.out_dir, bm, a.variant)
    print(f"rules converted : {r['rules']}")
    print(f"tile textures   : {r['textures']}")
    print(f"skipped rules   : {len(r['skipped'])}")
    for s in r["skipped"]:
        print("   -", s)
    if r["notes"]:
        kinds = {}
        for n in r["notes"]:
            kinds.setdefault(n.split(": ", 1)[1], 0)
            kinds[n.split(": ", 1)[1]] += 1
        print("notes:")
        for k, c in kinds.items():
            print(f"   - {k} (x{c})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
