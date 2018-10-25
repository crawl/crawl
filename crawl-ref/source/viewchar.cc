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
        // wall .. altar
         '#', U'▓',  '*',  '.',  ',', '\'',  '+',  '^',  '>',  '<', '#',  '_',
        // arch .. invis_exposed
        U'∩', U'⌠', U'≈',  '8',  '{',
#if defined(TARGET_OS_WINDOWS) && !defined(USE_TILE_LOCAL)
        U'⌂', // CP437 but "optional" in WGL4
#else
        U'∆', // WGL4 and DEC
#endif
         '0', U'φ',  ')',  '[',  '/',  '%',  '?',  '=',  '!',  '(', ':',  '|',
#if TAG_MAJOR_VERSION == 34
         '\\',
#endif
         '}', U'†', U'÷',  '$',  '"', U'§', U'♣',
#if TAG_MAJOR_VERSION == 34
        U'©',
#endif
        // transporter .. frame_top_left
        U'©', U'©',  ' ',  '#',  '*', U'÷',  'X',  '`',  '#', U'═', U'║', U'╔',
        // frame_top_right .. draw_down
        U'╗', U'╚', U'╝', U'─', U'│',  '/', '\\', U'┌', U'┐', U'└', U'┘',  'V',
        // draw_up .. draw_left
        U'Λ',  '>',  '<',
    },
    // CSET_ASCII
    {
        // wall .. altar
         '#',  '#',  '*',  '.',  ',', '\'',  '+',  '^',  '>',  '<',  '#',  '_',
        // arch .. item_food
        '\\',  '}',  '~',  '8',  '{',  '{',  '{',  '}',  ')',  '[',  '/',  '%',
        // item_scroll .. item_amulet
         '?',  '=',  '!',  '(',  ':',  '|',  '|',  '}',  '%',  '%',  '$',  '"',
        // cloud .. tree
         '0',  '7',
#if TAG_MAJOR_VERSION == 34
         '^',
#endif
        // transporter .. frame_top_left
         '^',  '^',  ' ',  '#',  '*',  '+',  'X',  '`',  '#',  '-',  '|',  '+',
        // frame_top_right .. draw_down
         '+',  '+',  '+',  '-',  '|',  '/', '\\',  '*',  '*',  '*',  '*',  'V',
        // draw_up .. draw_left
         '^',  '>',  '<'
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
        "door_closed", "trap", "stairs_down", "stairs_up", "grate", "altar",
        "arch", "fountain", "wavy", "statue", "invis_exposed", "item_detected",
        "item_orb", "item_rune", "item_weapon", "item_armour", "item_wand",
        "item_food", "item_scroll", "item_ring", "item_potion", "item_missile",
        "item_book", "item_staff",
#if TAG_MAJOR_VERSION == 34
        "item_rod",
#endif
        "item_miscellany", "item_corpse", "item_skeleton", "item_gold",
        "item_amulet", "cloud", "tree",
#if TAG_MAJOR_VERSION == 34
        "teleporter",
#endif
        "transporter", "transporter_landing", "space", "fired_bolt",
        "fired_zap", "fired_burst", "fired_debug", "fired_missile",
        "explosion", "frame_horiz", "frame_vert", "frame_top_left",
        "frame_top_right", "frame_bottom_left", "frame_bottom_right",
        "draw_horiz", "draw_vert", "draw_slash", "draw_backslash",
        "draw_top_left", "draw_top_right", "draw_bottom_left",
        "draw_bottom_right", "draw_down", "draw_up", "draw_right", "draw_left",
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
