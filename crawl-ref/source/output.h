/**
 * @file
 * @brief Functions used to print player related info.
**/

#ifndef OUTPUT_H
#define OUTPUT_H

enum status_redraw_flag_type
{
    REDRAW_HUNGER         = 0x00000001,
    REDRAW_BURDEN         = 0x00000002,
    REDRAW_LINE_1_MASK    = 0x00000003,

    REDRAW_PRAYER         = 0x00000100,
    REDRAW_BREATH         = 0x00000200,
    REDRAW_REPEL_MISSILES = 0x00000400,
    REDRAW_REGENERATION   = 0x00000800,
    REDRAW_INSULATION     = 0x00001000,
    REDRAW_FLY            = 0x00002000,
    REDRAW_INVISIBILITY   = 0x00004000,
    REDRAW_LINE_2_MASK    = 0x00007f00,

    REDRAW_CONFUSION      = 0x00010000,
    REDRAW_POISONED       = 0x00020000,
    REDRAW_LIQUID_FLAMES  = 0x00040000,
    REDRAW_DISEASED       = 0x00080000,
    REDRAW_CONTAMINATED   = 0x00100000,
    REDRAW_SWIFTNESS      = 0x00200000,
    REDRAW_SPEED          = 0x00400000,
    REDRAW_LINE_3_MASK    = 0x007f0000,
};

#ifdef DGL_SIMPLE_MESSAGING
void update_message_status(void);
#endif

void update_turn_count(void);

void print_stats(void);
void print_stats_level(void);
void draw_border(void);

string mpr_monster_list(bool past = false);
int update_monster_pane(void);

const char *equip_slot_to_name(int equip);
int equip_name_to_slot(const char *s);

void print_overview_screen(void);

string dump_overview_screen(bool full_id);

string magic_res_adjective(int mr);
string stealth_desc(int stealth);

#endif
