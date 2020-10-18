#pragma once

#include "catch.hpp"

// include from crawl's headers
#include "AppHdr.h"
#include "branch.h"
#include "cio.h"
#include "colour.h"
#include "crash.h"
#include "database.h"
#include "dungeon.h"
#include "dgn-overview.h"
#include "env.h"
#include "files.h"
#include "god-abil.h"
#include "item-name.h"
#include "item-prop.h"
#include "items.h"
#include "item-use.h"
#include "macro.h"
#include "maps.h"
#include "message.h"
#include "mon-cast.h"
#include "mon-transit.h"
#include "mon-util.h"
#include "mutation.h"
#include "ng-init.h"
#include "notes.h"
#include "output.h"
#include "spl-book.h"
#include "startup.h"
#include "state.h"
#include "syscalls.h"
#include "terrain.h"
#include "travel.h"
#include "viewchar.h"
#include "status.h"
#include "abyss.h"
#include "dlua.h"
#include "initfile.h"

// includes from catch2-tests/*
#include "init_guards.h"

namespace details_fixture_lua
{
    void _setup_fixture_lua();

    void _teardown_fixture_lua();
}

class FixtureLua
{
public:
    FixtureLua();

    ~FixtureLua();
};