#include <random>

#include "catch.hpp"

#include "AppHdr.h"

#include "viewmap.h"

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
    state.lpos.pos = coord_def(0, 0);

    SECTION ("Panning the map down works.") {
        state = process_map_command(CMD_MAP_MOVE_DOWN, state);

        REQUIRE( state.lpos.pos == coord_def(0, 1));
    }

    SECTION ("Panning the map left works.") {
        state = process_map_command(CMD_MAP_MOVE_LEFT, state);

        REQUIRE( state.lpos.pos == coord_def(-1, 0));
    }

    SECTION ("Panning the map right works.") {
        state = process_map_command(CMD_MAP_MOVE_RIGHT, state);

        REQUIRE( state.lpos.pos == coord_def(1, 0));
    }

    SECTION ("Panning the map up works.") {
        state = process_map_command(CMD_MAP_MOVE_UP, state);

        REQUIRE( state.lpos.pos == coord_def(0, -1));
    }

    SECTION ("Exiting the map with Enter chooses a tile.") {
        state = process_map_command(CMD_MAP_GOTO_TARGET, state);

        REQUIRE(state.chose == true);
    }

    SECTION ("Exiting the map with Esc does not choose a tile.") {
        state = process_map_command(CMD_MAP_EXIT_MAP, state);

        REQUIRE(state.chose == false);
    }
}
