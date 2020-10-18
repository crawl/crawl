/// \file
/// \brief Test some features of the lua fixture
/// \details
/// - Setup and tear down can happen for more than once
/// - Monster placement (dgn.create_monster)
/// - you.mutate, moveto
/// - tile and terrain

#include "catch.hpp"
#include "AppHdr.h"
#include "fixture_lua.h"
#include "stringutil.h"
#include "rltiles/tiledef-dngn.h"

// -------------------- //
// Forward declarations //
// -------------------- //
void place_monster(const coord_def& pos, const std::string& spec);

void require_mons_empty(const int);

void require_execstring(const std::string& cmd, int nresults,
                        const std::string&);

// ------------------------ //
// General helper functions //
// ------------------------ //

/// \brief Run dlua command and require no error
void require_execstring(const std::string& cmd, int nresults = 0,
                        const std::string& context = "base")
{
    const int err = dlua.execstring(cmd.c_str(), context.c_str(), nresults);
    INFO("Lua command: " << cmd.c_str());
    INFO("Lua error: " << dlua.error);
    REQUIRE(err == 0);
    REQUIRE(dlua.error == "");
}

// ------------- //
// General tests //
// ------------- //

/// \brief Setup and tear down the fixture for 3 times
TEST_CASE("fixture_lua: Test setup and teardown",
          "[single-file][test_fixture_lua]")
{
    for (int i = 0; i < 5; ++i)
    {
        details_fixture_lua::_setup_fixture_lua();
        details_fixture_lua::_teardown_fixture_lua();
    }
}

/// \brief Setup and tear down the fixture for 3 times
TEST_CASE("fixture_lua: Test running a Lua command",
          "[single-file][test_fixture_lua]")
{
    for (int i = 0; i < 5; ++i)
    {
        details_fixture_lua::_setup_fixture_lua();
        require_execstring("return 1 + 1", 1);
        details_fixture_lua::_teardown_fixture_lua();
    }
}

/// \brief Test running a Lua script
TEST_CASE("fixture_lua: Test running a Lua script",
          "[single-file][test_fixture_lua]")
{
    for (int i = 0; i < 5; ++i)
    {
        details_fixture_lua::_setup_fixture_lua();
        you.hp = 100;  // need this or it crashes
        dlua.execfile("mutation.lua");
        details_fixture_lua::_teardown_fixture_lua();
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
    const std::string cmd = make_stringf("dgn.create_monster(%d, %d, \"%s\")",
                                         pos.x, pos.y, spec.c_str());
    require_execstring(cmd, 1);
}

/// \brief Require that env.mons is empty, starting from an element
/// \param start Will start the check from this element
void require_mons_empty(const int start = 0)
{
    const int true_start = std::max(start, 0);
    for (size_t j = true_start; j < env.mons.size(); ++j)
    {
        const monster& m_now = env.mons[j];
        if (j < MAX_MONSTERS)
        {
            REQUIRE(m_now.type == monster_type::MONS_NO_MONSTER);
        } else if (j == MAX_MONSTERS)
        {
            REQUIRE(m_now.type == monster_type::MONS_PROGRAM_BUG);
        } else if (j > MAX_MONSTERS)
        {
            INFO("mons type: " << m_now.type);
            INFO("monster_name: " << m_now.name(DESC_PLAIN, true));
            REQUIRE(m_now.type == monster_type::MONS_PROGRAM_BUG);
        }
    }
}

// ---------------------- //
// Test monster placement //
// ---------------------- //

/// \brief Various tests of monster placement with Lua
TEST_CASE("fixture_lua: Monster placement",
          "[single-file][test_fixture_lua]")
{
    /// \brief Test if placing a monster causes Lua error
    SECTION("Test if monster placement gives lua error")
    {
        for (int i = 0; i < 3; ++i)
        {
            details_fixture_lua::_setup_fixture_lua();
            place_monster(coord_def(i + 3, i + 4), "orc ench:invis");
            details_fixture_lua::_teardown_fixture_lua();
        }
    }

        /// \brief Test if reseting env.mons works
    SECTION("Test reset monster placement")
    {
        for (int i = 0; i < 5; ++i)
        {
            details_fixture_lua::_setup_fixture_lua();
            // Require env.mons is totally clean after each init.
            require_mons_empty(0);
            place_monster(coord_def(i + 3, i + 4), "orc ench:invis");
            // Since we have just placed an monster, env.mons should be empty
            // except for the first slot
            require_mons_empty(1);
            details_fixture_lua::_teardown_fixture_lua();
        }
    }

        /// \brief Test if we placed the correct monster
    SECTION("Test monster's type")
    {
        for (int i = 0; i < 5; ++i)
        {
            details_fixture_lua::_setup_fixture_lua();
            place_monster(coord_def(i + 3, i + 4), "orc ench:invis");
            const monster& the_orc = env.mons[0];
            REQUIRE(the_orc.type == monster_type::MONS_ORC);
            details_fixture_lua::_teardown_fixture_lua();
        }
    }

        /// \brief Test if the monster is placed at the correct position
    SECTION("Test monster's position")
    {
        for (int i = 0; i < 5; ++i)
        {
            details_fixture_lua::_setup_fixture_lua();
            const coord_def pos(i + 3, i + 4);
            place_monster(pos, "orc ench:invis");
            const monster& the_orc = env.mons[0];
            REQUIRE(the_orc.pos() == pos);
            details_fixture_lua::_teardown_fixture_lua();
        }
    }

        /// \brief Test if the monster has the correct enchantment
    SECTION("Test monster's enchantments")
    {
        for (int i = 0; i < 5; ++i)
        {
            details_fixture_lua::_setup_fixture_lua();
            place_monster(coord_def(i + 3, i + 4), "orc ench:invis");
            const monster& the_orc = env.mons[0];
            // The orc has an invisibility enchantment
            REQUIRE(the_orc.has_ench(enchant_type::ENCH_INVIS));
            // The orc only has 1 enchanment
            REQUIRE(the_orc.enchantments.size() == 1);
            details_fixture_lua::_teardown_fixture_lua();
        }
    }
}

// ----------------------------------- //
// Helper functions for testing player //
// ----------------------------------- //

// ----------- //
// Test player //
// ----------- //
TEST_CASE("fixture_lua: Player", "[single-file][test_fixture_lua]")
{
    /// \brief Test if the fixture can reset the player's position
    SECTION("Test resetting the player's position")
    {
        for (int i = 0; i < 5; ++i)
        {
            details_fixture_lua::_setup_fixture_lua();
            // Assert that we can reset player's position
            REQUIRE(you.pos() == coord_def(0, 0));
            const coord_def pos(i + 3, i + 4);
            const std::string cmd = make_stringf(
                    "you.moveto(%d, %d)", pos.x, pos.y);
            require_execstring(cmd, 1);
            REQUIRE(you.pos() == pos);
            details_fixture_lua::_teardown_fixture_lua();
        }
    }

        /// \brief Test if the fixture can reset the player's mutations
    SECTION("Test resetting the player's mutation")
    {
        for (int i = 0; i < 5; ++i)
        {
            details_fixture_lua::_setup_fixture_lua();
            you.species = SP_HUMAN;  // Human has no innate mutation
            // Assert that the player has no mutation
            const int mut_lv_before = you.get_base_mutation_level(
                    mutation_type::MUT_ROBUST);
            REQUIRE(mut_lv_before == 0);
            you.hp = 100;   // need to set hp, or we get segmentation fault
            // apply the mutation with Lua
            const std::string cmd = make_stringf(
                    "you.mutate(\"%s\", \"%s\", \"%s\")",
                    "robust", "reason of the mutation", "false");
            require_execstring(cmd, 1);
            // Assert that the player now has 1 level of robustness mutation
            const int mut_lv_after = you.get_base_mutation_level(
                    mutation_type::MUT_ROBUST);
            REQUIRE(mut_lv_after == 1);
            details_fixture_lua::_teardown_fixture_lua();
        }
    }
}

// ----------------- //
// Test tile/terrain //
// ----------------- //
TEST_CASE("fixture_lua: Dungeon features", "[single-file][test_fixture_lua]")
{
    /// \brief Test if the fixture can reset a dungeon tile's flavor
    SECTION("Test resetting a dungeon tile")
    {
        for (int i = 0; i < 5; ++i)
        {
            const coord_def pos(3 + i, 4 + i);
            details_fixture_lua::_setup_fixture_lua();
            REQUIRE(env.tile_flv(pos).feat == 0);
            REQUIRE(env.tile_flv(pos).wall == 0);
            REQUIRE(env.tile_flv(pos).feat_idx == 0);
            const std::string cmd = make_stringf(
                    "dgn.tile_feat_changed(%d, %d, \"%s\")",
                    pos.x, pos.y, "DNGN_METAL_WALL");
            require_execstring(cmd, 1);
            // The dungeon feature doesn't change. Only the tile changed
            tileidx_t feat;
            std::string tilename = "DNGN_METAL_WALL";
            REQUIRE(tile_dngn_index(tilename.c_str(), &feat));
            REQUIRE(env.grid(pos) == DNGN_UNSEEN);
            REQUIRE(env.tile_flv(pos).feat == feat);
            REQUIRE(env.tile_flv(pos).wall == 0);
            REQUIRE(env.tile_flv(pos).feat_idx == 1);
            details_fixture_lua::_teardown_fixture_lua();
        }
    }

    /// \brief Test if the fixture can reset a dungeon terrain
    SECTION("Test resetting a dungeon terrain")
    {
        for (int i = 0; i < 5; ++i)
        {
            const coord_def pos(3 + i, 4 + i);
            details_fixture_lua::_setup_fixture_lua();
            // Require that the terrain has been resetted
            REQUIRE(env.grid(pos) == DNGN_UNSEEN);
            // Require that terrain has no special property
            REQUIRE(env.pgrid(pos) == FPROP_NONE);
            const std::string cmd = make_stringf(
                    "dgn.terrain_changed(%d, %d, \"%s\", %s)",
                    pos.x, pos.y, "closed_door", "false");
            require_execstring(cmd, 1);
            // Require that terrain has updated
            REQUIRE(env.grid(pos) == DNGN_CLOSED_DOOR);
            // Require that terrain has no special property
            REQUIRE(env.pgrid(pos) == FPROP_NONE);
            details_fixture_lua::_teardown_fixture_lua();
        }
    }
}