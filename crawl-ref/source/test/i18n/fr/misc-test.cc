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
    cout << "   expected: \"" << expected << "\"" << endl;
}

int main()
{
    Options.lang_name = "fr";
    SysEnv.crawl_dir = ".";
    setlocale(LC_ALL, "");
    databaseSystemInit(true);
    init_localisation("fr");

    string msg;

    msg = localise("%s is %s.", "the gray horse", "happy");
    msg = uppercase_first(msg);
    show_result(msg, "Le cheval gris est heureux.");

    msg = localise("%s is %s.", "the gray cow", "happy");
    msg = uppercase_first(msg);
    show_result(msg, "La vache grise est heureuse.");

    msg = localise("the runed translucent gate");
    show_result(msg, "le portail translucide décoré de runes");

    msg = localise("a runed translucent door");
    show_result(msg, "une porte translucide décorée de runes");

    msg = localise("large closed door, spattered with blood");
    show_result(msg, "grande porte fermée, éclaboussée de sang");
}
