#pragma once

/**
 * @file CtmRules.hpp
 * @brief Rule set loaded from config/ctm_rules.json + the face -> texture pipeline.
 *
 * No game types and no Android headers: everything here is unit-tested on the
 * host (tests/test_pipeline.cpp). The game-specific part (reading blocks from
 * the world, mapping a texture key to a UV set) sits behind GameBridge.hpp.
 */

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "CtmCore.hpp"

namespace ctm {

enum class Connect : uint8_t { Block, Tile, State };

struct Rule {
    std::string name;
    Method method = Method::Ctm;
    uint8_t faces = kFacesAll;
    bool innerSeams = false;
    Connect connect = Connect::Block;
    std::vector<std::string> tiles; // texture keys; empty string = "keep vanilla"
};

struct RuleSet {
    Orientation orientation;
    std::vector<Rule> rules;
    // block id ("minecraft:glass") -> rule indices, in priority order
    std::unordered_map<std::string, std::vector<uint32_t>> byBlock;

    const std::vector<uint32_t> *find(std::string_view blockId) const {
        auto it = byBlock.find(std::string(blockId));
        return it == byBlock.end() ? nullptr : &it->second;
    }
};

/// Parses ctm_rules.json. Returns false and fills `error` when the document is
/// unusable; individual bad rules are skipped and counted in `skipped`.
bool loadRuleSet(std::string_view jsonText, RuleSet &out, std::string &error, int *skipped = nullptr);

/// What the game side tells the pipeline about the face being drawn.
struct FaceRequest {
    std::string_view blockId; // e.g. "minecraft:glass"
    Int3 pos;
    Face face;
    ConnectFn sameBlock; // neighbour is the same block as the one being drawn
    ConnectFn sameState; // ... and has the same block state (may be null: falls back to sameBlock)
    void *user;
};

/// Returns the texture key to draw this face with, or nullptr to keep the
/// vanilla texture. Rules are tried in priority order; the first that applies wins.
const std::string *resolveFace(const RuleSet &rs, const FaceRequest &req);

} // namespace ctm
