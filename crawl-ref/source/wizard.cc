/**
 * @file
 * @brief Wizard mode command handling.
**/
#ifdef WIZARD
#include "AppHdr.h"

#include "wizard.h"

#include "abyss.h" // banished
#include "acquire.h"
#include "cio.h" // cursor_control
#include "clua.h"
#include "command.h" // show_keyhelp_menu
#include "dbg-util.h"
#include "dgn-shoals.h" // wizard_mod_tide
#include "files.h" // save_game
#include "god-companions.h" // wizard_list_companions
#include "god-passive.h" // jiyva_eat_offlevel_items
#include "hiscores.h"
#include "items.h"
#include "luaterp.h" // debug_terp_lua
#include "macro.h"
#include "menu.h" // column_composer
#include "message.h"
#include "notes.h"
#include "output.h"
#include "player.h"
#include "prompt.h" // yes_or_no
#include "religion.h" // religion_turn_end
#include "skills.h"
#include "spl-transloc.h" // wizard_blink
#include "stairs.h" // down_stairs
#include "state.h"
#include "timed-effects.h" // change_labyrinth
#include "wizard-option-type.h"
#include "wiz-dgn.h"
#include "wiz-dump.h"
#include "wiz-fsim.h"
#include "wiz-item.h"
#include "wiz-mon.h"
#include "wiz-you.h"
#include "xom.h" // debug_xom_effects

static void _do_wizard_command(int wiz_command)
{
    ASSERT(you.wizard);

    switch (wiz_command)
    {
    case '?':
    {
        const int key = list_wizard_commands(true);
        _do_wizard_command(key);
        return;
    }

    case 'a': acquirement(OBJ_RANDOM, AQ_WIZMODE); break;
    case 'A': wizard_set_all_skills(); break;
    case CONTROL('A'):
        if (player_in_branch(BRANCH_ABYSS))
            wizard_set_abyss();
        else
            mpr("You can only abyss_teleport() inside the Abyss.");
        break;

    case 'b': wizard_blink(); break;
    case 'B': you.teleport(true, true); break;
    case CONTROL('B'):
        if (!player_in_branch(BRANCH_ABYSS))
            banished("wizard command");
        else
            down_stairs(DNGN_EXIT_ABYSS);
        break;

    case 'c': wizard_draw_card(); break;
    case 'C': wizard_uncurse_item(); break;
    case CONTROL('C'): die("Intentional crash");

    case 'd': wizard_level_travel(true); break;
    case 'D': wizard_detect_creatures(); break;
    case CONTROL('D'): wizard_edit_durations(); break;

    case 'e': wizard_set_hunger_state(); break;
    case 'E': wizard_freeze_time(); break;
    case CONTROL('E'): debug_dump_levgen(); break;

    case 'f': wizard_quick_fsim(); break;
    case 'F': wizard_fight_sim(false); break;
    case CONTROL('F'): wizard_fight_sim(true); break;

    case 'g': wizard_exercise_skill(); break;
    case 'G': wizard_dismiss_all_monsters(); break;
#ifdef DEBUG_BONES
    case CONTROL('G'): debug_ghosts(); break;
#endif

    case 'h': wizard_heal(false); break;
    case 'H': wizard_heal(true); break;
    // case CONTROL('H'): break;

    case 'i': wizard_identify_pack(); break;
    case 'I': wizard_unidentify_pack(); break;
    case CONTROL('I'): debug_item_statistics(); break;

    // case 'j': break;
    case 'J':
        mpr("Running Jiyva off-level sacrifice.");
        jiyva_eat_offlevel_items();
        break;
    // case CONTROL('J'): break;

    case 'k': wizard_set_xl(true); break;
    case 'K':
        if (player_in_branch(BRANCH_LABYRINTH))
            change_labyrinth(true);
        else
            mpr("이 명령은 미궁 안에서만 유효하다!");
        break;
    case CONTROL('K'): wizard_clear_used_vaults(); break;

    case 'l': wizard_set_xl(); break;
    case 'L': debug_place_map(false); break;
    // case CONTROL('L'): break;

    case 'M':
    case 'm': wizard_create_spec_monster_name(); break;
    // case CONTROL('M'): break; // XXX do not use, menu command

    // case 'n': break;
    // case 'N': break;
    // case CONTROL('N'): break;

    case 'o': wizard_create_spec_object(); break;
    case 'O': debug_test_explore(); break;
    // case CONTROL('O'): break;

    case 'p': wizard_transform(); break;
    case 'P': debug_place_map(true); break;
    case CONTROL('P'): wizard_list_props(); break;

    // case 'q': break;
    // case 'Q': break;
    case CONTROL('Q'): wizard_toggle_dprf(); break;

    case 'r': wizard_change_species(); break;
    case 'R': wizard_spawn_control(); break;
    case CONTROL('R'): wizard_recreate_level(); break;

    case 's':
    case 'S': wizard_set_skill_level(); break;
    case CONTROL('S'): wizard_abyss_speed(); break;

    case 't': wizard_tweak_object(); break;
    case 'T': debug_make_trap(); break;
    case CONTROL('T'): debug_terp_dlua(); break;

    case 'u': wizard_level_travel(false); break;
    // case 'U': break;
    case CONTROL('U'): debug_terp_dlua(clua); break;

    case 'v': wizard_recharge_evokers(); break;
    case 'V': wizard_toggle_xray_vision(); break;
    case CONTROL('V'): wizard_value_item(); break;

    case 'w': wizard_god_mollify(); break;
    case 'W': wizard_god_wrath(); break;
    case CONTROL('W'): wizard_mod_tide(); break;

    case 'x':
        you.experience = 1 + exp_needed(1 + you.experience_level);
        level_change();
        break;
    case 'X': wizard_xom_acts(); break;
    case CONTROL('X'): debug_xom_effects(); break;

    case 'y': wizard_identify_all_items(); break;
    case 'Y': wizard_unidentify_all_items(); break;
    case CONTROL('Y'): wizard_suppress(); break;

    case 'z': wizard_cast_spec_spell(); break;
    // case 'Z': break;
    // case CONTROL('Z'): break;

    case '!': wizard_memorise_spec_spell(); break;
    case '@': wizard_set_stats(); break;
    case '#': wizard_load_dump_file(); break;
    case '$': wizard_set_gold(); break;
    case '%': wizard_create_spec_object_by_name(); break;
    case '^': wizard_set_piety(); break;
    case '&': wizard_list_companions(); break;
    // case '*': break; // XXX do not use, this is the alternate control prefix
    case '(': wizard_create_feature(); break;
    // case ')': break;

    // case '`': break;
    case '~': wizard_interlevel_travel(); break;

    case '-': wizard_get_god_gift(); break;
    case '_': wizard_join_religion(); break;

    case '=':
        mprf("Cost level: %d  Total experience: %d  Next cost level: %d Skill cost: %d",
              you.skill_cost_level, you.total_experience,
              skill_cost_needed(you.skill_cost_level + 1),
              calc_skill_cost(you.skill_cost_level));
        break;
    case '+': wizard_make_object_randart(); break;

    // case '[': break;
    case '{': wizard_map_level(); break;

    case ']':
        if (!wizard_add_mutation())
            mpr("돌연변이를 부여하는 데 실패했다.");
        break;
    case '}': wizard_reveal_traps(); break;

    case '\\': debug_make_shop(); break;
    case '|': wizard_create_all_artefacts(); break;

    case ';': wizard_list_levels(); break;
    case ':': wizard_list_branches(); break;

    case '\'': wizard_list_items(); break;
    case '"': debug_list_monsters(); break;

    case ',': wizard_place_stairs(false); break;
    // case '<': break; // XXX do not use, menu command

    case '.': wizard_place_stairs(true); break;
    // case '>': break; // XXX do not use, menu command

    // case '/': break;

    case ' ':
    case '\r':
    case '\n':
    case ESCAPE:
        canned_msg(MSG_OK);
        break;
    default:
        formatted_mpr(formatted_string::parse_string(
                          "Not a <magenta>Wizard</magenta> Command."));
        break;
    }
    // Force the placement of any delayed monster gifts.
    you.turn_is_over = true;
    religion_turn_end();

    you.turn_is_over = false;
}

static void _log_wizmode_entrance()
{
    scorefile_entry se(INSTANT_DEATH, MID_NOBODY, KILLED_BY_WIZMODE, nullptr);
    logfile_new_entry(se);
}

/// Prompt for and perform a wizard-mode command. If we are not already
// in wizard mode, prompt first.
void handle_wizard_command()
{
    int wiz_command;

    // WIZ_NEVER gives protection for those who have wiz compiles,
    // and don't want to risk their characters. Also, and hackishly,
    // it's used to prevent access for non-authorised users to wizard
    // builds in dgamelaunch builds unless the game is started with the
    // -wizard flag.
    if (Options.wiz_mode == WIZ_NEVER)
        return;

    if (you.suppress_wizard)
    {
        mprf(MSGCH_WARN, "Re-activating wizard mode.");
        you.wizard = true;
        you.suppress_wizard = false;
        redraw_screen();
        if (crawl_state.cmd_repeat_start)
        {
            crawl_state.cancel_cmd_repeat("Can't repeat re-activating wizard "
                                          "mode.");
        }
        return;
    }
    else if (!you.wizard)
    {
        mprf(MSGCH_WARN, "WARNING: ABOUT TO ENTER WIZARD MODE!");

#ifndef SCORE_WIZARD_CHARACTERS
        if (!you.explore)
            mprf(MSGCH_WARN, "If you continue, your game will not be scored!");
#endif

        if (!yes_or_no("Do you really want to enter wizard mode?"))
        {
            canned_msg(MSG_OK);
            return;
        }

        take_note(Note(NOTE_MESSAGE, 0, 0, "Entered wizard mode."));

#ifndef SCORE_WIZARD_CHARACTERS
        if (!you.explore)
            _log_wizmode_entrance();
#endif

        you.wizard = true;
        save_game(false);
        redraw_screen();

        if (crawl_state.cmd_repeat_start)
        {
            crawl_state.cancel_cmd_repeat("Can't repeat entering wizard "
                                          "mode.");
            return;
        }
    }

    {
        mprf(MSGCH_PROMPT, "Enter Wizard Command (? - help): ");
        cursor_control con(true);
        wiz_command = getchm();
        if (wiz_command == '*')
            wiz_command = CONTROL(toupper(getchm()));
    }

    if (crawl_state.cmd_repeat_start)
    {
        // Easiest to list which wizard commands *can* be repeated.
        switch (wiz_command)
        {
        case 'x':
        case '$':
        case 'a':
        case 'c':
        case 'h':
        case 'H':
        case 'm':
        case 'M':
        case 'X':
        case ']':
        case '^':
        case '%':
        case 'o':
        case 'z':
        case CONTROL('Z'):
            break;

        default:
            crawl_state.cant_cmd_repeat("You cannot repeat that "
                                        "wizard command.");
            return;
        }
    }

    _do_wizard_command(wiz_command);
}

void enter_explore_mode()
{
    // WIZ_NEVER gives protection for those who have wiz compiles,
    // and don't want to risk their characters. Also, and hackishly,
    // it's used to prevent access for non-authorised users to wizard
    // builds in dgamelaunch builds unless the game is started with the
    // -wizard flag.
    if (Options.explore_mode == WIZ_NEVER)
        return;

    if (you.wizard || you.suppress_wizard)
        handle_wizard_command();
    else if (!you.explore)
    {
        mprf(MSGCH_WARN, "WARNING: ABOUT TO ENTER EXPLORE MODE!");

#ifndef SCORE_WIZARD_CHARACTERS
        mprf(MSGCH_WARN, "If you continue, your game will not be scored!");
#endif

        if (!yes_or_no("Do you really want to enter explore mode?"))
        {
            canned_msg(MSG_OK);
            return;
        }

        take_note(Note(NOTE_MESSAGE, 0, 0, "Entered explore mode."));

#ifndef SCORE_WIZARD_CHARACTERS
        _log_wizmode_entrance();
#endif

        you.explore = true;
        save_game(false);
        redraw_screen();

        if (crawl_state.cmd_repeat_start)
        {
            crawl_state.cancel_cmd_repeat("Can't repeat entering explore mode");
            return;
        }
    }
}

int list_wizard_commands(bool do_redraw_screen)
{
    // 2 columns
    column_composer cols(2, 44);
    // Page size is number of lines - one line for --more-- prompt.
    cols.set_pagesize(get_number_of_lines());

    cols.add_formatted(0,
                       "<yellow>Player stats</yellow>\n"
                       "<w>A</w>      set all skills to level\n"
                       "<w>Ctrl-D</w> change enchantments/durations\n"
                       "<w>g</w>      exercise a skill\n"
                       "<w>k</w>      change experience level and skills\n"
                       "<w>l</w>      change experience level\n"
                       "<w>Ctrl-P</w> list props\n"
                       "<w>r</w>      change character's species\n"
                       "<w>s</w>      set skill to level\n"
                       "<w>x</w>      gain an experience level\n"
                       "<w>$</w>      set gold to a specified value\n"
                       "<w>]</w>      get a mutation\n"
                       "<w>_</w>      gain religion\n"
                       "<w>^</w>      set piety to a value\n"
                       "<w>@</w>      set Str Int Dex\n"
                       "<w>#</w>      load character from a dump file\n"
                       "<w>&</w>      list all divine followers\n"
                       "<w>=</w>      show info about skill points\n"
                       "\n"
                       "<yellow>Create level features</yellow>\n"
                       "<w>L</w>      place a vault by name\n"
                       "<w>T</w>      make a trap\n"
                       "<w>,</w>/<w>.</w>    create up/down staircase\n"
                       "<w>(</w>      turn cell into feature\n"
                       "<w>\\</w>      make a shop\n"
                       "<w>Ctrl-K</w> mark all vaults as unused\n"
                       "\n"
                       "<yellow>Other level related commands</yellow>\n"
                       "<w>Ctrl-A</w> generate new Abyss area\n"
                       "<w>b</w>      controlled blink\n"
                       "<w>B</w>      controlled teleport\n"
                       "<w>Ctrl-B</w> banish yourself to the Abyss\n"
                       "<w>K</w>      shift section of a labyrinth\n"
                       "<w>R</w>      change monster spawn rate\n"
                       "<w>Ctrl-S</w> change Abyss speed\n"
                       "<w>u</w>/<w>d</w>    shift up/down one level\n"
                       "<w>~</w>      go to a specific level\n"
                       "<w>:</w>      find branches and overflow\n"
                       "       temples in the dungeon\n"
                       "<w>;</w>      list known levels and counters\n"
                       "<w>{</w>      magic mapping\n"
                       "<w>}</w>      detect all traps on level\n"
                       "<w>Ctrl-W</w> change Shoals' tide speed\n"
                       "<w>Ctrl-E</w> dump level builder information\n"
                       "<w>Ctrl-R</w> regenerate current level\n"
                       "<w>P</w>      create a level based on a vault\n",
                       true);

    cols.add_formatted(1,
                       "<yellow>Other player related effects</yellow>\n"
                       "<w>c</w>      card effect\n"
#ifdef DEBUG_BONES
                       "<w>Ctrl-G</w> save/load ghost (bones file)\n"
#endif
                       "<w>h</w>/<w>H</w>    heal yourself (super-Heal)\n"
                       "<w>e</w>      set hunger state\n"
                       "<w>X</w>      make Xom do something now\n"
                       "<w>z</w>      cast spell by number/name\n"
                       "<w>!</w>      memorise spell\n"
                       "<w>W</w>      god wrath\n"
                       "<w>w</w>      god mollification\n"
                       "<w>p</w>      polymorph into a form\n"
                       "<w>V</w>      toggle xray vision\n"
                       "<w>E</w>      (un)freeze time\n"
                       "\n"
                       "<yellow>Monster related commands</yellow>\n"
                       "<w>m</w>/<w>M</w>    create specified monster\n"
                       "<w>D</w>      detect all monsters\n"
                       "<w>G</w>      dismiss all monsters\n"
                       "<w>\"</w>      list monsters\n"
                       "\n"
                       "<yellow>Item related commands</yellow>\n"
                       "<w>a</w>      acquirement\n"
                       "<w>C</w>      (un)curse item\n"
                       "<w>i</w>/<w>I</w>    identify/unidentify inventory\n"
                       "<w>y</w>/<w>Y</w>    id/unid item types+level items\n"
                       "<w>o</w>/<w>%</w>    create an object\n"
                       "<w>t</w>      tweak object properties\n"
                       "<w>v</w>      recharge all XP evokers\n"
                       "<w>Ctrl-V</w> show gold value of an item\n"
                       "<w>-</w>      get a god gift\n"
                       "<w>|</w>      create all unrand artefacts\n"
                       "<w>+</w>      make randart from item\n"
                       "<w>'</w>      list items\n"
                       "<w>J</w>      Jiyva off-level sacrifice\n"
                       "\n"
                       "<yellow>Debugging commands</yellow>\n"
                       "<w>f</w>      quick fight simulation\n"
                       "<w>F</w>      single scale fsim\n"
                       "<w>Ctrl-F</w> double scale fsim\n"
                       "<w>Ctrl-I</w> item generation stats\n"
                       "<w>O</w>      measure exploration time\n"
                       "<w>Ctrl-T</w> dungeon (D)Lua interpreter\n"
                       "<w>Ctrl-U</w> client (C)Lua interpreter\n"
                       "<w>Ctrl-X</w> Xom effect stats\n"
#ifdef DEBUG_DIAGNOSTICS
                       "<w>Ctrl-Q</w> make some debug messages quiet\n"
#endif
                       "<w>Ctrl-Y</w> temporarily suppress wizmode\n"
                       "<w>Ctrl-C</w> force a crash\n"
                       "\n"
                       "<yellow>Other wizard commands</yellow>\n"
                       "(not prefixed with <w>&</w>!)\n"
                       "<w>x?</w>     list targeted commands\n"
                       "<w>X?</w>     list map-mode commands\n",
                       true);

    int key = show_keyhelp_menu(cols.formatted_lines(), false,
                                Options.easy_exit_menu);
    if (do_redraw_screen)
        redraw_screen();
    return key;
}
#endif // defined(WIZARD)
