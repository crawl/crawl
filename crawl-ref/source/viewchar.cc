#include "AppHdr.h"

#include "viewchar.h"

#include "options.h"
#include "unicode.h"

// For order and meaning of symbols, see dungeon_char_type in enum.h.
static const char32_t dchar_table[NUM_CSET][NUM_DCHAR_TYPES] =
{
    // CSET_DEFAULT
    // It must be limited to stuff present both in CP437 and WGL4.
    {
    //       ▓
        '#', 0x2593, '*', '.', ',', '\'', '+', '^', '>', '<',
    //            ∩       ⌠       ≈
        '#', '_', 0x2229, 0x2320, 0x2248, '8', '{',
#if defined(TARGET_OS_WINDOWS) && !defined(USE_TILE_LOCAL)
    //  ⌂
        0x2302, // CP437 but "optional" in WGL4
#else
    //  ∆
        0x2206, // WGL4 and DEC
#endif
    //       φ
        '0', 0x03C6, ')', '[', '/', '%', '?', '=', '!', '(',
    //                       †       ÷               §     ♣       ©
        ':', '|', '\\', '}', 0x2020, 0xF7, '$', '"', 0xA7, 0x2663, 0xA9,
    //                 ÷
        ' ', '#', '*', 0xF7, 'X', '`', '#',  // space .. explosion
    //  ═       ║       ╔       ╗       ╚       ╝       ─       │
        0x2550, 0x2551, 0x2554, 0x2557, 0x255a, 0x255d, 0x2500, 0x2502, '/',
    //        ┌       ┐       └       ┘            Λ
        '\\', 0x250C, 0x2510, 0x2514, 0x2518, 'V', 0x39B, '>', '<'
    },
    // CSET_ASCII
    {
        '#', '#', '*', '.', ',', '\'', '+', '^', '>', '<',  // wall .. stairs up
        '#', '_', '\\', '}', '~', '8', '{', '{',       // grate .. item detect
        '{', '}', ')', '[', '/', '%', '?', '=', '!', '(',   // orb .. missile
        ':', '|', '|', '}', '%', '%', '$', '"', '0', '7', '^', // book .. teleporter
        ' ', '#', '*', '+', 'X', '`', '#', // space .. explosion
        '-', '|', '+', '+', '+', '+', '-', '|', '/',
        '\\', '*', '*', '*', '*', 'V', '^', '>', '<'
    }
};
COMPILE_CHECK(ARRAYSZ(dchar_table) == NUM_CSET);
COMPILE_CHECK(ARRAYSZ(dchar_table[0]) == NUM_DCHAR_TYPES);
COMPILE_CHECK(ARRAYSZ(dchar_table[1]) == NUM_DCHAR_TYPES);

dungeon_char_type dchar_by_name(const string &name)
{
    static const char *dchar_names[] =
    {
        "wall", "permawall", "wall_magic", "floor", "floor_magic", "door_open",
        "door_closed", "trap", "stairs_down", "stairs_up",
        "grate", "altar", "arch", "fountain", "wavy", "statue",
        "invis_exposed", "item_detected",
        "item_orb", "item_rune", "item_weapon", "item_armour", "item_wand", "item_food",
        "item_scroll", "item_ring", "item_potion", "item_missile", "item_book",
        "item_staff", "item_rod", "item_miscellany", "item_corpse", "item_skeleton",
        "item_gold", "item_amulet", "cloud", "tree", "teleporter",
        "space", "fired_bolt", "fired_zap", "fired_burst", "fired_debug",
        "fired_missile", "explosion", "frame_horiz", "frame_vert",
        "frame_top_left", "frame_top_right", "frame_bottom_left",
        "frame_bottom_right", "draw_horiz", "draw_vert", "draw_slash", "draw_backslash",
        "draw_top_left", "draw_top_right", "draw_bottom_left", "draw_bottom_right",
        "draw_down", "draw_up", "draw_right", "draw_left",
    };
    COMPILE_CHECK(ARRAYSZ(dchar_names) == NUM_DCHAR_TYPES);

    for (unsigned i = 0; i < ARRAYSZ(dchar_names); ++i)
        if (dchar_names[i] == name)
            return dungeon_char_type(i);

    return NUM_DCHAR_TYPES;
}

void init_char_table(char_set_type set)
{
    for (int i = 0; i < NUM_DCHAR_TYPES; i++)
    {
        char32_t c;
        if (Options.cset_override[i])
            c = Options.cset_override[i];
        else
            c = dchar_table[set][i];
        if (wcwidth(c) != 1)
            c = dchar_table[CSET_ASCII][i];
        Options.char_table[i] = c;
    }
}

char32_t dchar_glyph(dungeon_char_type dchar)
{
    if (dchar >= 0 && dchar < NUM_DCHAR_TYPES)
        return Options.char_table[dchar];
    else
        return 0;
}

string stringize_glyph(char32_t glyph)
{
    char buf[5];
    buf[wctoutf8(buf, glyph)] = 0;

    return buf;
}

dungeon_char_type get_feature_dchar(dungeon_feature_type feat)
{
    return get_feature_def(feat).dchar;
}
