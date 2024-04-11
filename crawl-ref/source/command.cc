/**
 * @file
 * @brief Misc commands.
**/

#include "AppHdr.h"

#include "command.h"

#include <cctype>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <iomanip>
#include <sstream>

#include "chardump.h"
#include "database.h"
#include "describe.h"
#include "env.h"
#include "files.h"
#include "hints.h"
#include "invent.h"
#include "item-prop.h"
#include "items.h"
#include "libutil.h"
#include "localise.h"
#include "lookup-help.h"
#include "macro.h"
#include "message.h"
#include "prompt.h"
#include "scroller.h"
#include "showsymb.h"
#include "state.h"
#include "stringutil.h"
#include "syscalls.h"
#include "unicode.h"
#include "version.h"
#include "viewchar.h"

using namespace ui;

#ifdef USE_TILE
 #include "rltiles/tiledef-gui.h"
#endif

// @noloc section start (diagnostic stuff)

static const char *features[] =
{
#ifdef CLUA_BINDINGS
    "Lua user scripts",
#endif

#ifdef USE_TILE_LOCAL
    "Tile support",
#endif

#ifdef USE_TILE_WEB
    "Web Tile support",
#endif

#ifdef WIZARD
    "Wizard mode",
#endif

#ifdef DEBUG
    "Debug mode",
#endif

#if defined(REGEX_POSIX)
    "POSIX regexps",
#elif defined(REGEX_PCRE)
    "PCRE regexps",
#else
    "Glob patterns",
#endif

#if defined(USE_SOUND) && defined(SOUND_BACKEND)
    SOUND_BACKEND,
#endif

#ifdef DGL_MILESTONES
    "Milestones",
#endif
};

static string _get_version_information()
{
    string result = string("This is <w>" CRAWL " ") + Version::Long + "</w>";
    return result;
}

static string _get_version_features()
{
    string result;
    if (crawl_state.need_save
#ifdef DGAMELAUNCH
        && (you.wizard || crawl_state.type == GAME_TYPE_CUSTOM_SEED)
#endif
       )
    {
        if (you.fully_seeded)
        {
            result += seed_description();
            if (Version::history_size() > 1)
                result += " (seed may be affected by game upgrades)";
        }
        else
            result += "Game is not seeded.";
        result += "\n\n";
    }
    if (Version::history_size() > 1)
    {
        result += "Version history for your current game:\n";
        result += Version::history();
        result += "\n\n";
    }

    result += "<w>Features</w>\n"
                 "--------\n";

    for (const char *feature : features)
    {
        result += " * ";
        result += feature;
        result += "\n";
    }

    return result;
}

static string _get_version_changes()
{
    // Attempts to print "Highlights" of the latest version.
    FILE* fp = fopen_u(datafile_path("changelog.txt", false).c_str(), "r");
    if (!fp)
        return "";

    string result = "";
    string help;
    char buf[200];
    bool start = false;
    while (fgets(buf, sizeof buf, fp))
    {
        // Remove trailing spaces.
        for (int i = strlen(buf) - 1; i >= 0; i++)
        {
            if (isspace(buf[i]))
                buf[i] = 0;
            else
                break;
        }
        help = buf;

        // Look for version headings
        if (starts_with(help, "Stone Soup "))
        {
            // Stop if this is for an older major version; otherwise, highlight
            if (help.find(string("Stone Soup ")+Version::Major) == string::npos)
                break;
            else
                goto highlight;
        }

        if (help.find("Highlights") != string::npos)
        {
        highlight:
            // Highlight the Highlights, so to speak.
            result += "<w>" + help + "</w>\n";
            // And start printing from now on.
            start = true;
        }
        else if (!start)
            continue;
        else
        {
            result += buf;
            result += "\n";
        }
    }
    fclose(fp);

    // Did we ever get to print the Highlights?
    if (start)
    {
        result.erase(1+result.find_last_not_of('\n'));
        result += "\n\n";
        result += "For earlier changes, see changelog.txt "
                  "in the docs/ directory.";
    }
    else
    {
        result += "For a list of changes, see changelog.txt in the docs/ "
                  "directory.";
    }

    return result;
}
// @noloc section end (diagnostic stuff)

//#define DEBUG_FILES
static void _print_version()
{
    const string info = _get_version_information(),
          feats = _get_version_features(),
          changes = _get_version_changes();

    auto vbox = make_shared<Box>(Widget::VERT);

#ifdef USE_TILE_LOCAL
    vbox->max_size().width = tiles.get_crt_font()->char_width()*80;
#endif

    auto title_hbox = make_shared<Box>(Widget::HORZ);
#ifdef USE_TILE
    auto icon = make_shared<Image>();
    icon->set_tile(tile_def(TILEG_STARTUP_STONESOUP));
    title_hbox->add_child(move(icon));
#endif

    auto title = make_shared<Text>(formatted_string::parse_string(info));
    title->set_margin_for_sdl(0, 0, 0, 10);
    title_hbox->add_child(move(title));

    title_hbox->set_cross_alignment(Widget::CENTER);
    title_hbox->set_margin_for_crt(0, 0, 1, 0);
    title_hbox->set_margin_for_sdl(0, 0, 20, 0);
    vbox->add_child(move(title_hbox));

    auto scroller = make_shared<Scroller>();
    auto content = formatted_string::parse_string(feats + "\n\n" + changes);
    auto text = make_shared<Text>(move(content));
    text->set_wrap_text(true);
    scroller->set_child(move(text));
    vbox->add_child(scroller);

    auto popup = make_shared<ui::Popup>(vbox);

    bool done = false;
    popup->on_keydown_event([&](const KeyEvent& ev) {
        done = !scroller->on_event(ev);
        return true;
    });

#ifdef USE_TILE_WEB
    tiles.json_open_object();
    tiles.json_write_string("information", info);
    tiles.json_write_string("features", feats);
    tiles.json_write_string("changes", changes);
    tiles.push_ui_layout("version", 0);
    popup->on_layout_pop([](){ tiles.pop_ui_layout(); });
#endif

    ui::run_layout(move(popup), done);
}

const string _indent = "    ";

void list_armour()
{
    ostringstream estr;
    for (int j = EQ_MIN_ARMOUR; j <= EQ_MAX_ARMOUR; j++)
    {
        const equipment_type i = static_cast<equipment_type>(j);
        const int armour_id = you.equip[i];
        int       colour    = MSGCOL_BLACK;

        estr.str("");
        estr.clear();

        string slot = ((i == EQ_CLOAK)       ? "Cloak" :
                       (i == EQ_HELMET)      ? "Helmet" :
                       (i == EQ_GLOVES)      ? "Gloves" :
                       (i == EQ_SHIELD)      ? "Shield" :
                       (i == EQ_BODY_ARMOUR) ? "Armour" :
                       (i == EQ_BOOTS)       ?
                         (you.wear_barding() ? "Barding"
                                             : "Boots")
                                             : "unknown");

        estr << setw(8) << left << localise(slot) << ": ";

        if (you_can_wear(i) == MB_FALSE)
            estr << _indent << localise("(unavailable)");
        else if (you_can_wear(i, true) == MB_FALSE)
            estr << _indent << localise("(currently unavailable)");
        else if (armour_id != -1)
        {
            estr << localise(you.inv[armour_id].name(DESC_INVENTORY));
            colour = menu_colour(estr.str(), item_prefix(you.inv[armour_id]),
                                 "equip");
        }
        else if (you_can_wear(i) == MB_MAYBE)
            estr << _indent << localise("(restricted)");
        else
            estr << _indent << localise("none");

        if (colour == MSGCOL_BLACK)
            colour = menu_colour(estr.str(), "", "equip");

        mpr_nolocalise(MSGCH_EQUIPMENT, colour, estr.str());
    }
}

void list_jewellery()
{
    string jstr;
    int cols = get_number_of_cols() - 1;
    bool split = species::arm_count(you.species) > 2 && cols > 84;

    for (int j = EQ_LEFT_RING; j < NUM_EQUIP; j++)
    {
        const equipment_type i = static_cast<equipment_type>(j);
        if (!you_can_wear(i))
            continue;

        const int jewellery_id = you.equip[i];
        int       colour       = MSGCOL_BLACK;

        string slot =
                 (i == EQ_LEFT_RING)   ? "Left ring" :
                 (i == EQ_RIGHT_RING)  ? "Right ring" :
                 (i == EQ_AMULET)      ? "Amulet" :
                 (i == EQ_RING_ONE)    ? "1st ring" :
                 (i == EQ_RING_TWO)    ? "2nd ring" :
                 (i == EQ_RING_THREE)  ? "3rd ring" :
                 (i == EQ_RING_FOUR)   ? "4th ring" :
                 (i == EQ_RING_FIVE)   ? "5th ring" :
                 (i == EQ_RING_SIX)    ? "6th ring" :
                 (i == EQ_RING_SEVEN)  ? "7th ring" :
                 (i == EQ_RING_EIGHT)  ? "8th ring" :
                 (i == EQ_RING_AMULET) ? "Amulet ring"
                                       : "unknown";
        slot = localise(slot);

        string item;
        if (you_can_wear(i, true) == MB_FALSE)
            item = _indent + localise("(currently unavailable)");
        else if (jewellery_id != -1)
        {
            item = localise(you.inv[jewellery_id].name(DESC_INVENTORY));
            string prefix = item_prefix(you.inv[jewellery_id]);
            colour = menu_colour(item, prefix, "equip");
        }
        else
            item = _indent + localise("none");

        if (colour == MSGCOL_BLACK)
            colour = menu_colour(item, "", "equip");

        item = chop_string(make_stringf("%-*s: %s",
                                        split ? cols > 96 ? 9 : 8 : 11,
                                        slot.c_str(), item.c_str()),
                           split && i > EQ_AMULET ? (cols - 1) / 2 : cols);
        item = colour_string(item, colour);

        // doesn't handle arbitrary arm counts
        if (i == EQ_RING_SEVEN && you.arm_count() == 7)
            mpr_nolocalise(MSGCH_EQUIPMENT, item);
        else if (split && i > EQ_AMULET && (i - EQ_AMULET) % 2)
            jstr = item + " ";
        else
            mprf_nolocalise(MSGCH_EQUIPMENT, "%s%s", jstr.c_str(), item.c_str());
    }
}

struct help_file
{
    const char* name;
    int hotkey;
    bool auto_hotkey;
};

static help_file help_files[] =
{
    { "crawl_manual.txt",  '*', true },
    { "aptitudes.txt",     '%', false },
    { "quickstart.txt",     '^', false },
    { "macros_guide.txt",  '~', false },
    { "options_guide.txt", '&', false },
#ifdef USE_TILE_LOCAL
    { "tiles_help.txt",    't', false },
#endif
    { nullptr, 0, false }
};

// Reads all questions from database/FAQ.txt, outputs them in the form of
// a selectable menu and prints the corresponding answer for a chosen question.
static void _handle_FAQ()
{
    vector<string> question_keys = getAllFAQKeys();
    if (question_keys.empty())
    {
        mpr("No questions found in FAQ! Please submit a bug report!");
        return;
    }
    Menu FAQmenu(MF_SINGLESELECT | MF_ANYPRINTABLE | MF_ALLOW_FORMATTING);
    MenuEntry *title = new MenuEntry(localise("Frequently Asked Questions"));
    title->colour = YELLOW;
    FAQmenu.set_title(title);

    for (unsigned int i = 0, size = question_keys.size(); i < size; i++)
    {
        const char letter = index_to_letter(i);
        string question = getFAQ_Question(question_keys[i]);
        trim_string_right(question);
        MenuEntry *me = new MenuEntry(question, MEL_ITEM, 1, letter);
        me->data = &question_keys[i];
        FAQmenu.add_entry(me);
    }

    while (true)
    {
        vector<MenuEntry*> sel = FAQmenu.show();
        if (sel.empty())
            return;
        else
        {
            ASSERT(sel.size() == 1);
            ASSERT(sel[0]->hotkeys.size() == 1);

            string key = *((string*) sel[0]->data);
            string answer = getFAQ_Answer(key);
            if (answer.empty())
            {
                answer = localise("No answer found in the FAQ! Please submit "
                                  "a bug report!");
            }
            answer = localise("Q: ") + getFAQ_Question(key) + "\n" + answer;
            show_description(answer);
        }
    }

    return;
}

////////////////////////////////////////////////////////////////////////////

int show_keyhelp_menu(const vector<formatted_string> &lines)
{
    int flags = FS_PREWRAPPED_TEXT | FS_EASY_EXIT;
    formatted_scroller cmd_help(flags);
    cmd_help.set_tag("help");
    cmd_help.set_more();

    for (unsigned i = 0; i < lines.size(); ++i)
        cmd_help.add_formatted_string(lines[i], i < lines.size()-1);

    cmd_help.show();

    return cmd_help.get_lastch();
}

void show_specific_help(const string &key)
{
    string help = getHelpString(key);
    trim_string_right(help);
    vector<formatted_string> formatted_lines;
    for (const string &line : split_string("\n", help, false, true))
        formatted_lines.push_back(formatted_string::parse_string(line));
    show_keyhelp_menu(formatted_lines);
}

void show_levelmap_help()
{
    show_specific_help("level-map");
}

void show_targeting_help()
{
    column_composer cols(2, 40);
    string targeting_help_1 = getHelpString("targeting-help-1");
    cols.add_formatted(0, targeting_help_1, true);
#ifdef WIZARD
    if (you.wizard)
    {
        string targeting_help_wiz = getHelpString("targeting-help-wiz");
        cols.add_formatted(0, targeting_help_wiz, true);
    }
#endif
    string targeting_help_2 = getHelpString("targeting-help-2");
    cols.add_formatted(1, targeting_help_2, true);
    show_keyhelp_menu(cols.formatted_lines());
}
void show_interlevel_travel_branch_help()
{
    show_specific_help("interlevel-travel.branch.prompt");
}

void show_interlevel_travel_depth_help()
{
    show_specific_help("interlevel-travel.depth.prompt");
}

void show_interlevel_travel_altar_help()
{
    show_specific_help("interlevel-travel.altar.prompt");
}

void show_annotate_help()
{
    show_specific_help("annotate.prompt");
}

void show_stash_search_help()
{
    show_specific_help("stash-search.prompt");
}

void show_skill_menu_help()
{
    show_specific_help("skill-menu");
}

void show_spell_library_help()
{
    if (crawl_state.game_is_hints_tutorial())
    {
        const string help1 = hints_memorise_info() + "\n\n";
        vector<formatted_string> formatted_lines;
        for (const string &line : split_string("\n", help1, false, true))
        {
            formatted_lines.push_back(formatted_string::parse_string(line,
                                        channel_to_colour(MSGCH_TUTORIAL)));
        }
        const string help2 = getHelpString("spell-library");
        for (const string &line : split_string("\n", help2, false, true))
            formatted_lines.push_back(formatted_string::parse_string(line));
        show_keyhelp_menu(formatted_lines);
    }
    else
        show_specific_help("spell-library");
}

static void _add_insert_commands(column_composer &cols, const int column,
                                 string desc,
                                 const vector<command_type> &cmd_vector)
{
    insert_commands(desc, cmd_vector);
    desc += "\n";
    cols.add_formatted(column, desc.c_str(), false);
}

static void _add_formatted_help_menu(column_composer &cols)
{
    cols.add_formatted(0, getHelpString("help-menu-1"));

    // TODO: generate this from the manual somehow
    cols.add_formatted(1, getHelpString("help-menu-2"));
}

static void _add_movement_diagram(column_composer &cols)
{
    // i18n: This should be the same regardless of language
    // @noloc section start
    _add_insert_commands(cols, 0, "                 <w>7 8 9      % % %",
                         { CMD_MOVE_UP_LEFT, CMD_MOVE_UP, CMD_MOVE_UP_RIGHT });
    _add_insert_commands(cols, 0, "                  \\|/        \\|/", {});
    _add_insert_commands(cols, 0, "                 <w>4</w>-<w>5</w>-<w>6</w>"
                                  "      <w>%</w>-<w>%</w>-<w>%</w>",
                         { CMD_MOVE_LEFT, CMD_WAIT, CMD_MOVE_RIGHT });
    _add_insert_commands(cols, 0, "                  /|\\        /|\\", {});
    _add_insert_commands(cols, 0, "                 <w>1 2 3      % % %",
                         { CMD_MOVE_DOWN_LEFT, CMD_MOVE_DOWN,
                           CMD_MOVE_DOWN_RIGHT });
    // @noloc section end
 }

static void _add_formatted_keyhelp(column_composer &cols)
{
    string move_help = getHelpString("cmd-help-movement");
    linebreak_string(move_help, 40);
    cols.add_formatted(0, move_help);

    _add_movement_diagram(cols);

    string rest_help = getHelpString("cmd-help-rest");
    insert_commands(rest_help, { CMD_WAIT, CMD_REST });
    linebreak_string(rest_help, 40, 4);
    cols.add_formatted(0, rest_help);

    string extended_move = getHelpString("cmd-help-ext-movement");
    insert_commands(extended_move,
                    { CMD_EXPLORE,
                      CMD_INTERLEVEL_TRAVEL,
                      CMD_SEARCH_STASHES,
                      CMD_FIX_WAYPOINT});
    linebreak_string(extended_move, 40, 9);
    cols.add_formatted(0, extended_move);

    string autofight_help = getHelpString("cmd-help-autofight");
    linebreak_string(autofight_help, 40, 4);
    cols.add_formatted(0, autofight_help);

    string item_type_help = getHelpString("cmd-help-item-types");
    item_type_help = replace_all(item_type_help, "@book_glyph@",
                            stringize_glyph(get_item_symbol(SHOW_ITEM_BOOK)));
    insert_commands(item_type_help,
                    { CMD_WIELD_WEAPON,
                      CMD_QUIVER_ITEM, CMD_FIRE, CMD_CYCLE_QUIVER_FORWARD,
                                                 CMD_CYCLE_QUIVER_BACKWARD,
                      CMD_WEAR_ARMOUR, CMD_REMOVE_ARMOUR,
                      CMD_READ,
                      CMD_QUAFF,
                      CMD_WEAR_JEWELLERY, CMD_REMOVE_JEWELLERY,
                      CMD_WEAR_JEWELLERY, CMD_REMOVE_JEWELLERY,
                      CMD_EVOKE,
                      CMD_MEMORISE_SPELL, CMD_CAST_SPELL, CMD_FORCE_CAST_SPELL,
                      CMD_WIELD_WEAPON,
                      CMD_EVOKE,
                      CMD_LIST_GOLD
                    });
    linebreak_string(item_type_help, 40, 4);
    cols.add_formatted(0, item_type_help);

    string other_gameplay = getHelpString("cmd-help-other-gameplay");
    insert_commands(other_gameplay,
                    { CMD_USE_ABILITY, CMD_USE_ABILITY,
                      CMD_CAST_SPELL,
                      CMD_FORCE_CAST_SPELL,
                      CMD_DISPLAY_SPELLS,
                      CMD_MEMORISE_SPELL,
                      CMD_SHOUT, CMD_SHOUT,
                      CMD_PREV_CMD_AGAIN,
                      CMD_REPEAT_CMD
                    });
    linebreak_string(other_gameplay, 40, 4);
    cols.add_formatted(0, other_gameplay);

    string non_gameplay = getHelpString("cmd-help-non-gameplay");
    insert_commands(non_gameplay,
                    { CMD_GAME_MENU,
                      CMD_REPLAY_MESSAGES,
                      CMD_REDRAW_SCREEN,
                      CMD_CLEAR_MAP,
                      CMD_MACRO_ADD,
                      CMD_MACRO_MENU,
                      CMD_ANNOTATE_LEVEL,
                      CMD_CHARACTER_DUMP,
                      CMD_MAKE_NOTE, CMD_DISPLAY_COMMANDS,
                      CMD_ADJUST_INVENTORY,
#ifdef USE_TILE_LOCAL
                      CMD_EDIT_PLAYER_TILE,
#endif
                    });
    linebreak_string(non_gameplay, 40, 4);
    cols.add_formatted(0, non_gameplay);

    string save_quit = getHelpString("cmd-help-save-quit");
    insert_commands(extended_move,
                    { CMD_SAVE_GAME,
                      CMD_SAVE_GAME_NOW,
                      CMD_QUIT});
    linebreak_string(save_quit, 40, 9);
    cols.add_formatted(1, save_quit);

    string character_help = getHelpString("cmd-help-character");
    insert_commands(character_help,
                    { CMD_DISPLAY_CHARACTER_STATUS,
                      CMD_DISPLAY_SKILLS,
                      CMD_RESISTS_SCREEN,
                      CMD_DISPLAY_RELIGION,
                      CMD_DISPLAY_MUTATIONS,
                      CMD_DISPLAY_KNOWN_OBJECTS,
                      CMD_MEMORISE_SPELL,
                      CMD_DISPLAY_RUNES,
                      CMD_LIST_ARMOUR,
                      CMD_LIST_JEWELLERY,
                      CMD_LIST_GOLD,
                      CMD_EXPERIENCE_CHECK,
                      CMD_QUIT});
    linebreak_string(character_help, 40, 4);
    cols.add_formatted(1, character_help);

    string dungeon_help = getHelpString("cmd-help-dungeon");
    insert_commands(dungeon_help,
                    { CMD_OPEN_DOOR, CMD_CLOSE_DOOR,
                      CMD_GO_UPSTAIRS, CMD_GO_DOWNSTAIRS,
                      CMD_INSPECT_FLOOR,
                      CMD_LOOK_AROUND,
                      CMD_DISPLAY_MAP, CMD_DISPLAY_MAP,
                      CMD_FULL_VIEW,
                      CMD_SHOW_TERRAIN,
                      CMD_DISPLAY_OVERMAP,
                      CMD_TOGGLE_AUTOPICKUP,
                      CMD_TOGGLE_TRAVEL_SPEED,
                    });
#ifdef USE_SOUND
    string sound_help = getHelpString("cmd-help-sound");
    insert_commands(sound_help, { CMD_TOGGLE_SOUND });
    dungeon_help += sound_help;
#endif
#ifdef USE_TILE_LOCAL
    string zoom_help = getHelpString("cmd-help-zoom");
    insert_commands(zoom_help, { CMD_ZOOM_OUT, CMD_ZOOM_IN });
    dungeon_help += zoom_help;
#endif
    linebreak_string(dungeon_help, 40, 9);
    cols.add_formatted(1, dungeon_help);

    string inventory_help = getHelpString("cmd-help-inventory");
    insert_commands(inventory_help,
                    { CMD_DISPLAY_INVENTORY,
                      CMD_PICKUP,
                      CMD_DROP,
                      CMD_DROP,
                      CMD_DROP_LAST,
                      CMD_QUIT});
    linebreak_string(inventory_help, 40, 4);
    cols.add_formatted(1, inventory_help);

    string item_help = getHelpString("cmd-help-items");
    insert_commands(item_help,
                    { CMD_INSCRIBE_ITEM,
                      CMD_FIRE,
                      CMD_THROW_ITEM_NO_QUIVER,
                      CMD_QUIVER_ITEM,
                      CMD_SWAP_QUIVER_RECENT,
                      CMD_QUAFF,
                      CMD_READ,
                      CMD_WIELD_WEAPON,
                      CMD_WEAPON_SWAP,
                      CMD_ADJUST_INVENTORY,
                      CMD_PRIMARY_ATTACK,
                      CMD_EVOKE,
                      CMD_WEAR_ARMOUR, CMD_REMOVE_ARMOUR,
                      CMD_WEAR_JEWELLERY, CMD_REMOVE_JEWELLERY,
                      CMD_QUIT});
    cols.add_formatted(1, item_help);

    string text = getHelpString("cmd-help-additional");
    insert_commands(text, { CMD_DISPLAY_MAP, CMD_LOOK_AROUND, CMD_FIRE,
                            CMD_SEARCH_STASHES, CMD_INTERLEVEL_TRAVEL,
                            CMD_DISPLAY_SPELLS, CMD_DISPLAY_SKILLS,
                            CMD_USE_ABILITY
                          });
    linebreak_string(text, 40);
    cols.add_formatted(1, text);
}

static void _add_formatted_hints_help(column_composer &cols)
{
    // First column.
    string move_help = getHelpString("cmd-help-movement");
    linebreak_string(move_help, 40);
    cols.add_formatted(0, move_help, false);

    _add_movement_diagram(cols);

    string move_hints = getHelpString("cmd-hints-move");
    insert_commands(move_hints,
                    { CMD_GO_UPSTAIRS, CMD_GO_DOWNSTAIRS,
                      CMD_EXPLORE
                    });
    linebreak_string(move_hints, 40);
    cols.add_formatted(0, move_hints);

    string rest_hints = getHelpString("cmd-hints-rest");
    insert_commands(rest_hints,
                    { CMD_WAIT,
                      CMD_REST
                    });
    linebreak_string(rest_hints, 40);
    cols.add_formatted(0, rest_hints);

    string attack_hints = getHelpString("cmd-hints-attack");
    linebreak_string(attack_hints, 40);
    cols.add_formatted(0, attack_hints);

    string ranged_hints = getHelpString("cmd-hints-ranged");
    insert_commands(ranged_hints,
                    { CMD_FIRE,
                      CMD_CAST_SPELL, CMD_FORCE_CAST_SPELL,
                          CMD_CAST_SPELL, CMD_DISPLAY_SPELLS,
                      CMD_MEMORISE_SPELL
                    });
    linebreak_string(ranged_hints, 40);
    cols.add_formatted(0, ranged_hints);

    // Second column.
    string item_hints = getHelpString("cmd-hints-items");
    item_hints = replace_all(item_hints, "@book_glyph@",
                             stringize_glyph(get_item_symbol(SHOW_ITEM_BOOK)));
    item_hints = replace_all(item_hints, "@staff_glyph@",
                             stringize_glyph(get_item_symbol(SHOW_ITEM_STAFF)));
    insert_commands(item_hints,
                    { CMD_WIELD_WEAPON,
                      CMD_QUIVER_ITEM, CMD_FIRE, CMD_CYCLE_QUIVER_FORWARD,
                                                 CMD_CYCLE_QUIVER_BACKWARD,
                      CMD_WEAR_ARMOUR, CMD_REMOVE_ARMOUR,
                      CMD_READ,
                      CMD_QUAFF,
                      CMD_WEAR_JEWELLERY, CMD_REMOVE_JEWELLERY,
                      CMD_WEAR_JEWELLERY, CMD_REMOVE_JEWELLERY,
                      CMD_EVOKE,
                      CMD_MEMORISE_SPELL, CMD_CAST_SPELL, CMD_FORCE_CAST_SPELL,
                      CMD_WIELD_WEAPON
                    });
    linebreak_string(item_hints, 40);
    cols.add_formatted(1, item_hints, false);

    string item_hints2 = getHelpString("cmd-hints-items2");
    insert_commands(item_hints2,
                    { CMD_DISPLAY_INVENTORY,
                      CMD_PICKUP,
                      CMD_DROP,
                      CMD_DROP_LAST
                    });
    linebreak_string(item_hints2, 40);
    cols.add_formatted(1, item_hints2);

    string hints_additional = getHelpString("cmd-hints-additional");
    insert_commands(hints_additional,
                    { CMD_SAVE_GAME_NOW,
                      CMD_REPLAY_MESSAGES,
                      CMD_USE_ABILITY,
                      CMD_RESISTS_SCREEN,
                      CMD_DISPLAY_RELIGION,
                      CMD_DISPLAY_MAP,
                      CMD_DISPLAY_OVERMAP
                    });
    linebreak_string(hints_additional, 40);
    cols.add_formatted(1, hints_additional);

    string targeting_hints = getHelpString("cmd-hints-targeting");
    linebreak_string(targeting_hints, 40);
    cols.add_formatted(1, targeting_hints);
}

static formatted_string _col_conv(void (*func)(column_composer &))
{
    column_composer cols(2, 42);
    func(cols);
    formatted_string contents;
    for (const auto& line : cols.formatted_lines())
    {
        contents += line;
        contents += "\n";
    }
    contents.ops.pop_back();
    return contents;
}

static int _get_help_section(int section, formatted_string &header_out, formatted_string &text_out, int &scroll_out)
{
    static map<int, int> hotkeys;
    static map<int, formatted_string> page_text;
    // @noloc section start
    static map<int, string> headers = {
        {'*', "Manual"}, {'%', "Aptitudes"}, {'^', "Quickstart"},
        {'~', "Macros"}, {'&', "Options"}, {'t', "Tiles"},
        {'?', "Key help"}
    };
    // @noloc section end

    if (!page_text.size())
    {
        for (int i = 0; help_files[i].name != nullptr; ++i)
        {
            formatted_string text;
            bool next_is_hotkey = false;
            char buf[200];
            string fname = canonicalise_file_separator(help_files[i].name);
            FILE* fp = fopen_u(datafile_path(fname, false).c_str(), "r");
            ASSERTM(fp, "Failed to open '%s'!", fname.c_str());
            while (fgets(buf, sizeof buf, fp))
            {
                text += string(buf);
                if (next_is_hotkey && (isaupper(buf[0]) || isadigit(buf[0])))
                {
                    int hotkey = tolower_safe(buf[0]);
                    hotkeys[hotkey] = count_occurrences(text.tostring(), "\n");
                }

                next_is_hotkey =
                    strstr(buf, "------------------------------------------"
                                        "------------------------------") == buf;
            }
            trim_string_right(text.ops.back().text);
            page_text[help_files[i].hotkey] = text;
        }
    }

    // All hotkeys are currently on *-page
    const int page = hotkeys.count(section) ? '*' : section;

    string header = headers.count(page) ? ": "+headers[page] : "";
    header_out = formatted_string::parse_string(
                    "<yellow>Dungeon Crawl Help"+header+"</yellow>"); // @noloc
    scroll_out = 0;
    switch (section)
    {
        case '?':
            if (crawl_state.game_is_hints_tutorial())
                text_out = _col_conv(_add_formatted_hints_help);
            else
                text_out = _col_conv(_add_formatted_keyhelp);
            return page;
        case CK_HOME:
            text_out = _col_conv(_add_formatted_help_menu);
            return page;
        default:
            if (hotkeys.count(section))
                scroll_out = hotkeys[section];
            if (page_text.count(page))
            {
                text_out = page_text[page];
                return page;
            }
            break;
    }
    return 0;
}

class help_popup : public formatted_scroller
{
public:
    help_popup(int key) : formatted_scroller(FS_PREWRAPPED_TEXT) {
        set_tag("help");
        process_key(key);
    };
private:
    bool process_key(int ch) override
    {
        int key = toalower(ch);

#ifdef USE_TILE_LOCAL
        const int line_height = tiles.get_crt_font()->char_height();
#else
        const int line_height = 1;
#endif

        int scroll, page;
        formatted_string header_text, help_text;
        switch (key)
        {
            case CK_ESCAPE: case ':': case '#': case '/': case 'q': case 'v':
                return false;
            default:
                if (!(page = _get_help_section(key, header_text, help_text, scroll)))
                    break;
                if (page != prev_page)
                {
                    contents = help_text;
                    m_contents_dirty = true;
                    prev_page = page;
                }
                scroll = scroll ? (scroll-2)*line_height : 0;
                set_scroll(scroll);
                return true;
        }

        return formatted_scroller::process_key(ch);
    };
    int prev_page{0};
};

static bool _show_help_special(int key)
{
    switch (key)
    {
        case ':':
            // If the game has begun, show notes.
            if (crawl_state.need_save)
                display_notes();
            return true;
        case '#':
            // If the game has begun, show dump.
            if (crawl_state.need_save)
                display_char_dump();
            return true;
        case '/':
            keyhelp_query_descriptions();
            return true;
        case 'q':
            _handle_FAQ();
            return true;
        case 'v':
            _print_version();
            return true;
        default:
            return false;
    }
}

void show_help(int section, string highlight_string)
{
    // if `section` is a special case, don't instantiate a help popup at all.
    if (_show_help_special(toalower(section)))
        return;
    help_popup help(section);
    help.highlight = highlight_string;
    int key = toalower(help.show());
    // handle the case where one of the special case help sections is triggered
    // from the help main menu.
    _show_help_special(key);
}
