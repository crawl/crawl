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
#include "localise.h"
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
//     Options.clear_messages = true;
    Options.show_more  = true;
    Options.small_more = false;

#ifdef USE_TILE
    Options.tile_tag_pref = TAGPREF_TUTORIAL;
#endif
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

    return localise("%c - %s %s %s",
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
    string prompt = "<white>You must be new here indeed!</white>"
        "\n\n"
        "<cyan>You can be:</cyan>";
    prompt = localise(prompt);
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
        hbox->add_child(move(tile));
        hbox->add_child(label);
#endif

        auto btn = make_shared<MenuButton>();
#ifdef USE_TILE_LOCAL
        hbox->set_margin_for_sdl(4,8);
        btn->set_child(move(hbox));
#else
        btn->set_child(move(label));
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
        string label_text = localise("Esc - Quit");
        auto label = make_shared<Text>(formatted_string(label_text, BROWN));
        auto btn = make_shared<MenuButton>();
        btn->set_child(move(label));
        btn->hotkey = CK_ESCAPE;
        btn->id = CK_ESCAPE;
        sub_items->add_button(btn, 0, 0);
    }
    {
        string label_text = localise("  * - Random hints mode character");
        auto label = make_shared<Text>(formatted_string(label_text, BROWN));
        auto btn = make_shared<MenuButton>();
        btn->set_child(move(label));
        btn->hotkey = '*';
        btn->id = '*';
        sub_items->add_button(btn, 0, 1);
    }

    auto popup = make_shared<ui::Popup>(vbox);

    ui::run_layout(move(popup), done);

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
        return SP_HILL_ORC;
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

static void _replace_static_tags(string &text)
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

    while ((p = text.find("$feature[")) != string::npos)
    {
        size_t q = text.find("]", p + 9);
        if (q == string::npos)
        {
            text += "<lightred>ERROR: unterminated $feature</lightred>";
            break;
        }

        string feat_name = text.substr(p + 9, q - p - 9);
        dungeon_feature_type feat = dungeon_feature_by_name(feat_name);

        string glyph = stringize_glyph(get_feat_symbol(feat));

        if (glyph == "<")
            glyph += "<";

        text.replace(p, q - p + 1, glyph);
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
    ui::run_layout(move(popup), done);
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

static string _do_hint_replacements(string text, const string& arg1 = "", const string& arg2 = "")
{
    _replace_static_tags(text);
    text = untag_tiles_console(text);


    // i18n: We can't just do a simple replace of args because some languages
    // have grammatical case, which potentially requires the args to be inflected

    // for English, empty string, and glyph tags, we can just do a simple replace
    if (!localisation_active() || arg1.empty() || arg1[0] == '<')
        text = replace_all(text, "$1", arg1);
    if (!localisation_active() || arg2.empty() || arg2[0] == '<')
        text = replace_all(text, "$2", arg2);

    if (contains(text, "$1") || contains(text, "$2"))
    {
        // Not English, and at least one arg is something other than a glyph tag
        text = replace_all(text, "$1", "@$1@");
        text = replace_all(text, "$2", "@$2@");
        text = localise(text, {{"@$1@", arg1}, {"@$2@", arg2}}, false);
    }

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
    string text = getHintString(key);
    if (text.empty())
        return mprf(MSGCH_ERROR, "Error, no hint for '%s'.", key.c_str());

    text = _do_hint_replacements(text, arg1, arg2);

    // "\n" to preserve indented parts, the rest is unwrapped, or split into
    // paragraphs by "\n\n", split_string() will ignore the empty line.
    for (const string &chunk : split_string("\n", text))
        mprf_nolocalise(MSGCH_TUTORIAL, "%s", chunk.c_str());

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

        if (!item_type_known(obj))
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

            print_hint("HINT_REST_BETWEEN_FIGHTS");

            if (you.hp < you.hp_max && you_worship(GOD_TROG)
                && you.can_go_berserk())
            {
                print_hint("HINT_BERSERK_CONSERVES_HP");
            }

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
    case OBJ_CORPSES:
        learned_something_new(HINT_SEEN_CARRION);
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
    case SK_TRANSLOCATIONS:
    case SK_TRANSMUTATIONS:
    case SK_FIRE_MAGIC:
    case SK_ICE_MAGIC:
    case SK_AIR_MAGIC:
    case SK_EARTH_MAGIC:
    case SK_POISON_MAGIC:
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
    case SK_SLINGS:
    case SK_BOWS:
    case SK_CROSSBOWS:
        learned_something_new(HINT_GAINED_RANGED_SKILL);
        break;

    default:
        break;
    }
}

#ifndef USE_TILE
// As safely as possible, colourize the passed glyph.
// Stringizes it and handles quoting "<".
static string _colourize_glyph(int col, unsigned ch)
{
    cglyph_t g;
    g.col = col;
    g.ch = ch;
    return glyph_to_tagstr(g);
}
#endif

static bool _mons_is_highlighted(const monster* mons)
{
    return mons->friendly()
               && Options.friend_brand != CHATTR_NORMAL
           || mons_looks_stabbable(*mons)
               && Options.stab_brand != CHATTR_NORMAL
           || mons_looks_distracted(*mons)
               && Options.may_stab_brand != CHATTR_NORMAL;
}

static bool _advise_use_wand()
{
    for (auto &obj : you.inv)
    {
        if (!obj.defined())
            continue;

        if (obj.base_type != OBJ_WANDS)
            continue;

        // Wand type unknown, might be useful.
        if (!item_type_known(obj))
            return true;

        // Can it be used to fight?
        switch (obj.sub_type)
        {
        case WAND_FLAME:
        case WAND_PARALYSIS:
        case WAND_ICEBLAST:
        case WAND_CHARMING:
        case WAND_ACID:
        case WAND_MINDBURST:
            return true;
        }
    }

    return false;
}

void hints_monster_seen(const monster& mon)
{
    if (mons_is_firewood(mon))
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
            learned_something_new(HINT_MONSTER_BRAND, mon.pos());
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

    if (is_tiles())
        print_hint("HINT_SEEN_MONSTER_TILES", mon.name(DESC_A).c_str());
    else
    {
        print_hint("HINT_SEEN_MONSTER_CONSOLE",
                   glyph_to_tagstr(get_mons_glyph(mi)));
        if (crawl_view.mlistsz.y > 0)
            print_hint("HINT_MONSTER_LIST_CONSOLE");
    }

    print_hint("HINT_LEARN_ABOUT_MONSTERS");
    print_hint("HINT_ATTACK_MONSTER");

    if (Hints.hints_type == HINT_RANGER_CHAR)
    {
        print_hint("HINT_ATTACK_MONSTER_RANGED");

        if (!you.weapon()
            || you.weapon()->base_type != OBJ_WEAPONS
            || you.weapon()->sub_type != WPN_SHORTBOW)
        {
            print_hint("HINT_RANGED_NOT_WIELDED");
        }
        else
            print_hint("HINT_RANGED_WIELDED");
    }
    else if (Hints.hints_type == HINT_MAGIC_CHAR)
        print_hint("HINT_ATTACK_MONSTER_MAGIC");
}

void hints_first_item(const item_def &item)
{
    // Happens if monster is standing on dropped corpse or item.
    if (monster_at(item.pos))
        return;

    if (!Hints.hints_events[HINT_SEEN_FIRST_OBJECT]
        || Hints.hints_just_triggered)
    {
        // NOTE: Since a new player might not think to pick up a
        // corpse (and why should they?), HINT_SEEN_CARRION is done when a
        // corpse is first seen.
        if (!Hints.hints_just_triggered
            && item.base_type == OBJ_CORPSES
            && !monster_at(item.pos))
        {
            learned_something_new(HINT_SEEN_CARRION, item.pos);
        }
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

static void _describe_portal(const coord_def &gc)
{
    const dungeon_feature_type feat = env.grid(gc);
    string glyph_tagstr = glyph_to_tagstr(get_cell_glyph(gc));

    // For the sake of completeness, though it's very unlikely that a
    // player will find a bazaar entrance before reaching XL 7.
    if (feat == DNGN_ENTER_BAZAAR)
        print_hint("HINT_SEEN_BAZAAR_PORTAL", glyph_tagstr);
    // Sewers can appear on D:3-6, ossuaries D:4-8.
    else
        print_hint("HINT_SEEN_PORTAL", glyph_tagstr);

    print_hint("HINT_SEEN_PORTAL_2", glyph_tagstr);
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
    case HINT_ANIMATE_CORPSE_SKELETON:
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

    case HINT_SEEN_RANDART:
        print_hint("HINT_SEEN_RANDART");
        break;

    case HINT_SEEN_CARRION:
        // TODO: Specialcase skeletons!

        if (gc.x <= 0 || gc.y <= 0) // XXX: only relevant for carrion shops?
            print_hint("HINT_NON_FLOOR_CARRION");
        else
        {
            int i = you.visible_igrd(gc);
            if (i == NON_ITEM)
                print_hint("HINT_NON_FLOOR_CARRION");
            else
            {
                print_hint("HINT_SEEN_CARRION");
#ifdef USE_TILE
                tiles.place_cursor(CURSOR_TUTORIAL, gc);
                tiles.add_text_tag(TAG_TUTORIAL, env.item[i].name(DESC_A), gc);
#endif
            }
        }
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
        _describe_portal(gc);
        break;

    case HINT_STAIR_BRAND:
        // Monster or player standing on stairs.
        if (actor_at(gc))
            DELAY_EVENT;

        print_hint("HINT_STAIR_BRAND");
        break;

    case HINT_HEAP_BRAND:
        // Monster or player standing on heap.
        if (actor_at(gc))
            DELAY_EVENT;

        print_hint("HINT_HEAP_BRAND");
        break;

    case HINT_TRAP_BRAND:
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
        {
            string glyph_tagstr;
#ifndef USE_TILE
            cglyph_t g = get_cell_glyph(gc);

            if (g.ch == ' ' || g.col == BLACK)
                g.col = LIGHTCYAN;

            glyph_tagstr = _colourize_glyph(g.col, '^');
#endif
            if (you.pos() == gc)
                print_hint("HINT_TRIGGERED_TRAP", glyph_tagstr);
            else
                print_hint("HINT_SEEN_TRAP", glyph_tagstr);
        }

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
        switch (you.religion)
        {
        // Gods where first granted ability is passive.
        case GOD_ASHENZARI:
        case GOD_BEOGH:
        case GOD_MAKHLEB:
        case GOD_VEHUMET:
        case GOD_XOM:
        case GOD_SHINING_ONE:
            // TODO: update me
            print_hint("HINT_NEW_ABILITY_GOD_PASSIVE");
            break;

        // Gods where first granted ability is active.
        default:
            print_hint("HINT_NEW_ABILITY_GOD_ACTIVE");
            break;
        }
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
            case GOD_XOM:
                return print_hint("HINT_CONVERT Xom");
            case GOD_GOZAG:
                return print_hint("HINT_CONVERT Gozag");
            case GOD_RU:
                return print_hint("HINT_CONVERT Ru");
            case GOD_USKAYAW:
                return print_hint("HINT_CONVERT Uskayaw");
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
                print_hint("HINT_EXCOMMUNICATED", old_god_name);
            else
                print_hint("HINT_GOD_ABANDONED", old_god_name);

        }
        else
        {
            bool angry = false;
            if (is_good_god(old_god))
            {
                if (is_good_god(new_god))
                {
                    print_hint("HINT_CONVERTED_GOD_NOT_ANGRY",
                               old_god_name, new_god_name);

                    if (old_piety > piety_breakpoint(0))
                        print_hint("HINT_CONVERTED_KEPT_PIETY");

                    print_hint("HINT_CONVERTED_GOOD_TO_GOOD");
                }
                else if (!god_hates_your_god(old_god))
                {
                    print_hint("HINT_CONVERTED_GOD_NOT_ANGRY",
                               old_god_name, new_god_name);
                    print_hint("HINT_CONVERTED_GOOD_TO_NEUTRAL");
                }
                else
                {
                    print_hint("HINT_CONVERTED_GOD_ANGRY",
                               old_god_name, new_god_name);
                    print_hint("HINT_CONVERTED_GOOD_TO_EVIL");
                    angry = true;
                }
            }
            else if (god_hates_your_god(old_god))
            {
                print_hint("HINT_CONVERTED_GOD_ANGRY",
                           old_god_name, new_god_name);
                print_hint("HINT_CONVERTED_NONGOOD_TO_HATED");
                angry = true;
            }

            if (angry)
                print_hint("HINT_CONVERTED_WRATH", old_god_name, new_god_name);
        }

        break;
    }

    case HINT_WIELD_WEAPON:
    {
        int wpn = you.equip[EQ_WEAPON];
        if (Hints.hints_type == HINT_RANGER_CHAR && wpn != -1
            && you.inv[wpn].is_type(OBJ_WEAPONS, WPN_SHORTBOW))
        {
            print_hint("HINT_SWITCH_WEAPONS_A_AND_B");
        }
        else
            print_hint("HINT_SWITCH_BACK_TO_WEAPON_A");

        break;
    }
    case HINT_FLEEING_MONSTER:
        if (Hints.hints_type != HINT_BERSERK_CHAR)
            return;

        print_hint("HINT_FLEEING_MONSTER");
        break;

    case HINT_MONSTER_BRAND:
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

        string mon_name = uppercase_first(m->name(DESC_THE, true));
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

        const item_def *shield = you.slot_item(EQ_SHIELD, false);
        if (!player_effectively_in_light_armour() || shield)
            print_hint("HINT_MISCAST_ARMOUR");

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
        text << localise("Welcome back! If it's been a while, you may want to "
                         "refresh your memory.\n");

        vector<const char *> listed;
        listed.push_back("Your <w>%</w>nventory");
        cmd.push_back(CMD_DISPLAY_INVENTORY);

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
        text << localise("%s are good things to check.",
                         comma_separated_line(listed.begin(), listed.end()));
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
    case HINT_ANIMATE_CORPSE_SKELETON:
        print_hint("HINT_ANIMATE_CORPSE_SKELETON");
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
    string broken = _do_hint_replacements(getHintString("HINT_ABILITIES_SCREEN"));
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
    string broken = _do_hint_replacements(getHintString("HINT_SKILLS_SCREEN"));
    text << broken;
    text << "</" << colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) << ">";

    return text.str();
}

string hints_skill_training_info()
{
    ostringstream text;
    text << "<" << colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) << ">";
    string broken = _do_hint_replacements(getHintString("HINT_SKILL_TRAINING"));
    text << broken;
    text << "</" << colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) << ">";

    return text.str();
}

string hints_skill_costs_info()
{
    ostringstream text;
    text << "<" << colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) << ">";
    string broken = _do_hint_replacements(getHintString("HINT_SKILL_COSTS"));
    text << broken;
    text << "</" << colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) << ">";

    return text.str();
}

string hints_skill_targets_info()
{
    ostringstream text;
    text << "<" << colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) << ">";
    string broken = _do_hint_replacements(getHintString("HINT_SKILL_TARGETS"));
    text << broken;
    text << "</" << colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) << ">";

    return text.str();
}

string hints_skills_description_info()
{
    ostringstream text;
    text << "<" << colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) << ">";
    string broken = _do_hint_replacements(getHintString("HINT_SKILL_DESCRIPTIONS"));
    linebreak_string(broken, _get_hints_cols());
    text << broken;
    text << "</" << colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) << ">";

    return text.str();
}

// A short explanation of Crawl's target mode and its most important commands.
static string _hints_target_mode(bool spells = false)
{
    string result = getHintString("HINT_TARGET_MODE");

    command_type cmd;
    string arg1;
    if (spells)
    {
        arg1 = "%ap";
        cmd = CMD_CAST_SPELL;
    }
    else
    {
        arg1 = "%f";
        cmd = CMD_FIRE;
    }

    result = _do_hint_replacements(result, arg1);
    insert_commands(result, { cmd });

    return result;
}

string hints_memorise_info()
{
    // TODO: this should probably be in z or I, but adding it to the memorise
    // menu was easier for the moment.
    ostringstream text;
    text << "<" << colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) << ">";
    string m = getHintString("HINT_MEMORISE_SCREEN");

    m += "\n";
    if (player_has_available_spells())
        m +=  _do_hint_replacements(getHintString("HINT_MEMORISE"));
    else
        m += _do_hint_replacements(getHintString("HINT_CANNOT_MEMORISE"));

    if (you.spell_no)
        m += "\n" + _do_hint_replacements(getHintString("HINT_CAST_SPELL"));

    if (you.spell_no)
        m += _hints_target_mode(true);
    linebreak_string(m, _get_hints_cols());
    text << m;

    return text.str();
}

static string _hints_abilities(const item_def& item)
{
    string str;

    if (item_is_equipped(item))
        str = getHintString("HINT_ACTIVATE_ABILITY_ITEM_EQUIPPED");
    else
    {
        switch (item.base_type)
        {
        case OBJ_WEAPONS:
            str = getHintString("HINT_ACTIVATE_ABILITY_WEAPON_NOT_WIELDED");
            break;
        case OBJ_ARMOUR:
            str = getHintString("HINT_ACTIVATE_ABILITY_ARMOUR_NOT_WORN");
            break;
        case OBJ_JEWELLERY:
            str = getHintString("HINT_ACTIVATE_ABILITY_JEWELLERY_NOT_WORN");
            break;
        default:
            str += "<r>(BUG! this item shouldn't give an ability)</r>";
            break;
        }
    }

    return str;
}

static string _hints_throw_stuff(const item_def &item)
{
    string result;

    if (item.slot)
    {
        result = getHintString("HINT_THROW_INVENTORY_ITEM");
        string slot = " ";
        slot[0] = (char)item.slot;
        result = replace_all(result, "$1", slot);
    }
    else
    {
        // you don't have this item at present
        result = getHintString("HINT_THROW_GROUND_ITEM");
    }

    result += _hints_target_mode();
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
            if (is_artefact(item) && item_type_known(item))
            {
                if (gives_ability(item))
                {
                    // You can activate it.
                    ostr << getHintString("HINT_WEAPON_HAS_ABILITY");
                    ostr << _hints_abilities(item);
                    break;
                }
                else if (gives_resistance(item))
                {
                    // It grants a resistance.
                    ostr << getHintString("HINT_WEAPON_HAS_RESIST");
                    break;
                }
                else
                    return "";
            }

            item_def *weap = you.slot_item(EQ_WEAPON, false);
            bool wielded = (weap && weap->slot == item.slot);

            if (!wielded)
            {
                ostr << getHintString("HINT_WIELD_WEAPON");

                // Weapon skill used by this weapon and the best weapon skill.
                skill_type curr_wpskill = item_attack_skill(item);
                skill_type best_wpskill;

                // Maybe this is a launching weapon?
                if (is_range_weapon(item))
                    best_wpskill = best_skill(SK_SLINGS, SK_THROWING);
                else
                    best_wpskill = best_skill(SK_SHORT_BLADES, SK_STAVES);

                // Maybe unarmed is better.
                if (you.skills[SK_UNARMED_COMBAT] > you.skills[best_wpskill])
                    best_wpskill = SK_UNARMED_COMBAT;

                if (you.skills[curr_wpskill] + 2 < you.skills[best_wpskill])
                {
                    string text = getHintString("HINT_WEAPON_SKILL");
                    text = _do_hint_replacements(text, skill_name(best_wpskill),
                                                 skill_name(curr_wpskill));
                    ostr << text;
                }
            }
            else // wielded weapon
            {
                if (is_range_weapon(item))
                {
                    ostr << getHintString("HINT_ATTACK_WITH_RANGED_WEAPON");
                    ostr << _hints_target_mode();
                }
                else
                    ostr << getHintString("HINT_ATTACK_WITH_MELEE_WEAPON");
            }

            if (!item_type_known(item)
                && (is_artefact(item)
                    || get_equip_desc(item) != ISFLAG_NO_DESC))
            {
                ostr << "\n" << getHintString("HINT_UNUSUAL_EQUIPMENT");
                Hints.hints_events[HINT_SEEN_RANDART] = false;
            }
            Hints.hints_events[HINT_SEEN_WEAPON] = false;
            break;
        }
        case OBJ_MISSILES:
            if (is_throwable(&you, item))
            {
                ostr << _do_hint_replacements(getHintString("HINT_THROWABLE"),
                                              item.name(DESC_YOUR));
                ostr << _hints_throw_stuff(item);
            }
            else if (is_launched(&you, you.weapon(), item) == launch_retval::LAUNCHED)
            {
                ostr << _do_hint_replacements(
                            getHintString("HINT_MISSILE_LAUNCHER_WIELDED"),
                            item.name(DESC_THE));
                ostr << _hints_target_mode();
            }
            else
            {
                ostr << _do_hint_replacements(
                            getHintString("HINT_MISSILE_LAUNCHER_NOT_WIELDED"),
                            item.name(DESC_THE));
            }
            Hints.hints_events[HINT_SEEN_MISSILES] = false;
            break;

        case OBJ_ARMOUR:
        {
            bool wearable = true;
            if (you.get_innate_mutation_level(MUT_HORNS) > 0
                && is_hard_helmet(item))
            {
                ostr << getHintString("HINT_CANNOT_WEAR_HELMET");
                wearable = false;
            }
            else if (item.sub_type == ARM_BARDING)
            {
                ostr << getHintString("HINT_CANNOT_WEAR_BARDING");
                wearable = false;
            }
            else
                ostr << getHintString("HINT_EQUIP_UNEQUIP_ARMOUR");

            if (Hints.hints_type == HINT_MAGIC_CHAR
                && get_armour_slot(item) == EQ_BODY_ARMOUR
                && !is_effectively_light_armour(&item))
            {
                ostr << "\n" << getHintString("HINT_ARMOUR_HINDERS_CASTING");
            }
            else if (Hints.hints_type == HINT_MAGIC_CHAR
                     && is_shield(item))
            {
                ostr << "\n" << getHintString("HINT_SHIELD_HINDERS_CASTING");
            }
            else if (Hints.hints_type == HINT_RANGER_CHAR
                     && is_shield(item))
            {
                ostr << "\n";
                ostr << getHintString("HINT_NO_SHIELD_WITH_MOST_LAUNCHERS");
            }

            if (!item_type_known(item)
                && (is_artefact(item)
                    || get_equip_desc(item) != ISFLAG_NO_DESC))
            {
                ostr << "\n" << getHintString("HINT_UNUSUAL_EQUIPMENT");
                Hints.hints_events[HINT_SEEN_RANDART] = false;
            }
            if (wearable)
            {
                if (gives_resistance(item))
                    ostr << "\n" << getHintString("HINT_ARMOUR_HAS_RESIST");
                if (gives_ability(item))
                {
                    ostr << "\n" << getHintString("HINT_ARMOUR_HAS_ABILITY");
                    ostr << _hints_abilities(item);
                }
            }
            Hints.hints_events[HINT_SEEN_ARMOUR] = false;
            break;
        }
        case OBJ_WANDS:
            ostr << getHintString("HINT_DESCRIBE_WAND");
            Hints.hints_events[HINT_SEEN_WAND] = false;
            break;

        case OBJ_SCROLLS:
            ostr << getHintString("HINT_DESCRIBE_SCROLL");
            Hints.hints_events[HINT_SEEN_SCROLL] = false;
            break;

        case OBJ_JEWELLERY:
        {
            ostr << getHintString("HINT_DESCRIBE_JEWELLERY");

            if (gives_resistance(item))
                ostr << getHintString("HINT_JEWELLERY_HAS_RESIST");
            if (gives_ability(item))
            {
                ostr << getHintString("HINT_JEWELLERY_HAS_ABILITY");
                ostr << _hints_abilities(item);
            }
            Hints.hints_events[HINT_SEEN_JEWELLERY] = false;
            break;
        }
        case OBJ_POTIONS:
            ostr << getHintString("HINT_DESCRIBE_POTION");
            Hints.hints_events[HINT_SEEN_POTION] = false;
            break;

        case OBJ_BOOKS:
            if (item.sub_type == BOOK_MANUAL)
                ostr << getHintString("HINT_DESCRIBE_MANUAL");
            else // It's a spellbook!
                ostr << getHintString("HINT_DESCRIBE_SPELLBOOK");
            Hints.hints_events[HINT_SEEN_SPBOOK] = false;
            break;

        case OBJ_CORPSES:
            Hints.hints_events[HINT_SEEN_CARRION] = false;
                ostr << getHintString("HINT_DESCRIBE_CORPSE");
            break;

       case OBJ_STAVES:
            ostr << getHintString("HINT_DESCRIBE_MAGICAL_STAFF");

            if (gives_resistance(item))
                ostr << getHintString("HINT_MAGICAL_STAFF_HAS_RESISTANCE");
            else if (you_worship(GOD_TROG))
                ostr << getHintString("HINT_MAGICAL_STAFF_WITH_TROG");
            Hints.hints_events[HINT_SEEN_STAFF] = false;
            break;

        case OBJ_MISCELLANY:
            ostr << getHintString("HINT_DESCRIBE_MISCELLANEOUS_ITEM");
            Hints.hints_events[HINT_SEEN_MISC] = false;
            break;

        default:
            return "";
    }

    ostr << "</" << colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) << ">";
    string broken = untag_tiles_console(ostr.str());
    _replace_static_tags(broken);
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
            + _do_hint_replacements(ostr.str())
            + "</" + colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) + ">";
}

static void _hints_describe_feature(int x, int y, ostringstream& ostr)
{
    const dungeon_feature_type feat = env.grid[x][y];
    const coord_def            where(x, y);

    if (!ostr.str().empty())
        ostr << "\n";

    switch (feat)
    {
    case DNGN_ORCISH_IDOL:
    case DNGN_GRANITE_STATUE:
        ostr << getHintString("HINT_DESCRIBE_STATUE");
        break;

    case DNGN_TRAP_TELEPORT:
    case DNGN_TRAP_TELEPORT_PERMANENT:
    case DNGN_TRAP_ALARM:
    case DNGN_TRAP_ZOT:
#if TAG_MAJOR_VERSION == 34
    case DNGN_TRAP_MECHANICAL:
#endif
    case DNGN_TRAP_ARROW:
    case DNGN_TRAP_SPEAR:
    case DNGN_TRAP_BLADE:
    case DNGN_TRAP_DART:
    case DNGN_TRAP_BOLT:
    case DNGN_TRAP_NET:
    case DNGN_TRAP_PLATE:
        ostr << getHintString("HINT_DESCRIBE_TRAP_PLATE");
        Hints.hints_events[HINT_SEEN_TRAP] = false;
        break;

    case DNGN_TRAP_SHAFT:
        ostr << getHintString("HINT_DESCRIBE_TRAP_SHAFT");
        Hints.hints_events[HINT_SEEN_TRAP] = false;
        break;

    case DNGN_TRAP_WEB:
        ostr << getHintString("HINT_DESCRIBE_TRAP_WEB");
        Hints.hints_events[HINT_SEEN_WEB] = false;
        break;

    case DNGN_STONE_STAIRS_DOWN_I:
    case DNGN_STONE_STAIRS_DOWN_II:
    case DNGN_STONE_STAIRS_DOWN_III:
        ostr << getHintString("HINT_DESCRIBE_DOWNSTAIRS");

        if (is_unknown_stair(where))
            ostr << getHintString("HINT_DESCRIBE_UNKNOWN_STAIRS");

        Hints.hints_events[HINT_SEEN_STAIRS] = false;
        break;

    case DNGN_EXIT_DUNGEON:
        ostr << getHintString("HINT_DESCRIBE_DUNGEON_EXIT");
        break;

    case DNGN_STONE_STAIRS_UP_I:
    case DNGN_STONE_STAIRS_UP_II:
    case DNGN_STONE_STAIRS_UP_III:
        ostr << getHintString("HINT_DESCRIBE_UPSTAIRS");
        if (is_unknown_stair(where))
            ostr << getHintString("HINT_DESCRIBE_UNKNOWN_STAIRS");
        Hints.hints_events[HINT_SEEN_STAIRS] = false;
        break;

    case DNGN_ESCAPE_HATCH_DOWN:
    case DNGN_ESCAPE_HATCH_UP:
        ostr << getHintString("HINT_DESCRIBE_ESCAPE_HATCH");

        Hints.hints_events[HINT_SEEN_ESCAPE_HATCH] = false;
        break;

#if TAG_MAJOR_VERSION == 34
    case DNGN_ENTER_PORTAL_VAULT:
        _describe_portal(where);
        Hints.hints_events[HINT_SEEN_PORTAL] = false;
        break;
#endif

    case DNGN_CLOSED_DOOR:
    case DNGN_CLOSED_CLEAR_DOOR:
        if (!Hints.hints_explored)
            ostr << getHintString("HINT_DESCRIBE_CLOSED_DOOR");
        break;

    default:
        if (feat_is_altar(feat))
        {
            god_type altar_god = feat_altar_god(feat);

            // TODO: mention Gozag here?
            if (you_worship(GOD_NO_GOD))
            {
                ostr << getHintString("HINT_DESCRIBE_ALTAR_ATHEIST");
                if (altar_god == GOD_ECUMENICAL)
                    ostr << getHintString("HINT_DESCRIBE_FADED_ALTAR_ATHEIST");
                else
                {
                    ostr << _do_hint_replacements(
                                getHintString("HINT_DESCRIBE_NONFADED_ALTAR_ATHEIST"),
                                god_name(altar_god));
                }
            }
            else if (you_worship(altar_god))
            {
                // If we don't have anything to say, return early.
                return;
            }
            else if (altar_god == GOD_ECUMENICAL)
            {
                ostr << _do_hint_replacements(
                            getHintString("HINT_DESCRIBE_FADED_ALTAR"),
                            god_name(you.religion));
            }
            else
            {
                ostr << _do_hint_replacements(
                            getHintString("HINT_DESCRIBE_FADED_ALTAR"),
                            god_name(you.religion), god_name(altar_god));
            }
            Hints.hints_events[HINT_SEEN_ALTAR] = false;
            break;
        }
        else if (feat_is_branch_entrance(feat))
        {
            ostr << getHintString("HINT_DESCRIBE_BRANCH_ENTRY");
            if (feat != DNGN_ENTER_TEMPLE)
                ostr << getHintString("HINT_DESCRIBE_BRANCH_ENTRY_DANGEROUS");
            break;
        }
    }
}

static void _hints_describe_cloud(int x, int y, ostringstream& ostr)
{
    cloud_struct* cloud = cloud_at(coord_def(x, y));
    if (!cloud)
        return;

    const string cname = "the " + cloud->cloud_name(true);
    const cloud_type ctype = cloud->type;

    if (!ostr.str().empty())
        ostr << "\n\n";

    bool plural = ends_with(cname, "s");
    string text;
    if (is_harmless_cloud(ctype))
    {
        text = getHintString(plural ? "HINT_DESCRIBE_HARMLESS_CLOUD_PLURAL"
                                    : "HINT_DESCRIBE_HARMLESS_CLOUD_SINGULAR");
    }
    else if (is_damaging_cloud(ctype, true))
    {
        text = getHintString(plural ? "HINT_DESCRIBE_HARMFUL_CLOUD_PLURAL"
                                    : "HINT_DESCRIBE_HARMFUL_CLOUD_SINGULAR");
    }
    else
    {
        text = getHintString(plural
                           ? "HINT_DESCRIBE_CURRENTLY_HARMLESS_CLOUD_PLURAL"
                           : "HINT_DESCRIBE_CURRENTLY_HARMLESS_CLOUD_SINGULAR");
    }
    ostr << uppercase_first(_do_hint_replacements(text, cname));

    if (is_opaque_cloud(ctype))
        ostr << getHintString("HINT_DESCRIBE_OPAQUE_CLOUD");
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
        ostr << getHintString("HINT_DESCRIBE_UNIQUE_MONSTER") << "\n";
        dangerous = true;
    }
    else if (mi.type == MONS_PLAYER_GHOST)
    {
        ostr << getHintString("HINT_DESCRIBE_PLAYER_GHOST") << "\n";
        dangerous = true;
    }
    else
    {
        const int tier = mons_demon_tier(mi.type);
        if (tier > 0)
        {
            if (tier == 1)
                ostr << getHintString("HINT_DESCRIBE_TIER_1_DEMON");
            else if (tier == 2)
                ostr << getHintString("HINT_DESCRIBE_TIER_2_DEMON");
            else if (tier == 3)
                ostr << getHintString("HINT_DESCRIBE_TIER_3_DEMON");
            else if (tier == 4)
                ostr << getHintString("HINT_DESCRIBE_TIER_4_DEMON");
            else if (tier == 5)
                ostr << getHintString("HINT_DESCRIBE_TIER_5_DEMON");
            else
                ostr << "This monster is a demon of the buggy tier.\n";
            ostr << "\n";
        }

        // Don't call friendly monsters dangerous.
        if (!mons_att_wont_attack(mi.attitude))
        {
            if (mi.threat == MTHRT_NASTY)
            {
                ostr << getHintString("HINT_DESCRIBE_NASTY_MONSTER");
                dangerous = true;
            }
            else if (mi.threat == MTHRT_TOUGH)
            {
                ostr << getHintString("HINT_DESCRIBE_TOUGH_MONSTER");
                dangerous = true;
            }
        }
    }

    if (mi.is(MB_BERSERK))
    {
        ostr << getHintString("HINT_DESCRIBE_BERSERKED_MONSTER") << "\n";
        dangerous = true;
    }

    // Monster is highlighted.
    if (mi.attitude == ATT_FRIENDLY)
    {
        ostr << getHintString("HINT_DESCRIBE_FRIENDLY_MONSTER");

        if (!mons_att_wont_attack(mi.attitude))
        {
            ostr << "\n"
                 << getHintString("HINT_DESCRIBE_TEMPORARILY_FRIENDLY_MONSTER");
        }
    }
    else if (dangerous)
    {
        if (!Hints.hints_explored && (mi.is(MB_WANDERING) || mi.is(MB_UNAWARE)))
            ostr << getHintString("HINT_DESCRIBE_UNAWARE_MONSTER");
        else
        {
            if (you_worship(GOD_TROG) && you.can_go_berserk())
                ostr << getHintString("HINT_RUN_AWAY_OR_BERSERK");
            else
                ostr << getHintString("HINT_RUN_AWAY");
        }
    }
    else if (Options.stab_brand != CHATTR_NORMAL
             && mi.is(MB_STABBABLE))
    {
        ostr << getHintString("HINT_DESCRIBE_STABBABLE_MONSTER");
    }
    else if (Options.may_stab_brand != CHATTR_NORMAL
             && mi.is(MB_DISTRACTED))
    {
        ostr << getHintString("HINT_DESCRIBE_DISTRACTED_MONSTER");
    }

    if (!dangerous && !has_stat_desc)
        ostr << "\n" << getHintString("HINT_DESCRIBE_UNREMARKABLE_MONSTER");

    if (ostr.str().empty())
        return "";
    string text = ostr.str();
    _replace_static_tags(text);
    return "\n<" + colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) + ">"
            + text
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

        if (Options.feature_item_brand != CHATTR_NORMAL
            && (is_feature('>', gc) || is_feature('<', gc)))
        {
            learned_something_new(HINT_STAIR_BRAND, gc);
        }
        else if (Options.trap_item_brand != CHATTR_NORMAL
                 && is_feature('^', gc))
        {
            learned_something_new(HINT_TRAP_BRAND, gc);
        }
        else if (Options.heap_brand != CHATTR_NORMAL && item.link != NON_ITEM)
            learned_something_new(HINT_HEAP_BRAND, gc);
    }
}

void tutorial_msg(const char *key, bool end)
{
    string text = getHintString(key);
    if (text.empty())
        return mprf(MSGCH_ERROR, "Error, no message for '%s'.", key);

    _replace_static_tags(text);
    text = untag_tiles_console(text);

    if (end)
        screen_end_game(text);

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
