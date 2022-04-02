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
    Options.lang_name = "ru";
    SysEnv.crawl_dir = ".";
    setlocale(LC_ALL, "");
    databaseSystemInit(true);
    init_localisation("ru");

    string msg;

    msg = localise("4 horses");
    show_result(msg, "4 лошади");

    msg = localise("5 horses");
    show_result(msg, "5 лошадей");
}
