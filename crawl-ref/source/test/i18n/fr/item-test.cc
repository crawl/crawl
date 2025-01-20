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
    test("la masse", "the mace");
    test("un fauchon", "a falchion");
    test("une masse", "a mace");
    test("ton fauchon", "your falchion");
    test("ta masse", "your mace");
    test("fauchon", "falchion");
    test("masse", "mace");
    cout << endl;

    // with inflected adjective
    test("le fauchon raffiné", "the fine falchion");
    test("la masse raffinée", "the fine mace");
    test("un fauchon opalescent", "an opalescent falchion");
    test("une masse opalescente", "an opalescent mace");
    cout << endl;

    // with uninflected adjective
    test("le fauchon de maître", "the masterwork falchion");
    test("la masse de maître", "the masterwork mace");
    test("un fauchon de maître", "a masterwork falchion");
    test("une masse de maître", "a masterwork mace");
    cout << endl;

    // with partially inflected adjective
    test("le fauchon incrusté d'agate", "the agate-encrusted falchion");
    test("la masse incrustée d'agate", "the agate-encrusted mace");
    test("un fauchon incrusté de diamant", "a diamond-encrusted falchion");
    test("une masse incrustée de diamant", "a diamond-encrusted mace");
    cout << endl;

    // with multiple adjectives
    test("la potion bleue bouillonnante", "the bubbling blue potion");
    test("une potion verte grumeleuse", "a lumpy green potion");
    test("potion rouge avec des sédiments", "sedimented red potion");
    cout << endl;

    // show results summary
    cout << num_passes << " TESTS PASSED" << endl;
    if (num_fails > 0)
        cout << "**** " << num_fails << " TESTS FAILED ****" << endl;

    return num_fails;
}
