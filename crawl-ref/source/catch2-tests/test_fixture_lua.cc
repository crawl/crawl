/// \file
/// \brief Test some features of the lua fixture
/// \details
/// - Setup and tear down can happen for more than once
/// - Monster placement (dgn.create_monster)
/// - you.mutate, moveto
/// - tile and terrain
#include "catch.hpp"
#include "AppHdr.h"
#include "dlua.h"
#include "env.h"
#include "fixture_lua.h"
#include "player.h"
#include "rltiles/tiledef-dngn.h"
#include "state.h"
#include "stringutil.h"
#include "random.h"

/// \brief Random coordinate generator
/// \details Generate random valid coordinates for monster placement
class RandCoordGen
{
public:
    explicit RandCoordGen() : x_dist(X_BOUND_1 + 1, X_BOUND_2 - 1),
                              y_dist(Y_BOUND_1 + 1, Y_BOUND_2 - 1) {}

    std::vector<coord_def> roll(size_t size = 5)
    {
        std::vector<coord_def> data(size);
        for (size_t i = 0; i < data.size(); ++i)
            data[i] = coord_def(x_dist(gen), y_dist(gen));
        return data;
    }

private:
    std::default_random_engine gen;
    std::uniform_int_distribution<int> x_dist;
    std::uniform_int_distribution<int> y_dist;
};

// -------------------- //
// Forward declarations //
// -------------------- //
void place_monster(const coord_def& pos, const std::string& spec);

void require_mons_empty(const int);

// ------------- //
// General tests //
// ------------- //

/// \brief Setup and tear down the fixture for 3 times
TEST_CASE("Test setup and teardown", "[single-file][test_fixture_lua]")
{
    for (int i = 0; i < 5; ++i) FixtureLua fl;
}

/// \brief Setup and tear down the fixture for 3 times
TEST_CASE("Test running a Lua command", "[single-file][test_fixture_lua]")
{
    for (int i = 0; i < 5; ++i)
    {
        FixtureLua fl;
        dlua_exec("return 1 + 1", 1);
    }
}

/// \brief Test running a Lua script
TEST_CASE("Test running a Lua script", "[single-file][test_fixture_lua]")
{
    for (int i = 0; i < 5; ++i)
    {
        FixtureLua fl;
        you.hp = 100;  // need this or it crashes
        dlua.execfile("mutation.lua");
    }
}

// ---------------------------------------------- //
// Helper functions for testing monster placement //
// ---------------------------------------------- //

/// \brief Place monster and assert no Lua error
/// \param pos Coordinate to place the monster.
/// \param spec Specification for the monster. For example, "orc ench:invis".
void place_monster(const coord_def& pos, const std::string& spec)
{
    const std::string cmd = make_stringf(R"(dgn.create_monster(%d, %d, "%s"))",
                                         pos.x, pos.y, spec.c_str());
    dlua_exec(cmd, 1);
}

/// \brief Require that env.mons is empty, starting from an element
/// \param start Will start the check from this element
void require_mons_empty(const int start = 0)
{
    const int true_start = std::max(start, 0);
    for (size_t i = true_start; i < env.mons.size(); ++i)
    {
        const monster& m_now = env.mons[i];
        CAPTURE(i, m_now.type, m_now.name(DESC_PLAIN, true));
        if (i < MAX_MONSTERS)
            REQUIRE(m_now.type == monster_type::MONS_NO_MONSTER);
        else
            REQUIRE(m_now.type == monster_type::MONS_PROGRAM_BUG);
    }
}

// ---------------------- //
// Test monster placement //
// ---------------------- //

/// \brief Various tests of monster placement with Lua
TEST_CASE("Monster placement", "[single-file][test_fixture_lua]")
{
    RandCoordGen rcg;
    /// \brief Test if placing a monster causes Lua error
    SECTION("Test if monster placement gives lua error")
    {
        for (const auto& c : rcg.roll())
        {
            FixtureLua fl;
            place_monster(c, "orc ench:invis");
        }
    }

        /// \brief Test if reseting env.mons works
    SECTION("Test reset monster placement")
    {
        for (const auto& c : rcg.roll())
        {
            FixtureLua fl;
            // Require env.mons is totally clean after each init.
            require_mons_empty(0);
            place_monster(c, "orc ench:invis");
            // Since we have just placed an monster, env.mons should be empty
            // except for the first slot
            require_mons_empty(1);
        }
    }

        /// \brief Test if we placed the correct monster
    SECTION("Test monster's type")
    {
        for (const auto& c : rcg.roll())
        {
            FixtureLua fl;
            place_monster(c, "orc ench:invis");
            const monster& the_orc = env.mons[0];
            REQUIRE(the_orc.type == monster_type::MONS_ORC);
        }
    }

        /// \brief Test if the monster is placed at the correct position
    SECTION("Test monster's position")
    {
        for (const auto& c : rcg.roll())
        {
            FixtureLua fl;
            place_monster(c, "orc ench:invis");
            const monster& the_orc = env.mons[0];
            REQUIRE(the_orc.pos() == c);
        }
    }

        /// \brief Test if the monster has the correct enchantment
    SECTION("Test monster's enchantments")
    {
        for (const auto& c : rcg.roll())
        {
            FixtureLua fl;
            place_monster(c, "orc ench:invis");
            const monster& the_orc = env.mons[0];
            // The orc has an invisibility enchantment
            REQUIRE(the_orc.has_ench(enchant_type::ENCH_INVIS));
            // The orc only has 1 enchanment
            REQUIRE(the_orc.enchantments.size() == 1);
        }
    }
}

// ----------------------------------- //
// Helper functions for testing player //
// ----------------------------------- //

// ----------- //
// Test player //
// ----------- //
TEST_CASE("Player", "[single-file][test_fixture_lua]")
{
    RandCoordGen rcg;
    /// \brief Test if the fixture can reset the player's position
    SECTION("Test resetting the player's position")
    {
        for (const auto& c : rcg.roll())
        {
            FixtureLua fl;
            // Assert that we can reset player's position
            REQUIRE(you.pos() == coord_def(0, 0));
            const std::string cmd = make_stringf(
                    "you.moveto(%d, %d)", c.x, c.y);
            dlua_exec(cmd, 1);
            REQUIRE(you.pos() == c);
        }
    }

        /// \brief Test if the fixture can reset the player's mutations
    SECTION("Test resetting the player's mutation")
    {
        for (int i = 0; i < 5; ++i)
        {
            FixtureLua fl;
            you.species = SP_HUMAN;  // Human has no innate mutation
            // Assert that the player has no mutation
            const int mut_lv_before = you.get_base_mutation_level(
                    mutation_type::MUT_ROBUST);
            REQUIRE(mut_lv_before == 0);
            you.hp = 100;   // need to set hp, or we get segmentation fault
            // apply the mutation with Lua
            const std::string cmd = make_stringf(
                    R"(you.mutate("%s", "%s", "%s"))",
                    "robust", "reason of the mutation", "false");
            dlua_exec(cmd, 1);
            // Assert that the player now has 1 level of robustness mutation
            const int mut_lv_after = you.get_base_mutation_level(
                    mutation_type::MUT_ROBUST);
            REQUIRE(mut_lv_after == 1);
        }
    }
}

// ----------------- //
// Test tile/terrain //
// ----------------- //
TEST_CASE("Dungeon features", "[single-file][test_fixture_lua]")
{
    RandCoordGen rcg;
    /// \brief Test if the fixture can reset a dungeon tile's flavor
    SECTION("Test resetting a dungeon tile")
    {
        for (const auto& c : rcg.roll())
        {
            FixtureLua fl;
            REQUIRE(env.tile_flv(c).feat == 0);
            REQUIRE(env.tile_flv(c).wall == 0);
            REQUIRE(env.tile_flv(c).feat_idx == 0);
            const std::string cmd = make_stringf(
                    R"(dgn.tile_feat_changed(%d, %d, "DNGN_METAL_WALL"))",
                    c.x, c.y);
            dlua_exec(cmd, 1);
            // The dungeon feature doesn't change. Only the tile changed
            tileidx_t feat;
            REQUIRE(tile_dngn_index("DNGN_METAL_WALL", &feat));
            REQUIRE(env.grid(c) == DNGN_UNSEEN);
            REQUIRE(env.tile_flv(c).feat == feat);
            REQUIRE(env.tile_flv(c).wall == 0);
            REQUIRE(env.tile_flv(c).feat_idx == 1);
        }
    }

        /// \brief Test if the fixture can reset a dungeon terrain
    SECTION("Test resetting a dungeon terrain")
    {
        for (const auto& c : rcg.roll())
        {
            FixtureLua fl;
            // Require that the terrain has been resetted
            REQUIRE(env.grid(c) == DNGN_UNSEEN);
            // Require that terrain has no special property
            REQUIRE(env.pgrid(c) == FPROP_NONE);
            const std::string cmd = make_stringf(
                    R"(dgn.terrain_changed(%d, %d, "closed_door", false))",
                    c.x, c.y);
            dlua_exec(cmd, 1);
            // Require that terrain has updated
            REQUIRE(env.grid(c) == DNGN_CLOSED_DOOR);
            // Require that terrain has no special property
            REQUIRE(env.pgrid(c) == FPROP_NONE);
        }
    }
}

// ----------- //
// Test random //
// ----------- //

/// \brief Test if the random number generators have a fixed seed
TEST_CASE("Random seed", "[single-file][test_fixture_lua]")
{
    SECTION("Place random creatures")
    {
        int n_monsters = 20;
        std::vector<std::string> saved_names(n_monsters);
        for (int i = 0; i < 2; ++i)
        {
            FixtureLua fl;
            for (int j = 0; j < n_monsters; ++j)
            {
                place_monster(coord_def(j + 1, 10), "orc / bat");
                CAPTURE(crawl_state.seed, you.game_seed, Options.seed);
                const std::string m_name = env.mons[j].name(DESC_PLAIN, true);
                if (i == 0)
                {
                    saved_names[j] = m_name;
                }
                else
                {
                    CAPTURE(i, j, m_name, saved_names[j]);
                    REQUIRE(saved_names[j] == m_name);
                }
            }
        }
    }
}