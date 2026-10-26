// Host-side tests for the CTM engine. Build: g++ -std=c++20 -Isrc tests/test_ctm_core.cpp
#include "CtmCore.hpp"

#include <cstdio>
#include <set>
#include <tuple>

using namespace ctm;

static int g_fail = 0, g_checks = 0;
#define CHECK(...)                                                                    \
    do {                                                                              \
        ++g_checks;                                                                   \
        if (!(__VA_ARGS__)) { ++g_fail; std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #__VA_ARGS__); } \
    } while (0)
#define CHECK_EQ(a, b)                                                                \
    do {                                                                              \
        ++g_checks;                                                                   \
        const long _a = (a), _b = (b);                                                \
        if (_a != _b) { ++g_fail; std::printf("FAIL %s:%d  %s == %s  (%ld vs %ld)\n", __FILE__, __LINE__, #a, #b, _a, _b); } \
    } while (0)

struct World {
    std::set<std::tuple<int, int, int>> blocks;
    void add(int x, int y, int z) { blocks.insert({x, y, z}); }
    bool has(Int3 p) const { return blocks.count({p.x, p.y, p.z}) != 0; }
};
static bool connectFn(void *u, Int3 p) { return static_cast<World *>(u)->has(p); }
static int pick(Method m, World &w, Int3 pos, Face f, bool seams = false, int tiles = 47) {
    Query q{pos, f, connectFn, &w, seams, {}};
    return selectTile(m, q, tiles);
}

int main() {
    // --- table sanity -----------------------------------------------------
    {
        bool seen[47] = {};
        for (const auto &s : kCtmTiles) {
            CHECK(s.tile < 47);
            CHECK(!seen[s.tile]);
            seen[s.tile] = true;
        }
        for (int m = 0; m < 256; ++m) CHECK(kCtmLookup[m] != 0xFF); // every mask resolves
        // 47 distinct tiles are reachable from the 256 masks
        bool reach[47] = {};
        for (int m = 0; m < 256; ++m) reach[kCtmLookup[m]] = true;
        for (int i = 0; i < 47; ++i) CHECK(reach[i]);
        CHECK_EQ(kCtmLookup[0], 0);          // isolated
        CHECK_EQ(kCtmLookup[0xFF], 26);      // fully surrounded
        CHECK_EQ(kCtmLookup[0x55], 46);      // 4 edges, no diagonals
        // a diagonal that has no edge pair behind it must not change the tile
        CHECK_EQ(kCtmLookup[0x01 | 0x02], kCtmLookup[0x01]); // U + UR(only) == U
    }

    // --- face frames: right-handed, viewed from outside -------------------
    {
        
        const auto fn = frameFor(Face::North), fs = frameFor(Face::South), fe = frameFor(Face::East),
                   fw = frameFor(Face::West), ft = frameFor(Face::Up), fd = frameFor(Face::Down);
        CHECK(fn.right == Int3{-1, 0, 0});  // looking south, west is on the right
        CHECK(fs.right == Int3{1, 0, 0});   // looking north, east is on the right
        CHECK(fe.right == Int3{0, 0, -1});  // looking west, north is on the right
        CHECK(fw.right == Int3{0, 0, 1});   // looking east, south is on the right
        CHECK(ft.right == Int3{1, 0, 0});   // looking down, north up: east on the right
        CHECK(fd.right == Int3{1, 0, 0});
    }

    // --- isolated block: tile 0 on every face -----------------------------
    {
        World w; w.add(0, 0, 0);
        for (int f = 0; f < 6; ++f) CHECK_EQ(pick(Method::Ctm, w, {0, 0, 0}, Face(f)), 0);
    }

    // --- 3x3 wall facing north (plane z=0) --------------------------------
    {
        World w;
        for (int x = 0; x < 3; ++x) for (int y = 0; y < 3; ++y) w.add(x, y, 0);
        CHECK_EQ(pick(Method::Ctm, w, {1, 1, 0}, Face::North), 26);   // centre
        // viewer on the north side: left = +X, right = -X
        // block x=0 (west-most) has its neighbour on the viewer's LEFT.
        // bottom row, west-most: neighbours U and L(+X) connected, diagonal UL too -> edges U|L = 9, diag UL
        CHECK_EQ(pick(Method::Ctm, w, {0, 0, 0}, Face::North), 39);   // {9,1}
        // top row, east-most (x=2): neighbours D and R(-X); diagonal DR
        CHECK_EQ(pick(Method::Ctm, w, {2, 2, 0}, Face::North), 13);   // {6,4}
        // a plain line of three along X, north face
        World line; for (int x = 0; x < 3; ++x) line.add(x, 0, 0);
        CHECK_EQ(pick(Method::Ctm, line, {0, 0, 0}, Face::North), 3);   // edges L only -> {8,0}
        CHECK_EQ(pick(Method::Ctm, line, {1, 0, 0}, Face::North), 2);   // L|R   -> {10,0}
        CHECK_EQ(pick(Method::Ctm, line, {2, 0, 0}, Face::North), 1);   // R only -> {2,0}
        // seen from the south the left/right swap
        CHECK_EQ(pick(Method::Ctm, line, {0, 0, 0}, Face::South), 1);
        CHECK_EQ(pick(Method::Ctm, line, {2, 0, 0}, Face::South), 3);
    }

    // --- horizontal / vertical --------------------------------------------
    {
        World line; for (int x = 0; x < 3; ++x) line.add(x, 0, 0);
        // viewer's left end = east-most block (x=2): connects to its right only -> tile 0
        CHECK_EQ(pick(Method::Horizontal, line, {2, 0, 0}, Face::North, false, 4), 0);
        CHECK_EQ(pick(Method::Horizontal, line, {1, 0, 0}, Face::North, false, 4), 1);
        CHECK_EQ(pick(Method::Horizontal, line, {0, 0, 0}, Face::North, false, 4), 2);
        World one; one.add(5, 5, 5);
        CHECK_EQ(pick(Method::Horizontal, one, {5, 5, 5}, Face::East, false, 4), 3);

        World col; for (int y = 0; y < 3; ++y) col.add(0, y, 0);
        CHECK_EQ(pick(Method::Vertical, col, {0, 0, 0}, Face::North, false, 4), 0); // bottom end: up only
        CHECK_EQ(pick(Method::Vertical, col, {0, 1, 0}, Face::North, false, 4), 1);
        CHECK_EQ(pick(Method::Vertical, col, {0, 2, 0}, Face::North, false, 4), 2); // top end: down only
        CHECK_EQ(pick(Method::Vertical, one, {5, 5, 5}, Face::North, false, 4), 3);
    }

    // --- horizontal+vertical / vertical+horizontal ------------------------
    {
        World col; for (int y = 0; y < 3; ++y) col.add(0, y, 0);
        CHECK_EQ(pick(Method::HorizontalVertical, col, {0, 0, 0}, Face::North, false, 7), 4);
        CHECK_EQ(pick(Method::HorizontalVertical, col, {0, 1, 0}, Face::North, false, 7), 5);
        CHECK_EQ(pick(Method::HorizontalVertical, col, {0, 2, 0}, Face::North, false, 7), 6);
        // L shape: (0,0) (1,0) (0,1); block (0,1) sits on (0,0) whose right neighbour exists ->
        // horizontal row wins, vertical connection is ignored
        World L; L.add(0, 0, 0); L.add(1, 0, 0); L.add(0, 1, 0);
        CHECK_EQ(pick(Method::HorizontalVertical, L, {0, 1, 0}, Face::North, false, 7), 3);
        // horizontal neighbours take priority
        CHECK_EQ(pick(Method::HorizontalVertical, L, {0, 0, 0}, Face::North, false, 7), 2);
        // vertical first: the L's vertical link wins for the lower block
        CHECK_EQ(pick(Method::VerticalHorizontal, L, {0, 0, 0}, Face::North, false, 7), 0);
        // pure horizontal run under VH uses the +4 tiles: x=2 is viewer-left end -> 4
        World line; for (int x = 0; x < 3; ++x) line.add(x, 0, 0);
        CHECK_EQ(pick(Method::VerticalHorizontal, line, {2, 0, 0}, Face::North, false, 7), 4);
        CHECK_EQ(pick(Method::VerticalHorizontal, line, {1, 0, 0}, Face::North, false, 7), 5);
        CHECK_EQ(pick(Method::VerticalHorizontal, line, {0, 0, 0}, Face::North, false, 7), 6);
    }

    // --- top ----------------------------------------------------------------
    {
        World w; w.add(0, 0, 0); w.add(0, 1, 0);
        CHECK_EQ(pick(Method::Top, w, {0, 0, 0}, Face::North, false, 1), 0);
        CHECK_EQ(pick(Method::Top, w, {0, 1, 0}, Face::North, false, 1), -1);
        CHECK_EQ(pick(Method::Top, w, {0, 0, 0}, Face::Up, false, 1), -1);
    }

    // --- inner seams ----------------------------------------------------------
    {
        World w; w.add(0, 0, 0); w.add(1, 0, 0); w.add(1, 0, -1); // block in front of the neighbour
        CHECK_EQ(pick(Method::Horizontal, w, {0, 0, 0}, Face::North, false, 4), 2); // connects
        CHECK_EQ(pick(Method::Horizontal, w, {0, 0, 0}, Face::North, true, 4), 3);  // seam kept
    }

    // --- top/bottom orientation ------------------------------------------------
    {
        World line; line.add(0, 0, 0); line.add(1, 0, 0);
        // looking down, east is the viewer's right: block x=0 has its neighbour on the right
        CHECK_EQ(pick(Method::Ctm, line, {0, 0, 0}, Face::Up), 1);
        CHECK_EQ(pick(Method::Ctm, line, {1, 0, 0}, Face::Up), 3);
    }

    // --- guards -----------------------------------------------------------------
    {
        World w; w.add(0, 0, 0);
        CHECK_EQ(pick(Method::Ctm, w, {0, 0, 0}, Face::North, false, 46), -1); // too few tiles
        CHECK_EQ(pick(Method::Fixed, w, {0, 0, 0}, Face::North, false, 1), 0);
    }

    std::printf("%d checks, %d failed\n", g_checks, g_fail);
    return g_fail ? 1 : 0;
}
