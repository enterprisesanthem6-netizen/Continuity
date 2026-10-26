#include "GameBridge.hpp"

#include "Signatures.hpp"

namespace ctm::game {

// ---------------------------------------------------------------------------
// Resolver (game independent)
// ---------------------------------------------------------------------------

Resolver::Current &Resolver::current() {
    static thread_local Current c;
    return c;
}

Resolver::Cache &Resolver::cache() {
    static thread_local Cache c;
    return c;
}

void Resolver::begin(const void *source, Int3 pos, std::string_view blockId) {
    Current &c = current();
    c.source = source;
    c.pos = pos;
    c.blockId.assign(blockId.data(), blockId.size());
    c.active = true;
}

void Resolver::end() { current().active = false; }

bool Resolver::sameBlock(void *self, Int3 p) {
    auto *r = static_cast<Resolver *>(self);
    const Current &c = current();
    return r->backend_.blockIdAt(c.source, p) == std::string_view(c.blockId);
}

bool Resolver::sameState(void *self, Int3 p) {
    auto *r = static_cast<Resolver *>(self);
    const Current &c = current();
    if (!sameBlock(self, p)) return false;
    if (!r->backend_.blockStateAt) return true;
    return r->backend_.blockStateAt(c.source, p) == r->backend_.blockStateAt(c.source, c.pos);
}

const void *Resolver::uvFor(Face face) {
    const Current &c = current();
    if (!c.active || !rules_ || !backend_.complete()) return nullptr;

    FaceRequest req{c.blockId, c.pos, face, &Resolver::sameBlock,
                    backend_.blockStateAt ? &Resolver::sameState : nullptr, this};
    const std::string *key = resolveFace(*rules_, req);
    if (!key) return nullptr;

    Cache &k = cache();
    const uint64_t gen = generation_.load(std::memory_order_acquire);
    if (k.generation != gen) {
        k.uv.clear();
        k.generation = gen;
    }
    auto it = k.uv.find(key);
    if (it == k.uv.end()) it = k.uv.emplace(key, backend_.uvForKey(*key)).first;
    return it->second; // nullptr when the atlas lacks the tile: vanilla texture stays
}

// ---------------------------------------------------------------------------
// Binding to libminecraftpe.so
// ---------------------------------------------------------------------------
//
// STATUS: not bound. The hooks below need addresses inside libminecraftpe.so
// for the exact build being run, and those can only be found by looking at that
// binary. docs/HOOKS.md lists the four things to locate. Until then bindGame()
// reports why it is inactive and the mod does nothing (it never touches the game).
//
// When the patterns exist, the work to do here is small and mechanical:
//   1. pl::memory::resolveSignature(kXxxPattern, "libminecraftpe.so") for each
//   2. pl::memory::HookHandle on the tessellation entry: detour calls
//      resolver.begin(blockSource, pos, blockId), then the original, then end()
//   3. pl::memory::HookHandle on the face-UV getter: detour calls
//      resolver.uvFor(face); if non-null return it, else call the original
//   4. fill Backend { blockIdAt, blockStateAt, uvForKey } and resolver.setBackend()

bool bindGame(Resolver &resolver, std::string &why) {
    (void)resolver;
    if (!kTessellateBlockPattern[0] || !kFaceUvPattern[0] || !kBlockIdAtPattern[0] || !kAtlasLookupPattern[0]) {
        why = "game hooks are not bound: signatures for libminecraftpe.so are not filled in (see docs/HOOKS.md)";
        return false;
    }
    why = "signatures are present but the binding code in GameBridge.cpp has not been written for them";
    return false;
}

void unbindGame() {}

} // namespace ctm::game
