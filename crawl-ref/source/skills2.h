/**
 * @file
 * @brief More skill related functions.
**/


#ifndef SKILLS2_H
#define SKILLS2_H

const int MAX_SKILL_ORDER = 100;

#include <queue>
#include "enum.h"
#include "menu.h"
#include "player.h"

int get_skill_percentage(const skill_type x);
const char *skill_name(skill_type which_skill);
skill_type str_to_skill(const std::string &skill);

std::string skill_title(
    skill_type best_skill, uint8_t skill_lev,
    // these used for ghosts and hiscores:
    int species = -1, int str = -1, int dex = -1, int god = -1);
std::string skill_title_by_rank(
    skill_type best_skill, uint8_t skill_rank,
    // these used for ghosts and hiscores:
    int species = -1, int str = -1, int dex = -1, int god = -1);
unsigned get_skill_rank(unsigned skill_lev);

std::string player_title();

skill_type best_skill(skill_type min_skill, skill_type max_skill,
                      skill_type excl_skill = SK_NONE);
void init_skill_order();

void calc_mp();
void calc_hp();
bool is_useless_skill(int skill);

int species_apt(skill_type skill, species_type species = you.species);
float species_apt_factor(skill_type sk, species_type sp = you.species);
unsigned int skill_exp_needed(int lev);
unsigned int skill_exp_needed(int lev, skill_type sk,
                              species_type sp = you.species);

bool compare_skills(skill_type sk1, skill_type sk2);
float crosstrain_bonus(skill_type sk);
bool crosstrain_other(skill_type sk, bool show_zero);
bool is_antitrained(skill_type sk);
bool antitrain_other(skill_type sk, bool show_zero);

void skill_menu(bool reskilling = false);
bool is_invalid_skill(skill_type skill);
void dump_skills(std::string &text);
int skill_transfer_amount(skill_type sk);
int transfer_skill_points(skill_type fsk, skill_type tsk, int skp_max,
                          bool simu);

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

    SK_SPELLCASTING, SK_CONJURATIONS, SK_HEXES, SK_CHARMS, SK_SUMMONINGS,
    SK_NECROMANCY, SK_TRANSLOCATIONS, SK_TRANSMUTATIONS,
    SK_FIRE_MAGIC, SK_ICE_MAGIC, SK_AIR_MAGIC, SK_EARTH_MAGIC, SK_POISON_MAGIC,

    SK_BLANK_LINE,

    SK_INVOCATIONS, SK_EVOCATIONS,

    SK_COLUMN_BREAK,
};

static const int ndisplayed_skills = ARRAYSZ(skill_display_order);

// Skill Menu

enum skill_menu_flags
{
    SKMF_NONE            = 0,

//Actions
    SKMF_DO_PRACTISE     = 0x0001,
    SKMF_DO_SHOW_DESC    = 0x0002,
    SKMF_DO_RESKILL_FROM = 0x0004,
    SKMF_DO_RESKILL_TO   = 0x0008,
    SKMF_ACTION_MASK     = 0x000F,

//Display
    SKMF_DISP_NORMAL     = 0x0010,
    SKMF_DISP_ENHANCED   = 0x0020,
    SKMF_DISP_RESKILL    = 0x0040,
    SKMF_DISP_NEW_LEVEL  = 0x0080,
    SKMF_DISP_POINTS     = 0x0100,
    SKMF_DISP_MASK       = 0x01F0,
    SKMF_DISP_ALL        = 0x0200,
    SKMF_DISP_APTITUDE   = 0x0400,

//Ashenzari transfer knowledge ability set this flag.
    SKMF_RESKILLING      = 0x0800,

    SKMF_SKILL_ICONS     = 0x1000,
    SKMF_SIMPLE          = 0x2000, // Simple mode for tutorial and hint mode.
};

class SkillMenu;

class SkillMenuEntry
{
public:
    static menu_letter m_letter;
    static SkillMenu* m_skm;

    SkillMenuEntry() {};
    SkillMenuEntry(coord_def coord, MenuFreeform* ff);
    void set_skill(skill_type sk = SK_NONE);
    skill_type get_skill() const;
    void set_name(bool keep_hotkey);
    void set_display();
    void refresh(bool keep_hotkey);
    int get_id();
    bool is_set(int flag) const;
    bool is_selectable(bool keep_hotkey = true);
    TextItem* get_name_item() const;

private:
    skill_type m_sk;

    TextItem* m_name;
#ifdef USE_TILE
    TextTileItem* m_name_tile;
#endif
    NoSelectTextItem* m_level;
    NoSelectTextItem* m_progress;
    FormattedTextItem* m_aptitude;

    COLORS _get_colour() const;
    std::string _get_prefix();
    void _set_level();
    void _set_progress();
    void _set_aptitude();
    void _set_reskill_progress();
    void _set_new_level();
    void _set_title();
    void _clear();
#ifdef DEBUG
    void _set_points();
#endif
};

static const int SK_ARR_LN  = (ndisplayed_skills - 1) / 2;
static const int SK_ARR_COL =  2;

class SkillMenu : public PrecisionMenu
{
public:
    SkillMenu(int flags);
    bool is_set(int flag) const;
    void set_flag(int flag);
    void clear_flag(int flag);
    void toggle_flag(int flag);
    void change_action();
    void change_display(bool init = false);
    void toggle_practise(skill_type sk, int keyn);
    void show_description(skill_type sk);
    void toggle_show_all();
    void clear_selections();
    void set_crosstrain();
    void set_antitrain();

private:
    MenuFreeform*        m_ff;
    BoxMenuHighlighter*  m_highlighter;

    int m_flags;
    int m_current_help;
    coord_def m_min_coord;
    coord_def m_max_coord;

    bool m_crosstrain;
    bool m_antitrain;

    SkillMenuEntry  m_skills[SK_ARR_LN][SK_ARR_COL];

    NoSelectTextItem*  m_title;
    FormattedTextItem* m_help;
    NoSelectTextItem*  m_current_action;
    TextItem*          m_next_action;
    TextItem*          m_next_display;
    TextItem*          m_show_all;

    std::queue<int> m_disp_queue;

    SkillMenuEntry* _find_entry(skill_type sk);
    void _init_disp_queue();
    void _init_title();
    void _init_footer(coord_def coord);
    void _refresh_display();
    void _refresh_names();
    void _set_title();
    void _set_skills();
    void _set_help(int flag = SKMF_NONE);
    void _set_footer();
    TextItem* _find_closest_selectable(int start_ln, int col);
    void _set_links();
    int _get_next_action() const;
    int _get_next_display() const;
    bool _skill_enhanced() const;
};

#endif
