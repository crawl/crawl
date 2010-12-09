/*
 *  File:       skills2.cc
 *  Summary:    More skill related functions.
 *  Written by: Linley Henzell
 */

#include "AppHdr.h"

#include "skills2.h"
#include "skills_menu.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <sstream>

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "artefact.h"
#include "cio.h"
#include "describe.h"
#include "externs.h"
#include "fight.h"
#include "godabil.h"
#include "itemprop.h"
#include "player.h"
#include "species.h"
#include "skills.h"
#include "stuff.h"
#include "hints.h"

#ifdef USE_TILE
#include "tilereg-crt.h"
#endif

typedef std::string (*string_fn)();
typedef std::map<std::string, string_fn> skill_op_map;

static skill_op_map Skill_Op_Map;

// The species for which the skill title is being worked out.
static species_type Skill_Species = SP_UNKNOWN;

class skill_title_key_t
{
public:
    skill_title_key_t(const char *k, string_fn o) : key(k), op(o)
    {
        Skill_Op_Map[k] = o;
    }

    static std::string get(const std::string &_key)
    {
        skill_op_map::const_iterator i = Skill_Op_Map.find(_key);
        return (i == Skill_Op_Map.end()? std::string() : (i->second)());
    }
private:
    const char *key;
    string_fn op;
};

typedef skill_title_key_t stk;

// Basic goals for titles:
// The higher titles must come last.
// Referring to the skill itself is fine ("Transmuter") but not impressive.
// No overlaps, high diversity.

// Replace @Adj@ with uppercase adjective form, @genus@ with lowercase genus,
// @Genus@ with uppercase genus, and %s with special cases defined below,
// including but not limited to species.

// NOTE:  Even though %s could be used with most of these, remember that
// the character's race will be listed on the next line.  It's only really
// intended for cases where things might be really awkward without it. -- bwr

// NOTE: If a skill name is changed, remember to also adapt the database entry.
const char *skills[50][6] =
{
  //  Skill name        levels 1-7       levels 8-14        levels 15-20       levels 21-26      level 27
    {"Fighting",       "Skirmisher",    "Fighter",         "Warrior",         "Slayer",         "Conqueror"},
    {"Short Blades",   "Cutter",        "Slicer",          "Swashbuckler",    "Blademaster",    "Eviscerator"},
    {"Long Blades",    "Slasher",       "Carver",          "Fencer",          "@Adj@ Blade",    "Swordmaster"},
    {"Axes",           "Chopper",       "Cleaver",         "Severer",         "Executioner",    "Axe Maniac"},
    {"Maces & Flails", "Cudgeler",      "Basher",          "Bludgeoner",      "Shatterer",      "Skullcrusher"},
    {"Polearms",       "Poker",         "Spear-Bearer",    "Impaler",         "Phalangite",     "@Adj@ Porcupine"},
    {"Staves",         "Twirler",       "Cruncher",        "Stickfighter",    "Pulveriser",     "Chief of Staff"},
    {"Slings",         "Vandal",        "Slinger",         "Whirler",         "Slingshot",      "@Adj@ Catapult"},
    {"Bows",           "Shooter",       "Archer",          "Marks@genus@",    "Crack Shot",     "Merry @Genus@"},
    {"Crossbows",      "Bolt Thrower",  "Quickloader",     "Sharpshooter",    "Sniper",         "@Adj@ Arbalest"},
    {"Throwing",       "Chucker",       "Thrower",         "Deadly Accurate", "Hawkeye",        "@Adj@ Ballista"},
    {"Armour",         "Covered",       "Protected",       "Tortoise",        "Impregnable",    "Invulnerable"},
    {"Dodging",        "Ducker",        "Nimble",          "Spry",            "Acrobat",        "Intangible"},
    {"Stealth",        "Sneak",         "Covert",          "Unseen",          "Imperceptible",  "Ninja"},
    {"Stabbing",       "Miscreant",     "Blackguard",      "Backstabber",     "Cutthroat",      "Politician"},
    {"Shields",        "Shield-Bearer", "Hoplite",         "Blocker",         "Peltast",        "@Adj@ Barricade"},
    {"Traps & Doors",  "Scout",         "Disarmer",        "Vigilant",        "Perceptive",     "Dungeon Master"},
    // STR based fighters, for DEX/martial arts titles see below.  Felids get their own cathegory, too.
    {"Unarmed Combat", "Ruffian",       "Grappler",        "Brawler",         "Wrestler",       "@Weight@weight Champion"},

    {NULL},
    {NULL},
    {NULL},
    {NULL},
    {NULL},
    {NULL},
    {NULL},

    {"Spellcasting",   "Magician",      "Thaumaturge",     "Eclecticist",     "Sorcerer",       "Archmage"},
    {"Conjurations",   "Ruinous",       "Conjurer",        "Destroyer",       "Devastator",     "Annihilator"},
    {"Enchantments",   "Charm-Maker",   "Infuser",         "Bewitcher",       "Enchanter",      "Spellbinder"},
    {"Summonings",     "Caller",        "Summoner",        "Convoker",        "Demonologist",   "Hellbinder"},
    {"Necromancy",     "Grave Robber",  "Reanimator",      "Necromancer",     "Thanatomancer",  "@Genus_Short@ of Death"},
    {"Translocations", "Grasshopper",   "Placeless @Genus@", "Blinker",       "Portalist",      "Plane @Walker@"},
    {"Transmutations", "Changer",       "Transmogrifier",  "Alchemist",       "Malleable",      "Shapeless @Genus@"},

    {"Fire Magic",     "Firebug",       "Arsonist",        "Scorcher",        "Pyromancer",     "Infernalist"},
    {"Ice Magic",      "Chiller",       "Frost Mage",      "Gelid",           "Cryomancer",     "Englaciator"},
    {"Air Magic",      "Gusty",         "Cloud Mage",      "Aerator",         "Anemomancer",    "Meteorologist"},
    {"Earth Magic",    "Digger",        "Geomancer",       "Earth Mage",      "Metallomancer",  "Petrodigitator"},
    {"Poison Magic",   "Stinger",       "Tainter",         "Polluter",        "Contaminator",   "Envenomancer"},

    // These titles apply to atheists only, worshippers of the various gods
    // use the god titles instead, depending on piety or, in Xom's case, mood.
    {"Invocations",    "Unbeliever",    "Agnostic",        "Dissident",       "Heretic",        "Apostate"},
    {"Evocations",     "Charlatan",     "Prestidigitator", "Fetichist",       "Evocator",       "Talismancer"},

    {NULL},
    {NULL},
    {NULL},
    {NULL},
    {NULL},
    {NULL},
    {NULL},
    {NULL},
    {NULL},
    {NULL}
};

const char *martial_arts_titles[6] =
    {"Unarmed Combat", "Insei", "Martial Artist", "Black Belt", "Sensei", "Grand Master"};
const char *claw_and_tooth_titles[6] =
    {"Unarmed Combat", "Scratcher", "Gouger", "Ripper", "Eviscerator", "Sabretooth"};

struct species_skill_aptitude
{
    species_type species;
    skill_type   skill;
    int aptitude;          // -50..50, with 0 for humans

    species_skill_aptitude(species_type _species,
                           skill_type _skill,
                           int _aptitude)
        : species(_species), skill(_skill), aptitude(_aptitude)
    {
    }
};

#include "aptitudes.h"

// Traditionally, Spellcasting and In/Evocations formed the exceptions here:
// Spellcasting skill was more expensive with about 130%, the other two got
// a discount with about 75%.
static int _spec_skills[NUM_SPECIES][NUM_SKILLS];

/* *************************************************************

// these were unimplemented "level titles" for two classes {dlb}

JOB_PRIEST
   "Preacher";
   "Priest";
   "Evangelist";
   "Pontifex";

JOB_PALADIN:
   "Holy Warrior";
   "Holy Crusader";
   "Paladin";
   "Scourge of Evil";

************************************************************* */

static const skill_type skill_display_order[] =
{
    SK_TITLE,
    SK_FIGHTING, SK_SHORT_BLADES, SK_LONG_BLADES, SK_AXES,
    SK_MACES_FLAILS, SK_POLEARMS, SK_STAVES, SK_UNARMED_COMBAT,

    SK_BLANK_LINE,

    SK_BOWS, SK_CROSSBOWS, SK_THROWING, SK_SLINGS,

    SK_BLANK_LINE,

    SK_ARMOUR, SK_DODGING, SK_STEALTH, SK_SHIELDS,

    SK_COLUMN_BREAK, SK_TITLE,

    SK_STABBING, SK_TRAPS_DOORS,

    SK_BLANK_LINE,

    SK_SPELLCASTING, SK_CONJURATIONS, SK_ENCHANTMENTS, SK_SUMMONINGS,
    SK_NECROMANCY, SK_TRANSLOCATIONS, SK_TRANSMUTATIONS,
    SK_FIRE_MAGIC, SK_ICE_MAGIC, SK_AIR_MAGIC, SK_EARTH_MAGIC, SK_POISON_MAGIC,

    SK_BLANK_LINE,

    SK_INVOCATIONS, SK_EVOCATIONS,

    SK_COLUMN_BREAK,
};

static const int ndisplayed_skills =
            sizeof(skill_display_order) / sizeof(*skill_display_order);

int get_skill_percentage(const skill_type x)
{
    const int needed = skill_exp_needed(you.skills[x] + 1, x);
    const int prev_needed = skill_exp_needed(you.skills[x], x);

    const int amt_done = you.skill_points[x] - prev_needed;
    int percent_done = (amt_done*100) / (needed - prev_needed);

    if (percent_done >= 100) // paranoia (1)
        percent_done = 99;

    if (percent_done < 0)    // paranoia (2)
        percent_done = 0;

    // Round down to multiple of 5.
    return ((percent_done / 5) * 5);
}

static void _add_item(TextItem* item, MenuObject* mo, const int size,
                      coord_def &coord)
{
    item->set_bounds(coord, coord_def(coord.x + size, coord.y + 1));
    item->set_visible(true);
    mo->attach_item(item);
    coord.x += size + 1;
}

#define NAME_SIZE 19
#define LEVEL_SIZE 4
#define PROGRESS_SIZE 6
#define APTITUDE_SIZE 5
SkillMenuEntry::SkillMenuEntry(coord_def coord, MenuFreeform* ff)
{
    m_name = new TextItem();
    _add_item(m_name, ff, NAME_SIZE, coord);

    m_level = new NoSelectTextItem();
    _add_item(m_level, ff, LEVEL_SIZE, coord);

    m_progress = new NoSelectTextItem();
    _add_item(m_progress, ff, PROGRESS_SIZE, coord);

    m_aptitude = new FormattedTextItem();
    _add_item(m_aptitude, ff, APTITUDE_SIZE, coord);
}

void SkillMenuEntry::set_skill(skill_type sk)
{
    m_sk = sk;

    if (sk == SK_TITLE)
        _set_title();
    else if (is_invalid_skill(sk))
        _clear();
    else
    {
        set_name(false);
        set_display();
        if (is_set(SKMF_DISP_APTITUDE))
            _set_aptitude();
    }
}

skill_type SkillMenuEntry::get_skill()
{
    return m_sk;
}

void SkillMenuEntry::set_name(bool keep_hotkey)
{
    if (is_invalid_skill(m_sk))
        return;

    if (!keep_hotkey)
        m_name->clear_hotkeys();

    if (_is_selectable())
    {
        if (!keep_hotkey)
            m_name->add_hotkey(++m_letter);
        m_name->set_id(m_sk);
        m_name->set_highlight_colour(LIGHTGRAY);
        m_name->allow_highlight(true);
    }
    else
    {
        m_name->set_id(-1);
        m_name->allow_highlight(false);
    }

    m_name->set_text(make_stringf("%s %-15s", _get_prefix().c_str(),
                                skill_name(m_sk)));
    m_name->set_fg_colour(_get_colour());
    _set_level();
}

void SkillMenuEntry::set_display()
{
    if (is_invalid_skill(m_sk))
        return;

    if (is_set(SKMF_DISP_PROGRESS))
        _set_progress();
    else if (is_set(SKMF_DISP_RESKILL))
        _set_reskill_progress();
#ifdef DEBUG_DIAGNOSTICS
    else if (is_set(SKMF_DISP_POINTS))
        _set_points();
#endif
    else if (is_set(SKMF_RESKILLING))
        _set_new_level();
}

int SkillMenuEntry::get_id()
{
    return m_name->get_id();
}

bool SkillMenuEntry::is_set(int flag) const
{
    return m_skm->is_set(flag);
}

COLORS SkillMenuEntry::_get_colour() const
{
    int ct_bonus = crosstrain_bonus(m_sk);

    if (is_set(SKMF_DO_PRACTISE) && you.practise_skill[m_sk] == 0)
        return(DARKGREY);
    else if (is_set(SKMF_DISP_RESKILL) && (m_sk == you.transfer_from_skill
                                        || m_sk == you.transfer_to_skill))
    {
        return(GREEN);
    }
    else if (is_set(SKMF_DO_RESKILL_TO) && m_sk == you.transfer_from_skill)
        return(WHITE);
    else if (you.skills[m_sk] == 27)
        return(YELLOW);
    else if (you.practise_skill[m_sk] == 0 || you.skills[m_sk] == 0)
        return(DARKGREY);
    else if (ct_bonus > 1 && is_set(SKMF_DISP_APTITUDE))
        return(LIGHTBLUE);
    else if (is_antitrained(m_sk) && is_set(SKMF_DISP_APTITUDE))
        return(MAGENTA);
    else
        return(LIGHTGREY);
}

std::string SkillMenuEntry::_get_prefix()
{
    int letter;
    const std::vector<int> hotkeys = m_name->get_hotkeys();

    if (hotkeys.size())
        letter = hotkeys[0];
    else
        letter = ' ';

    const int sign = (you.skills[m_sk] == 0 || you.skills[m_sk] == 27) ? ' '
                                    : (you.practise_skill[m_sk]) ? '+' : '-';
    return make_stringf("%c %c", letter, sign);
}

bool SkillMenuEntry::_is_selectable() const
{
    if (is_invalid_skill(m_sk))
        return false;

    if (you.skills[m_sk] == 0 && !is_set(SKMF_DISP_ALL))
        return false;

    if (is_set(SKMF_DO_SHOW_DESC))
        return true;

    if (is_set(SKMF_DO_RESKILL_TO) && you.transfer_from_skill == m_sk)
        return false;

    if (you.skills[m_sk] == 0 && !is_set(SKMF_DO_RESKILL_TO))
        return false;

    if (you.skills[m_sk] == 27 && !is_set(SKMF_DO_RESKILL_FROM))
        return false;

    return true;
}

void SkillMenuEntry::_set_level()
{
    m_level->set_text(make_stringf("%2d", you.skills[m_sk]));
    m_level->set_fg_colour(_get_colour());
}

void SkillMenuEntry::_set_progress()
{
    if (you.skills[m_sk] == 27)
        m_progress->set_text("");
    else
        m_progress->set_text(make_stringf("(%2d%%)",
                                          get_skill_percentage(m_sk)));
        m_progress->set_fg_colour(CYAN);
}

void SkillMenuEntry::_set_aptitude()
{
    std::string text = "<red>";

    const int apt = species_apt(m_sk, you.species);
    const int ct_bonus = crosstrain_bonus(m_sk);
    const bool show_all = is_set(SKMF_DISP_ALL);

    if (apt != 0)
        text += make_stringf("%+d", apt);
    else
        text += make_stringf(" %d", apt);

    text += "</red>";

    if (crosstrain_other(m_sk, show_all) || ct_bonus > 1)
    {
        text += "<lightblue>";
        text += crosstrain_other(m_sk, show_all) ? "*" : " ";

        if ( ct_bonus > 1)
            text += make_stringf("+%d", ct_bonus * 2);

        text += "</lightblue>";
    }
    else if (antitrain_other(m_sk, show_all) || is_antitrained(m_sk))
    {
        text += "<magenta>";
        text += antitrain_other(m_sk, show_all) ? "*" : " ";
        if (is_antitrained(m_sk))
            text += "-4";

        text += "</magenta>";
    }

    m_aptitude->set_text(text);
}

void SkillMenuEntry::_set_reskill_progress()
{
    std::string text;
    if (m_sk == you.transfer_from_skill)
        text = "  *  ";
    else if (m_sk == you.transfer_to_skill)
    {
        text += make_stringf("(%2d%%)",
                             (you.transfer_total_skill_points
                             - you.transfer_skill_points)
                                 * 100 / you.transfer_total_skill_points);
    }
    else
        text = "";

    m_progress->set_text(text);
    m_progress->set_fg_colour(GREEN);
}

void SkillMenuEntry::_set_new_level()
{
    int new_level = 0;
    if (you.skills[m_sk] > 0 && is_set(SKMF_DO_RESKILL_FROM)
        || m_sk == you.transfer_from_skill)
    {
        new_level = transfer_skill_points(m_sk, m_sk,
                                          skill_transfer_amount(m_sk), true);
        m_progress->set_fg_colour(BROWN);
    }
    else if (is_set(SKMF_DO_RESKILL_TO))
    {
        new_level = transfer_skill_points(you.transfer_from_skill, m_sk,
                                          you.transfer_skill_points, true);
        m_progress->set_fg_colour(GREEN);
    }

    if (_is_selectable() || m_sk == you.transfer_from_skill)
        m_progress->set_text(make_stringf("-> %2d", new_level));
    else
        m_progress->set_text("");
}

void SkillMenuEntry::_set_title()
{
    m_name->allow_highlight(false);
    m_name->set_text("    Skill");
    m_level->set_text("Lvl");

    if (is_set(SKMF_DISP_PROGRESS))
        m_progress->set_text("Progr");
    else if (is_set(SKMF_DISP_RESKILL))
        m_progress->set_text("Trnsf");
    else if (is_set(SKMF_DISP_POINTS))
        m_progress->set_text("Pnts");
    else if (is_set(SKMF_RESKILLING))
        m_progress->set_text("->Lvl");

    m_name->set_fg_colour(BLUE);
    m_level->set_fg_colour(BLUE);
    m_progress->set_fg_colour(BLUE);

    m_aptitude->set_text("<blue>Apt </blue>");
}

void SkillMenuEntry::_clear()
{
    m_sk = SK_NONE;

    m_name->set_text("");
    m_level->set_text("");
    m_progress->set_text("");
    m_aptitude->set_text("");

    m_name->set_id(-1);
    m_name->clear_hotkeys();
    m_name->allow_highlight(false);
}

#ifdef DEBUG_DIAGNOSTICS
void SkillMenuEntry::_set_points()
{
    m_progress->set_text(make_stringf("%5d", you.skill_points[m_sk]));
    m_progress->set_fg_colour(LIGHTGREY);
}
#endif

#define CURRENT_ACTION_SIZE 24
#define NEXT_ACTION_SIZE    15
#define NEXT_DISPLAY_SIZE   18
#define SHOW_ALL_SIZE       16
SkillMenu::SkillMenu(int flags) : PrecisionMenu(), m_flags(flags),
    m_disp_queue()
{
    SkillMenuEntry::m_skm = this;
    m_max_col    = get_number_of_cols();
    // Don't want the help line to appear too far down a big window.
    m_max_ln   = std::min(30, get_number_of_lines());

    m_ff = new MenuFreeform();
    m_ff->init(coord_def(1, 1),
               coord_def(m_max_col, m_max_ln + 1), "freeform");
    attach_object(m_ff);
    set_active_object(m_ff);

    _init_title();
    for (int ln = 0; ln < SK_ARR_LN; ++ln)
        for (int col = 0; col < SK_ARR_COL; ++col)
        {
            m_skills[ln][col] = SkillMenuEntry(coord_def(1 + 40*col, 2 + ln),
                                               m_ff);
        }

    m_help = new FormattedTextItem();
    m_help->set_bounds(coord_def(1, m_max_ln - 2),
                       coord_def(m_max_col, m_max_ln));
    m_ff->attach_item(m_help);

    _init_disp_queue();
    _init_footer();

    _set_title();
    _set_skills();
    _set_help();
    _set_footer();


#ifdef USE_TILE
    tiles.get_crt()->attach_menu(this);
    // Black and White highlighter looks kinda bad on tiles
    m_highlighter = new BoxMenuHighlighter(this);
#else
    m_highlighter = new BlackWhiteHighlighter(this);
#endif
    m_highlighter->init(coord_def(-1,-1), coord_def(-1,-1), "highlighter");
    attach_object(m_highlighter);

    m_ff->set_visible(true);
    m_highlighter->set_visible(true);
    m_help->set_visible(true);
}

bool SkillMenu::is_set(int flag) const
{
    return (m_flags & flag);
}

void SkillMenu::change_action()
{
    const int new_action = _get_next_action();
    m_flags &= ~SKMF_ACTION_MASK;
    m_flags |= new_action;
    _set_help(new_action);
    _refresh_names();
    _set_footer();
}

void SkillMenu::change_display()
{
    const int new_disp = _get_next_display();
    m_flags &= ~SKMF_DISP_MASK;
    m_flags |= new_disp;
    m_disp_queue.pop();
    m_disp_queue.push(new_disp);
    _set_help(new_disp);
    _refresh_display();
    _set_footer();
}

void SkillMenu::toggle_practise(skill_type sk)
{
    you.practise_skill[sk] = !you.practise_skill[sk];
    _refresh_name(sk);
}

void SkillMenu::show_description(skill_type sk)
{
    describe_skill(sk);
    clrscr();
#ifdef USE_TILE
    tiles.get_crt()->attach_menu(this);
#endif
}

void SkillMenu::toggle_show_all()
{
    m_flags ^= SKMF_DISP_ALL;
    _set_skills();
    _set_footer();
}

void SkillMenu::clear_selections()
{
    _clear_selections();
}

void SkillMenu::_init_disp_queue()
{
#ifdef DEBUG_DIAGNOSTICS
    m_disp_queue.push(SKMF_DISP_POINTS);
#endif

    m_disp_queue.push(SKMF_DISP_PROGRESS);

    if (is_set(SKMF_RESKILLING))
        m_disp_queue.push(SKMF_DISP_NEW_LEVEL);
    else if (!is_invalid_skill(you.transfer_to_skill))
        m_disp_queue.push(SKMF_DISP_RESKILL);
}

void SkillMenu::_init_title()
{
    m_title = new NoSelectTextItem();
    m_title->set_bounds(coord_def(1, 1), coord_def(m_max_col, 2));
    m_title->set_fg_colour(WHITE);
    m_ff->attach_item(m_title);
    m_title->set_visible(true);
}

void SkillMenu::_init_footer()
{
    coord_def coord = coord_def(1, m_max_ln);
    m_current_action = new NoSelectTextItem();
    _add_item(m_current_action, m_ff, CURRENT_ACTION_SIZE, coord);
    m_current_action->set_fg_colour(WHITE);

    m_next_action = new TextItem();
    _add_item(m_next_action, m_ff, NEXT_ACTION_SIZE, coord);
    m_next_action->set_highlight_colour(WHITE);
    m_next_action->set_fg_colour(WHITE);
    m_next_action->add_hotkey('?');
    m_next_action->set_id(-2);

    if (m_disp_queue.size() > 1)
    {
        m_next_display = new TextItem();
        _add_item(m_next_display, m_ff, NEXT_DISPLAY_SIZE, coord);
        m_next_display->set_highlight_colour(WHITE);
        m_next_display->set_fg_colour(WHITE);
        m_next_display->add_hotkey('!');
        m_next_display->set_id(-3);
    }

    m_show_all = new TextItem();
    _add_item(m_show_all, m_ff, SHOW_ALL_SIZE, coord);
    m_show_all->set_highlight_colour(WHITE);
    m_show_all->set_fg_colour(WHITE);
    m_show_all->add_hotkey('*');
    m_show_all->set_id(-4);

}

void SkillMenu::_refresh_name(skill_type sk)
{
    for (int col = 0; col < SK_ARR_COL; ++col)
        for (int ln = 0; ln < SK_ARR_LN; ++ln)
            if (m_skills[ln][col].get_skill() == sk)
                m_skills[ln][col].set_name(true);
}

void SkillMenu::_refresh_names()
{
    SkillMenuEntry::m_letter = 'Z';
    for (int col = 0; col < SK_ARR_COL; ++col)
        for (int ln = 0; ln < SK_ARR_LN; ++ln)
                m_skills[ln][col].set_name(false);
}

void SkillMenu::_refresh_display()
{
    for (int ln = 0; ln < SK_ARR_LN; ++ln)
        for (int col = 0; col < SK_ARR_COL; ++col)
                m_skills[ln][col].set_display();
}

void SkillMenu::_set_title()
{
    const char* format = "Transfer Knowledge: select the %s skill";
    std::string t;
    if (is_set(SKMF_DO_RESKILL_FROM))
        t = make_stringf(format, "source");
    else if (is_set(SKMF_DO_RESKILL_TO))
        t = make_stringf(format, "destination");
    else
    {
#ifdef DEBUG_DIAGNOSTICS
        t = make_stringf("You have %d points of unallocated experience "
                         " (cost lvl %d; total %d).\n\n",
                         you.exp_available, you.skill_cost_level,
                         you.total_skill_points);
#else
        t = make_stringf(" You have %s unallocated experience.\n\n",
                         you.exp_available == 0? "no" :
                         make_stringf("%d point%s of", you.exp_available,
                                      you.exp_available == 1? "" : "s").c_str()
                        );
#endif
    }

    m_title->set_text(t);
}

void SkillMenu::_set_skills()
{
    int previous_active;
    if (m_ff->get_active_item() != NULL)
        previous_active = m_ff->get_active_item()->get_id();
    else
        previous_active = -1;

    SkillMenuEntry::m_letter = 'Z';

    int col = 0, ln = 0;

    for (int i = 0; i < ndisplayed_skills; ++i)
    {
        skill_type sk = skill_display_order[i];

        if (sk == SK_COLUMN_BREAK)
        {
            while (ln < SK_ARR_LN)
            {
                m_skills[ln][col].set_skill();
                ln++;
            }
            col++;
            ln = 0;
            continue;
        }
        else if (!is_invalid_skill(sk) && you.skills[sk] == 0
                 && ! is_set(SKMF_DISP_ALL))
        {
            continue;
        }
        else if (sk == SK_TITLE)
            m_skills[ln][col].set_skill(sk);
        else
        {
            m_skills[ln][col].set_skill(sk);
            if (previous_active == -1)
                previous_active = m_skills[ln][col].get_id();
        }
        ++ln;
    }
    m_ff->set_active_item(previous_active);
}

void SkillMenu::_set_help(int flag)
{
    if (flag == SKMF_NONE)
    {
        if (is_set(SKMF_DO_RESKILL_FROM))
            flag = SKMF_DO_RESKILL_FROM;
        else if (is_set(SKMF_DO_RESKILL_TO))
            flag = SKMF_DO_RESKILL_TO;
        else if (is_set(SKMF_DISP_RESKILL))
            flag = SKMF_DISP_RESKILL;
        else
            flag = SKMF_DISP_APTITUDE;
    }

    std::string help;
    switch (flag)
    {
    case SKMF_DO_PRACTISE:
        help = "Press the letter of a skill to choose whether you want to "
               "practise it. Skills marked with '-' will train very slowly.";
        break;
    case SKMF_DO_SHOW_DESC:
        help = "Press the letter of a skill to read its description.";
        break;
    case SKMF_DO_RESKILL_FROM:
        help = "Select a skill as the source of the knowledge transfer. The "
               "chosen skill will be reduced to the level showned in "
               "<brown>brown</brown>.";
        break;
    case SKMF_DO_RESKILL_TO:
        help = "Select a skill as the destination of the knowledge transfer. "
               "The chosen skill will be raised to the level showned in "
               "<green>green</green>.";
        break;
    case SKMF_DISP_PROGRESS:
        help = "The percentage of the progress done before reaching next level"
               " is in <cyan>cyan</cyan>.";
        break;
    case SKMF_DISP_APTITUDE:
        help = "The species aptitude is in <red>red</red>. Crosstraining is "
               "in <blue>blue</blue>, antitraining in "
               "<magenta>magenta</magenta>. The skill responsible for the "
               "bonus or malus is marked with '*'.";
        break;
    case SKMF_DISP_RESKILL:
        help = "The progress of the knowledge transfer is displayed in "
               "<green>green</green> in front of the skill receiving the "
               "knowledge. The donating skill is marked with '*'.";
        break;
    default:
        return;
    }
    m_help->set_fg_colour(LIGHTGREY);
    m_help->set_text(help);
}

void SkillMenu::_set_footer()
{
    std::string text;
    if (is_set(SKMF_DO_PRACTISE))
        text = "toggle training";
    else if (is_set(SKMF_DO_SHOW_DESC))
        text = "read description";
    else if (is_set(SKMF_DO_RESKILL_FROM))
        text = "select source";
    else if (is_set(SKMF_DO_RESKILL_TO))
        text = "select destination";

    m_current_action->set_text(make_stringf("[a-%c:%-18s]",
                                            SkillMenuEntry::m_letter.letter,
                                            text.c_str()));

    switch (_get_next_action())
    {
    case SKMF_DO_PRACTISE:
        text = "training";
        break;
    case SKMF_DO_SHOW_DESC:
        text = "description";
        break;
    case SKMF_DO_RESKILL_FROM:
    case SKMF_DO_RESKILL_TO:
        text = "transfer";
    }

    m_next_action->set_text(make_stringf("[?:%-11s]", text.c_str()));

    if (m_disp_queue.size() > 1)
    {
        switch (_get_next_display())
        {
        case SKMF_DISP_PROGRESS:
            text = "progress";
            break;
        case SKMF_DISP_POINTS:
            text = "points";
            break;
        case SKMF_DISP_RESKILL:
        case SKMF_DISP_NEW_LEVEL:
            text = "transfer";
            break;
        default:
            text = "";
        }

        m_next_display->set_text(make_stringf("[!:show %-9s]", text.c_str()));
    }

    text = is_set(SKMF_DISP_ALL) ? "hide unknown" : "show all";
    m_show_all->set_text(make_stringf("[*:%-12s]", text.c_str()));
}

int SkillMenu::_get_next_action() const
{
    if (is_set(SKMF_DO_PRACTISE))
        return SKMF_DO_SHOW_DESC;
    else if (is_set(SKMF_DO_SHOW_DESC) && is_set(SKMF_RESKILLING))
    {
        return (is_invalid_skill(you.transfer_to_skill) ? SKMF_DO_RESKILL_FROM
                                                        : SKMF_DO_RESKILL_TO);
    }
    else
        return SKMF_DO_PRACTISE;
}

int SkillMenu::_get_next_display() const
{
    return m_disp_queue.front();
}

void skill_menu(bool reskilling)
{
    int flags = SKMF_NONE;

    if (reskilling)
    {
        if (is_invalid_skill(you.transfer_from_skill))
            flags |= SKMF_DO_RESKILL_FROM;
        else
            flags |= SKMF_DO_RESKILL_TO;

        flags |= SKMF_RESKILLING | SKMF_DISP_NEW_LEVEL;
    }
    else
    {
        flags |= SKMF_DO_PRACTISE;

        if (is_invalid_skill(you.transfer_from_skill))
            flags |= SKMF_DISP_PROGRESS;
        else
            flags |= SKMF_DISP_RESKILL;
    }

    // For now, aptitudes are always showned.
    flags |= SKMF_DISP_APTITUDE;

#ifdef DEBUG_DIAGNOSTICS
    flags |= SKMF_DISP_ALL;
#endif

    clrscr();
    SkillMenu skm(flags);
    int keyn;

    while (true)
    {
        skm.draw_menu();
        keyn = getch_ck();

        if (!skm.process_key(keyn))
        {
            switch (keyn)
            {
            case CK_UP:
            case CK_DOWN:
            case CK_LEFT:
            case CK_RIGHT:
                continue;
            default:
                return;
            }
        }
        else
        {
            std::vector<MenuItem*> selection = skm.get_selected_items();
            skm.clear_selections();
            // There should only be one selection, otherwise something broke
            if (selection.size() != 1)
                continue;
            int sel_id = selection.at(0)->get_id();
            switch (sel_id)
            {
            case -2:
                skm.change_action();
                break;
            case -3:
                skm.change_display();
                break;
            case -4:
                skm.toggle_show_all();
                break;
            default:
                skill_type sk = static_cast<skill_type>(sel_id);
                ASSERT(!is_invalid_skill(sk));

                if (skm.is_set(SKMF_DO_PRACTISE))
                    skm.toggle_practise(sk);
                else if (skm.is_set(SKMF_DO_SHOW_DESC))
                    skm.show_description(sk);
                else if (skm.is_set(SKMF_DO_RESKILL_FROM))
                {
                    you.transfer_from_skill = sk;
                    return;
                }
                else if (skm.is_set(SKMF_DO_RESKILL_TO))
                {
                    you.transfer_to_skill = sk;
                    return;
                }
                else
                    return;
            }
        }
    }
}

const char *skill_name(skill_type which_skill)
{
    return (skills[which_skill][0]);
}

skill_type str_to_skill(const std::string &skill)
{
    for (int i = SK_FIRST_SKILL; i < NUM_SKILLS; ++i)
        if (skills[i][0] && skill == skills[i][0])
            return (static_cast<skill_type>(i));

    return (SK_FIGHTING);
}

static std::string _stk_adj_cap()
{
    return species_name(Skill_Species, false, true);
}

static std::string _stk_genus_cap()
{
    if (Skill_Species == SP_CAT)
        return "Cat";
    return species_name(Skill_Species, true, false);
}

static std::string _stk_genus_nocap()
{
    std::string s = _stk_genus_cap();
    return (lowercase(s));
}

static std::string _stk_genus_short_cap()
{
    return (Skill_Species == SP_DEMIGOD ? "God" :
            Skill_Species == SP_CAT     ? "Cat" :
            _stk_genus_cap());
}

static std::string _stk_walker()
{
    return (Skill_Species == SP_NAGA  ? "Slider" :
            Skill_Species == SP_KENKU ? "Glider"
                                      : "Walker");
}

static std::string _stk_weight()
{
    switch (Skill_Species)
    {
    case SP_OGRE:
    case SP_TROLL:
        return "Heavy";

    case SP_NAGA:
    case SP_CENTAUR:
        return "Cruiser";

    default:
        return "Middle";

    case SP_HIGH_ELF:
    case SP_DEEP_ELF:
    case SP_SLUDGE_ELF:
    case SP_KENKU:
        return "Light";

    case SP_HALFLING:
    case SP_KOBOLD:
        return "Feather";

    case SP_SPRIGGAN:
        return "Fly";

    case SP_CAT:
        return "Bacteria"; // not used
    }
}

static skill_title_key_t _skill_title_keys[] = {
    stk("Adj", _stk_adj_cap),
    stk("Genus", _stk_genus_cap),
    stk("genus", _stk_genus_nocap),
    stk("Genus_Short", _stk_genus_short_cap),
    stk("Walker", _stk_walker),
    stk("Weight", _stk_weight),
};

static std::string _replace_skill_keys(const std::string &text)
{
    std::string::size_type at = 0, last = 0;
    std::ostringstream res;
    while ((at = text.find('@', last)) != std::string::npos)
    {
        res << text.substr(last, at - last);
        const std::string::size_type end = text.find('@', at + 1);
        if (end == std::string::npos)
            break;

        const std::string key = text.substr(at + 1, end - at - 1);
        const std::string value = stk::get(key);

        ASSERT(!value.empty());

        res << value;

        last = end + 1;
    }
    if (!last)
        return text;

    res << text.substr(last);
    return res.str();
}

unsigned get_skill_rank(unsigned skill_lev)
{
    // Translate skill level into skill ranking {dlb}:
    return ((skill_lev <= 7)  ? 0 :
                            (skill_lev <= 14) ? 1 :
                            (skill_lev <= 20) ? 2 :
                            (skill_lev <= 26) ? 3
                            /* level 27 */    : 4);
}

std::string skill_title_by_rank(skill_type best_skill, uint8_t skill_rank,
                                int species, int str, int dex, int god)
{
    // paranoia
    if (is_invalid_skill(best_skill))
        return ("Adventurer");

    if (species == -1)
        species = you.species;

    if (str == -1)
        str = you.base_stats[STAT_STR];

    if (dex == -1)
        dex = you.base_stats[STAT_DEX];

    if (god == -1)
        god = you.religion;

    // Increment rank by one to "skip" skill name in array {dlb}:
    ++skill_rank;

    std::string result;

    if (best_skill < NUM_SKILLS)
    {
        // Note that ghosts default to (dex == str) and god == no_god, due
        // to a current lack of that information... the god case is probably
        // suitable for most cases (TSO/Zin/Ely at the very least). -- bwr
        switch (best_skill)
        {
        case SK_UNARMED_COMBAT:
            if (species == SP_CAT)
            {
                result = claw_and_tooth_titles[skill_rank];
                break;
            }
            result = (dex >= str) ? martial_arts_titles[skill_rank]
                                  : skills[best_skill][skill_rank];

            break;

        case SK_INVOCATIONS:
            if (god == GOD_NO_GOD)
                result = skills[best_skill][skill_rank];
            else
                result = god_title((god_type)god, (species_type)species);
            break;

        case SK_BOWS:
            if (player_genus(GENPC_ELVEN, static_cast<species_type>(species))
                && skill_rank == 5)
            {
                result = "Master Archer";
                break;
            }
            else
                result = skills[best_skill][skill_rank];
            break;

        case SK_SPELLCASTING:
            if (player_genus(GENPC_OGREISH, static_cast<species_type>(species)))
            {
                result = "Ogre Mage";
                break;
            }
            // else fall-through
        default:
            result = skills[best_skill][skill_rank];
            break;
        }
    }

    {
        unwind_var<species_type> sp(Skill_Species,
                                    static_cast<species_type>(species));
        result = _replace_skill_keys(result);
    }

    return (result.empty() ? std::string("Invalid Title")
                           : result);
}

std::string skill_title(skill_type best_skill, uint8_t skill_lev,
                        int species, int str, int dex, int god)
{
    return skill_title_by_rank(best_skill, get_skill_rank(skill_lev), species, str, dex, god);
}

std::string player_title()
{
    const skill_type best = best_skill(SK_FIRST_SKILL, SK_LAST_SKILL);
    return (skill_title(best, you.skills[ best ]));
}

skill_type best_skill(skill_type min_skill, skill_type max_skill,
                      skill_type excl_skill)
{
    skill_type ret = SK_FIGHTING;
    unsigned int best_skill_level = 0;
    unsigned int best_position = 1000;

    for (int i = min_skill; i <= max_skill; i++)    // careful!!!
    {
        skill_type sk = static_cast<skill_type>(i);
        if (sk == excl_skill || is_invalid_skill(sk))
            continue;

        if (you.skills[sk] > best_skill_level)
        {
            ret = sk;
            best_skill_level = you.skills[sk];
            best_position = you.skill_order[sk];

        }
        else if (you.skills[sk] == best_skill_level
                && you.skill_order[sk] < best_position)
        {
            ret = sk;
            best_position = you.skill_order[sk];
        }
    }

    return static_cast<skill_type>(ret);
}

// Calculate the skill_order array from scratch.
//
// The skill order array is used for breaking ties in best_skill.
// This is done by ranking each skill by the order in which it
// has attained its current level (the values are the number of
// skills at or above that level when the current skill reached it).
//
// In this way, the skill which has been at a level for the longest
// is judged to be the best skill (thus, nicknames are sticky)...
// other skills will have to attain the next level higher to be
// considered a better skill (thus, the first skill to reach level 27
// becomes the characters final nickname).
//
// As for other uses of best_skill:  this method is still appropriate
// in that there is no additional advantage anywhere else in the game
// for partial skill levels.  Besides, its probably best if the player
// isn't able to micromanage at that level.  -- bwr
void init_skill_order(void)
{
    for (int i = SK_FIRST_SKILL; i < NUM_SKILLS; i++)
    {
        skill_type si = static_cast<skill_type>(i);
        if (is_invalid_skill(si))
        {
            you.skill_order[si] = MAX_SKILL_ORDER;
            continue;
        }

        const unsigned int i_points = you.skill_points[si]
                                      / species_apt_factor(si);

        you.skill_order[si] = 0;

        for (int j = SK_FIRST_SKILL; j < NUM_SKILLS; j++)
        {
            skill_type sj = static_cast<skill_type>(j);
            if (si == sj || is_invalid_skill(sj))
                continue;

            const unsigned int j_points = you.skill_points[sj]
                                          / species_apt_factor(sj);

            if (you.skills[sj] == you.skills[si]
                && (j_points > i_points
                    || (j_points == i_points && sj > si)))
            {
                you.skill_order[si]++;
            }
        }
    }
}

void calc_hp()
{
    you.hp_max = get_real_hp(true, false);
    deflate_hp(you.hp_max, false);
}

void calc_mp()
{
    you.max_magic_points = get_real_mp(true);
    you.magic_points = std::min(you.magic_points, you.max_magic_points);
    you.redraw_magic_points = true;
}

// What aptitude value corresponds to doubled skill learning
// (i.e., old-style aptitude 50).
#define APT_DOUBLE 4

static float _apt_to_factor(int apt)
{
    return (1 / exp(log(2) * apt / APT_DOUBLE));
}

// Base skill cost, i.e. old-style human aptitudes.
static int _base_cost(skill_type sk)
{
    switch (sk)
    {
    case SK_SPELLCASTING:
        return 130;
    case SK_INVOCATIONS:
    case SK_EVOCATIONS:
        return 80;
    // Quick fix for the fact that stealth can't be gained fast enough
    // to keep up with the monster levels. This was a skill points bonus
    // in _exercise2 and was changed to a reduced base_cost to keep
    // total_skill_points progression the same for all skills.
    case SK_STEALTH:
        return 50;
    default:
        return 100;
    }
}

unsigned int skill_exp_needed(int lev)
{
    switch (lev)
    {
    case 0:  return 0;
    case 1:  return 200;
    case 2:  return 300;
    case 3:  return 500;
    case 4:  return 750;
    case 5:  return 1050;
    case 6:  return 1350;
    case 7:  return 1700;
    case 8:  return 2100;
    case 9:  return 2550;
    case 10: return 3150;
    case 11: return 3750;
    case 12: return 4400;
    case 13: return 5250;
    default: return 6200 + 1800 * (lev - 14);
    }
    return 0;
}

unsigned int skill_exp_needed(int lev, skill_type sk, species_type sp)
{
    return skill_exp_needed(lev) * species_apt_factor(sk, sp)
           * _base_cost(sk) / 100;
}

int species_apt(skill_type skill, species_type species)
{
    static bool spec_skills_initialised = false;
    if (!spec_skills_initialised)
    {
        // Setup sentinel values to find errors more easily.
        const int sentinel = -20; // this gives cost 3200
        for (int sp = 0; sp < NUM_SPECIES; ++sp)
            for (int sk = 0; sk < NUM_SKILLS; ++sk)
                _spec_skills[sp][sk] = sentinel;
        for (unsigned i = 0; i < ARRAYSZ(species_skill_aptitudes); ++i)
        {
            const species_skill_aptitude &ssa(species_skill_aptitudes[i]);
            ASSERT(_spec_skills[ssa.species][ssa.skill] == sentinel);
            _spec_skills[ssa.species][ssa.skill] = ssa.aptitude;
        }
        spec_skills_initialised = true;
    }

    return _spec_skills[species][skill];
}

float species_apt_factor(skill_type sk, species_type sp)
{
    return _apt_to_factor(species_apt(sk, sp));
}

static std::vector<skill_type> _get_crosstrain_skills(skill_type sk)
{
    std::vector<skill_type> ret;

    switch (sk)
    {
    case SK_SHORT_BLADES:
        ret.push_back(SK_LONG_BLADES);
        return ret;
    case SK_LONG_BLADES:
        ret.push_back(SK_SHORT_BLADES);
        return ret;
    case SK_AXES:
    case SK_STAVES:
        ret.push_back(SK_POLEARMS);
        ret.push_back(SK_MACES_FLAILS);
        return ret;
    case SK_MACES_FLAILS:
    case SK_POLEARMS:
        ret.push_back(SK_AXES);
        ret.push_back(SK_STAVES);
        return ret;
    case SK_SLINGS:
        ret.push_back(SK_THROWING);
        return ret;
    case SK_THROWING:
        ret.push_back(SK_SLINGS);
        return ret;
    default:
        return ret;
    }
}

float crosstrain_bonus(skill_type sk)
{
    int bonus = 1;

    std::vector<skill_type> crosstrain_skills = _get_crosstrain_skills(sk);

    for (unsigned int i = 0; i < crosstrain_skills.size(); ++i)
        if (you.skills[crosstrain_skills[i]] > you.skills[sk])
            bonus *= 2;

    return bonus;
}

bool crosstrain_other(skill_type sk, bool show_zero)
{
    std::vector<skill_type> crosstrain_skills = _get_crosstrain_skills(sk);

    for (unsigned int i = 0; i < crosstrain_skills.size(); ++i)
        if (you.skills[crosstrain_skills[i]] < you.skills[sk]
            && (you.skills[crosstrain_skills[i]] != 0 || show_zero))
        {
            return true;
        }

    return false;
}

static skill_type _get_opposite(skill_type sk)
{
    switch (sk)
    {
    case SK_FIRE_MAGIC  : return SK_ICE_MAGIC;   break;
    case SK_ICE_MAGIC   : return SK_FIRE_MAGIC;  break;
    case SK_AIR_MAGIC   : return SK_EARTH_MAGIC; break;
    case SK_EARTH_MAGIC : return SK_AIR_MAGIC;   break;
    default: return SK_NONE;
    }
}

bool is_antitrained(skill_type sk)
{
    skill_type opposite = _get_opposite(sk);
    if (opposite == SK_NONE)
        return false;

    return (you.skills[sk] < you.skills[opposite]
            || you.skills[sk] == you.skills[opposite]
               && you.skill_order[sk] > you.skill_order[opposite]
               && you.skills[sk] != 0);
}

bool antitrain_other(skill_type sk, bool show_zero)
{
    skill_type opposite = _get_opposite(sk);
    if (opposite == SK_NONE)
        return false;

    return ((you.skills[opposite] != 0 || show_zero)
            && (you.skills[sk] > you.skills[opposite]
                || you.skills[sk] == you.skills[opposite]
                   && you.skill_order[sk] < you.skill_order[opposite]));
}

void wield_warning(bool newWeapon)
{
    // Early out - no weapon.
    if (!you.weapon())
         return;

    const item_def& wep = *you.weapon();

    // Early out - don't warn for non-weapons or launchers.
    if (wep.base_type != OBJ_WEAPONS || is_range_weapon(wep))
        return;

    // Don't warn if the weapon is OK, of course.
    if (effective_stat_bonus() > -4)
        return;

    std::string msg;

    // We know if it's an artefact because we just wielded
    // it, so no information leak.
    if (is_artefact(wep))
        msg = "the";
    else if (newWeapon)
        msg = "this";
    else
        msg = "your";
    msg += " " + wep.name(DESC_BASENAME);
    const char* mstr = msg.c_str();

    if (you.strength() < you.dex())
    {
        if (you.strength() < 11)
        {
            mprf(MSGCH_WARN, "You have %strouble swinging %s.",
                 (you.strength() < 7) ? "" : "a little ", mstr);
        }
        else
        {
            mprf(MSGCH_WARN, "You'd be more effective with "
                 "%s if you were stronger.", mstr);
        }
    }
    else
    {
        if (you.dex() < 11)
        {
            mprf(MSGCH_WARN, "Wielding %s is %s awkward.",
                 mstr, (you.dex() < 7) ? "fairly" : "a little");
        }
        else
        {
            mprf(MSGCH_WARN, "You'd be more effective with "
                 "%s if you were nimbler.", mstr);
        }
    }
}

bool is_invalid_skill(skill_type skill)
{
    if (skill < 0 || skill >= NUM_SKILLS)
        return (true);

    if (skill > SK_UNARMED_COMBAT && skill < SK_SPELLCASTING)
        return (true);

    return (false);
}

void dump_skills(std::string &text)
{
    char tmp_quant[20];
    for (uint8_t i = 0; i < NUM_SKILLS; i++)
    {
        if (you.skills[i] > 0)
        {
            text += ((you.skills[i] == 27)   ? " * " :
                     (you.practise_skill[i]) ? " + "
                                             : " - ");

            text += "Level ";
            itoa(you.skills[i], tmp_quant, 10);
            text += tmp_quant;
            text += " ";
            text += skill_name(static_cast<skill_type>(i));
            text += "\n";
        }
    }
}

int skill_transfer_amount(skill_type sk)
{
    ASSERT(!is_invalid_skill(sk));
    if (you.skill_points[sk] < 1000)
        return you.skill_points[sk];
    else
        return std::max<int>(1000, you.skill_points[sk] / 2);
}

// Transfer skill points from one skill to another (Ashenzari transfer
// knowledge ability). If simu, it just simulates the transfer and don't
// change anything. It returns the new level of tsk.
int transfer_skill_points(skill_type fsk, skill_type tsk, int skp_max,
                          bool simu)
{
    ASSERT(!is_invalid_skill(fsk) && !is_invalid_skill(tsk));

    const int penalty = 90; // 10% XP penalty
    int total_skp_lost   = 0; // skill points lost in fsk.
    int total_skp_gained = 0; // skill points gained in tsk.
    int fsk_level = you.skills[fsk];
    int tsk_level = you.skills[tsk];
    int fsk_points = you.skill_points[fsk];
    int tsk_points = you.skill_points[tsk];
    int fsk_ct_points = you.ct_skill_points[fsk];
    int tsk_ct_points = you.ct_skill_points[tsk];

#ifdef DEBUG_DIAGNOSTICS
    if (!simu && you.ct_skill_points[fsk] > 0)
        dprf("ct_skill_points[%s]: %d", skill_name(fsk), you.ct_skill_points[fsk]);
#endif

    // We need to transfer by small steps and updating skill levels each time
    // so that cross/anti-training are handled properly.
    while (total_skp_lost < skp_max
           && (simu || total_skp_lost < (int)you.transfer_skill_points))
    {
        int skp_lost = std::min(20, skp_max - total_skp_lost);
        int skp_gained = skp_lost * penalty / 100;

        float ct_bonus = crosstrain_bonus(tsk);
        if (ct_bonus > 1 && fsk != tsk)
        {
            skp_gained *= ct_bonus;
            you.ct_skill_points[tsk] += (1 - 1 / ct_bonus) * skp_gained;
        }
        else if (is_antitrained(tsk))
            skp_gained /= ANTITRAIN_PENALTY;

        ASSERT(you.skill_points[fsk] > you.ct_skill_points[fsk]);

        int ct_penalty = skp_lost * you.ct_skill_points[fsk]
                          / (you.skill_points[fsk] - you.ct_skill_points[fsk]);
        ct_penalty = std::min<int>(ct_penalty, you.ct_skill_points[fsk]);
        you.ct_skill_points[fsk] -= ct_penalty;
        skp_lost += ct_penalty;

        if (!simu)
        {
            skp_lost = std::min<int>(skp_lost, you.transfer_skill_points
                                               - total_skp_lost);
        }

        total_skp_lost += skp_lost;
        total_skp_gained += skp_gained;
        change_skill_points(fsk, -skp_lost, false);
        if (fsk != tsk)
        {
            change_skill_points(tsk, skp_gained, false);
            if (you.skills[tsk] == 27)
                break;
        }
    }

    int new_level = you.skills[tsk];
    // Restore the level
    you.skills[fsk] = fsk_level;
    you.skills[tsk] = tsk_level;

    if (simu)
    {
        you.skill_points[fsk] = fsk_points;
        you.skill_points[tsk] = tsk_points;
        you.ct_skill_points[fsk] = fsk_ct_points;
        you.ct_skill_points[tsk] = tsk_ct_points;
    }
    else
    {
        // Perform the real level up
        check_skill_level_change(fsk);
        check_skill_level_change(tsk);
        you.transfer_skill_points -= total_skp_lost;

        dprf("skill %s lost %d points", skill_name(fsk), total_skp_lost);
        dprf("skill %s gained %d points", skill_name(tsk), total_skp_gained);
#ifdef DEBUG_DIAGNOSTICS
        if (you.ct_skill_points[fsk] > 0)
            dprf("ct_skill_points[%s]: %d", skill_name(fsk), you.ct_skill_points[fsk]);
#endif

        if (you.transfer_skill_points <= 0 || you.skills[tsk] == 27)
            ashenzari_end_transfer(true);
        else if (you.transfer_skill_points > 0)
            dprf("%d skill points left to transfer", you.transfer_skill_points);
    }
    return new_level;
}
