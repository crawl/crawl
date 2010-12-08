/*
 *  File:       skill_menu.h
 *  Summary:    Skill menu
 *  Written by: Raphaël Langella
 */

#ifndef SKILL_MENU_H
#define SKILL_MENU_H

#include <queue>
#include "enum.h"
#include "menu.h"

enum skill_menu_flags
{
    SKMF_NONE            = 0,

//Actions
    SKMF_DO_PRACTISE     = 0x001,
    SKMF_DO_SHOW_DESC    = 0x002,
    SKMF_DO_RESKILL_FROM = 0x004,
    SKMF_DO_RESKILL_TO   = 0x008,
    SKMF_ACTION_MASK     = 0x00F,

//Display
    SKMF_DISP_PROGRESS   = 0x010,
    SKMF_DISP_RESKILL    = 0x020,
    SKMF_DISP_NEW_LEVEL  = 0x040,
    SKMF_DISP_POINTS     = 0x080,
    SKMF_DISP_MASK       = 0x0F0,
    SKMF_DISP_ALL        = 0x100,
    SKMF_DISP_APTITUDE   = 0x200,

//Ashenzari transfer knowledge ability set this flag.
    SKMF_RESKILLING      = 0x400,
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
    skill_type get_skill();
    void set_name(bool keep_hotkey);
    void set_display();
    int get_id();
    bool is_set(int flag) const;

private:
    skill_type m_sk;
    TextItem* m_name;
    NoSelectTextItem* m_level;
    NoSelectTextItem* m_progress;
    FormattedTextItem* m_aptitude;

    COLORS _get_colour() const;
    std::string _get_prefix();
    bool _is_selectable() const;
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

menu_letter SkillMenuEntry::m_letter;
SkillMenu* SkillMenuEntry::m_skm;

static const int SK_ARR_LN  = 19;
// static const int SK_ARR_LN  = (ndisplayed_skills +1) / 2;
static const int SK_ARR_COL =  2;

class SkillMenu : public PrecisionMenu
{
public:
    SkillMenu(int flags);
    bool is_set(int flag) const;
    void change_action();
    void change_display();
    void toggle_practise(skill_type sk);
    void show_description(skill_type sk);
    void toggle_show_all();
    void clear_selections();

private:
    MenuFreeform*        m_ff;
    BoxMenuHighlighter*  m_highlighter;

    int m_flags;
    int m_max_col;
    int m_max_ln;

    SkillMenuEntry  m_skills[SK_ARR_LN][SK_ARR_COL];

    NoSelectTextItem*  m_title;
    FormattedTextItem* m_help;
    NoSelectTextItem*  m_current_action;
    TextItem*          m_next_action;
    TextItem*          m_next_display;
    TextItem*          m_show_all;

    std::queue<int> m_disp_queue;

    void _init_disp_queue();
    void _init_title();
    void _init_footer();
    void _refresh_display();
    void _refresh_name(skill_type sk);
    void _refresh_names();
    void _set_title();
    void _set_skills();
    void _set_help(int flag = SKMF_NONE);
    void _set_footer();
    int _get_next_action() const;
    int _get_next_display() const;
};

#endif
