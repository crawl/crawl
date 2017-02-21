/**
 * @file
 * @brief Dumps character info out to the morgue file.
**/

#ifndef CHARDUMP_H
#define CHARDUMP_H

#include <cstdio>
#include <string>

#include "caction-type.h"

enum item_origin_dump_selector
{
    IODS_PRICE            = 0,      // Extra info is provided based on price
    IODS_ARTEFACTS        = 1,      // Extra information on artefacts
    IODS_EGO_ARMOUR       = 2,
    IODS_EGO_WEAPON       = 4,
    IODS_JEWELLERY        = 8,
    IODS_RUNES            = 16,
#if TAG_MAJOR_VERSION == 34
                          //32,
    IODS_STAVES           = 64,
    IODS_BOOKS            = 128,
#else
    IODS_STAVES           = 32,
    IODS_BOOKS            = 64,
#endif
    IODS_EVERYTHING       = 0xFF,
};

class scorefile_entry;
string morgue_directory();
bool dump_char(const string &fname, bool quiet = false, bool full_id = false,
               const scorefile_entry *se = nullptr);
void dump_map(const char* fname, bool debug = false, bool dist = false);
void dump_map(FILE *fp, bool debug = false, bool dist = false);
void display_notes();
void display_char_dump();
string chardump_desc(const item_def& item);
const char *hunger_level();

#ifdef DGL_WHEREIS
void whereis_record(const char *status = "active");
#endif

void record_turn_timestamp();

enum evoc_type
{
    EVOC_WAND,
#if TAG_MAJOR_VERSION == 34
    EVOC_ROD,
#endif
    EVOC_DECK,
#if TAG_MAJOR_VERSION == 34
    EVOC_MISC,
    EVOC_BUGGY_TOME,
#endif
};

enum dodge_type    // subtype for counted actions
{
    DODGE_EVASION,
    DODGE_DEFLECT,
    NUM_DODGES
};

enum block_type    // auxtype for counted actions
{
    BLOCK_OTHER,
    BLOCK_REFLECT,
    NUM_BLOCKS
};

void count_action(caction_type type, int subtype = -32768,
                  int auxtype = -32768);
pair<int, int> caction_extract_types(int compound_subtype);
int caction_compound(int subtype = -32768, int auxtype = -32768);

#endif
