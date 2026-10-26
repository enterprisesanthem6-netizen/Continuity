// Host-side test of rules loading + face resolution against the real generated ctm_rules.json.
// Build: g++ -std=c++20 -Isrc -Ithird_party -DJSON_NOEXCEPTION tests/test_pipeline.cpp src/CtmRules.cpp
#include "CtmRules.hpp"

#include <cstdio>
#include <fstream>
#include <map>
#include <sstream>
#include <tuple>

using namespace ctm;

static int g_fail = 0, g_checks = 0;
#define CHECK(...)                                                                    \
    do {                                                                              \
        ++g_checks;                                                                   \
        if (!(__VA_ARGS__)) { ++g_fail; std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #__VA_ARGS__); } \
    } while (0)

struct World {
    std::map<std::tuple<int, int, int>, std::string> blocks;
    std::string origin; // block id being drawn
    void set(int x, int y, int z, const char *id) { blocks[{x, y, z}] = id; }
    const std::string *at(Int3 p) const {
        auto it = blocks.find({p.x, p.y, p.z});
        return it == blocks.end() ? nullptr : &it->second;
    }
};
static bool sameBlock(void *u, Int3 p) {
    auto *w = static_cast<World *>(u);
    auto *b = w->at(p);
    return b && *b == w->origin;
}

static std::string resolve(const RuleSet &rs, World &w, Int3 pos, Face f) {
    w.origin = *w.at(pos);
    FaceRequest rq{w.origin, pos, f, sameBlock, nullptr, &w};
    const std::string *k = resolveFace(rs, rq);
    return k ? *k : std::string("<vanilla>");
}

static bool expectEq(const std::string &got, const char *want, int line) {
    ++g_checks;
    if (got != want) { ++g_fail; std::printf("FAIL line %d: got '%s' want '%s'\n", line, got.c_str(), want); return false; }
    return true;
}
#define EXPECT(got, want) expectEq((got), (want), __LINE__)

int main(int argc, char **argv) {
    const char *path = argc > 1 ? argv[1] : "config/ctm_rules.json";
    std::ifstream in(path);
    std::stringstream ss; ss << in.rdbuf();
    RuleSet rs; std::string err; int skipped = -1;
    CHECK(loadRuleSet(ss.str(), rs, err, &skipped));
    CHECK(skipped == 0);
    CHECK(rs.rules.size() == 42);
    CHECK(rs.find("minecraft:glass") != nullptr);
    CHECK(rs.find("minecraft:dirt") == nullptr);

    // glass: alone, then a 3x3 wall
    {
        World w; w.set(0, 0, 0, "minecraft:glass");
        EXPECT(resolve(rs, w, {0, 0, 0}, Face::North), "ctm_glass_standard_0");
        World wall;
        for (int x = 0; x < 3; ++x) for (int y = 0; y < 3; ++y) wall.set(x, y, 0, "minecraft:glass");
        EXPECT(resolve(rs, wall, {1, 1, 0}, Face::North), "ctm_glass_standard_26");
        EXPECT(resolve(rs, wall, {0, 0, 0}, Face::North), "ctm_glass_standard_39");
        EXPECT(resolve(rs, wall, {2, 2, 0}, Face::North), "ctm_glass_standard_13");
    }
    // stained glass uses its own tile set; different colours do not connect
    {
        World w; w.set(0, 0, 0, "minecraft:yellow_stained_glass"); w.set(1, 0, 0, "minecraft:yellow_stained_glass");
        EXPECT(resolve(rs, w, {0, 0, 0}, Face::North), "ctm_glass_yellow_3");
        w.set(1, 0, 0, "minecraft:red_stained_glass");
        EXPECT(resolve(rs, w, {0, 0, 0}, Face::North), "ctm_glass_yellow_0");
    }
    // bookshelf: horizontal, sides only
    {
        World w; for (int x = 0; x < 3; ++x) w.set(x, 0, 0, "minecraft:bookshelf");
        EXPECT(resolve(rs, w, {2, 0, 0}, Face::North), "ctm_bookshelf_0"); // viewer's left end
        EXPECT(resolve(rs, w, {1, 0, 0}, Face::North), "ctm_bookshelf_1");
        EXPECT(resolve(rs, w, {0, 0, 0}, Face::North), "ctm_bookshelf_2");
        EXPECT(resolve(rs, w, {1, 0, 0}, Face::Up), "<vanilla>");         // faces=sides
        World one; one.set(0, 0, 0, "minecraft:bookshelf");
        EXPECT(resolve(rs, one, {0, 0, 0}, Face::East), "ctm_bookshelf_3");
    }
    // sandstone: "top" method, tile only when the block above connects
    {
        World w; w.set(0, 0, 0, "minecraft:sandstone"); w.set(0, 1, 0, "minecraft:sandstone");
        EXPECT(resolve(rs, w, {0, 0, 0}, Face::North), "ctm_sandstone_sandstone_0");
        EXPECT(resolve(rs, w, {0, 1, 0}, Face::North), "<vanilla>");
        EXPECT(resolve(rs, w, {0, 0, 0}, Face::Up), "<vanilla>");
    }

    // robustness: garbage in must not crash and must be rejected
    {
        RuleSet x; std::string e;
        CHECK(!loadRuleSet("", x, e));
        CHECK(!loadRuleSet("{", x, e));
        CHECK(!loadRuleSet("[]", x, e));
        CHECK(!loadRuleSet(R"({"format":"1","rules":[]})", x, e));
        CHECK(!loadRuleSet(R"({"format":2,"rules":[]})", x, e));
        CHECK(!loadRuleSet(R"({"format":1})", x, e));
        CHECK(!loadRuleSet(R"({"format":1,"rules":[{"method":5,"tiles":1,"blocks":2}]})", x, e));
        int sk = 0;
        // one good, one bad (too few tiles), one with unknown method
        CHECK(loadRuleSet(R"({"format":1,"orientation":{"topUp":"south"},"rules":[
            {"method":"fixed","blocks":["minecraft:stone"],"tiles":["a"]},
            {"method":"ctm","blocks":["minecraft:dirt"],"tiles":["a","b"]},
            {"method":"nope","blocks":["minecraft:x"],"tiles":["a"]}]})", x, e, &sk));
        CHECK(sk == 2);
        CHECK(x.rules.size() == 1);
        CHECK((x.orientation.topUp == Int3{0, 0, 1}));
        World w; w.set(0, 0, 0, "minecraft:stone");
        EXPECT(resolve(x, w, {0, 0, 0}, Face::North), "a");
    }

    std::printf("%d checks, %d failed\n", g_checks, g_fail);
    return g_fail ? 1 : 0;
}
