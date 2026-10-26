#include "CtmMod.hpp"

#include <fstream>
#include <sstream>

#include <nlohmann/json.hpp>

namespace ctm {

namespace {

bool readFile(const std::filesystem::path &p, std::string &out) {
    std::ifstream in(p, std::ios::binary);
    if (!in) return false;
    std::stringstream ss;
    ss << in.rdbuf();
    out = ss.str();
    return true;
}

} // namespace

CtmMod &CtmMod::instance() {
    static CtmMod mod;
    return mod;
}

bool CtmMod::load(ll::mod::ModContext &context) {
    auto &log = context.logger();

    // config/ctm_config.json: {"enabled": true}. Missing or broken file = defaults.
    std::string text;
    if (readFile(context.configDir() / "ctm_config.json", text)) {
        auto doc = nlohmann::json::parse(text, nullptr, /*allow_exceptions=*/false);
        if (doc.is_object()) {
            auto it = doc.find("enabled");
            if (it != doc.end() && it->is_boolean()) enabledInConfig_ = it->get<bool>();
        } else {
            log.warn("ctm_config.json is not valid JSON, using defaults");
        }
    }

    // config/ctm_rules.json is produced by tools/convert_continuity.py.
    auto rs = std::make_shared<RuleSet>();
    std::string err;
    int skipped = 0;
    if (!readFile(context.configDir() / "ctm_rules.json", text)) {
        log.error("config/ctm_rules.json not found; the mod stays inactive");
        return true; // never fail the launcher over missing data
    }
    if (!loadRuleSet(text, *rs, err, &skipped)) {
        log.error("ctm_rules.json rejected: {}", err);
        return true;
    }
    log.info("loaded {} connected-texture rules ({} skipped), {} block ids", rs->rules.size(), skipped,
             rs->byBlock.size());
    rules_ = std::move(rs);
    resolver_.setRules(rules_);
    return true;
}

bool CtmMod::enable(ll::mod::ModContext &context) {
    auto &log = context.logger();
    if (!rules_) return true;
    if (!enabledInConfig_) {
        log.info("disabled in ctm_config.json");
        return true;
    }
    std::string why;
    bound_ = game::bindGame(resolver_, why);
    if (bound_) log.info("connected textures active");
    else log.warn("inactive: {}", why);
    return true;
}

bool CtmMod::disable(ll::mod::ModContext &) {
    if (bound_) game::unbindGame();
    bound_ = false;
    return true;
}

bool CtmMod::unload(ll::mod::ModContext &) {
    rules_.reset();
    return true;
}

} // namespace ctm
