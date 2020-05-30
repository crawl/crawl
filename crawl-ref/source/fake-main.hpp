/*
 * fake_main.hpp contains the common elements of the main files for
 * catch2_tests and monster. Both of those builds substitute crawl's
 * normal main.cc for a different file with a different main() function.
 * This file contains various bits of main.cc needed by crawl's various
 * subsystems to do anything (primarily global variables). It would
 * probably be possible to refactor out some common elements of this
 * file and regular main.cc.
 */
#include "externs.h"
#include "directn.h"
#include "env.h"
#include "colour.h"
#include "dungeon.h"
#include "mon-abil.h"
#include "mon-cast.h"
#include "libutil.h"
#include "random.h"
#include "state.h"
#include "stepdown.h"
#include "stringutil.h"
#include "artefact.h"
#include <sstream>
#include <set>
#include <unistd.h>

//////////////////////////////////////////////////////////////////////////
// main.cc stuff

CLua clua(true);
CLua dlua(false);      // Lua interpreter for the dungeon builder.
crawl_environment env; // Requires dlua.
player you;
game_state crawl_state;

void process_command(command_type);
void process_command(command_type, command_type) {}

void world_reacts();
void world_reacts() {}

// Clockwise, around the compass from north (same order as enum RUN_DIR)
const struct coord_def Compass[9] = {
    coord_def(0, -1), coord_def(1, -1),  coord_def(1, 0),
    coord_def(1, 1),  coord_def(0, 1),   coord_def(-1, 1),
    coord_def(-1, 0), coord_def(-1, -1), coord_def(0, 0),
};
