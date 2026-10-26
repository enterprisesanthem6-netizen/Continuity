#!/usr/bin/env bash
# Host-side tests: engine, rule loading, resolver, shipped data. No Android toolchain needed.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."
OUT="${TMPDIR:-/tmp}/ctmbe-tests"
mkdir -p "$OUT"
CXX="${CXX:-g++}"
FLAGS=(-std=c++20 -Wall -Wextra -Isrc -Ithird_party -DJSON_NOEXCEPTION -pthread)

echo "== engine";   "$CXX" "${FLAGS[@]}" tests/test_ctm_core.cpp -o "$OUT/core"      && "$OUT/core"
echo "== pipeline"; "$CXX" "${FLAGS[@]}" tests/test_pipeline.cpp src/CtmRules.cpp -o "$OUT/pipe" && "$OUT/pipe" config/ctm_rules.json
echo "== bridge";   "$CXX" "${FLAGS[@]}" tests/test_bridge.cpp src/CtmRules.cpp src/GameBridge.cpp -o "$OUT/bridge" && "$OUT/bridge" config/ctm_rules.json
echo "== data";     python3 -I tests/test_pack.py
echo "== sdk syntax (host stubs)"
for f in src/CtmMod.cpp src/main.cpp; do
  "$CXX" -std=c++20 -fsyntax-only -Wall -Wextra -fno-rtti -fno-exceptions -DFMT_HEADER_ONLY -DJSON_NOEXCEPTION \
    -Itests/stubs -Iinclude -Isrc -Ithird_party -Ithird_party/fmt "$f"
done
echo "all tests passed"
