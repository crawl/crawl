/**
 * @file
 * @brief Skill menu.
**/

// this precompiled header must be the first include otherwise
// includes before it will be ignored without any warning in MSVC
#include "AppHdr.h"

#include <cmath>
#include <clocale>

#include "skill-menu.h"

#include "artefact.h"
#include "art-enum.h"
#include "cio.h"
#include "clua.h"
#include "command.h"
#include "describe.h"
#include "english.h" // apostrophise
#include "evoke.h"
#include "god-passive.h" // passive_t::bondage_skill_boost
#include "hints.h"
#include "options.h"
#include "output.h"
#include "religion.h"
#include "state.h"
#include "stringutil.h"
#include "tilefont.h"
#ifdef USE_TILE
 #include "tilepick.h"
 #include "tilereg-crt.h"
#endif
#include "ui.h"
#include "unwind.h"

using namespace ui;

menu_letter2 SkillMenuEntry::m_letter;
SkillMenu skm;

static string _format_skill_target(int target);

#ifdef USE_TILE_LOCAL
bool SkillTextTileItem::handle_mouse(const wm_mouse_event& me)
{
    if (me.event == wm_mouse_event::PRESS
        && (me.button == wm_mouse_event::LEFT && me.mod & TILES_MOD_SHIFT))
    {
        skill_type sk = skill_type(get_id());
        if (is_invalid_skill(sk))
            return false;

        skm.select(sk, 'A');
        return true;
    }
    else if (me.event == wm_mouse_event::PRESS
        && me.button == wm_mouse_event::RIGHT)
    {
        skill_type sk = skill_type(get_id());
        if (is_invalid_skill(sk))
            return false;
        describe_skill(sk);
        return true;
    }
    else
        return false;
}
#endif

#define NAME_SIZE 20
#define LEVEL_SIZE 5
#define PROGRESS_SIZE 6
#define APTITUDE_SIZE 5
SkillMenuEntry::SkillMenuEntry(coord_def coord)
{
#ifdef USE_TILE_LOCAL
    m_name = new SkillTextTileItem();
#else
    m_name = new TextItem();
#endif

    m_level = new NoSelectTextItem();
    m_progress = new EditableTextItem();
    m_progress->set_editable(false);
    m_aptitude = new FormattedTextItem();
    m_aptitude->allow_highlight(false);

#ifdef USE_TILE_LOCAL
    const int height = skm.get_line_height();
    m_name->set_height(height);
    if (is_set(SKMF_SKILL_ICONS))
    {
        m_level->set_height(height);
        m_progress->set_height(height);
        m_aptitude->set_height(height);
    }
#endif

    skm.add_item(m_name, NAME_SIZE + (is_set(SKMF_SKILL_ICONS) ? 4 : 0), coord);
    m_name->set_highlight_colour(RED);
    skm.add_item(m_level, LEVEL_SIZE, coord);
    skm.add_item(m_progress, PROGRESS_SIZE, coord);
    skm.add_item(m_aptitude, APTITUDE_SIZE, coord);
}

// Public methods
int SkillMenuEntry::get_id()
{
    return m_name->get_id();
}

TextItem* SkillMenuEntry::get_name_item() const
{
    return m_name;
}

skill_type SkillMenuEntry::get_skill() const
{
    return m_sk;
}

static bool _show_skill(skill_type sk, skill_menu_state state)
{
    switch (state)
    {
    case SKM_SHOW_DEFAULT:
        return you.can_currently_train[sk] && (you.should_show_skill[sk]
                                               || you.training[sk])
            || you.skill(sk, 10, false, false);
    case SKM_SHOW_ALL:     return true;
    default:               return false;
    }
}

bool SkillMenuEntry::is_selectable(bool)
{
    if (is_invalid_skill(m_sk))
        return false;

    if (is_set(SKMF_HELP))
        return true;

    if (you.has_mutation(MUT_DISTRIBUTED_TRAINING))
        return false;

    if (mastered())
        return false;

    // if it's visible at this point, it's selectable.
    return true;
}

bool SkillMenuEntry::is_set(int flag) const
{
    return skm.is_set(flag);
}

bool SkillMenuEntry::mastered() const
{
    return (is_set(SKMF_EXPERIENCE) ? skm.get_raw_skill_level(m_sk)
                                    : you.skills[m_sk]) >= MAX_SKILL_LEVEL;
}

void SkillMenuEntry::refresh(bool keep_hotkey)
{
    if (m_sk == SK_TITLE)
        set_title();
    else if (is_invalid_skill(m_sk) || is_useless_skill(m_sk))
        _clear();
    else
    {
        set_name(keep_hotkey);
        set_display();
        if (is_set(SKMF_APTITUDE))
            set_aptitude();
    }
}

void SkillMenuEntry::set_display()
{
    if (m_sk == SK_TITLE)
        set_title();

    if (is_invalid_skill(m_sk))
        return;

    switch (skm.get_state(SKM_VIEW))
    {
    case SKM_VIEW_TRAINING:  set_training();         break;
    case SKM_VIEW_COST:      set_cost();             break;
    case SKM_VIEW_TARGETS:   set_targets();          break;
    case SKM_VIEW_PROGRESS:  set_progress();         break;
    case SKM_VIEW_NEW_LEVEL: set_new_level();        break;
    case SKM_VIEW_POINTS:    set_points();           break;
    default: die("Invalid view state.");
    }
}

void SkillMenuEntry::set_name(bool keep_hotkey)
{
    if (is_invalid_skill(m_sk))
        return;

    if (!keep_hotkey)
        m_name->clear_hotkeys();

    if (is_selectable(keep_hotkey))
    {
        if (!keep_hotkey)
        {
            m_name->add_hotkey(++m_letter);
            m_name->add_hotkey(toupper_safe(m_letter));
        }
        m_name->set_id(m_sk);
        m_name->allow_highlight(true);
    }
    else
    {
        m_name->set_id(-1);
        m_name->allow_highlight(false);
    }

    m_name->set_text(make_stringf("%s %-15s", get_prefix().c_str(),
                                skill_name(m_sk)));
    m_name->set_fg_colour(get_colour());
#ifdef USE_TILE_LOCAL
    if (is_set(SKMF_SKILL_ICONS))
    {
        m_name->clear_tile();
        if (you.skills[m_sk] >= MAX_SKILL_LEVEL)
            m_name->add_tile(tile_def(tileidx_skill(m_sk, TRAINING_MASTERED)));
        else if (you.training[m_sk] == TRAINING_DISABLED)
            m_name->add_tile(tile_def(tileidx_skill(m_sk, TRAINING_INACTIVE)));
        else
            m_name->add_tile(tile_def(tileidx_skill(m_sk, you.train[m_sk])));
    }
#endif
    set_level();
}

void SkillMenuEntry::set_skill(skill_type sk)
{
    m_sk = sk;
    refresh(false);
}

// Private Methods
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
#ifdef USE_TILE_LOCAL
    if (is_set(SKMF_SKILL_ICONS))
        m_name->clear_tile();
#endif
}
COLOURS SkillMenuEntry::get_colour() const
{
    // Skills being actively trained may get further distinction.
    bool use_bright_colour = you.train[m_sk] == TRAINING_ENABLED
        || you.train[m_sk] == TRAINING_FOCUSED;

    if (is_set(SKMF_HELP))
        return LIGHTGRAY;
    else if (skm.get_state(SKM_LEVEL) == SKM_LEVEL_ENHANCED
             && you.skill(m_sk, 10, true) != you.skill(m_sk, 10, false))
    {
        if (you.skill(m_sk, 10, true) < you.skill(m_sk, 10, false))
            return use_bright_colour ? LIGHTGREEN : GREEN;
        else
            return use_bright_colour ? LIGHTMAGENTA : MAGENTA;
    }
    else if (mastered())
        return YELLOW;
    else if (!you.training[m_sk])
        return DARKGREY;
    else if (you.skill_manual_points[m_sk])
        return LIGHTBLUE;
    else if (you.train[m_sk] == TRAINING_FOCUSED)
        return WHITE;
    else
        return LIGHTGREY;
}

string SkillMenuEntry::get_prefix()
{
    int letter;
    const vector<int> hotkeys = m_name->get_hotkeys();

    if (!hotkeys.empty())
        letter = hotkeys[0];
    else
        letter = ' ';

    const int sign = (!you.can_currently_train[m_sk] || mastered()) ? ' ' :
                                   (you.train[m_sk] == TRAINING_FOCUSED) ? '*' :
                                          you.train[m_sk] ? '+'
                                                          : '-';
    return make_stringf(" %c %c", letter, sign);
}

void SkillMenuEntry::set_aptitude()
{
    string text = "<white>";

    const bool manual = you.skill_manual_points[m_sk] > 0;
    const int apt = species_apt(m_sk, you.species);

    if (apt != 0)
        text += make_stringf("%+d", apt);
    else
        text += make_stringf(" %d", apt);

    text += "</white>";
    if (apt < 10 && apt > -10)
        text += " ";

    if (manual)
    {
        skm.set_flag(SKMF_MANUAL);
        text += "<lightred>+4</lightred>";
    }

    m_aptitude->set_text(text);
}

void SkillMenuEntry::set_level()
{
    int level;
    const bool real = skm.get_state(SKM_LEVEL) != SKM_LEVEL_ENHANCED;

    if (is_set(SKMF_EXPERIENCE))
        level = skm.get_saved_skill_level(m_sk, real);
    else
        level = you.skill(m_sk, 10, real);

    if (mastered())
        m_level->set_text(to_string(level / 10));
    else
        m_level->set_text(make_stringf("%4.1f", level / 10.0));
    m_level->set_fg_colour(get_colour());
}

void SkillMenuEntry::set_new_level()
{
    m_progress->set_editable(false);

    const bool real = skm.get_state(SKM_LEVEL) != SKM_LEVEL_ENHANCED;
    int new_level = 0;
    if (is_set(SKMF_EXPERIENCE) && is_selectable())
    {
        new_level = you.skill(m_sk, 10, real);
        m_progress->set_fg_colour(CYAN);
        if (you.training[m_sk])
            m_progress->set_text(make_stringf("> %4.1f", new_level / 10.0));
        else
            m_progress->set_text(string(PROGRESS_SIZE, ' '));
        return;
    }

    if (is_selectable())
        m_progress->set_text(make_stringf("> %4.1f", new_level / 10.0));
    else
        m_progress->set_text("");

}

void SkillMenuEntry::set_points()
{
    m_progress->set_text(make_stringf("%5d", you.skill_points[m_sk]));
    m_progress->set_fg_colour(LIGHTGREY);
    m_progress->set_editable(false);
}

void SkillMenuEntry::set_progress()
{
    if (mastered())
        m_progress->set_text("");
    else
    {
        m_progress->set_text(make_stringf(" %2d%%",
                                          get_skill_percentage(m_sk)));
    }
    m_progress->set_fg_colour(CYAN);
    m_progress->set_editable(false);
}

EditableTextItem *SkillMenuEntry::get_progress()
{
    return m_progress;
}

void SkillMenuEntry::set_targets()
{
    int target = you.get_training_target(m_sk);
    if (target == 0)
    {
        m_progress->set_text("--");
        m_progress->set_fg_colour(DARKGREY);
    }
    else
    {
        m_progress->set_text(_format_skill_target(target));
        if (target_met(m_sk))
            m_progress->set_fg_colour(DARKGREY); // mainly comes up in wizmode
        else
            m_progress->set_fg_colour(get_colour());
    }
    m_progress->set_editable(true, 4);
}

void SkillMenuEntry::set_title()
{
    m_name->allow_highlight(false);
    m_name->set_text("     Skill");
    m_level->set_text("Level");

    m_name->set_fg_colour(BLUE);
    m_level->set_fg_colour(BLUE);
    m_progress->set_fg_colour(BLUE);

    if (is_set(SKMF_APTITUDE))
        m_aptitude->set_text("<blue>Apt </blue>");

    switch (skm.get_state(SKM_VIEW))
    {
    case SKM_VIEW_TRAINING:  m_progress->set_text("Train"); break;
    case SKM_VIEW_TARGETS:   m_progress->set_text("Target"); break;
    case SKM_VIEW_PROGRESS:  m_progress->set_text("Progr"); break;
    case SKM_VIEW_POINTS:    m_progress->set_text("Points");break;
    case SKM_VIEW_COST:      m_progress->set_text("Cost");  break;
    case SKM_VIEW_NEW_LEVEL: m_progress->set_text("> New"); break;
    default: die("Invalid view state.");
    }
}

void SkillMenuEntry::set_training()
{
    m_progress->set_editable(false);

    if (!you.training[m_sk])
        m_progress->set_text("");
    else
        m_progress->set_text(make_stringf("%2d%%", you.training[m_sk]));
    m_progress->set_fg_colour(BROWN);
}

void SkillMenuEntry::set_cost()
{
    m_progress->set_editable(false);

    if (you.skills[m_sk] == MAX_SKILL_LEVEL)
        return;
    if (you.skill_manual_points[m_sk])
        m_progress->set_fg_colour(LIGHTRED);
    else
        m_progress->set_fg_colour(CYAN);

    auto ratio = scaled_skill_cost(m_sk);
    // Don't let the displayed number go greater than 4 characters
    if (ratio > 0)
        m_progress->set_text(make_stringf("%4.*f", ratio < 100 ? 1 : 0, ratio));
}

SkillMenuSwitch::SkillMenuSwitch(string name, int hotkey) : m_name(name)
{
    add_hotkey(hotkey);
    set_highlight_colour(YELLOW);
}

void SkillMenuSwitch::add(skill_menu_state state)
{
    m_states.push_back(state);
    if (m_states.size() == 1)
        m_state = state;
}
skill_menu_state SkillMenuSwitch::get_state()
{
    return m_state;
}

static bool _any_crosstrained()
{
    for (skill_type sk = SK_FIRST_SKILL; sk < NUM_SKILLS; ++sk)
    {
        // Assumes crosstraining is symmetric; otherwise we should
        // iterate over the result of get_crosstrain_skills and
        // check the levels of *those* skills
        if (you.skill_points[sk]
            && !get_crosstrain_skills(sk).empty())
        {
            // Didn't necessarily boost us by a noticeable amount,
            // but close enough.
            return true;
        }
    }
    return false;
}

static bool _charlatan_bonus()
{
    if (you.unrand_equipped(UNRAND_CHARLATANS_ORB)
        && you.skill(SK_EVOCATIONS, 10, true) > 0)
    {
        return true;
    }
    return false;
}

static bool _hermit_bonus()
{
    if (you.unrand_equipped(UNRAND_HERMITS_PENDANT)
        && you.skill(SK_INVOCATIONS, 10,  true) < 140)
    {
        return true;
    }
    return false;
}

static bool _hermit_penalty()
{
    if (you.unrand_equipped(UNRAND_HERMITS_PENDANT))
    {
        if (you.skill(SK_EVOCATIONS, 10, true) > 0
            || you.skill(SK_INVOCATIONS, 10, true) > 140)
        return true;
    }
    return false;
}

string SkillMenuSwitch::get_help()
{
    switch (m_state)
    {
    case SKM_MODE_AUTO:
        return "In automatic mode, skills are trained as you use them.";
    case SKM_MODE_MANUAL:
        return "In manual mode, experience is spread evenly across all "
                "activated skills.";
    case SKM_DO_PRACTISE:
        if (skm.is_set(SKMF_SIMPLE))
            return hints_skills_info();
        else
            return "Press the letter of a skill to choose whether you want to "
                   "practise it. Skills marked with '<darkgrey>-</darkgrey>' "
                   "will not be trained.";
    case SKM_DO_FOCUS:
        return "Press the letter of a skill to cycle between "
               "<darkgrey>disabled</darkgrey> (<darkgrey>-</darkgrey>), "
               "enabled (+) and <white>focused</white> (<white>*</white>). "
               "Focused skills train twice as fast relative to others.";
    case SKM_LEVEL_ENHANCED:
    {
        string result;
        if (skm.is_set(SKMF_ENHANCED))
        {
            vector<string> causes;
            if (you.duration[DUR_HEROISM])
                causes.push_back("Heroism");

            if (!you.skill_boost.empty()
                && have_passive(passive_t::bondage_skill_boost))
            {
                causes.push_back(apostrophise(god_name(you.religion))
                                 + " power");
            }
            if (_any_crosstrained())
                causes.push_back("cross-training");
            if (_hermit_bonus())
                causes.push_back("the Hermit's pendant");
            if (_charlatan_bonus())
                causes.push_back("the Charlatan's Orb");
            result = "Skills enhanced by "
                     + comma_separated_line(causes.begin(), causes.end())
                     + " are in <green>green</green>.";
        }

        if (skm.is_set(SKMF_REDUCED))
        {
            vector<const char *> causes;
            if (player_under_penance(GOD_ASHENZARI))
                causes.push_back("Ashenzari's anger");
            if (_hermit_penalty())
                causes.push_back("the Hermit's pendant");
            if (!result.empty())
                result += "\n";
            result += "Skills reduced by "
                      + comma_separated_line(causes.begin(), causes.end())
                      + " are in <magenta>magenta</magenta>.";
        }

        if (!result.empty())
            return result;
    }
    case SKM_VIEW_TRAINING:
        if (skm.is_set(SKMF_SIMPLE))
            return hints_skill_training_info();
        else
            return "The percentage of incoming experience used"
                   " to train each skill is in <brown>brown</brown>.\n";
    case SKM_VIEW_TARGETS:
        if (skm.is_set(SKMF_SIMPLE))
            return hints_skill_targets_info();
        else
            return "The current training targets, if any.\n";
    case SKM_VIEW_PROGRESS:
        return "The percentage of the progress done before reaching next "
               "level is in <cyan>cyan</cyan>.\n";
    case SKM_VIEW_COST:
    {
        if (skm.is_set(SKMF_SIMPLE))
            return hints_skill_costs_info();

        string result =
               "The relative cost of raising each skill is in "
               "<cyan>cyan</cyan>";
        if (skm.is_set(SKMF_MANUAL))
        {
            result += " (or <lightred>red</lightred> if enhanced by a "
                      "manual)";
        }
        result += ".\n";
        return result;
    }
    default: return "";
    }
}

string SkillMenuSwitch::get_name(skill_menu_state state)
{
    switch (state)
    {
    case SKM_MODE_AUTO:      return "auto";
    case SKM_MODE_MANUAL:    return "manual";
    case SKM_DO_PRACTISE:    return "train";
    case SKM_DO_FOCUS:       return "focus";
    case SKM_SHOW_DEFAULT:   return "useful";
    case SKM_SHOW_ALL:       return "all";
    case SKM_LEVEL_ENHANCED:
        return (skm.is_set(SKMF_ENHANCED)
                && skm.is_set(SKMF_REDUCED)) ? "modified" :
                   skm.is_set(SKMF_ENHANCED) ? "enhanced"
                                             : "reduced";
    case SKM_LEVEL_NORMAL:   return "base";
    case SKM_VIEW_TRAINING:  return "training";
    case SKM_VIEW_TARGETS:   return "targets";
    case SKM_VIEW_PROGRESS:  return "progress";
    case SKM_VIEW_POINTS:    return "points";
    case SKM_VIEW_NEW_LEVEL: return "new level";
    case SKM_VIEW_COST:      return "cost";
    default: die ("Invalid switch state.");
    }
}

void SkillMenuSwitch::set_state(skill_menu_state state)
{
    // We only set it if it's a valid state.
    for (auto candidate : m_states)
    {
        if (candidate == state)
        {
            m_state = state;
            return;
        }
    }
}

int SkillMenuSwitch::size() const
{
    if (m_states.size() <= 1)
        return 0;
    else
        return formatted_string::parse_string(m_text).width();
}

bool SkillMenuSwitch::toggle()
{
    if (m_states.size() <= 1)
        return false;

    auto it = find(begin(m_states), end(m_states), m_state);
    ASSERT(it != m_states.end());
    ++it;
    if (it == m_states.end())
        it = m_states.begin();

    m_state = *it;
    update();
    return true;
}

void SkillMenuSwitch::update()
{
    if (m_states.size() <= 1)
    {
        set_text("");
        return;
    }

    const vector<int> hotkeys = get_hotkeys();
    ASSERT(hotkeys.size());
    string text = make_stringf(" [<yellow>%c</yellow>] ", hotkeys[0]);
    for (auto it = m_states.begin(); it != m_states.end(); ++it)
    {
        if (it != m_states.begin())
            text += '|';

        const string col = (*it == m_state) ? "white" : "darkgrey";
        text += make_stringf("<%s>%s</%s>", col.c_str(), get_name(*it).c_str(),
                             col.c_str());
    }
    if (m_name.empty())
        text += "  ";
    else
        text += make_stringf(" %s  ", m_name.c_str());
    set_text(text);
}

#define TILES_COL 6
SkillMenu::SkillMenu() : PrecisionMenu(), m_min_coord(), m_max_coord(),
                         m_help_button(nullptr)
{
}

void SkillMenu::init_experience()
{
    if (is_set(SKMF_EXPERIENCE) && !m_skill_backup.state_saved())
    {
        m_skill_backup.save();
        if (you.auto_training)
            for (int i = 0; i < NUM_SKILLS; ++i)
            {
                // only enable currently autotraining skills, not all skills
                const skill_type sk = skill_type(i);
                if (!you.training[sk])
                    you.train[sk] = TRAINING_DISABLED;
            }
        you.auto_training = false;
        reset_training();
        you.clear_training_targets();

        for (int i = 0; i < NUM_SKILLS; ++i)
        {
            const skill_type sk = skill_type(i);
            if (!is_useless_skill(sk) && !you.can_currently_train[sk])
            {
                you.can_currently_train.set(sk);
                you.train[sk] = TRAINING_DISABLED;
            }
        }
    }
}

/**
 * resolve the player's experience state at the end of the skill menu, dealing
 * with any experience applied to this skill menu instance (e.g. by a potion
 * of experience).
 *
 * @param experience_change whether to actually do the experience change, or
 *                          just do cleanup.
 */
void SkillMenu::finish_experience(bool experience_change)
{
    if (is_set(SKMF_EXPERIENCE) && m_skill_backup.state_saved())
    {
        if (experience_change)
        {
            redraw_screen();
            update_screen();
            unwind_bool change_xp_for_real(crawl_state.simulating_xp_gain, false);
            train_skills();
        }
        m_skill_backup.restore_training();
        m_skill_backup = skill_state();
    }
}

void SkillMenu::init(int flag, int region_height)
{
    m_flags = flag;
    init_flags();

    init_experience();

#ifdef USE_TILE_LOCAL
    const int char_height = tiles.get_crt_font()->char_height();
    if (Options.tile_menu_icons)
    {
        line_height = min((region_height - char_height * 6) / SK_ARR_LN,
                          Options.tile_cell_pixels);
        set_flag(SKMF_SKILL_ICONS);
    }
    else
        line_height = char_height;
#endif

    m_min_coord.x = 1;
    m_min_coord.y = 1;
    m_pos = m_min_coord;
    m_ff = new MenuFreeform();

    m_max_coord.x = MIN_COLS + 1;

#ifdef USE_TILE_LOCAL
    m_max_coord.y = region_height / char_height;
    if (is_set(SKMF_SKILL_ICONS))
    {
        m_ff->set_height(line_height);
        m_max_coord.x += 2 * TILES_COL;
    }
#else
    m_max_coord.y = region_height + 1;
#endif

    m_ff->init(m_min_coord, m_max_coord, "freeform");
    attach_object(m_ff);
    set_active_object(m_ff);

    if (is_set(SKMF_SPECIAL))
        init_title();

    m_pos.x++;
    const int col_split = MIN_COLS / 2
                          + (is_set(SKMF_SKILL_ICONS) ? TILES_COL : 0);
    for (int col = 0; col < SK_ARR_COL; ++col)
        for (int ln = 0; ln < SK_ARR_LN; ++ln)
        {
            m_skills[ln][col] = SkillMenuEntry(coord_def(m_pos.x
                                                         + col_split * col,
                                                         m_pos.y + ln));
        }

    m_pos.y += SK_ARR_LN;
#ifdef USE_TILE_LOCAL
    if (is_set(SKMF_SKILL_ICONS))
        m_pos.y = tiles.to_lines(m_pos.y, line_height);
    else
        ++m_pos.y;
#endif

    init_button_row();
    --m_pos.x;
    init_switches();

    if (m_pos.y < m_max_coord.y - 1)
        shift_bottom_down();

    if (is_set(SKMF_SPECIAL))
        set_title();
    set_skills();
    set_default_help();

    if (is_set(SKMF_EXPERIENCE))
        refresh_display();

    m_highlighter = new BoxMenuHighlighter(this);
    m_highlighter->init(coord_def(-1,-1), coord_def(-1,-1), "highlighter");
    attach_object(m_highlighter);

    m_ff->set_visible(true);
    m_highlighter->set_visible(true);
    refresh_button_row();
    do_skill_enabled_check();
}

static string _format_skill_target(int target)
{
    // Use locale-sensitive decimal marker for consistency with other parts of
    // this menu.
    std::lconv *locale_data = std::localeconv();
    return make_stringf("%d%s%d", target / 10,
                            locale_data->decimal_point, target % 10);
}

static keyfun_action _keyfun_target_input(int &ch)
{
    if (ch == '-')
        return KEYFUN_BREAK; // reset to 0
    // Use the locale decimal point, because atof (used in read_skill_target)
    // is locale-sensitive, as is the output of printf code.
    // `decimal_point` on the result is guaranteed to be non-empty by the
    // standard.
    std::lconv *locale_data = std::localeconv();

    if (ch == CONTROL('K') || ch == CONTROL('D') || ch == CONTROL('W') ||
            ch == CONTROL('U') || ch == CONTROL('A') || ch == CONTROL('E') ||
            ch == CK_ENTER || ch == CK_BKSP || ch == CK_ESCAPE ||
            ch < 0 || // this should get all other special keys
            ch == *locale_data->decimal_point || isadigit(ch))
    {
        return KEYFUN_PROCESS;
    }
    return KEYFUN_IGNORE;
}

int SkillMenu::read_skill_target(skill_type sk)
{
    SkillMenuEntry *entry = find_entry(sk);
    ASSERT(entry);
    EditableTextItem *progress = entry->get_progress();
    ASSERT(progress);

    const int old_target = you.get_training_target(sk);
    const string prefill = old_target <= 0 ? "0"
                                           : _format_skill_target(old_target);

    progress->set_editable(true, 5);
    progress->set_highlight_colour(RED);

    // for webtiles dialog input
    progress->set_prompt(make_stringf("Enter a skill target for %s: ",
                                            skill_name(sk)));
    progress->set_tag("skill_target");

    edit_result r = progress->edit(&prefill, _keyfun_target_input);

    const char *result_buf = r.text.c_str();

    int input;
    if (r.reader_result == '-')
        input = 0;
    else if (r.reader_result || result_buf[0] == '\0')
    {
        // editing canceled
        cancel_set_target();
        return -1;
    }
    else
    {
        input = round(atof(result_buf) * 10.0);    // TODO: parse fixed point?
        if (input > 270)
        {
            // 27.0 is the maximum target
            set_help("<lightred>Your training target must be 27 or below!</lightred>");
            return -1;
        }
        else
            set_help("");
    }
    you.set_training_target(sk, input);
    cancel_set_target();
    refresh_display();
    return input;
}

void SkillMenu::clear()
{
    PrecisionMenu::clear(); // deletes m_ff, which deletes its entries
    m_switches.clear();
    m_ff = nullptr;
    m_help_button = nullptr;
    m_middle_button = nullptr;
    m_clear_targets_button = nullptr;
    m_skill_backup = skill_state();
    m_flags = 0;
}

//Public methods
void SkillMenu::clear_flag(int flag)
{
    m_flags &= ~flag;
}

bool SkillMenu::is_set(int flag) const
{
    return m_flags & flag;
}

void SkillMenu::set_flag(int flag)
{
    m_flags |= flag;
}

void SkillMenu::toggle_flag(int flag)
{
    m_flags ^= flag;
}

void SkillMenu::add_item(TextItem* item, const int size, coord_def &coord)
{
    if (coord.x + size > m_max_coord.x)
    {
        coord.x = 1;
        ++coord.y;
    }
    item->set_bounds(coord, coord_def(coord.x + size, coord.y + 1));
    item->set_visible(true);
    m_ff->attach_item(item);
    if (size)
        coord.x += size + 1;
}

void SkillMenu::cancel_help()
{
    clear_flag(SKMF_HELP);
    refresh_names();
    set_default_help();
}

/**
 * Does the player have at least one skill enabled?
 * Side effect: will set an error message in the help line if not.
 *
 * @return true if the check passes: either a skill is enabled or no skills
 *         can be enabled.
 */
bool SkillMenu::do_skill_enabled_check()
{
    if (skills_being_trained())
        return true;

    if (trainable_skills())
    {
        // Shouldn't happen, but crash rather than locking the player in the
        // menu. Training will be fixed up on load.
        ASSERT(!you.has_mutation(MUT_DISTRIBUTED_TRAINING));
        set_help("<lightred>You need to enable at least one skill.</lightred>");
        // It can be confusing if the only trainable skills are hidden. Turn on
        // SKM_SHOW_ALL if so.
        if (get_state(SKM_SHOW) == SKM_SHOW_DEFAULT)
        {
            bool showing_trainable = false;
            for (skill_type sk = SK_FIRST_SKILL; sk < NUM_SKILLS; ++sk)
                if (_show_skill(sk, SKM_SHOW_DEFAULT) && can_enable_skill(sk))
                {
                    showing_trainable = true;
                    break;
                }
            if (!showing_trainable)
                toggle(SKM_SHOW);
        }
        return false;
    }
    return true;
}

bool SkillMenu::exit(bool just_reset)
{
    if (just_reset)
    {
        finish_experience(false);
        clear();
        return true;
    }
    if (crawl_state.seen_hups)
    {
        clear();
        return true;
    }

    // Before we exit, make sure there's at least one skill enabled.
    if (!do_skill_enabled_check())
        return false;

    finish_experience(true);

    clear();
    return true;
}

#ifdef USE_TILE_LOCAL
int SkillMenu::get_line_height()
{
    return line_height;
}
#endif

int SkillMenu::get_raw_skill_level(skill_type sk)
{
    return m_skill_backup.skills[sk];
}

int SkillMenu::get_saved_skill_level(skill_type sk, bool real)
{
    if (real)
        return m_skill_backup.real_skills[sk];
    else
        return m_skill_backup.changed_skills[sk];
}

skill_menu_state SkillMenu::get_state(skill_menu_switch sw)
{
    if (is_set(SKMF_EXPERIENCE))
    {
        switch (sw)
        {
        case SKM_MODE:  return SKM_MODE_MANUAL;
        case SKM_DO:    return SKM_DO_FOCUS;
        case SKM_VIEW:  return SKM_VIEW_NEW_LEVEL;
        default:        return !m_switches[sw] ? SKM_NONE
                                               : m_switches[sw]->get_state();
        }
    }
    else if (!m_switches[sw])
    {
        if (you.has_mutation(MUT_DISTRIBUTED_TRAINING))
        {
            switch (sw)
            {
            case SKM_MODE:  return SKM_MODE_MANUAL;
            case SKM_DO:    return SKM_DO_FOCUS;
            case SKM_SHOW:  return SKM_SHOW_ALL;
            default:        return SKM_NONE;
            }
        }
        else
            return SKM_NONE;
    }
    else
        return m_switches[sw]->get_state();
}

void SkillMenu::set_target_mode()
{
    if (get_state(SKM_VIEW) != SKM_VIEW_TARGETS && m_switches[SKM_VIEW])
    {
        m_switches[SKM_VIEW]->set_state(SKM_VIEW_TARGETS);
        m_switches[SKM_VIEW]->update();
        refresh_display();
    }
    set_flag(SKMF_SET_TARGET);
    refresh_names();
}

void SkillMenu::cancel_set_target()
{
    clear_flag(SKMF_SET_TARGET);
    refresh_names();
}

void SkillMenu::clear_targets()
{
    if (get_state(SKM_VIEW) != SKM_VIEW_TARGETS)
        return;
    you.clear_training_targets();
    refresh_display();
}

void SkillMenu::help()
{
    if (!is_set(SKMF_HELP))
    {
        string text;
        if (is_set(SKMF_SIMPLE))
            text = hints_skills_description_info();
        else
            text = "Press the letter of a skill to read its description.\n"
                   "Press <w>?</w> for a general explanation"
                   " of skilling and the various toggles.";
        set_help(text);
    }
    else
    {
        if (!is_set(SKMF_SIMPLE))
            show_skill_menu_help();
        set_default_help();
    }
    toggle_flag(SKMF_HELP);
    refresh_names();
}

void SkillMenu::select(skill_type sk, int keyn)
{
    if (is_set(SKMF_HELP))
        show_description(sk);
    else if (skm.get_state(SKM_VIEW) == SKM_VIEW_TARGETS
                                            && skm.is_set(SKMF_SET_TARGET))
    {
        read_skill_target(sk);
    }
    else if (get_state(SKM_DO) == SKM_DO_PRACTISE
             || get_state(SKM_DO) == SKM_DO_FOCUS)
    {
        toggle_practise(sk, keyn);
        if (get_state(SKM_VIEW) == SKM_VIEW_TRAINING || is_set(SKMF_EXPERIENCE)
            || keyn >= 'A' && keyn <= 'Z')
        {
            refresh_display();
        }
    }
}

void SkillMenu::toggle(skill_menu_switch sw)
{
    if (!m_switches[sw]->toggle())
        return;

    // XXX: should use a pointer instead.
    FixedVector<training_status, NUM_SKILLS> tmp;

    switch (sw)
    {
    case SKM_MODE:
        you.auto_training = !you.auto_training;
        // TODO: these are probably now redundant
        Options.default_manual_training = !you.auto_training;
        Options.prefs_dirty = true;

        // Switch the skill train state with the saved version.
        tmp = you.train;
        you.train = you.train_alt;
        you.train_alt = tmp;

        reset_training();
        break;
    case SKM_DO:
        you.skill_menu_do = get_state(SKM_DO);
        refresh_names();
        if (m_ff->get_active_item() != nullptr
            && !m_ff->get_active_item()->can_be_highlighted())
        {
            m_ff->activate_default_item();
        }
        break;
    case SKM_SHOW:
        set_skills();
        break;
    case SKM_VIEW:
        you.skill_menu_view = get_state(SKM_VIEW);
        break;
    case SKM_LEVEL: break;
    }
    refresh_display();
    set_help(m_switches[sw]->get_help());
}

// Private methods
SkillMenuEntry* SkillMenu::find_entry(skill_type sk)
{
    for (int col = 0; col < SK_ARR_COL; ++col)
        for (int ln = 0; ln < SK_ARR_LN; ++ln)
            if (m_skills[ln][col].get_skill() == sk)
                return &m_skills[ln][col];

    return nullptr;
}

void SkillMenu::init_flags()
{
    if (crawl_state.game_is_hints_tutorial())
        set_flag(SKMF_SIMPLE);
    else
        set_flag(SKMF_APTITUDE);

    for (unsigned int i = 0; i < NUM_SKILLS; ++i)
    {
        skill_type type = skill_type(i);
        if (you.skill(type, 10) > you.skill(type, 10, true))
            set_flag(SKMF_ENHANCED);
        else if (you.skill(type, 10) < you.skill(type, 10, true))
            set_flag(SKMF_REDUCED);
    }
}

void SkillMenu::init_title()
{
    m_title = new NoSelectTextItem();
    m_title->set_bounds(m_pos,
                        coord_def(m_max_coord.x, m_pos.y + 1));
    ++m_pos.y;
    m_title->set_fg_colour(WHITE);
    m_ff->attach_item(m_title);
    m_title->set_visible(true);
}

void SkillMenu::init_button_row()
{
    m_help = new FormattedTextItem();
    int help_height = 3;
    if (is_set(SKMF_SIMPLE))
    {
        // We just assume that the player won't learn too many skills while
        // in tutorial/hint mode.
        m_pos.y -= 1;
        help_height += 2;
    }

    m_help->set_bounds(m_pos, coord_def(m_max_coord.x, m_pos.y + help_height));
    m_pos.y += help_height;
    m_ff->attach_item(m_help);
    m_help->set_fg_colour(LIGHTGREY);
    m_help->set_visible(true);

    if (!is_set(SKMF_SPECIAL))
    {
        // left button always display help.
        m_help_button = new FormattedTextItem();
        m_help_button->set_id(SKM_HELP);
        m_help_button->add_hotkey('?');
        m_help_button->set_highlight_colour(YELLOW);
        add_item(m_help_button, 23, m_pos);

        // middle button is used to display [a-z] legends, and also to set a
        // skill target in the proper mode.
        m_middle_button = new FormattedTextItem();
        m_middle_button->add_hotkey('=');
        m_middle_button->set_id(SKM_SET_TARGET);
        m_middle_button->set_highlight_colour(YELLOW);
        add_item(m_middle_button, 24, m_pos);

        // right button is either blank or shows target clearing options.
        m_clear_targets_button = new FormattedTextItem();
        m_clear_targets_button->set_id(SKM_CLEAR_TARGETS);
        m_clear_targets_button->add_hotkey('-');
        m_clear_targets_button->set_highlight_colour(YELLOW);
        add_item(m_clear_targets_button, 25, m_pos);
        refresh_button_row();
    }
}

void SkillMenu::init_switches()
{
    SkillMenuSwitch* sw;
    if (!you.has_mutation(MUT_DISTRIBUTED_TRAINING))
    {
        sw = new SkillMenuSwitch("mode", '/');
        m_switches[SKM_MODE] = sw;
        sw->add(SKM_MODE_AUTO);
        if (!is_set(SKMF_SPECIAL) && !is_set(SKMF_SIMPLE))
            sw->add(SKM_MODE_MANUAL);
        if (!you.auto_training)
            sw->set_state(SKM_MODE_MANUAL);
        sw->update();
        sw->set_id(SKM_MODE);
        add_item(sw, sw->size(), m_pos);

        sw = new SkillMenuSwitch("skill", '|');
        m_switches[SKM_DO] = sw;
        if (!is_set(SKMF_EXPERIENCE)
            && (is_set(SKMF_SIMPLE) || Options.skill_focus != SKM_FOCUS_ON))
        {
            sw->add(SKM_DO_PRACTISE);
        }
        if (!is_set(SKMF_SIMPLE) && Options.skill_focus != SKM_FOCUS_OFF)
            sw->add(SKM_DO_FOCUS);

        sw->set_state(you.skill_menu_do);
        sw->add_hotkey('\t');
        sw->update();
        sw->set_id(SKM_DO);
        add_item(sw, sw->size(), m_pos);

        sw = new SkillMenuSwitch("skills", '*');
        m_switches[SKM_SHOW] = sw;
        sw->add(SKM_SHOW_DEFAULT);
        if (!is_set(SKMF_SIMPLE))
        {
            sw->add(SKM_SHOW_ALL);
            if (Options.default_show_all_skills)
                sw->set_state(SKM_SHOW_ALL);
        }
        sw->update();
        sw->set_id(SKM_SHOW);
        add_item(sw, sw->size(), m_pos);
    }

    if (is_set(SKMF_CHANGED))
    {
        sw = new SkillMenuSwitch("level", '_');
        m_switches[SKM_LEVEL] = sw;
        sw->add(SKM_LEVEL_ENHANCED);
        sw->add(SKM_LEVEL_NORMAL);
        sw->update();
        sw->set_id(SKM_LEVEL);
        add_item(sw, sw->size(), m_pos);
    }

    sw = new SkillMenuSwitch("", '!');
    m_switches[SKM_VIEW] = sw;
    if (!is_set(SKMF_SPECIAL) || you.wizard)
    {
        sw->add(SKM_VIEW_TRAINING);

        sw->add(SKM_VIEW_COST);

        if (!you.auto_training)
            sw->set_state(SKM_VIEW_COST);

        if (!you.has_mutation(MUT_DISTRIBUTED_TRAINING))
            sw->add(SKM_VIEW_TARGETS);
    }

    if (you.wizard)
    {
        sw->add(SKM_VIEW_PROGRESS);
        sw->add(SKM_VIEW_POINTS);
    }

    if (is_set(SKMF_SPECIAL))
    {
        sw->add(SKM_VIEW_NEW_LEVEL);
        sw->set_state(SKM_VIEW_NEW_LEVEL);
    }
    else
        sw->set_state(you.skill_menu_view);

    sw->update();
    sw->set_id(SKM_VIEW);
    add_item(sw, sw->size(), m_pos);
}

void SkillMenu::refresh_display()
{
    if (is_set(SKMF_EXPERIENCE))
        train_skills(true);

    for (int ln = 0; ln < SK_ARR_LN; ++ln)
        for (int col = 0; col < SK_ARR_COL; ++col)
            m_skills[ln][col].refresh(true);

    if (is_set(SKMF_EXPERIENCE))
        m_skill_backup.restore_levels();
    refresh_button_row();
}

void SkillMenu::refresh_button_row()
{
    if (is_set(SKMF_SPECIAL))
        return;
    const string helpstring = "[<yellow>?</yellow>] ";
    const string azstring = "[<yellow>a</yellow>-<yellow>z</yellow>] ";

    string legend = is_set(SKMF_SIMPLE) ? "Skill descriptions" : "Help";
    string midlegend = "";
    string clearlegend = "";
    if (is_set(SKMF_HELP))
    {
        legend = is_set(SKMF_SIMPLE) ? "Return to skill selection"
                 : "<w>Help</w>";
        midlegend = azstring + "skill descriptions";
    }
    else if (!is_set(SKMF_SIMPLE) && get_state(SKM_VIEW) == SKM_VIEW_TARGETS)
    {
        if (is_set(SKMF_SET_TARGET))
        {
            midlegend = azstring + "set skill target";
            clearlegend = "[<yellow>-</yellow>] clear selected target";
        }
        else
        {
            midlegend = "[<yellow>=</yellow>] set a skill target";
            clearlegend = "[<yellow>-</yellow>] clear all targets";
        }
    }
    else if (!you.has_mutation(MUT_DISTRIBUTED_TRAINING)) // SKM_VIEW_TARGETS unavailable for Gn
        midlegend = "[<yellow>=</yellow>] set a skill target";

    m_help_button->set_text(helpstring + legend);
    m_middle_button->set_text(midlegend);
    m_clear_targets_button->set_text(clearlegend + "\n");
}

void SkillMenu::refresh_names()
{
    SkillMenuEntry::m_letter = '9';
    bool default_set = false;
    for (int col = 0; col < SK_ARR_COL; ++col)
        for (int ln = 0; ln < SK_ARR_LN; ++ln)
        {
            SkillMenuEntry& skme = m_skills[ln][col];
            if (!default_set && skme.is_selectable())
            {
                m_ff->set_default_item(skme.get_name_item());
                default_set = true;
            }
            skme.set_name(false);
        }
    set_links();
    if (m_ff->get_active_item() != nullptr
        && !m_ff->get_active_item()->can_be_highlighted())
    {
        m_ff->activate_default_item();
    }
    refresh_button_row();
}

void SkillMenu::set_default_help()
{
    string text;
    if (is_set(SKMF_EXPERIENCE))
    {
        text = "Select the skills you want to be trained. "
               "The chosen skills will be raised to the level shown in "
               "<cyan>cyan</cyan>.";
    }
    else if (is_set(SKMF_SIMPLE))
        text = hints_skills_info();
    else
    {
        if (!is_set(SKMF_MANUAL))
            text = m_switches[SKM_VIEW]->get_help();

        if (get_state(SKM_LEVEL) == SKM_LEVEL_ENHANCED)
            text += m_switches[SKM_LEVEL]->get_help() + " ";
        else
            text += "The species aptitude is in <white>white</white>. ";

        if (is_set(SKMF_MANUAL))
            text += "Bonus from skill manuals is in <lightred>red</lightred>. ";
    }

    m_help->set_text(text);
}

void SkillMenu::set_help(string msg)
{
    if (msg.empty())
        set_default_help();
    else
        m_help->set_text(msg);
}

void SkillMenu::set_skills()
{
    int previous_active;
    if (m_ff->get_active_item() != nullptr)
        previous_active = m_ff->get_active_item()->get_id();
    else
        previous_active = -1;

    SkillMenuEntry::m_letter = '9';
    bool default_set = false;
    clear_flag(SKMF_MANUAL);

    int col = 0, ln = 0;

    for (int i = 0; i < ndisplayed_skills; ++i)
    {
        skill_type sk = skill_display_order[i];
        SkillMenuEntry& skme = m_skills[ln][col];

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
        else if (!is_invalid_skill(sk) && !_show_skill(sk, get_state(SKM_SHOW)))
            continue;
        else
        {
            skme.set_skill(sk);
            if (!default_set && skme.is_selectable())
            {
                m_ff->set_default_item(skme.get_name_item());
                default_set = true;
            }
        }
        ++ln;
    }
    if (previous_active != -1)
        m_ff->set_active_item(previous_active);

    set_links();
}

void SkillMenu::toggle_practise(skill_type sk, int keyn)
{
    ASSERT(you.can_currently_train[sk]);
    if (keyn >= 'A' && keyn <= 'Z')
        you.train.init(TRAINING_DISABLED);
    if (get_state(SKM_DO) == SKM_DO_PRACTISE)
        set_training_status(sk, you.train[sk] ? TRAINING_DISABLED : TRAINING_ENABLED);
    else if (get_state(SKM_DO) == SKM_DO_FOCUS)
        set_training_status(sk, (training_status)((you.train[sk] + 1) % NUM_TRAINING_STATUSES));
    else
        die("Invalid state.");
    reset_training();
    if (is_magic_skill(sk) && you.has_mutation(MUT_INNATE_CASTER))
    {
        // This toggles every single magic skill, so let's just regenerate the display.
        refresh_display();
        return;
    }

    // Otherwise, only toggle the affected skill button.
    SkillMenuEntry* skme = find_entry(sk);
    skme->set_name(true);
    const vector<int> hotkeys = skme->get_name_item()->get_hotkeys();

    if (!hotkeys.empty())
    {
        int letter;
        letter = hotkeys[0];
        ASSERT(letter != '9'); // if you get here, make the skill menu smaller
        if (letter == 'z')
            letter = '0';
        else
            ++letter;

        MenuItem* next_item = m_ff->find_item_by_hotkey(letter);
        if (next_item != nullptr)
        {
            if (m_ff->get_active_item() != nullptr && keyn == CK_ENTER)
                m_ff->set_active_item(next_item);
            else
                m_ff->set_default_item(next_item);
        }
    }
}

void SkillMenu::set_title()
{
    string t;
    if (is_set(SKMF_EXPERIENCE))
    {
        t = "You have gained great experience. "
            "Select the skills to train.";
    }

    m_title->set_text(t);
}

void SkillMenu::shift_bottom_down()
{
    const coord_def down(0, 1);
    m_help->move(down);
    for (auto &entry : m_switches)
        entry.second->move(down);

    if (m_help_button)
    {
        m_help_button->move(down);
        m_middle_button->move(down);
        m_clear_targets_button->move(down);
    }
}

void SkillMenu::show_description(skill_type sk)
{
    describe_skill(sk);
}

TextItem* SkillMenu::find_closest_selectable(int start_ln, int col)
{
    int delta = 0;
    while (1)
    {
        int ln_up = max(0, start_ln - delta);
        int ln_down = min(SK_ARR_LN, start_ln + delta);
        if (m_skills[ln_up][col].is_selectable())
            return m_skills[ln_up][col].get_name_item();
        else if (m_skills[ln_down][col].is_selectable())
            return m_skills[ln_down][col].get_name_item();
        else if (ln_up == 0 && ln_down == SK_ARR_LN)
            return nullptr;

        ++delta;
    }
}

void SkillMenu::set_links()
{
    TextItem* top_left = nullptr;
    TextItem* top_right = nullptr;
    TextItem* bottom_left = nullptr;
    TextItem* bottom_right = nullptr;

    for (int ln = 0; ln < SK_ARR_LN; ++ln)
    {
        if (m_skills[ln][0].is_selectable())
        {
            TextItem* left = m_skills[ln][0].get_name_item();
            left->set_link_up(nullptr);
            left->set_link_down(nullptr);
            left->set_link_right(find_closest_selectable(ln, 1));
            if (top_left == nullptr)
                top_left = left;
            bottom_left = left;
        }
        if (m_skills[ln][1].is_selectable())
        {
            TextItem* right = m_skills[ln][1].get_name_item();
            right->set_link_up(nullptr);
            right->set_link_down(nullptr);
            right->set_link_left(find_closest_selectable(ln, 0));
            if (top_right == nullptr)
                top_right = right;
            bottom_right = right;
        }
    }
    if (top_left != nullptr)
    {
        top_left->set_link_up(bottom_right);
        bottom_left->set_link_down(top_right);
    }
    if (top_right != nullptr)
    {
        top_right->set_link_up(bottom_left);
        bottom_right->set_link_down(top_left);
    }
}

class UISkillMenu : public Widget
{
public:
    UISkillMenu(int _flag) : flag(_flag) {};
    ~UISkillMenu() {};

    virtual void _render() override;
    virtual SizeReq _get_preferred_size(Direction dim, int prosp_width) override;
    virtual void _allocate_region() override;
#ifdef USE_TILE_LOCAL
    virtual bool on_event(const Event& ev) override;
#endif

protected:
    int flag;
};

SizeReq UISkillMenu::_get_preferred_size(Direction dim, int /*prosp_width*/)
{
#ifdef USE_TILE_LOCAL
    SizeReq ret;
    if (!dim)
        ret = { 90, 90 };
    else
        ret = { 25, 38 };

    const FontWrapper* font = tiles.get_crt_font();
    const int f = !dim ? font->char_width() : font->char_height();
    ret.min *= f;
    ret.nat *= f;

    return ret;
#else
    if (!dim)
        return { 80, 80 };
    else
        return { 24, 24 };
#endif
}

void UISkillMenu::_allocate_region()
{
    skm.exit(true);
    skm.init(flag, m_region.height);
}

void UISkillMenu::_render()
{
#ifdef USE_TILE_LOCAL
    GLW_3VF t = {(float)m_region.x, (float)m_region.y, 0}, s = {1, 1, 1};
    glmanager->set_transform(t, s);
#endif
    skm.draw_menu();
#ifdef USE_TILE_LOCAL
    glmanager->reset_transform();
#endif
}

#ifdef USE_TILE_LOCAL
bool UISkillMenu::on_event(const Event& ev)
{
    if (ev.type() != Event::Type::MouseMove
     && ev.type() != Event::Type::MouseDown
     && ev.type() != Event::Type::MouseWheel)
    {
        return Widget::on_event(ev);
    }

    const auto mouse_event = static_cast<const MouseEvent&>(ev);

    wm_mouse_event mev = ui::to_wm_event(mouse_event);
    mev.px -= m_region.x;
    mev.py -= m_region.y;

    int key = skm.handle_mouse(mev);
    if (key && key != CK_NO_KEY)
    {
        wm_keyboard_event fake_key = {0};
        fake_key.keysym.sym = key;
        KeyEvent key_ev(Event::Type::KeyDown, fake_key);
        Widget::on_event(key_ev);
    }

    if (ev.type() == Event::Type::MouseMove)
        _expose();

    return true;
}
#endif

void skill_menu(int flag, int exp)
{
    // experience potion; you may elect to put experience in normally
    // untrainable skills (e.g. skills that your god hates). The only
    // case where we abort is if all in-principle trainable skills are maxed.
    if (flag & SKMF_EXPERIENCE && !trainable_skills(true))
    {
        mpr("You feel omnipotent.");
        return;
    }

    unwind_bool xp_gain(crawl_state.simulating_xp_gain, flag & SKMF_EXPERIENCE);

    // notify the player again
    you.received_noskill_warning = false;

    you.exp_available += exp;

    bool done = false;
    auto skill_menu_ui = make_shared<UISkillMenu>(flag);
    auto popup = make_shared<ui::Popup>(skill_menu_ui);

    skill_menu_ui->on_keydown_event([&done, &skill_menu_ui](const KeyEvent& ev) {
        const auto keyn = numpad_to_regular(ev.key(), true);

        skill_menu_ui->_expose();

        if (!skm.process_key(keyn))
        {
            switch (keyn)
            {
            case CK_UP:
            case CK_DOWN:
            case CK_LEFT:
            case CK_RIGHT:
                return true;
            case CK_ENTER:
                if (!skm.is_set(SKMF_EXPERIENCE))
                    return true;
            // Fallthrough. In experience mode, you can exit with enter.
            case CK_ESCAPE:
                // Escape cancels help if it is being displayed.
                if (skm.is_set(SKMF_HELP))
                {
                    skm.cancel_help();
                    return true;
                }
                else if (skm.is_set(SKMF_SET_TARGET))
                {
                    skm.cancel_set_target();
                    return true;
                }
            // Fallthrough
            default:
                if (ui::key_exits_popup(keyn, true) && skm.exit(false))
                    return done = true;
                // Don't exit from !experience on random keys.
                if (!skm.is_set(SKMF_EXPERIENCE) && skm.exit(false))
                    return done = true;
            }
        }
        else
        {
            vector<MenuItem*> selection = skm.get_selected_items();
            skm.clear_selections();
            // There should only be one selection, otherwise something broke
            if (selection.size() != 1)
                return true;
            int sel_id = selection.at(0)->get_id();
            if (sel_id == SKM_HELP)
                skm.help();
            else if (sel_id == SKM_CLEAR_TARGETS)
                skm.clear_targets();
            else if (sel_id == SKM_SET_TARGET)
                skm.set_target_mode();
            else if (sel_id < 0)
            {
                if (sel_id <= SKM_SWITCH_FIRST)
                    skm.toggle((skill_menu_switch)sel_id);
                else
                    dprf("Unhandled menu switch %d", sel_id);
            }
            else
            {
                skill_type sk = static_cast<skill_type>(sel_id);
                ASSERT(!is_invalid_skill(sk));
                skm.select(sk, keyn);
                skill_menu_ui->_expose();
            }
        }

        return true;
    });

#ifdef USE_TILE_WEB
    tiles_crt_popup show_as_popup("skills");
#endif
    // XXX: this is, in theory, an arbitrary initial height. In practice,
    // there's a bug where an item in the MenuFreeform stays at its original
    // position even after skm.init is called again, crashing when the screen
    // size is actually too small; ideally, the skill menu will get rewritten
    // to use the new ui framework; until then, this fixes the crash.
    skm.init(flag, MIN_LINES);

    // Calling a user lua function here to let players automatically accept
    // the given skill distribution for a potion of experience.
    if (skm.is_set(SKMF_EXPERIENCE)
        && clua.callbooleanfn(false, "auto_experience", nullptr)
        && skm.exit(false))
    {
        return;
    }

    ui::run_layout(std::move(popup), done);

    skm.clear();
}
