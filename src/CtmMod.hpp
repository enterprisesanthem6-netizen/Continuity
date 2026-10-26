#pragma once

#include <memory>
#include <string>

#include <pl/Mod.hpp>

#include "GameBridge.hpp"

namespace ctm {

class CtmMod {
public:
    static CtmMod &instance();

    bool load(ll::mod::ModContext &context);
    bool enable(ll::mod::ModContext &context);
    bool disable(ll::mod::ModContext &context);
    bool unload(ll::mod::ModContext &context);

private:
    std::shared_ptr<const RuleSet> rules_;
    game::Resolver resolver_;
    bool enabledInConfig_ = true;
    bool bound_ = false;
};

} // namespace ctm
