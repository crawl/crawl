#include "AppHdr.h"
#include "fake-main.hpp"
#include "localise.h"
#include "database.h"
#include "initfile.h"
#include "options.h"

#include <iostream>
#include <string>
#include <vector>
#include <map>
using namespace std;


const vector<string> cases = {
    "nom", "acc", "dat"
};

const vector<string> test_items = {

    "a scroll labelled BLAHDEY BLAH",
    "3 scrolls labelled FOO",

    "a scroll of blinking",
    "2 scrolls of remove curse",

    "a green potion",
    "an amethyst potion",
    "10 smoky black potions",

    "a potion of heal wounds",
    "5 potions of curing",

    "a +4 ring of protection (468 gold)",
    "a +6 ring of intelligence (760 gold)",
    "a cursed -4 ring of dexterity (26 gold)",

    "an orc corpse (skeletalised by now)",
    "a goblin corpse (skeletalised by now)",

    "a wand of digging (18)",
    "a lightning rod (3/4)",

    "the ring of Fedhas's Hope {MP+9 Int+4 Dex+2}",

    "a knobbly runed staff",

    "club",
    "+9 mace",
    "an arrow",
    "27 arrows",
    "a broad axe",
    "a glowing broad axe",
    "a runed whip",
    "a - a +9 broad axe of holy wrath",
    "a +0 giant spiked club",
    "a cursed -1 giant club",
    "a +0 vorpal battleaxe",
    "a +0 broad axe of flaming",
    "a -1 broad axe of freezing",
    "a cursed -2 broad axe of draining",
    "the +11 broad axe \"Jetioplo\" (weapon) {vorpal, Str+4}",
    "the +9 trident of the Crushed Allies {vorpal, Fragile +Inv Str+3 Int+3}",
    "the +7 pair of quick blades \"Gyre\" and \"Gimble\"",
    "+7 pair of quick blades",


    "a long sword, 30 bolts and an arbalest",
};

const vector<string> expected = {

    "eine Schriftrolle beschriftet mit BLAHDEY BLAH",
    "3 Schriftrollen beschriftet mit FOO",

    "eine Schriftrolle des Blinzelns",
    "2 Schriftrollen des Segnens",

    "ein grüner Zaubertrank",
    "ein amethystfarbener Zaubertrank",
    "10 rauchige schwarze Zaubertränke",

    "ein Trank der Wundheilung",
    "5 Tränke des Gegenmittels",

    "ein +4 Ring des Schutzes (468 Gold)",
    "ein +6 Ring der Intelligenz (760 Gold)",
    "ein verfluchter -4 Ring der Geschicklichkeit (26 Gold)",

    "eine Ork-Leiche (inzwischen skelettiert)",
    "eine Goblin-Leiche (inzwischen skelettiert)",

    "ein Zauberstab des Grabens (18)",
    "eine Blitzstange (3/4)",

    "der Ring von \"Fedhas's Hope\" {MP+9 Int+4 Ges+2}",

    "ein knubbeliger runenverzierter Stab",

    "Knüppel",
    "+9 Streitkolben",
    "ein Pfeil",
    "27 Pfeile",
    "eine Breitaxt",
    "eine glühende Breitaxt",
    "eine runenverzierte Peitsche",
    "a - eine +9 Breitaxt des heiligen Zorns",
    "eine +0 riesige Stachelkeule",
    "eine verfluchte -1 Riesenkeule",
    "eine +0 gebiffte Streitaxt",
    "eine +0 Breitaxt des Flammens",
    "eine -1 Breitaxt des Einfrierens",
    "eine verfluchte -2 Breitaxt der Auszehrung",
    "die +11 Breitaxt \"Jetioplo\" (Waffe) {Gebifft, Stä+4}",
    "der +9 Dreizack von \"the Crushed Allies\" {Gebifft, Fragil +Uns Stä+3 Int+3}",
    "das +7 Paar Schnellklingen \"Gyre\" und \"Gimble\"",
    "+7 Paar Schnellklingen",


    "ein Langschwert, 30 Bolzen und eine Armbrust"
};

const vector<string> armour_en =
{
    "a leather armour",
    "a brightly glowing leather armour",
    "a +1 scale mail",
    "an cursed robe",
    "a cursed -1 chain mail",
    "a pair of gloves",
    "an enchanted pair of boots",
    "a pair of runed gloves",
    "a cursed pair of runed gloves",
    "an cursed +0 pair of boots",
    "a cursed +2 pair of glowing boots",
    "an cursed +2 pair of gloves of archery",
    "the +1 plate armour of Dice, Bag, and Bottle {Str+6 Int-3 Dex+4 SInv} (5390 gold)"
};

const vector<string> armour_de =
{
    "eine Lederrüstung",
    "eine hell glühende Lederrüstung",
    "ein +1 Schuppenpanzer",
    "eine verfluchte Robe",
    "ein verfluchter -1 Kettenpanzer",
    "ein Paar Handschuhe",
    "ein verzaubertes Paar Stiefel",
    "ein Paar runenverzierte Handschuhe",
    "ein verfluchtes Paar runenverzierte Handschuhe",
    "ein verfluchtes +0 Paar Stiefel",
    "ein verfluchtes +2 Paar glühende Stiefel",
    "ein verfluchtes +2 Paar Handschuhe der Schießkunst",
    "die +1 Plattenrüstung von Würfel, Beutel, und Flasche {Stä+6 Int-3 Ges+4 UnsS} (5390 Gold)"
};

vector<map<string, string>> wands =
{
    {
        {"en", "an ivory wand"},
        {"nom", "ein Elfenbeinzauberstab"},
        {"acc", "einen Elfenbeinzauberstab"}
    },
    {
        {"en", "a cursed iron wand"},
        {"nom", "ein verfluchter Eisenzauberstab"},
        {"acc", "einen verfluchten Eisenzauberstab"}
    },
    {
        {"en", "a wand of flame (13)"},
        {"nom", "ein Zauberstab der Flammen (13)"},
        {"acc", "einen Zauberstab der Flammen (13)"}
    },
};

vector<map<string, string>> rings =
{
    {
        {"en", "a diamond ring"},
        {"nom", "ein Diamantring"},
        {"acc", "einen Diamantring"},
        {"dat", "einem Diamantring"},
    },
    {
        {"en", "an ivory ring"},
        {"nom", "ein Elfenbeinring"},
        {"acc", "einen Elfenbeinring"},
        {"dat", "einem Elfenbeinring"},
    },
    {
        {"en", "an cursed ivory ring"},
        {"nom", "ein verfluchter Elfenbeinring"},
        {"acc", "einen verfluchten Elfenbeinring"},
        {"dat", "einem verfluchten Elfenbeinring"},
    },
    {
        {"en", "a small silver ring"},
        {"nom", "ein kleiner Silberring"},
        {"acc", "einen kleinen Silberring"},
        {"dat", "einem kleinen Silberring"},
    },
    {
        {"en", "a ring of wizardry"},
        {"nom", "ein Ring der Zauberei"},
        {"acc", "einen Ring der Zauberei"},
        {"dat", "einem Ring der Zauberei"},
    },
    {
        {"en", "an cursed ring of protection from fire"},
        {"nom", "ein verfluchter Ring der Feuerresistenz"},
        {"acc", "einen verfluchten Ring der Feuerresistenz"},
        {"dat", "einem verfluchten Ring der Feuerresistenz"},
    },
    {
        {"en", "an cursed +4 ring of protection"},
        {"nom", "ein verfluchter +4 Ring des Schutzes"},
        {"acc", "einen verfluchten +4 Ring des Schutzes"},
        {"dat", "einem verfluchten +4 Ring des Schutzes"},
    },
    {
        {"en", "the ring \"Cuti\" {rF+ MR+ Dex+2}"},
        {"nom", "der Ring \"Cuti\" {rF+ MR+ Ges+2}"},
        {"acc", "den Ring \"Cuti\" {rF+ MR+ Ges+2}"},
        {"dat", "dem Ring \"Cuti\" {rF+ MR+ Ges+2}"},
    },
    {
        {"en", "ring of intelligence"},
        {"nom", "Ring der Intelligenz"},
        {"acc", "Ring der Intelligenz"},
        {"dat", "Ring der Intelligenz"},
    },
    {
        {"en", "ring of ice"},
        {"nom", "Ring des Eises"},
        {"acc", "Ring des Eises"},
        {"dat", "Ring des Eises"},
    },
   {
        {"en", "your ring of intelligence"},
        {"nom", "dein Ring der Intelligenz"},
        {"acc", "deinen Ring der Intelligenz"},
        {"dat", "deinem Ring der Intelligenz"},
    },
    {
        {"en", "your ring of ice"},
        {"nom", "dein Ring des Eises"},
        {"acc", "deinen Ring des Eises"},
        {"dat", "deinem Ring des Eises"},
    },
};

vector<map<string, string>> amulets =
{
    {
        {"en", "a filigree amulet"},
        {"nom", "ein Filigranamulett"},
        {"acc", "ein Filigranamulett"}
    },
    {
        {"en", "a dented ruby amulet"},
        {"nom", "ein verbeultes Rubinamulett"},
        {"acc", "ein verbeultes Rubinamulett"}
    },
    {
        {"en", "a cursed thin beryl amulet"},
        {"nom", "ein verfluchtes dünnes Beryllamulett"},
        {"acc", "ein verfluchtes dünnes Beryllamulett"}
    },
    {
        {"en", "an amulet of the acrobat"},
        {"nom", "ein Amulett des Akrobaten"},
        {"acc", "ein Amulett des Akrobaten"}
    },
    {
        {"en", "a cursed amulet of faith"},
        {"nom", "ein verfluchtes Amulett des Glaubens"},
        {"acc", "ein verfluchtes Amulett des Glaubens"}
    },
};

vector<map<string, string>> runes =
{
    {
        {"en", "the demonic rune"},
        {"nom", "die dämonische Rune"},
        {"acc", "die dämonische Rune"},
        {"dat", "der dämonischen Rune"},
    },
    {
        {"en", "the golden rune of Zot"},
        {"nom", "die goldene Rune von Zot"},
        {"acc", "die goldene Rune von Zot"},
        {"dat", "der goldenen Rune von Zot"},
    },
    {
        {"en", "an obsidian rune of Zot"},
        {"nom", "eine Obsidianrune von Zot"},
        {"acc", "eine Obsidianrune von Zot"},
        {"dat", "einer Obsidianrune von Zot"},
    },
    {
        {"en", "silver rune of Zot"},
        {"nom", "silberne Rune von Zot"},
        {"acc", "silberne Rune von Zot"},
        {"dat", "silberner Rune von Zot"},
    },
};

vector<map<string, string>> staves =
{
    {
        {"en", "a weird smoking staff"},
        {"nom", "ein seltsamer rauchiger Stab"},
        {"acc", "einen seltsamen rauchigen Stab"}
    },
    {
        {"en", "a cursed weird smoking staff"},
        {"nom", "ein verfluchter seltsamer rauchiger Stab"},
        {"acc", "einen verfluchten seltsamen rauchigen Stab"}
    },
    {
        {"en", "a staff of earth"},
        {"nom", "ein Stab der Erde"},
        {"acc", "einen Stab der Erde"}
    },
    {
        {"en", "an cursed staff of fire"},
        {"nom", "ein verfluchter Stab des Feuers"},
        {"acc", "einen verfluchten Stab des Feuers"}
    },
};

vector<map<string, string>> books =
{
    {
        {"en", "a book of Vapours"},
        {"nom", "ein Buch der Dämpfe"},
        {"acc", "ein Buch der Dämpfe"},
        {"dat", "einem Buch der Dämpfe"},
    },
};

int num_passes = 0;
int num_fails = 0;

static void test(const string& context, const string& item, const string& expect)
{
    string actual = localise_contextual(context, item);

    string status;
    if (actual == expect)
    {
        num_passes++;
        status = "PASS";
    }
    else
    {
        num_fails++;
        status = "*FAIL*";
    }

    cout << status << ": \"" << item << "\" (" << context << ")" << " -> \"" << actual << "\"" << endl;
}

static void test_group(const string& casus, const string& group_name, vector<map<string, string>>& items)
{
    cout << endl << group_name << ":" << endl;
    for (size_t i = 0; i < items.size(); i++)
    {
        map<string, string>& item = items.at(i);
        if (item.count(casus) != 0)
            test(casus, item["en"], item[casus]);
    }
}

int main()
{
    Options.lang_name = "de";
    SysEnv.crawl_dir = ".";
    setlocale(LC_ALL, "");
    databaseSystemInit(true);
    init_localisation("de");

    const int num_cases = cases.size();
    const int num_items = test_items.size();

    for (int j = 0; j < num_items; j++)
        test("nom", test_items[j], expected[j]);

    cout << endl << "ARMOUR:" << endl;
    for (size_t j = 0; j < armour_en.size(); j++)
        test("nom", armour_en[j], armour_de[j]);

    for (int i = 0; i < num_cases; i++)
    {
        string cse = cases[i];

        test_group(cse, "RINGS", rings);
        test_group(cse, "AMULETS", amulets);
        test_group(cse, "WANDS", wands);
        test_group(cse, "STAVES", staves);
        test_group(cse, "BOOKS", books);
        test_group(cse, "RUNES", runes);
    }

    cout << endl << num_passes << " TESTS PASSED" << endl;
    if (num_fails > 0)
        cout << "**** " << num_fails << " TESTS FAILED ****" << endl;

    return num_fails;
}
