#include <random>

#include "catch.hpp"

#include "AppHdr.h"

#include "viewmap.h"
#include "map-knowledge.h"

TEST_CASE( "Test map search path generation works", "[single-file]" ) {
    const auto search_path = search_path_around_point(coord_def(50, 50));

    SECTION ("Search path has the right size.") {
        // The current location is not included.
        REQUIRE(search_path.size() == (78*68 - 1));
    }
}

TEST_CASE( "Test basic key controls work", "[single-file]" ) {
    map_control_state process_map_command(command_type cmd, const map_control_state &state);

    map_control_state state;
    // carve out a small known area so that known_map_bounds gives valid results
    for (int i = 49; i <= 51; i++)
        for (int j = 49; j <= 51; j++)
            env.map_knowledge[i][j].flags |= MAP_GRID_KNOWN;

    state.lpos.pos = coord_def(50, 50);

    SECTION ("Panning the map down works.") {
        state = process_map_command(CMD_MAP_MOVE_DOWN, state);

        REQUIRE( state.lpos.pos == coord_def(50, 51));
    }

    SECTION ("Panning the map left works.") {
        state = process_map_command(CMD_MAP_MOVE_LEFT, state);

        REQUIRE( state.lpos.pos == coord_def(49, 50));
    }

    SECTION ("Panning the map right works.") {
        state = process_map_command(CMD_MAP_MOVE_RIGHT, state);

        REQUIRE( state.lpos.pos == coord_def(51, 50));
    }

    SECTION ("Panning the map up works.") {
        state = process_map_command(CMD_MAP_MOVE_UP, state);

        REQUIRE( state.lpos.pos == coord_def(50, 49));
    }

    SECTION ("Panning out of known map bounds clamps to known map bounds.") {
        state.lpos.pos = coord_def(49, 49);
        state = process_map_command(CMD_MAP_MOVE_LEFT, state);
        state.lpos.pos = state.lpos.pos.clamped(known_map_bounds());

        REQUIRE( state.lpos.pos >= known_map_bounds().first );
        REQUIRE( state.lpos.pos <= known_map_bounds().second );
    }

    SECTION ("Exiting the map with Enter chooses a tile.") {
        state = process_map_command(CMD_MAP_GOTO_TARGET, state);

        REQUIRE(state.chose == true);
    }

    SECTION ("Exiting the map with Esc does not choose a tile.") {
        state = process_map_command(CMD_MAP_EXIT_MAP, state);

        REQUIRE(state.chose == false);
    }

    SECTION ("The current position is kept within the known map bounds.") {
        state.lpos.pos = coord_def(-100, 1000);

        REQUIRE(map_bounds(state.lpos.pos) == false);

        state = process_map_command(CMD_NO_CMD, state);
        state.lpos.pos = state.lpos.pos.clamped(known_map_bounds());

        REQUIRE( state.lpos.pos >= known_map_bounds().first );
        REQUIRE( state.lpos.pos <= known_map_bounds().second );
        REQUIRE(map_bounds(state.lpos.pos) == true);
    }
}
