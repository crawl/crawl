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

    "ein Langschwert, 30 Bolzen und eine Armbrust"
};

vector<map<string, string>> weapons =
{
    {
        {"en", "the lance \"Wyrmbane\""},
        {"nom", "die Lanze \"Wyrmbane\""},
        {"acc", "die Lanze \"Wyrmbane\""},
        {"dat", "der Lanze \"Wyrmbane\""},
    },
    {
        {"en", "a lance"},
        {"nom", "eine Lanze"},
        {"acc", "eine Lanze"},
        {"dat", "einer Lanze"},
    },
    {
        {"en", "lance"},
        {"nom", "Lanze"},
        {"acc", "Lanze"},
        {"dat", "Lanze"},
    },
    {
        {"en", "the +9 sword of Cerebov"},
        {"nom", "das +9 Schwert von Cerebov"},
        {"acc", "das +9 Schwert von Cerebov"},
        {"dat", "dem +9 Schwert von Cerebov"},
    },
    {
        {"en", "+9 sword of Cerebov"},
        {"nom", "+9 Schwert von Cerebov"},
        {"acc", "+9 Schwert von Cerebov"},
        {"dat", "+9 Schwert von Cerebov"},
    },
    {
        // unidentified sword of Cerebov
        {"en", "a great serpentine sword"},
        {"nom", "ein großes, flammenförmiges Schwert"},
        {"acc", "ein großes, flammenförmiges Schwert"},
        //{"dat", "einem großen, flammenförmigen Schwert"},
    },
    {
        // sword of Cerebov base type
        {"en", "a flamberge"},
        {"nom", "ein Flamberg"},
        {"acc", "einen Flamberg"},
        {"dat", "einem Flamberg"},
    },
    {
        // sword of Cerebov base type
        {"en", "flamberge"},
        {"nom", "Flamberg"},
        {"acc", "Flamberg"},
        {"dat", "Flamberg"},
    },
    {
        {"en", "the +8 Wrath of Trog {antimagic, *Rage Rampage}"},
        {"nom", "der +8 Zorn von Trog {Antimagie, *Wut Randale}"},
        {"acc", "den +8 Zorn von Trog {Antimagie, *Wut Randale}"},
        {"dat", "dem +8 Zorn von Trog {Antimagie, *Wut Randale}"},
    },
    {
        // unidentified Wrath of Trog
        {"en", "a Wrath of Trog"},
        {"nom", "ein Zorn von Trog"},
        {"acc", "einen Zorn von Trog"},
        {"dat", "einem Zorn von Trog"},
    },
    {
        {"en", "Wrath of Trog"},
        {"nom", "Zorn von Trog"},
        {"acc", "Zorn von Trog"},
        {"dat", "Zorn von Trog"},
    },
    {
        {"en", "the +7 pair of quick blades \"Gyre\" and \"Gimble\""},
        {"nom", "das +7 Paar Schnellklingen \"Gyre\" und \"Gimble\""},
        {"acc", "das +7 Paar Schnellklingen \"Gyre\" und \"Gimble\""},
        {"dat", "dem +7 Paar Schnellklingen \"Gyre\" und \"Gimble\""},
    },
    {
        {"en", "the +7 pair of quick blades"},
        {"nom", "das +7 Paar Schnellklingen"},
        {"acc", "das +7 Paar Schnellklingen"},
        {"dat", "dem +7 Paar Schnellklingen"},
    },
    {
        {"en", "a +7 pair of quick blades"},
        {"nom", "ein +7 Paar Schnellklingen"},
        {"acc", "ein +7 Paar Schnellklingen"},
        {"dat", "einem +7 Paar Schnellklingen"},
    },
    {
        {"en", "the cursed +5 scythe \"Finisher\" {speed, eviscerate}"},
        {"nom", "die verfluchte +5 Sense \"Beender\" {Geschw, ausweiden}"},
        {"acc", "die verfluchte +5 Sense \"Beender\" {Geschw, ausweiden}"},
        {"dat", "der verfluchten +5 Sense \"Beender\" {Geschw, ausweiden}"},
    },
    {
        {"en", "a scythe \"Finisher\""},
        {"nom", "eine Sense \"Beender\""},
        {"acc", "eine Sense \"Beender\""},
        {"dat", "einer Sense \"Beender\""},
    },
    {
        {"en", "scythe \"Finisher\""},
        {"nom", "Sense \"Beender\""},
        {"acc", "Sense \"Beender\""},
        {"dat", "Sense \"Beender\""},
    },
};

vector<map<string, string>> armour =
{
    {
        {"en", "a leather armour"},
        {"nom", "eine Lederrüstung"},
        {"acc", "eine Lederrüstung"},
        {"dat", "einer Lederrüstung"},
    },
    {
        {"en", "a brightly glowing leather armour"},
        {"nom", "eine hell glühende Lederrüstung"},
        {"acc", "eine hell glühende Lederrüstung"},
        {"dat", "einer hell glühenden Lederrüstung"},
    },
    {
        {"en", "a +1 scale mail"},
        {"nom", "ein +1 Schuppenpanzer"},
        {"acc", "einen +1 Schuppenpanzer"},
        {"dat", "einem +1 Schuppenpanzer"},
    },
    {
        {"en", "an cursed robe"},
        {"nom", "eine verfluchte Robe"},
        {"acc", "eine verfluchte Robe"},
        {"dat", "einer verfluchten Robe"},
    },
    {
        {"en", "a cursed -1 chain mail"},
        {"nom", "ein verfluchter -1 Kettenpanzer"},
        {"acc", "einen verfluchten -1 Kettenpanzer"},
        {"dat", "einem verfluchten -1 Kettenpanzer"},
    },
    {
        {"en", "a pair of gloves"},
        {"nom", "ein Paar Handschuhe"},
        {"acc", "ein Paar Handschuhe"},
        {"dat", "einem Paar Handschuhe"},
    },
    {
        {"en", "an enchanted pair of boots"},
        {"nom", "ein verzaubertes Paar Stiefel"},
        {"acc", "ein verzaubertes Paar Stiefel"},
        {"dat", "einem verzauberten Paar Stiefel"},
    },
    {
        {"en", "a pair of runed gloves"},
        {"nom", "ein Paar runenverzierte Handschuhe"},
        {"acc", "ein Paar runenverzierte Handschuhe"},
        {"dat", "einem Paar runenverzierte Handschuhe"},
    },
    {
        {"en", "a cursed pair of runed gloves"},
        {"nom", "ein verfluchtes Paar runenverzierte Handschuhe"},
        {"acc", "ein verfluchtes Paar runenverzierte Handschuhe"},
        {"dat", "einem verfluchten Paar runenverzierte Handschuhe"},
    },
    {
        {"en", "an cursed +0 pair of boots"},
        {"nom", "ein verfluchtes +0 Paar Stiefel"},
        {"acc", "ein verfluchtes +0 Paar Stiefel"},
        {"dat", "einem verfluchten +0 Paar Stiefel"},
    },
    {
        {"en", "a cursed +2 pair of glowing boots"},
        {"nom", "ein verfluchtes +2 Paar glühende Stiefel"},
        {"acc", "ein verfluchtes +2 Paar glühende Stiefel"},
        {"dat", "einem verfluchten +2 Paar glühende Stiefel"},
    },
    {
        {"en", "an cursed +2 pair of gloves of archery"},
        {"nom", "ein verfluchtes +2 Paar Handschuhe der Schießkunst"},
        {"acc", "ein verfluchtes +2 Paar Handschuhe der Schießkunst"},
        {"dat", "einem verfluchten +2 Paar Handschuhe der Schießkunst"},
    },
    {
        {"en", "the pair of seven-league boots"},
        {"nom", "das Paar Siebenmeilenstiefel"},
        {"acc", "das Paar Siebenmeilenstiefel"},
        {"dat", "dem Paar Siebenmeilenstiefel"},
    },
    {
        {"en", "the +3 pair of seven-league boots"},
        {"nom", "das +3 Paar Siebenmeilenstiefel"},
        {"acc", "das +3 Paar Siebenmeilenstiefel"},
        {"dat", "dem +3 Paar Siebenmeilenstiefel"},
    },
    {
        {"en", "the +1 plate armour of Dice, Bag, and Bottle {Str+6 Int-3 Dex+4 SInv} (5390 gold)"},
        {"nom", "die +1 Plattenrüstung von Würfel, Beutel, und Flasche {Stä+6 Int-3 Ges+4 UnsS} (5390 Gold)"},
        {"acc", "die +1 Plattenrüstung von Würfel, Beutel, und Flasche {Stä+6 Int-3 Ges+4 UnsS} (5390 Gold)"},
        {"dat", "der +1 Plattenrüstung von Würfel, Beutel, und Flasche {Stä+6 Int-3 Ges+4 UnsS} (5390 Gold)"},
    },
    // plural
    {
        {"en", "the cursed +12 storm dragon scales"},
        {"nom", "die verfluchten +12 Sturmdrachenschuppen"},
        {"acc", "die verfluchten +12 Sturmdrachenschuppen"},
        {"dat", "den verfluchten +12 Sturmdrachenschuppen"},
    },
    {
        {"en", "your cursed +12 storm dragon scales"},
        {"nom", "deine verfluchten +12 Sturmdrachenschuppen"},
        {"acc", "deine verfluchten +12 Sturmdrachenschuppen"},
        {"dat", "deinen verfluchten +12 Sturmdrachenschuppen"},
    },
    {
        {"en", "cursed +12 storm dragon scales"},
        {"nom", "verfluchte +12 Sturmdrachenschuppen"},
        {"acc", "verfluchte +12 Sturmdrachenschuppen"},
        {"dat", "verfluchten +12 Sturmdrachenschuppen"},
    },
    {
        {"en", "storm dragon scales"},
        {"nom", "Sturmdrachenschuppen"},
        {"acc", "Sturmdrachenschuppen"},
        {"dat", "Sturmdrachenschuppen"},
    },
    {
        // with inventory letter
        {"en", "c - cursed +12 storm dragon scales"},
        {"nom", "c - verfluchte +12 Sturmdrachenschuppen"},
    },
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
        {"nom", "ein Buch der Dünste"},
        {"acc", "ein Buch der Dünste"},
        {"dat", "einem Buch der Dünste"},
    },
};

vector<map<string, string>> others =
{
    {
        {"en", "the Orb of Zot"},
        {"nom", "die Kugel von Zot"},
        {"acc", "die Kugel von Zot"},
        {"dat", "der Kugel von Zot"},
    },
    {
        {"en", "The Orb of Zot"},
        {"nom", "Die Kugel von Zot"},
    },
    {
        {"en", "Orb of Zot"},
        {"nom", "Kugel von Zot"},
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

    cout << status << ": \"" << item << "\"";
    if (!context.empty())
        cout << " (" << context << ")";
    cout << " -> \"" << actual << "\"" << endl;
}

static void test(const string& item, const string& expect)
{
    return test("", item, expect);
}

static void test_group(const string& group_name, vector<map<string, string>>& items)
{
    static const vector<string> cases = {"nom", "acc", "dat"};

    cout << endl << group_name << ":" << endl;
    for (size_t i = 0; i < items.size(); i++)
    {
        cout << endl;
        map<string, string>& item = items.at(i);

        for (const string& casus: cases)
        {
            if (item.count(casus) != 0)
                test(casus, item["en"], item[casus]);
        }
    }
}

int main()
{
    Options.lang_name = "de";
    SysEnv.crawl_dir = ".";
    setlocale(LC_ALL, "");
    databaseSystemInit(true);
    init_localisation("de");

    const int num_items = test_items.size();

    for (int j = 0; j < num_items; j++)
        test("nom", test_items[j], expected[j]);

    test_group("WEAPONS", weapons);
    test_group("ARMOUR", armour);
    test_group("RINGS", rings);
    test_group("AMULETS", amulets);
    test_group("WANDS", wands);
    test_group("STAVES", staves);
    test_group("BOOKS", books);
    test_group("RUNES", runes);
    test_group("OTHERS", others);

    cout << endl << "BOOK TITLES: " << endl;

    // randomly-generated
    test("the Almanac of Magic", "der Almanach der Magie");
    test("The Compendium of the Arcane Arts", "Das Kompendium der arkanen Künste");
    test("Wizardry, Part One", "Zauberei, Band Eins");
    test("Easy Theurgy", "Einfache Theurgie");
    test("Advanced Sorcery", "Fortgeschrittenes Hexenwerk");
    test("Sophisticated Witchcraft", "Raffinierte Hexerei");
    test("A Beginning Course on Rites and Rhymes", "Ein Einstiegskurs für Riten und Reime");
    test("An Introduction to Thaumaturgy", "Eine Einführung in Thaumaturgie");
    test("First Steps in Casting", "Erste Schritte im Zauberwirken");
    test("Spellcasting for Beginners", "Zaubern für Anfänger");
    test("Witchery 101", "Hexerei Basiskurs");
    test("A Primer on Spellcraft", "Eine Fibel für Zauberkunst");
    test("An Early Guide to the Hermit's Heritage", "Ein erster Leitfaden für das Erbe des Eremiten");
    test("The Foundations of Spellweaving", "Die Grundlagen des Zauberwebens");
    test("Secret Knowledge in Simple Steps", "Geheimwissen in einfachen Schritten");
    test("Hermeneutics Continued", "Hermeneutik fortgesetzt");
    test("Magic, and How To Use It", "Magie und die Verwendung davon");
    test("Advanced Steps in the Arcane Arts", "Weitere Schritte in den arkanen Künsten");
    test("The Advanced Guide to Theurgy", "Der fortgeschrittene Leitfaden für Theurgie");
    test("Further Steps in the Hermit's Heritage", "Weitere Schritte im Erbe des Eremiten");
    test("Further Progress in Thaumaturgy", "Weitere Fortschritte in der Thaumaturgie");
    test("The Second Course in Casting", "Der Zweite Kurs im Zauberwirken");
    test("Improving Your Spellcasting", "Verbessert Euer Zaubern");
    test("A Thorough Guide to Wizardry", "Ein gründlicher Leitfaden für Zauberei");
    test("The Last Secrets of Sorcery", "Die letzten Geheimnisse des Hexenwerks");
    test("The Lost Secrets of Witchcraft", "Die verlorenen Geheimnisse der Hexerei");
    test("The Arcane Secrets of Witchery", "Die arkanen Geheimnisse der Hexerei");
    test("The Intricacies of Spellcraft", "Die Feinheiten der Zauberkunst");
    test("Mastering Spellweaving", "Beherrschung des Zauberwebens");
    test("The Testament of Boris", "Das Testament von Boris");
    test("The Last Tome of the Archmage", "Der letzte Foliant des Erzmagiers");
    test("The Lost Papyrus of the Deep Elf Lorekeeper", "Der verlorene Papyrus des Weisen der Tiefelfen");
    test("The Handbook of Greatest Risks and Utility", "Das Handbuch der größten Risiken und des höchsten Nutzens");
    // if suffix no translation...
    test("The Catalogue of suffix that has no translation", "Der Katalog von \"suffix that has no translation\"");
    // single school
    test("the Compendium of Curses", "das Kompendium der Flüche");
    test("the Handbook of Fire", "das Handbuch des Feuers");
    test("the Handbook of Ice", "das Handbuch des Eises");
    test("the Disquisition on Rituals", "die Abhandlung über Rituale");
    // two schools
    test("the Almanac of Darkness and the Frostbite", "der Almanach der Dunkelheit und der Erfrierung");
    test("the Almanac of Battle Magic and Calling", "der Almanach der Kampfmagie und des Rufens");
    test("the Handbook of Fire and Ice", "das Handbuch des Feuers und des Eises");
    test("the Reference Book on the Oath and Snow", "das Nachschlagewerk über den Schwur und Schnee");
    test("the Compendium of Fiery Attacks", "das Kompendium der feurigen Angriffe");
    test("the Guide on Toxic Damage Dealing", "der Leitfaden für toxischen Schaden");

    // randomly-generated with owner
    cout << endl;
    test("Alistair's Incunabulum of Magic", "Alistairs Inkunabel der Magie");
    test("Borgnjor's Collected Works on the Arcane Arts", "Borgnjors gesammelte Werke über die arkanen Künste");
    test("Cigotuvi's Wizardry, Part Six", "Cigotuvis Zauberei, Band Sechs");
    test("Iskenderun's Easy Theurgy", "Iskenderuns einfache Theurgie");
    test("Lee's Advanced Sorcery", "Lees fortgeschrittenes Hexenwerk");
    test("Lehudib's Sophisticated Witchcraft", "Lehudibs raffinierte Hexerei");
    test("Olgreb's Beginning Course on Rites and Rhymes", "Olgrebs Einstiegskurs für Riten und Reime");
    test("Ozocubu's Introduction to Thaumaturgy", "Ozocubus Einführung in Thaumaturgie");
    test("Sif Muna's First Steps in Casting", "Sif Munas erste Schritte im Zauberwirken");
    test("Vehumet's Spellcasting for Beginners", "Vehumets Zaubern für Anfänger");
    test("Kikubaaqudgha's Witchery 101", "Kikubaaqudghas Hexerei Basiskurs");
    test("Ts'ui Pên's Primer on Spellcraft", "Ts'ui Pêns Fibel für Zauberkunst");
    test("Ts'ui Pên's Early Guide to the Hermit's Heritage", "Ts'ui Pêns erster Leitfaden für das Erbe des Eremiten");
    test("Ypeomyrph's Foundations of Spellweaving", "Ypeomyrphs Grundlagen des Zauberwebens");
    test("Kikubaaqudgha's Secret Knowledge in Simple Steps", "Kikubaaqudghas Geheimwissen in einfachen Schritten");
    test("Olgreb's Hermeneutics Continued", "Olgrebs Hermeneutik fortgesetzt");
    test("Lee's Magic, and How To Use It", "Lees Magie und die Verwendung davon");
    test("Alistair's Advanced Steps in the Arcane Arts", "Alistairs weitere Schritte in den arkanen Künsten");
    test("Borgnjor's Advanced Guide to Theurgy", "Borgnjors fortgeschrittener Leitfaden für Theurgie");
    test("Golubria's Further Steps in the Hermit's Heritage", "Golubrias weitere Schritte im Erbe des Eremiten");
    test("Gell's Further Progress in Thaumaturgy", "Gells weitere Fortschritte in der Thaumaturgie");
    test("Mara's Second Course in Casting", "Maras zweiter Kurs im Zauberwirken");
    test("Eringya's Improving Your Spellcasting", "Eringyas Verbessert Euer Zaubern");
    test("Khufu's Thorough Guide to Wizardry", "Khufus gründlicher Leitfaden für Zauberei");
    test("Xom's Last Secrets of Sorcery", "Xoms letzte Geheimnisse des Hexenwerks");
    test("Asmodeus's Lost Secrets of Witchcraft", "Asmodäus' verlorene Geheimnisse der Hexerei");
    test("Dispater's Arcane Secrets of Witchery", "Dispaters arkane Geheimnisse der Hexerei");
    test("Ereshkigal's Intricacies of Spellcraft", "Ereshkigals Feinheiten der Zauberkunst");
    test("Gloorx Vloq's Mastering Spellweaving", "Gloorx Vloqs Beherrschung des Zauberwebens");
    test("Mnoleg's Handbook of Greatest Risks and Utility", "Mnolegs Handbuch der größten Risiken und des höchsten Nutzens");
    // single school
    test ("Kikubaaqudgha's Collected Works on Unholy Magic", "Kikubaaqudghas gesammelte Werke über unheilige Magie");
    // two schooles
    test("Olgreb's Almanac of Darkness and the Frostbite", "Olgrebs Almanach der Dunkelheit und der Erfrierung");
    test("Vehumet's Almanac of Battle Magic and Calling", "Vehumets Almanach der Kampfmagie und des Rufens");
    test("Golubria's Handbook of Fire and Ice", "Golubrias Handbuch des Feuers und des Eises");
    test("Borgnjor's Compendium of Fiery Attacks", "Borgnjors Kompendium der feurigen Angriffe");

    cout << endl << num_passes << " TESTS PASSED" << endl;
    if (num_fails > 0)
        cout << "**** " << num_fails << " TESTS FAILED ****" << endl;

    return num_fails;
}
