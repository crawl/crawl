/**
 * @file
 * @brief Skill menu.
**/


#ifndef SKILLS_MENU_H
#define SKILLS_MENU_H

#include <queue>
#include "enum.h"
#include "menu.h"
#include "skills2.h"

// Skill Menu

enum skill_menu_flags
{
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
#ifdef DEBUG_DIAGNOSTICS
    void _set_points();
#endif
};

static const int SK_ARR_LN  = (ndisplayed_skills - 1) / 2;
static const int SK_ARR_COL =  2;

class SkillMenu : public PrecisionMenu
{
public:
    SkillMenu(bool reskilling);
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

private:
    MenuFreeform*        m_ff;
    BoxMenuHighlighter*  m_highlighter;

    int m_flags;
    int m_current_help;
    coord_def m_min_coord;
    coord_def m_max_coord;
    coord_def m_pos;

    bool m_crosstrain;
    bool m_antitrain;
    bool m_enhanced;
    bool m_reduced;

    SkillMenuEntry  m_skills[SK_ARR_LN][SK_ARR_COL];

    NoSelectTextItem*  m_title;
    FormattedTextItem* m_help;
    NoSelectTextItem*  m_current_action;
    TextItem*          m_next_action;
    TextItem*          m_next_display;
    TextItem*          m_show_all;

    std::queue<int> m_disp_queue;

    SkillMenuEntry* _find_entry(skill_type sk);
    void _init_flags(bool reskilling);
    void _init_disp_queue();
    void _init_title();
    void _init_footer(coord_def coord);
    void _refresh_display();
    void _refresh_names();
    void _set_title();
    void _set_skills();
    void _set_help(int flag = 0);
    void _set_footer();
    TextItem* _find_closest_selectable(int start_ln, int col);
    void _set_links();
    int _get_next_action() const;
    int _get_next_display() const;
};
#endif
