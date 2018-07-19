/**
 * @file
 * @brief Skill menu.
**/

#include <cmath>

#include "AppHdr.h"

#include "skill-menu.h"

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

#ifdef USE_TILE_LOCAL
bool SkillTextTileItem::handle_mouse(const MouseEvent& me)
{
    if (me.event == MouseEvent::PRESS
        && (me.button == MouseEvent::LEFT && me.mod & TILES_MOD_SHIFT))
    {
        skill_type sk = skill_type(get_id());
        if (is_invalid_skill(sk))
            return false;

        skm.select(sk, 'A');
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
        return you.can_train[sk] || you.skill(sk, 10, false, false)
               || sk == you.transfer_from_skill || sk == you.transfer_to_skill;
    case SKM_SHOW_ALL:     return true;
    default:               return false;
    }
}

bool SkillMenuEntry::is_selectable(bool keep_hotkey)
{
    if (is_invalid_skill(m_sk))
        return false;

    if (is_set(SKMF_HELP))
        return true;

    if (you.species == SP_GNOLL)
        return false;

    if (!_show_skill(m_sk, skm.get_state(SKM_SHOW)))
        return false;

    if (is_set(SKMF_RESKILL_TO) && you.transfer_from_skill == m_sk)
    {
        if (!keep_hotkey)
            ++m_letter;
        return false;
    }

    if (is_set(SKMF_RESKILL_TO) && is_useless_skill(m_sk))
        return false;

    if (is_set(SKMF_RESKILL_FROM) && !you.skill_points[m_sk])
    {
        if (!keep_hotkey)
            ++m_letter;
        return false;
    }

    if (!you.can_train[m_sk] && !is_set(SKMF_RESKILL_TO)
        && !is_set(SKMF_RESKILL_FROM))
    {
        return false;
    }

    if (mastered())
    {
        if (is_set(SKMF_RESKILL_TO) && !keep_hotkey)
            ++m_letter;
        if (!is_set(SKMF_RESKILL_FROM))
            return false;
    }

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
    case SKM_VIEW_TRANSFER:  set_reskill_progress(); break;
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
            m_name->add_hotkey(toupper(m_letter));
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
        {
            m_name->add_tile(tile_def(tileidx_skill(m_sk, TRAINING_MASTERED),
                                      TEX_GUI));
        }
        else if (you.training[m_sk] == TRAINING_DISABLED)
        {
            m_name->add_tile(tile_def(tileidx_skill(m_sk, TRAINING_INACTIVE),
                                      TEX_GUI));
        }
        else
        {
            m_name->add_tile(tile_def(tileidx_skill(m_sk, you.train[m_sk]),
                                      TEX_GUI));
        }
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
    else if (is_set(SKMF_RESKILL_TO) && m_sk == you.transfer_from_skill)
        return BROWN;
    else if (skm.get_state(SKM_VIEW) == SKM_VIEW_TRANSFER
             && (m_sk == you.transfer_from_skill
                 || m_sk == you.transfer_to_skill))
    {
        return CYAN;
    }
    else if (skm.get_state(SKM_LEVEL) == SKM_LEVEL_ENHANCED
             && (you.skill(m_sk, 10, true) != you.skill(m_sk, 10, false)
                 // Drained a tiny but nonzero amount.
                 || you.attribute[ATTR_XP_DRAIN] && you.skill_points[m_sk]))
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
    else if (skill_has_manual(m_sk))
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

    const int sign = (!you.can_train[m_sk] || mastered()) ? ' ' :
                                   (you.train[m_sk] == TRAINING_FOCUSED) ? '*' :
                                          you.train[m_sk] ? '+'
                                                          : '-';
    return make_stringf(" %c %c", letter, sign);
}

void SkillMenuEntry::set_aptitude()
{
    string text = "<white>";

    const bool manual = skill_has_manual(m_sk);
    const int apt = species_apt(m_sk, you.species);

    if (apt != 0)
        text += make_stringf("%+d", apt);
    else
        text += make_stringf(" %d", apt);

    text += "</white> ";

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

    if (mastered() && !you.attribute[ATTR_XP_DRAIN])
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

    if (you.skills[m_sk] > 0 && is_set(SKMF_RESKILL_FROM)
        || m_sk == you.transfer_from_skill)
    {
        new_level = transfer_skill_points(m_sk, m_sk,
                                          skill_transfer_amount(m_sk), true,
                                          !real);
        m_progress->set_fg_colour(BROWN);
    }
    else if (is_set(SKMF_RESKILL_TO))
    {
        new_level = transfer_skill_points(you.transfer_from_skill, m_sk,
                                          you.transfer_skill_points, true,
                                          !real);
        m_progress->set_fg_colour(CYAN);
    }

    if (is_selectable() || m_sk == you.transfer_from_skill)
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
        m_progress->set_text(make_stringf("%d.%d", target / 10, target % 10));
        if (target_met(m_sk))
            m_progress->set_fg_colour(DARKGREY); // mainly comes up in wizmode
        else
            m_progress->set_fg_colour(get_colour());
    }
    m_progress->set_editable(true, 4);
}

void SkillMenuEntry::set_reskill_progress()
{
    string text;
    if (m_sk == you.transfer_from_skill)
        text = "  *  ";
    else if (m_sk == you.transfer_to_skill)
    {
        text += make_stringf(" %2d%%",
                             (you.transfer_total_skill_points
                             - you.transfer_skill_points)
                                 * 100 / you.transfer_total_skill_points);
    }
    else
        text = "";

    m_progress->set_text(text);
    m_progress->set_fg_colour(CYAN);
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

    if (is_set(SKMF_RESKILLING))
    {
        if (is_set(SKMF_RESKILL_FROM))
            m_progress->set_text("Source");
        else
            m_progress->set_text("Target");
        return;
    }

    switch (skm.get_state(SKM_VIEW))
    {
    case SKM_VIEW_TRAINING:  m_progress->set_text("Train"); break;
    case SKM_VIEW_TARGETS:   m_progress->set_text("Target"); break;
    case SKM_VIEW_PROGRESS:  m_progress->set_text("Progr"); break;
    case SKM_VIEW_TRANSFER:  m_progress->set_text("Trnsf"); break;
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
    if (skill_has_manual(m_sk))
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
            result = "Skills enhanced by "
                     + comma_separated_line(causes.begin(), causes.end())
                     + " are in <green>green</green>.";
        }

        if (skm.is_set(SKMF_REDUCED))
        {
            vector<const char *> causes;
            if (you.attribute[ATTR_XP_DRAIN])
                causes.push_back("draining");
            if (player_under_penance(GOD_ASHENZARI))
                causes.push_back("Ashenzari's anger");

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
        return "The current training targets, if any.\n";
    case SKM_VIEW_PROGRESS:
        return "The percentage of the progress done before reaching next "
               "level is in <cyan>cyan</cyan>.\n";
    case SKM_VIEW_TRANSFER:
        return "The progress of the knowledge transfer is displayed in "
               "<cyan>cyan</cyan> in front of the skill receiving the "
               "knowledge. The donating skill is marked with <cyan>*</cyan>.";
    case SKM_VIEW_COST:
    {
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
    case SKM_SHOW_DEFAULT:   return "trainable";
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
    case SKM_VIEW_TRANSFER:  return "transfer";
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
        you.auto_training = false;
        reset_training();

        for (int i = 0; i < NUM_SKILLS; ++i)
        {
            const skill_type sk = skill_type(i);
            if (!is_useless_skill(sk) && !you.can_train[sk])
            {
                you.can_train.set(sk);
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

static keyfun_action _keyfun_target_input(int &ch)
{
    if (ch == '-')
        return KEYFUN_BREAK; // reset to 0
    if (ch == CONTROL('K') || ch == CONTROL('D') || ch == CONTROL('W') ||
            ch == CONTROL('U') || ch == CONTROL('A') || ch == CONTROL('E') ||
            ch == CK_ENTER || ch == CK_BKSP || ch == CK_ESCAPE ||
            ch < 0 || // this should get all other special keys
            ch == '.' || isadigit(ch))
    {
        return KEYFUN_PROCESS;
    }
    return KEYFUN_IGNORE;
}

int SkillMenu::read_skill_target(skill_type sk, int keyn)
{
    SkillMenuEntry *entry = find_entry(sk);
    ASSERT(entry);
    EditableTextItem *progress = entry->get_progress();
    ASSERT(progress);

    const int old_target = you.get_training_target(sk);
    const string prefill = old_target <= 0 ? "0"
                    : make_stringf("%d.%d", old_target / 10, old_target % 10);

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
        input = round(atof(result_buf) * 10.0);    // TODO: parse fixed point?

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
        ASSERT(you.species != SP_GNOLL);
        set_help("<lightred>You need to enable at least one skill.</lightred>");
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
        case SKM_SHOW:  return SKM_SHOW_DEFAULT;
        case SKM_LEVEL: return SKM_LEVEL_NORMAL;
        case SKM_VIEW:  return SKM_VIEW_NEW_LEVEL;
        default:        return SKM_NONE;
        }
    }
    else if (!m_switches[sw])
    {
        if (you.species == SP_GNOLL)
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
    else if (is_set(SKMF_RESKILL_FROM))
        you.transfer_from_skill = sk;
    else if (is_set(SKMF_RESKILL_TO))
        you.transfer_to_skill = sk;
    else if (skm.get_state(SKM_VIEW) == SKM_VIEW_TARGETS
                                            && skm.is_set(SKMF_SET_TARGET))
    {
        read_skill_target(sk, keyn);
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

    // You might be drained by a small enough amount to not affect the
    // rounded numbers.
    if (you.attribute[ATTR_XP_DRAIN])
        set_flag(SKMF_REDUCED);
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
        add_item(m_middle_button, 27, m_pos);

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
    if (you.species != SP_GNOLL)
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
        if (!is_set(SKMF_RESKILLING) && !is_set(SKMF_SIMPLE)
            && Options.skill_focus != SKM_FOCUS_OFF)
        {
            sw->add(SKM_DO_FOCUS);
        }
        sw->set_state(you.skill_menu_do);
        sw->add_hotkey('\t');
        sw->update();
        sw->set_id(SKM_DO);
        add_item(sw, sw->size(), m_pos);

        sw = new SkillMenuSwitch("skills", '*');
        m_switches[SKM_SHOW] = sw;
        sw->add(SKM_SHOW_DEFAULT);
        if (!is_set(SKMF_SIMPLE) && !is_set(SKMF_EXPERIENCE))
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
    const bool transferring = !is_invalid_skill(you.transfer_to_skill);
    if (!is_set(SKMF_SPECIAL) || you.wizard)
    {
        sw->add(SKM_VIEW_TRAINING);

        if (transferring)
        {
            sw->add(SKM_VIEW_TRANSFER);
            sw->set_state(SKM_VIEW_TRANSFER);
        }

        sw->add(SKM_VIEW_COST);

        if (!you.auto_training)
            sw->set_state(SKM_VIEW_COST);
    }

    if (you.species != SP_GNOLL)
        sw->add(SKM_VIEW_TARGETS);

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

    string legend = is_set(SKMF_SIMPLE) ? "Read skill descriptions" : "Help";
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
    else if (you.species != SP_GNOLL) // SKM_VIEW_TARGETS unavailable for Gn
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
    if (is_set(SKMF_RESKILL_FROM))
    {
        text = "Select a skill as the source of the knowledge transfer. The "
               "chosen skill will be reduced to the level shown in "
               "<brown>brown</brown>.";
    }
    else if (is_set(SKMF_RESKILL_TO))
    {
        text = "Select a skill as the destination of the knowledge transfer. "
               "The chosen skill will be raised to the level shown in "
               "<cyan>cyan</cyan>.";
    }
    else if (is_set(SKMF_EXPERIENCE))
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

    // This one takes priority.
    if (get_state(SKM_VIEW) == SKM_VIEW_TRANSFER)
        text = m_switches[SKM_VIEW]->get_help();

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
    ASSERT(you.can_train[sk]);
    if (keyn >= 'A' && keyn <= 'Z')
        you.train.init(TRAINING_DISABLED);
    if (get_state(SKM_DO) == SKM_DO_PRACTISE)
        you.train[sk] = (you.train[sk] ? TRAINING_DISABLED : TRAINING_ENABLED);
    else if (get_state(SKM_DO) == SKM_DO_FOCUS)
    {
        you.train[sk]
            = (training_status)((you.train[sk] + 1) % NUM_TRAINING_STATUSES);
    }
    else
        die("Invalid state.");
    reset_training();
    SkillMenuEntry* skme = find_entry(sk);
    skme->set_name(true);
    const vector<int> hotkeys = skme->get_name_item()->get_hotkeys();

    if (!hotkeys.empty())
    {
        int letter;
        letter = hotkeys[0];
        MenuItem* next_item = m_ff->find_item_by_hotkey(++letter);
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
    const char* format = is_set(SKMF_RESKILLING)
                                ? "Transfer Knowledge: select the %s skill"
                                : "You have %s. Select the skills to train.";
    string t;
    if (is_set(SKMF_RESKILL_FROM))
        t = make_stringf(format, "source");
    else if (is_set(SKMF_RESKILL_TO))
        t = make_stringf(format, "destination");
    else if (is_set(SKMF_EXPERIENCE))
        t = make_stringf(format, "quaffed a potion of experience");

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
    virtual bool on_event(const wm_event& ev) override;
#endif

protected:
    int flag;
};

SizeReq UISkillMenu::_get_preferred_size(Direction dim, int prosp_width)
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
    int height = m_region[3];
    skm.init(flag, height);
}

void UISkillMenu::_render()
{
#ifdef USE_TILE_LOCAL
    GLW_3VF t = {(float)m_region[0], (float)m_region[1], 0}, s = {1, 1, 1};
    glmanager->set_transform(t, s);
#endif
    skm.draw_menu();
#ifdef USE_TILE_LOCAL
    glmanager->reset_transform();
#endif
}

#ifdef USE_TILE_LOCAL
bool UISkillMenu::on_event(const wm_event& ev)
{
    if (ev.type != WME_MOUSEMOTION
     && ev.type != WME_MOUSEBUTTONDOWN
     && ev.type != WME_MOUSEWHEEL)
    {
        return Widget::on_event(ev);
    }

    MouseEvent mouse_ev = ev.mouse_event;
    mouse_ev.px -= m_region[0];
    mouse_ev.py -= m_region[1];

    int key = skm.handle_mouse(mouse_ev);
    if (key && key != CK_NO_KEY)
    {
        wm_event fake_key = {0};
        fake_key.type = WME_KEYDOWN;
        fake_key.key.keysym.sym = key;
        Widget::on_event(fake_key);
    }

    if (ev.type == WME_MOUSEMOTION)
        _expose();

    return true;
}
#endif

void skill_menu(int flag, int exp)
{
    // experience potion; you may elect to put experience in normally
    // untrainable skills (skills where you aren't carrying the right item,
    // or skills that your god hates). The only case where we abort is if all
    // in-principle trainable skills are maxed.
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

    skill_menu_ui->on(Widget::slots.event, [&done, &skill_menu_ui](wm_event ev) {
        if (ev.type != WME_KEYDOWN)
            return false;
        int keyn = ev.key.keysym.sym;

        skill_menu_ui->_expose();

        if (!skm.process_key(keyn))
        {
            switch (keyn)
            {
            case CK_UP:
            case CK_DOWN:
            case CK_LEFT:
            case CK_RIGHT:
            case 1004:
            case 1002:
            case 1008:
            case 1006:
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
            case ' ':
                // Space and escape exit in any mode.
                if (skm.exit(false))
                    return done = true;
            default:
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
                if (skm.is_set(SKMF_RESKILLING))
                {
                    skm.clear();
                    return done = true;
                }
            }
        }

        return true;
    });

#ifdef USE_TILE_WEB
    tiles_crt_control show_as_menu(CRT_MENU, "skills");
#endif
    auto popup = make_shared<ui::Popup>(skill_menu_ui);
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

    ui::run_layout(move(popup), done);

    skm.clear();
}
