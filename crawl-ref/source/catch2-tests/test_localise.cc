#include "catch_amalgamated.hpp"

#include "AppHdr.h"
#include "database.h"
#include "initfile.h"
#include "localise.h"
#include "options.h"

#include <unistd.h>

TEST_CASE( "Localise", "[single-file]" )
{
    setlocale(LC_ALL, "");
    Options.lang_name = "de";
    Options.language = lang_t::DE;
    SysEnv.crawl_dir = ".";
    databaseSystemInit();
    init_localisation();

    SECTION("Monsters")
    {
        CHECK( localise("Natasha") == "Natascha" );
        CHECK( localise("Natasha (D:3)") == "Natascha (K:3)" );
    }

    SECTION("Items")
    {
        CHECK( localise("a short sword") == "ein Kurzschwert" );
        CHECK( localise("m - 2 potions of might") == "m - 2 Tränke der Macht" );
    }

    SECTION("Parameterised messages")
    {
        CHECK( localise("You kill the rat!") == "Du tötest die Ratte!" );
        CHECK( localise("You kill the orc!") == "Du tötest den Ork!" );
        CHECK( localise("The broad axe dances into the air!") == "Die Breitaxt tanzt in die Luft!" );
        CHECK( localise("The gnoll's halberd dances into the air!") == "Die Hellebarde des Gnolls tanzt in die Luft!" );
    }

    databaseSystemShutdown();
    Options.lang_name = "en";
    Options.language = lang_t::EN;
}
