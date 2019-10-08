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
#include "lookup-help.h"
#include "macro.h"
#include "message.h"
#include "output.h"
#include "prompt.h"
#include "scroller.h"
#include "showsymb.h"
#include "sound.h"
#include "state.h"
#include "stringutil.h"
#include "syscalls.h"
#include "unicode.h"
#include "version.h"
#include "viewchar.h"
#include "view.h"

using namespace ui;

#ifdef USE_TILE
 #include "tiledef-gui.h"
#endif

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
            result += make_stringf(
                "Game seed: %" PRIu64 ", levelgen mode: %s",
                crawl_state.seed, you.deterministic_levelgen
                                                ? "deterministic" : "classic");
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
    icon->set_tile(tile_def(TILEG_STARTUP_STONESOUP, TEX_GUI));
    title_hbox->add_child(move(icon));
#endif

    auto title = make_shared<Text>(formatted_string::parse_string(info));
    title->set_margin_for_crt({0, 0, 0, 0});
    title->set_margin_for_sdl({0, 0, 0, 10});
    title_hbox->add_child(move(title));

    title_hbox->align_items = Widget::CENTER;
    title_hbox->set_margin_for_crt({0, 0, 1, 0});
    title_hbox->set_margin_for_sdl({0, 0, 20, 0});
    vbox->add_child(move(title_hbox));

    auto scroller = make_shared<Scroller>();
    auto content = formatted_string::parse_string(feats + "\n\n" + changes);
    auto text = make_shared<Text>(move(content));
    text->wrap_text = true;
    scroller->set_child(move(text));
    vbox->add_child(scroller);

    auto popup = make_shared<ui::Popup>(vbox);

    bool done = false;
    popup->on(Widget::slots.event, [&done, &vbox](wm_event ev) {
        if (ev.type != WME_KEYDOWN)
            return false;
        done = !vbox->on_event(ev);
        return true;
    });

#ifdef USE_TILE_WEB
    tiles.json_open_object();
    tiles.json_write_string("information", info);
    tiles.json_write_string("features", feats);
    tiles.json_write_string("changes", changes);
    tiles.push_ui_layout("version", 0);
#endif

    ui::run_layout(move(popup), done);

#ifdef USE_TILE_WEB
    tiles.pop_ui_layout();
#endif
}

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

        estr << ((i == EQ_CLOAK)       ? "Cloak  " :
                 (i == EQ_HELMET)      ? "Helmet " :
                 (i == EQ_GLOVES)      ? "Gloves " :
                 (i == EQ_SHIELD)      ? "Shield " :
                 (i == EQ_BODY_ARMOUR) ? "Armour " :
                 (i == EQ_BOOTS) ?
                 ((you.species == SP_CENTAUR
                   || you.species == SP_NAGA) ? "Barding"
                                              : "Boots  ")
                                 : "unknown")
             << " : ";

        if (you_can_wear(i) == MB_FALSE)
            estr << "    (unavailable)";
        else if (you_can_wear(i, true) == MB_FALSE)
            estr << "    (currently unavailable)";
        else if (armour_id != -1)
        {
            estr << you.inv[armour_id].name(DESC_INVENTORY);
            colour = menu_colour(estr.str(), item_prefix(you.inv[armour_id]),
                                 "equip");
        }
        else if (you_can_wear(i) == MB_MAYBE)
            estr << "    (restricted)";
        else
            estr << "    none";

        if (colour == MSGCOL_BLACK)
            colour = menu_colour(estr.str(), "", "equip");

        mprf(MSGCH_EQUIPMENT, colour, "%s", estr.str().c_str());
    }
}

void list_jewellery()
{
    string jstr;
    int cols = get_number_of_cols() - 1;
    bool split = you.species == SP_OCTOPODE && cols > 84;

    for (int j = EQ_LEFT_RING; j < NUM_EQUIP; j++)
    {
        const equipment_type i = static_cast<equipment_type>(j);
        if (!you_can_wear(i))
            continue;

        const int jewellery_id = you.equip[i];
        int       colour       = MSGCOL_BLACK;

        const char *slot =
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

        string item;
        if (you_can_wear(i, true) == MB_FALSE)
            item = "    (currently unavailable)";
        else if (jewellery_id != -1)
        {
            item = you.inv[jewellery_id].name(DESC_INVENTORY);
            string prefix = item_prefix(you.inv[jewellery_id]);
            colour = menu_colour(item, prefix, "equip");
        }
        else
            item = "    none";

        if (colour == MSGCOL_BLACK)
            colour = menu_colour(item, "", "equip");

        item = chop_string(make_stringf("%-*s: %s",
                                        split ? cols > 96 ? 9 : 8 : 11,
                                        slot, item.c_str()),
                           split && i > EQ_AMULET ? (cols - 1) / 2 : cols);
        item = colour_string(item, colour);

        if (i == EQ_RING_SEVEN && you.species == SP_OCTOPODE &&
                you.get_mutation_level(MUT_MISSING_HAND))
        {
            mprf(MSGCH_EQUIPMENT, "%s", item.c_str());
        }
        else if (split && i > EQ_AMULET && (i - EQ_AMULET) % 2)
            jstr = item + " ";
        else
            mprf(MSGCH_EQUIPMENT, "%s%s", jstr.c_str(), item.c_str());
    }
}

static const char *targeting_help_1 =
    "<h>Examine surroundings ('<w>x</w><h>' in main):\n"
    "<w>Esc</w> : cancel (also <w>Space</w>, <w>x</w>)\n"
    "<w>Dir.</w>: move cursor in that direction\n"
    "<w>.</w> : move to cursor (also <w>Enter</w>, <w>Del</w>)\n"
    "<w>g</w> : pick up item at cursor\n"
    "<w>v</w> : describe monster under cursor\n"
    "<w>+</w> : cycle monsters forward (also <w>=</w>)\n"
    "<w>-</w> : cycle monsters backward\n"
    "<w>*</w> : cycle objects forward (also <w>'</w>)\n"
    "<w>/</w> : cycle objects backward (also <w>;</w>)\n"
    "<w>^</w> : cycle through traps\n"
    "<w>_</w> : cycle through altars\n"
    "<w><<</w>/<w>></w> : cycle through up/down stairs\n"
    "<w>Tab</w> : cycle through shops and portals\n"
    "<w>r</w> : move cursor to you\n"
    "<w>e</w> : create/remove travel exclusion\n"
    "<w>Ctrl-P</w> : repeat prompt\n"
;
#ifdef WIZARD
static const char *targeting_help_wiz =
    "<h>Wizard targeting commands:</h>\n"
    "<w>Ctrl-C</w> : cycle through beam paths\n"
    "<w>D</w>: get debugging information about the monster\n"
    "<w>o</w>: give item to monster\n"
    "<w>F</w>: cycle monster friendly/good neutral/neutral/hostile\n"
    "<w>G</w>: make monster gain experience\n"
    "<w>Ctrl-H</w>: heal the monster fully\n"
    "<w>P</w>: apply divine blessing to monster\n"
    "<w>m</w>: move monster or player\n"
    "<w>M</w>: cause spell miscast for monster or player\n"
    "<w>s</w>: force monster to shout or speak\n"
    "<w>S</w>: make monster a summoned monster\n"
    "<w>w</w>: calculate shortest path to any point on the map\n"
    "<w>\"</w>: get debugging information about a portal\n"
    "<w>~</w>: polymorph monster to specific type\n"
    "<w>,</w>: bring down the monster to 1 hp\n"
    "<w>(</w>: place a mimic\n"
    "<w>Ctrl-B</w>: banish monster\n"
    "<w>Ctrl-K</w>: kill monster\n"
;
#endif

static const char *targeting_help_2 =
    "<h>Targeting (zap wands, cast spells, etc.):\n"
    "Most keys from examine surroundings work.\n"
    "Some keys fire at the target. By default,\n"
    "range is respected and beams don't stop.\n"
    "<w>Enter</w> : fire (<w>Space</w>, <w>Del</w>)\n"
    "<w>.</w> : fire, stop at target\n"
    "<w>@</w> : fire, stop at target, ignore range\n"
    "<w>!</w> : fire, don't stop, ignore range\n"
    "<w>p</w> : fire at Previous target (also <w>f</w>)\n"
    "<w>:</w> : show/hide beam path\n"
    "<w>Shift-Dir.</w> : fire straight-line beam\n"
    "\n"
    "<h>Firing or throwing a missile:\n"
    "<w>(</w> : cycle to next suitable missile.\n"
    "<w>)</w> : cycle to previous suitable missile.\n"
    "<w>i</w> : choose from Inventory.\n"
;

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
    { "quickstart.txt",    '^', false },
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
    MenuEntry *title = new MenuEntry("Frequently Asked Questions");
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
                answer = "No answer found in the FAQ! Please submit a "
                         "bug report!";
            }
            answer = "Q: " + getFAQ_Question(key) + "\n" + answer;
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
    cols.add_formatted(0, targeting_help_1, true);
#ifdef WIZARD
    if (you.wizard)
        cols.add_formatted(0, targeting_help_wiz, true);
#endif
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

void show_stash_search_help()
{
    show_specific_help("stash-search.prompt");
}

void show_butchering_help()
{
    show_specific_help("butchering");
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

static void _add_command(column_composer &cols, const int column,
                         const command_type cmd,
                         const string &desc,
                         const unsigned int space_to_colon = 7)
{
    string command_name = command_to_string(cmd);
    if (strcmp(command_name.c_str(), "<") == 0)
        command_name += "<";

    const int cmd_len = strwidth(command_name);
    string line = "<w>" + command_name + "</w>";
    for (unsigned int i = cmd_len; i < space_to_colon; ++i)
        line += " ";
    line += ": " + untag_tiles_console(desc) + "\n";

    cols.add_formatted(
            column,
            line.c_str(),
            false);
}

static void _add_insert_commands(column_composer &cols, const int column,
                                 const unsigned int space_to_colon,
                                 command_type lead_cmd, string desc,
                                 const vector<command_type> &cmd_vector)
{
    insert_commands(desc, cmd_vector);
    desc += "\n";
    _add_command(cols, column, lead_cmd, desc, space_to_colon);
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
    cols.add_formatted(
        0,
        "<h>Dungeon Crawl Help\n"
        "\n"
        "Press one of the following keys to\n"
        "obtain more information on a certain\n"
        "aspect of Dungeon Crawl.\n"

        "<w>?</w>: List of commands\n"
        "<w>^</w>: Quickstart Guide\n"
        "<w>:</w>: Browse character notes\n"
        "<w>#</w>: Browse character dump\n"
        "<w>~</w>: Macros help\n"
        "<w>&</w>: Options help\n"
        "<w>%</w>: Table of aptitudes\n"
        "<w>/</w>: Lookup description\n"
        "<w>Q</w>: FAQ\n"
#ifdef USE_TILE_LOCAL
        "<w>T</w>: Tiles key help\n"
#endif
        "<w>V</w>: Version information\n"
        "<w>Home</w>: This screen\n");

    // TODO: generate this from the manual somehow
    cols.add_formatted(
        1,
        "<h>Manual Contents\n\n"
        "<w>*</w>       Table of contents\n"
        "<w>A</w>.      Overview\n"
        "<w>B</w>.      Starting Screen\n"
        "<w>C</w>.      Attributes and Stats\n"
        "<w>D</w>.      Exploring the Dungeon\n"
        "<w>E</w>.      Experience and Skills\n"
        "<w>F</w>.      Monsters\n"
        "<w>G</w>.      Items\n"
        "<w>H</w>.      Spellcasting\n"
        "<w>I</w>.      Targeting\n"
        "<w>J</w>.      Religion\n"
        "<w>K</w>.      Mutations\n"
        "<w>L</w>.      Licence, Contact, History\n"
        "<w>M</w>.      Macros, Options, Performance\n"
        "<w>N</w>.      Philosophy\n"
        "<w>1</w>.      List of Character Species\n"
        "<w>2</w>.      List of Character Backgrounds\n"
        "<w>3</w>.      List of Skills\n"
        "<w>4</w>.      List of Keys and Commands\n"
        "<w>5</w>.      Inscriptions\n"
        "<w>6</w>.      Dungeon sprint modes\n");
}

static void _add_formatted_keyhelp(column_composer &cols)
{
    cols.add_formatted(
            0,
            "<h>Movement:\n"
            "To move in a direction or to attack, \n"
            "use the numpad (try Numlock off and \n"
            "on) or vi keys:\n");

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

    cols.add_formatted(
            0,
            "<h>Rest:\n");

    _add_command(cols, 0, CMD_WAIT, "wait a turn (also <w>s</w>, <w>Del</w>)", 2);
    _add_command(cols, 0, CMD_REST, "rest and long wait; stops when", 2);
    cols.add_formatted(
            0,
            "    Health or Magic become full or\n"
            "    something is detected. If Health\n"
            "    and Magic are already full, stops\n"
            "    when 100 turns over (<w>numpad-5</w>)\n",
            false);

    cols.add_formatted(
            0,
            "<h>Extended Movement:\n");

    _add_command(cols, 0, CMD_EXPLORE, "auto-explore");
    _add_command(cols, 0, CMD_INTERLEVEL_TRAVEL, "interlevel travel");
    _add_command(cols, 0, CMD_SEARCH_STASHES, "Find items");
    _add_command(cols, 0, CMD_FIX_WAYPOINT, "set Waypoint");

    cols.add_formatted(
            0,
            "<w>/ Dir.</w>, <w>Shift-Dir.</w>: long walk\n"
            "<w>* Dir.</w>, <w>Ctrl-Dir.</w> : attack without move \n",
            false);

    cols.add_formatted(
            0,
            "<h>Autofight:\n"
            "<w>Tab</w>       : attack nearest monster,\n"
            "            moving if necessary\n"
            "<w>Shift-Tab</w> : attack nearest monster\n"
            "            without moving\n");

    cols.add_formatted(
            0,
            "<h>Item types (and common commands)\n");

    _add_insert_commands(cols, 0, "<cyan>)</cyan> : hand weapons (<w>%</w>ield)",
                         { CMD_WIELD_WEAPON });
    _add_insert_commands(cols, 0, "<brown>(</brown> : missiles (<w>%</w>uiver, "
                                  "<w>%</w>ire, <w>%</w>/<w>%</w> cycle)",
                         { CMD_QUIVER_ITEM, CMD_FIRE, CMD_CYCLE_QUIVER_FORWARD,
                           CMD_CYCLE_QUIVER_BACKWARD });
    _add_insert_commands(cols, 0, "<cyan>[</cyan> : armour (<w>%</w>ear and <w>%</w>ake off)",
                         { CMD_WEAR_ARMOUR, CMD_REMOVE_ARMOUR });
    _add_insert_commands(cols, 0, "<brown>percent</brown> : corpses and food "
                                  "(<w>%</w>hop up and <w>%</w>at)",
                         { CMD_BUTCHER, CMD_EAT });
    _add_insert_commands(cols, 0, "<w>?</w> : scrolls (<w>%</w>ead)",
                         { CMD_READ });
    _add_insert_commands(cols, 0, "<magenta>!</magenta> : potions (<w>%</w>uaff)",
                         { CMD_QUAFF });
    _add_insert_commands(cols, 0, "<blue>=</blue> : rings (<w>%</w>ut on and <w>%</w>emove)",
                         { CMD_WEAR_JEWELLERY, CMD_REMOVE_JEWELLERY });
    _add_insert_commands(cols, 0, "<red>\"</red> : amulets (<w>%</w>ut on and <w>%</w>emove)",
                         { CMD_WEAR_JEWELLERY, CMD_REMOVE_JEWELLERY });
    _add_insert_commands(cols, 0, "<lightgrey>/</lightgrey> : wands (e<w>%</w>oke)",
                         { CMD_EVOKE });

    string item_types = "<lightcyan>";
    item_types += stringize_glyph(get_item_symbol(SHOW_ITEM_BOOK));
    item_types +=
        "</lightcyan> : books (<w>%</w>emorise, <w>%</w>ap, <w>%</w>ap,\n"
        "    pick up to add to library)";
    _add_insert_commands(cols, 0, item_types,
                         { CMD_MEMORISE_SPELL, CMD_CAST_SPELL,
                           CMD_FORCE_CAST_SPELL });
    _add_insert_commands(cols, 0, "<brown>\\</brown> : staves (<w>%</w>ield and e<w>%</w>oke)",
                         { CMD_WIELD_WEAPON, CMD_EVOKE_WIELDED });
    _add_insert_commands(cols, 0, "<lightgreen>}</lightgreen> : miscellaneous items (e<w>%</w>oke)",
                         { CMD_EVOKE });
    _add_insert_commands(cols, 0, "<yellow>$</yellow> : gold (<w>%</w> counts gold)",
                         { CMD_LIST_GOLD });

    cols.add_formatted(
            0,
            "<lightmagenta>0</lightmagenta> : the Orb of Zot\n"
            "    Carry it to the surface and win!\n",
            false);

    cols.add_formatted(
            0,
            "<h>Other Gameplay Actions:\n");

    _add_insert_commands(cols, 0, 2, CMD_USE_ABILITY,
                         "use special Ability (<w>%!</w> for help)",
                         { CMD_USE_ABILITY });
    _add_command(cols, 0, CMD_CAST_SPELL, "cast spell, abort without targets", 2);
    _add_command(cols, 0, CMD_FORCE_CAST_SPELL, "cast spell, no matter what", 2);
    _add_command(cols, 0, CMD_DISPLAY_SPELLS, "list all memorized spells", 2);
    _add_command(cols, 0, CMD_MEMORISE_SPELL, "Memorise a spell from your library", 2);

    _add_insert_commands(cols, 0, 2, CMD_SHOUT,
                         "tell allies (<w>%t</w> to shout)",
                         { CMD_SHOUT });
    _add_command(cols, 0, CMD_PREV_CMD_AGAIN, "re-do previous command", 2);
    _add_command(cols, 0, CMD_REPEAT_CMD, "repeat next command # of times", 2);

    cols.add_formatted(
            0,
            "<h>Non-Gameplay Commands / Info\n");

    _add_command(cols, 0, CMD_REPLAY_MESSAGES, "show Previous messages");
    _add_command(cols, 0, CMD_REDRAW_SCREEN, "Redraw screen");
    _add_command(cols, 0, CMD_CLEAR_MAP, "Clear main and level maps");
    _add_command(cols, 0, CMD_ANNOTATE_LEVEL, "annotate the dungeon level", 2);
    _add_command(cols, 0, CMD_CHARACTER_DUMP, "dump character to file", 2);
    _add_insert_commands(cols, 0, 2, CMD_MAKE_NOTE,
                         "add note (use <w>%:</w> to read notes)",
                         { CMD_DISPLAY_COMMANDS });
    _add_command(cols, 0, CMD_MACRO_ADD, "add macro (also <w>Ctrl-D</w>)", 2);
    _add_command(cols, 0, CMD_ADJUST_INVENTORY, "reassign inventory/spell letters", 2);
#ifdef USE_TILE_LOCAL
    _add_command(cols, 0, CMD_EDIT_PLAYER_TILE, "edit player doll", 2);
#else
#ifdef USE_TILE_WEB
    if (tiles.is_controlled_from_web())
    {
        cols.add_formatted(0, "<w>F12</w> : read messages (online play only)",
                           false);
    }
    else
#endif
    _add_command(cols, 0, CMD_READ_MESSAGES, "read messages (online play only)", 2);
#endif

    cols.add_formatted(
            1,
            "<h>Game Saving and Quitting:\n");

    _add_command(cols, 1, CMD_SAVE_GAME, "Save game and exit");
    _add_command(cols, 1, CMD_SAVE_GAME_NOW, "Save and exit without query");
    _add_command(cols, 1, CMD_QUIT, "Abandon the current character");
    cols.add_formatted(1, "         and quit the game\n",
                       false);

    cols.add_formatted(
            1,
            "<h>Player Character Information:\n");

    _add_command(cols, 1, CMD_DISPLAY_CHARACTER_STATUS, "display character status", 2);
    _add_command(cols, 1, CMD_DISPLAY_SKILLS, "show skill screen", 2);
    _add_command(cols, 1, CMD_RESISTS_SCREEN, "character overview", 2);
    _add_command(cols, 1, CMD_DISPLAY_RELIGION, "show religion screen", 2);
    _add_command(cols, 1, CMD_DISPLAY_MUTATIONS, "show Abilities/mutations", 2);
    _add_command(cols, 1, CMD_DISPLAY_KNOWN_OBJECTS, "show item knowledge", 2);
    _add_command(cols, 1, CMD_MEMORISE_SPELL, "show your spell library", 2);
    _add_command(cols, 1, CMD_DISPLAY_RUNES, "show runes collected", 2);
    _add_command(cols, 1, CMD_LIST_ARMOUR, "display worn armour", 2);
    _add_command(cols, 1, CMD_LIST_JEWELLERY, "display worn jewellery", 2);
    _add_command(cols, 1, CMD_LIST_GOLD, "display gold in possession", 2);
    _add_command(cols, 1, CMD_EXPERIENCE_CHECK, "display experience info", 2);

    cols.add_formatted(
            1,
            "<h>Dungeon Interaction and Information:\n");

    _add_insert_commands(cols, 1, "<w>%</w>/<w>%</w>    : Open/Close door",
                         { CMD_OPEN_DOOR, CMD_CLOSE_DOOR });
    _add_insert_commands(cols, 1, "<w>%</w>/<w>%</w>    : use staircase",
                         { CMD_GO_UPSTAIRS, CMD_GO_DOWNSTAIRS });

    _add_command(cols, 1, CMD_INSPECT_FLOOR, "examine occupied tile and");
    cols.add_formatted(1, "         pickup part of a single stack\n",
                       false);

    _add_command(cols, 1, CMD_LOOK_AROUND, "eXamine surroundings/targets");
    _add_insert_commands(cols, 1, 7, CMD_DISPLAY_MAP,
                         "eXamine level map (<w>%?</w> for help)",
                         { CMD_DISPLAY_MAP });
    _add_command(cols, 1, CMD_FULL_VIEW, "list monsters, items, features");
    cols.add_formatted(1, "         in view\n",
                       false);
    _add_command(cols, 1, CMD_SHOW_TERRAIN, "toggle view layers");
    _add_command(cols, 1, CMD_DISPLAY_OVERMAP, "show dungeon Overview");
    _add_command(cols, 1, CMD_TOGGLE_AUTOPICKUP, "toggle auto-pickup");
#ifdef USE_SOUND
    _add_command(cols, 1, CMD_TOGGLE_SOUND, "mute/unmute sound effects");
#endif
    _add_command(cols, 1, CMD_TOGGLE_TRAVEL_SPEED, "set your travel speed to your");
    cols.add_formatted(1, "         slowest ally\n",
                           false);
#ifdef USE_TILE_LOCAL
    _add_insert_commands(cols, 1, "<w>%</w>/<w>%</w> : zoom out/in",
                        { CMD_ZOOM_OUT, CMD_ZOOM_IN });
#endif

    cols.add_formatted(
            1,
            "<h>Inventory management:\n");

    _add_command(cols, 1, CMD_DISPLAY_INVENTORY, "show Inventory list", 2);
    _add_command(cols, 1, CMD_PICKUP, "pick up items (also <w>g</w>)", 2);
    cols.add_formatted(
            1,
            "    (press twice for pick up menu)\n",
            false);

    _add_command(cols, 1, CMD_DROP, "Drop an item", 2);
    _add_insert_commands(cols, 1, "<w>%#</w>: Drop exact number of items",
                         { CMD_DROP });
    _add_command(cols, 1, CMD_DROP_LAST, "Drop the last item(s) you picked up", 2);

    cols.add_formatted(
            1,
            "<h>Item Interaction:\n");

    _add_command(cols, 1, CMD_INSCRIBE_ITEM, "inscribe item", 2);
    _add_command(cols, 1, CMD_BUTCHER, "Chop up a corpse on floor", 2);
    _add_command(cols, 1, CMD_EAT, "Eat food (tries floor first) \n", 2);
    _add_command(cols, 1, CMD_FIRE, "Fire next appropriate item", 2);
    _add_command(cols, 1, CMD_THROW_ITEM_NO_QUIVER, "select an item and Fire it", 2);
    _add_command(cols, 1, CMD_QUIVER_ITEM, "select item slot to be Quivered", 2);
    _add_command(cols, 1, CMD_QUAFF, "Quaff a potion", 2);
    _add_command(cols, 1, CMD_READ, "Read a scroll (or book on floor)", 2);
    _add_command(cols, 1, CMD_WIELD_WEAPON, "Wield an item (<w>-</w> for none)", 2);
    _add_command(cols, 1, CMD_WEAPON_SWAP, "wield item a, or switch to b", 2);

    _add_insert_commands(cols, 1, "    (use <w>%</w> to assign slots)",
                         { CMD_ADJUST_INVENTORY });

    _add_command(cols, 1, CMD_EVOKE_WIELDED, "eVoke power of wielded item", 2);
    _add_command(cols, 1, CMD_EVOKE, "eVoke wand and miscellaneous item", 2);

    _add_insert_commands(cols, 1, "<w>%</w>/<w>%</w> : Wear or Take off armour",
                         { CMD_WEAR_ARMOUR, CMD_REMOVE_ARMOUR });
    _add_insert_commands(cols, 1, "<w>%</w>/<w>%</w> : Put on or Remove jewellery",
                         { CMD_WEAR_JEWELLERY, CMD_REMOVE_JEWELLERY });

    cols.add_formatted(
            1,
            "<h>Additional help:\n");

    string text =
            "Many commands have context sensitive "
            "help, among them <w>%</w>, <w>%</w>, <w>%</w> (or any "
            "form of targeting), <w>%</w>, and <w>%</w>.\n"
            "You can read descriptions of your "
            "current spells (<w>%</w>), skills (<w>%?</w>) and "
            "abilities (<w>%!</w>).";
    insert_commands(text, { CMD_DISPLAY_MAP, CMD_LOOK_AROUND, CMD_FIRE,
                            CMD_SEARCH_STASHES, CMD_INTERLEVEL_TRAVEL,
                            CMD_DISPLAY_SPELLS, CMD_DISPLAY_SKILLS,
                            CMD_USE_ABILITY
                          });
    linebreak_string(text, 40);

    cols.add_formatted(
            1, text,
            false);
}

static void _add_formatted_hints_help(column_composer &cols)
{
    // First column.
    cols.add_formatted(
            0,
            "<h>Movement:\n"
            "To move in a direction or to attack, \n"
            "use the numpad (try Numlock off and \n"
            "on) or vi keys:\n",
            false);

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

    cols.add_formatted(0, " ", false);
    cols.add_formatted(0, "<w>Shift-Dir.</w> runs into one direction",
                       false);
    _add_insert_commands(cols, 0, "<w>%</w> or <w>%</w> : ascend/descend the stairs",
                         { CMD_GO_UPSTAIRS, CMD_GO_DOWNSTAIRS });
    _add_command(cols, 0, CMD_EXPLORE, "autoexplore", 2);

    cols.add_formatted(
            0,
            "<h>Rest:\n");

    _add_command(cols, 0, CMD_WAIT, "wait a turn (also <w>s</w>, <w>Del</w>)", 2);
    _add_command(cols, 0, CMD_REST, "rest and long wait; stops when", 2);
    cols.add_formatted(
            0,
            "    Health or Magic become full or\n"
            "    something is detected. If Health\n"
            "    and Magic are already full, stops\n"
            "    when 100 turns over (<w>numpad-5</w>)\n",
            false);

    cols.add_formatted(
            0,
            "\n<h>Attacking monsters\n"
            "Walking into a monster will attack it\n"
            "with the wielded weapon or barehanded.",
            false);

    cols.add_formatted(
            0,
            "\n<h>Ranged combat and magic\n",
            false);

    _add_insert_commands(cols, 0, "<w>%</w> to throw/fire missiles",
                         { CMD_FIRE });
    _add_insert_commands(cols, 0, "<w>%</w>/<w>%</w> to cast spells "
                                  "(<w>%?/%</w> lists spells)",
                         { CMD_CAST_SPELL, CMD_FORCE_CAST_SPELL, CMD_CAST_SPELL,
                           CMD_DISPLAY_SPELLS });
    _add_command(cols, 0, CMD_MEMORISE_SPELL, "Memorise spells and view spell\n"
                                              "    library (get books to add to it)", 2);

    // Second column.
    cols.add_formatted(
            1, "<h>Item types and inventory management\n",
            false);

    _add_insert_commands(cols, 1,
                         "<console><cyan>)</cyan> : </console>"
                         "hand weapons (<w>%</w>ield)",
                         { CMD_WIELD_WEAPON });
    _add_insert_commands(cols, 1,
                         "<console><brown>(</brown> : </console>"
                         "missiles (<w>%</w>uiver, <w>%</w>ire, <w>%</w>/<w>%</w> cycle)",
                         { CMD_QUIVER_ITEM, CMD_FIRE, CMD_CYCLE_QUIVER_FORWARD,
                           CMD_CYCLE_QUIVER_BACKWARD });
    _add_insert_commands(cols, 1,
                         "<console><cyan>[</cyan> : </console>"
                         "armour (<w>%</w>ear and <w>%</w>ake off)",
                         { CMD_WEAR_ARMOUR, CMD_REMOVE_ARMOUR });
    _add_insert_commands(cols, 1,
                         "<console><brown>percent</brown> : </console>"
                         "corpses and food (<w>%</w>hop up and <w>%</w>at)",
                         { CMD_BUTCHER, CMD_EAT });
    _add_insert_commands(cols, 1,
                         "<console><w>?</w> : </console>"
                         "scrolls (<w>%</w>ead)",
                         { CMD_READ });
    _add_insert_commands(cols, 1,
                         "<console><magenta>!</magenta> : </console>"
                         "potions (<w>%</w>uaff)",
                         { CMD_QUAFF });
    _add_insert_commands(cols, 1,
                         "<console><blue>=</blue> : </console>"
                         "rings (<w>%</w>ut on and <w>%</w>emove)",
                         { CMD_WEAR_JEWELLERY, CMD_REMOVE_JEWELLERY });
    _add_insert_commands(cols, 1,
                         "<console><red>\"</red> : </console>"
                         "amulets (<w>%</w>ut on and <w>%</w>emove)",
                         { CMD_WEAR_JEWELLERY, CMD_REMOVE_JEWELLERY });
    _add_insert_commands(cols, 1,
                         "<console><lightgrey>/</lightgrey> : </console>"
                         "wands (e<w>%</w>oke)",
                         { CMD_EVOKE });

    string item_types =
                  "<console><lightcyan>";
    item_types += stringize_glyph(get_item_symbol(SHOW_ITEM_BOOK));
    item_types +=
        "</lightcyan> : </console>"
        "books (<w>%</w>emorise, <w>%</w>ap, <w>%</w>ap,\n"
        "    pick up to add to spell library)";
    _add_insert_commands(cols, 1, item_types,
                         { CMD_MEMORISE_SPELL, CMD_CAST_SPELL,
                           CMD_FORCE_CAST_SPELL });

    item_types =
                  "<console><brown>";
    item_types += stringize_glyph(get_item_symbol(SHOW_ITEM_STAFF));
    item_types +=
        "</brown> : </console>"
        "staves (<w>%</w>ield and e<w>%</w>oke)";
    _add_insert_commands(cols, 1, item_types,
                         { CMD_WIELD_WEAPON, CMD_EVOKE_WIELDED });

    cols.add_formatted(1, " ", false);
    _add_command(cols, 1, CMD_DISPLAY_INVENTORY, "inventory (select item to view)", 2);
    _add_command(cols, 1, CMD_PICKUP, "pick up item from ground (also <w>g</w>)", 2);
    _add_command(cols, 1, CMD_DROP, "drop item", 2);
    _add_command(cols, 1, CMD_DROP_LAST, "drop the last item(s) you picked up", 2);

    cols.add_formatted(
            1,
            "<h>Additional important commands\n");

    _add_command(cols, 1, CMD_SAVE_GAME_NOW, "Save the game and exit", 2);
    _add_command(cols, 1, CMD_REPLAY_MESSAGES, "show previous messages", 2);
    _add_command(cols, 1, CMD_USE_ABILITY, "use an ability", 2);
    _add_command(cols, 1, CMD_RESISTS_SCREEN, "show character overview", 2);
    _add_command(cols, 1, CMD_DISPLAY_RELIGION, "show religion overview", 2);
    _add_command(cols, 1, CMD_DISPLAY_MAP, "show map of the whole level", 2);
    _add_command(cols, 1, CMD_DISPLAY_OVERMAP, "show dungeon overview", 2);

    cols.add_formatted(
            1,
            "\n<h>Targeting\n"
            "<w>Enter</w> or <w>.</w> or <w>Del</w> : confirm target\n"
            "<w>+</w> and <w>-</w> : cycle between targets\n"
            "<w>f</w> or <w>p</w> : shoot at previous target\n"
            "         if still alive and in sight\n",
            false);
}

static formatted_string _col_conv(void (*func)(column_composer &))
{
    column_composer cols(2, 42);
    func(cols);
    formatted_string contents;
    for (const auto& line : cols.formatted_lines())
    {
        contents += line;
        contents += formatted_string("\n");
    }
    contents.ops.pop_back();
    return contents;
}

static int _get_help_section(int section, formatted_string &header_out, formatted_string &text_out, int &scroll_out)
{
    static map<int, int> hotkeys;
    static map<int, formatted_string> page_text;
    static map<int, string> headers = {
        {'*', "Manual"}, {'%', "Aptitudes"}, {'^', "Quickstart"},
        {'~', "Macros"}, {'&', "Options"}, {'t', "Tiles"},
        {'?', "Key help"}
    };

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
                text += formatted_string(buf);
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
                    "<yellow>Dungeon Crawl Help"+header+"</yellow>");
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
