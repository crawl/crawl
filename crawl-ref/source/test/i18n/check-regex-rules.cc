#include "AppHdr.h"
#include "fake-main.hpp"
#include "localise.h"
#include "database.h"
#include "initfile.h"
#include "options.h"
#include "stringutil.h"

#include <iostream>
#include <fstream>
#include <string>
#include <codecvt>
#include <regex>
#include <vector>

using namespace std;

static bool is_changed(const string& old_value, const string& new_value)
{
    if (new_value == old_value)
        return false;
    else if (starts_with(old_value, "{"))
    {
        // remove gender hint from old value
        size_t pos = old_value.find('}');
        if (pos != string::npos)
        {
            string trimmed_old_value = old_value.substr(pos+1);
            if (new_value == trimmed_old_value)
                return false;
        }
    }
    return true;
}

static void _process_input_file(const string& context, const string& filename, bool show_all)
{

    fstream infile;
    infile.open(filename, ios::in);
    if (!infile.is_open()) {
        return;
    }

    string context_prefix = string("{") + context + "}";
    string line;
    string key;
    string old_value;
    string last;
    while (getline(infile, line))
    {
        line = trim_string(line);
        if (line.empty() || line[0] == '#')
        {
            cout << line << endl;
            last = line;
        }
        else if (line == "%%%%")
        {
            string value = localise_contextual(context, key);
            if (!key.empty() && (show_all || is_changed(old_value, value)))
            {
                if (!show_all)
                    cout << context_prefix;
                cout << key << endl;
                cout << value << endl;
                cout << "%%%%" << endl;
            }
            else if (last != line)
                cout << line << endl;
            key.clear();
            value.clear();
            old_value.clear();
            last = line;
        }
        else if (key.empty())
            key = line;
        else
        {
            if (!old_value.empty())
                old_value += "\n";
            old_value += line;
        }
    }

    if (!key.empty())
    {
        string value = localise_contextual(context, key);
        if (show_all || is_changed(old_value, value))
        {
            if (!show_all)
                cout << context_prefix;
            cout << key << endl;
            cout << value << endl;
            cout << "%%%%" << endl;
        }
    }

    infile.close();

}

int main(int argc, char *argv[])
{
    // TODO: Make this changeable
    Options.lang_name = "de";
    SysEnv.crawl_dir = ".";
    setlocale(LC_ALL, "");
    databaseSystemInit(true);
    init_localisation(Options.lang_name);

    // show all or only the different ones?
    bool show_all = false;
    string context, infile_name;

    bool arg_error = false;
    for (int i = 1; i < argc; i++)
    {
        string arg(argv[i]);
        if (arg == "--show-all")
            show_all = true;
        else if (context.empty())
            context = arg;
        else if (infile_name.empty())
            infile_name = arg;
        else
            arg_error = true;
    }

    if (infile_name.empty())
        arg_error = true;

    if (arg_error)
    {
        cerr << "Usage: check-regex-rules <context> <infile> [--show-all]" << endl;
        cerr << "  where --show-all means output all strings (default is to only output the ones that change)" << endl;
        return 1;
    }

    _process_input_file(context, infile_name, show_all);

    return 0;
}
