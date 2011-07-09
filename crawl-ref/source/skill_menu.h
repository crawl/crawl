/**
 * @file
 * @brief Skill menu.
**/


#ifndef SKILLS_MENU_H
#define SKILLS_MENU_H

#include "enum.h"
#include "menu.h"
#include "skills2.h"

// Skill Menu

enum skill_menu_flags
{
    //Ashenzari transfer knowledge ability.
    SKMF_RESKILLING   = 1<<0,
    SKMF_RESKILL_FROM = 1<<1,
    SKMF_RESKILL_TO   = 1<<2,

    SKMF_CROSSTRAIN   = 1<<3,
    SKMF_ANTITRAIN    = 1<<4,
    SKMF_ENHANCED     = 1<<5,
    SKMF_REDUCED      = 1<<6,
    SKMF_CHANGED      = SKMF_ENHANCED | SKMF_REDUCED,

    SKMF_SKILL_ICONS  = 1<<7,
    SKMF_APTITUDE     = 1<<8,
    SKMF_SIMPLE       = 1<<9, // Simple mode for tutorial and hint mode.
    SKMF_HELP         = 1<<10,
};

#define SKM_HELP -1
enum skill_menu_switch
{
    SKM_MODE  = -2,
    SKM_DO    = -3,
    SKM_SHOW  = -4,
    SKM_LEVEL = -5,
    SKM_VIEW  = -6,
};

// TAG_MAJOR_VERSION. Just sort this enum when bumping major.
enum skill_menu_state
{
    SKM_NONE,
    SKM_MODE_AUTO,
    SKM_MODE_MANUAL,
    SKM_DO_PRACTISE,
    SKM_SHOW_KNOWN,
    SKM_SHOW_ALL,
    SKM_LEVEL_ENHANCED,
    SKM_LEVEL_NORMAL,
    SKM_VIEW_TRAINING,
    SKM_VIEW_PROGRESS,
    SKM_VIEW_TRANSFER,
    SKM_VIEW_POINTS,
    SKM_DO_FOCUS,
};

class SkillMenu;

class SkillMenuEntry
{
public:
    static menu_letter m_letter;
    static SkillMenu* m_skm;

    SkillMenuEntry() {};
    SkillMenuEntry(coord_def coord);
    int get_id();
    TextItem* get_name_item() const;
    skill_type get_skill() const;
    bool is_selectable(bool keep_hotkey = true);
    bool is_set(int flag) const;
    void refresh(bool keep_hotkey);
    void set_display();
    void set_name(bool keep_hotkey);
    void set_skill(skill_type sk = SK_NONE);

private:
    skill_type m_sk;

    TextItem* m_name;
#ifdef USE_TILE
    TextTileItem* m_name_tile;
#endif
    NoSelectTextItem* m_level;
    NoSelectTextItem* m_progress;
    FormattedTextItem* m_aptitude;

    void _clear();
    COLORS get_colour() const;
    std::string get_prefix();
    void set_aptitude();
    void set_level();
    void set_new_level();
    void set_points();
    void set_progress();
    void set_reskill_progress();
    void set_title();
    void set_training();
};

class SkillMenuSwitch : public FormattedTextItem
{
public:
    static SkillMenu* m_skm;

    SkillMenuSwitch(std::string name, int hotkey);
    void add(skill_menu_state state);
    std::string get_help();
    std::string get_name(skill_menu_state state);
    skill_menu_state get_state();
    void set_state(skill_menu_state state);
    int size() const;
    bool toggle();
    void update();

private:
    std::string m_name;
    skill_menu_state m_state;
    std::vector<skill_menu_state> m_states;
};

static const int SK_ARR_LN  = (ndisplayed_skills - 1) / 2;
static const int SK_ARR_COL =  2;

class SkillMenu : public PrecisionMenu
{
public:
    SkillMenu(bool reskilling);

    void clear_flag(int flag);
    bool is_set(int flag) const;
    void set_flag(int flag);
    void toggle_flag(int flag);

    void add_item(TextItem* item, const int size, coord_def &coord);
    void cancel_help();
    void clear_selections();
    bool exit();
    skill_menu_state get_state(skill_menu_switch sw);
    void help();
    void select(skill_type sk, int keyn);
    void toggle(skill_menu_switch sw);

private:
    MenuFreeform*        m_ff;
    BoxMenuHighlighter*  m_highlighter;

    int m_flags;
    coord_def m_min_coord;
    coord_def m_max_coord;
    coord_def m_pos;

    SkillMenuEntry  m_skills[SK_ARR_LN][SK_ARR_COL];

    NoSelectTextItem*  m_title;
    FormattedTextItem* m_help;

    std::map<skill_menu_switch, SkillMenuSwitch*> m_switches;
    FormattedTextItem* m_help_button;

    SkillMenuEntry* find_entry(skill_type sk);
    void init_flags(bool reskilling);
    void init_help();
    void init_title();
    void init_switches();
    void refresh_display();
    void refresh_names();
    void set_default_help();
    void set_help(std::string msg);
    void set_skills();
    void set_title();
    void shift_bottom_down();
    void show_description(skill_type sk);
    void toggle_practise(skill_type sk, int keyn);

    TextItem* find_closest_selectable(int start_ln, int col);
    void set_links();
};
#endif
