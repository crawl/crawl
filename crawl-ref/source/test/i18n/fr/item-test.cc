#include "AppHdr.h"
#include "fake-main.hpp"
#include "localise.h"
#include "database.h"
#include "initfile.h"
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
    Options.lang_name = "fr";
    SysEnv.crawl_dir = ".";
    setlocale(LC_ALL, "");
    databaseSystemInit(true);
    init_localisation("fr");

    // simple
    test("le fauchon", "the falchion");
    test("la rapière", "the rapier");
    test("un fauchon", "a falchion");
    test("une rapière", "a rapier");
    test("ton fauchon", "your falchion");
    test("ta rapière", "your rapier");
    test("fauchon", "falchion");
    test("rapière", "rapier");
    cout << endl;

    // with inflected adjective
    test("le fauchon raffiné", "the fine falchion");
    test("la rapière raffinée", "the fine rapier");
    test("un fauchon opalescent", "an opalescent falchion");
    test("une rapière opalescente", "an opalescent rapier");
    cout << endl;

    // with uninflected adjective
    test("le fauchon chef d'oeuvre", "the masterwork falchion");
    test("la rapière chef d'oeuvre", "the masterwork rapier");
    test("un fauchon chef d'oeuvre", "a masterwork falchion");
    test("une rapière chef d'oeuvre", "a masterwork rapier");
    cout << endl;

    // with partially inflected adjective
    test("le fauchon incrusté d'agate", "the agate-encrusted falchion");
    test("la rapière incrustée d'agate", "the agate-encrusted rapier");
    test("un fauchon incrusté de diamant", "a diamond-encrusted falchion");
    test("une rapière incrustée de diamant", "a diamond-encrusted rapier");
    cout << endl;

    // with multiple adjectives
    test("la potion bouillonnante bleue", "the bubbling blue potion");
    test("une potion grumeleuse verte", "a lumpy green potion");
    cout << endl;

    // show results summary
    cout << num_passes << " TESTS PASSED" << endl;
    if (num_fails > 0)
        cout << "**** " << num_fails << " TESTS FAILED ****" << endl;

    return num_fails;
}
