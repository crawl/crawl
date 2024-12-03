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

static void test(const string& expected, const char* fmt...)
{
    va_list args;
    va_start(args, fmt);

    string actual = vlocalise(fmt, args);
    show_result(actual, expected);

    va_end(args);
}

int main()
{
    Options.lang_name = "de";
    SysEnv.crawl_dir = ".";
    setlocale(LC_ALL, "");
    databaseSystemInit(true);
    init_localisation("de");

    // simple masculine
    test("der Ork", "the orc");
    test("ein Ork", "an orc");
    test("dein Ork", "your orc");
    test("Ork", "orc");
    test("Du triffst den Ork", "You hit %s", "the orc");
    test("Du kollidierst mit dem Ork!", "You collide with %s!", "the orc");
    test("Du blockierst den Angriff des Orks.", "You block %s attack.", "the orc's");
    test("Die Wunden des Orks heilen von selbst!", "%s's wounds heal themselves!", "the orc");
    cout << endl;

    // simple feminine
    test("die Königspython", "the ball python");
    test("eine Königspython", "a ball python");
    test("deine Königspython", "your ball python");
    test("Königspython", "ball python");
    test("Du triffst die Königspython", "You hit %s", "the ball python");
    test("Du kollidierst mit der Königspython!", "You collide with %s!", "the ball python");
    test("Du blockierst den Angriff der Königspython.", "You block %s attack.", "the ball python's");
    test("Die Wunden der Königspython heilen von selbst!", "%s's wounds heal themselves!", "the ball python");
    cout << endl;

    // simple neuter
    test("das Skelett", "the skeleton");
    test("ein Skelett", "a skeleton");
    test("dein Skelett", "your skeleton");
    test("Skelett", "skeleton");
    test("Du triffst das Skelett", "You hit %s", "the skeleton");
    test("Du kollidierst mit dem Skelett!", "You collide with %s!", "the skeleton");
    test("Du blockierst den Angriff des Skeletts.", "You block %s attack.", "the skeleton's");
    test("Die Wunden des Skeletts heilen von selbst!", "%s's wounds heal themselves!", "the skeleton");
     cout << endl;

    // masculine with adjective
    test("der hilflose Ork", "the helpless orc");
    test("ein hilfloser Ork", "a helpless orc");
    test("dein hilfloser Ork", "your helpless orc");
    test("hilfloser Ork", "helpless orc");
    test("Du triffst den hilflosen Ork", "You hit %s", "the helpless orc");
    test("Du kollidierst mit dem hilflosen Ork!", "You collide with %s!", "the helpless orc");
    test("Du blockierst den Angriff des spektralen Orks.", "You block %s attack.", "the spectral orc's");
    test("Die Wunden des spektralen Orks heilen von selbst!", "%s's wounds heal themselves!", "the spectral orc");
    cout << endl;

    // feminine with adjective
    test("die hilflose Königspython", "the helpless ball python");
    test("eine hilflose Königspython", "a helpless ball python");
    test("deine hilflose Königspython", "your helpless ball python");
    test("hilflose Königspython", "helpless ball python");
    test("Du triffst die hilflose Königspython", "You hit %s", "the helpless ball python");
    test("Du kollidierst mit der hilflosen Königspython!", "You collide with %s!", "the helpless ball python");
    test("Du blockierst den Angriff der spektralen Königspython.", "You block %s attack.", "the spectral ball python's");
    test("Die Wunden der spektralen Königspython heilen von selbst!", "%s's wounds heal themselves!", "the spectral ball python");
    cout << endl;

    // neuter with adjective
    test("das hilflose Skelett", "the helpless skeleton");
    test("ein hilfloses Skelett", "a helpless skeleton");
    test("dein hilfloses Skelett", "your helpless skeleton");
    test("hilfloses Skelett", "helpless skeleton");
    test("Du triffst das hilflose Skelett", "You hit %s", "the helpless skeleton");
    test("Du kollidierst mit dem hilflosen Skelett!", "You collide with %s!", "the helpless skeleton");
    test("Du blockierst den Angriff des spektralen Schweins.", "You block %s attack.", "the spectral hog's");
    test("Die Wunden des spektralen Schweins heilen von selbst!", "%s's wounds heal themselves!", "the spectral hog");
    cout << endl;

    // masculine with weak declension
    test("der Feuerdrache", "the fire dragon");
    test("ein Feuerdrache", "a fire dragon");
    test("dein Feuerdrache", "your fire dragon");
    test("Feuerdrache", "fire dragon");
    test("der hilflose Feuerdrache", "the helpless fire dragon");
    test("ein hilfloser Feuerdrache", "a helpless fire dragon");
    test("dein hilfloser Feuerdrache", "your helpless fire dragon");
    test("hilfloser Feuerdrache", "helpless fire dragon");
    test("Du triffst den Feuerdrachen", "You hit %s", "the fire dragon");
    test("Du triffst den hilflosen Feuerdrachen", "You hit %s", "the helpless fire dragon");
    test("Du kollidierst mit dem Feuerdrachen!", "You collide with %s!", "the fire dragon");
    test("Du kollidierst mit dem hilflosen Feuerdrachen!", "You collide with %s!", "the helpless fire dragon");
    test("Du blockierst den Angriff des spektralen Feuerdrachen.", "You block %s attack.", "the spectral fire dragon's");
    test("Die Wunden des spektralen Feuerdrachen heilen von selbst!", "%s's wounds heal themselves!", "the spectral fire dragon");
    cout << endl;

    // unique with simple name
    test("Natascha", "Natasha");
    test("Du triffst Natascha", "You hit %s", "Natasha");
    test("Du triffst die hilflose Natascha", "You hit %s", "the helpless Natasha");
    test("Du kollidierst mit Natascha!", "You collide with %s!", "Natascha");
    test("Du kollidierst mit der hilflosen Natascha!", "You collide with %s!", "the helpless Natasha");
    test("Du blockierst den Angriff Nataschas.", "You block %s attack.", "Natasha's");
    test("Die Wunden Nataschas heilen von selbst!", "%s's wounds heal themselves!", "Natasha");
    cout << endl;

    // unique with definite article in name
    test("die Zauberin", "the Enchantress");
    test("Zauberin", "Enchantress");
    test("Du triffst die Zauberin", "You hit %s", "the Enchantress");
    test("Du triffst die hilflose Zauberin", "You hit %s", "the helpless Enchantress");
    test("Du kollidierst mit der Zauberin!", "You collide with %s!", "the Enchantress");
    test("Du kollidierst mit der hilflosen Zauberin!", "You collide with %s!", "the helpless Enchantress");
    test("Du blockierst den Angriff der Zauberin.", "You block %s attack.", "the Enchantress's");
    test("Die Wunden der Zauberin heilen von selbst!", "%s's wounds heal themselves!", "the Enchantress");
    cout << endl;

    // unique with weak declension
    test("Prinz Ribbit", "Prince Ribbit");
    test("der hilflose Prinz Ribbit", "the helpless Prince Ribbit");
    test("hilfloser Prinz Ribbit", "helpless Prince Ribbit");
    test("Du triffst Prinzen Ribbit", "You hit %s", "Prince Ribbit");
    test("Du triffst den hilflosen Prinzen Ribbit", "You hit %s", "the helpless Prince Ribbit");
    test("Du kollidierst mit Prinzen Ribbit!", "You collide with %s!", "Prince Ribbit");
    test("Du kollidierst mit dem hilflosen Prinzen Ribbit!", "You collide with %s!", "the helpless Prince Ribbit");
    test("Du blockierst den Angriff Prinzen Ribbits.", "You block %s attack.", "Prince Ribbit's");
    test("Die Wunden Prinzen Ribbits heilen von selbst!", "%s's wounds heal themselves!", "Prince Ribbit");
    cout << endl;

    // unique with a capitalised adjective in the name
    test("Verrückter Yiuf", "Crazy Yiuf");
    test("der hilflose Verrückte Yiuf", "the helpless Crazy Yiuf");
    test("hilfloser Verrückter Yiuf", "helpless Crazy Yiuf");
    test("Du triffst Verrückten Yiuf", "You hit %s", "Crazy Yiuf");
    test("Du triffst den hilflosen Verrückten Yiuf", "You hit %s", "the helpless Crazy Yiuf");
    test("Du kollidierst mit Verrücktem Yiuf!", "You collide with %s!", "Crazy Yiuf");
    test("Du kollidierst mit dem hilflosen Verrückten Yiuf!", "You collide with %s!", "the helpless Crazy Yiuf");
    test("Du blockierst den Angriff Verrückten Yiufs.", "You block %s attack.", "Crazy Yiuf's");
    test("Die Wunden Verrückten Yiufs heilen von selbst!", "%s's wounds heal themselves!", "Crazy Yiuf");
    cout << endl;

    // another unique with a capitalised adjective in the name
    test("die Lernäische Hydra", "the Lernaean hydra");
    test("die hilflose Lernäische Hydra", "the helpless Lernaean hydra");
    test("hilflose Lernäische Hydra", "helpless Lernaean hydra");
    test("die 27-köpfige Lernäische Hydra", "the 27-headed Lernaean hydra");
    test("die hilflose 27-köpfige Lernäische Hydra", "the helpless 27-headed Lernaean hydra");
    test("Du triffst die hilflose 27-köpfige Lernäische Hydra", "You hit %s", "the helpless 27-headed Lernaean hydra");
    test("Du kollidierst mit der hilflosen 27-köpfigen Lernäischen Hydra!", "You collide with %s!", "the helpless 27-headed Lernaean hydra");
    test("Du blockierst den Angriff der 27-köpfigen Lernäischen Hydra.", "You block %s attack.", "the 27-headed Lernaean hydra's");
    test("Die Wunden der 27-köpfigen Lernäischen Hydra heilen von selbst!", "%s's wounds heal themselves!", "the 27-headed Lernaean hydra");
    cout << endl;

    // unique with "of" in the English name (this could mess things up)
    test("die Höllenschlange", "the Serpent of Hell");
    test("Höllenschlange", "Serpent of Hell");
    test("die hilflose Höllenschlange", "the helpless Serpent of Hell");
    test("Du triffst die Höllenschlange", "You hit %s", "the Serpent of Hell");
    test("Du triffst die hilflose Höllenschlange", "You hit %s", "the helpless Serpent of Hell");
    test("Du kollidierst mit der Höllenschlange!", "You collide with %s!", "the Serpent of Hell");
    test("Du kollidierst mit der hilflosen Höllenschlange!", "You collide with %s!", "the helpless Serpent of Hell");
    test("Du blockierst den Angriff der Höllenschlange.", "You block %s attack.", "the Serpent of Hell's");
    test("Die Wunden der Höllenschlange heilen von selbst!", "%s's wounds heal themselves!", "the Serpent of Hell");
    cout << endl;

    // unique with definite article in the middle
    test("Blork der Ork", "Blork the orc");
    // these ones currently fail
    //test("der hilflose Blork der Ork", "the helpless Blork the orc");
    //test("hilfloser Blork der Ork", "helpless Blork the orc");
    test("Du triffst Blork den Ork", "You hit %s", "Blork the orc");
    test("Du kollidierst mit Blork dem Ork!", "You collide with %s!", "Blork the orc");
    test("Du blockierst den Angriff Blork des Orks.", "You block %s attack.", "Blork the orc's");
    test("Die Wunden Blork des Orks heilen von selbst!", "%s's wounds heal themselves!", "Blork the orc");
    cout << endl;

    // named ally
    test("Boghold der Ork-Warlord", "Boghold the orc warlord");
    //test("der hilflose Boghold der Ork-Warlord", "the helpless Boghold the orc warlord");
    cout << endl;

    // derived monsters (should have the gender of the derived monster, not the original)
    test("Du triffst den hilflosen Meervolk-Zombie", "You hit %s", "the helpless merfolk zombie");
    test("Du triffst das hilflose Ork-Skelett", "You hit %s", "the helpless orc skeleton");
    test("Du triffst das hilflose Ork-Simulacrum", "You hit %s", "the helpless orc simulacrum");
    test("eine Ork-förmige Salzsäule", "an orc shaped pillar of salt");
    test("Du siehst hier einen Mennas-förmigen Eisblock.", "You see here %s.", "a Mennas shaped block of ice");
    test("Du siehst hier einen Tarantella-förmigen Eisblock.", "You see here %s.", "a tarantella shaped block of ice");

    // show results summary
    cout << num_passes << " TESTS PASSED" << endl;
    if (num_fails > 0)
        cout << "**** " << num_fails << " TESTS FAILED ****" << endl;

    return num_fails;
}
