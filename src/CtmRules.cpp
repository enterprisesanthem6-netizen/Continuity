#include "CtmRules.hpp"

#include <nlohmann/json.hpp>

namespace ctm {

namespace {

bool parseMethod(const std::string &s, Method &m) {
    if (s == "ctm") m = Method::Ctm;
    else if (s == "horizontal") m = Method::Horizontal;
    else if (s == "vertical") m = Method::Vertical;
    else if (s == "horizontal+vertical") m = Method::HorizontalVertical;
    else if (s == "vertical+horizontal") m = Method::VerticalHorizontal;
    else if (s == "top") m = Method::Top;
    else if (s == "fixed") m = Method::Fixed;
    else return false;
    return true;
}

bool parseFace(const std::string &s, Face &f) {
    if (s == "down") f = Face::Down;
    else if (s == "up") f = Face::Up;
    else if (s == "north") f = Face::North;
    else if (s == "south") f = Face::South;
    else if (s == "west") f = Face::West;
    else if (s == "east") f = Face::East;
    else return false;
    return true;
}

bool parseDirection(const std::string &s, Int3 &v) {
    Face f;
    if (!parseFace(s, f)) return false;
    v = faceNormal(f);
    return true;
}

// The mod is built with JSON_NOEXCEPTION, where a type mismatch aborts the
// process, so every read goes through a type check first.
std::string getStr(const nlohmann::json &j, const char *key, std::string def = {}) {
    auto it = j.find(key);
    return (it != j.end() && it->is_string()) ? it->get<std::string>() : def;
}
bool getBool(const nlohmann::json &j, const char *key, bool def) {
    auto it = j.find(key);
    return (it != j.end() && it->is_boolean()) ? it->get<bool>() : def;
}

} // namespace

bool loadRuleSet(std::string_view jsonText, RuleSet &out, std::string &error, int *skipped) {
    using nlohmann::json;
    json doc = json::parse(jsonText.begin(), jsonText.end(), nullptr, /*allow_exceptions=*/false);
    if (doc.is_discarded() || !doc.is_object()) {
        error = "ctm_rules.json is not valid JSON";
        return false;
    }
    if (!doc.contains("format") || !doc["format"].is_number_integer() || doc["format"].get<int>() != 1) {
        error = "unsupported ctm_rules.json format (expected 1)";
        return false;
    }
    out = RuleSet{};
    int bad = 0;

    if (doc.contains("orientation") && doc["orientation"].is_object()) {
        const auto &o = doc["orientation"];
        Int3 v;
        if (parseDirection(getStr(o, "topUp"), v)) out.orientation.topUp = v;
        if (parseDirection(getStr(o, "bottomUp"), v)) out.orientation.bottomUp = v;
    }

    if (!doc.contains("rules") || !doc["rules"].is_array()) {
        error = "ctm_rules.json has no rules array";
        return false;
    }
    for (const auto &j : doc["rules"]) {
        Rule r;
        if (!j.is_object() || !parseMethod(getStr(j, "method"), r.method)) { ++bad; continue; }
        r.name = getStr(j, "name");
        r.innerSeams = getBool(j, "innerSeams", false);
        const std::string conn = getStr(j, "connect", "block");
        r.connect = conn == "state" ? Connect::State : conn == "tile" ? Connect::Tile : Connect::Block;

        r.faces = 0;
        if (j.contains("faces") && j["faces"].is_array()) {
            for (const auto &f : j["faces"]) {
                Face face;
                if (f.is_string() && parseFace(f.get<std::string>(), face)) r.faces |= faceBit(face);
            }
        } else {
            r.faces = kFacesAll;
        }

        if (!j.contains("tiles") || !j["tiles"].is_array()) { ++bad; continue; }
        for (const auto &t : j["tiles"]) r.tiles.push_back(t.is_string() ? t.get<std::string>() : std::string());
        if (int(r.tiles.size()) < requiredTiles(r.method) || r.faces == 0) { ++bad; continue; }

        if (!j.contains("blocks") || !j["blocks"].is_array() || j["blocks"].empty()) { ++bad; continue; }
        const uint32_t idx = uint32_t(out.rules.size());
        out.rules.push_back(std::move(r));
        for (const auto &b : j["blocks"])
            if (b.is_string()) out.byBlock[b.get<std::string>()].push_back(idx);
    }
    if (skipped) *skipped = bad;
    if (out.rules.empty()) {
        error = "no usable rules";
        return false;
    }
    return true;
}

const std::string *resolveFace(const RuleSet &rs, const FaceRequest &req) {
    const auto *list = rs.find(req.blockId);
    if (!list) return nullptr;
    for (uint32_t idx : *list) {
        const Rule &rule = rs.rules[idx];
        if (!(rule.faces & faceBit(req.face))) continue;
        const ConnectFn fn = (rule.connect == Connect::State && req.sameState) ? req.sameState : req.sameBlock;
        Query q{req.pos, req.face, fn, req.user, rule.innerSeams, rs.orientation};
        const int tile = selectTile(rule.method, q, int(rule.tiles.size()));
        if (tile < 0 || tile >= int(rule.tiles.size())) continue;
        if (rule.tiles[tile].empty()) continue; // <skip>: vanilla texture
        return &rule.tiles[tile];
    }
    return nullptr;
}

} // namespace ctm
