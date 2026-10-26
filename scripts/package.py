#!/usr/bin/env python3
"""Packages the build:

  ContinuityBE.levipack   LeviLauncher mod (top-level folder = runtime mod id)
  ContinuityBE.mcpack     the Bedrock resource pack with the tile textures

usage: package.py --root . --lib build/libcontinuitybe.so --out dist
"""
import argparse
import json
import zipfile
from pathlib import Path


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=".")
    ap.add_argument("--lib", default="build/libcontinuitybe.so")
    ap.add_argument("--out", default="dist")
    a = ap.parse_args()
    root, out = Path(a.root).resolve(), Path(a.out)
    out.mkdir(parents=True, exist_ok=True)
    mod_id = "continuitybe"
    lib = Path(a.lib) if Path(a.lib).is_absolute() else root / a.lib

    manifest = root / "manifest.json"
    if json.loads(manifest.read_text()).get("entry") != lib.name:
        raise SystemExit("manifest entry does not match the library name")
    entries = {
        f"{mod_id}/manifest.json": manifest,
        f"{mod_id}/{lib.name}": lib,
        f"{mod_id}/icon.png": root / "icon.png",
        f"{mod_id}/config/ctm_rules.json": root / "config" / "ctm_rules.json",
        f"{mod_id}/config/ctm_config.json": root / "config" / "ctm_config.json",
    }
    for p in entries.values():
        if not p.is_file():
            raise SystemExit(f"missing file: {p}")
    levi = out / "ContinuityBE.levipack"
    with zipfile.ZipFile(levi, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for name, path in entries.items():
            z.write(path, name)

    pack = root / "pack"
    mc = out / "ContinuityBE.mcpack"
    files = sorted(p for p in pack.rglob("*") if p.is_file())
    if not any(p.name == "manifest.json" and p.parent == pack for p in files):
        raise SystemExit("pack/manifest.json missing")
    with zipfile.ZipFile(mc, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for p in files:
            z.write(p, p.relative_to(pack).as_posix())
    print(f"{levi} ({levi.stat().st_size} bytes)")
    print(f"{mc} ({mc.stat().st_size} bytes, {len(files)} files)")


if __name__ == "__main__":
    main()
