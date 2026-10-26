// Host test of the game-independent resolver using a fake backend.
// Build: g++ -std=c++20 -Isrc -Ithird_party -DJSON_NOEXCEPTION tests/test_bridge.cpp src/CtmRules.cpp src/GameBridge.cpp
#include "GameBridge.hpp"

#include <cstdio>
#include <fstream>
#include <map>
#include <sstream>
#include <thread>
#include <tuple>

using namespace ctm;
using namespace ctm::game;

static int g_fail = 0, g_checks = 0;
#define CHECK(...)                                                                    \
    do {                                                                              \
        ++g_checks;                                                                   \
        if (!(__VA_ARGS__)) { ++g_fail; std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #__VA_ARGS__); } \
    } while (0)

static std::map<std::tuple<int, int, int>, std::string> g_world;
static std::map<std::string, int> g_atlas; // key -> fake UV object
static int g_uvLookups = 0;

static std::string_view blockIdAt(const void *, Int3 p) {
    auto it = g_world.find({p.x, p.y, p.z});
    return it == g_world.end() ? std::string_view() : std::string_view(it->second);
}
static uint64_t stateAt(const void *, Int3 p) { return uint64_t(p.y); } // fake: state = height
static const void *uvForKey(const std::string &k) {
    ++g_uvLookups;
    auto it = g_atlas.find(k);
    return it == g_atlas.end() ? nullptr : &it->second;
}

int main(int argc, char **argv) {
    std::ifstream in(argc > 1 ? argv[1] : "config/ctm_rules.json");
    std::stringstream ss; ss << in.rdbuf();
    auto rs = std::make_shared<RuleSet>();
    std::string err;
    CHECK(loadRuleSet(ss.str(), *rs, err));
    for (const auto &r : rs->rules) for (const auto &t : r.tiles) if (!t.empty()) g_atlas[t] = int(g_atlas.size());

    Resolver res;
    res.setRules(rs);
    CHECK(res.uvFor(Face::North) == nullptr); // no backend, nothing active

    Backend be; be.blockIdAt = blockIdAt; be.uvForKey = uvForKey;
    res.setBackend(be);
    CHECK(res.uvFor(Face::North) == nullptr); // no block begun yet

    // three glass blocks in a row, middle one
    for (int x = 0; x < 3; ++x) g_world[{x, 0, 0}] = "minecraft:glass";
    res.begin(nullptr, {1, 0, 0}, "minecraft:glass");
    const void *a = res.uvFor(Face::North);
    CHECK(a == &g_atlas["ctm_glass_standard_2"]);  // L|R connected
    const int lookups = g_uvLookups;
    CHECK(res.uvFor(Face::North) == a);
    CHECK(g_uvLookups == lookups);                 // second call is served from the per-thread cache
    res.end();
    CHECK(res.uvFor(Face::North) == nullptr);      // outside a block: untouched

    // a block that no rule knows
    g_world[{5, 5, 5}] = "minecraft:dirt";
    res.begin(nullptr, {5, 5, 5}, "minecraft:dirt");
    CHECK(res.uvFor(Face::North) == nullptr);
    res.end();

    // tile missing from the atlas: fall back to vanilla instead of crashing
    g_atlas.erase("ctm_glass_standard_2");
    res.setRules(rs); // bumps the generation, drops the cache
    res.begin(nullptr, {1, 0, 0}, "minecraft:glass");
    CHECK(res.uvFor(Face::North) == nullptr);
    res.end();

    // connect=state falls back to block identity when the backend has no state reader
    // (covered by sameState()); also exercise it with a state reader present
    be.blockStateAt = stateAt; res.setBackend(be);
    g_atlas["ctm_glass_standard_2"] = 99;
    res.setRules(rs);
    res.begin(nullptr, {1, 0, 0}, "minecraft:glass");
    CHECK(res.uvFor(Face::North) == &g_atlas["ctm_glass_standard_2"]);
    res.end();

    // thread isolation: another thread's current block must not leak into this one
    res.begin(nullptr, {1, 0, 0}, "minecraft:glass");
    bool otherSawNothing = false;
    std::thread t([&] { otherSawNothing = (res.uvFor(Face::North) == nullptr); });
    t.join();
    CHECK(otherSawNothing);
    CHECK(res.uvFor(Face::North) != nullptr);
    res.end();

    // bindGame must refuse cleanly while signatures are empty
    std::string why;
    CHECK(!bindGame(res, why));
    CHECK(!why.empty());

    std::printf("%d checks, %d failed\n", g_checks, g_fail);
    return g_fail ? 1 : 0;
}
