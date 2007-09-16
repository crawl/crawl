/*
 *  File:       output.cc
 *  Summary:    Functions used to print player related info.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *               <1>     -/--/--        LRH             Created
 */


#ifndef OUTPUT_H
#define OUTPUT_H

#include "format.h"

enum status_redraw_flag_type
{
    REDRAW_HUNGER         = 0x00000001,
    REDRAW_BURDEN         = 0x00000002,
    REDRAW_LINE_1_MASK    = 0x00000003,

    REDRAW_PRAYER         = 0x00000100,
    REDRAW_REPEL_UNDEAD   = 0x00000200,
    REDRAW_BREATH         = 0x00000400,
    REDRAW_REPEL_MISSILES = 0x00000800,
    REDRAW_REGENERATION   = 0x00001000,
    REDRAW_INSULATION     = 0x00002000,
    REDRAW_FLY            = 0x00004000,
    REDRAW_INVISIBILITY   = 0x00008000,
    REDRAW_LINE_2_MASK    = 0x0000ff00,

    REDRAW_CONFUSION      = 0x00010000,
    REDRAW_POISONED       = 0x00020000,
    REDRAW_LIQUID_FLAMES  = 0x00040000,
    REDRAW_DISEASED       = 0x00080000,
    REDRAW_CONTAMINATED   = 0x00100000,
    REDRAW_SWIFTNESS      = 0x00200000,
    REDRAW_SPEED          = 0x00400000,
    REDRAW_LINE_3_MASK    = 0x007f0000
};

#ifdef DGL_SIMPLE_MESSAGING
void update_message_status();
#endif

void update_turn_count();

void print_stats(void);

std::vector<formatted_string> get_full_detail(bool calc_unid, long score = -1);

const char *equip_slot_to_name(int equip);

int equip_name_to_slot(const char *s);

const char *equip_slot_to_name(int equip);

void print_overview_screen(void);

#endif
