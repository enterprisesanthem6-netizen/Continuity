#pragma once

/**
 * @file Signatures.hpp
 * @brief Byte signatures for libminecraftpe.so (arm64-v8a). EMPTY until derived.
 *
 * Format is the same as BetterZoom's (hex bytes, "?" wildcard, PC-relative
 * instructions wildcarded). Derive and verify with the tools you already have:
 *
 *     python3 tools/derive_pattern.py --lib OLD.so --addr 0xADDR --find-in NEW.so
 *     python3 tools/verify_signatures.py libminecraftpe.so
 *
 * What each target is, and how to recognise it, is in docs/HOOKS.md.
 */

namespace ctm {

// Per-block tessellation entry: called once for every block while a chunk mesh is built.
inline constexpr const char *kTessellateBlockPattern = "";

// Lookup of the TextureUVCoordinateSet used for one face of a block.
inline constexpr const char *kFaceUvPattern = "";

// BlockSource::getBlock(BlockPos) or an equivalent block read, plus how to get its name.
inline constexpr const char *kBlockIdAtPattern = "";

// Terrain atlas lookup: terrain_texture.json key -> TextureUVCoordinateSet.
inline constexpr const char *kAtlasLookupPattern = "";

} // namespace ctm
