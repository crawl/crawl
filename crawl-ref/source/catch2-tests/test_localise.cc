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
        CHECK( localise("the slime creature") == "die Schleimkreatur" );

        // indefinite article
        CHECK( localise("an orc") == "ein Ork" );
        CHECK( localise("a bat") == "eine Fledermaus" );
        CHECK( localise("a hell hog") == "ein höllisches Schwein" );
        CHECK( localise("a green draconian") == "ein grüner Drakonier" );
        CHECK( localise("a large slime creature") == "eine große Schleimkreatur" );

        // your (allied)
        CHECK( localise("your orc") == "dein Ork" );
        CHECK( localise("your bat") == "deine Fledermaus" );
        CHECK( localise("your hell hog") == "dein höllisches Schwein" );
        CHECK( localise("your yellow draconian") == "dein gelber Drakonier" );
        CHECK( localise("your very large slime creature") == "deine sehr große Schleimkreatur" );

        // no article
        CHECK( localise("orc") == "Ork" );
        CHECK( localise("bat") == "Fledermaus" );
        CHECK( localise("hell hog") == "höllisches Schwein" );
        CHECK( localise("black draconian") == "schwarzer Drakonier" );
        CHECK( localise("enormous slime creature") == "enorme Schleimkreatur" );

        // plural
        CHECK( localise("2 orcs") == "2 Orks" );
        CHECK( localise("3 bats") == "3 Fledermäuse" );
        CHECK( localise("4 hell hogs") == "4 höllische Schweine" );
        CHECK( localise("5 white draconians") == "5 weiße Drakonier" );
        CHECK( localise("6 titanic slime creatures") == "6 titanische Schleimkreaturen" );

        // unique
        CHECK( localise("Dowan") == "Dowan" );
        CHECK( localise("Natasha") == "Natascha" );
        CHECK( localise("Natasha (D:3)") == "Natascha (K:3)" );
        CHECK( localise("the Lernaean hydra") == "die Lernäische Hydra" );
        CHECK( localise("Blorkula the orcula") == "Blorkula der Orkula" );

        // named ally
        CHECK( localise("Boghold the orc") == "Boghold der Ork" );

        // shape-shifted unique
        CHECK( localise("Sigmund the bat") == "Sigmund die Fledermaus" );
        CHECK( localise("Natasha the acid blob") == "Natascha der Säureklumpen" );

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
        CHECK( localise("a non-hostile very large slime creature") ==
                        "eine nicht feindliche sehr große Schleimkreatur" );
        CHECK( localise("a neutral bribed spectral orc") ==
                        "ein neutraler bestochener spektraler Ork" );

        // player ghost
        CHECK( localise("MrDizzy's ghost") == "der Geist von MrDizzy" );
        CHECK( localise("the helpless RepoMan's ghost") == "der hilflose Geist von RepoMan" );

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

        // derived monsters
        CHECK( localise("the merfolk zombie") == "der Zombie eines Meerwesens" );
        CHECK( localise("an orc skeleton") == "ein Skelett eines Orks" );
        CHECK( localise("a red draconian simulacrum") == "ein Simulacrum eines roten Drakoniers" );
        /*CHECK( localise("a neutral orc skeleton") == "ein neutrales Skelett eines Orks" );
        CHECK( localise("a non-hostile red draconian simulacrum") ==
                        "ein nicht feindliches Simulacrum eines roten Drakoniers" );*/
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
        CHECK( localise("Sigmund's scythe dances into the air!") == "Die Sense von Sigmund tanzt in die Luft!" );

        CHECK( localise("You draw life force from the goblin.") ==
                        "Du ziehst Lebenskraft aus dem Goblin." );
        CHECK( localise("You draw life force from Fannar!!") ==
                        "Du ziehst Lebenskraft aus Fannar!!" );
        CHECK( localise("The vampire draws life force from you!!!") ==
                        "Der Vampir zieht Lebenskraft aus dir!!!" );
    }

    SECTION("Lists")
    {
        CHECK( localise("a goblin, 2 orcs and a kobold") == "ein Goblin, 2 Orks und ein Kobold" );
        CHECK( localise("a goblin, 2 orcs, and a kobold") == "ein Goblin, 2 Orks und ein Kobold" );
        CHECK( localise("a goblin, 2 orcs or a kobold") == "ein Goblin, 2 Orks oder ein Kobold" );
        CHECK( localise("a goblin, 2 orcs, or a kobold") == "ein Goblin, 2 Orks oder ein Kobold" );
        //CHECK( localise("A goblin, 2 orcs and a kobold come into view.") ==
        //                "Ein Goblin, 2 Orks und ein Kobold kommen in Sicht." );
    }

    databaseSystemShutdown();
    Options.lang_name = "en";
    Options.language = lang_t::EN;
}
