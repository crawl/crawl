/**
 * @file
 * @brief Skill menu.
**/

#pragma once

#include "enum.h"
#include "menu.h"
#include "skills.h"

// Skill Menu

enum skill_menu_flags
{
    //Ashenzari transfer knowledge ability.
    SKMF_RESKILL_FROM      = 1<<0,
    SKMF_RESKILL_TO        = 1<<1,
    SKMF_RESKILLING        = SKMF_RESKILL_FROM | SKMF_RESKILL_TO,
    SKMF_EXPERIENCE        = 1<<2,
    SKMF_SPECIAL           = SKMF_RESKILLING | SKMF_EXPERIENCE,

    SKMF_MANUAL            = 1<<3,
    SKMF_ENHANCED          = 1<<4,
    SKMF_REDUCED           = 1<<5,
    SKMF_CHANGED           = SKMF_ENHANCED | SKMF_REDUCED,

    SKMF_SKILL_ICONS       = 1<<6,
    SKMF_APTITUDE          = 1<<7,
    SKMF_SIMPLE            = 1<<8, // Simple mode for tutorial and hint mode.
    SKMF_HELP              = 1<<9,
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

class SkillMenu;

#ifdef USE_TILE_LOCAL
class SkillTextTileItem : public TextTileItem
{
public:
    SkillTextTileItem() {};
protected:
    bool handle_mouse(const MouseEvent& me) override;
};
#endif

class SkillMenuEntry
{
public:
    static menu_letter2 m_letter;

    SkillMenuEntry() {};
    SkillMenuEntry(coord_def coord);
    int get_id();
    TextItem* get_name_item() const;
    skill_type get_skill() const;
    bool is_selectable(bool keep_hotkey = true);
    bool is_set(int flag) const;
    bool mastered() const;
    void refresh(bool keep_hotkey);
    void set_display();
    void set_name(bool keep_hotkey);
    void set_skill(skill_type sk = SK_NONE);
    void set_cost();
private:
    skill_type m_sk;

#ifdef USE_TILE_LOCAL
    SkillTextTileItem* m_name;
#else
    TextItem* m_name;
#endif
    NoSelectTextItem* m_level;
    NoSelectTextItem* m_progress;
    FormattedTextItem* m_aptitude;

    void _clear();
    COLOURS get_colour() const;
    string get_prefix();
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
    SkillMenuSwitch(string name, int hotkey);
    void add(skill_menu_state state);
    string get_help();
    string get_name(skill_menu_state state);
    skill_menu_state get_state();
    void set_state(skill_menu_state state);
    int size() const;
    bool toggle();
    void update();

private:
    string m_name;
    skill_menu_state m_state;
    vector<skill_menu_state> m_states;
};

static const int SK_ARR_LN  = (ndisplayed_skills - 1) / 2;
static const int SK_ARR_COL =  2;

class SkillMenu : public PrecisionMenu
{
public:
    SkillMenu();

    void clear_flag(int flag);
    void init(int flag);
    void clear() override;
    bool is_set(int flag) const;
    void set_flag(int flag);
    void toggle_flag(int flag);

    void add_item(TextItem* item, const int size, coord_def &coord);
    void cancel_help();
    bool exit();
#ifdef USE_TILE_LOCAL
    int get_line_height();
#endif
    int get_raw_skill_level(skill_type sk);
    int get_saved_skill_level(skill_type sk, bool real);
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
#ifdef USE_TILE_LOCAL
    int line_height;
#endif

    SkillMenuEntry  m_skills[SK_ARR_LN][SK_ARR_COL];

    NoSelectTextItem*  m_title;
    FormattedTextItem* m_help;

    map<skill_menu_switch, SkillMenuSwitch*> m_switches;
    FormattedTextItem* m_help_button;

    skill_state m_skill_backup;

    SkillMenuEntry* find_entry(skill_type sk);
    void init_flags();
    void init_help();
    void init_title();
    void init_switches();
    void refresh_display();
    void refresh_help_button();
    void refresh_names();
    void set_default_help();
    void set_help(string msg);
    void set_skills();
    void set_title();
    void shift_bottom_down();
    void show_description(skill_type sk);
    void toggle_practise(skill_type sk, int keyn);

    TextItem* find_closest_selectable(int start_ln, int col);
    void set_links();
};
