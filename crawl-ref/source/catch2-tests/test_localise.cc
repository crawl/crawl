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

    SECTION("Sentence")
    {
        CHECK( localise("You kill the rat!") == "Du tötest die Ratte!" );
        CHECK( localise("You kill the orc!") == "Du tötest den Ork!" );
    }
}
