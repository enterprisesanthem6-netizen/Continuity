/**
 * @file main.cpp
 * @brief Continuity BE: connected textures for Minecraft Bedrock (LeviLauncher native mod).
 */

#include <pl/Mod.hpp>

#include "CtmMod.hpp"

namespace {

class ContinuityBeMod {
public:
    static ContinuityBeMod &instance() {
        static ContinuityBeMod mod;
        return mod;
    }
    bool load(ll::mod::ModContext &c) { return ctm::CtmMod::instance().load(c); }
    bool enable(ll::mod::ModContext &c) { return ctm::CtmMod::instance().enable(c); }
    bool disable(ll::mod::ModContext &c) { return ctm::CtmMod::instance().disable(c); }
    bool unload(ll::mod::ModContext &c) { return ctm::CtmMod::instance().unload(c); }
};

} // namespace

PL_REGISTER_MOD(ContinuityBeMod, ContinuityBeMod::instance())
