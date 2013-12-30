/**
 * @file
 * @brief Skill menu.
**/

#include "AppHdr.h"

#include "skill_menu.h"

#include "cio.h"
#include "command.h"
#include "describe.h"
#include "evoke.h"
#include "fontwrapper-ft.h"
#include "hints.h"
#include "libutil.h"
#include "options.h"
#include "player.h"
#include "religion.h"
#include "skills.h"
#include "skills2.h"
#include "state.h"
#include "stuff.h"
#ifdef USE_TILE
 #include "tilepick.h"
 #include "tilereg-crt.h"
#endif

menu_letter2 SkillMenuEntry::m_letter;
SkillMenu skm;

#ifdef USE_TILE_LOCAL
bool SkillTextTileItem::handle_mouse(const MouseEvent& me)
{
    if (me.event == MouseEvent::PRESS
        && (me.button == MouseEvent::LEFT && me.mod & MOD_SHIFT
            || me.button == MouseEvent::RIGHT))
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
    m_progress = new NoSelectTextItem();
    m_aptitude = new FormattedTextItem();

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
    case SKM_SHOW_KNOWN:   return you.skills[sk];
    case SKM_SHOW_DEFAULT: return (you.can_train[sk] || you.skills[sk]);
    case SKM_SHOW_ALL:     return true;
    default:               return false;
    }
}

bool SkillMenuEntry::is_selectable(bool keep_hotkey)
{
    if (is_invalid_skill(m_sk))
        return false;

    if (!_show_skill(m_sk, skm.get_state(SKM_SHOW)))
        return false;

    if (is_set(SKMF_HELP))
        return true;

    if (is_set(SKMF_RESKILL_TO) && you.transfer_from_skill == m_sk)
    {
        if (!keep_hotkey)
            ++m_letter;
        return false;
    }

    if (is_set(SKMF_RESKILL_TO) && is_useless_skill(m_sk))
        return false;

    if (is_set(SKMF_RESKILL_FROM)
        && you.skill_points[m_sk] <= skill_exp_needed(1, m_sk))
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
                                    : you.skills[m_sk]) >= 27;

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
        if (you.skills[m_sk] >= 27)
            m_name->add_tile(tile_def(tileidx_skill(m_sk, -1), TEX_GUI));
        else if (!you.training[m_sk])
            m_name->add_tile(tile_def(tileidx_skill(m_sk, 0), TEX_GUI));
        else
            m_name->add_tile(tile_def(tileidx_skill(m_sk, you.train[m_sk]), TEX_GUI));
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
COLORS SkillMenuEntry::get_colour() const
{
    if (is_set(SKMF_HELP))
        return DARKGREY;
    else if (is_set(SKMF_RESKILL_TO) && m_sk == you.transfer_from_skill)
        return BROWN;
    else if (skm.get_state(SKM_VIEW) == SKM_VIEW_TRANSFER
             && (m_sk == you.transfer_from_skill
                 || m_sk == you.transfer_to_skill))
    {
        return CYAN;
    }
    else if (skm.get_state(SKM_LEVEL) == SKM_LEVEL_ENHANCED
             && you.skill(m_sk, 10, true) != you.skill(m_sk, 10, false))
    {
        if (you.skill(m_sk, 10, true) > you.skill(m_sk, 10, false))
            return you.train[m_sk] ? LIGHTMAGENTA : MAGENTA;
        else
            return you.train[m_sk] ? LIGHTBLUE : BLUE;
    }
    else if (mastered())
        return YELLOW;
    else if (!you.training[m_sk])
        return DARKGREY;
    else if (skill_has_manual(m_sk))
    {
        if (is_set(SKMF_APTITUDE))
            return LIGHTGREEN;
        return you.train[m_sk] ? LIGHTGREEN : GREEN;
    }
    else if (crosstrain_bonus(m_sk) > 1 && is_set(SKMF_APTITUDE))
        return GREEN;
    else if (is_antitrained(m_sk) && is_set(SKMF_APTITUDE))
        return RED;
    else if (you.train[m_sk] == 2)
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
                                   (you.train[m_sk] == 2) ? '*' :
                                          you.train[m_sk] ? '+'
                                                          : '-';
#ifdef USE_TILE_LOCAL
    return make_stringf(" %c %c", letter, sign);
#else
    return make_stringf("%c %c", letter, sign);
#endif
}

void SkillMenuEntry::set_aptitude()
{
    string text = "<white>";

    const bool manual = skill_has_manual(m_sk);
    const int apt = species_apt(m_sk, you.species);
    const bool show_all = skm.get_state(SKM_SHOW) == SKM_SHOW_ALL;

    // Crosstraining + manuals aptitude bonus.
    int ct_bonus = manual ? 4 : 0;

    for (int ct_mult = crosstrain_bonus(m_sk); ct_mult > 1; ct_mult /= 2)
        ct_bonus += 4;

    if (apt != 0)
        text += make_stringf("%+d", apt);
    else
        text += make_stringf(" %d", apt);

    text += "</white>";

    if (antitrain_other(m_sk, show_all))
    {
        skm.set_flag(SKMF_ANTITRAIN);
        text += "<red>*</red>";
    }
    else if (crosstrain_other(m_sk, show_all))
    {
        skm.set_flag(SKMF_CROSSTRAIN);
        text += "<green>*</green>";
    }
    else
        text += " ";

    if (is_antitrained(m_sk))
    {
        skm.set_flag(SKMF_ANTITRAIN);
        text += make_stringf("<red>%d</red>", ct_bonus - 4);
    }
    else if (ct_bonus)
    {
        skm.set_flag(SKMF_CROSSTRAIN);
        text += manual ? "<lightgreen>" : "<green>";

        // Only room for two characters.
        if (ct_bonus < 10)
            text += make_stringf("+%d", ct_bonus);
        else
            text += make_stringf("%d", ct_bonus);

        text += manual ? "</lightgreen>" : "</green>";
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
        m_level->set_text(make_stringf("%d", level / 10));
    else
        m_level->set_text(make_stringf("%4.1f", level / 10.0));
    m_level->set_fg_colour(get_colour());
}

void SkillMenuEntry::set_new_level()
{
    const bool real = skm.get_state(SKM_LEVEL) != SKM_LEVEL_ENHANCED;
    if (is_set(SKMF_EXPERIENCE) && is_selectable())
    {
        m_progress->set_fg_colour(CYAN);
        m_progress->set_text(make_stringf("->%4.1f",
                                          you.skill(m_sk, 10, real) / 10.0));
        return;
    }

    int new_level = 0;
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
        m_progress->set_text(make_stringf("->%4.1f", new_level / 10.0));
    else
        m_progress->set_text("");
}

void SkillMenuEntry::set_points()
{
    m_progress->set_text(make_stringf("%5d", you.skill_points[m_sk]));
    m_progress->set_fg_colour(LIGHTGREY);
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
    m_name->set_text("    Skill");
    m_level->set_text("Level");

    m_name->set_fg_colour(BLUE);
    m_level->set_fg_colour(BLUE);
    m_progress->set_fg_colour(BLUE);

    if (is_set(SKMF_APTITUDE))
        m_aptitude->set_text("<blue>Apt </blue>");

    if (is_set(SKMF_RESKILLING))
    {
        m_progress->set_text("-> Lvl");
        return;
    }

    switch (skm.get_state(SKM_VIEW))
    {
    case SKM_VIEW_TRAINING:  m_progress->set_text("Train"); break;
    case SKM_VIEW_PROGRESS:  m_progress->set_text("Progr"); break;
    case SKM_VIEW_TRANSFER:  m_progress->set_text("Trnsf"); break;
    case SKM_VIEW_POINTS:    m_progress->set_text("Pnts");  break;
    case SKM_VIEW_NEW_LEVEL: m_progress->set_text("New");   break;
    default: die("Invalid view state.");
    }
}

void SkillMenuEntry::set_training()
{
    if (!you.training[m_sk])
        m_progress->set_text("");
    else
        m_progress->set_text(make_stringf(" %2d%%", you.training[m_sk]));
    m_progress->set_fg_colour(BROWN);
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
        {
            return "Press the letter of a skill to choose whether you want to "
                   "practise it. Skills marked with '-' will not be trained.";
        }
    case SKM_DO_FOCUS:
        return "Press the letter of a skill to cycle between "
               "<darkgrey>disabled</darkgrey> (-), enabled (+) and "
               "<white>focused</white> (*). Focused skills train twice as "
               "fast as others.";
    case SKM_LEVEL_ENHANCED:
        if (skm.is_set(SKMF_ENHANCED))
        {
            return make_stringf("Skills enhanced by the power of %s are in "
                                "<blue>blue</blue>. ",
                                god_name(you.religion).c_str());
        }
        else
        {
            vector<string> causes;
            if (you.attribute[ATTR_XP_DRAIN])
                causes.push_back("draining");
            if (player_under_penance(GOD_ASHENZARI))
                causes.push_back("the power of Ashenzari");
            return "Skills reduced by "
                   + comma_separated_line(causes.begin(), causes.end())
                   + " are in <magenta>magenta</magenta>. ";
        }
    case SKM_VIEW_TRAINING:
        if (skm.is_set(SKMF_SIMPLE))
            return hints_skill_training_info();
        else
        {
            return "The percentage of the experience used to train each skill "
                   "is in <brown>brown</brown>.\n";
        }
    case SKM_VIEW_PROGRESS:
        return "The percentage of the progress done before reaching next "
               "level is in <cyan>cyan</cyan>.\n";
    case SKM_VIEW_TRANSFER:
        return "The progress of the knowledge transfer is displayed in "
               "<cyan>cyan</cyan> in front of the skill receiving the "
               "knowledge. The donating skill is marked with '*'.";
    default: return "";
    }
}

string SkillMenuSwitch::get_name(skill_menu_state state)
{
    switch (state)
    {
    case SKM_MODE_AUTO:      return "auto";
    case SKM_MODE_MANUAL:    return "manual";
    case SKM_DO_PRACTISE:    return "practise";
    case SKM_DO_FOCUS:       return "focus";
    case SKM_SHOW_KNOWN:     return "known";
    case SKM_SHOW_DEFAULT:   return "default";
    case SKM_SHOW_ALL:       return "all";
    case SKM_LEVEL_ENHANCED:
        return (skm.is_set(SKMF_ENHANCED)
                && skm.is_set(SKMF_REDUCED)) ? "changed" :
                   skm.is_set(SKMF_ENHANCED) ? "enhanced"
                                             : "reduced";
    case SKM_LEVEL_NORMAL:   return "normal";
    case SKM_VIEW_TRAINING:  return "training";
    case SKM_VIEW_PROGRESS:  return "progress";
    case SKM_VIEW_TRANSFER:  return "transfer";
    case SKM_VIEW_POINTS:    return "points";
    case SKM_VIEW_NEW_LEVEL: return "new level";
    default: die ("Invalid switch state.");
    }
}

void SkillMenuSwitch::set_state(skill_menu_state state)
{
    // We only set it if it's a valid state.
    for (vector<skill_menu_state>::iterator it = m_states.begin();
         it != m_states.end(); ++it)
    {
        if (*it == state)
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

    vector<skill_menu_state>::iterator it = m_states.begin();
    while (*it != m_state)
        ++it;

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
    string text = make_stringf("[%s(<yellow>%c</yellow>): ",
                               m_name.c_str(), hotkeys[0]);
    for (vector<skill_menu_state>::iterator it = m_states.begin();
         it != m_states.end(); ++it)
    {
        if (it != m_states.begin())
            text += '|';

        const string col = (*it == m_state) ? "white" : "darkgrey";
        text += make_stringf("<%s>%s</%s>", col.c_str(), get_name(*it).c_str(),
                             col.c_str());
    }
    text += ']';
    set_text(text);
}

#define TILES_COL 6
SkillMenu::SkillMenu() : PrecisionMenu(), m_min_coord(), m_max_coord(),
                         m_help_button(NULL)
{
}

void SkillMenu::init(int flag)
{
    m_flags = flag;
    init_flags();

    if (is_set(SKMF_EXPERIENCE))
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
                you.train[sk] = false;
            }
        }
    }

#ifdef USE_TILE_LOCAL
    const int char_height = tiles.get_crt_font()->char_height();
    if (Options.tile_menu_icons)
    {
        line_height = min((tiles.get_crt()->wy - char_height * 4) / SK_ARR_LN,
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
    m_max_coord.y = get_number_of_lines();
    if (is_set(SKMF_SKILL_ICONS))
    {
        m_ff->set_height(line_height);
        m_max_coord.x += 2 * TILES_COL;
    }
#else
    m_max_coord.y = get_number_of_lines() + 1;
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

    init_help();
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

#ifdef USE_TILE_LOCAL
    tiles.get_crt()->attach_menu(this);
#endif
    m_highlighter = new BoxMenuHighlighter(this);
    m_highlighter->init(coord_def(-1,-1), coord_def(-1,-1), "highlighter");
    attach_object(m_highlighter);

    m_ff->set_visible(true);
    m_highlighter->set_visible(true);
}

void SkillMenu::clear()
{
    PrecisionMenu::clear();
    m_switches.clear();
    m_help_button = 0;
}

//Public methods
void SkillMenu::clear_flag(int flag)
{
    m_flags &= ~flag;
}

bool SkillMenu::is_set(int flag) const
{
    return (m_flags & flag);
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

// Before we exit, make sure there's at least one skill enabled.
bool SkillMenu::exit()
{
    if (crawl_state.seen_hups)
    {
        clear();
        return true;
    }

    bool enabled_skill = false;

    for (int i = 0; i < NUM_SKILLS; ++i)
    {
        if (skill_trained(i))
        {
            enabled_skill = true;
            break;
        }
    }

    if (!enabled_skill && !all_skills_maxed())
    {
        set_help("<lightred>You need to enable at least one skill.</lightred>");
        return false;
    }

    if (is_set(SKMF_EXPERIENCE))
    {
        redraw_screen();
        train_skills();
        m_skill_backup.restore_training();
    }

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
        return SKM_NONE;
    else
        return m_switches[sw]->get_state();
}

void SkillMenu::help()
{
    if (!is_set(SKMF_HELP))
    {
        string text;
        if (is_set(SKMF_SIMPLE))
            text = hints_skills_description_info();
        else
            text = "Press the letter of a skill to read its description. "
                   "Press ? for an explanation of how skills work and the "
                   "various modes.";
        set_help(text);
    }
    else
    {
        if (!is_set(SKMF_SIMPLE))
        {
            show_skill_menu_help();
            clrscr();
#ifdef USE_TILE_LOCAL
            tiles.get_crt()->attach_menu(this);
#endif
        }
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
    FixedVector<int8_t, NUM_SKILLS> tmp;

    switch (sw)
    {
    case SKM_MODE:
        you.auto_training = !you.auto_training;

        // Switch the skill train state with the saved version.
        tmp = you.train;
        you.train = you.train_alt;
        you.train_alt = tmp;

        reset_training();
        if (get_state(SKM_VIEW) == SKM_VIEW_TRAINING)
            refresh_display();
        break;
    case SKM_DO:
        you.skill_menu_do = get_state(SKM_DO);
        refresh_names();
        if (m_ff->get_active_item() != NULL
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
    //fall through.
    case SKM_LEVEL:
        refresh_display();
    }
    set_help(m_switches[sw]->get_help());
}

// Private methods
SkillMenuEntry* SkillMenu::find_entry(skill_type sk)
{
    for (int col = 0; col < SK_ARR_COL; ++col)
        for (int ln = 0; ln < SK_ARR_LN; ++ln)
            if (m_skills[ln][col].get_skill() == sk)
                return &m_skills[ln][col];

    return NULL;
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

void SkillMenu::init_help()
{
    m_help = new FormattedTextItem();
    int help_height;
    if (is_set(SKMF_SIMPLE))
    {
        // We just assume that the player won't learn too many skills while
        // in tutorial/hint mode.
        m_pos.y -= 4;
        help_height = 6;
    }
    else
        help_height = 2;

    m_help->set_bounds(m_pos, coord_def(m_max_coord.x, m_pos.y + help_height));
    m_pos.y += help_height;
    m_ff->attach_item(m_help);
    m_help->set_fg_colour(LIGHTGREY);
    m_help->set_visible(true);
}

void SkillMenu::init_switches()
{
    SkillMenuSwitch* sw;
    sw = new SkillMenuSwitch("Mode", '/');
    m_switches[SKM_MODE] = sw;
    sw->add(SKM_MODE_AUTO);
    if (!is_set(SKMF_SPECIAL) && !is_set(SKMF_SIMPLE))
        sw->add(SKM_MODE_MANUAL);
    if (!you.auto_training)
        sw->set_state(SKM_MODE_MANUAL);
    sw->update();
    sw->set_id(SKM_MODE);
    add_item(sw, sw->size(), m_pos);

    sw = new SkillMenuSwitch("Do", '|');
    m_switches[SKM_DO] = sw;
    if (!is_set(SKMF_EXPERIENCE) && Options.skill_focus != SKM_FOCUS_ON)
        sw->add(SKM_DO_PRACTISE);
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

    sw = new SkillMenuSwitch("Show", '*');
    m_switches[SKM_SHOW] = sw;
    //sw->add(SKM_SHOW_KNOWN);
    sw->add(SKM_SHOW_DEFAULT);
    if (!is_set(SKMF_SIMPLE) && !is_set(SKMF_EXPERIENCE))
        sw->add(SKM_SHOW_ALL);
    sw->update();
    sw->set_id(SKM_SHOW);
    add_item(sw, sw->size(), m_pos);

    if (is_set(SKMF_CHANGED))
    {
        sw = new SkillMenuSwitch("Level", '_');
        m_switches[SKM_LEVEL] = sw;
        sw->add(SKM_LEVEL_ENHANCED);
        sw->add(SKM_LEVEL_NORMAL);
        sw->update();
        sw->set_id(SKM_LEVEL);
        add_item(sw, sw->size(), m_pos);
    }


    sw = new SkillMenuSwitch("View", '!');
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

    if (!is_set(SKMF_SPECIAL))
    {
        m_help_button = new FormattedTextItem();
        m_help_button->set_text("[Help(<yellow>?</yellow>)]");
        m_help_button->set_id(SKM_HELP);
        m_help_button->add_hotkey('?');
        m_help_button->set_highlight_colour(YELLOW);
        add_item(m_help_button, 9, m_pos);
    }
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
    if (m_ff->get_active_item() != NULL
        && !m_ff->get_active_item()->can_be_highlighted())
    {
        m_ff->activate_default_item();
    }
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
        if (!is_set(SKMF_CROSSTRAIN) && !is_set(SKMF_ANTITRAIN))
            text = m_switches[SKM_VIEW]->get_help();

        if (get_state(SKM_LEVEL) == SKM_LEVEL_ENHANCED
            && !(is_set(SKMF_CROSSTRAIN) && is_set(SKMF_ANTITRAIN)))
        {
            text += m_switches[SKM_LEVEL]->get_help();
        }
        else
            text += "The species aptitude is in <white>white</white>. ";

        if (is_set(SKMF_CROSSTRAIN))
            text += "Crosstraining is in <green>green</green>. ";
        if (is_set(SKMF_ANTITRAIN))
            text += "Antitraining is in <red>red</red>. ";

        if (is_set(SKMF_CROSSTRAIN) && is_set(SKMF_ANTITRAIN))
        {
            text += "The skill responsible for the bonus or malus is "
                    "marked with '*'.";
        }
        else if (is_set(SKMF_CROSSTRAIN))
        {
            text += "The skill responsible for the bonus is marked with "
                    "'<green>*</green>'.";
        }
        else if (is_set(SKMF_ANTITRAIN))
        {
            text += "The skill responsible for the malus is marked with "
                    "'<red>*</red>'.";
        }
    }

    // This one takes priority.
    if (get_state(SKM_VIEW) == SKM_VIEW_TRANSFER)
        text = m_switches[SKM_VIEW]->get_help();

    m_help->set_text(text);
}

void SkillMenu::set_help(string msg)
{
    if (msg == "")
        set_default_help();
    else
        m_help->set_text(msg);
}

void SkillMenu::set_skills()
{
    int previous_active;
    if (m_ff->get_active_item() != NULL)
        previous_active = m_ff->get_active_item()->get_id();
    else
        previous_active = -1;

    SkillMenuEntry::m_letter = '9';
    bool default_set = false;
    clear_flag(SKMF_CROSSTRAIN);
    clear_flag(SKMF_ANTITRAIN);

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
        you.train.init(0);
    if (get_state(SKM_DO) == SKM_DO_PRACTISE)
        you.train[sk] = !you.train[sk];
    else if (get_state(SKM_DO) == SKM_DO_FOCUS)
        you.train[sk] = (you.train[sk] + 1) % 3;
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
        if (next_item != NULL)
        {
            if (m_ff->get_active_item() != NULL && keyn == CK_ENTER)
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
    else if (is_set(SKMF_EXPERIENCE_CARD) && is_set(SKMF_EXPERIENCE_POTION))
        t = "You are more experienced. Select the skills to train.";
    else if (is_set(SKMF_EXPERIENCE_CARD))
        t = make_stringf(format, "drawn an Experience card");
    else if (is_set(SKMF_EXPERIENCE_POTION))
        t = make_stringf(format, "quaffed a potion of experience");

    m_title->set_text(t);
}

void SkillMenu::shift_bottom_down()
{
    const coord_def down(0, 1);
    m_help->move(down);
    for (map<skill_menu_switch, SkillMenuSwitch*>::iterator it
         = m_switches.begin(); it != m_switches.end(); ++it)
    {
        it->second->move(down);
    }
    if (m_help_button)
        m_help_button->move(down);
}

void SkillMenu::show_description(skill_type sk)
{
    describe_skill(sk);
    clrscr();
#ifdef USE_TILE_LOCAL
    tiles.get_crt()->attach_menu(this);
#endif
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
            return NULL;

        ++delta;
    }
}

void SkillMenu::set_links()
{
    TextItem* top_left = NULL;
    TextItem* top_right = NULL;
    TextItem* bottom_left = NULL;
    TextItem* bottom_right = NULL;

    for (int ln = 0; ln < SK_ARR_LN; ++ln)
    {
        if (m_skills[ln][0].is_selectable())
        {
            TextItem* left = m_skills[ln][0].get_name_item();
            left->set_link_up(NULL);
            left->set_link_down(NULL);
            left->set_link_right(find_closest_selectable(ln, 1));
            if (top_left == NULL)
                top_left = left;
            bottom_left = left;
        }
        if (m_skills[ln][1].is_selectable())
        {
            TextItem* right = m_skills[ln][1].get_name_item();
            right->set_link_up(NULL);
            right->set_link_down(NULL);
            right->set_link_left(find_closest_selectable(ln, 0));
            if (top_right == NULL)
                top_right = right;
            bottom_right = right;
        }
    }
    if (top_left != NULL)
    {
        top_left->set_link_up(bottom_right);
        bottom_left->set_link_down(top_right);
    }
    if (top_right != NULL)
    {
        top_right->set_link_up(bottom_left);
        bottom_right->set_link_down(top_left);
    }
}

void skill_menu(int flag, int exp)
{
    // potion or card; you may elect to sin against Trog
    if (flag & SKMF_EXPERIENCE && all_skills_maxed(true))
    {
        mpr("You feel omnipotent.");
        return;
    }

    // notify the player again
    you.received_noskill_warning = false;

    you.exp_available += exp;

#ifdef USE_TILE_WEB
    tiles_crt_control show_as_menu(CRT_MENU, "skills");
#endif

    clrscr();
    skm.init(flag);
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
            case 1004:
            case 1002:
            case 1008:
            case 1006:
                continue;
            case CK_ENTER:
                if (!skm.is_set(SKMF_EXPERIENCE))
                    continue;
            // Fallthrough. In experience mode, you can exit with enter.
            case CK_ESCAPE:
                // Escape cancels help if it is being displayed.
                if (skm.is_set(SKMF_HELP))
                {
                    skm.cancel_help();
                    continue;
                }
            // Fallthrough
            case ' ':
                // Space and escape exit in any mode.
                if (skm.exit())
                    return;
            default:
                // Don't exit from !experience on random keys.
                if (!skm.is_set(SKMF_EXPERIENCE) && skm.exit())
                    return;
            }
        }
        else
        {
            vector<MenuItem*> selection = skm.get_selected_items();
            skm.clear_selections();
            // There should only be one selection, otherwise something broke
            if (selection.size() != 1)
                continue;
            int sel_id = selection.at(0)->get_id();
            if (sel_id == SKM_HELP)
                skm.help();
            else if (sel_id < 0)
                skm.toggle((skill_menu_switch)sel_id);
            else
            {
                skill_type sk = static_cast<skill_type>(sel_id);
                ASSERT(!is_invalid_skill(sk));
                skm.select(sk, keyn);
                if (skm.is_set(SKMF_RESKILLING))
                {
                    skm.clear();
                    return;
                }
            }
        }
    }
}
