#include "AppHdr.h"
#include "fake-main.hpp"
#include "localise.h"
#include "database.h"
#include "initfile.h"
#include "mgen-data.h"
#include "mon-place.h"
#include "options.h"
#include "unicode.h"

#include <iostream>
#include <string>
#include <vector>
#include <map>
using namespace std;

int num_passes = 0;
int num_fails = 0;

static void show_result(const string& actual, const string& expected)
{
    string status;
    if (actual == expected)
    {
        num_passes++;
        status = "PASS:   ";
    }
    else
    {
        num_fails++;
        status = "*FAIL*: ";
    }

    cout << status << "got: \"" << actual << "\"" << endl;
    if (actual != expected)
        cout << "   expected: \"" << expected << "\"" << endl;
}

int main()
{
    Options.lang_name = "de";
    SysEnv.crawl_dir = ".";
    setlocale(LC_ALL, "");
    databaseSystemInit(true);
    init_localisation("de");

    string s = "Deine Zaubersprüche";
    cout << "strwidth(\"" << s << "\") = " << strwidth(s) << endl;
    /*char32_t c;
    utf8towc(&c, "ü");
    printf("%02X\n", c);*/
    string result = localise("%-34.34s", "Your Spells");
    show_result(result, "Deine Zaubersprüche               ");

    string msg, expected;

    msg = localise("%s is %s.", "a gray horse", "happy");
    msg = uppercase_first(msg);
    show_result(msg, "Ein graues Pferd ist glücklich.");

    msg = localise("%s is %s.", "a gray cow", "happy");
    msg = uppercase_first(msg);
    show_result(msg, "Eine graue Kuh ist glücklich.");

    msg = localise("large closed door, spattered with blood");
    show_result(msg, "große geschlossene Tür, mit Blut bespritzt");

    msg = localise("a great orb of eyes");
    show_result(msg, "eine große Kugel der Augen");

    msg = localise("a greater orb of eyes");
    show_result(msg, "eine größere Kugel der Augen");

    msg = localise("10 greater orbs of eyes");
    show_result(msg, "10 größere Kugeln der Augen");

    // test with @foo@ style placeholders

    map<string, string> params;
    params["The_monster"] = "the orc";
    msg = localise("@The_monster@ hits you.", params);
    show_result(msg, "Der Ork trifft Euch.");

    params.clear();
    params["the_monster"] = "the orc";
    msg = localise("You hit @the_monster@.", params);
    show_result(msg, "Du triffst den Ork.");

    // test with %s params

    msg = uppercase_first(localise("%s hits %s", "an orc", "a skeleton"));
    expected = "Ein Ork trifft ein Skelett";
    show_result(msg, expected);

    msg = uppercase_first(localise("%s hits %s", "a skeleton", "an orc"));
    expected = "Ein Skelett trifft einen Ork";
    show_result(msg, expected);

    msg = uppercase_first(localise("%s hits %s", "your boomerang", "an orc"));
    expected = "Dein Bumerang trifft einen Ork";
    show_result(msg, expected);

    // test list with acc context
    msg = localise("You see here %s.", "a potion of lignification, a +0 short sword and a +2 arbalest");
    expected = "Du siehst hier einen Trank der Verholzung, ein +0 Kurzschwert und eine +2 Armbrust.";
    show_result(msg, expected);

    // test list with dat context
    msg = localise("You begin with the following equipment: %s", "a potion of lignification, a +0 short sword and a +2 arbalest");
    expected = "Du beginnst mit der folgenden Ausrüstung: einem Trank der Verholzung, einem +0 Kurzschwert und einer +2 Armbrust";
    show_result(msg, expected);

    // test mutant beasts
    msg = localise("the juvenile shock beast");
    expected = "das juvenile Schockbiest";
    show_result(msg, expected);

    msg = localise("an elder weird beast");
    expected = "ein älteres bizarres Biest";
    show_result(msg, expected);

    msg = localise_contextual("dat", "your mature fire beast");
    expected = "deinem reifen Feuerbiest";
    show_result(msg, expected);

    // test derived undead
    msg = localise("the orc skeleton");
    expected = "das Ork-Skelett";
    show_result(msg, expected);

    msg = localise_contextual("acc", "your elf zombie");
    expected = "deinen Elf-Zombie";
    show_result(msg, expected);

    msg = localise("3 ogre simulacra");
    expected = "3 Oger-Simulacra";
    show_result(msg, expected);

    msg = localise("3 Maras (2 ally target)");
    expected = "3 Maras (2 Ziele Verbündeter)";
    show_result(msg, expected);

    msg = localise("Boghold the orc warlord (strong)");
    expected = "Boghold der Ork-Warlord (stark)";
    show_result(msg, expected);

    msg = localise_contextual("acc", "Fred the pandemonium lord (strong)");
    expected = "Fred den Pandämoniumsfürsten (stark)";
    show_result(msg, expected);

    msg = localise("The apparition of %s.", "MrDizzy the Warrior, an amateur Naga Venom Mage of Cheibriados");
    expected = "Die Erscheinung von MrDizzy dem Krieger, einem amateurhaften Naga Giftmagier von Cheibriados.";
    show_result(msg, expected);

    msg = localise("hydra");
    expected = "Hydra";
    show_result(msg, expected);

    msg = localise("a hydra");
    expected = "eine Hydra";
    show_result(msg, expected);

    msg = localise("a seven-headed hydra (sleeping)");
    expected = "eine siebenköpfige Hydra (schlafend)";
    show_result(msg, expected);

    msg = localise("the 27-headed Lernaean hydra (sleeping)");
    expected = "die 27-köpfige Lernäische Hydra (schlafend)";
    show_result(msg, expected);

    msg = localise("a seven-headed hydra zombie (dormant)");
    expected = "ein siebenköpfiger Hydra-Zombie (ruhend)";
    show_result(msg, expected);

    msg = localise("a 27-headed Lernaean hydra zombie (dormant)");
    expected = "ein 27-köpfiger Lernäische-Hydra-Zombie (ruhend)";
    show_result(msg, expected);

    msg = localise("You hit %s", "the 27-headed Lernaean hydra zombie");
    expected = "Du triffst den 27-köpfigen Lernäische-Hydra-Zombie";
    show_result(msg, expected);

    msg = localise("an infernal demonspawn simulacrum (dormant)");
    expected = "ein Höllische-Dämonenbrut-Simulacrum (ruhend)";
    show_result(msg, expected);

    msg = localise("You see here %s.", "a yellow draconian skeleton");
    expected = "Du siehst hier ein Gelber-Drakonier-Skelett.";
    show_result(msg, expected);

    msg = localise("You see here %s.", "3 red draconian skeletons");
    expected = "Du siehst hier 3 Roter-Drakonier-Skelette.";
    show_result(msg, expected);

    msg = localise("an orc shaped pillar of salt");
    expected = "eine Ork-förmige Salzsäule";
    show_result(msg, expected);

    msg = localise("You see here %s.", "a Mennas shaped block of ice");
    expected = "Du siehst hier einen Mennas-förmigen Eisblock.";
    show_result(msg, expected);

    msg = localise("You strike %s from behind!", "the helpless yellow draconian");
    expected = "Du schlägst den hilflosen gelben Drakonier von hinten!";
    show_result(msg, expected);

    msg = localise("%s %s", "the spriggan's", "rapier");
    expected = "des Spriggans Rapier";
    show_result(msg, expected);

    msg = localise("%s %s", "the neutral spriggan's", "rapier");
    expected = "des neutralen Spriggans Rapier";
    show_result(msg, expected);

    msg = localise("%s %d times for up to %d damage each", "bite", 7, 10);
    expected = "7 Mal beißen für bis zu 10 Schaden pro Mal";
    show_result(msg, expected);

    msg = localise("You detect %s.", "the hexed spriggan");
    show_result(msg, "Du spürst den verhexten Spriggan.");

    msg = localise("The barbs in your skin will harm you if you move."
                   " Rampaging like this could really hurt!"
                   " Continue?");
    expected = "Die Widerhaken in deiner Haut werden dich verletzen, wenn du dich bewegst."
               " So eine Randale könnte wirklich wehtun!"
               " Weitermachen?";
    show_result(msg, expected);

    // player character selections
    show_result(localise("Hill Orc Fighter"), "Hügelork Kämpfer");
    show_result(localise("the Hill Orc Fighter"), "der Hügelork Kämpfer");
    show_result(localise("Hill Orc Gladiator"), "Hügelork Gladiator");
    show_result(localise("Deep Elf Ice Elementalist"), "Tiefenelf Eismagier");
    show_result(localise("Random Earth Elementalist"), "Zufälliger Erdmagier");
    show_result(localise("Recommended Hill Orc"), "Vorgeschlagener Hügelork");
    show_result(localise("Recommended Mummy"), "Vorgeschlagene Mumie");
    show_result(localise("Recommended Merfolk"), "Vorgeschlagenes Meervolk");

    msg = localise("You found it @in_location@", {{"in_location", "on level 3 of the Dungeon"}});
    show_result (msg, "Du hast diesen Gegenstand auf Ebene 3 des Dungeons gefunden");

    msg = localise("You found it @in_location@", {{"in_location", "in a volcano"}});
    show_result(msg, "Du hast diesen Gegenstand in einem Vulkan gefunden");

    msg = localise("Sparks fly from your @hands@", {{"hands", "hands"}});
    show_result(msg, "Funken fliegen aus deinen Händen");

    msg = localise("Sparks fly from your @hands@", {{"hands", "hand"}});
    show_result(msg, "Funken fliegen aus deiner Hand");

    msg = localise("You cannot do that while held in a net.");
    show_result(msg, "Das kannst du nicht, solange du in einem Netz gefangen bist.");

    msg = localise("You cannot do that while caught in a web.");
    show_result(msg, "Das kannst du nicht, solange du in einem Gewebe gefangen bist.");

    msg = localise("\"You will pay for your transgression, mortal!\"");
    show_result(msg, "\"Du wirst für deine Übertretung bezahlen, Sterblicher!\"");

    msg = localise("No spell in slot a!");
    show_result(msg, "Kein Zauber im Platz a!");

    msg = localise("You don't have enough magic to cast Fireball!");
    show_result(msg, "Du hast nicht genug Magie, um Feuerball zu wirken!");

    msg = localise("Automagic will cast spell in slot b (Blade Hands).");
    show_result(msg, "Automagie wird den Zauber im Platz b (Klingenhände) wirken.");

    msg = localise("Automagic enabled, will cast spell in slot c (Ice Form).");
    show_result(msg, "Automagie aktiviert, wird den Zauber im Platz c (Eisform) wirken.");

    msg = localise("You hear the whistling of a wind.");
    show_result(msg, "Du hörst das Pfeifen eines Windes.");

    msg = localise("You hear the brisk rumble of a very distant avalanche of rocks.");
    show_result(msg, "Du hörst das stramme Grollen einer Steinlawine weit in der Ferne.");

    msg = localise("You hear the thunderous beating of a drum.");
    show_result(msg, "Du hörst donnernde Paukenschläge.");

    msg = localise("You hear the stately tolling of a bell very nearby.");
    show_result(msg, "Du hörst das getragene Läuten einer Glocke ganz in der Nähe.");

    msg = localise("You hear the hiss of an avalanche of sand very nearby.");
    show_result(msg, "Du hörst das Zischen einer Sandlawine ganz in der Nähe.");

    msg = localise("This portal requires the presence of the gossamer rune to function.");
    show_result(msg, "Dieses Portal erfordert das Vorhandensein der Spinnfäden-Rune, um zu funktionieren.");

    msg = localise("This portal needs a +8 mace to function.");
    show_result(msg, "Dieses Portal benötigt einen +8 Streitkolben, um zu funktionieren.");

    msg = localise("You don't have 10 scrolls of teleportation with you.");
    show_result(msg, "Du hast 10 Schriftrollen der Teleportation nicht dabei.");

    msg = localise("Depths:2");
    show_result(msg, "Tiefen:2");

    msg = localise("The portal disappears!");
    show_result(msg, "Das Portal verschwindet!");

    msg = localise("show slimy rune");
    show_result(msg, "schleimige Rune zeigen");

    you.position = coord_def(10, 10);
    env.grid.init(DNGN_FLOOR);
    env.pgrid.init(FPROP_NONE);
    env.level_map_ids.init(INVALID_MAP_INDEX);
    env.level_map_mask.init(INVALID_MAP_INDEX);
    init_monsters();

    monster *dowan = get_free_monster();
    dowan->type = MONS_DOWAN;
    dowan->hit_points = 30;
    dowan->position = coord_def(10, 9);

    monster *orc = get_free_monster();
    orc->type = MONS_ORC;
    orc->hit_points = 20;
    orc->position = coord_def(10, 11);

    dowan->foe = orc->mindex();
    msg = getSpeakString("Gozag permabribe");
    msg = do_mon_str_replacements(msg, *dowan);
    expected = "Dowan grinst den Ork gierig an und klimpert mit dem Geldbeutel.";
    show_result(msg, expected);

    dowan->foe = MHITYOU;
    msg = getSpeakString("Gozag permabribe");
    msg = do_mon_str_replacements(msg, *dowan);
    expected = "Dowan grinst dich gierig an und klimpert mit dem Geldbeutel.";
    show_result(msg, expected);

    dowan->foe = orc->mindex();
    msg = getSpeakString("_Vashnia_rare_2_");
    msg = do_mon_str_replacements(msg, *dowan);
    expected = "Dowan sagt zu dem Ork, \"Du wirst ein schönes Nadelkissen abgeben.\"";
    show_result(msg, expected);

    dowan->foe = MHITYOU;
    msg = getSpeakString("_Vashnia_rare_2_");
    msg = do_mon_str_replacements(msg, *dowan);
    expected = "Dowan sagt, \"Du wirst ein schönes Nadelkissen abgeben.\"";
    show_result(msg, expected);

    msg = getSpeakString("fleeing Dowan");
    msg = do_mon_str_replacements(msg, *orc);
    expected = "VISUAL:Der Ork schreit entsetzt auf.";
    show_result(msg, expected);

    // test invisible monster
    orc->add_ench(mon_enchant(ENCH_INVIS));
    msg = getSpeakString("fleeing Dowan");
    msg = do_mon_str_replacements(msg, *orc);
    expected = "VISUAL:Etwas schreit entsetzt auf.";
    show_result(msg, expected);

    cout << "TEST INSULTS: " << endl;
    you.species = SP_MINOTAUR;
    orc->foe = MHITYOU;

    msg = getSpeakString("generic_insult");
    msg = do_mon_str_replacements(msg, *orc);
    cout << msg << endl;

    msg = getSpeakString("generic_insult");
    msg = do_mon_str_replacements(msg, *orc);
    cout << msg << endl;

    cout << endl << num_passes << " TESTS PASSED" << endl;
    if (num_fails > 0)
        cout << "**** " << num_fails << " TESTS FAILED ****" << endl;

    return num_fails;
}
