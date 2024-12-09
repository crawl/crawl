/**
 * @file
 * @brief A hints mode as an introduction on how to play Dungeon Crawl.
**/

#include "AppHdr.h"

#include "hints.h"

#include <cstring>
#include <sstream>

#include "ability.h"
#include "artefact.h"
#include "cloud.h"
#include "colour.h"
#include "database.h"
#include "describe.h"
#include "end.h"
#include "english.h"
#include "env.h"
#include "fprop.h"
#include "invent.h"
#include "item-prop.h"
#include "item-status-flag-type.h"
#include "items.h"
#include "jobs.h"
#include "libutil.h"
#include "macro.h"
#include "message.h"
#include "mutation.h"
#include "outer-menu.h"
#include "nearby-danger.h"
#include "options.h"
#include "output.h"
#include "religion.h"
#include "shopping.h"
#include "showsymb.h"
#include "skills.h"
#include "spl-book.h"
#include "state.h"
#include "stringutil.h"
#include "tag-version.h"
#include "terrain.h"
#include "tilepick-p.h"
#include "travel.h"
#include "viewchar.h"
#include "viewgeom.h"
#include "viewmap.h"
#include "ui.h"

using namespace ui;

static species_type _get_hints_species(unsigned int type);
static job_type     _get_hints_job(unsigned int type);
static bool         _hints_feat_interesting(dungeon_feature_type feat);
static void         _hints_describe_cloud(int x, int y, ostringstream& ostr);
static void         _hints_describe_feature(int x, int y, ostringstream& ostr);
static void         _hints_healing_reminder();

static int _get_hints_cols()
{
#ifdef USE_TILE_LOCAL
    return crawl_view.msgsz.x;
#else
    int ncols = get_number_of_cols();
    return ncols > 80 ? 80 : ncols;
#endif
}

hints_state Hints;

void save_hints(writer& outf)
{
    marshallInt(outf, HINT_EVENTS_NUM);
    marshallShort(outf, Hints.hints_type);
    for (int i = 0; i < HINT_EVENTS_NUM; ++i)
        marshallBoolean(outf, Hints.hints_events[i]);
}

void load_hints(reader& inf)
{
    int version = unmarshallInt(inf);
    // Discard everything if the number doesn't match.
    if (version != HINT_EVENTS_NUM)
        return;

    Hints.hints_type = unmarshallShort(inf);
    for (int i = 0; i < HINT_EVENTS_NUM; ++i)
        Hints.hints_events[i] = unmarshallBoolean(inf);
}

// Override init file definition for some options.
void init_hints_options()
{
    if (!crawl_state.game_is_hints())
        return;

    // Clear possible debug messages before messing
    // with messaging options.
    clear_messages(true);
    Options.show_more  = true;
    Options.small_more = false;
}

void init_hints()
{
    // Activate all triggers.
    // This is rather backwards: If (true) an event still needs to be
    // triggered, if (false) the relevant message was already printed.
    Hints.hints_events.init(true);

    // Used to compare which fighting means was used most often.
    // XXX: This gets reset with every save, which seems odd.
    //      On the other hand, it's precisely between saves that
    //      players are most likely to forget these.
    Hints.hints_spell_counter   = 0;
    Hints.hints_throw_counter   = 0;
    Hints.hints_melee_counter   = 0;
    Hints.hints_berserk_counter = 0;

    // Store whether explore, stash search or travelling was used.
    // XXX: Also not stored across save games.
    Hints.hints_explored = true;
    Hints.hints_stashes  = true;
    Hints.hints_travel   = true;

    // For occasional healing reminders.
    Hints.hints_last_healed = 0;

    // Did the player recently see a monster turn invisible?
    Hints.hints_seen_invisible = 0;
}

static string _print_hints_menu(hints_types type)
{
    char letter = 'a' + type;
    char desc[100];

    switch (type)
    {
    case HINT_BERSERK_CHAR:
        strcpy(desc, "(Melee oriented character with divine support)");
        break;
    case HINT_MAGIC_CHAR:
        strcpy(desc, "(Magic oriented character)");
        break;
    case HINT_RANGER_CHAR:
        strcpy(desc, "(Ranged fighter)");
        break;
    default: // no further choices
        strcpy(desc, "(erroneous character)");
        break;
    }

    return make_stringf("%c - %s %s %s",
            letter, species::name(_get_hints_species(type)).c_str(),
                    get_job_name(_get_hints_job(type)), desc);
}

static void _fill_newgame_choice_for_hints(newgame_def& choice, hints_types type)
{
    choice.species  = _get_hints_species(type);
    choice.job = _get_hints_job(type);
    // easiest choice for fighters
    choice.weapon = choice.job == JOB_HUNTER ? WPN_SHORTBOW
                                                : WPN_HAND_AXE;
}

// Hints mode selection screen and choice.
void pick_hints(newgame_def& choice)
{
    string prompt = "<white>Welcome!</white>"
        "\n\n"
        "<cyan>You can be:</cyan>";
    auto prompt_ui = make_shared<Text>(formatted_string::parse_string(prompt));

    auto vbox = make_shared<Box>(Box::VERT);
    vbox->set_cross_alignment(Widget::Align::STRETCH);
    vbox->add_child(prompt_ui);

    auto main_items = make_shared<OuterMenu>(true, 1, 3);
    main_items->set_margin_for_sdl(15, 0);
    main_items->set_margin_for_crt(1, 0);
    vbox->add_child(main_items);

    for (int i = 0; i < 3; i++)
    {
        auto label = make_shared<Text>();
        label->set_text(_print_hints_menu(static_cast<hints_types>(i)));

#ifdef USE_TILE_LOCAL
        auto hbox = make_shared<Box>(Box::HORZ);
        hbox->set_cross_alignment(Widget::Align::CENTER);
        dolls_data doll;
        newgame_def tng = choice;
        _fill_newgame_choice_for_hints(tng, static_cast<hints_types>(i));
        fill_doll_for_newgame(doll, tng);
        auto tile = make_shared<ui::PlayerDoll>(doll);
        tile->set_margin_for_sdl(0, 6, 0, 0);
        hbox->add_child(std::move(tile));
        hbox->add_child(label);
#endif

        auto btn = make_shared<MenuButton>();
#ifdef USE_TILE_LOCAL
        hbox->set_margin_for_sdl(4,8);
        btn->set_child(std::move(hbox));
#else
        btn->set_child(std::move(label));
#endif
        btn->id = i;
        btn->hotkey = 'a' + i;

        if (i == 0)
            main_items->set_initial_focus(btn.get());

        main_items->add_button(btn, 0, i);
    }

    auto sub_items = make_shared<OuterMenu>(false, 1, 2);
    vbox->add_child(sub_items);

    bool cancelled = false;
    bool done = false;
    vbox->on_activate_event([&](const ActivateEvent& event) {
        const auto button = static_pointer_cast<MenuButton>(event.target());
        int id = button->id;
        if (id == CK_ESCAPE)
            return done = cancelled = true;
        else if (id == '*')
            id = random2(HINT_TYPES_NUM);
        Hints.hints_type = id;
        _fill_newgame_choice_for_hints(choice, static_cast<hints_types>(id));
        return done = true;
    });

    main_items->linked_menus[2] = sub_items;
    sub_items->linked_menus[0] = main_items;

    {
        auto label = make_shared<Text>(formatted_string("Esc - Quit", BROWN));
        auto btn = make_shared<MenuButton>();
        btn->set_child(std::move(label));
        btn->hotkey = CK_ESCAPE;
        btn->id = CK_ESCAPE;
        sub_items->add_button(btn, 0, 0);
    }
    {
        auto label = make_shared<Text>(formatted_string("  * - Random hints mode character", BROWN));
        auto btn = make_shared<MenuButton>();
        btn->set_child(std::move(label));
        btn->hotkey = '*';
        btn->id = '*';
        sub_items->add_button(btn, 0, 1);
    }

    auto popup = make_shared<ui::Popup>(vbox);

    popup->on_keydown_event([&](const KeyEvent& ev) {
        if (ui::key_exits_popup(ev.key(), false))
            return done = cancelled = true;
        return false;
    });

    ui::run_layout(std::move(popup), done);

    if (cancelled)
    {
#ifdef USE_TILE_WEB
        tiles.send_exit_reason("cancel");
#endif
        game_ended(game_exit::abort);
    }
}

void hints_load_game()
{
    if (!crawl_state.game_is_hints())
        return;

    learned_something_new(HINT_LOAD_SAVED_GAME);

    // Reinitialise counters for explore, stash search and travelling.
    Hints.hints_explored = Hints.hints_events[HINT_AUTO_EXPLORE];
    Hints.hints_stashes  = true;
    Hints.hints_travel   = true;
}

static species_type _get_hints_species(unsigned int type)
{
    switch (type)
    {
    case HINT_BERSERK_CHAR:
        return SP_MOUNTAIN_DWARF;
    case HINT_MAGIC_CHAR:
        return SP_DEEP_ELF;
    case HINT_RANGER_CHAR:
        return SP_MINOTAUR;
    default:
        // Use something fancy for debugging.
        return SP_TENGU;
    }
}

static job_type _get_hints_job(unsigned int type)
{
    switch (type)
    {
    case HINT_BERSERK_CHAR:
        return JOB_BERSERKER;
    case HINT_MAGIC_CHAR:
        return JOB_CONJURER;
    case HINT_RANGER_CHAR:
        return JOB_HUNTER;
    default:
        // Use something fancy for debugging.
        return JOB_NECROMANCER;
    }
}

/// Substitute $cmd[CMD_FOO] with the corresponding key, and likewise
/// transform <input>foo</input>.
void hint_replace_cmds(string &text)
{
    size_t p;
    while ((p = text.find("$cmd[")) != string::npos)
    {
        size_t q = text.find("]", p + 5);
        if (q == string::npos)
        {
            text += "<lightred>ERROR: unterminated $cmd</lightred>";
            break;
        }

        string command = text.substr(p + 5, q - p - 5);
        command_type cmd = name_to_command(command);

        command = command_to_string(cmd);
        if (command == "<")
            command += "<";

        text.replace(p, q - p + 1, command);
    }

    // Brand user-input -related (tutorial) items with <w>[(text here)]</w>.
    while ((p = text.find("<input>")) != string::npos)
    {
        size_t q = text.find("</input>", p + 7);
        if (q == string::npos)
        {
            text += "<lightred>ERROR: unterminated <input></lightred>";
            break;
        }

        string input = text.substr(p + 7, q - p - 7);
        input = "<w>[" + input;
        input += "]</w>";
        text.replace(p, q - p + 8, input);
    }
}

static void _replace_static_tags(string &text)
{
    hint_replace_cmds(text);

    size_t p;
    while ((p = text.find("$item[")) != string::npos)
    {
        size_t q = text.find("]", p + 6);
        if (q == string::npos)
        {
            text += "<lightred>ERROR: unterminated $item</lightred>";
            break;
        }

        string item = text.substr(p + 6, q - p - 6);
        int type;
        for (type = OBJ_WEAPONS; type < NUM_OBJECT_CLASSES; ++type)
            if (item == item_class_name(type, true))
                break;

        item_def dummy;
        dummy.base_type = static_cast<object_class_type>(type);
        dummy.sub_type = 0;
        if (item == "amulet") // yay shared item classes
            dummy.base_type = OBJ_JEWELLERY, dummy.sub_type = AMU_FAITH;
        item = stringize_glyph(get_item_symbol(show_type(dummy).item));

        if (item == "<")
            item += "<";

        text.replace(p, q - p + 1, item);
    }
}

// Prints the hints mode welcome screen.
void hints_starting_screen()
{
    string text = getHintString("welcome");
    _replace_static_tags(text);
    trim_string(text);

    auto prompt_ui = make_shared<Text>(formatted_string::parse_string(text));
    prompt_ui->set_wrap_text(true);
#ifdef USE_TILE_LOCAL
    prompt_ui->max_size().width = 800;
#else
    prompt_ui->max_size().width = 80;
#endif

    bool done = false;
    auto popup = make_shared<ui::Popup>(prompt_ui);
    popup->on_keydown_event([&](const KeyEvent&) { return done = true; });

    mouse_control mc(MOUSE_MODE_MORE);
    ui::run_layout(std::move(popup), done);
}

// Called each turn from _input. Better name welcome.
void hints_new_turn()
{
    if (!crawl_state.game_is_hints())
        return;

    Hints.hints_just_triggered = false;

    if (you.attribute[ATTR_HELD])
    {
        learned_something_new(HINT_CAUGHT_IN_NET);
        return;
    }

    if (i_feel_safe() && !player_in_branch(BRANCH_ABYSS))
    {
        // We don't want those "Whew, it's safe to rest now" messages
        // if you were just cast into the Abyss. Right?

        if (2 * you.hp < you.hp_max
            || 2 * you.magic_points < you.max_magic_points)
        {
            _hints_healing_reminder();
            return;
        }

        if (you.running
                 || you.hp != you.hp_max
                 || you.magic_points != you.max_magic_points)
        {
            return;
        }

        if (Hints.hints_events[HINT_SHIFT_RUN] && you.num_turns >= 200)
            learned_something_new(HINT_SHIFT_RUN);
        else if (Hints.hints_events[HINT_MAP_VIEW] && you.num_turns >= 500)
            learned_something_new(HINT_MAP_VIEW);
        else if (Hints.hints_events[HINT_AUTO_EXPLORE] && you.num_turns >= 700)
            learned_something_new(HINT_AUTO_EXPLORE);

        return;
    }

    if (poison_is_lethal())
    {
        if (Hints.hints_events[HINT_NEED_POISON_HEALING])
            learned_something_new(HINT_NEED_POISON_HEALING);
    }
    else if (2*you.hp < you.hp_max)
        learned_something_new(HINT_RUN_AWAY);

    if (Hints.hints_type == HINT_MAGIC_CHAR && you.magic_points < 1)
        learned_something_new(HINT_RETREAT_CASTER);
}

/**
 * Look up and return a hint message from the database.
 * @param arg1 A string that can be inserted into the hint message.
 * @param arg2 Another string that can be inserted into the hint message.
 */
static string _get_hint(string key, const string& arg1 = "", const string& arg2 = "")
{
    string text = getHintString(key);
    if (text.empty())
        mprf(MSGCH_ERROR, "Error, no hint for '%s'.", key.c_str());

    _replace_static_tags(text);
    text = untag_tiles_console(text);
    text = replace_all(text, "$1", arg1);
    text = replace_all(text, "$2", arg2);

    // split into paragraphs by "\n\n"
    text = replace_all(text, "\n\n", "\n");

    return text;
}

/**
 * Look up and display a hint message from the database. Is usable from dlua,
 * so wizard mode in-game Lua interpreter can be used to test the messages.
 * @param arg1 A string that can be inserted into the hint message.
 * @param arg2 Another string that can be inserted into the hint message.
 */
void print_hint(string key, const string& arg1, const string& arg2)
{
    string text = _get_hint(key, arg1, arg2);

    // "\n" preserves indented parts and paragraphs
    for (const string &chunk : split_string("\n", text))
        mprf(MSGCH_TUTORIAL, "%s", chunk.c_str());

    stop_running();
}

// Once a hints mode character dies, offer some last playing hints.
void hints_death_screen()
{
    string text;

    print_hint("death");
    more();

    if (Hints.hints_type == HINT_MAGIC_CHAR
        && Hints.hints_spell_counter < Hints.hints_melee_counter)
    {
        print_hint("death conjurer melee");
    }
    else if (you_worship(GOD_TROG) && Hints.hints_berserk_counter <= 3
             && !you.berserk() && !you.duration[DUR_BERSERK_COOLDOWN])
    {
        print_hint("death berserker unberserked");
    }
    else if (Hints.hints_type == HINT_RANGER_CHAR
             && 2*Hints.hints_throw_counter < Hints.hints_melee_counter)
    {
        print_hint("death ranger melee");
    }
    else
    {
        int hint = random2(6);

        bool skip_first_hint = false;
        // If a character has been unusually busy with projectiles and spells
        // give some other hint rather than the first one.
        if (hint == 0 && Hints.hints_throw_counter + Hints.hints_spell_counter
                          >= Hints.hints_melee_counter)
        {
            hint = random2(5) + 1;
            skip_first_hint = true;
        }
        // FIXME: The hints below could be somewhat less random, so that e.g.
        // the message for fighting several monsters in a corridor only happens
        // if there's more than one monster around and you're not in a corridor,
        // or the one about using consumable objects only if you actually have
        // any (useful or unidentified) scrolls/wands/potions.

        if (hint == 5)
        {
            vector<monster* > visible =
                get_nearby_monsters(false, true, true, false);

            if (visible.size() < 2)
            {
                if (skip_first_hint)
                    hint = random2(4) + 1;
                else
                    hint = random2(5);
            }
        }

        print_hint(make_stringf("death random %d", hint));
    }
    mprf(MSGCH_TUTORIAL, "%s", untag_tiles_console(text).c_str());
    more();

    mprf(MSGCH_TUTORIAL, "See you next game!");

    Hints.hints_events.init(false);
}

// If a character survives until XL 7, the hints mode is declared finished
// and they get a more advanced playing hint, depending on what they might
// know by now.
void hints_finished()
{
    crawl_state.type = GAME_TYPE_NORMAL;

    print_hint("finished");
    more();

    if (Hints.hints_explored)
        print_hint("finished explored");
    else if (Hints.hints_travel)
        print_hint("finished travel");
    else if (Hints.hints_stashes)
        print_hint("finished stashes");
    else
        print_hint(make_stringf("finished random %d", random2(4)));
    more();

    Hints.hints_events.init(false);

    // Remove the hints mode file.
    you.save->delete_chunk("tut");
}

static bool _advise_use_healing_potion()
{
    for (auto &obj : you.inv)
    {
        if (!obj.defined())
            continue;

        if (obj.base_type != OBJ_POTIONS)
            continue;

        if (!obj.is_identified())
            continue;

        if (obj.sub_type == POT_CURING
            || obj.sub_type == POT_HEAL_WOUNDS)
        {
            return true;
        }
    }

    return false;
}

void hints_healing_check()
{
    if (2*you.hp <= you.hp_max
        && _advise_use_healing_potion())
    {
        learned_something_new(HINT_HEALING_POTIONS);
    }
}

// Occasionally remind injured characters of resting.
static void _hints_healing_reminder()
{
    if (!crawl_state.game_is_hints())
        return;

    if (Hints.hints_seen_invisible > 0
        && you.num_turns - Hints.hints_seen_invisible <= 20)
    {
        // If we recently encountered an invisible monster, we need a
        // special message.
        learned_something_new(HINT_NEED_HEALING_INVIS);
        // If that one was already displayed, don't print a reminder.
    }
    else
    {
        if (Hints.hints_events[HINT_NEED_HEALING])
            learned_something_new(HINT_NEED_HEALING);
        else if (you.num_turns - Hints.hints_last_healed >= 50
                 && !you.duration[DUR_POISONING])
        {
            if (Hints.hints_just_triggered)
                return;

            Hints.hints_just_triggered = true;

            string text;
            text =  "Remember to rest between fights and to enter unexplored "
                    "terrain with full health and magic. Ideally you "
                    "should retreat into areas you've already explored and "
                    "cleared of monsters; resting on the edge of the explored "
                    "terrain increases the chances of your rest being "
                    "interrupted by wandering monsters. To rest, press "
                    "<w>5</w> or <w>Shift-numpad 5</w>"
                    "<tiles>, or <w>click the rest button</w></tiles>"
                    ".";

            if (you.hp < you.hp_max && you_worship(GOD_TROG)
                && you.can_go_berserk())
            {
                text += "\nAlso, berserking might help you not to lose so much "
                        "health in the first place. To use your abilities "
                        "press <w>a</w>.";
            }
            mprf(MSGCH_TUTORIAL, "%s", untag_tiles_console(text).c_str());

            if (is_resting())
                stop_running();
        }
        Hints.hints_last_healed = you.num_turns;
    }
}

// Give a message if you see, pick up or inspect an item type for the
// first time.
void taken_new_item(object_class_type item_type)
{
    switch (item_type)
    {
    case OBJ_WANDS:
        learned_something_new(HINT_SEEN_WAND);
        break;
    case OBJ_SCROLLS:
        learned_something_new(HINT_SEEN_SCROLL);
        break;
    case OBJ_JEWELLERY:
        learned_something_new(HINT_SEEN_JEWELLERY);
        break;
    case OBJ_POTIONS:
        learned_something_new(HINT_SEEN_POTION);
        break;
    case OBJ_BOOKS:
        learned_something_new(HINT_SEEN_SPBOOK);
        break;
    case OBJ_WEAPONS:
        learned_something_new(HINT_SEEN_WEAPON);
        break;
    case OBJ_ARMOUR:
        learned_something_new(HINT_SEEN_ARMOUR);
        break;
    case OBJ_MISSILES:
        learned_something_new(HINT_SEEN_MISSILES);
        break;
    case OBJ_MISCELLANY:
        learned_something_new(HINT_SEEN_MISC);
        break;
    case OBJ_STAVES:
        learned_something_new(HINT_SEEN_STAFF);
        break;
    case OBJ_GOLD:
        learned_something_new(HINT_SEEN_GOLD);
        break;
    default: // nothing to be done
        return;
    }
}

// Give a special message if you gain a skill you didn't have before.
void hints_gained_new_skill(skill_type skill)
{
    if (!crawl_state.game_is_hints())
        return;

    learned_something_new(HINT_SKILL_RAISE);

    switch (skill)
    {
    // Special cases first.
    case SK_FIGHTING:
    case SK_ARMOUR:
    case SK_STEALTH:
    case SK_UNARMED_COMBAT:
    case SK_INVOCATIONS:
    case SK_EVOCATIONS:
    case SK_DODGING:
    case SK_SHIELDS:
    case SK_SHAPESHIFTING:
    case SK_THROWING:
    case SK_SPELLCASTING:
    {
        mprf(MSGCH_TUTORIAL, "%s", get_skill_description(skill).c_str());
        stop_running();
        break;
    }
    // Only one message for all magic skills (except Spellcasting).
    case SK_CONJURATIONS:
    case SK_HEXES:
    case SK_SUMMONINGS:
    case SK_NECROMANCY:
    case SK_FORGECRAFT:
    case SK_TRANSLOCATIONS:
    case SK_FIRE_MAGIC:
    case SK_ICE_MAGIC:
    case SK_AIR_MAGIC:
    case SK_EARTH_MAGIC:
    case SK_ALCHEMY:
        learned_something_new(HINT_GAINED_MAGICAL_SKILL);
        break;

    // Melee skills.
    case SK_SHORT_BLADES:
    case SK_LONG_BLADES:
    case SK_AXES:
    case SK_MACES_FLAILS:
    case SK_POLEARMS:
    case SK_STAVES:
        learned_something_new(HINT_GAINED_MELEE_SKILL);
        break;

    // Ranged skills.
    case SK_RANGED_WEAPONS:
        learned_something_new(HINT_GAINED_RANGED_SKILL);
        break;

    default:
        break;
    }
}

static bool _mons_is_highlighted(const monster* mons)
{
    return mons->friendly()
               && Options.friend_highlight != CHATTR_NORMAL
           || mons_looks_stabbable(*mons)
               && Options.stab_highlight != CHATTR_NORMAL
           || mons_looks_distracted(*mons)
               && Options.may_stab_highlight != CHATTR_NORMAL;
}

static bool _advise_use_wand()
{
    for (auto &obj : you.inv)
        if (obj.defined() && obj.base_type == OBJ_WANDS && obj.sub_type != WAND_DIGGING)
            return true;
    return false;
}

void hints_monster_seen(const monster& mon)
{
    if (mon.is_firewood())
    {
        if (Hints.hints_events[HINT_SEEN_ZERO_EXP_MON])
        {
            if (Hints.hints_just_triggered)
                return;

            learned_something_new(HINT_SEEN_ZERO_EXP_MON, mon.pos());
            return;
        }

        // Don't do HINT_SEEN_MONSTER for zero exp monsters.
        if (Hints.hints_events[HINT_SEEN_MONSTER])
            return;
    }

    if (!Hints.hints_events[HINT_SEEN_MONSTER])
    {
        if (Hints.hints_just_triggered)
            return;

        if (_mons_is_highlighted(&mon))
            learned_something_new(HINT_MONSTER_HIGHLIGHT, mon.pos());
        if (mon.friendly())
            learned_something_new(HINT_MONSTER_FRIENDLY, mon.pos());

        if (you_worship(GOD_TROG) && you.can_go_berserk()
            && one_chance_in(4))
        {
            learned_something_new(HINT_CAN_BERSERK);
        }
        return;
    }

    stop_running();

    Hints.hints_events[HINT_SEEN_MONSTER] = false;
    Hints.hints_just_triggered = true;

    monster_info mi(&mon);
#ifdef USE_TILE
    // need to highlight monster
    const coord_def gc = mon.pos();
    tiles.place_cursor(CURSOR_TUTORIAL, gc);
    tiles.add_text_tag(TAG_TUTORIAL, mi);
#endif

    string text = "That ";

    if (is_tiles())
    {
        text +=
            string("monster is a ") +
            mon.name(DESC_PLAIN).c_str() +
            ". You can learn about any monster by hovering your mouse over it,"
            " and read its description by <w>right-clicking</w> on it.";
    }
    else
    {
        text +=
            glyph_to_tagstr(get_mons_glyph(mi)) +
            " is a monster, usually depicted by a letter. Some typical "
            "early monsters look like <brown>r</brown>, <green>l</green>, "
            "<brown>K</brown> or <lightgrey>g</lightgrey>. ";
        if (crawl_view.mlistsz.y > 0)
        {
            text += "Your console settings allowing, you'll always see a "
                    "list of monsters somewhere on the screen.\n";
        }
        text += "You can gain information about it by pressing <w>x</w> and "
                "moving the cursor over the monster, and read the monster "
                "description by then pressing <w>v</w>. ";
    }

    text += "\nTo attack this monster with your wielded weapon, just move "
            "into it. ";
    if (is_tiles())
    {
        text +=
            "Note that as long as there's a non-friendly monster in view you "
            "won't be able to automatically move to distant squares, to avoid "
            "death by misclicking.";
    }

    mprf(MSGCH_TUTORIAL, "%s", text.c_str());

    if (Hints.hints_type == HINT_RANGER_CHAR)
    {
        text =  "However, as a hunter you will want to deal with it using your "
                "bow. If you have a look at your shortbow from your "
                "<w>i</w>nventory, you'll find an explanation of how to do "
                "this. ";

        if (!you.weapon()
            || you.weapon()->base_type != OBJ_WEAPONS
            || you.weapon()->sub_type != WPN_SHORTBOW)
        {
            text += "First <w>w</w>ield it, then follow the instructions."
                "<tiles>\nAs a short-cut you can also <w>right-click</w> on your "
                "shortbow to read its description, and <w>left-click</w> to wield "
                "it.</tiles>";
        }
        else
        {
            text += "<tiles>Clicking with your <w>right mouse button</w> on your "
                    "shortbow will also let you read its description.</tiles>";
        }

        mprf(MSGCH_TUTORIAL, "%s", untag_tiles_console(text).c_str());

    }
    else if (Hints.hints_type == HINT_MAGIC_CHAR)
    {
        text =  "However, as a conjurer you will want to deal with it using "
                "magic. If you look at the help entry for the "
                "<w>M</w>emorisation screen you'll find an explanation of how "
                "to do this.";
        mprf(MSGCH_TUTORIAL, "%s", untag_tiles_console(text).c_str());

    }
}

void hints_first_item(const item_def &item)
{
    // Happens if monster is standing on dropped corpse or item.
    if (monster_at(item.pos))
        return;

    if (!Hints.hints_events[HINT_SEEN_FIRST_OBJECT]
#ifdef USE_TILE
        || item.base_type == OBJ_CORPSES
        // Don't show hints for corpses - they're purely decorative.
        // In console, it's less obvious what's happening, so hint anyway.
#endif
        || Hints.hints_just_triggered)
    {
        return;
    }

    stop_running();

    Hints.hints_events[HINT_SEEN_FIRST_OBJECT] = false;
    Hints.hints_just_triggered = true;

#ifdef USE_TILE
    const coord_def gc = item.pos;
    tiles.place_cursor(CURSOR_TUTORIAL, gc);
    tiles.add_text_tag(TAG_TUTORIAL, item.name(DESC_A), gc);
#endif

    print_hint("HINT_SEEN_FIRST_OBJECT",
               glyph_to_tagstr(get_item_glyph(item)));
}

static string _describe_portal(const coord_def &gc)
{
    const dungeon_feature_type feat = env.grid(gc);
    string text;
    string glyph_tagstr = glyph_to_tagstr(get_cell_glyph(gc));

    // For the sake of completeness, though it's very unlikely that a
    // player will find a bazaar entrance before reaching XL 7.
    if (feat == DNGN_ENTER_BAZAAR)
        text = _get_hint("HINT_SEEN_BAZAAR_PORTAL", glyph_tagstr);
    // Sewers can appear on D:3-6, ossuaries D:4-8.
    else
        text = _get_hint("HINT_SEEN_PORTAL", glyph_tagstr);

    text += _get_hint("HINT_SEEN_PORTAL_2",
                      stringize_glyph(get_feat_symbol(DNGN_EXIT_SEWER)));
    return text;
}

#define DELAY_EVENT \
{ \
    Hints.hints_events[seen_what] = true; \
    return; \
}

// Really rare or important events should get a comment even if
// learned_something_new() was already triggered this turn.
// NOTE: If put off, the SEEN_<feature> variant will be triggered the
//       next turn, so they may be rare but aren't urgent.
static bool _rare_hints_event(hints_event_type event)
{
    switch (event)
    {
    case HINT_SEEN_RUNED_DOOR: // The runed door could be opened in one turn.
    case HINT_KILLED_MONSTER:
    case HINT_NEW_LEVEL:
    case HINT_YOU_ENCHANTED:
    case HINT_YOU_POISON:
    case HINT_GLOWING:
    case HINT_CAUGHT_IN_NET:
    case HINT_YOU_SILENCE:
    case HINT_NEED_POISON_HEALING:
    case HINT_ON_FIRE:
    case HINT_INVISIBLE_DANGER:
    case HINT_NEED_HEALING_INVIS:
    case HINT_ABYSS:
    case HINT_RUN_AWAY:
    case HINT_RETREAT_CASTER:
    case HINT_YOU_MUTATED:
    case HINT_NEW_ABILITY_GOD:
    case HINT_NEW_ABILITY_ITEM:
    case HINT_CONVERT:
    case HINT_GOD_DISPLEASED:
    case HINT_EXCOMMUNICATE:
    case HINT_GAINED_MAGICAL_SKILL:
    case HINT_GAINED_MELEE_SKILL:
    case HINT_GAINED_RANGED_SKILL:
    case HINT_CHOOSE_STAT:
    case HINT_AUTO_EXCLUSION:
    case HINT_MALEVOLENCE:
    case HINT_OPPORTUNITY_ATTACK:
        return true;
    default:
        return false;
    }
}

// Allow for a few specific hint mode messages.
static bool _tutorial_interesting(hints_event_type event)
{
    switch (event)
    {
    case HINT_AUTOPICKUP_THROWN:
    case HINT_TARGET_NO_FOE:
    case HINT_YOU_POISON:
    case HINT_NEW_ABILITY_ITEM:
    case HINT_ITEM_RESISTANCES:
    case HINT_HEALING_POTIONS:
    case HINT_GAINED_SPELLCASTING:
    case HINT_FUMBLING_SHALLOW_WATER:
    case HINT_SPELL_MISCAST:
    case HINT_CLOUD_WARNING:
    case HINT_SKILL_RAISE:
        return true;
    default:
        return false;
    }
}

// A few special tutorial explanations require triggers.
// Initialize the corresponding events, so they can get displayed.
void tutorial_init_hints()
{
    Hints.hints_events.init(false);
    for (int i = 0; i < HINT_EVENTS_NUM; ++i)
        if (_tutorial_interesting((hints_event_type) i))
            Hints.hints_events[i] = true;
}

// Here most of the hints mode messages for various triggers are handled.
void learned_something_new(hints_event_type seen_what, coord_def gc)
{
    if (!crawl_state.game_is_hints_tutorial())
        return;

    // Already learned about that.
    if (!Hints.hints_events[seen_what])
        return;

    // Don't trigger twice in the same turn.
    // Not required in the tutorial.
    if (crawl_state.game_is_hints() && Hints.hints_just_triggered
        && !_rare_hints_event(seen_what))
    {
        return;
    }

    ostringstream text;
    vector<command_type> cmd;

    Hints.hints_just_triggered    = true;
    Hints.hints_events[seen_what] = false;

    switch (seen_what)
    {
    case HINT_SEEN_POTION:
        print_hint("HINT_SEEN_POTION");
        break;

    case HINT_SEEN_SCROLL:
        print_hint("HINT_SEEN_SCROLL");
        break;

    case HINT_SEEN_WAND:
        print_hint("HINT_SEEN_WAND");
        break;

    case HINT_SEEN_SPBOOK:
        print_hint("HINT_SEEN_SPBOOK");
        break;

    case HINT_SEEN_WEAPON:
        print_hint("HINT_SEEN_WEAPON");
        break;

    case HINT_SEEN_MISSILES:
        print_hint("HINT_SEEN_MISSILES");
        break;

    case HINT_SEEN_ARMOUR:
        print_hint("HINT_SEEN_ARMOUR");
        break;

    case HINT_SEEN_JEWELLERY:
        print_hint("HINT_SEEN_JEWELLERY");
        break;

    case HINT_SEEN_MISC:
        print_hint("HINT_SEEN_MISC");
        break;

    case HINT_SEEN_STAFF:
        print_hint("HINT_SEEN_STAFF");
        break;

    case HINT_SEEN_GOLD:
        print_hint("HINT_SEEN_GOLD");
        break;

    case HINT_SEEN_STAIRS:
        // Don't give this information during the first turn, to give
        // the player time to have a look around.
        if (you.num_turns < 1)
            DELAY_EVENT;

#ifndef USE_TILE
        // Is a monster blocking the view?
        if (monster_at(gc))
            DELAY_EVENT;
#else
        tiles.place_cursor(CURSOR_TUTORIAL, gc);
        tiles.add_text_tag(TAG_TUTORIAL, "Stairs", gc);
#endif
        print_hint("HINT_SEEN_STAIRS", glyph_to_tagstr(get_cell_glyph(gc)));
        break;

    case HINT_SEEN_ESCAPE_HATCH:
        if (you.num_turns < 1)
            DELAY_EVENT;

        // monsters standing on stairs
        if (monster_at(gc))
            DELAY_EVENT;

#ifdef USE_TILE
        tiles.place_cursor(CURSOR_TUTORIAL, gc);
        tiles.add_text_tag(TAG_TUTORIAL, "Escape hatch", gc);
#endif
        print_hint("HINT_SEEN_ESCAPE_HATCH",
                   glyph_to_tagstr(get_cell_glyph(gc)));
        break;

    case HINT_SEEN_BRANCH:
#ifndef USE_TILE
        // Is a monster blocking the view?
        if (monster_at(gc))
            DELAY_EVENT;
#else
        tiles.place_cursor(CURSOR_TUTORIAL, gc);
        tiles.add_text_tag(TAG_TUTORIAL, "Branch stairs", gc);
#endif
        print_hint("HINT_SEEN_BRANCH", glyph_to_tagstr(get_cell_glyph(gc)));
        break;

    case HINT_SEEN_PORTAL:
        // Delay in the unlikely event that a player still in hints mode
        // creates a portal with a Trowel card, since a portal vault
        // entry's description doesn't seem to get set properly until
        // after the vault is done being placed.
        if (you.pos() == gc)
            DELAY_EVENT;

#ifndef USE_TILE
        // Is a monster blocking the view?
        if (monster_at(gc))
            DELAY_EVENT;
#else
        tiles.place_cursor(CURSOR_TUTORIAL, gc);
        tiles.add_text_tag(TAG_TUTORIAL, "Portal", gc);
#endif
        text << _describe_portal(gc);
        break;

    case HINT_STAIR_HIGHLIGHT:
        // Monster or player standing on stairs.
        if (actor_at(gc))
            DELAY_EVENT;

        print_hint("HINT_STAIR_BRAND");
        break;

    case HINT_HEAP_HIGHLIGHT:
        // Monster or player standing on heap.
        if (actor_at(gc))
            DELAY_EVENT;

        print_hint("HINT_HEAP_BRAND");
        break;

    case HINT_TRAP_HIGHLIGHT:
#ifdef USE_TILE
        // Tiles show both the trap and the item heap.
        return;
#else
        // Monster or player standing on trap.
        if (actor_at(gc))
            DELAY_EVENT;

        print_hint("HINT_TRAP_BRAND");
#endif
        break;

    case HINT_SEEN_TRAP:
        print_hint("HINT_SEEN_TRAP");
        break;

    case HINT_SEEN_ALTAR:
#ifndef USE_TILE
        // Is a monster blocking the view?
        if (monster_at(gc))
            DELAY_EVENT;
#else
        {
            tiles.place_cursor(CURSOR_TUTORIAL, gc);
            string altar = "An altar to ";
            altar += god_name(feat_altar_god(env.grid(gc)));
            tiles.add_text_tag(TAG_TUTORIAL, altar, gc);
        }
#endif
        print_hint("HINT_SEEN_ALTAR", glyph_to_tagstr(get_cell_glyph(gc)));
        break;

    case HINT_SEEN_SHOP:
#ifdef USE_TILE
        tiles.place_cursor(CURSOR_TUTORIAL, gc);
        tiles.add_text_tag(TAG_TUTORIAL, shop_name(*shop_at(gc)), gc);
#else
        // Is a monster blocking the view?
        if (monster_at(gc))
            DELAY_EVENT;
#endif
        print_hint("HINT_SEEN_SHOP", glyph_to_tagstr(get_cell_glyph(gc)));
        break;

    case HINT_SEEN_DOOR:
        if (you.num_turns < 1)
            DELAY_EVENT;

#ifdef USE_TILE
        tiles.place_cursor(CURSOR_TUTORIAL, gc);
        tiles.add_text_tag(TAG_TUTORIAL, "Closed door", gc);
#endif
        print_hint("HINT_SEEN_DOOR", glyph_to_tagstr(get_cell_glyph(gc)));
        break;

    case HINT_SEEN_RUNED_DOOR:
#ifdef USE_TILE
        tiles.place_cursor(CURSOR_TUTORIAL, gc);
        tiles.add_text_tag(TAG_TUTORIAL, "Runed door", gc);
#endif
        print_hint("HINT_SEEN_RUNED_DOOR", glyph_to_tagstr(get_cell_glyph(gc)));
        break;

    case HINT_KILLED_MONSTER:
        print_hint("HINT_KILLED_MONSTER");
        break;

    case HINT_NEW_LEVEL:
        print_hint("HINT_NEW_LEVEL");
        break;

    case HINT_SKILL_RAISE:
        print_hint("HINT_SKILL_RAISE");
        break;

    case HINT_GAINED_MAGICAL_SKILL:
        print_hint("HINT_GAINED_MAGICAL_SKILL");
        break;

    case HINT_GAINED_MELEE_SKILL:
        print_hint("HINT_GAINED_MELEE_SKILL");
        break;

    case HINT_GAINED_RANGED_SKILL:
        print_hint("HINT_GAINED_RANGED_SKILL");
        break;

    case HINT_CHOOSE_STAT:
        print_hint("HINT_CHOOSE_STAT");
        break;

    case HINT_MALEVOLENCE:
        print_hint("HINT_MALEVOLENCE");
        break;

    case HINT_OPPORTUNITY_ATTACK:
        print_hint("HINT_OPPORTUNITY_ATTACK");
        break;

    case HINT_YOU_ENCHANTED:
        print_hint("HINT_YOU_ENCHANTED");
        break;

    case HINT_YOU_POISON:
        if (crawl_state.game_is_hints())
        {
            // Hack: reset hints_just_triggered, to force recursive calling of
            // learned_something_new(). Don't do this for the tutorial!
            Hints.hints_just_triggered = false;
            learned_something_new(HINT_YOU_ENCHANTED);
            Hints.hints_just_triggered = true;
        }
        print_hint("HINT_YOU_POISON");
        break;

    case HINT_ON_FIRE:
        print_hint("HINT_ON_FIRE");
        break;

    case HINT_MULTI_PICKUP:
        print_hint("HINT_MULTI_PICKUP");
        break;

    case HINT_FULL_INVENTORY:
        print_hint("HINT_FULL_INVENTORY");
        break;

    case HINT_SHIFT_RUN:
        print_hint("HINT_SHIFT_RUN");
        break;

    case HINT_MAP_VIEW:
        print_hint("HINT_MAP_VIEW");
        break;

    case HINT_AUTO_EXPLORE:
        if (!Hints.hints_explored)
            return;

        print_hint("HINT_AUTO_EXPLORE");
        Hints.hints_explored = false;
        break;

    case HINT_DONE_EXPLORE:
        // XXX: You'll only get this message if you're using auto exploration.
        print_hint("HINT_DONE_EXPLORE");
        break;

    case HINT_AUTO_EXCLUSION:
        // In the highly unlikely case the player encounters a
        // hostile statue or oklob plant during the hints mode...
        if (Hints.hints_explored)
        {
            // Hack: Reset hints_just_triggered, to force recursive calling of
            //       learned_something_new().
            Hints.hints_just_triggered = false;
            learned_something_new(HINT_AUTO_EXPLORE);
            Hints.hints_just_triggered = true;
        }
        print_hint("HINT_AUTO_EXCLUSION");
        break;

    case HINT_HEALING_POTIONS:
        print_hint("HINT_HEALING_POTIONS");
        break;

    case HINT_NEED_HEALING:
        print_hint("HINT_NEED_HEALING");
        break;

    case HINT_NEED_POISON_HEALING:
        print_hint("HINT_NEED_POISON_HEALING");
        break;

    case HINT_INVISIBLE_DANGER:
        print_hint("HINT_INVISIBLE_DANGER");

        // To prevent this text being immediately followed by the next one...
        Hints.hints_last_healed = you.num_turns - 30;
        break;

            // XXX: replace this with an explanation of autopickup toggles?
    case HINT_NEED_HEALING_INVIS:
        print_hint("HINT_NEED_HEALING_INVIS");
        Hints.hints_last_healed = you.num_turns;
        break;

    case HINT_CAN_BERSERK:
        // Don't print this information if the player already knows it.
        if (Hints.hints_berserk_counter)
            return;

        print_hint("HINT_CAN_BERSERK");
        break;

    case HINT_POSTBERSERK:
        print_hint("HINT_POSTBERSERK");
        break;

    case HINT_RUN_AWAY:
        print_hint("HINT_RUN_AWAY_1");

        if (you_worship(GOD_TROG) && you.can_go_berserk())
            print_hint("HINT_LAST_MINUTE_BERSERK");

        print_hint("HINT_RUN_AWAY_2");

        break;

    case HINT_RETREAT_CASTER:
        print_hint("HINT_RETREAT_CASTER");

        if (_advise_use_wand())
            print_hint("HINT_ADVISE_USE_WAND");
        break;

    case HINT_YOU_MUTATED:
        print_hint("HINT_YOU_MUTATED");
        break;

    case HINT_NEW_ABILITY_GOD:
        if (get_god_abilities(true, false).size())
            print_hint("HINT_NEW_ABILITY_GOD_ACTIVE");
        else
            print_hint("HINT_NEW_ABILITY_GOD_PASSIVE");
        break;

    case HINT_NEW_ABILITY_ITEM:
        print_hint("HINT_NEW_ABILITY_ITEM");
        break;

    case HINT_ITEM_RESISTANCES:
        // Specialcase flight because it's a guaranteed trigger in the
        // tutorial.
        if (you.equip_flight())
            print_hint("HINT_ITEM_FLIGHT");
        else
            print_hint("HINT_ITEM_RESISTANCES");
        break;

            // TODO: rethink this v
    case HINT_CONVERT:
        switch (you.religion)
        {
            // gods without traditional piety
        case GOD_ASHENZARI:
            return print_hint("HINT_CONVERT Ashenzari");
        case GOD_GOZAG:
            return print_hint("HINT_CONVERT Gozag");
        case GOD_RU:
            return print_hint("HINT_CONVERT Ru");
        case GOD_USKAYAW:
            return print_hint("HINT_CONVERT Uskayaw");
        case GOD_XOM:
            return print_hint("HINT_CONVERT Xom");
        case GOD_YREDELEMNUL:
            return print_hint("HINT_CONVERT Yredelemnul");
        default:
            print_hint("HINT_CONVERT");

        }

        break;

    case HINT_GOD_DISPLEASED:
        print_hint("HINT_GOD_DISPLEASED", god_name(you.religion));
        break;

    case HINT_EXCOMMUNICATE:
    {
        const god_type new_god   = (god_type) gc.x;
        const int      old_piety = gc.y;

        god_type old_god = GOD_NO_GOD;
        for (god_iterator it; it; ++it)
            if (you.worshipped[*it] > 0)
            {
                old_god = *it;
                break;
            }

        const string old_god_name  = god_name(old_god);
        const string new_god_name  = god_name(new_god);

        if (new_god == GOD_NO_GOD)
        {
            if (old_piety < 1)
            {
                text << "Uh-oh, " << old_god_name << " just excommunicated you "
                        "for running out of piety (your divine favour went "
                        "to nothing). Maybe you repeatedly violated the "
                        "religious rules, or maybe you failed to please your "
                        "deity often enough, or some combination of the two. "
                        "If you can find an altar dedicated to "
                     << old_god_name;
            }
            else
            {
                text << "Should you decide that abandoning " << old_god_name
                     << "wasn't such a smart move after all, and you'd like to "
                        "return to your old faith, you'll have to find an "
                        "altar dedicated to " << old_god_name << " where";
            }
            text << " you can re-convert, and all will be well.";

            if (god_hates_your_god(old_god, new_god))
            {
                text << "Otherwise, you'll have to weather this god's "
                        "displeasure until their divine wrath is spent.";
            }

            break;
        }

        bool angry = false;
        if (is_good_god(old_god))
        {
            if (is_good_god(new_god))
            {
                text << "Fortunately, it seems that " << old_god_name <<
                        " didn't mind your converting to " << new_god_name
                     << ". ";

                if (old_piety > piety_breakpoint(0))
                    text << "You even kept some of your piety! ";

                text << "Note that this kind of alliance only exists "
                        "between the three good gods, so don't expect this "
                        "to be the norm.";
            }
            else if (!god_hates_your_god(old_god))
            {
                text << "Fortunately, it seems that " << old_god_name <<
                        " didn't mind your converting to " << new_god_name
                     << ". That's because " << old_god_name << " is one of "
                        "the good gods who generally are rather forgiving "
                        "about change of faith - unless you switch over to "
                        "the path of evil, in which case their retribution "
                        "can be nasty indeed!";
            }
            else
            {
                text << "Looks like " << old_god_name << " didn't "
                        "appreciate your converting to " << new_god_name
                     << "! But really, changing from one of the good gods "
                        "to an evil one, what did you expect!? For any god "
                        "not on the opposing side of the faith, "
                     << old_god_name << " would have been much more "
                        "forgiving. ";

                angry = true;
            }
        }
        else if (god_hates_your_god(old_god))
        {
            text << "Looks like " << old_god_name << " didn't appreciate "
                    "your converting to " << new_god_name << "! (Actually, "
                    "only the three good gods will usually be forgiving "
                    "about this kind of faithlessness.) ";

            angry = true;
        }

        if (angry)
        {
            text << "Unfortunately, while converting back would appease "
                 << old_god_name << ", it would annoy " << new_god_name
                 << ", so you're stuck with having to suffer the wrath of "
                    "one god or another.";
        }

        break;
    }

    case HINT_WIELD_WEAPON:
        print_hint("HINT_WIELD_WEAPON");
        break;

    case HINT_MONSTER_HIGHLIGHT:
#ifdef USE_TILE
        tiles.place_cursor(CURSOR_TUTORIAL, gc);
        if (const monster* m = monster_at(gc))
            tiles.add_text_tag(TAG_TUTORIAL, m->name(DESC_A), gc);
#endif
        print_hint("HINT_MONSTER_BRAND");
        break;

    case HINT_MONSTER_FRIENDLY:
    {
        const monster* m = monster_at(gc);

        if (!m)
            DELAY_EVENT;

#ifdef USE_TILE
        tiles.place_cursor(CURSOR_TUTORIAL, gc);
        tiles.add_text_tag(TAG_TUTORIAL, m->name(DESC_A), gc);
#endif
        print_hint("HINT_MONSTER_FRIENDLY");

        if (!mons_att_wont_attack(m->attitude))
            print_hint("HINT_TEMPORARILY_FRIENDLY");

        break;
    }

    case HINT_MONSTER_SHOUT:
    {
        const monster* m = monster_at(gc);

        if (!m)
            DELAY_EVENT;

        // "Shouts" from zero experience monsters are boring, ignore
        // them.
        if (!mons_is_threatening(*m))
        {
            Hints.hints_events[HINT_MONSTER_SHOUT] = true;
            return;
        }

        const bool vis = you.can_see(*m);

#ifdef USE_TILE
        if (vis)
        {
            tiles.place_cursor(CURSOR_TUTORIAL, gc);
            tiles.add_text_tag(TAG_TUTORIAL, m->name(DESC_A), gc);
        }
#endif
        if (!vis)
            print_hint("HINT_MONSTER_SHOUTED_UNSEEN");
        else if (!mons_can_shout(m->type))
            print_hint("HINT_MONSTER_NO_SHOUT");
        else
            print_hint("HINT_MONSTER_SHOUTED_SEEN");
        break;
    }

    case HINT_MONSTER_LEFT_LOS:
    {
        const monster* m = monster_at(gc);

        if (!m || !you.can_see(*m))
            DELAY_EVENT;

        string mon_name = m->name(DESC_THE, true);
        print_hint("HINT_MONSTER_LEFT_LOS", mon_name);
        break;
    }

    case HINT_SEEN_MONSTER:
    case HINT_SEEN_FIRST_OBJECT:
        // Handled in special functions.
        break;

    case HINT_SEEN_ZERO_EXP_MON:
    {
        const monster_info* mi = env.map_knowledge(gc).monsterinfo();

        if (!mi)
            DELAY_EVENT;

        print_hint("HINT_SEEN_ZERO_EXP_MON",
                   glyph_to_tagstr(get_mons_glyph(*mi)),
                   mi->proper_name(DESC_A));
        break;
    }

    case HINT_ABYSS:
        print_hint("HINT_ABYSS",
                   stringize_glyph(get_feat_symbol(DNGN_EXIT_ABYSS)));
        break;

    case HINT_SPELL_MISCAST:
    {
        // Don't give at the beginning of your spellcasting career.
        if (you.max_magic_points <= 2)
            DELAY_EVENT;

        if (!crawl_state.game_is_hints())
        {
            print_hint("HINT_SPELL_MISCAST_EFFECTS");
            break;
        }
        print_hint("HINT_YOU_MISCAST");

        if (!player_effectively_in_light_armour()
            || is_shield(you.shield()))
        {
            print_hint("HINT_MISCAST_ARMOUR");
        }

        print_hint("HINT_MISCAST_CONTAMINATION_AND_MP");
        break;
    }

    case HINT_GLOWING:
        print_hint("HINT_GLOWING");

        if (!player_severe_contamination())
            print_hint("HINT_CONTAMINATION_MILD");
        else
            print_hint("HINT_CONTAMINATION_SEVERE");
        break;

    case HINT_YOU_RESIST:
        print_hint("HINT_YOU_RESIST");
        break;

    case HINT_CAUGHT_IN_NET:
        print_hint("HINT_CAUGHT_IN_NET");
        break;

    case HINT_YOU_SILENCE:
        redraw_screen();
        update_screen();
        print_hint("HINT_YOU_SILENCE");
        break;

    case HINT_LOAD_SAVED_GAME:
    {
        text << "Welcome back! If it's been a while, you may want to refresh "
                "your memory.\nYour <w>%</w>nventory, ";
        cmd.push_back(CMD_DISPLAY_INVENTORY);

        vector<const char *> listed;
        if (you.spell_no > 0)
        {
            listed.push_back("your spells (<w>%?</w>)");
            cmd.push_back(CMD_CAST_SPELL);
        }
        if (!your_talents(false).empty())
        {
            listed.push_back("your <w>%</w>bilities");
            cmd.push_back(CMD_USE_ABILITY);
        }
        if (Hints.hints_type != HINT_MAGIC_CHAR || you.how_mutated())
        {
            listed.push_back("your set of mutations (<w>%</w>)");
            cmd.push_back(CMD_DISPLAY_MUTATIONS);
        }
        if (!you_worship(GOD_NO_GOD))
        {
            listed.push_back("your religious standing (<w>%</w>)");
            cmd.push_back(CMD_DISPLAY_RELIGION);
        }

        listed.push_back("the message history (<w>%</w>)");
        listed.push_back("the character overview screen (<w>%</w>)");
        listed.push_back("the dungeon overview screen (<w>%</w>)");
        text << comma_separated_line(listed.begin(), listed.end())
             << " are good things to check.";
        cmd.push_back(CMD_REPLAY_MESSAGES);
        cmd.push_back(CMD_RESISTS_SCREEN);
        cmd.push_back(CMD_DISPLAY_OVERMAP);
        break;
    }
    case HINT_AUTOPICKUP_THROWN:
        print_hint("HINT_AUTOPICKUP_THROWN");
        break;
    case HINT_GAINED_SPELLCASTING:
        print_hint("HINT_GAINED_SPELLCASTING");
        break;
    case HINT_FUMBLING_SHALLOW_WATER:
        print_hint("HINT_FUMBLING_SHALLOW_WATER");
        break;
    case HINT_CLOUD_WARNING:
        print_hint("HINT_CLOUD_WARNING");
        break;
    default:
        print_hint("HINT_FOUND_UNKNOWN");
    }

    if (!text.str().empty())
    {
        string output = text.str();
        if (!cmd.empty())
            insert_commands(output, cmd);
        else
            output = untag_tiles_console(output); // also in insert_commands
        mprf(MSGCH_TUTORIAL, "%s", output.c_str());

        stop_running();
    }
}

formatted_string hints_abilities_info()
{
    ostringstream text;
    text << "<" << colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) << ">";
    string broken = "This screen shows your character's set of talents. "
        "You can gain new abilities via certain items, through religion or by "
        "way of mutations. Activation of an ability usually comes at a cost, "
        "e.g. Magic power. Press '<w>!</w>' or '<w>?</w>' to "
        "toggle between ability selection and description.";
    linebreak_string(broken, _get_hints_cols());
    text << broken;

    text << "</" << colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) << ">";

    return formatted_string::parse_string(text.str());
}

// Explains the basics of the skill screen. Don't bother the player with the
// aptitude information.
string hints_skills_info()
{
    ostringstream text;
    text << "<" << colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) << ">";
    string broken = "This screen shows the skill set of your character. "
        "The number next to the skill is your current level, the higher the "
        "better. <w>Training</w> displays training percentages. "
        "<w>Costs</w> displays relative training costs. "
        "<w>Targets</w> displays skill training targets. "
        "You can toggle which skills to train by "
        "pressing their slot letters. A <darkgrey>grey</darkgrey> skill "
        "will not be trained and ease the training of others.";
    text << broken;
    text << "</" << colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) << ">";

    return text.str();
}

string hints_skill_training_info()
{
    ostringstream text;
    text << "<" << colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) << ">";
    string broken = "The training percentage (in <brown>brown</brown>) "
        "shows the relative amount of the experience gained which will be "
        "used to train each skill. It is automatically set depending on "
        "which skills you have used recently. Disabling a skill sets the "
        "training rate to 0.";
    text << broken;
    text << "</" << colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) << ">";

    return text.str();
}

string hints_skill_costs_info()
{
    ostringstream text;
    text << "<" << colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) << ">";
    string broken = "The training cost (in <cyan>cyan</cyan>) "
        "shows the experience cost to raise the given skill one level, "
        "relative to the cost of raising an aptitude zero skill from level "
        "zero to level one.";
    text << broken;
    text << "</" << colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) << ">";

    return text.str();
}

string hints_skill_targets_info()
{
    ostringstream text;
    text << "<" << colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) << ">";
    string broken = "Press the letter of a skill to set a training target. "
        "When the target is reached a message will appear and "
        "the training of the skill will be disabled.";
    text << broken;
    text << "</" << colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) << ">";

    return text.str();
}

string hints_skills_description_info()
{
    ostringstream text;
    text << "<" << colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) << ">";
    string broken = "This screen shows the skill set of your character. "
                    "Press the letter of a skill to read its description, or "
                    "press <w>?</w> again to return to the skill selection.";

    linebreak_string(broken, _get_hints_cols());
    text << broken;
    text << "</" << colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) << ">";

    return text.str();
}

// A short explanation of Crawl's target mode and its most important commands.
static string _hints_target_mode(bool spells = false)
{
    string result;
    result = "then be taken to target mode with the nearest monster or "
             "previous target already targeted. You can also cycle through "
             "all hostile monsters in sight with <w>+</w> or <w>-</w>. "
             "Once you're aiming at the correct monster, simply hit "
             "<w>f</w>, <w>Enter</w> or <w>.</w> to shoot at it. "
             "If you miss, <w>";

    command_type cmd;
    if (spells)
    {
        result += "%ap";
        cmd = CMD_CAST_SPELL;
    }
    else
    {
        result += "%f";
        cmd = CMD_FIRE;
    }

    result += "</w> fires at the same target again.";
    insert_commands(result, { cmd });

    return result;
}

string hints_memorise_info()
{
    // TODO: this should probably be in z or I, but adding it to the memorise
    // menu was easier for the moment.
    ostringstream text;
    vector<command_type> cmd;
    text << "<" << colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) << ">";
    string m = "This screen shows the spells in your spell library. From here "
               "you can memorise spells by selecting them, as well as view "
               "spell descriptions, search for them, and organize them. As a "
               "conjurer, you start with five memorisable spells, and one "
               "already memorised: Magic Dart. (To view memorised spells, you "
               "can exit this menu and select <w>I</w>.)";

    if (player_has_available_spells())
    {
        m += "\n\nA spell that isn't <darkgray>grayed out</darkgray> or "
             "<lightred>forbidden</lightred> can be "
             "memorised right away by selecting it at this menu.";
    }
    else
    {
        m += "\n\nYou cannot memorise any ";
        m += (you.spell_no ? "more " : "");
        m += "spells right now. This will change as you grow in levels and "
             "Spellcasting proficiency. ";
    }

    if (you.spell_no)
    {
        m += "\n\nTo use magic, ";
#ifdef USE_TILE
        m += "you can <w>left mouse click</w> on the monster you wish to "
             "target (or on your player character to cast a spell on "
             "yourself) while pressing the <w>Control key</w>, and then select "
             "a spell from the menu. Or you can switch to the spellcasting "
             "display by <w>clicking on the</w> corresponding <w>tab</w>."
             "\n\nAlternatively, ";
#endif
        m += "you can press <w>%</w> and choose a spell, e.g. <w>a</w> (check "
             "with <w>?</w>). For attack spells you'll ";
        cmd.push_back(CMD_CAST_SPELL);
    }

    if (you.spell_no)
        m += _hints_target_mode(true);
    linebreak_string(m, _get_hints_cols());
    if (!cmd.empty())
        insert_commands(m, cmd);
    text << m;

    return text.str();
}

static string _hints_abilities(const item_def& item)
{
    string str = "To do this, ";

    vector<command_type> cmd;
    if (!item_is_equipped(item))
    {
        switch (item.base_type)
        {
        case OBJ_WEAPONS:
            str += "first <w>%</w>ield it";
            cmd.push_back(CMD_WIELD_WEAPON);
            break;
        case OBJ_ARMOUR:
            str += "first <w>%</w>ear it";
            cmd.push_back(CMD_WEAR_ARMOUR);
            break;
        case OBJ_JEWELLERY:
            str += "first <w>%</w>ut it on";
            cmd.push_back(CMD_WEAR_JEWELLERY);
            break;
        default:
            str += "<r>(BUG! this item shouldn't give an ability)</r>";
            break;
        }
        str += ", then ";
    }
    str += "enter the ability menu with <w>%</w>, and then "
           "choose the corresponding ability. Note that such an attempt of "
           "activation, especially by the untrained, is likely to fail.";
    cmd.push_back(CMD_USE_ABILITY);

    insert_commands(str, cmd);
    return str;
}

static string _hints_throw_stuff(const item_def &item)
{
    string result;

    result  = "To do this, press <w>%</w> to fire, then ";
    if (item.slot)
    {
        result += "<w>";
        result += item.slot;
        result += "</w> for";
    }
    else
    {
        // you don't have this/these stuff(s) at present
        result += "select ";
    }
    result += (item.quantity > 1 ? "these" : "this");
    result += " ";
    result += item_base_name(item);
    result += (item.quantity > 1? "s" : "");
    result += ". You'll ";
    result += _hints_target_mode();

    insert_commands(result, { CMD_FIRE });
    return result;
}

// num_old_talents describes the number of activatable abilities you had
// before putting on this item.
void check_item_hint(const item_def &item, unsigned int num_old_talents)
{
    if (Hints.hints_events[HINT_NEW_ABILITY_ITEM]
        && your_talents(false).size() > num_old_talents)
    {
        learned_something_new(HINT_NEW_ABILITY_ITEM);
    }
    else if (Hints.hints_events[HINT_ITEM_RESISTANCES]
             && gives_resistance(item))
    {
        learned_something_new(HINT_ITEM_RESISTANCES);
    }
}

// Explains the most important commands necessary to use an item, and mentions
// special effects, etc.
string hints_describe_item(const item_def &item)
{
    ostringstream ostr;
    ostr << "<" << colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) << ">";
    vector<command_type> cmd;

    switch (item.base_type)
    {
        case OBJ_WEAPONS:
        {
            if (is_artefact(item) && item.is_identified())
            {
                if (gives_ability(item))
                {
                    // You can activate it.
                    ostr << "When wielded, some weapons (such as this one) "
                            "offer certain abilities you can activate. ";
                    ostr << _hints_abilities(item);
                    break;
                }
                else if (gives_resistance(item))
                {
                    // It grants a resistance.
                    ostr << "\nThis weapon offers its wearer protection from "
                            "certain damage sources. For an overview of your "
                            "resistances (among other things) press <w>%</w>"
#ifdef USE_TILE
                            " or click on your avatar with the <w>right mouse "
                            "button</w>"
#endif
                            ".";
                    cmd.push_back(CMD_RESISTS_SCREEN);
                    break;
                }
                else
                    return "";
            }

            item_def *weap = you.slot_item(EQ_WEAPON, false);
            bool wielded = (weap && weap->slot == item.slot);

            if (!wielded)
            {
                ostr << "You can wield this weapon with <w>%</w>, or use "
                        "<w>%</w> to switch between the weapons in slot "
                        "a and b. (Use <w>%i</w> to adjust item slots.)";
                cmd.push_back(CMD_WIELD_WEAPON);
                cmd.push_back(CMD_WEAPON_SWAP);
                cmd.push_back(CMD_ADJUST_INVENTORY);

                // Weapon skill used by this weapon and the best weapon skill.
                skill_type curr_wpskill = item_attack_skill(item);
                skill_type best_wpskill;

                // Maybe this is a launching weapon?
                if (is_range_weapon(item))
                    best_wpskill = SK_THROWING;
                else
                    best_wpskill = best_skill(SK_SHORT_BLADES, SK_STAVES);

                // Maybe unarmed is better.
                if (you.skills[SK_UNARMED_COMBAT] > you.skills[best_wpskill])
                    best_wpskill = SK_UNARMED_COMBAT;

                if (you.skills[curr_wpskill] + 2 < you.skills[best_wpskill])
                {
                    ostr << "\nHowever, you've been training in <w>"
                         << skill_name(best_wpskill)
                         << "</w> for a while, so maybe you should "
                            "continue training that rather than <w>"
                         << skill_name(curr_wpskill)
                         << "</w>. (Press <w>%</w> to see the skill "
                            "management screen for the actual numbers.)";

                    cmd.push_back(CMD_DISPLAY_SKILLS);
                }
            }
            else // wielded weapon
            {
                if (is_range_weapon(item))
                {
                    ostr << "To attack a monster, ";
#ifdef USE_TILE
                    ostr << "<w>left mouse click</w> on the monster.\n\n";
                    ostr << "To fire a ranged weapon using the keyboard, ";
#endif
                    ostr << "press <w>%</w>. You'll ";
                    ostr << _hints_target_mode();
                    cmd.push_back(CMD_PRIMARY_ATTACK);
                }
                else
                    ostr << "To attack a monster, you can simply walk into it.";
            }

            Hints.hints_events[HINT_SEEN_WEAPON] = false;
            break;
        }
        case OBJ_MISSILES:
            if (is_throwable(&you, item))
            {
                ostr << item.name(DESC_YOUR)
                     << " can be <w>%</w>ired without the use of a launcher. ";
                ostr << _hints_throw_stuff(item);
                cmd.push_back(CMD_FIRE);
            }
            Hints.hints_events[HINT_SEEN_MISSILES] = false;
            break;

        case OBJ_ARMOUR:
        {
            bool wearable = true;
            if (you.get_innate_mutation_level(MUT_HORNS) > 0
                && is_hard_helmet(item))
            {
                ostr << "Because of your horns you cannot wear helmets. "
                        "(Press <w>%</w> to see a list of your mutations and "
                        "innate abilities.)";
                cmd.push_back(CMD_DISPLAY_MUTATIONS);
                wearable = false;
            }
            else if (item.sub_type == ARM_BARDING)
            {
                ostr << "Only nagas and armataurs can wear barding.";
                wearable = false;
            }
            else
            {
                ostr << "You can wear pieces of armour with <w>%</w> and take "
                        "them off again with <w>%</w>"
#ifdef USE_TILE
                        ", or, alternatively, simply click on their tiles to "
                        "perform either action"
#endif
                        ".";
                cmd.push_back(CMD_WEAR_ARMOUR);
                cmd.push_back(CMD_REMOVE_ARMOUR);
            }

            if (Hints.hints_type == HINT_MAGIC_CHAR
                && get_armour_slot(item) == EQ_BODY_ARMOUR
                && !is_effectively_light_armour(&item))
            {
                ostr << "\nNote that body armour with a high encumbrance "
                        "rating may hinder your ability to cast spells. Light "
                        "armour such as robes and leather armour will be "
                        "generally safe for any aspiring spellcaster.";
            }
            else if (Hints.hints_type == HINT_MAGIC_CHAR
                     && is_shield(item))
            {
                ostr << "\nNote that shields will hinder your ability to "
                        "cast spells, until you've gained enough Shields "
                        "skill to remove the penalty.";
            }
            else if (Hints.hints_type == HINT_RANGER_CHAR
                     && is_offhand(item))
            {
                ostr << "\nNote that many ranged weapons are two handed and so "
                        "cannot be used with an offhand item.";
            }

            if (!item.is_identified()
                && (is_artefact(item)
                    || get_equip_desc(item) != ISFLAG_NO_DESC))
            {
                ostr << "\n\nWeapons and armour that have unusual descriptions "
                     << "like this are much more likely to be of higher "
                     << "enchantment or have special properties, good or bad.";
            }
            if (wearable)
            {
                if (gives_resistance(item))
                {
                    ostr << "\n\nThis armour offers its wearer protection from "
                            "certain sources. For an overview of your"
                            " resistances (among other things) press <w>%</w>"
#ifdef USE_TILE
                            " or click on your avatar with the <w>right mouse "
                            "button</w>"
#endif
                            ".";
                    cmd.push_back(CMD_RESISTS_SCREEN);
                }
                if (gives_ability(item))
                {
                    ostr << "\n\nWhen worn, some types of armour (such as "
                            "this one) offer certain <w>%</w>bilities you can "
                            "activate. ";
                    ostr << _hints_abilities(item);
                    cmd.push_back(CMD_USE_ABILITY);
                }
            }
            Hints.hints_events[HINT_SEEN_ARMOUR] = false;
            break;
        }
        case OBJ_WANDS:
            ostr << "The magic within can be unleashed by evoking "
                    "(<w>%</w>) it.";
            cmd.push_back(CMD_EVOKE);
#ifdef USE_TILE
            ostr << " Alternatively, you can 1) <w>left mouse click</w> on "
                    "the monster you wish to target (or your player character "
                    "to target yourself) while pressing the <w>";
#ifdef USE_TILE_WEB
            ostr << "Ctrl + Shift keys";
#else
#if defined(UNIX) && defined(USE_TILE_LOCAL)
            if (!tiles.is_fullscreen())
              ostr << "Ctrl + Shift keys";
            else
#endif
              ostr << "Alt key";
#endif
            ostr << "</w> and pick the wand from the menu, or 2) "
                    "<w>left mouse click</w> on the wand tile and then "
                    "<w>left mouse click</w> on your target.";
#endif
            Hints.hints_events[HINT_SEEN_WAND] = false;
            break;

        case OBJ_SCROLLS:
            ostr << "Press <w>%</w> to read this scroll"
#ifdef USE_TILE
                    "or simply click on it with your <w>left mouse button</w>"
#endif
                    ".";
            cmd.push_back(CMD_READ);

            Hints.hints_events[HINT_SEEN_SCROLL] = false;
            break;

        case OBJ_JEWELLERY:
        {
            ostr << "Jewellery can be <w>%</w>ut on or <w>%</w>emoved "
                    "again"
#ifdef USE_TILE
                    ", though in Tiles, either can be done by clicking on the "
                    "item in your inventory"
#endif
                    ".";
            cmd.push_back(CMD_WEAR_JEWELLERY);
            cmd.push_back(CMD_REMOVE_JEWELLERY);

            if (gives_resistance(item))
            {
                ostr << "\n\nThis "
                     << (item.sub_type < NUM_RINGS ? "ring" : "amulet")
                     << " offers its wearer protection "
                        "from certain sources. For an overview of your "
                        "resistances (among other things) press <w>%</w>"
#ifdef USE_TILE
                        " or click on your avatar with the <w>right mouse "
                        "button</w>"
#endif
                        ".";
                cmd.push_back(CMD_RESISTS_SCREEN);
            }
            if (gives_ability(item))
            {
                ostr << "\n\nWhen worn, some types of jewellery (such as this "
                        "one) offer certain <w>%</w>bilities you can activate. ";
                cmd.push_back(CMD_USE_ABILITY);
                ostr << _hints_abilities(item);
            }
            Hints.hints_events[HINT_SEEN_JEWELLERY] = false;
            break;
        }
        case OBJ_POTIONS:
            ostr << "Press <w>%</w> to quaff this potion"
#ifdef USE_TILE
                    "or simply click on it with your <w>left mouse button</w>"
#endif
                    ".";
            cmd.push_back(CMD_QUAFF);
            Hints.hints_events[HINT_SEEN_POTION] = false;
            break;

        case OBJ_BOOKS:
            if (item.sub_type == BOOK_MANUAL)
            {
                ostr << "A manual can greatly help you in training a skill. "
                        "After you pick one up, the skill in question will be "
                        "trained more efficiently and will level up faster "
                        "until you exhaust the manual's contents.";
                cmd.push_back(CMD_READ);
            }
            else // It's a spellbook!
            {
                ostr << "\nYou can pick up a spellbook to add its spells to "
                        "your spell library. (View your spell library with "
                        "<w>%</w>.)";
                cmd.push_back(CMD_MEMORISE_SPELL);
            }
            ostr << "\n";
            Hints.hints_events[HINT_SEEN_SPBOOK] = false;
            break;

        case OBJ_CORPSES:
            ostr << "Skeletons and corpses can be used as components for "
                    "certain necromantic spells. Apart from that, they are "
                    "largely useless.";
            break;

       case OBJ_STAVES:
            ostr << "This staff can enhance your spellcasting, making spells "
                    "of its related spell school more powerful.";

            if (gives_resistance(item))
            {
                ostr << "It also offers its wielder protection from "
                        "certain sources. For an overview of your "
                        "resistances (among other things) press <w>%</w>"
#ifdef USE_TILE
                        " or click on your avatar with the <w>right mouse "
                        "button</w>"
#endif
                        ".";

                cmd.push_back(CMD_RESISTS_SCREEN);
            }
            else if (you_worship(GOD_TROG))
            {
                ostr << "\n\nSeeing how "
                     << god_name(GOD_TROG, false)
                     << " frowns upon the use of magic, this staff will be "
                        "of little use to you and you might just as well "
                        "<w>%</w>rop it now.";
                cmd.push_back(CMD_DROP);
            }
            Hints.hints_events[HINT_SEEN_STAFF] = false;
            break;

        case OBJ_MISCELLANY:
            ostr << "Miscellaneous items sometimes harbour magical powers "
                    "that can be harnessed by e<w>%</w>oking the item.";
            cmd.push_back(CMD_EVOKE);

            Hints.hints_events[HINT_SEEN_MISC] = false;
            break;

        default:
            return "";
    }

    ostr << "</" << colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) << ">";
    string broken = ostr.str();
    if (!cmd.empty())
        insert_commands(broken, cmd);
    return broken;
}

// FIXME: With the new targeting system, the hints for interesting monsters
//        and features ("right-click/press v for more information") are no
//        longer getting displayed.
//        Players might still end up e'x'aming and particularly clicking on
//        but it's a lot more hit'n'miss now.
bool hints_pos_interesting(int x, int y)
{
    return cloud_at(coord_def(x, y))
           || _hints_feat_interesting(env.grid[x][y]);
}

static bool _hints_feat_interesting(dungeon_feature_type feat)
{
    // Altars and branch entrances are always interesting.
    return feat_is_altar(feat)
           || feat_is_branch_entrance(feat)
           || feat_is_stone_stair(feat)
           || feat_is_escape_hatch(feat)
           || feat_is_trap(feat)
           || feat_is_statuelike(feat);
}

string hints_describe_pos(int x, int y)
{
    ostringstream ostr;
    _hints_describe_cloud(x, y, ostr);
    _hints_describe_feature(x, y, ostr);
    if (ostr.str().empty())
        return "";
    return "\n<" + colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) + ">"
            + ostr.str()
            + "</" + colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) + ">";
}

static void _hints_describe_feature(int x, int y, ostringstream& ostr)
{
    const dungeon_feature_type feat = env.grid[x][y];
    const coord_def            where(x, y);

    if (!ostr.str().empty())
        ostr << "\n\n";

    switch (feat)
    {
    case DNGN_TRAP_TELEPORT:
    case DNGN_TRAP_TELEPORT_PERMANENT:
    case DNGN_TRAP_ALARM:
    case DNGN_TRAP_ZOT:
#if TAG_MAJOR_VERSION == 34
    case DNGN_TRAP_MECHANICAL:
    case DNGN_TRAP_ARROW:
    case DNGN_TRAP_SPEAR:
    case DNGN_TRAP_BLADE:
    case DNGN_TRAP_DART:
    case DNGN_TRAP_BOLT:
#endif
    case DNGN_TRAP_NET:
    case DNGN_TRAP_PLATE:
        ostr << "These nasty constructions can cause a range of "
                "unpleasant effects. You won't be able to avoid "
                "tripping traps by flying over them; their magic "
                "construction will cause them to be triggered anyway.";
        Hints.hints_events[HINT_SEEN_TRAP] = false;
        break;

    case DNGN_TRAP_SHAFT:
        ostr << "The dungeon contains a number of natural obstacles such "
                "as shafts, which lead one to three levels down. Once you "
                "know the shaft is there, you can safely step over it.\n"
                "If you want to jump down there, use <w>></w> to do so. "
                "Be warned that getting back here might be difficult.";
        Hints.hints_events[HINT_SEEN_TRAP] = false;
        break;

    case DNGN_TRAP_WEB:
        ostr << "Some areas of the dungeon, such as the Spider Nest, may "
                "be strewn with giant webs that may ensnare you for a short "
                "time. Insects, oozes and incorporeal entities can navigate "
                "the webs safely.";
        Hints.hints_events[HINT_SEEN_WEB] = false;
        break;

    case DNGN_STONE_STAIRS_DOWN_I:
    case DNGN_STONE_STAIRS_DOWN_II:
    case DNGN_STONE_STAIRS_DOWN_III:
        ostr << "You can enter the next (deeper) level by following them "
                "down (<w>></w>). To get back to this level again, "
                "press <w><<</w> while standing on the upstairs.";
#ifdef USE_TILE
        ostr << " In Tiles, you can achieve the same, in either direction, "
                "by clicking the <w>left mouse button</w>.";
#endif

        if (is_unknown_stair(where))
        {
            ostr << "\n\nYou have not yet passed through this particular "
                    "set of stairs. ";
        }

        Hints.hints_events[HINT_SEEN_STAIRS] = false;
        break;

    case DNGN_EXIT_DUNGEON:
        ostr << "These stairs lead out of the dungeon. Following them "
                "will end the game. The only way to win is to "
                "transport the fabled Orb of Zot outside.";
        break;

    case DNGN_STONE_STAIRS_UP_I:
    case DNGN_STONE_STAIRS_UP_II:
    case DNGN_STONE_STAIRS_UP_III:
        ostr << "You can enter the previous (shallower) level by "
                "following these up (<w><<</w>). This is ideal for "
                "retreating or finding a safe resting spot, since the "
                "previous level will have less monsters, and monsters "
                "on this level can't follow you up unless they're "
                "standing right next to you. To get back to this "
                "level again, press <w>></w> while standing on the "
                "downstairs.";
#ifdef USE_TILE
        ostr << " In Tiles, you can perform either action simply by "
                "clicking the <w>left mouse button</w> instead.";
#endif
        if (is_unknown_stair(where))
        {
            ostr << "\n\nYou have not yet passed through this "
                    "particular set of stairs. ";
        }
        Hints.hints_events[HINT_SEEN_STAIRS] = false;
        break;

    case DNGN_ESCAPE_HATCH_DOWN:
    case DNGN_ESCAPE_HATCH_UP:
        ostr << "Escape hatches can be used to quickly leave a level with "
                "<w><<</w> and <w>></w>, respectively. Note that you will "
                "usually be unable to return right away.";

        Hints.hints_events[HINT_SEEN_ESCAPE_HATCH] = false;
        break;

#if TAG_MAJOR_VERSION == 34
    case DNGN_ENTER_PORTAL_VAULT:
        ostr << _describe_portal(where);
        Hints.hints_events[HINT_SEEN_PORTAL] = false;
        break;
#endif

    case DNGN_CLOSED_DOOR:
    case DNGN_CLOSED_CLEAR_DOOR:
        if (!Hints.hints_explored)
        {
            ostr << "\nTo avoid accidentally opening a door you'd rather "
                    "remain closed during travel or autoexplore, you can "
                    "mark it with an exclusion from the map view "
                    "(<w>X</w>) with <w>ee</w> while your cursor is on the "
                    "grid in question. Such an exclusion will prevent "
                    "autotravel from ever entering that grid until you "
                    "remove the exclusion with another press of <w>Xe</w>.";
        }
        break;

    default:
        if (feat_is_altar(feat))
        {
            god_type altar_god = feat_altar_god(feat);

            // TODO: mention Gozag here?
            if (you_worship(GOD_NO_GOD))
            {
                ostr << "This is your chance to join a religion! In "
                        "general, the gods will help their followers, "
                        "bestowing powers of all sorts upon them, but many "
                        "of them demand a life of dedication, constant "
                        "tributes or entertainment in return.\n";
                if (altar_god == GOD_ECUMENICAL)
                {
                    ostr << "This particular altar is so ancient that you "
                            "cannot make out which god it is dedicated to! "
                            "Converting here by pressing <w>></w> while "
                            "standing on the altar will enter you into service "
                            "of a random god, who will grant you some "
                            "additional piety in thanks.";
                }
                else
                {
                    ostr << "You can get information about <w>"
                         << god_name(altar_god)
                         << "</w> by pressing <w>></w> while standing on the "
                            "altar. Before taking up the responding faith "
                            "you'll be asked for confirmation.";
                }
            }
            else if (you_worship(altar_god))
            {
                // If we don't have anything to say, return early.
                return;
            }
            else if (altar_god == GOD_ECUMENICAL)
            {
                ostr << "This particular altar is so ancient that you cannot "
                        "make out which god it is dedicated to, and "
                     << god_name(you.religion)
                     << " probably won't like it if you switch allegiance. If "
                        "you want to take the risk, you can convert here by "
                        "pressing <w>></w> while standing on the altar. Doing "
                        "so will enter you into service of a random god, who "
                        "will grant you some additional piety in thanks.";
            }
            else
            {
                ostr << god_name(you.religion)
                     << " probably won't like it if you switch allegiance, "
                        "but having a look won't hurt: to get information "
                        "on <w>";
                ostr << god_name(altar_god);
                ostr << "</w>, press <w>></w> while standing on the "
                        "altar. Before taking up the responding faith (and "
                        "abandoning your current one!) you'll be asked for "
                        "confirmation."
                        "\nTo see your current standing with "
                     << god_name(you.religion)
                     << " press <w>^</w>"
#ifdef USE_TILE
                        ", or click with your <w>right mouse button</w> "
                        "on your avatar while pressing <w>Shift</w>"
#endif
                        ".";
            }
            Hints.hints_events[HINT_SEEN_ALTAR] = false;
            break;
        }
        else if (feat_is_branch_entrance(feat))
        {
            ostr << "An entryway into one of the many dungeon side branches in "
                    "Crawl. ";
            if (feat != DNGN_ENTER_TEMPLE)
                ostr << "Beware, sometimes these can be deadly!";
            break;
        }
    }
}

static void _hints_describe_cloud(int x, int y, ostringstream& ostr)
{
    cloud_struct* cloud = cloud_at(coord_def(x, y));
    if (!cloud)
        return;

    const string cname = cloud->cloud_name(true);
    const cloud_type ctype = cloud->type;

    if (!ostr.str().empty())
        ostr << "\n\n";

    ostr << "The " << cname << " ";

    if (ends_with(cname, "s"))
        ostr << "are ";
    else
        ostr << "is ";

    bool need_cloud = false;
    if (is_harmless_cloud(ctype))
        ostr << "harmless. ";
    else if (is_damaging_cloud(ctype, true))
    {
        ostr << "probably dangerous, and you should stay out of it if you "
                "can. ";
    }
    else
    {
        ostr << "currently harmless, but that could change at some point. "
                "Check the overview screen (<w>%</w>) to view your "
                "resistances.";
        need_cloud = true;
    }

    if (is_opaque_cloud(ctype))
    {
        ostr << (need_cloud? "\nThis cloud" : "It")
             << " is opaque. If two or more opaque clouds are between "
                "you and a square, you won't be able to see anything in that "
                "square.";
    }
}

bool hints_monster_interesting(const monster* mons)
{
    if (mons_is_unique(mons->type) || mons->type == MONS_PLAYER_GHOST)
        return true;

    // Highlighted in some way.
    if (_mons_is_highlighted(mons))
        return true;

    // Dangerous.
    return mons_threat_level(*mons) == MTHRT_NASTY;
}

string hints_describe_monster(const monster_info& mi, bool has_stat_desc)
{
    ostringstream ostr;
    bool dangerous = false;
    if (mons_is_unique(mi.type))
    {
        ostr << "Did you think you were the only adventurer in the dungeon? "
                "Well, you thought wrong! These unique adversaries often "
                "possess skills that normal monsters wouldn't, so be "
                "careful.\n\n";
        dangerous = true;
    }
    else if (mi.type == MONS_PLAYER_GHOST)
    {
        ostr << "The ghost of a deceased adventurer, it would like nothing "
                "better than to send you the same way.\n\n";
        dangerous = true;
    }
    // Don't call friendly monsters dangerous.
    else if (!mons_att_wont_attack(mi.attitude))
    {
        if (mi.threat == MTHRT_NASTY)
        {
            ostr << "This monster appears to be really dangerous!\n";
            dangerous = true;
        }
        else if (mi.threat == MTHRT_TOUGH)
        {
            ostr << "This monster appears to be quite dangerous.\n";
            dangerous = true;
        }
    }

    if (mi.is(MB_BERSERK))
    {
        ostr << "A berserking monster is bloodthirsty and fighting madly. "
                "Such a blood rage makes it particularly dangerous!\n\n";
        dangerous = true;
    }

    // Monster is highlighted.
    if (mi.attitude == ATT_FRIENDLY)
    {
        ostr << "Friendly monsters will follow you around and attempt to aid "
                "you in battle. You can order nearby allies by <w>t</w>alking "
                "to them.";

        if (!mons_att_wont_attack(mi.attitude))
        {
            ostr << "\n\nHowever, it is only <w>temporarily</w> friendly, "
                    "and will become dangerous again when this friendliness "
                    "wears off.";
        }
    }
    else if (dangerous)
    {
        if (!Hints.hints_explored && (mi.is(MB_WANDERING) || mi.is(MB_UNAWARE)))
        {
            ostr << "You can easily mark its square as dangerous to avoid "
                    "accidentally entering into its field of view when using "
                    "auto-explore or auto-travel. To do so, enter targeting "
                    "mode with <w>x</w> and then press <w>e</w> when your "
                    "cursor is hovering over the monster's grid. Doing so will "
                    "mark this grid and all surrounding ones within a radius "
                    "of 8 as \"excluded\" ones that explore or travel modes "
                    "won't enter.";
        }
        else
        {
            ostr << "This might be a good time to run away";

            if (you_worship(GOD_TROG) && you.can_go_berserk())
                ostr << " or apply your Berserk <w>a</w>bility";
            ostr << ".";
        }
    }
    else if (Options.stab_highlight != CHATTR_NORMAL
             && mi.is(MB_STABBABLE))
    {
        ostr << "Apparently it has not noticed you - yet. Note that you do "
                "not have to engage every monster you meet. Sometimes, "
                "discretion is the better part of valour.";
    }
    else if (Options.may_stab_highlight != CHATTR_NORMAL
             && mi.is(MB_DISTRACTED))
    {
        ostr << "Apparently it has been distracted by something. You could "
                "use this opportunity to sneak up on this monster - or to "
                "sneak away.";
    }

    if (!dangerous && !has_stat_desc)
    {
        ostr << "\nThis monster doesn't appear to have any resistances or "
                "susceptibilities. It cannot fly and is of average speed. "
                "Examining other, possibly more high-level monsters can give "
                "important clues as to how to deal with them.";
    }

    if (ostr.str().empty())
        return "";
    return "\n<" + colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) + ">"
            + ostr.str()
            + "</" + colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) + ">";
}

void hints_observe_cell(const coord_def& gc)
{
    if (feat_is_escape_hatch(env.grid(gc)))
        learned_something_new(HINT_SEEN_ESCAPE_HATCH, gc);
    else if (feat_is_portal_entrance(env.grid(gc)))
        learned_something_new(HINT_SEEN_PORTAL, gc);
    else if (feat_is_branch_entrance(env.grid(gc)))
        learned_something_new(HINT_SEEN_BRANCH, gc);
    else if (is_feature('>', gc))
        learned_something_new(HINT_SEEN_STAIRS, gc);
    else if (is_feature('_', gc))
        learned_something_new(HINT_SEEN_ALTAR, gc);
    else if (is_feature('^', gc))
        learned_something_new(HINT_SEEN_TRAP, gc);
    else if (feat_is_runed(env.grid(gc)))
        learned_something_new(HINT_SEEN_RUNED_DOOR, gc);
    else if (feat_is_door(env.grid(gc)))
        learned_something_new(HINT_SEEN_DOOR, gc);
    else if (env.grid(gc) == DNGN_ENTER_SHOP)
        learned_something_new(HINT_SEEN_SHOP, gc);

    const int it = you.visible_igrd(gc);
    if (it != NON_ITEM)
    {
        const item_def& item(env.item[it]);

        if (Options.feature_item_highlight != CHATTR_NORMAL
            && (is_feature('>', gc) || is_feature('<', gc)))
        {
            learned_something_new(HINT_STAIR_HIGHLIGHT, gc);
        }
        else if (Options.trap_item_highlight != CHATTR_NORMAL
                 && is_feature('^', gc))
        {
            learned_something_new(HINT_TRAP_HIGHLIGHT, gc);
        }
        else if (Options.heap_highlight != CHATTR_NORMAL && item.link != NON_ITEM)
            learned_something_new(HINT_HEAP_HIGHLIGHT, gc);
    }
}

void tutorial_msg(const char *key, bool end)
{
    string text = getHintString(key);
    if (text.empty())
        return mprf(MSGCH_ERROR, "Error, no message for '%s'.", key);

    _replace_static_tags(text);
    text = untag_tiles_console(text);

    // n.b. leaving the dungeon counts as "winning" here (and pops up the
    // tutorial summary)
    if (end)
        screen_end_game(text, you.hp <= 0 ? game_exit::death : game_exit::win);

    // "\n" to preserve indented parts, the rest is unwrapped, or split into
    // paragraphs by "\n\n", split_string() will ignore the empty line.
    for (const string &chunk : split_string("\n", text, false))
        mprf(MSGCH_TUTORIAL, "%s", chunk.c_str());

    // tutorial_msg can get called in an vault epilogue during --builddb,
    // which can lead to a crash on tiles builds in runrest::stop as
    // there is no `tiles`. This seemed like the best place to fix this.
    #ifdef USE_TILE_LOCAL
    if (!crawl_state.tiles_disabled)
    #endif
        stop_running();
}
