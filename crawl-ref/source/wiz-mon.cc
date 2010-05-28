/*
 *  File:       dbg-mon.cc
 *  Summary:    Monster related debugging functions.
 *  Written by: Linley Henzell and Jesse Jones
 */

#include "AppHdr.h"

#include "wiz-mon.h"

#include "areas.h"
#include "cio.h"
#include "colour.h"
#include "coord.h"
#include "dbg-util.h"
#include "delay.h"
#include "dungeon.h"
#include "env.h"
#include "files.h"
#include "ghost.h"
#include "goditem.h"
#include "invent.h"
#include "items.h"
#include "jobs.h"
#include "macro.h"
#include "map_knowledge.h"
#include "message.h"
#include "mon-place.h"
#include "terrain.h"
#include "mgen_data.h"
#include "mapdef.h"
#include "mon-pathfind.h"
#include "mon-speak.h"
#include "mon-stuff.h"
#include "mon-iter.h"
#include "mon-util.h"
#include "output.h"
#include "religion.h"
#include "shout.h"
#include "spells2.h"
#include "spl-mis.h"
#include "spl-util.h"
#include "stuff.h"
#include "view.h"
#include "viewmap.h"

#ifdef WIZARD
// Creates a specific monster by mon type number.
void wizard_create_spec_monster(void)
{
    int mon = debug_prompt_for_int( "Create which monster by number? ", true );

    if (mon == -1 || (mon >= NUM_MONSTERS
                      && mon != RANDOM_MONSTER
                      && mon != RANDOM_DRACONIAN
                      && mon != RANDOM_BASE_DRACONIAN
                      && mon != RANDOM_NONBASE_DRACONIAN
                      && mon != WANDERING_MONSTER))
    {
        canned_msg( MSG_OK );
    }
    else
    {
        create_monster(
            mgen_data::sleeper_at(
                static_cast<monster_type>(mon), you.pos()));
    }
}

// Creates a specific monster by name. Uses the same patterns as
// map definitions.
void wizard_create_spec_monster_name()
{
    char specs[100];
    mpr("Which monster by name? ", MSGCH_PROMPT);
    if (cancelable_get_line_autohist(specs, sizeof specs) || !*specs)
    {
        canned_msg(MSG_OK);
        return;
    }

    mons_list mlist;
    std::string err = mlist.add_mons(specs);

    if (!err.empty())
    {
        std::string newerr;
        // Try for a partial match, but not if the user accidently entered
        // only a few letters.
        monster_type partial = get_monster_by_name(specs);
        if (strlen(specs) >= 3 && partial != NON_MONSTER)
        {
            mlist.clear();
            newerr = mlist.add_mons(mons_type_name(partial, DESC_PLAIN));
        }

        if (!newerr.empty())
        {
            mpr(err.c_str());
            return;
        }
    }

    mons_spec mspec = mlist.get_monster(0);
    if (mspec.mid == -1)
    {
        mpr("Such a monster couldn't be found.", MSGCH_DIAGNOSTICS);
        return;
    }

    int type = mspec.mid;
    if (mons_class_is_zombified(mspec.mid))
        type = mspec.monbase;

    coord_def place = find_newmons_square(type, you.pos());
    if (!in_bounds(place))
    {
        // Try again with habitat HT_LAND.
        // (Will be changed to the necessary terrain type in dgn_place_monster.)
        place = find_newmons_square(MONS_NO_MONSTER, you.pos());
    }

    if (!in_bounds(place))
    {
        mpr("Found no space to place monster.", MSGCH_DIAGNOSTICS);
        return;
    }

    // Wizmode users should be able to conjure up uniques even if they
    // were already created. Yay, you can meet 3 Sigmunds at once! :p
    if (mons_is_unique(mspec.mid) && you.unique_creatures[mspec.mid])
        you.unique_creatures[mspec.mid] = false;

    if (dgn_place_monster(mspec, you.absdepth0, place, true, false) == -1)
    {
        mpr("Unable to place monster.", MSGCH_DIAGNOSTICS);
        return;
    }

    if (mspec.mid == MONS_KRAKEN)
    {
        unsigned short mid = mgrd(place);

        if (mid >= MAX_MONSTERS || menv[mid].type != MONS_KRAKEN)
        {
            for (mid = 0; mid < MAX_MONSTERS; mid++)
            {
                if (menv[mid].type == MONS_KRAKEN && menv[mid].alive())
                {
                    menv[mid].colour = element_colour(ETC_KRAKEN);
                    return;
                }
            }
        }
        if (mid >= MAX_MONSTERS)
        {
            mpr("Couldn't find player kraken!");
            return;
        }
    }

    // FIXME: This is a bit useless, seeing how you cannot set the
    // ghost's stats, brand or level.
    if (mspec.mid == MONS_PLAYER_GHOST)
    {
        unsigned short mid = mgrd(place);

        if (mid >= MAX_MONSTERS || menv[mid].type != MONS_PLAYER_GHOST)
        {
            for (mid = 0; mid < MAX_MONSTERS; mid++)
            {
                if (menv[mid].type == MONS_PLAYER_GHOST
                    && menv[mid].alive())
                {
                    break;
                }
            }
        }

        if (mid >= MAX_MONSTERS)
        {
            mpr("Couldn't find player ghost, probably going to crash.");
            more();
            return;
        }

        monsters    &mon = menv[mid];
        ghost_demon ghost;

        ghost.name = "John Doe";

        char input_str[80];
        msgwin_get_line("Make player ghost which species? (case-sensitive) ",
                        input_str, sizeof(input_str));

        species_type sp_id = get_species_by_abbrev(input_str);
        if (sp_id == SP_UNKNOWN)
            sp_id = str_to_species(input_str);
        if (sp_id == SP_UNKNOWN)
        {
            mpr("No such species, making it Human.");
            sp_id = SP_HUMAN;
        }
        ghost.species = static_cast<species_type>(sp_id);

        msgwin_get_line("Give player ghost which background? ",
                        input_str, sizeof(input_str));

        int job_id = get_job_by_abbrev(input_str);

        if (job_id == JOB_UNKNOWN)
            job_id = get_job_by_name(input_str);

        if (job_id == JOB_UNKNOWN)
        {
            mpr("No such background, making it a Fighter.");
            job_id = JOB_FIGHTER;
        }
        ghost.job = static_cast<job_type>(job_id);
        ghost.xl = 7;

        mon.set_ghost(ghost);

        ghosts.push_back(ghost);
    }
}

static bool _sort_monster_list(int a, int b)
{
    const monsters* m1 = &menv[a];
    const monsters* m2 = &menv[b];

    if (m1->alive() != m2->alive())
        return m1->alive();
    else if (!m1->alive())
        return a < b;

    if (m1->type == m2->type)
    {
        if (!m1->alive() || !m2->alive())
            return (false);

        return ( m1->name(DESC_PLAIN, true) < m2->name(DESC_PLAIN, true) );
    }

    const unsigned glyph1 = mons_char(m1->type);
    const unsigned glyph2 = mons_char(m2->type);
    if (glyph1 != glyph2)
    {
        return (glyph1 < glyph2);
    }

    return (m1->type < m2->type);
}

void debug_list_monsters()
{
    std::vector<std::string> mons;
    int nfound = 0;

    int mon_nums[MAX_MONSTERS];

    for (int i = 0; i < MAX_MONSTERS; ++i)
        mon_nums[i] = i;

    std::sort(mon_nums, mon_nums + MAX_MONSTERS, _sort_monster_list);

    long total_exp = 0, total_adj_exp = 0, total_nonuniq_exp = 0;

    std::string prev_name = "";
    int         count     = 0;

    for (int i = 0; i < MAX_MONSTERS; ++i)
    {
        const int idx = mon_nums[i];
        if (invalid_monster_index(idx))
            continue;

        const monsters *mi(&menv[idx]);
        if (!mi->alive())
            continue;

        std::string name = mi->name(DESC_PLAIN, true);

        if (prev_name != name && count > 0)
        {
            char buf[80];
            if (count > 1)
                snprintf(buf, sizeof(buf), "%d %s", count,
                         pluralise(prev_name).c_str());
            else
                snprintf(buf, sizeof(buf), "%s", prev_name.c_str());
            mons.push_back(buf);

            count = 0;
        }
        nfound++;
        count++;
        prev_name = name;

        int exp = exper_value(mi);
        total_exp += exp;
        if (!mons_is_unique(mi->type))
            total_nonuniq_exp += exp;

        if ((mi->flags & (MF_WAS_NEUTRAL | MF_NO_REWARD))
            || mi->has_ench(ENCH_ABJ))
        {
            continue;
        }
        if (mi->flags & MF_GOT_HALF_XP)
            exp /= 2;

        total_adj_exp += exp;
    }

    char buf[80];
    if (count > 1)
        snprintf(buf, sizeof(buf), "%d %s", count, pluralise(prev_name).c_str());
    else
        snprintf(buf, sizeof(buf), "%s", prev_name.c_str());
    mons.push_back(buf);

    mpr_comma_separated_list("Monsters: ", mons);

    if (total_adj_exp == total_exp)
    {
        mprf("%d monsters, %ld total exp value (%ld non-uniq)",
             nfound, total_exp, total_nonuniq_exp);
    }
    else
    {
        mprf("%d monsters, %ld total exp value (%ld non-uniq, %ld adjusted)",
             nfound, total_exp, total_nonuniq_exp, total_adj_exp);
    }
}

void wizard_spawn_control()
{
    mpr("(c)hange spawn rate or (s)pawn monsters? ", MSGCH_PROMPT);
    const int c = tolower(getchm());

    char specs[256];
    bool done = false;

    if (c == 'c')
    {
        mprf(MSGCH_PROMPT, "Set monster spawn rate to what? (now %d, lower value = higher rate) ",
             env.spawn_random_rate);

        if (!cancelable_get_line(specs, sizeof(specs)))
        {
            const int rate = atoi(specs);
            if (rate)
            {
                env.spawn_random_rate = rate;
                done = true;
            }
        }
    }
    else if (c == 's')
    {
        // 50 spots are reserved for non-wandering monsters.
        int max_spawn = MAX_MONSTERS - 50;
        for (monster_iterator mi; mi; ++mi)
            if (mi->alive())

        if (max_spawn <= 0)
        {
            mpr("Level already filled with monsters, get rid of some "
                "of them first.", MSGCH_PROMPT);
            return;
        }

        mprf(MSGCH_PROMPT, "Spawn how many random monsters (max %d)? ",
             max_spawn);

        if (!cancelable_get_line(specs, sizeof(specs)))
        {
            const int num = std::min(atoi(specs), max_spawn);
            if (num > 0)
            {
                int curr_rate = env.spawn_random_rate;
                // Each call to spawn_random_monsters() will spawn one with
                // the rate at 5 or less.
                env.spawn_random_rate = 5;

                for (int i = 0; i < num; ++i)
                    spawn_random_monsters();

                env.spawn_random_rate = curr_rate;
                done = true;
            }
        }
    }

    if (!done)
        canned_msg(MSG_OK);
}

// Prints a number of useful (for debugging, that is) stats on monsters.
void debug_stethoscope(int mon)
{
    dist stth;
    coord_def stethpos;

    int i;

    if (mon != RANDOM_MONSTER)
        i = mon;
    else
    {
        mpr("Which monster?", MSGCH_PROMPT);

        direction(stth, direction_chooser_args());

        if (!stth.isValid)
            return;

        if (stth.isTarget)
            stethpos = stth.target;
        else
            stethpos = you.pos() + stth.delta;

        if (env.cgrid(stethpos) != EMPTY_CLOUD)
        {
            mprf(MSGCH_DIAGNOSTICS, "cloud type: %d delay: %d",
                 env.cloud[ env.cgrid(stethpos) ].type,
                 env.cloud[ env.cgrid(stethpos) ].decay );
        }

        if (!monster_at(stethpos))
        {
            mprf(MSGCH_DIAGNOSTICS, "item grid = %d", igrd(stethpos) );
            return;
        }

        i = mgrd(stethpos);
    }

    monsters& mons(menv[i]);

    // Print type of monster.
    mprf(MSGCH_DIAGNOSTICS, "%s (id #%d; type=%d loc=(%d,%d) align=%s)",
         mons.name(DESC_CAP_THE, true).c_str(),
         i, mons.type, mons.pos().x, mons.pos().y,
         ((mons.attitude == ATT_HOSTILE)        ? "hostile" :
          (mons.attitude == ATT_FRIENDLY)       ? "friendly" :
          (mons.attitude == ATT_NEUTRAL)        ? "neutral" :
          (mons.attitude == ATT_GOOD_NEUTRAL)   ? "good neutral":
          (mons.attitude == ATT_STRICT_NEUTRAL) ? "strictly neutral"
                                                : "unknown alignment") );

    // Print stats and other info.
    mprf(MSGCH_DIAGNOSTICS,
         "HD=%d (%lu) HP=%d/%d AC=%d EV=%d MR=%d SP=%d "
         "energy=%d%s%s num=%d flags=%04lx",
         mons.hit_dice,
         mons.experience,
         mons.hit_points, mons.max_hit_points,
         mons.ac, mons.ev,
         mons.res_magic(),
         mons.speed, mons.speed_increment,
         mons.base_monster != MONS_NO_MONSTER ? " base=" : "",
         mons.base_monster != MONS_NO_MONSTER ?
         get_monster_data(mons.base_monster)->name : "",
         mons.number, mons.flags );

    // Print habitat and behaviour information.
    const habitat_type hab = mons_habitat(&mons);

    mprf(MSGCH_DIAGNOSTICS,
         "hab=%s beh=%s(%d) foe=%s(%d) mem=%d target=(%d,%d) god=%s",
         ((hab == HT_LAND)                       ? "land" :
          (hab == HT_AMPHIBIOUS)                 ? "amphibious" :
          (hab == HT_WATER)                      ? "water" :
          (hab == HT_LAVA)                       ? "lava" :
          (hab == HT_ROCK)                       ? "rock"
                                                 : "unknown"),
         (mons.asleep()        ? "sleep" :
          mons_is_wandering(&mons)       ? "wander" :
          mons_is_seeking(&mons)         ? "seek" :
          mons_is_fleeing(&mons)         ? "flee" :
          mons_is_cornered(&mons)        ? "cornered" :
          mons_is_panicking(&mons)       ? "panic" :
          mons_is_lurking(&mons)         ? "lurk"
                                         : "unknown"),
         mons.behaviour,
         ((mons.foe == MHITYOU)                    ? "you" :
          (mons.foe == MHITNOT)                    ? "none" :
          (menv[mons.foe].type == MONS_NO_MONSTER) ? "unassigned monster"
          : menv[mons.foe].name(DESC_PLAIN, true).c_str()),
         mons.foe,
         mons.foe_memory,
         mons.target.x, mons.target.y,
         god_name(mons.god).c_str() );

    // Print resistances.
    mprf(MSGCH_DIAGNOSTICS, "resist: fire=%d cold=%d elec=%d pois=%d neg=%d "
                            "acid=%d sticky=%s rot=%s",
         mons.res_fire(),
         mons.res_cold(),
         mons.res_elec(),
         mons.res_poison(),
         mons.res_negative_energy(),
         mons.res_acid(),
         mons.res_sticky_flame() ? "yes" : "no",
         mons.res_rotting() ? "yes" : "no");

    mprf(MSGCH_DIAGNOSTICS, "ench: %s",
         mons.describe_enchantments().c_str());

    std::ostringstream spl;
    const monster_spells &hspell_pass = mons.spells;
    bool found_spell = false;
    for (int k = 0; k < NUM_MONSTER_SPELL_SLOTS; ++k)
    {
        if (hspell_pass[k] != SPELL_NO_SPELL)
        {
            if (found_spell)
                spl << ", ";

            found_spell = true;

            spl << k << ": ";

            if (hspell_pass[k] >= NUM_SPELLS)
                spl << "buggy spell";
            else
                spl << spell_title(hspell_pass[k]);

            spl << " (" << static_cast<int>(hspell_pass[k]) << ")";
        }
    }
    if (found_spell)
        mprf(MSGCH_DIAGNOSTICS, "spells: %s", spl.str().c_str());

    if (mons_is_ghost_demon(mons.type))
    {
        ASSERT(mons.ghost.get());
        const ghost_demon &ghost = *mons.ghost;
        mprf(MSGCH_DIAGNOSTICS, "Ghost damage: %d; brand: %d; att_type: %d; "
                                "att_flav: %d",
             ghost.damage, ghost.brand, ghost.att_type, ghost.att_flav);
    }
}

// Detects all monsters on the level, using their exact positions.
void wizard_detect_creatures()
{
    const int prev_detected = count_detected_mons();
    const int num_creatures = detect_creatures(60, true);

    if (!num_creatures)
        mpr("You detect nothing.");
    else if (num_creatures == prev_detected)
        mpr("You detect no further creatures.");
    else
        mpr("You detect creatures!");
}

// Dismisses all monsters on the level or all monsters that match a user
// specified regex.
void wizard_dismiss_all_monsters(bool force_all)
{
    char buf[80] = "";
    if (!force_all)
    {
        mpr("Regex of monsters to dismiss (ENTER for all): ", MSGCH_PROMPT);
        bool validline = !cancelable_get_line_autohist(buf, sizeof buf);

        if (!validline)
        {
            canned_msg( MSG_OK );
            return;
        }
    }

    dismiss_monsters(buf);
    // If it was turned off turn autopickup back on if all monsters went away.
    if (!*buf)
        autotoggle_autopickup(false);
}

void debug_make_monster_shout(monsters* mon)
{
    mpr("Make the monster (S)hout or (T)alk? ", MSGCH_PROMPT);

    char type = (char) getchm(KMC_DEFAULT);
    type = tolower(type);

    if (type != 's' && type != 't')
    {
        canned_msg( MSG_OK );
        return;
    }

    int num_times = debug_prompt_for_int("How many times? ", false);

    if (num_times <= 0)
    {
        canned_msg( MSG_OK );
        return;
    }

    if (type == 's')
    {
        if (silenced(you.pos()))
            mpr("You are silenced and likely won't hear any shouts.");
        else if (silenced(mon->pos()))
            mpr("The monster is silenced and likely won't give any shouts.");

        for (int i = 0; i < num_times; ++i)
            force_monster_shout(mon);
    }
    else
    {
        if (mon->invisible())
            mpr("The monster is invisible and likely won't speak.");

        if (silenced(you.pos()) && !silenced(mon->pos()))
        {
            mpr("You are silenced but the monster isn't; you will "
                "probably hear/see nothing.");
        }
        else if (!silenced(you.pos()) && silenced(mon->pos()))
            mpr("The monster is silenced and likely won't say anything.");
        else if (silenced(you.pos()) && silenced(mon->pos()))
        {
            mpr("Both you and the monster are silenced, so you likely "
                "won't hear anything.");
        }

        for (int i = 0; i< num_times; ++i)
            mons_speaks(mon);
    }

    mpr("== Done ==");
}

static bool _force_suitable(const monsters *mon)
{
    return (mon->alive());
}

void wizard_apply_monster_blessing(monsters* mon)
{
    mpr("Apply blessing of (B)eogh, The (S)hining One, or (R)andomly? ",
        MSGCH_PROMPT);

    char type = (char) getchm(KMC_DEFAULT);
    type = tolower(type);

    if (type != 'b' && type != 's' && type != 'r')
    {
        canned_msg( MSG_OK );
        return;
    }
    god_type god = GOD_NO_GOD;
    if (type == 'b' || type == 'r' && coinflip())
        god = GOD_BEOGH;
    else
        god = GOD_SHINING_ONE;

    if (!bless_follower(mon, god, _force_suitable, true))
        mprf("%s won't bless this monster for you!", god_name(god).c_str());
}

void wizard_give_monster_item(monsters *mon)
{
    mon_itemuse_type item_use = mons_itemuse(mon);
    if (item_use < MONUSE_STARTING_EQUIPMENT)
    {
        mpr("That type of monster can't use any items.");
        return;
    }

    int player_slot = prompt_invent_item( "Give which item to monster?",
                                          MT_DROP, -1 );

    if (player_slot == PROMPT_ABORT)
        return;

    for (int i = 0; i < NUM_EQUIP; ++i)
        if (you.equip[i] == player_slot)
        {
            mpr("Can't give equipped items to a monster.");
            return;
        }

    item_def     &item = you.inv[player_slot];
    mon_inv_type mon_slot = NUM_MONSTER_SLOTS;

    switch (item.base_type)
    {
    case OBJ_WEAPONS:
        // Let wizard specify which slot to put weapon into via
        // inscriptions.
        if (item.inscription.find("first") != std::string::npos
            || item.inscription.find("primary") != std::string::npos)
        {
            mpr("Putting weapon into primary slot by inscription");
            mon_slot = MSLOT_WEAPON;
            break;
        }
        else if (item.inscription.find("second") != std::string::npos
                 || item.inscription.find("alt") != std::string::npos)
        {
            mpr("Putting weapon into alt slot by inscription");
            mon_slot = MSLOT_ALT_WEAPON;
            break;
        }

        // For monsters which can wield two weapons, prefer whichever
        // slot is empty (if there is an empty slot).
        if (mons_wields_two_weapons(mon))
        {
            if (mon->inv[MSLOT_WEAPON] == NON_ITEM)
            {
                mpr("Dual wielding monster, putting into empty primary slot");
                mon_slot = MSLOT_WEAPON;
                break;
            }
            else if (mon->inv[MSLOT_ALT_WEAPON] == NON_ITEM)
            {
                mpr("Dual wielding monster, putting into empty alt slot");
                mon_slot = MSLOT_ALT_WEAPON;
                break;
            }
        }

        // Try to replace a ranged weapon with a ranged weapon and
        // a non-ranged weapon with a non-ranged weapon
        if (mon->inv[MSLOT_WEAPON] != NON_ITEM
            && (is_range_weapon(mitm[mon->inv[MSLOT_WEAPON]])
                == is_range_weapon(item)))
        {
            mpr("Replacing primary slot with similar weapon");
            mon_slot = MSLOT_WEAPON;
            break;
        }
        if (mon->inv[MSLOT_ALT_WEAPON] != NON_ITEM
            && (is_range_weapon(mitm[mon->inv[MSLOT_ALT_WEAPON]])
                == is_range_weapon(item)))
        {
            mpr("Replacing alt slot with similar weapon");
            mon_slot = MSLOT_ALT_WEAPON;
            break;
        }

        // Prefer the empty slot (if any)
        if (mon->inv[MSLOT_WEAPON] == NON_ITEM)
        {
            mpr("Putting weapon into empty primary slot");
            mon_slot = MSLOT_WEAPON;
            break;
        }
        else if (mon->inv[MSLOT_ALT_WEAPON] == NON_ITEM)
        {
            mpr("Putting weapon into empty alt slot");
            mon_slot = MSLOT_ALT_WEAPON;
            break;
        }

        // Default to primary weapon slot
        mpr("Defaulting to primary slot");
        mon_slot = MSLOT_WEAPON;
        break;

    case OBJ_ARMOUR:
    {
        // May only return shield or armour slot.
        equipment_type eq = get_armour_slot(item);

        // Force non-shield, non-body armour to be worn anyway.
        if (eq == EQ_NONE)
            eq = EQ_BODY_ARMOUR;

        mon_slot = equip_slot_to_mslot(eq);
        break;
    }
    case OBJ_MISSILES:
        mon_slot = MSLOT_MISSILE;
        break;
    case OBJ_WANDS:
        mon_slot = MSLOT_WAND;
        break;
    case OBJ_SCROLLS:
        mon_slot = MSLOT_SCROLL;
        break;
    case OBJ_POTIONS:
        mon_slot = MSLOT_POTION;
        break;
    case OBJ_MISCELLANY:
        mon_slot = MSLOT_MISCELLANY;
        break;
    default:
        mpr("You can't give that type of item to a monster.");
        return;
    }

    // Shouldn't we be using MONUSE_MAGIC_ITEMS?
    if (item_use == MONUSE_STARTING_EQUIPMENT
        && !mons_is_unique(mon->type))
    {
        switch (mon_slot)
        {
        case MSLOT_WEAPON:
        case MSLOT_ALT_WEAPON:
        case MSLOT_ARMOUR:
        case MSLOT_MISSILE:
            break;

        default:
            mpr("That type of monster can only use weapons and armour.");
            return;
        }
    }

    int index = get_item_slot(10);
    if (index == NON_ITEM)
    {
        mpr("Too many items on level, bailing.");
        return;
    }

    // Move monster's old item to player's inventory as last step.
    int  old_eq     = NON_ITEM;
    bool unequipped = false;
    if (mon_slot != NUM_MONSTER_SLOTS
        && mon->inv[mon_slot] != NON_ITEM
        && !items_stack(item, mitm[mon->inv[mon_slot]]))
    {
        old_eq = mon->inv[mon_slot];
        // Alternative weapons don't get (un)wielded unless the monster
        // can wield two weapons.
        if (mon_slot != MSLOT_ALT_WEAPON || mons_wields_two_weapons(mon))
        {
            mon->unequip(*(mon->mslot_item(mon_slot)), mon_slot, 1, true);
            unequipped = true;
        }
        mon->inv[mon_slot] = NON_ITEM;
    }

    mitm[index] = item;

    unwind_var<int> save_speedinc(mon->speed_increment);
    if (!mon->pickup_item(mitm[index], false, true))
    {
        mpr("Monster wouldn't take item.");
        if (old_eq != NON_ITEM && mon_slot != NUM_MONSTER_SLOTS)
        {
            mon->inv[mon_slot] = old_eq;
            if (unequipped)
                mon->equip(mitm[old_eq], mon_slot, 1);
        }
        unlink_item(index);
        destroy_item(item);
        return;
    }

    // Item is gone from player's inventory.
    dec_inv_item_quantity(player_slot, item.quantity);

    if ((mon->flags & MF_HARD_RESET) && !(item.flags & ISFLAG_SUMMONED))
    {
       mprf(MSGCH_WARN, "WARNING: Monster has MF_HARD_RESET and all its "
            "items will disappear when it does.");
    }
    else if ((item.flags & ISFLAG_SUMMONED) && !mon->is_summoned())
    {
       mprf(MSGCH_WARN, "WARNING: Item is summoned and will disappear when "
            "the monster does.");
    }
    // Monster's old item moves to player's inventory.
    if (old_eq != NON_ITEM)
    {
        mpr("Fetching monster's old item.");
        if (mitm[old_eq].flags & ISFLAG_SUMMONED)
        {
           mprf(MSGCH_WARN, "WARNING: Item is summoned and shouldn't really "
                "be anywhere but in the inventory of a summoned monster.");
        }
        mitm[old_eq].pos.reset();
        mitm[old_eq].link = NON_ITEM;
        move_item_to_player(old_eq, mitm[old_eq].quantity);
    }
}

static void _move_player(const coord_def& where)
{
    if (!you.can_pass_through_feat(grd(where)))
        grd(where) = DNGN_FLOOR;
    move_player_to_grid(where, false, true, true);
}

static void _move_monster(const coord_def& where, int mid1)
{
    dist moves;
    direction_chooser_args args;
    args.needs_path = false;
    args.top_prompt = "Move monster to where?";
    direction(moves, args);

    if (!moves.isValid || !in_bounds(moves.target))
        return;

    monsters* mon1 = &menv[mid1];

    const int mid2 = mgrd(moves.target);
    monsters* mon2 = monster_at(moves.target);

    mon1->moveto(moves.target);
    mgrd(moves.target) = mid1;
    mon1->check_redraw(moves.target);

    mgrd(where) = mid2;

    if (mon2 != NULL)
    {
        mon2->moveto(where);
        mon1->check_redraw(where);
    }
}

void wizard_move_player_or_monster(const coord_def& where)
{
    crawl_state.cancel_cmd_again();
    crawl_state.cancel_cmd_repeat();

    static bool already_moving = false;

    if (already_moving)
    {
        mpr("Already doing a move command.");
        return;
    }

    already_moving = true;

    int mid = mgrd(where);

    if (mid == NON_MONSTER)
    {
        if (crawl_state.arena_suspended)
        {
            mpr("You can't move yourself into the arena.");
            more();
            return;
        }
        _move_player(where);
    }
    else
        _move_monster(where, mid);

    already_moving = false;
}

void wizard_make_monster_summoned(monsters* mon)
{
    int summon_type = 0;
    if (mon->is_summoned(NULL, &summon_type) || summon_type != 0)
    {
        mpr("Monster is already summoned.", MSGCH_PROMPT);
        return;
    }

    int dur = debug_prompt_for_int("What summon longevity (1 to 6)? ", true);

    if (dur < 1 || dur > 6)
    {
        canned_msg( MSG_OK );
        return;
    }

    mpr("[a] clone [b] animated [c] chaos [d] miscast [e] zot", MSGCH_PROMPT);
    mpr("[f] wrath [g] aid                [m] misc    [s] spell",
        MSGCH_PROMPT);

    mpr("Which summon type? ", MSGCH_PROMPT);

    char choice = tolower(getchm());

    if (!(choice >= 'a' && choice <= 'g') && choice != 'm' && choice != 's')
    {
        canned_msg( MSG_OK );
        return;
    }

    int type = 0;

    switch (choice)
    {
        case 'a': type = MON_SUMM_CLONE; break;
        case 'b': type = MON_SUMM_ANIMATE; break;
        case 'c': type = MON_SUMM_CHAOS; break;
        case 'd': type = MON_SUMM_MISCAST; break;
        case 'e': type = MON_SUMM_ZOT; break;
        case 'f': type = MON_SUMM_WRATH; break;
        case 'g': type = MON_SUMM_AID; break;
        case 'm': type = 0; break;

        case 's':
        {
            char specs[80];

            msgwin_get_line("Cast which spell by name? ",
                            specs, sizeof(specs));

            if (specs[0] == '\0')
            {
                canned_msg( MSG_OK );
                return;
            }

            spell_type spell = spell_by_name(specs, true);
            if (spell == SPELL_NO_SPELL)
            {
                mpr("No such spell.", MSGCH_PROMPT);
                return;
            }
            type = (int) spell;
            break;
        }

        default:
            DEBUGSTR("Invalid summon type choice.");
            break;
    }

    mon->mark_summoned(dur, true, type);
    mpr("Monster is now summoned.");
}

void wizard_polymorph_monster(monsters* mon)
{
    monster_type old_type =  mon->type;
    monster_type type     = debug_prompt_for_monster();

    if (type == NUM_MONSTERS)
    {
        canned_msg( MSG_OK );
        return;
    }

    if (invalid_monster_type(type))
    {
        mpr("Invalid monster type.", MSGCH_PROMPT);
        return;
    }

    if (type == old_type)
    {
        mpr("Old type and new type are the same, not polymorphing.");
        return;
    }

    if (mons_species(type) == mons_species(old_type))
    {
        mpr("Target species must be different from current species.");
        return;
    }

    monster_polymorph(mon, type, PPT_SAME, true);

    if (!mon->alive())
    {
        mpr("Polymorph killed monster?", MSGCH_ERROR);
        return;
    }

    mon->check_redraw(mon->pos());

    if (mon->type == old_type)
        mpr("Polymorph failed.");
    else if (mon->type != type)
        mpr("Monster turned into something other than the desired type.");
}

void debug_pathfind(int mid)
{
    if (mid == NON_MONSTER)
        return;

    mpr("Choose a destination!");
#ifndef USE_TILE
    more();
#endif
    coord_def dest;
    level_pos ldest;
    bool chose = show_map(ldest, false, true, false);
    dest = ldest.pos;
    redraw_screen();
    if (!chose)
    {
        canned_msg(MSG_OK);
        return;
    }

    monsters &mon = menv[mid];
    mprf("Attempting to calculate a path from (%d, %d) to (%d, %d)...",
         mon.pos().x, mon.pos().y, dest.x, dest.y);
    monster_pathfind mp;
    bool success = mp.init_pathfind(&mon, dest, true, true);
    if (success)
    {
        std::vector<coord_def> path = mp.backtrack();
        std::string path_str;
        mpr("Here's the shortest path: ");
        for (unsigned int i = 0; i < path.size(); ++i)
        {
            snprintf(info, INFO_SIZE, "(%d, %d)  ", path[i].x, path[i].y);
            path_str += info;
        }
        mpr(path_str.c_str());
        mprf("-> path length: %d", path.size());

        mpr("");
        path = mp.calc_waypoints();
        path_str = "";
        mpr("");
        mpr("And here are the needed waypoints: ");
        for (unsigned int i = 0; i < path.size(); ++i)
        {
            snprintf(info, INFO_SIZE, "(%d, %d)  ", path[i].x, path[i].y);
            path_str += info;
        }
        mpr(path_str.c_str());
        mprf("-> #waypoints: %d", path.size());
    }
}

static void _miscast_screen_update()
{
    viewwindow(false);

    you.redraw_status_flags =
        REDRAW_LINE_1_MASK | REDRAW_LINE_2_MASK | REDRAW_LINE_3_MASK;
    print_stats();

#ifndef USE_TILE
    update_monster_pane();
#endif
}

void debug_miscast( int target_index )
{
    crawl_state.cancel_cmd_repeat();

    actor* target;
    if (target_index == NON_MONSTER)
        target = &you;
    else
        target = &menv[target_index];

    if (!target->alive())
    {
        mpr("Can't make already dead target miscast.");
        return;
    }

    char specs[100];
    mpr("Miscast which school or spell, by name? ", MSGCH_PROMPT);
    if (cancelable_get_line_autohist(specs, sizeof specs) || !*specs)
    {
        canned_msg(MSG_OK);
        return;
    }

    spell_type         spell  = spell_by_name(specs, true);
    spschool_flag_type school = school_by_name(specs);

    // Prefer exact matches for school name over partial matches for
    // spell name.
    if (school != SPTYP_NONE
        && (strcasecmp(specs, spelltype_short_name(school)) == 0
            || strcasecmp(specs, spelltype_long_name(school)) == 0))
    {
        spell = SPELL_NO_SPELL;
    }

    if (spell == SPELL_NO_SPELL && school == SPTYP_NONE)
    {
        mpr("No matching spell or spell school.");
        return;
    }
    else if (spell != SPELL_NO_SPELL && school != SPTYP_NONE)
    {
        mprf("Ambiguous: can be spell '%s' or school '%s'.",
            spell_title(spell), spelltype_short_name(school));
        return;
    }

    int disciplines = 0;
    if (spell != SPELL_NO_SPELL)
    {
        disciplines = get_spell_disciplines(spell);

        if (disciplines == 0)
        {
            mprf("Spell '%s' has no disciplines.", spell_title(spell));
            return;
        }
    }

    if (is_holy_spell(spell))
    {
        mpr("Can't miscast holy spells.");
        return;
    }

    if (spell != SPELL_NO_SPELL)
        mprf("Miscasting spell %s.", spell_title(spell));
    else
        mprf("Miscasting school %s.", spelltype_long_name(school));

    if (spell != SPELL_NO_SPELL)
        mpr("Enter spell_power,spell_failure: ", MSGCH_PROMPT );
    else
    {
        mpr("Enter miscast_level or spell_power,spell_failure: ",
             MSGCH_PROMPT);
    }

    if (cancelable_get_line_autohist(specs, sizeof specs) || !*specs)
    {
        canned_msg(MSG_OK);
        return;
    }

    int level = -1, pow = -1, fail = -1;

    if (strchr(specs, ','))
    {
        std::vector<std::string> nums = split_string(",", specs);
        pow  = atoi(nums[0].c_str());
        fail = atoi(nums[1].c_str());

        if (pow <= 0 || fail <= 0)
        {
            canned_msg(MSG_OK);
            return;
        }
    }
    else
    {
        if (spell != SPELL_NO_SPELL)
        {
            mpr("Can only enter fixed miscast level for schools, not spells.");
            return;
        }

        level = atoi(specs);
        if (level < 0)
        {
            canned_msg(MSG_OK);
            return;
        }
        else if (level > 3)
        {
            mpr("Miscast level can be at most 3.");
            return;
        }
    }

    // Handle repeats ourselves since miscasts are likely to interrupt
    // command repetions, especially if the player is the target.
    int repeats = debug_prompt_for_int("Number of repetitions? ", true);
    if (repeats < 1)
    {
        canned_msg(MSG_OK);
        return;
    }

    // Supress "nothing happens" message for monster miscasts which are
    // only harmless messages, since a large number of those are missing
    // monster messages.
    nothing_happens_when_type nothing = NH_DEFAULT;
    if (target_index != NON_MONSTER && level == 0)
        nothing = NH_NEVER;

    MiscastEffect *miscast;

    if (spell != SPELL_NO_SPELL)
    {
        miscast = new MiscastEffect(target, target_index, spell, pow, fail,
                                    "", nothing);
    }
    else
    {
        if (level != -1)
        {
            miscast = new MiscastEffect(target, target_index, school,
                                        level, "wizard testing miscast",
                                        nothing);
        }
        else
        {
            miscast = new MiscastEffect(target, target_index, school,
                                        pow, fail, "wizard testing miscast",
                                        nothing);
        }
    }
    // Merely creating the miscast object causes one miscast effect to
    // happen.
    repeats--;
    if (level != 0)
        _miscast_screen_update();

    while (target->alive() && repeats-- > 0)
    {
        if (kbhit())
        {
            mpr("Key pressed, interrupting miscast testing.");
            getchm();
            break;
        }

        miscast->do_miscast();
        if (level != 0)
            _miscast_screen_update();
    }

    delete miscast;
}

void debug_ghosts()
{
    mpr("(C)reate or (L)oad bones file?", MSGCH_PROMPT);
    const char c = tolower(getchm());

    if (c == 'c')
        save_ghost(true);
    else if (c == 'l')
        load_ghost(false);
    else
        canned_msg(MSG_OK);
}

#endif
