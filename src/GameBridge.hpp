#pragma once

/**
 * @file GameBridge.hpp
 * @brief The seam between the CTM logic and Minecraft.
 *
 * Everything the game has to provide is the small `Backend` below. The
 * resolver around it (per-thread "current block", rule lookup, UV cache) is
 * game-independent and unit-tested on the host with a fake backend
 * (tests/test_bridge.cpp). Binding `Backend` to libminecraftpe.so is the only
 * build-specific work left; see docs/HOOKS.md and src/GameBridge.cpp.
 */

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

#include "CtmRules.hpp"

namespace ctm::game {

struct Backend {
    /// Block id at `pos` ("minecraft:glass"), read through the BlockSource the
    /// tessellation hook captured. Empty view = unknown / air / not loaded.
    std::string_view (*blockIdAt)(const void *source, Int3 pos) = nullptr;
    /// Hash of the full block state at `pos` (for connect=state). May be null.
    uint64_t (*blockStateAt)(const void *source, Int3 pos) = nullptr;
    /// Terrain-atlas UV set registered for a terrain_texture.json key
    /// ("ctm_glass_standard_26"), or nullptr if the atlas does not have it.
    const void *(*uvForKey)(const std::string &key) = nullptr;

    bool complete() const { return blockIdAt && uvForKey; }
};

class Resolver {
public:
    void setBackend(const Backend &b) { backend_ = b; }
    void setRules(std::shared_ptr<const RuleSet> rules) {
        rules_ = std::move(rules);
        generation_.fetch_add(1, std::memory_order_release);
    }

    /// Tessellation hook: a block is about to be drawn. Per-thread state.
    void begin(const void *source, Int3 pos, std::string_view blockId);
    void end();

    /// Face-texture hook: returns the replacement UV set for `face` of the
    /// current block, or nullptr to keep the original.
    const void *uvFor(Face face);

private:
    struct Current {
        const void *source = nullptr;
        Int3 pos;
        std::string blockId;
        bool active = false;
    };
    struct Cache {
        uint64_t generation = ~0ull;
        std::unordered_map<const std::string *, const void *> uv;
    };

    static bool sameBlock(void *self, Int3 p);
    static bool sameState(void *self, Int3 p);
    static Current &current();
    static Cache &cache();

    Backend backend_;
    std::shared_ptr<const RuleSet> rules_;
    std::atomic<uint64_t> generation_{0};
};

/// Resolves libminecraftpe.so symbols for the running build and installs the
/// hooks that feed the Resolver. Returns false (and a reason) when the build is
/// not supported; the mod then stays loaded but inactive.
bool bindGame(Resolver &resolver, std::string &why);
void unbindGame();

} // namespace ctm::game
