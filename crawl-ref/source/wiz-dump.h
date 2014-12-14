/**
 * @file
 * @brief Wizmode character dump loading
**/

#ifndef WIZDUMP_H
#define WIZDUMP_H

class chardump_parser
{
public:
    chardump_parser(const string &f)
        : filename(f), in_equipment(false), seen_skills(false) { }

    bool parse();

private:
    bool _parse_from_file(const string &full_filename);

    void _modify_character(const string &line);

    bool _check_skill(const vector<string> &tokens);
    bool _check_stats1(const vector<string> &tokens);
    bool _check_stats2(const vector<string> &tokens);
    bool _check_stats3(const vector<string> &tokens);
    bool _check_char(const vector<string> &tokens);
    bool _check_equipment(const vector<string> &tokens);

    string filename;
    bool in_equipment;
    bool seen_skills;
};

void wizard_load_dump_file();

#endif
