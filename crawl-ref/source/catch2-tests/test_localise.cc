#include "catch_amalgamated.hpp"

#include "AppHdr.h"
#include "database.h"
#include "initfile.h"
#include "localise.h"
#include "options.h"

#include <unistd.h>

TEST_CASE( "Localise German", "[single-file]" )
{
    setlocale(LC_ALL, "");
    Options.lang_name = "de";
    Options.language = lang_t::DE;
    SysEnv.crawl_dir = ".";
    databaseSystemInit();
    init_localisation();

    SECTION("Monsters")
    {
        // definite article
        CHECK( localise("the orc") == "der Ork" );
        CHECK( localise("the bat") == "die Fledermaus" );
        CHECK( localise("the hell hog") == "das höllische Schwein" );
        CHECK( localise("the red draconian") == "der rote Drakonier" );

        // indefinite article
        CHECK( localise("an orc") == "ein Ork" );
        CHECK( localise("a bat") == "eine Fledermaus" );
        CHECK( localise("a hell hog") == "ein höllisches Schwein" );
        CHECK( localise("a red draconian") == "ein roter Drakonier" );

        // your (allied)
        CHECK( localise("your orc") == "dein Ork" );
        CHECK( localise("your bat") == "deine Fledermaus" );
        CHECK( localise("your hell hog") == "dein höllisches Schwein" );
        CHECK( localise("your red draconian") == "dein roter Drakonier" );

        // no article
        CHECK( localise("orc") == "Ork" );
        CHECK( localise("bat") == "Fledermaus" );
        CHECK( localise("hell hog") == "höllisches Schwein" );
        CHECK( localise("red draconian") == "roter Drakonier" );

        // plural
        CHECK( localise("2 orcs") == "2 Orks" );
        CHECK( localise("3 bats") == "3 Fledermäuse" );
        CHECK( localise("4 hell hogs") == "4 höllische Schweine" );
        CHECK( localise("5 red draconians") == "5 rote Drakonier" );

        // unique
        CHECK( localise("Dowan") == "Dowan" );
        CHECK( localise("Natasha") == "Natascha" );
        CHECK( localise("Natasha (D:3)") == "Natascha (K:3)" );
        CHECK( localise("the Lernaean hydra") == "die Lernäische Hydra" );

        // with added adjectives
        CHECK( localise("the charmed orc") == "der verzauberte Ork" );
        CHECK( localise("a spectral orc") == "ein spektraler Ork" );
        CHECK( localise("neutral orc") == "neutraler Ork" );
        CHECK( localise("the helpless Natasha") == "die hilflose Natascha" );
        CHECK( localise("helpless Natasha") == "hilflose Natascha" );
        CHECK( localise("the helpless Royal Jelly") == "das hilflose Gelée Royale" );
        CHECK( localise("helpless Royal Jelly") == "hilfloses Gelée Royale" );
        CHECK( localise("a one-headed hydra") == "eine einköpfige Hydra" );
        CHECK( localise("an eight-headed hydra") == "eine achtköpfige Hydra" );
        CHECK( localise("an 11-headed hydra") == "eine 11-köpfige Hydra" );
        CHECK( localise("a 12-headed hydra") == "eine 12-köpfige Hydra" );
        CHECK( localise("the 27-headed Lernaean hydra") == "die 27-köpfige Lernäische Hydra" );

        // definite article - possessive
        CHECK( localise("the orc's") == "des Orks" );
        CHECK( localise("the bat's") == "der Fledermaus" );
        CHECK( localise("the hell hog's") == "des höllischen Schweins" );
        CHECK( localise("the red draconian's") == "des roten Drakoniers" );

        // indefinite article - possessive
        CHECK( localise("an orc's") == "eines Orks" );
        CHECK( localise("a bat's") == "einer Fledermaus" );
        CHECK( localise("a hell hog's") == "eines höllischen Schweins" );
        CHECK( localise("a red draconian's") == "eines roten Drakoniers" );

        // unique - possessive
        CHECK( localise("Natasha's") == "von Natascha" );
        CHECK( localise("the Lernaean hydra's") == "der Lernäischen Hydra" );
        CHECK( localise("the 27-headed Lernaean hydra's") == "der 27-köpfigen Lernäischen Hydra" );
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
