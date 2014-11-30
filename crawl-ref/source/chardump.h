/**
 * @file
 * @brief Dumps character info out to the morgue file.
**/

#ifndef CHARDUMP_H
#define CHARDUMP_H

#include <cstdio>
#include <string>

enum item_origin_dump_selector
{
    IODS_PRICE            = 0,      // Extra info is provided based on price
    IODS_ARTEFACTS        = 1,      // Extra information on artefacts
    IODS_EGO_ARMOUR       = 2,
    IODS_EGO_WEAPON       = 4,
    IODS_JEWELLERY        = 8,
    IODS_RUNES            = 16,
    IODS_RODS             = 32,
    IODS_STAVES           = 64,
    IODS_BOOKS            = 128,
    IODS_EVERYTHING       = 0xFF,
};

class scorefile_entry;
string morgue_directory();
bool dump_char(const string &fname, bool quiet = false, bool full_id = false,
               const scorefile_entry *se = nullptr);
void dump_map(const char* fname, bool debug = false, bool dist = false);
void dump_map(FILE *fp, bool debug = false, bool dist = false);
void display_notes();
string munge_description(string inStr);
const char *hunger_level();

#ifdef DGL_WHEREIS
void whereis_record(const char *status = "active");
#endif

void record_turn_timestamp();

#endif
