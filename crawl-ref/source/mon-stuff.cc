/**
 * @file
 * @brief Misc monster related functions.
**/

#include "AppHdr.h"
#include <math.h>
#include "mon-stuff.h"

#include "act-iter.h"
#include "areas.h"
#include "arena.h"
#include "art-enum.h"
#include "artefact.h"
#include "attitude-change.h"
#include "cloud.h"
#include "cluautil.h"
#include "coordit.h"
#include "database.h"
#include "delay.h"
#include "dactions.h"
#include "describe.h"
#include "dgnevent.h"
#include "dgn-overview.h"
#include "dlua.h"
#include "dungeon.h"
#include "effects.h"
#include "env.h"
#include "exclude.h"
#include "fprop.h"
#include "files.h"
#include "food.h"
#include "godabil.h"
#include "godcompanions.h"
#include "godconduct.h"
#include "hints.h"
#include "hiscores.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "kills.h"
#include "libutil.h"
#include "losglobal.h"
#include "makeitem.h"
#include "mapmark.h"
#include "message.h"
#include "mgen_data.h"
#include "misc.h"
#include "mon-abil.h"
#include "mon-behv.h"
#include "mon-death.h"
#include "mon-place.h"
#include "mon-speak.h"
#include "notes.h"
#include "ouch.h"
#include "player.h"
#include "random.h"
#include "religion.h"
#include "shout.h"
#include "spl-damage.h"
#include "spl-miscast.h"
#include "spl-summoning.h"
#include "spl-util.h"
#include "state.h"
#include "stuff.h"
#include "target.h"
#include "teleport.h"
#include "terrain.h"
#include "transform.h"
#include "traps.h"
#include "unwind.h"
#include "view.h"
#include "viewchar.h"
#include "xom.h"

#include <vector>
#include <algorithm>

static bool _wounded_damaged(mon_holy_type holi);
static int _calc_player_experience(const monster* mons);

dungeon_feature_type get_mimic_feat(const monster* mimic)
{
    if (mimic->props.exists("feat_type"))
        return static_cast<dungeon_feature_type>(mimic->props["feat_type"].get_short());
    else
        return DNGN_FLOOR;
}

bool feature_mimic_at(const coord_def &c)
{
    return map_masked(c, MMT_MIMIC);
}

item_def* item_mimic_at(const coord_def &c)
{
    for (stack_iterator si(c); si; ++si)
        if (si->flags & ISFLAG_MIMIC)
            return &*si;
    return NULL;
}

bool mimic_at(const coord_def &c)
{
    return feature_mimic_at(c) || item_mimic_at(c);
}

// Monster curses a random player inventory item.
bool curse_an_item(bool quiet)
{
    // allowing these would enable mummy scumming
    if (you_worship(GOD_ASHENZARI))
    {
        if (!quiet)
        {
            mprf(MSGCH_GOD, "The curse is absorbed by %s.",
                 god_name(GOD_ASHENZARI).c_str());
        }
        return false;
    }

    int count = 0;
    int item  = ENDOFPACK;

    for (int i = 0; i < ENDOFPACK; i++)
    {
        if (!you.inv[i].defined())
            continue;

        if (is_weapon(you.inv[i])
            || you.inv[i].base_type == OBJ_ARMOUR
            || you.inv[i].base_type == OBJ_JEWELLERY)
        {
            if (you.inv[i].cursed())
                continue;

            // Melded items cannot be cursed.
            if (item_is_melded(you.inv[i]))
                continue;

            // Item is valid for cursing, so we'll give it a chance.
            count++;
            if (one_chance_in(count))
                item = i;
        }
    }

    // Any item to curse?
    if (item == ENDOFPACK)
        return false;

    do_curse_item(you.inv[item], false);

    return true;
}

// The default suitable() function for monster_drop_things().
bool is_any_item(const item_def& item)
{
    return true;
}

void monster_drop_things(monster* mons,
                          bool mark_item_origins,
                          bool (*suitable)(const item_def& item),
                          int owner_id)
{
    // Drop weapons and missiles last (i.e., on top), so others pick up.
    for (int i = NUM_MONSTER_SLOTS - 1; i >= 0; --i)
    {
        int item = mons->inv[i];

        if (item != NON_ITEM && suitable(mitm[item]))
        {
            const bool summoned_item =
                testbits(mitm[item].flags, ISFLAG_SUMMONED);
            if (summoned_item)
            {
                item_was_destroyed(mitm[item], mons->mindex());
                destroy_item(item);
            }
            else
            {
                if (mons->friendly() && mitm[item].defined())
                    mitm[item].flags |= ISFLAG_DROPPED_BY_ALLY;

                if (mark_item_origins && mitm[item].defined())
                    origin_set_monster(mitm[item], mons);

                if (mitm[item].props.exists("autoinscribe"))
                {
                    add_inscription(mitm[item],
                        mitm[item].props["autoinscribe"].get_string());
                    mitm[item].props.erase("autoinscribe");
                }

                // Unrands held by fixed monsters would give awfully redundant
                // messages ("Cerebov hits you with the Sword of Cerebov."),
                // thus delay identification until drop/death.
                autoid_unrand(mitm[item]);

                // If a monster is swimming, the items are ALREADY
                // underwater.
                move_item_to_grid(&item, mons->pos(), mons->swimming());
            }

            mons->inv[i] = NON_ITEM;
        }
    }
}

// Initialises a corpse item using the given monster and monster type.
// The monster pointer is optional; you may pass in NULL to bypass
// per-monster checks.
//
// force_corpse forces creation of the corpse item even if the monster
// would not otherwise merit a corpse.
monster_type fill_out_corpse(const monster* mons,
                             monster_type mtype,
                             item_def& corpse,
                             bool force_corpse)
{
    ASSERT(!invalid_monster_type(mtype));
    corpse.clear();

    int summon_type;
    if (mons && (mons->is_summoned(NULL, &summon_type)
                    || (mons->flags & (MF_BANISHED | MF_HARD_RESET))))
    {
        return MONS_NO_MONSTER;
    }

    monster_type corpse_class = mons_species(mtype);

    if (mons)
    {
        // If this was a corpse that was temporarily animated then turn the
        // monster back into a corpse.
        if (mons_class_is_zombified(mons->type)
            && (summon_type == SPELL_ANIMATE_DEAD
                || summon_type == SPELL_ANIMATE_SKELETON
                || summon_type == MON_SUMM_ANIMATE))
        {
            corpse_class = mons_zombie_base(mons);
        }

        if (mons && mons_genus(mtype) == MONS_DRACONIAN)
        {
            if (mons->type == MONS_TIAMAT)
                corpse_class = MONS_DRACONIAN;
            else
                corpse_class = draco_subspecies(mons);
        }

        if (mons->has_ench(ENCH_GLOWING_SHAPESHIFTER))
            mtype = corpse_class = MONS_GLOWING_SHAPESHIFTER;
        else if (mons->has_ench(ENCH_SHAPESHIFTER))
            mtype = corpse_class = MONS_SHAPESHIFTER;
    }

    // Doesn't leave a corpse.
    if (!mons_class_can_leave_corpse(corpse_class) && !force_corpse)
        return MONS_NO_MONSTER;

    corpse.flags       = 0;
    corpse.base_type   = OBJ_CORPSES;
    corpse.plus        = corpse_class;
    corpse.plus2       = 0;    // butcher work done
    corpse.sub_type    = CORPSE_BODY;
    corpse.special     = FRESHEST_CORPSE;  // rot time
    corpse.quantity    = 1;
    corpse.orig_monnum = mtype;

    if (mtype == MONS_PLAGUE_SHAMBLER)
        corpse.special = ROTTING_CORPSE;

    if (mons)
    {
        corpse.props[MONSTER_HIT_DICE] = short(mons->hit_dice);
        corpse.props[MONSTER_NUMBER]   = short(mons->number);
        // XXX: Appears to be a safe conversion?
        corpse.props[MONSTER_MID]      = int(mons->mid);
    }

    corpse.colour = mons_class_colour(corpse_class);
    if (corpse.colour == BLACK)
    {
        if (mons)
            corpse.colour = mons->colour;
        else
        {
            // [ds] Ick: no easy way to get a monster's colour
            // otherwise:
            monster m;
            m.type = mtype;
            define_monster(&m);
            corpse.colour = m.colour;
        }
    }

    if (mons && !mons->mname.empty() && !(mons->flags & MF_NAME_NOCORPSE))
    {
        corpse.props[CORPSE_NAME_KEY] = mons->mname;
        corpse.props[CORPSE_NAME_TYPE_KEY].get_int64() = mons->flags;
    }
    else if (mons_is_unique(mtype))
    {
        corpse.props[CORPSE_NAME_KEY] = mons_type_name(mtype, DESC_PLAIN);
        corpse.props[CORPSE_NAME_TYPE_KEY].get_int64() = 0;
    }

    return corpse_class;
}

bool explode_corpse(item_def& corpse, const coord_def& where)
{
    // Don't want chunks to show up behind the player.
    los_def ld(where, opc_no_actor);

    if (mons_class_leaves_hide(corpse.mon_type)
        && mons_genus(corpse.mon_type) == MONS_DRAGON)
    {
        // Uh... dragon hide is tough stuff and it keeps the monster in
        // one piece?  More importantly, it prevents a flavour feature
        // from becoming a trap for the unwary.

        return false;
    }

    ld.update();

    const int max_chunks = get_max_corpse_chunks(corpse.mon_type);

    int nchunks = 1 + random2(max_chunks);
    nchunks = stepdown_value(nchunks, 4, 4, 12, 12);

    int ntries = 0;

    corpse.base_type = OBJ_FOOD;
    corpse.sub_type  = FOOD_CHUNK;
    if (is_bad_food(corpse))
        corpse.flags |= ISFLAG_DROPPED;

    int blood = nchunks * 3;

    if (food_is_rotten(corpse))
        blood /= 3;

    blood_spray(where, corpse.mon_type, blood);

    while (nchunks > 0 && ntries < 10000)
    {
        ++ntries;

        coord_def cp = where;
        cp.x += random_range(-LOS_RADIUS, LOS_RADIUS);
        cp.y += random_range(-LOS_RADIUS, LOS_RADIUS);

        dprf("Trying to scatter chunk to %d, %d...", cp.x, cp.y);

        if (!in_bounds(cp))
            continue;

        if (!ld.see_cell(cp))
            continue;

        dprf("Cell is visible...");

        if (cell_is_solid(cp) || actor_at(cp))
            continue;

        --nchunks;

        dprf("Success");

        copy_item_to_grid(corpse, cp);
    }

    return true;
}

// Returns the item slot of a generated corpse, or -1 if no corpse.
int place_monster_corpse(const monster* mons, bool silent,
                         bool force)
{
    // The game can attempt to place a corpse for an out-of-bounds monster
    // if a shifter turns into a giant spore and explodes.  In this
    // case we place no corpse since the explosion means anything left
    // over would be scattered, tiny chunks of shifter.
    if (!in_bounds(mons->pos()))
        return -1;

    // Don't attempt to place corpses within walls, either.
    // Currently, this only applies to (shapeshifter) dryads.
    if (feat_is_wall(grd(mons->pos())))
        return -1;

    // If we were told not to leave a corpse, don't.
    if (mons->props.exists("never_corpse"))
        return -1;

    item_def corpse;
    const monster_type corpse_class = fill_out_corpse(mons, mons->type,
                                                      corpse);

    bool vault_forced = false;

    // Don't place a corpse?  If a zombified monster is somehow capable
    // of leaving a corpse, then always place it.
    if (mons_class_is_zombified(mons->type))
        force = true;

    // "always_corpse" forces monsters to always generate a corpse upon
    // their deaths.
    if (mons->props.exists("always_corpse")
        || mons_class_flag(mons->type, M_ALWAYS_CORPSE))
    {
        vault_forced = true;
    }

    if (corpse_class == MONS_NO_MONSTER
        || (!force && !vault_forced && coinflip())
        || (mons_corpse_effect(corpse_class) == CE_MUTAGEN
           && !one_chance_in(3)))
    {
        return -1;
    }

    if (mons && mons_is_phoenix(mons))
        corpse.props["destroy_xp"].get_int() = _calc_player_experience(mons);

    int o = get_mitm_slot();

    // Zotdef corpse creation forces cleanup, otherwise starvation
    // kicks in. The magic number 9 is less than the magic number of
    // 10 in get_mitm_slot which indicates that a cull will be initiated
    // if a free slot can't be found.
    if (o == NON_ITEM && crawl_state.game_is_zotdef())
        o = get_mitm_slot(9);

    if (o == NON_ITEM)
    {
        item_was_destroyed(corpse);
        return -1;
    }

    mitm[o] = corpse;

    origin_set_monster(mitm[o], mons);

    if ((mons->flags & MF_EXPLODE_KILL) && explode_corpse(corpse, mons->pos()))
    {
        // We already have a spray of chunks.
        item_was_destroyed(mitm[o]);
        destroy_item(o);
        return -1;
    }

    move_item_to_grid(&o, mons->pos(), !mons->swimming());

    if (you.see_cell(mons->pos()))
    {
        if (force && !silent)
        {
            if (you.can_see(mons))
                simple_monster_message(mons, " turns back into a corpse!");
            else
            {
                mprf("%s appears out of nowhere!",
                     mitm[o].name(DESC_A).c_str());
            }
        }
        if (o != NON_ITEM && !silent)
        {
            const bool poison =
                (chunk_is_poisonous(mons_corpse_effect(corpse_class))
                 && player_res_poison() <= 0);
            hints_dissection_reminder(!poison);
        }
    }

    return o == NON_ITEM ? -1 : o;
}

static void _hints_inspect_kill()
{
    if (Hints.hints_events[HINT_KILLED_MONSTER])
        learned_something_new(HINT_KILLED_MONSTER);
}

static string _milestone_kill_verb(killer_type killer)
{
    return killer == KILL_BANISHED ? "banished" :
           killer == KILL_PACIFIED ? "pacified" :
           killer == KILL_ENSLAVED ? "enslaved" : "killed";
}

void record_monster_defeat(monster* mons, killer_type killer)
{
    if (crawl_state.game_is_arena())
        return;
    if (killer == KILL_RESET || killer == KILL_DISMISSED)
        return;
    if (mons->has_ench(ENCH_FAKE_ABJURATION))
        return;
    if (MONST_INTERESTING(mons))
    {
        take_note(Note(NOTE_DEFEAT_MONSTER, mons->type, mons->friendly(),
                       mons->full_name(DESC_A).c_str(),
                       _milestone_kill_verb(killer).c_str()));
    }
    // XXX: See comment in monster_polymorph.
    bool is_unique = mons_is_unique(mons->type);
    if (mons->props.exists("original_was_unique"))
        is_unique = mons->props["original_was_unique"].get_bool();
    if (mons->type == MONS_PLAYER_GHOST)
    {
        monster_info mi(mons);
        string milestone = _milestone_kill_verb(killer) + " the ghost of ";
        milestone += get_ghost_description(mi, true);
        milestone += ".";
        mark_milestone("ghost", milestone);
    }
    // Or summoned uniques, which a summoned ghost is treated as {due}
    else if (is_unique && !mons->is_summoned())
    {
        mark_milestone("uniq",
                       _milestone_kill_verb(killer)
                       + " "
                       + mons->name(DESC_THE, true)
                       + ".");
    }
}

static int _calc_monster_experience(monster* victim, killer_type killer,
                                    int killer_index)
{
    const int experience = exper_value(victim);
    const bool no_xp = victim->has_ench(ENCH_ABJ) || !experience || victim->has_ench(ENCH_FAKE_ABJURATION);
    const bool created_friendly = testbits(victim->flags, MF_NO_REWARD);

    if (no_xp || !MON_KILL(killer) || invalid_monster_index(killer_index))
        return 0;

    monster* mon = &menv[killer_index];
    if (!mon->alive())
        return 0;

    if ((created_friendly && mon->friendly())
        || mons_aligned(mon, victim))
    {
        return 0;
    }

    return experience;
}

static void _give_monster_experience(int experience, int killer_index)
{
    if (experience <= 0 || invalid_monster_index(killer_index))
        return;

    monster* mon = &menv[killer_index];
    if (!mon->alive())
        return;

    if (mon->gain_exp(experience))
    {
        if (!you_worship(GOD_SHINING_ONE) && !you_worship(GOD_BEOGH)
            || player_under_penance()
            || !one_chance_in(3))
        {
            return;
        }

        // Randomly bless the follower who gained experience.
        if (you_worship(GOD_SHINING_ONE)
                && random2(you.piety) >= piety_breakpoint(0)
            || you_worship(GOD_BEOGH)
                && random2(you.piety) >= piety_breakpoint(2))
        {
            bless_follower(mon);
        }
    }
}

static void _beogh_spread_experience(int exp)
{
    int total_hd = 0;

    for (monster_near_iterator mi(&you); mi; ++mi)
    {
        if (is_orcish_follower(*mi))
            total_hd += mi->hit_dice;
    }

    if (total_hd > 0)
    {
        for (monster_near_iterator mi(&you); mi; ++mi)
        {
            if (is_orcish_follower(*mi))
            {
                _give_monster_experience(exp * mi->hit_dice / total_hd,
                                         mi->mindex());
            }
        }
    }
}

static int _calc_player_experience(const monster* mons)
{
    int experience = exper_value(mons);

    const bool created_friendly = testbits(mons->flags, MF_NO_REWARD);
    const bool was_neutral = testbits(mons->flags, MF_WAS_NEUTRAL);
    const bool no_xp = mons->has_ench(ENCH_ABJ) || !experience || mons->has_ench(ENCH_FAKE_ABJURATION);
    const bool already_got_half_xp = testbits(mons->flags, MF_GOT_HALF_XP);
    const int half_xp = (experience + 1) / 2;

    // We give double exp for shedu here, rather than artificially
    // adjusting the modifier.
    if (mons->flags & MF_BAND_MEMBER && mons_is_shedu(mons))
        experience *= 2;

    if (created_friendly || was_neutral || no_xp
        || mons_is_shedu(mons) && shedu_pair_alive(mons))
    {
        return 0; // No xp if monster was created friendly or summoned.
                    // or if you've only killed one of two shedu.
    }

    if (!mons->damage_total)
    {
        mprf(MSGCH_WARN, "Error, exp for monster with no damage: %s",
             mons->name(DESC_PLAIN, true).c_str());
        return 0;
    }

    dprf("Damage ratio: %1.1f/%d (%d%%)",
         0.5 * mons->damage_friendly, mons->damage_total,
         50 * mons->damage_friendly / mons->damage_total);
    experience = (experience * mons->damage_friendly / mons->damage_total
                  + 1) / 2;
    ASSERT(mons->damage_friendly <= 2 * mons->damage_total);

    // All deaths of hostiles grant at least 50% XP in ZotDef.
    if (crawl_state.game_is_zotdef() && experience < half_xp)
        experience = half_xp;

    // Note: This doesn't happen currently since monsters with
    //       MF_GOT_HALF_XP have always gone through pacification,
    //       hence also have MF_WAS_NEUTRAL. [rob]
    if (already_got_half_xp)
    {
        experience -= half_xp;
        if (experience < 0)
            experience = 0;
    }

    return experience;
}

static void _give_player_experience(int experience, killer_type killer,
                                    bool pet_kill, bool was_visible)
{
    if (experience <= 0 || crawl_state.game_is_arena())
        return;

    unsigned int exp_gain = 0;
    gain_exp(experience, &exp_gain);

    kill_category kc =
            (killer == KILL_YOU || killer == KILL_YOU_MISSILE) ? KC_YOU :
            (pet_kill)                                         ? KC_FRIENDLY :
                                                                 KC_OTHER;
    PlaceInfo& curr_PlaceInfo = you.get_place_info();
    PlaceInfo  delta;

    delta.mon_kill_num[kc]++;
    delta.mon_kill_exp       += exp_gain;

    you.global_info += delta;
    you.global_info.assert_validity();

    curr_PlaceInfo += delta;
    curr_PlaceInfo.assert_validity();

    // Give a message for monsters dying out of sight.
    if (exp_gain > 0 && !was_visible)
        mpr("You feel a bit more experienced.");

    if (kc == KC_YOU && you_worship(GOD_BEOGH))
        _beogh_spread_experience(experience / 2);
}

static void _give_experience(int player_exp, int monster_exp,
                             killer_type killer, int killer_index,
                             bool pet_kill, bool was_visible)
{
    _give_player_experience(player_exp, killer, pet_kill, was_visible);
    _give_monster_experience(monster_exp, killer_index);
}

static bool _is_pet_kill(killer_type killer, int i)
{
    if (!MON_KILL(killer))
        return false;

    if (i == ANON_FRIENDLY_MONSTER)
        return true;

    if (invalid_monster_index(i))
        return false;

    const monster* m = &menv[i];
    if (m->friendly()) // This includes enslaved monsters.
        return true;

    // Check if the monster was confused by you or a friendly, which
    // makes casualties to this monster collateral kills.
    const mon_enchant me = m->get_ench(ENCH_CONFUSION);
    const mon_enchant me2 = m->get_ench(ENCH_INSANE);
    return me.ench == ENCH_CONFUSION
           && (me.who == KC_YOU || me.who == KC_FRIENDLY)
           || me2.ench == ENCH_INSANE
              && (me2.who == KC_YOU || me2.who == KC_FRIENDLY);
}

int exp_rate(int killer)
{
    // Damage by the spectral weapon is considered to be the player's damage ---
    // so the player does not lose any exp from dealing damage with a spectral weapon summon
    if (!invalid_monster_index(killer)
        && menv[killer].type == MONS_SPECTRAL_WEAPON
        && menv[killer].summoner == MID_PLAYER)
    {
        return 2;
    }

    if (killer == MHITYOU || killer == YOU_FAULTLESS)
        return 2;

    if (_is_pet_kill(KILL_MON, killer))
        return 1;

    return 0;
}

// Elyvilon will occasionally (5% chance) protect the life of one of
// your allies.
static bool _ely_protect_ally(monster* mons, killer_type killer)
{
    if (!you_worship(GOD_ELYVILON))
        return false;

    if (!MON_KILL(killer) && !YOU_KILL(killer))
        return false;

    if (!mons->is_holy()
            && mons->holiness() != MH_NATURAL
        || !mons->friendly()
        || !you.can_see(mons) // for simplicity
        || !one_chance_in(20))
    {
        return false;
    }

    mons->hit_points = 1;

    snprintf(info, INFO_SIZE, " protects %s from harm!",
             mons->name(DESC_THE).c_str());

    simple_god_message(info);

    return true;
}

// Elyvilon retribution effect: Heal hostile monsters that were about to
// be killed by you or one of your friends.
static bool _ely_heal_monster(monster* mons, killer_type killer, int i)
{
    god_type god = GOD_ELYVILON;

    if (!you.penance[god] || !god_hates_your_god(god))
        return false;

    const int ely_penance = you.penance[god];

    if (mons->friendly() || !one_chance_in(10))
        return false;

    if (MON_KILL(killer) && !invalid_monster_index(i))
    {
        monster* mon = &menv[i];
        if (!mon->friendly() || !one_chance_in(3))
            return false;

        if (!mons_near(mons))
            return false;
    }
    else if (!YOU_KILL(killer))
        return false;

    dprf("monster hp: %d, max hp: %d", mons->hit_points, mons->max_hit_points);

    mons->hit_points = min(1 + random2(ely_penance/3), mons->max_hit_points);

    dprf("new hp: %d, ely penance: %d", mons->hit_points, ely_penance);

    snprintf(info, INFO_SIZE, "%s heals %s%s",
             god_name(god, false).c_str(),
             mons->name(DESC_THE).c_str(),
             mons->hit_points * 2 <= mons->max_hit_points ? "." : "!");

    god_speaks(god, info);
    dec_penance(god, 1);

    return true;
}

static bool _yred_enslave_soul(monster* mons, killer_type killer)
{
    if (you_worship(GOD_YREDELEMNUL) && mons_enslaved_body_and_soul(mons)
        && mons_near(mons) && killer != KILL_RESET
        && killer != KILL_DISMISSED
        && killer != KILL_BANISHED)
    {
        record_monster_defeat(mons, killer);
        record_monster_defeat(mons, KILL_ENSLAVED);
        yred_make_enslaved_soul(mons, player_under_penance());
        return true;
    }

    return false;
}

static bool _beogh_forcibly_convert_orc(monster* mons, killer_type killer,
                                        int i)
{
    if (you_worship(GOD_BEOGH)
        && mons_genus(mons->type) == MONS_ORC
        && !mons->is_summoned() && !mons->is_shapeshifter()
        && !player_under_penance() && you.piety >= piety_breakpoint(2)
        && mons_near(mons) && !mons_is_god_gift(mons))
    {
        bool convert = false;

        if (YOU_KILL(killer))
            convert = true;
        else if (MON_KILL(killer) && !invalid_monster_index(i))
        {
            monster* mon = &menv[i];
            if (is_follower(mon) && !one_chance_in(3))
                convert = true;
        }

        // Orcs may convert to Beogh under threat of death, either from
        // you or, less often, your followers.  In both cases, the
        // checks are made against your stats.  You're the potential
        // messiah, after all.
        if (convert)
        {
#ifdef DEBUG_DIAGNOSTICS
            mprf(MSGCH_DIAGNOSTICS, "Death convert attempt on %s, HD: %d, "
                 "your xl: %d",
                 mons->name(DESC_PLAIN).c_str(),
                 mons->hit_dice,
                 you.experience_level);
#endif
            if (random2(you.piety) >= piety_breakpoint(0)
                && random2(you.experience_level) >= random2(mons->hit_dice)
                // Bias beaten-up-conversion towards the stronger orcs.
                && random2(mons->hit_dice) > 2)
            {
                beogh_convert_orc(mons, true, MON_KILL(killer));
                return true;
            }
        }
    }

    return false;
}

static bool _lost_soul_nearby(const coord_def pos)
{
    for (monster_near_iterator mi(pos, LOS_NO_TRANS); mi; ++mi)
        if (mi->type == MONS_LOST_SOUL)
            return true;

    return false;
}

static bool _monster_avoided_death(monster* mons, killer_type killer, int i)
{
    if (mons->max_hit_points <= 0 || mons->hit_dice < 1)
        return false;

    // Before the hp check since this should not care about the power of the
    // finishing blow
    if (mons->holiness() == MH_UNDEAD && !mons_is_zombified(mons)
        && killer != KILL_RESET
        && killer != KILL_DISMISSED
        && killer != KILL_BANISHED
        && _lost_soul_nearby(mons->pos()))
    {
        if (lost_soul_revive(mons))
            return true;
    }

    if (mons->hit_points < -25 || mons->hit_points < -mons->max_hit_points)
        return false;

    // Elyvilon specials.
    if (_ely_protect_ally(mons, killer))
        return true;
    if (_ely_heal_monster(mons, killer, i))
        return true;

    // Yredelemnul special.
    if (_yred_enslave_soul(mons, killer))
        return true;

    // Beogh special.
    if (_beogh_forcibly_convert_orc(mons, killer, i))
        return true;

    return false;
}

static void _jiyva_died()
{
    if (you_worship(GOD_JIYVA))
        return;

    add_daction(DACT_REMOVE_JIYVA_ALTARS);

    if (!player_in_branch(BRANCH_SLIME))
        return;

    if (silenced(you.pos()))
    {
        god_speaks(GOD_JIYVA, "With an infernal shudder, the power ruling "
                   "this place vanishes!");
    }
    else
    {
        god_speaks(GOD_JIYVA, "With infernal noise, the power ruling this "
                   "place vanishes!");
    }
}

static void _fire_monster_death_event(monster* mons,
                                      killer_type killer,
                                      int i, bool polymorph)
{
    int type = mons->type;

    // Treat whatever the Royal Jelly polymorphed into as if it were still
    // the Royal Jelly (but if a player chooses the character name
    // "shaped Royal Jelly" don't unlock the vaults when the player's
    // ghost is killed).
    if (mons->mname == "shaped Royal Jelly"
        && !mons_is_pghost(mons->type))
    {
        type = MONS_ROYAL_JELLY;
    }

    if (!polymorph)
    {
        dungeon_events.fire_event(
            dgn_event(DET_MONSTER_DIED, mons->pos(), 0,
                      mons->mid, killer));
    }

    if (killer == KILL_BANISHED)
        return;

    los_monster_died(mons);

    if (type == MONS_ROYAL_JELLY && !polymorph)
    {
        you.royal_jelly_dead = true;

        if (jiyva_is_dead())
            _jiyva_died();
    }
}

static void _mummy_curse(monster* mons, killer_type killer, int index)
{
    int pow;

    switch (killer)
    {
        // Mummy killed by trap or something other than the player or
        // another monster, so no curse.
        case KILL_MISC:
        // Mummy sent to the Abyss wasn't actually killed, so no curse.
        case KILL_RESET:
        case KILL_DISMISSED:
        case KILL_BANISHED:
            return;

        default:
            break;
    }

    switch (mons->type)
    {
        case MONS_MENKAURE:
        case MONS_MUMMY:          pow = 1; break;
        case MONS_GUARDIAN_MUMMY: pow = 3; break;
        case MONS_MUMMY_PRIEST:   pow = 8; break;
        case MONS_GREATER_MUMMY:  pow = 11; break;
        case MONS_KHUFU:          pow = 15; break;

        default:
            mprf(MSGCH_DIAGNOSTICS, "Unknown mummy type.");
            return;
    }

    // beam code might give an index of MHITYOU for the player.
    if (YOU_KILL(killer))
        index = NON_MONSTER;

    // Killed by a Zot trap, a god, etc.
    if (index != NON_MONSTER && invalid_monster_index(index))
        return;

    actor* target;
    if (index == NON_MONSTER)
        target = &you;
    else
    {
        // Mummies committing suicide don't cause a death curse.
        if (index == mons->mindex())
            return;
        target = &menv[index];
    }

    // Mummy was killed by a giant spore or ball lightning?
    if (!target->alive())
        return;

    if ((mons->type == MONS_MUMMY || mons->type == MONS_MENKAURE)
        && YOU_KILL(killer))
    {
        // Kiku protects you from ordinary mummy curses.
        if (you_worship(GOD_KIKUBAAQUDGHA) && !player_under_penance()
            && you.piety >= piety_breakpoint(1))
        {
            simple_god_message(" averts the curse.");
            return;
        }

        mprf(MSGCH_MONSTER_SPELL, "You feel nervous for a moment...");
        curse_an_item();
    }
    else
    {
        if (index == NON_MONSTER)
            mprf(MSGCH_MONSTER_SPELL, "You feel extremely nervous for a moment...");
        else if (you.can_see(target))
        {
            mprf(MSGCH_MONSTER_SPELL, "A malignant aura surrounds %s.",
                 target->name(DESC_THE).c_str());
        }
        MiscastEffect(target, mons->mindex(), SPTYP_NECROMANCY,
                      pow, random2avg(88, 3), "a mummy death curse");
    }
}

static void _setup_base_explosion(bolt & beam, const monster& origin)
{
    beam.is_tracer    = false;
    beam.is_explosion = true;
    beam.beam_source  = origin.mindex();
    beam.glyph        = dchar_glyph(DCHAR_FIRED_BURST);
    beam.source       = origin.pos();
    beam.source_name  = origin.base_name(DESC_BASENAME, true);
    beam.target       = origin.pos();
    beam.noise_msg    = "You hear an explosion!";

    if (!crawl_state.game_is_arena() && origin.attitude == ATT_FRIENDLY
        && !origin.is_summoned())
    {
        beam.thrower = KILL_YOU;
    }
    else
        beam.thrower = KILL_MON;

    beam.aux_source.clear();
    beam.attitude = origin.attitude;
}

void setup_spore_explosion(bolt & beam, const monster& origin)
{
    _setup_base_explosion(beam, origin);
    beam.flavour = BEAM_SPORE;
    beam.damage  = dice_def(3, 15);
    beam.name    = "explosion of spores";
    beam.colour  = LIGHTGREY;
    beam.ex_size = 2;
}

static void _setup_lightning_explosion(bolt & beam, const monster& origin)
{
    _setup_base_explosion(beam, origin);
    beam.flavour   = BEAM_ELECTRICITY;
    beam.damage    = dice_def(3, 20);
    beam.name      = "blast of lightning";
    beam.noise_msg = "You hear a clap of thunder!";
    beam.colour    = LIGHTCYAN;
    beam.ex_size   = coinflip() ? 3 : 2;
}

static void _setup_prism_explosion(bolt& beam, const monster& origin)
{
    _setup_base_explosion(beam, origin);
    beam.flavour = BEAM_MMISSILE;
    beam.damage  = (origin.number == 2 ? dice_def(3, 6 + origin.hit_dice * 7 / 4)
                    : dice_def(2, 6 + origin.hit_dice * 7 / 4));
    beam.name    = "blast of energy";
    beam.colour  = MAGENTA;
    beam.ex_size = origin.number;
}

static void _setup_inner_flame_explosion(bolt & beam, const monster& origin,
                                         actor* agent)
{
    _setup_base_explosion(beam, origin);
    const int size   = origin.body_size(PSIZE_BODY);
    beam.flavour     = BEAM_FIRE;
    beam.damage      = (size > SIZE_BIG)  ? dice_def(3, 25) :
                       (size > SIZE_TINY) ? dice_def(3, 20) :
                                            dice_def(3, 15);
    beam.name        = "fiery explosion";
    beam.colour      = RED;
    beam.ex_size     = (size > SIZE_BIG) ? 2 : 1;
    beam.source_name = origin.name(DESC_A, true);
    beam.thrower     = (agent && agent->is_player()) ? KILL_YOU_MISSILE
                                                     : KILL_MON_MISSILE;
}

static bool _explode_monster(monster* mons, killer_type killer,
                             int killer_index, bool pet_kill, bool wizard)
{
    if (mons->hit_points > 0 || mons->hit_points <= -15 || wizard
        || killer == KILL_RESET || killer == KILL_DISMISSED
        || killer == KILL_BANISHED)
    {
        if (killer != KILL_TIMEOUT)
            return false;
    }

    bolt beam;
    const int type = mons->type;
    const char* sanct_msg = NULL;
    actor* agent = mons;

    if (type == MONS_GIANT_SPORE)
    {
        setup_spore_explosion(beam, *mons);
        sanct_msg    = "By Zin's power, the giant spore's explosion is "
                       "contained.";
    }
    else if (type == MONS_BALL_LIGHTNING)
    {
        _setup_lightning_explosion(beam, *mons);
        sanct_msg    = "By Zin's power, the ball lightning's explosion "
                       "is contained.";
    }
    else if (type == MONS_LURKING_HORROR)
        sanct_msg = "The lurking horror fades away harmlessly.";
    else if (type == MONS_FULMINANT_PRISM)
    {
        _setup_prism_explosion(beam, *mons);
        sanct_msg = "By Zin's power, the prism's explosion is contained.";
    }
    else if (mons->has_ench(ENCH_INNER_FLAME))
    {
        mon_enchant i_f = mons->get_ench(ENCH_INNER_FLAME);
        ASSERT(i_f.ench == ENCH_INNER_FLAME);
        agent = actor_by_mid(i_f.source);
        _setup_inner_flame_explosion(beam, *mons, agent);
        // This might need to change if monsters ever get the ability to cast
        // Inner Flame...
        if (i_f.source == MID_ANON_FRIEND)
            mons_add_blame(mons, "hexed by Xom");
        else if (agent && agent->is_player())
            mons_add_blame(mons, "hexed by the player character");
        else if (agent)
            mons_add_blame(mons, "hexed by " + agent->name(DESC_A, true));
        mons->flags    |= MF_EXPLODE_KILL;
        sanct_msg       = "By Zin's power, the fiery explosion "
                          "is contained.";
        beam.aux_source = "exploding inner flame";
    }
    else
    {
        msg::streams(MSGCH_DIAGNOSTICS) << "Unknown spore type: "
                                        << static_cast<int>(type)
                                        << endl;
        return false;
    }

    if (beam.aux_source.empty())
    {
        if (YOU_KILL(killer))
            beam.aux_source = "set off by themself";
        else if (pet_kill)
            beam.aux_source = "set off by their pet";
    }

    bool saw = false;
    if (you.can_see(mons))
    {
        saw = true;
        viewwindow();
        if (is_sanctuary(mons->pos()))
            mprf(MSGCH_GOD, "%s", sanct_msg);
        else
            mprf(MSGCH_MONSTER_DAMAGE, MDAM_DEAD, "%s explodes!",
                 mons->full_name(DESC_THE).c_str());
    }

    if (is_sanctuary(mons->pos()))
        return false;

    // Explosion side-effects.
    if (type == MONS_LURKING_HORROR)
        torment(mons, mons->mindex(), mons->pos());
    else if (mons->has_ench(ENCH_INNER_FLAME))
    {
        for (adjacent_iterator ai(mons->pos(), false); ai; ++ai)
            if (!cell_is_solid(*ai) && env.cgrid(*ai) == EMPTY_CLOUD
                && !one_chance_in(5))
            {
                place_cloud(CLOUD_FIRE, *ai, 10 + random2(10), agent);
            }
    }

    // Detach monster from the grid first, so it doesn't get hit by
    // its own explosion. (GDL)
    mgrd(mons->pos()) = NON_MONSTER;

    // The explosion might cause a monster to be placed where the spore
    // used to be, so make sure that mgrd() doesn't get cleared a second
    // time (causing the new monster to become floating) when
    // mons->reset() is called.
    if (type == MONS_GIANT_SPORE)
        mons->set_position(coord_def(0,0));

    // Exploding kills the monster a bit earlier than normal.
    mons->hit_points = -16;
    if (saw)
        viewwindow();

    // FIXME: show_more == mons_near(mons)
    if (type == MONS_LURKING_HORROR)
    {
        targetter_los hitfunc(mons, LOS_SOLID);
        flash_view_delay(DARKGRAY, 300, &hitfunc);
    }
    else
        beam.explode();

    activate_ballistomycetes(mons, beam.target, YOU_KILL(beam.killer()));
    // Monster died in explosion, so don't re-attach it to the grid.
    return true;
}

static void _monster_die_cloud(const monster* mons, bool corpse, bool silent,
                        bool summoned)
{
    // Chaos spawn always leave behind a cloud of chaos.
    if (mons->type == MONS_CHAOS_SPAWN)
    {
        summoned = true;
        corpse   = false;
    }

    if (!summoned)
        return;

    if (cell_is_solid(mons->pos()))
        return;

    string prefix = " ";
    if (corpse)
    {
        if (mons_weight(mons_species(mons->type)) == 0)
            return;

        prefix = "'s corpse ";
    }

    string msg = summoned_poof_msg(mons) + "!";

    cloud_type cloud = CLOUD_NONE;
    if (msg.find("smoke") != string::npos)
        cloud = random_smoke_type();
    else if (msg.find("chaos") != string::npos)
        cloud = CLOUD_CHAOS;

    if (!silent)
        simple_monster_message(mons, (prefix + msg).c_str());

    if (cloud != CLOUD_NONE)
        place_cloud(cloud, mons->pos(), 1 + random2(3), mons);
}

void mons_relocated(monster* mons)
{
    // If the main body teleports get rid of the tentacles
    if (mons_is_tentacle_head(mons_base_type(mons)))
    {
        int headnum = mons->mindex();

        if (invalid_monster_index(headnum))
            return;

        for (monster_iterator mi; mi; ++mi)
        {
            if (mi->is_child_tentacle_of(mons))
            {
                for (monster_iterator connect; connect; ++connect)
                {
                    if (connect->is_child_tentacle_of(*mi))
                        monster_die(*connect, KILL_RESET, -1, true, false);
                }
                monster_die(*mi, KILL_RESET, -1, true, false);
            }
        }
    }
    // If a tentacle/segment is relocated just kill the tentacle
    else if (mons->is_child_monster())
    {
        int base_id = mons->mindex();

        monster* tentacle = mons;

        if (mons->is_child_tentacle_segment()
                && !::invalid_monster_index(base_id)
                && menv[base_id].is_parent_monster_of(mons))
        {
            tentacle = &menv[base_id];
        }

        for (monster_iterator connect; connect; ++connect)
        {
            if (connect->is_child_tentacle_of(tentacle))
                monster_die(*connect, KILL_RESET, -1, true, false);
        }

        monster_die(tentacle, KILL_RESET, -1, true, false);
    }
    else if (mons->type == MONS_ELDRITCH_TENTACLE
             || mons->type == MONS_ELDRITCH_TENTACLE_SEGMENT)
    {
        int base_id = mons->type == MONS_ELDRITCH_TENTACLE
                      ? mons->mindex() : mons->number;

        monster_die(&menv[base_id], KILL_RESET, -1, true, false);

        for (monster_iterator mit; mit; ++mit)
        {
            if (mit->type == MONS_ELDRITCH_TENTACLE_SEGMENT
                && (int) mit->number == base_id)
            {
                monster_die(*mit, KILL_RESET, -1, true, false);
            }
        }

    }

    mons->clear_clinging();
}

// When given either a tentacle end or segment, kills the end and all segments
// of that tentacle.
static int _destroy_tentacle(monster* mons)
{
    int seen = 0;

    monster* head = mons_is_tentacle_segment(mons->type)
            ? mons_get_parent_monster(mons) : mons;

    //If we tried to find the head, but failed (probably because it is already
    //dead), cancel trying to kill this tentacle
    if (head == NULL)
        return 0;

    // Some issue with using monster_die leading to DEAD_MONSTER
    // or w/e. Using hurt seems to cause more problems though.
    for (monster_iterator mi; mi; ++mi)
    {
        if (mi->is_child_tentacle_of(head))
        {
            if (mons_near(*mi))
                seen++;
            //mi->hurt(*mi, INSTANT_DEATH);
            monster_die(*mi, KILL_MISC, NON_MONSTER, true);
        }
    }

    if (mons != head)
    {
        if (mons_near(head))
                seen++;

        //mprf("killing base, %d %d", origin->mindex(), tentacle_idx);
        //menv[tentacle_idx].hurt(&menv[tentacle_idx], INSTANT_DEATH);
        monster_die(head, KILL_MISC, NON_MONSTER, true);
    }

    return seen;
}

static int _destroy_tentacles(monster* head)
{
    int seen = 0;
    for (monster_iterator mi; mi; ++mi)
    {
        if (mi->is_child_tentacle_of(head))
        {
            if (_destroy_tentacle(*mi))
                seen++;
            if (!mi->is_child_tentacle_segment())
            {
                monster_die(mi->as_monster(), KILL_MISC, NON_MONSTER, true);
                seen++;
            }
        }
    }
    return seen;
}

static string _killer_type_name(killer_type killer)
{
    switch (killer)
    {
    case KILL_NONE:
        return "none";
    case KILL_YOU:
        return "you";
    case KILL_MON:
        return "mon";
    case KILL_YOU_MISSILE:
        return "you_missile";
    case KILL_MON_MISSILE:
        return "mon_missile";
    case KILL_YOU_CONF:
        return "you_conf";
    case KILL_MISCAST:
        return "miscast";
    case KILL_MISC:
        return "misc";
    case KILL_RESET:
        return "reset";
    case KILL_DISMISSED:
        return "dismissed";
    case KILL_BANISHED:
        return "banished";
    case KILL_UNSUMMONED:
        return "unsummoned";
    case KILL_TIMEOUT:
        return "timeout";
    case KILL_PACIFIED:
        return "pacified";
    case KILL_ENSLAVED:
        return "enslaved";
    }
    die("invalid killer type");
}

static void _make_spectral_thing(monster* mons, bool quiet)
{
    if (mons->holiness() == MH_NATURAL && mons_can_be_zombified(mons))
    {
        const monster_type spectre_type = mons_species(mons->type);
        enchant_type shapeshift = ENCH_NONE;
        if (mons->has_ench(ENCH_SHAPESHIFTER))
            shapeshift = ENCH_SHAPESHIFTER;
        else if (mons->has_ench(ENCH_GLOWING_SHAPESHIFTER))
            shapeshift = ENCH_GLOWING_SHAPESHIFTER;

        // Headless hydras cannot be made spectral hydras, sorry.
        if (spectre_type == MONS_HYDRA && mons->number == 0)
        {
            mpr("A glowing mist gathers momentarily, then fades.");
            return;
        }

        // Use the original monster type as the zombified type here, to
        // get the proper stats from it.
        if (monster *spectre = create_monster(
                mgen_data(MONS_SPECTRAL_THING, BEH_FRIENDLY, &you,
                    0, SPELL_DEATH_CHANNEL, mons->pos(), MHITYOU,
                    0, static_cast<god_type>(you.attribute[ATTR_DIVINE_DEATH_CHANNEL]),
                    mons->type, mons->number)))
        {
            if (!quiet)
                mpr("A glowing mist starts to gather...");

            // If the original monster has been drained or levelled up,
            // its HD might be different from its class HD, in which
            // case its HP should be rerolled to match.
            if (spectre->hit_dice != mons->hit_dice)
            {
                spectre->hit_dice = max(mons->hit_dice, 1);
                roll_zombie_hp(spectre);
            }

            name_zombie(spectre, mons);

            spectre->add_ench(mon_enchant(ENCH_FAKE_ABJURATION, 6));
            if (shapeshift)
                spectre->add_ench(shapeshift);
        }
    }
}

static bool _mons_reaped(actor *killer, monster* victim);

static bool _reaping(monster *mons)
{
    if (!mons->props.exists("reaping_damage"))
        return false;

    int rd = mons->props["reaping_damage"].get_int();
    dprf("Reaping chance: %d/%d", rd, mons->damage_total);
    if (!x_chance_in_y(rd, mons->damage_total))
        return false;

    actor *killer = actor_by_mid(mons->props["reaper"].get_int());
    if (killer)
        return _mons_reaped(killer, mons);
    return false;
}

int monster_die(monster* mons, const actor *killer, bool silent,
                bool wizard, bool fake)
{
    killer_type ktype = KILL_YOU;
    int kindex = NON_MONSTER;

    if (killer->is_monster())
    {
        const monster *kmons = killer->as_monster();
        ktype = kmons->confused_by_you() ? KILL_YOU_CONF : KILL_MON;
        kindex = kmons->mindex();
    }

    return monster_die(mons, ktype, kindex, silent, wizard, fake);
}

// Returns the slot of a possibly generated corpse or -1.
int monster_die(monster* mons, killer_type killer,
                int killer_index, bool silent, bool wizard, bool fake)
{
    if (invalid_monster(mons))
        return -1;

    const bool was_visible = you.can_see(mons);

    // If a monster was banished to the Abyss and then killed there,
    // then its death wasn't a banishment.
    if (player_in_branch(BRANCH_ABYSS))
        mons->flags &= ~MF_BANISHED;

    if (!silent && !fake
        && _monster_avoided_death(mons, killer, killer_index))
    {
        mons->flags &= ~MF_EXPLODE_KILL;
        return -1;
    }

    // If the monster was calling the tide, let go now.
    mons->del_ench(ENCH_TIDE);

    // Same for silencers.
    mons->del_ench(ENCH_SILENCE);

    // ... and liquefiers.
    mons->del_ench(ENCH_LIQUEFYING);

    // Clean up any blood from the flayed effect
    if (mons->has_ench(ENCH_FLAYED))
        heal_flayed_effect(mons, true, true);

    crawl_state.inc_mon_acting(mons);

    ASSERT(!(YOU_KILL(killer) && crawl_state.game_is_arena()));

    if (mons->props.exists("monster_dies_lua_key"))
    {
        lua_stack_cleaner clean(dlua);

        dlua_chunk &chunk = mons->props["monster_dies_lua_key"];

        if (!chunk.load(dlua))
        {
            push_monster(dlua, mons);
            clua_pushcxxstring(dlua, _killer_type_name(killer));
            dlua.callfn(NULL, 2, 0);
        }
        else
        {
            mprf(MSGCH_ERROR,
                 "Lua death function for monster '%s' didn't load: %s",
                 mons->full_name(DESC_PLAIN).c_str(),
                 dlua.error.c_str());
        }
    }

    mons_clear_trapping_net(mons);
    mons->stop_constricting_all(false);
    mons->stop_being_constricted();

    you.remove_beholder(mons);
    you.remove_fearmonger(mons);
    // Uniques leave notes and milestones, so this information is already leaked.
    remove_unique_annotation(mons);

    // Clear auto exclusion now the monster is killed - if we know about it.
    if (mons_near(mons) || wizard || mons_is_unique(mons->type))
        remove_auto_exclude(mons);

          int  duration      = 0;
    const bool summoned      = mons->is_summoned(&duration);
    const int monster_killed = mons->mindex();
    const bool hard_reset    = testbits(mons->flags, MF_HARD_RESET);
    bool unsummoned          = killer == KILL_UNSUMMONED;
    bool timeout             = killer == KILL_TIMEOUT;
    const bool gives_xp      = (!summoned && !mons_class_flag(mons->type,
                                M_NO_EXP_GAIN) && !mons_is_phoenix(mons));

    bool drop_items    = !hard_reset;

    const bool mons_reset(killer == KILL_RESET || killer == KILL_DISMISSED);

    bool fake_abjuration = (mons->has_ench(ENCH_FAKE_ABJURATION));

    const bool submerged     = mons->submerged();

    bool in_transit          = false;
    bool was_banished        = (killer == KILL_BANISHED);

    // Award experience for suicide if the suicide was caused by the
    // player.
    if (MON_KILL(killer) && monster_killed == killer_index)
    {
        if (mons->confused_by_you())
        {
            ASSERT(!crawl_state.game_is_arena());
            killer = KILL_YOU_CONF;
        }
    }
    else if (MON_KILL(killer) && mons->has_ench(ENCH_CHARM))
    {
        ASSERT(!crawl_state.game_is_arena());
        killer = KILL_YOU_CONF; // Well, it was confused in a sense... (jpeg)
    }

    // Kills by the spectral weapon are considered as kills by the player instead
    if (killer == KILL_MON
        && !invalid_monster_index(killer_index)
        && menv[killer_index].type == MONS_SPECTRAL_WEAPON
        && menv[killer_index].summoner == MID_PLAYER)
    {
        killer = KILL_YOU;
        killer_index = you.mindex();
    }

    // Take notes and mark milestones.
    record_monster_defeat(mons, killer);

    // Various sources of berserk extension on kills.
    if (killer == KILL_YOU && you.berserk())
    {
        if (you_worship(GOD_TROG)
            && !player_under_penance() && you.piety > random2(1000))
        {
            const int bonus = (3 + random2avg(10, 2)) / 2;

            you.increase_duration(DUR_BERSERK, bonus);

            mprf(MSGCH_GOD, GOD_TROG,
                 "You feel the power of Trog in you as your rage grows.");
        }
        else if (player_equip_unrand(UNRAND_BLOODLUST))
        {
            if (coinflip())
            {
                const int bonus = (2 + random2(4)) / 2;
                you.increase_duration(DUR_BERSERK, bonus);
                mpr("The necklace of Bloodlust glows a violent red.");
            }
        }
        else if (you.wearing(EQ_AMULET, AMU_RAGE)
                 && one_chance_in(30))
        {
            const int bonus = (2 + random2(4)) / 2;
            you.increase_duration(DUR_BERSERK, bonus);
            mpr("Your amulet glows a violent red.");
        }
    }

    if (you.prev_targ == monster_killed)
    {
        you.prev_targ = MHITNOT;
        crawl_state.cancel_cmd_repeat();
    }

    if (killer == KILL_YOU)
        crawl_state.cancel_cmd_repeat();

    const bool pet_kill = _is_pet_kill(killer, killer_index);

    bool did_death_message = false;

    if (mons->type == MONS_GIANT_SPORE
        || mons->type == MONS_BALL_LIGHTNING
        || mons->type == MONS_LURKING_HORROR
        || (mons->type == MONS_FULMINANT_PRISM && mons->number > 0)
        || mons->has_ench(ENCH_INNER_FLAME))
    {
        did_death_message =
            _explode_monster(mons, killer, killer_index, pet_kill, wizard);
    }
    else if (mons->type == MONS_FULMINANT_PRISM && mons->number == 0)
    {
        if (!silent && !hard_reset && !was_banished)
        {
            simple_monster_message(mons, " detonates feebly.",
                                   MSGCH_MONSTER_DAMAGE, MDAM_DEAD);
            silent = true;
        }
    }
    else if (mons->type == MONS_FIRE_VORTEX
             || mons->type == MONS_SPATIAL_VORTEX
             || mons->type == MONS_TWISTER)
    {
        if (!silent && !mons_reset && !was_banished)
        {
            simple_monster_message(mons, " dissipates!",
                                   MSGCH_MONSTER_DAMAGE, MDAM_DEAD);
            silent = true;
        }

        if (mons->type == MONS_FIRE_VORTEX && !wizard && !mons_reset
            && !submerged && !was_banished && !cell_is_solid(mons->pos()))
        {
            place_cloud(CLOUD_FIRE, mons->pos(), 2 + random2(4), mons);
        }

        if (killer == KILL_RESET)
            killer = KILL_DISMISSED;
    }
    else if (mons->type == MONS_SIMULACRUM)
    {
        if (!silent && !mons_reset && !was_banished)
        {
            simple_monster_message(mons, " vapourises!",
                                   MSGCH_MONSTER_DAMAGE, MDAM_DEAD);
            silent = true;
        }

        if (!wizard && !mons_reset && !submerged && !was_banished
            && !cell_is_solid(mons->pos()))
        {
            place_cloud(CLOUD_COLD, mons->pos(), 2 + random2(4), mons);
        }

        if (killer == KILL_RESET)
            killer = KILL_DISMISSED;
    }
    else if (mons->type == MONS_DANCING_WEAPON)
    {
        if (!hard_reset)
        {
            if (killer == KILL_RESET)
                killer = KILL_DISMISSED;
        }

        int w_idx = mons->inv[MSLOT_WEAPON];
        ASSERT(w_idx != NON_ITEM);

        // XXX: This can probably become mons->is_summoned(): there's no
        // feasible way for a dancing weapon to "drop" it's weapon and somehow
        // gain a summoned one, or vice versa.
        bool summoned_it = mitm[w_idx].flags & ISFLAG_SUMMONED;

        if (!silent && !hard_reset && !was_banished)
        {
            if (!summoned_it)
            {
                simple_monster_message(mons, " falls from the air.",
                                       MSGCH_MONSTER_DAMAGE, MDAM_DEAD);
                silent = true;
            }
            else
                killer = KILL_RESET;
        }

        if (was_banished && !summoned_it && !hard_reset && mons->has_ench(ENCH_ABJ))
        {
            if (is_unrandom_artefact(mitm[w_idx]))
                set_unique_item_status(mitm[w_idx], UNIQ_LOST_IN_ABYSS);

            destroy_item(w_idx);
        }
    }
    else if (mons->type == MONS_ELDRITCH_TENTACLE)
    {
        if (!silent && !mons_reset && !mons->has_ench(ENCH_SEVERED) && !was_banished)
        {
            if (you.can_see(mons))
            {
                mprf(MSGCH_MONSTER_DAMAGE, MDAM_DEAD, silenced(mons->pos()) ?
                    "The tentacle is hauled back through the portal!" :
                    "With a roar, the tentacle is hauled back through the portal!");
            }
            silent = true;
        }

        if (killer == KILL_RESET)
            killer = KILL_DISMISSED;
    }
    else if (mons->type == MONS_BATTLESPHERE)
    {
        if (!wizard && !mons_reset && !was_banished)
            place_cloud(CLOUD_MAGIC_TRAIL, mons->pos(), 3 + random2(3), mons);
        end_battlesphere(mons, true);
    }
    else if (mons->type == MONS_BRIAR_PATCH)
    {
        if (timeout && !silent)
            simple_monster_message(mons, " crumbles away.");
    }
    else if (mons->type == MONS_SPECTRAL_WEAPON)
    {
        end_spectral_weapon(mons, true, killer == KILL_RESET);
        silent = true;
    }
    else if (mons->type == MONS_ELEMENTAL_WELLSPRING
             && mons->mindex() == killer_index)
    {
        if (!silent)
            simple_monster_message(mons, " exhausts itself and dries up.");
        silent = true;
    }

    const bool death_message = !silent && !did_death_message
                               && mons_near(mons)
                               && mons->visible_to(&you);
    const bool exploded      = mons->flags & MF_EXPLODE_KILL;

    const bool created_friendly = testbits(mons->flags, MF_NO_REWARD);
          bool anon = (killer_index == ANON_FRIENDLY_MONSTER);
    mon_holy_type targ_holy = mons->holiness();

    // Dual holiness, Trog and Kiku like dead demons but not undead.
    if ((mons->type == MONS_ABOMINATION_SMALL
         || mons->type == MONS_ABOMINATION_LARGE
         || mons->type == MONS_CRAWLING_CORPSE
         || mons->type == MONS_MACABRE_MASS)
        && (you_worship(GOD_TROG)
         || you_worship(GOD_KIKUBAAQUDGHA)))
    {
        targ_holy = MH_DEMONIC;
    }

    // Adjust song of slaying bonus
    // Kills by the spectral weapon should be adjusted by this point to be
    // kills by the player --- so kills by the spectral weapon are considered here as well
    if (killer == KILL_YOU && you.duration[DUR_SONG_OF_SLAYING] && !mons->is_summoned() && gives_xp)
    {
        int sos_bonus = you.props["song_of_slaying_bonus"].get_int();
        mon_threat_level_type threat = mons_threat_level(mons, true);
        // Only certain kinds of threats at different sos levels will increase the bonus
        if (threat == MTHRT_TRIVIAL && sos_bonus < 3
            || threat == MTHRT_EASY && sos_bonus < 5
            || threat == MTHRT_TOUGH && sos_bonus < 7
            || threat == MTHRT_NASTY)
        {
            you.props["song_of_slaying_bonus"] = sos_bonus + 1;
        }
    }

    switch (killer)
    {
        case KILL_YOU:          // You kill in combat.
        case KILL_YOU_MISSILE:  // You kill by missile or beam.
        case KILL_YOU_CONF:     // You kill by confusion.
        {
            const bool bad_kill    = god_hates_killing(you.religion, mons);
            const bool was_neutral = testbits(mons->flags, MF_WAS_NEUTRAL);
            // killing friendlies is good, more bloodshed!
            // Undead feel no pain though, tormenting them is not as satisfying.
            const bool good_kill   = !created_friendly && gives_xp
                || (is_evil_god(you.religion) && !mons->is_summoned()
                    && !fake_abjuration
                    && (targ_holy == MH_NATURAL || targ_holy == MH_HOLY));

            if (death_message)
            {
                if (killer == KILL_YOU_CONF
                    && (anon || !invalid_monster_index(killer_index)))
                {
                    mprf(MSGCH_MONSTER_DAMAGE, MDAM_DEAD, "%s is %s!",
                         mons->name(DESC_THE).c_str(),
                         exploded                        ? "blown up" :
                         _wounded_damaged(targ_holy)     ? "destroyed"
                                                         : "killed");
                }
                else
                {
                    mprf(MSGCH_MONSTER_DAMAGE, MDAM_DEAD, "You %s %s!",
                         exploded                        ? "blow up" :
                         _wounded_damaged(targ_holy)     ? "destroy"
                                                         : "kill",
                         mons->name(DESC_THE).c_str());
                }

                if ((created_friendly || was_neutral) && gives_xp)
                    mpr("That felt strangely unrewarding.");
            }

            // Killing triggers hints mode lesson.
            if (gives_xp)
                _hints_inspect_kill();

            // Prevent summoned creatures from being good kills.
            if (bad_kill || good_kill)
            {
                if (targ_holy == MH_NATURAL)
                {
                    did_god_conduct(DID_KILL_LIVING,
                                    mons->hit_dice, true, mons);

                    // TSO hates natural evil and unholy beings.
                    if (mons->is_unholy())
                    {
                        did_god_conduct(DID_KILL_NATURAL_UNHOLY,
                                        mons->hit_dice, true, mons);
                    }
                    else if (mons->is_evil())
                    {
                        did_god_conduct(DID_KILL_NATURAL_EVIL,
                                        mons->hit_dice, true, mons);
                    }
                }
                else if (targ_holy == MH_UNDEAD)
                {
                    did_god_conduct(DID_KILL_UNDEAD,
                                    mons->hit_dice, true, mons);
                }
                else if (targ_holy == MH_DEMONIC)
                {
                    did_god_conduct(DID_KILL_DEMON,
                                    mons->hit_dice, true, mons);
                }

                // Zin hates unclean and chaotic beings.
                if (mons->is_unclean())
                {
                    did_god_conduct(DID_KILL_UNCLEAN,
                                    mons->hit_dice, true, mons);
                }
                else if (mons->is_chaotic())
                {
                    did_god_conduct(DID_KILL_CHAOTIC,
                                    mons->hit_dice, true, mons);
                }

                // jmf: Trog hates wizards.
                if (mons->is_actual_spellcaster())
                {
                    did_god_conduct(DID_KILL_WIZARD,
                                    mons->hit_dice, true, mons);
                }

                // Beogh hates priests of other gods.
                if (mons->is_priest())
                {
                    did_god_conduct(DID_KILL_PRIEST,
                                    mons->hit_dice, true, mons);
                }

                // Jiyva hates you killing slimes, but eyeballs
                // mutation can confuse without you meaning it.
                if (mons_is_slime(mons) && killer != KILL_YOU_CONF)
                {
                    did_god_conduct(DID_KILL_SLIME, mons->hit_dice,
                                    true, mons);
                }

                if (fedhas_protects(mons))
                {
                    did_god_conduct(DID_KILL_PLANT, mons->hit_dice,
                                    true, mons);
                }

                // Cheibriados hates fast monsters.
                if (cheibriados_thinks_mons_is_fast(mons)
                    && !mons->cannot_move())
                {
                    did_god_conduct(DID_KILL_FAST, mons->hit_dice,
                                    true, mons);
                }

                // Yredelemnul hates artificial beings.
                if (mons->is_artificial())
                {
                    did_god_conduct(DID_KILL_ARTIFICIAL, mons->hit_dice,
                                    true, mons);
                }

                // Holy kills are always noticed.
                if (mons->is_holy())
                {
                    did_god_conduct(DID_KILL_HOLY, mons->hit_dice,
                                    true, mons);
                }
            }

            // Divine health and mana restoration doesn't happen when
            // killing born-friendly monsters.
            if (good_kill
                && (you_worship(GOD_MAKHLEB)
                    || you_worship(GOD_VEHUMET)
                    || you_worship(GOD_SHINING_ONE)
                       && (mons->is_evil() || mons->is_unholy()))
                && !mons_is_object(mons->type)
                && !player_under_penance()
                && random2(you.piety) >= piety_breakpoint(0))
            {
                int hp_heal = 0, mp_heal = 0;

                switch (you.religion)
                {
                case GOD_MAKHLEB:
                    hp_heal = mons->hit_dice + random2(mons->hit_dice);
                    break;
                case GOD_SHINING_ONE:
                    hp_heal = random2(1 + 2 * mons->hit_dice);
                    mp_heal = random2(2 + mons->hit_dice / 3);
                    break;
                case GOD_VEHUMET:
                    mp_heal = 1 + random2(mons->hit_dice / 2);
                    break;
                default:
                    die("bad kill-on-healing god!");
                }

                if (you.species == SP_DJINNI)
                    hp_heal = max(hp_heal, mp_heal * 2), mp_heal = 0;

                if (hp_heal && you.hp < you.hp_max
                    && !you.duration[DUR_DEATHS_DOOR])
                {
                    mpr("You feel a little better.");
                    inc_hp(hp_heal);
                }

                if (mp_heal && you.magic_points < you.max_magic_points)
                {
                    mpr("You feel your power returning.");
                    inc_mp(mp_heal);
                }
            }

            // Randomly bless a follower.
            if (!created_friendly
                && gives_xp
                && (you_worship(GOD_BEOGH)
                    && random2(you.piety) >= piety_breakpoint(2))
                && !mons_is_object(mons->type)
                && !player_under_penance())
            {
                bless_follower();
            }

            if (you.duration[DUR_DEATH_CHANNEL] && gives_xp)
                _make_spectral_thing(mons, !death_message);
            break;
        }

        case KILL_MON:          // Monster kills in combat.
        case KILL_MON_MISSILE:  // Monster kills by missile or beam.
            if (death_message)
            {
                const char* msg =
                    exploded                     ? " is blown up!" :
                    _wounded_damaged(targ_holy)  ? " is destroyed!"
                                                 : " dies!";
                simple_monster_message(mons, msg, MSGCH_MONSTER_DAMAGE,
                                       MDAM_DEAD);
            }

            if (crawl_state.game_is_arena())
                break;

            // No piety loss if god gifts killed by other monsters.
            // Also, dancing weapons aren't really friendlies.
            if (mons->friendly() && !mons_is_object(mons->type))
            {
                const int mon_intel = mons_class_intel(mons->type) - I_ANIMAL;

                did_god_conduct(mon_intel > 0 ? DID_SOULED_FRIEND_DIED
                                              : DID_FRIEND_DIED,
                                1 + (mons->hit_dice / 2),
                                true, mons);
            }

            if (pet_kill && fedhas_protects(mons))
            {
                did_god_conduct(DID_PLANT_KILLED_BY_SERVANT, 1 + (mons->hit_dice / 2),
                                true, mons);
            }

            // Trying to prevent summoning abuse here, so we're trying to
            // prevent summoned creatures from being done_good kills.  Only
            // affects creatures which were friendly when summoned.
            if (!created_friendly && gives_xp && pet_kill
                && (anon || !invalid_monster_index(killer_index)))
            {
                bool notice = false;

                monster* killer_mon = NULL;
                if (!anon)
                {
                    killer_mon = &menv[killer_index];

                    // If the killer is already dead, treat it like an
                    // anonymous monster.
                    if (killer_mon->type == MONS_NO_MONSTER)
                        anon = true;
                }

                const mon_holy_type killer_holy =
                    anon ? MH_NATURAL : killer_mon->holiness();

                if (you_worship(GOD_ZIN)
                    || you_worship(GOD_SHINING_ONE)
                    || you_worship(GOD_YREDELEMNUL)
                    || you_worship(GOD_KIKUBAAQUDGHA)
                    || you_worship(GOD_MAKHLEB)
                    || you_worship(GOD_LUGONU)
                    || !anon && mons_is_god_gift(killer_mon))
                {
                    if (killer_holy == MH_UNDEAD)
                    {
                        const bool confused =
                            anon ? false : !killer_mon->friendly();

                        // Yes, these are hacks, but they make sure that
                        // confused monsters doing kills are not
                        // referred to as "slaves", and I think it's
                        // okay that e.g. Yredelemnul ignores kills done
                        // by confused monsters as opposed to enslaved
                        // or friendly ones. (jpeg)
                        if (targ_holy == MH_NATURAL)
                        {
                            notice |= did_god_conduct(
                                          !confused ? DID_LIVING_KILLED_BY_UNDEAD_SLAVE :
                                                      DID_LIVING_KILLED_BY_SERVANT,
                                          mons->hit_dice);
                        }
                        else if (targ_holy == MH_UNDEAD)
                        {
                            notice |= did_god_conduct(
                                          !confused ? DID_UNDEAD_KILLED_BY_UNDEAD_SLAVE :
                                                      DID_UNDEAD_KILLED_BY_SERVANT,
                                          mons->hit_dice);
                        }
                        else if (targ_holy == MH_DEMONIC)
                        {
                            notice |= did_god_conduct(
                                          !confused ? DID_DEMON_KILLED_BY_UNDEAD_SLAVE :
                                                      DID_DEMON_KILLED_BY_SERVANT,
                                          mons->hit_dice);
                        }

                        if (mons->is_unclean())
                        {
                            notice |= did_god_conduct(DID_UNCLEAN_KILLED_BY_SERVANT,
                                                      mons->hit_dice);
                        }

                        if (mons->is_chaotic())
                        {
                            notice |= did_god_conduct(DID_CHAOTIC_KILLED_BY_SERVANT,
                                                      mons->hit_dice);
                        }

                        if (mons->is_artificial())
                        {
                            notice |= did_god_conduct(
                                          !confused ? DID_ARTIFICIAL_KILLED_BY_UNDEAD_SLAVE :
                                                      DID_ARTIFICIAL_KILLED_BY_SERVANT,
                                          mons->hit_dice);
                        }
                    }
                    // Yes, we are splitting undead pets from the others
                    // as a way to focus Necromancy vs. Summoning
                    // (ignoring Haunt here)... at least we're being
                    // nice and putting the natural creature summons
                    // together with the demonic ones.  Note that
                    // Vehumet gets a free pass here since those
                    // followers are assumed to come from summoning
                    // spells...  the others are from invocations (TSO,
                    // Makhleb, Kiku). - bwr
                    else if (targ_holy == MH_NATURAL)
                    {
                        notice |= did_god_conduct(DID_LIVING_KILLED_BY_SERVANT,
                                                  mons->hit_dice);

                        if (mons->is_unholy())
                        {
                            notice |= did_god_conduct(
                                          DID_NATURAL_UNHOLY_KILLED_BY_SERVANT,
                                          mons->hit_dice);
                        }

                        if (mons->is_evil())
                        {
                            notice |= did_god_conduct(
                                          DID_NATURAL_EVIL_KILLED_BY_SERVANT,
                                          mons->hit_dice);
                        }
                    }
                    else if (targ_holy == MH_UNDEAD)
                    {
                        notice |= did_god_conduct(DID_UNDEAD_KILLED_BY_SERVANT,
                                                  mons->hit_dice);
                    }
                    else if (targ_holy == MH_DEMONIC)
                    {
                        notice |= did_god_conduct(DID_DEMON_KILLED_BY_SERVANT,
                                                  mons->hit_dice);
                    }

                    if (mons->is_unclean())
                    {
                        notice |= did_god_conduct(DID_UNCLEAN_KILLED_BY_SERVANT,
                                                  mons->hit_dice);
                    }

                    if (mons->is_chaotic())
                    {
                        notice |= did_god_conduct(DID_CHAOTIC_KILLED_BY_SERVANT,
                                                  mons->hit_dice);
                    }

                    if (mons->is_artificial())
                    {
                        notice |= did_god_conduct(DID_ARTIFICIAL_KILLED_BY_SERVANT,
                                                  mons->hit_dice);
                    }
                }

                // Holy kills are always noticed.
                if (mons->is_holy())
                {
                    if (killer_holy == MH_UNDEAD)
                    {
                        const bool confused =
                            anon ? false : !killer_mon->friendly();

                        // Yes, this is a hack, but it makes sure that
                        // confused monsters doing kills are not
                        // referred to as "slaves", and I think it's
                        // okay that Yredelemnul ignores kills done by
                        // confused monsters as opposed to enslaved or
                        // friendly ones. (jpeg)
                        notice |= did_god_conduct(
                                      !confused ? DID_HOLY_KILLED_BY_UNDEAD_SLAVE :
                                                  DID_HOLY_KILLED_BY_SERVANT,
                                      mons->hit_dice, true, mons);
                    }
                    else
                        notice |= did_god_conduct(DID_HOLY_KILLED_BY_SERVANT,
                                                  mons->hit_dice, true, mons);
                }

                if (you_worship(GOD_SHINING_ONE)
                    && (mons->is_evil() || mons->is_unholy())
                    && !player_under_penance()
                    && random2(you.piety) >= piety_breakpoint(0)
                    && !invalid_monster_index(killer_index))
                {
                    // Randomly bless the follower who killed.
                    if (!one_chance_in(3) && killer_mon->alive()
                        && bless_follower(killer_mon))
                    {
                        break;
                    }

                    if (killer_mon->alive()
                        && killer_mon->hit_points < killer_mon->max_hit_points)
                    {
                        simple_monster_message(killer_mon,
                                               " looks invigorated.");
                        killer_mon->heal(1 + random2(mons->hit_dice / 4));
                    }
                }

                if (you_worship(GOD_BEOGH)
                    && random2(you.piety) >= piety_breakpoint(2)
                    && !player_under_penance()
                    && !one_chance_in(3)
                    && !invalid_monster_index(killer_index))
                {
                    // Randomly bless the follower who killed.
                    bless_follower(killer_mon);
                }

                if (you.duration[DUR_DEATH_CHANNEL] && gives_xp && was_visible)
                    _make_spectral_thing(mons, !death_message);
            }
            break;

        // Monster killed by trap/inanimate thing/itself/poison not from you.
        case KILL_MISC:
        case KILL_MISCAST:
            if (death_message)
            {
                if (fake_abjuration)
                {
                    if (mons_genus(mons->type) == MONS_ADDER)
                    {
                        // Sticks to Snake
                        simple_monster_message(mons, " withers and dies!",
                            MSGCH_MONSTER_DAMAGE, MDAM_DEAD);
                    }
                    else if (mons->type == MONS_SPECTRAL_THING)
                    {
                        // Death Channel
                        simple_monster_message(mons, " fades into mist!");
                    }
                    else
                    {
                        string msg = " " + summoned_poof_msg(mons) + "!";
                        simple_monster_message(mons, msg.c_str());
                    }
                }
                else
                {
                    const char* msg =
                        exploded                     ? " is blown up!" :
                        _wounded_damaged(targ_holy)  ? " is destroyed!"
                                                     : " dies!";
                    simple_monster_message(mons, msg, MSGCH_MONSTER_DAMAGE,
                                           MDAM_DEAD);
                }
            }
            break;

        case KILL_BANISHED:
            // Monster doesn't die, just goes back to wherever it came from.
            // This must only be called by monsters running out of time (or
            // abjuration), because it uses the beam variables!  Or does it???
            // Pacified monsters leave the level when this happens.

            // KILL_RESET monsters no longer lose their whole inventory, only
            // items they were generated with.
            if (mons->pacified() || !mons->needs_abyss_transit())
            {
                // A banished monster that doesn't go on the transit list
                // loses all items.
                if (!mons->is_summoned())
                    drop_items = false;
                break;
            }

            // Monster goes to the Abyss.
            mons->flags |= MF_BANISHED;
            {
                unwind_var<int> dt(mons->damage_total, 0);
                unwind_var<int> df(mons->damage_friendly, 0);
                mons->set_transit(level_id(BRANCH_ABYSS));
            }
            set_unique_annotation(mons, BRANCH_ABYSS);
            in_transit = true;
            drop_items = false;
            mons->firing_pos.reset();
            // Make monster stop patrolling and/or travelling.
            mons->patrol_point.reset();
            mons->travel_path.clear();
            mons->travel_target = MTRAV_NONE;
            break;

        case KILL_RESET:
            drop_items = false;
            break;

        case KILL_TIMEOUT:
        case KILL_UNSUMMONED:
        case KILL_DISMISSED:
            break;

        default:
            drop_items = false;
            break;
    }

    // Make sure Boris has a foe to address.
    if (mons->foe == MHITNOT)
    {
        if (!mons->wont_attack() && !crawl_state.game_is_arena())
            mons->foe = MHITYOU;
        else if (!invalid_monster_index(killer_index))
            mons->foe = killer_index;
    }

    // Make sure that the monster looks dead.
    if (mons->alive() && (!summoned || duration > 0))
    {
        dprf("Granting xp for non-damage kill.");
        if (YOU_KILL(killer))
            mons->damage_friendly += mons->hit_points * 2;
        else if (pet_kill)
            mons->damage_friendly += mons->hit_points;
        mons->damage_total += mons->hit_points;

        if (!in_transit) // banishment only
            mons->hit_points = -1;
    }

    if (!silent && !wizard && you.see_cell(mons->pos()))
    {
        // Make sure that the monster looks dead.
        if (mons->alive() && !in_transit && (!summoned || duration > 0))
            mons->hit_points = -1;
        // Hack: with cleanup_dead=false, a tentacle [segment] of a dead
        // [malign] kraken has no valid head reference.
        if (!mons_is_tentacle_or_tentacle_segment(mons->type))
            mons_speaks(mons);
    }

    if (mons->type == MONS_BORIS && !in_transit && !mons->pacified())
    {
        // XXX: Actual blood curse effect for Boris? - bwr

        // Now that Boris is dead, he's a valid target for monster
        // creation again. - bwr
        you.unique_creatures.set(mons->type, false);
        // And his vault can be placed again.
        you.uniq_map_names.erase("uniq_boris");
    }
    if (mons->type == MONS_JORY && !in_transit)
        blood_spray(mons->pos(), MONS_JORY, 50);
    else if (mons_is_kirke(mons)
             && !in_transit
             && !testbits(mons->flags, MF_WAS_NEUTRAL))
    {
        hogs_to_humans();
    }
    else if (mons_is_pikel(mons))
    {
        // His slaves don't care if he's dead or not, just whether or not
        // he goes away.
        pikel_band_neutralise();
    }
    else if (mons->is_named() && mons->friendly() && killer != KILL_RESET)
        take_note(Note(NOTE_ALLY_DEATH, 0, 0, mons->mname.c_str()));
    else if (mons_is_tentacle_head(mons_base_type(mons)))
    {
        if (_destroy_tentacles(mons)
            && !in_transit
            && you.see_cell(mons->pos()))
        {
            if (mons_base_type(mons) == MONS_KRAKEN)
                mpr("The dead kraken's tentacles slide back into the water.");
            else if (mons->type == MONS_TENTACLED_STARSPAWN)
                mpr("The starspawn's tentacles wither and die.");
        }
    }
    else if (mons_is_tentacle_or_tentacle_segment(mons->type)
             && killer != KILL_MISC
                 || mons->type == MONS_ELDRITCH_TENTACLE
                 || mons->type == MONS_SNAPLASHER_VINE)
    {
        if (mons->type == MONS_SNAPLASHER_VINE)
        {
            if (mons->props.exists("vine_awakener"))
            {
                monster* awakener =
                        monster_by_mid(mons->props["vine_awakener"].get_int());
                if (awakener)
                    awakener->props["vines_awakened"].get_int()--;
            }
        }
        _destroy_tentacle(mons);
    }
    else if (mons->type == MONS_ELDRITCH_TENTACLE_SEGMENT
             && killer != KILL_MISC)
    {
        if (!invalid_monster_index(mons->number)
             && mons_base_type(&menv[mons->number]) == MONS_ELDRITCH_TENTACLE
             && menv[mons->number].alive())
        {
            monster_die(&menv[mons->number], killer, killer_index, silent,
                        wizard, fake);
        }
    }
    else if (mons_is_elven_twin(mons) && mons_near(mons))
        elven_twin_died(mons, in_transit, killer, killer_index);
    else if (mons_is_shedu(mons))
    {
        if (was_banished) // Don't try resurrecting them.
            mons->number = 0;
        else
            shedu_do_resurrection(mons);
    }
    else if (mons_is_phoenix(mons))
    {
        if (!was_banished)
            phoenix_died(mons);
    }
    else if (mons->type == MONS_VAULT_WARDEN)
        timeout_terrain_changes(0, true);
    else if (mons->type == MONS_FLAYED_GHOST)
        end_flayed_effect(mons);
    else if (mons->type == MONS_PLAGUE_SHAMBLER && !was_banished
             && !mons->pacified() && (!summoned || duration > 0) && !wizard)
    {
        if (you.can_see(mons))
        {
            mprf(MSGCH_WARN, "Miasma billows from the fallen %s!",
                 mons->name(DESC_PLAIN).c_str());
        }

        map_cloud_spreader_marker *marker =
            new map_cloud_spreader_marker(mons->pos(), CLOUD_MIASMA, 10,
                                          18 + random2(7), 4, 8, mons);
        // Start the cloud at radius 1, regardless of the speed of the killing blow
        marker->speed_increment -= you.time_taken;
        env.markers.add(marker);
        env.markers.clear_need_activate();
    }
    // Give the treant a last chance to release its wasps if it is killed in a
    // single blow from above half health
    else if (mons->type == MONS_TREANT && !was_banished
             && !mons->pacified() && (!summoned || duration > 0) && !wizard)
    {
        treant_release_wasps(mons);
    }
    else if (mons_is_mimic(mons->type))
        drop_items = false;
    else if (!mons->is_summoned())
    {
        if (mons_genus(mons->type) == MONS_MUMMY)
            _mummy_curse(mons, killer, killer_index);
    }

    if (mons->mons_species() == MONS_BALLISTOMYCETE)
    {
        activate_ballistomycetes(mons, mons->pos(),
                                 YOU_KILL(killer) || pet_kill);
    }

    if (!wizard && !submerged && !was_banished)
    {
        _monster_die_cloud(mons, !mons_reset && !fake_abjuration && !unsummoned
                                 && !timeout, silent, summoned);
    }

    int corpse = -1;
    if (!mons_reset && !summoned && !fake_abjuration && !unsummoned
        && !timeout && !was_banished)
    {
        // Have to add case for disintegration effect here? {dlb}
        int corpse2 = -1;

        if (mons->type == MONS_SPRIGGAN_RIDER)
        {
            corpse2 = mounted_kill(mons, MONS_FIREFLY, killer, killer_index);
            mons->type = MONS_SPRIGGAN;
        }
        corpse = place_monster_corpse(mons, silent);
        if (corpse == -1)
            corpse = corpse2;
    }

    if ((killer == KILL_YOU
         || killer == KILL_YOU_MISSILE
         || killer == KILL_YOU_CONF
         || pet_kill)
             && corpse >= 0 && player_mutation_level(MUT_POWERED_BY_DEATH))
    {
        const int pbd_dur = player_mutation_level(MUT_POWERED_BY_DEATH) * 8
                            + roll_dice(2, 8);
        if (pbd_dur > you.duration[DUR_POWERED_BY_DEATH])
            you.set_duration(DUR_POWERED_BY_DEATH, pbd_dur);
    }

    unsigned int player_exp = 0, monster_exp = 0;
    if (!mons_reset && !fake_abjuration && !timeout && !unsummoned)
    {
        player_exp = _calc_player_experience(mons);
        monster_exp = _calc_monster_experience(mons, killer, killer_index);
    }

    if (!mons_reset && !fake_abjuration && !crawl_state.game_is_arena()
        && !unsummoned && !timeout && !in_transit)
    {
        you.kills->record_kill(mons, killer, pet_kill);
    }

    if (fake)
    {
        if (corpse != -1 && _reaping(mons))
            corpse = -1;

        _give_experience(player_exp, monster_exp, killer, killer_index,
                         pet_kill, was_visible);
        crawl_state.dec_mon_acting(mons);

        return corpse;
    }

    mons_remove_from_grid(mons);
    _fire_monster_death_event(mons, killer, killer_index, false);

    if (crawl_state.game_is_arena())
        arena_monster_died(mons, killer, killer_index, silent, corpse);

    // Monsters haloes should be removed when they die.
    if (mons->holiness() == MH_HOLY)
        invalidate_agrid();
    // Likewise silence and umbras
    if (mons->type == MONS_SILENT_SPECTRE
        || mons->type == MONS_PROFANE_SERVITOR
        || mons->type == MONS_LOST_SOUL)
    {
        invalidate_agrid();
    }

    // Done before items are dropped so that we can clone them
    if (mons->holiness() == MH_NATURAL
        && killer != KILL_RESET
        && killer != KILL_DISMISSED
        && killer != KILL_BANISHED
        && _lost_soul_nearby(mons->pos()))
    {
        lost_soul_spectralize(mons);
    }

    const coord_def mwhere = mons->pos();
    if (drop_items)
        monster_drop_things(mons, YOU_KILL(killer) || pet_kill);
    else
    {
        // Destroy the items belonging to MF_HARD_RESET monsters so they
        // don't clutter up mitm[].
        mons->destroy_inventory();
    }

    if (!silent && !wizard && !mons_reset && corpse != -1
        && !fake_abjuration
        && !timeout
        && !unsummoned
        && !(mons->flags & MF_KNOWN_SHIFTER)
        && mons->is_shapeshifter())
    {
        simple_monster_message(mons, "'s shape twists and changes as "
                               "it dies.");
    }

    if (mons->is_divine_companion()
        && killer != KILL_RESET
        && !(mons->flags & MF_BANISHED))
    {
        remove_companion(mons);
    }

    // If we kill an invisible monster reactivate autopickup.
    // We need to check for actual invisibility rather than whether we
    // can see the monster. There are several edge cases where a monster
    // is visible to the player but we still need to turn autopickup
    // back on, such as TSO's halo or sticky flame. (jpeg)
    if (mons_near(mons) && mons->has_ench(ENCH_INVIS))
        autotoggle_autopickup(false);

    if (corpse != -1 && _reaping(mons))
        corpse = -1;

    crawl_state.dec_mon_acting(mons);
    monster_cleanup(mons);

    // Force redraw for monsters that die.
    if (in_bounds(mwhere) && you.see_cell(mwhere))
    {
        view_update_at(mwhere);
        update_screen();
    }

    if (gives_xp)
    {
        _give_experience(player_exp, monster_exp, killer, killer_index,
                         pet_kill, was_visible);
    }

    return corpse;
}

void unawaken_vines(const monster* mons, bool quiet)
{
    int vines_seen = 0;
    for (monster_iterator mi; mi; ++mi)
    {
        if (mi->type == MONS_SNAPLASHER_VINE
            && mi->props.exists("vine_awakener")
            && monster_by_mid(mi->props["vine_awakener"].get_int()) == mons)
        {
            if (you.can_see(*mi))
                ++vines_seen;
            monster_die(*mi, KILL_RESET, NON_MONSTER);
        }
    }

    if (!quiet && vines_seen)
    {
        mprf("The vine%s fall%s limply to the ground.",
              (vines_seen > 1 ? "s" : ""), (vines_seen == 1 ? "s" : ""));
    }
}

// Clean up after a dead monster.
void monster_cleanup(monster* mons)
{
    crawl_state.mon_gone(mons);

    if (mons->has_ench(ENCH_AWAKEN_FOREST))
    {
        forest_message(mons->pos(), "The forest abruptly stops moving.");
        env.forest_awoken_until = 0;
    }

    if (mons->has_ench(ENCH_AWAKEN_VINES))
        unawaken_vines(mons, false);

    // So that a message is printed for the effect ending
    if (mons->has_ench(ENCH_CONTROL_WINDS))
        mons->del_ench(ENCH_CONTROL_WINDS);

    // So proper messages are printed
    if (mons->has_ench(ENCH_GRASPING_ROOTS_SOURCE))
        mons->del_ench(ENCH_GRASPING_ROOTS_SOURCE);

    // May have been constricting something. No message because that depends
    // on the order in which things are cleaned up: If the constrictee is
    // cleaned up first, we wouldn't get a message anyway.
    mons->stop_constricting_all(false, true);

    if (mons_is_tentacle_head(mons_base_type(mons)))
        _destroy_tentacles(mons);

    env.mid_cache.erase(mons->mid);
    unsigned int monster_killed = mons->mindex();
    mons->reset();

    for (monster_iterator mi; mi; ++mi)
        if (mi->foe == monster_killed)
            mi->foe = MHITNOT;

    if (you.pet_target == monster_killed)
        you.pet_target = MHITNOT;
}

int mounted_kill(monster* daddy, monster_type mc, killer_type killer,
                int killer_index)
{
    monster mon;
    mon.type = mc;
    mon.moveto(daddy->pos());
    define_monster(&mon);
    mon.flags = daddy->flags;
    mon.attitude = daddy->attitude;
    mon.damage_friendly = daddy->damage_friendly;
    mon.damage_total = daddy->damage_total;
    // Keep the rider's name, if it had one (Mercenary card).
    if (!daddy->mname.empty() && mon.type == MONS_SPRIGGAN)
        mon.mname = daddy->mname;
    if (daddy->props.exists("reaping_damage"))
    {
        dprf("Mounted kill: marking the other monster as reaped as well.");
        mon.props["reaping_damage"].get_int() = daddy->props["reaping_damage"].get_int();
        mon.props["reaper"].get_int() = daddy->props["reaper"].get_int();
    }

    return monster_die(&mon, killer, killer_index, false, false, true);
}

// If you're invis and throw/zap whatever, alerts menv to your position.
void alert_nearby_monsters(void)
{
    // Judging from the above comment, this function isn't
    // intended to wake up monsters, so we're only going to
    // alert monsters that aren't sleeping.  For cases where an
    // event should wake up monsters and alert them, I'd suggest
    // calling noisy() before calling this function. - bwr
    for (monster_near_iterator mi(you.pos()); mi; ++mi)
        if (!mi->asleep())
             behaviour_event(*mi, ME_ALERT, &you);
}

static bool _valid_morph(monster* mons, monster_type new_mclass)
{
    const dungeon_feature_type current_tile = grd(mons->pos());

    monster_type old_mclass = mons_base_type(mons);

    // Shapeshifters cannot polymorph into glowing shapeshifters or
    // vice versa.
    if ((new_mclass == MONS_GLOWING_SHAPESHIFTER
             && mons->has_ench(ENCH_SHAPESHIFTER))
         || (new_mclass == MONS_SHAPESHIFTER
             && mons->has_ench(ENCH_GLOWING_SHAPESHIFTER)))
    {
        return false;
    }

    // [ds] Non-base draconians are much more trouble than their HD
    // suggests.
    if (mons_genus(new_mclass) == MONS_DRACONIAN
        && new_mclass != MONS_DRACONIAN
        && !player_in_branch(BRANCH_ZOT)
        && !one_chance_in(10))
    {
        return false;
    }

    // Various inappropriate polymorph targets.
    if (mons_class_holiness(new_mclass) != mons_class_holiness(old_mclass)
        || mons_class_flag(new_mclass, M_UNFINISHED)  // no unfinished monsters
        || mons_class_flag(new_mclass, M_CANT_SPAWN)  // no dummy monsters
        || mons_class_flag(new_mclass, M_NO_POLY_TO)  // explicitly disallowed
        || mons_class_flag(new_mclass, M_UNIQUE)      // no uniques
        || mons_class_flag(new_mclass, M_NO_EXP_GAIN) // not helpless
        || new_mclass == MONS_PROGRAM_BUG

        // 'morph targets are _always_ "base" classes, not derived ones.
        || new_mclass != mons_species(new_mclass)
        || new_mclass == mons_species(old_mclass)
        // They act as separate polymorph classes on their own.
        || mons_class_is_zombified(new_mclass)
        || mons_is_zombified(mons) && !mons_zombie_size(new_mclass)
        // Currently unused (no zombie shapeshifters, no polymorph).
        || mons->type == MONS_SKELETON && !mons_skeleton(new_mclass)
        || mons->type == MONS_ZOMBIE && !mons_zombifiable(new_mclass)

        // These require manual setting of the ghost demon struct to
        // indicate their characteristics, which we currently aren't
        // smart enough to handle.
        || mons_is_ghost_demon(new_mclass)

        // Other poly-unsuitable things.
        || mons_is_statue(new_mclass)
        || mons_is_projectile(new_mclass)
        || mons_is_tentacle_or_tentacle_segment(new_mclass)

        // Don't polymorph things without Gods into priests.
        || (mons_class_flag(new_mclass, MF_PRIEST) && mons->god == GOD_NO_GOD)
        // The spell on Prince Ribbit can't be broken so easily.
        || (new_mclass == MONS_HUMAN
            && (mons->type == MONS_PRINCE_RIBBIT
                || mons->mname == "Prince Ribbit")))
    {
        return false;
    }

    // Determine if the monster is happy on current tile.
    return monster_habitable_grid(new_mclass, current_tile);
}

static bool _is_poly_power_unsuitable(poly_power_type power,
                                       int src_pow, int tgt_pow, int relax)
{
    switch (power)
    {
    case PPT_LESS:
        return (tgt_pow > src_pow - 3 + relax * 3 / 2)
                || (power == PPT_LESS && (tgt_pow < src_pow - relax / 2));
    case PPT_MORE:
        return (tgt_pow < src_pow + 2 - relax)
                || (power == PPT_MORE && (tgt_pow > src_pow + relax));
    default:
    case PPT_SAME:
        return (tgt_pow < src_pow - relax)
                || (tgt_pow > src_pow + relax * 3 / 2);
    }
}

static bool _jiyva_slime_target(monster_type targetc)
{
    return you_worship(GOD_JIYVA)
           && (targetc == MONS_DEATH_OOZE
              || targetc == MONS_OOZE
              || targetc == MONS_JELLY
              || targetc == MONS_BROWN_OOZE
              || targetc == MONS_SLIME_CREATURE
              || targetc == MONS_GIANT_AMOEBA
              || targetc == MONS_ACID_BLOB
              || targetc == MONS_AZURE_JELLY);

}

void change_monster_type(monster* mons, monster_type targetc)
{
    bool could_see     = you.can_see(mons);
    bool degenerated = (targetc == MONS_PULSATING_LUMP);
    bool slimified = _jiyva_slime_target(targetc);

    // Quietly remove the old monster's invisibility before transforming
    // it.  If we don't do this, it'll stay invisible even after losing
    // the invisibility enchantment below.
    mons->del_ench(ENCH_INVIS, false, false);

    // Remove replacement tile, since it probably doesn't work for the
    // new monster.
    mons->props.erase("monster_tile_name");
    mons->props.erase("monster_tile");

    // Even if the monster transforms from one type that can behold the
    // player into a different type which can also behold the player,
    // the polymorph disrupts the beholding process.  Do this before
    // changing mons->type, since unbeholding can only happen while
    // the monster is still a mermaid/siren.
    you.remove_beholder(mons);
    you.remove_fearmonger(mons);

    if (mons_is_tentacle_head(mons_base_type(mons)))
        _destroy_tentacles(mons);

    // Inform listeners that the original monster is gone.
    _fire_monster_death_event(mons, KILL_MISC, NON_MONSTER, true);

    // the actual polymorphing:
    uint64_t flags =
        mons->flags & ~(MF_INTERESTING | MF_SEEN | MF_ATT_CHANGE_ATTEMPT
                           | MF_WAS_IN_VIEW | MF_BAND_MEMBER | MF_KNOWN_SHIFTER
                           | MF_MELEE_MASK | MF_SPELL_MASK);
    flags |= MF_POLYMORPHED;
    string name;

    // Preserve the names of uniques and named monsters.
    if (mons->type == MONS_ROYAL_JELLY
        || mons->mname == "shaped Royal Jelly")
    {
        name   = "shaped Royal Jelly";
        flags |= MF_INTERESTING | MF_NAME_SUFFIX;
    }
    else if (mons->type == MONS_LERNAEAN_HYDRA
             || mons->mname == "shaped Lernaean hydra")
    {
        name   = "shaped Lernaean hydra";
        flags |= MF_INTERESTING | MF_NAME_SUFFIX;
    }
    else if (mons->mons_species() == MONS_SERPENT_OF_HELL
             || mons->mname == "shaped Serpent of Hell")
    {
        name   = "shaped Serpent of Hell";
        flags |= MF_INTERESTING | MF_NAME_SUFFIX;
    }
    else if (!mons->mname.empty())
    {
        if (flags & MF_NAME_MASK)
        {
            // Remove the replacement name from the new monster
            flags = flags & ~(MF_NAME_MASK | MF_NAME_DESCRIPTOR
                              | MF_NAME_DEFINITE | MF_NAME_SPECIES
                              | MF_NAME_ZOMBIE | MF_NAME_NOCORPSE);
        }
        else
            name = mons->mname;
    }
    else if (mons_is_unique(mons->type))
    {
        flags |= MF_INTERESTING;

        name = mons->name(DESC_PLAIN, true);

        // "Blork the orc" and similar.
        const size_t the_pos = name.find(" the ");
        if (the_pos != string::npos)
            name = name.substr(0, the_pos);
    }

    const monster_type real_targetc =
        (mons->has_ench(ENCH_GLOWING_SHAPESHIFTER)) ? MONS_GLOWING_SHAPESHIFTER :
        (mons->has_ench(ENCH_SHAPESHIFTER))         ? MONS_SHAPESHIFTER
                                                    : targetc;

    const god_type god =
        (player_will_anger_monster(real_targetc)
            || (you_worship(GOD_BEOGH)
                && mons_genus(real_targetc) != MONS_ORC)) ? GOD_NO_GOD
                                                          : mons->god;

    if (god == GOD_NO_GOD)
        flags &= ~MF_GOD_GIFT;

    const int  old_hp             = mons->hit_points;
    const int  old_hp_max         = mons->max_hit_points;
    const bool old_mon_caught     = mons->caught();
    const char old_ench_countdown = mons->ench_countdown;

    // XXX: mons_is_unique should be converted to monster::is_unique, and that
    // function should be testing the value of props["original_was_unique"]
    // which would make things a lot simpler.
    // See also record_monster_defeat.
    bool old_mon_unique           = mons_is_unique(mons->type);
    if (mons->props.exists("original_was_unique")
        && mons->props["original_was_unique"].get_bool())
    {
        old_mon_unique = true;
    }

    mon_enchant abj       = mons->get_ench(ENCH_ABJ);
    mon_enchant fabj      = mons->get_ench(ENCH_FAKE_ABJURATION);
    mon_enchant charm     = mons->get_ench(ENCH_CHARM);
    mon_enchant shifter   = mons->get_ench(ENCH_GLOWING_SHAPESHIFTER,
                                           ENCH_SHAPESHIFTER);
    mon_enchant sub       = mons->get_ench(ENCH_SUBMERGED);
    mon_enchant summon    = mons->get_ench(ENCH_SUMMON);
    mon_enchant tp        = mons->get_ench(ENCH_TP);

    monster_spells spl    = mons->spells;
    const bool need_save_spells
            = (!name.empty()
               && (!mons->can_use_spells() || mons->is_actual_spellcaster())
               && !degenerated && !slimified);

    mons->number       = 0;

    // Note: define_monster() will clear out all enchantments! - bwr
    if (mons_is_zombified(mons))
        define_zombie(mons, targetc, mons->type);
    else
    {
        mons->type         = targetc;
        mons->base_monster = MONS_NO_MONSTER;
        define_monster(mons);
    }

    mons->mname = name;
    mons->props["original_name"] = name;
    mons->props["original_was_unique"] = old_mon_unique;
    mons->god   = god;
    mons->props.erase("dbname");

    mons->flags = flags;
    // Line above might clear melee and/or spell flags; restore.
    mons->bind_melee_flags();
    mons->bind_spell_flags();

    // Forget various speech/shout Lua functions.
    mons->props.erase("speech_prefix");

    // Keep spells for named monsters, but don't override innate ones
    // for dragons and the like. This means that Sigmund polymorphed
    // into a goblin will still cast spells, but if he ends up as a
    // swamp drake he'll breathe fumes and, if polymorphed further,
    // won't remember his spells anymore.
    if (need_save_spells
        && (!mons->can_use_spells() || mons->is_actual_spellcaster()))
    {
        mons->spells = spl;
    }

    mons->add_ench(abj);
    mons->add_ench(fabj);
    mons->add_ench(charm);
    mons->add_ench(shifter);
    mons->add_ench(sub);
    mons->add_ench(summon);
    mons->add_ench(tp);

    // Allows for handling of submerged monsters which polymorph into
    // monsters that can't submerge on this square.
    if (mons->has_ench(ENCH_SUBMERGED)
        && !monster_can_submerge(mons, grd(mons->pos())))
    {
        mons->del_ench(ENCH_SUBMERGED);
    }

    mons->ench_countdown = old_ench_countdown;

    if (mons_class_flag(mons->type, M_INVIS))
        mons->add_ench(ENCH_INVIS);

    mons->hit_points = mons->max_hit_points * old_hp / old_hp_max
                       + random2(mons->max_hit_points);

    mons->hit_points = min(mons->max_hit_points, mons->hit_points);

    // Don't kill it.
    mons->hit_points = max(mons->hit_points, 1);

    mons->speed_increment = 67 + random2(6);

    monster_drop_things(mons);

    // New monster type might be interesting.
    mark_interesting_monst(mons);

    // If new monster is visible to player, then we've seen it.
    if (you.can_see(mons))
    {
        seen_monster(mons);
        // If the player saw both the beginning and end results of a
        // shifter changing, then s/he knows it must be a shifter.
        if (could_see && shifter.ench != ENCH_NONE)
            discover_shifter(mons);
    }

    if (old_mon_caught)
        check_net_will_hold_monster(mons);

    // Even if the new form can constrict, it might be with a different
    // body part.  Likewise, the new form might be too large for its
    // current constrictor.  Rather than trying to handle these as special
    // cases, just stop the constriction entirely.  The usual message about
    // evaporating and reforming justifies this behaviour.
    mons->stop_constricting_all(false);
    mons->stop_being_constricted();

    mons->check_clinging(false);
}

// If targetc == RANDOM_MONSTER, then relpower indicates the desired
// power of the new monster, relative to the current monster.
// Relaxation still takes effect when needed, no matter what relpower
// says.
bool monster_polymorph(monster* mons, monster_type targetc,
                       poly_power_type power,
                       bool force_beh)
{
    // Don't attempt to polymorph a monster that is busy using the stairs.
    if (mons->flags & MF_TAKING_STAIRS)
        return false;
    ASSERT(!(mons->flags & MF_BANISHED) || player_in_branch(BRANCH_ABYSS));

    int source_power, target_power, relax;
    int source_tier, target_tier;
    int tries = 1000;

    // Used to be mons_power, but that just returns hit_dice
    // for the monster class.  By using the current hit dice
    // the player gets the opportunity to use draining more
    // effectively against shapeshifters. - bwr
    source_power = mons->hit_dice;
    source_tier = mons_demon_tier(mons->type);

    // There's not a single valid target on the '&' demon tier, so unless we
    // make one, let's ban this outright.
    if (source_tier == -1)
    {
        return simple_monster_message(mons,
            "'s appearance momentarily alters.");
    }
    relax = 1;

    if (targetc == RANDOM_MONSTER)
    {
        do
        {
            // Pick a monster species that's guaranteed happy at this grid.
            targetc = random_monster_at_grid(mons->pos(), true);

            target_power = mons_power(targetc);
            // Can't compare tiers in valid_morph, since we want to affect only
            // random polymorphs, and not absolutely, too.
            target_tier = mons_demon_tier(targetc);

            if (one_chance_in(200))
                relax++;

            if (relax > 50)
                return simple_monster_message(mons, " shudders.");
        }
        while (tries-- && (!_valid_morph(mons, targetc)
                           || source_tier != target_tier && !x_chance_in_y(relax, 200)
                           || _is_poly_power_unsuitable(power, source_power,
                                                        target_power, relax)));
    }

    bool could_see = you.can_see(mons);
    bool need_note = (could_see && MONST_INTERESTING(mons));
    string old_name_a = mons->full_name(DESC_A);
    string old_name_the = mons->full_name(DESC_THE);
    monster_type oldc = mons->type;

    if (targetc == RANDOM_TOUGHER_MONSTER)
    {
        vector<monster_type> target_types;
        for (int mc = 0; mc < NUM_MONSTERS; ++mc)
        {
            const monsterentry *me = get_monster_data((monster_type) mc);
            int delta = (int) me->hpdice[0] - mons->hit_dice;
            if (delta != 1)
                continue;
            if (!_valid_morph(mons, (monster_type) mc))
                continue;
            target_types.push_back((monster_type) mc);
        }
        if (target_types.empty())
            return false;

        shuffle_array(target_types);
        targetc = target_types[0];
    }

    if (!_valid_morph(mons, targetc))
        return simple_monster_message(mons, " looks momentarily different.");

    change_monster_type(mons, targetc);

    bool can_see = you.can_see(mons);

    // Messaging
    bool player_messaged = true;
    if (could_see)
    {
        string verb = "";
        string obj = "";

        if (!can_see)
            obj = "something you cannot see";
        else
        {
            obj = mons_type_name(targetc, DESC_A);
            if (targetc == MONS_PULSATING_LUMP)
                obj += " of flesh";
        }

        if (oldc == MONS_OGRE && targetc == MONS_TWO_HEADED_OGRE)
        {
            verb = "grows a second head";
            obj = "";
        }
        else if (mons->is_shapeshifter())
            verb = "changes into ";
        else if (targetc == MONS_PULSATING_LUMP)
            verb = "degenerates into ";
        else if (_jiyva_slime_target(targetc))
            verb = "quivers uncontrollably and liquefies into ";
        else
            verb = "evaporates and reforms as ";

        mprf("%s %s%s!", old_name_the.c_str(), verb.c_str(), obj.c_str());
    }
    else if (can_see)
    {
        mprf("%s appears out of thin air!", mons->name(DESC_A).c_str());
        autotoggle_autopickup(false);
    }
    else
        player_messaged = false;

    if (need_note || could_see && can_see && MONST_INTERESTING(mons))
    {
        string new_name = can_see ? mons->full_name(DESC_A)
                                  : "something unseen";

        take_note(Note(NOTE_POLY_MONSTER, 0, 0, old_name_a.c_str(),
                       new_name.c_str()));

        if (can_see)
            mons->flags |= MF_SEEN;
    }

    if (!force_beh)
        player_angers_monster(mons);

    // Xom likes watching monsters being polymorphed.
    if (can_see)
    {
        xom_is_stimulated(mons->is_shapeshifter()               ? 12 :
                          power == PPT_LESS || mons->friendly() ? 25 :
                          power == PPT_SAME                     ? 50
                                                                : 100);
    }

    return player_messaged;
}

// If the returned value is mon.pos(), then nothing was found.
static coord_def _random_monster_nearby_habitable_space(const monster& mon)
{
    const bool respect_sanctuary = mon.wont_attack();

    coord_def target;
    int tries;

    for (tries = 0; tries < 150; ++tries)
    {
        const coord_def delta(random2(13) - 6, random2(13) - 6);

        // Check that we don't get something too close to the
        // starting point.
        if (delta.origin())
            continue;

        // Blinks by 1 cell are not allowed.
        if (delta.rdist() == 1)
            continue;

        // Update target.
        target = delta + mon.pos();

        // Check that the target is valid and survivable.
        if (!in_bounds(target))
            continue;

        if (!monster_habitable_grid(&mon, grd(target)))
            continue;

        if (respect_sanctuary && is_sanctuary(target))
            continue;

        if (target == you.pos())
            continue;

        if (!cell_see_cell(mon.pos(), target, LOS_NO_TRANS))
            continue;

        // Survived everything, break out (with a good value of target.)
        break;
    }

    if (tries == 150)
        target = mon.pos();

    return target;
}

bool monster_blink(monster* mons, bool quiet)
{
    coord_def near = _random_monster_nearby_habitable_space(*mons);
    return mons->blink_to(near, quiet);
}

bool mon_can_be_slimified(monster* mons)
{
    const mon_holy_type holi = mons->holiness();

    return !(mons->flags & MF_GOD_GIFT)
           && !mons->is_insubstantial()
           && !mons_is_tentacle_or_tentacle_segment(mons->type)
           && (holi == MH_UNDEAD
                || holi == MH_NATURAL && !mons_is_slime(mons))
         ;
}

void slimify_monster(monster* mon, bool hostile)
{
    monster_type target = MONS_JELLY;

    const int x = mon->hit_dice + (coinflip() ? 1 : -1) * random2(5);

    if (x < 3)
        target = MONS_OOZE;
    else if (x >= 3 && x < 5)
        target = MONS_JELLY;
    else if (x >= 5 && x < 7)
        target = MONS_BROWN_OOZE;
    else if (x >= 7 && x <= 11)
    {
        if (coinflip())
            target = MONS_SLIME_CREATURE;
        else
            target = MONS_GIANT_AMOEBA;
    }
    else
    {
        if (coinflip())
            target = MONS_ACID_BLOB;
        else
            target = MONS_AZURE_JELLY;
    }

    if (feat_is_water(grd(mon->pos()))) // Pick something amphibious.
        target = (x < 7) ? MONS_JELLY : MONS_SLIME_CREATURE;

    if (mon->holiness() == MH_UNDEAD)
        target = MONS_DEATH_OOZE;

    // Bail out if jellies can't live here.
    if (!monster_habitable_grid(target, grd(mon->pos())))
    {
        simple_monster_message(mon, " quivers momentarily.");
        return;
    }

    monster_polymorph(mon, target);

    if (!mons_eats_items(mon))
        mon->add_ench(ENCH_EAT_ITEMS);

    if (!hostile)
        mon->attitude = ATT_STRICT_NEUTRAL;
    else
        mon->attitude = ATT_HOSTILE;

    mons_make_god_gift(mon, GOD_JIYVA);

    // Don't want shape-shifters to shift into non-slimes.
    mon->del_ench(ENCH_GLOWING_SHAPESHIFTER);
    mon->del_ench(ENCH_SHAPESHIFTER);

    mons_att_changed(mon);
}

void corrode_monster(monster* mons, const actor* evildoer)
{
    item_def *has_shield = mons->mslot_item(MSLOT_SHIELD);
    item_def *has_armour = mons->mslot_item(MSLOT_ARMOUR);

    if (!one_chance_in(3) && (has_shield || has_armour))
    {
        item_def &thing_chosen = (has_armour ? *has_armour : *has_shield);
        corrode_item(thing_chosen, mons);
    }
    else if (!one_chance_in(3) && !(has_shield || has_armour)
             && mons->can_bleed() && !mons->res_acid())
    {
        mons->add_ench(mon_enchant(ENCH_BLEED, 3, evildoer, (5 + random2(5))*10));

        if (you.can_see(mons))
        {
            mprf("%s writhes in agony as %s flesh is eaten away!",
                 mons->name(DESC_THE).c_str(),
                 mons->pronoun(PRONOUN_POSSESSIVE).c_str());
        }
    }
}

// Swap monster to this location.  Player is swapped elsewhere.
bool swap_places(monster* mons, const coord_def &loc)
{
    ASSERT(map_bounds(loc));
    ASSERT(monster_habitable_grid(mons, grd(loc)));

    if (monster_at(loc))
    {
        mpr("Something prevents you from swapping places.");
        return false;
    }

    mpr("You swap places.");

    mgrd(mons->pos()) = NON_MONSTER;

    mons->moveto(loc);

    mgrd(mons->pos()) = mons->mindex();

    return true;
}

// Returns true if this is a valid swap for this monster.  If true, then
// the valid location is set in loc. (Otherwise loc becomes garbage.)
bool swap_check(monster* mons, coord_def &loc, bool quiet)
{
    loc = you.pos();

    if (you.form == TRAN_TREE)
    {
        mpr("You can't move.");
        return false;
    }

    // Don't move onto dangerous terrain.
    if (is_feat_dangerous(grd(mons->pos())) && !you.can_cling_to(mons->pos()))
    {
        canned_msg(MSG_UNTHINKING_ACT);
        return false;
    }

    if (mons->is_projectile())
    {
        if (!quiet)
            mpr("It's unwise to walk into this.");
        return false;
    }

    if (mons->caught())
    {
        if (!quiet)
        {
            simple_monster_message(mons,
                make_stringf(" is %s!", held_status(mons)).c_str());
        }
        return false;
    }

    if (mons->is_constricted())
    {
        if (!quiet)
            simple_monster_message(mons, " is being constricted!");
        return false;
    }

    // First try: move monster onto your position.
    bool swap = !monster_at(loc) && monster_habitable_grid(mons, grd(loc));

    // Choose an appropriate habitat square at random around the target.
    if (!swap)
    {
        int num_found = 0;

        for (adjacent_iterator ai(mons->pos()); ai; ++ai)
            if (!monster_at(*ai) && monster_habitable_grid(mons, grd(*ai))
                && one_chance_in(++num_found))
            {
                loc = *ai;
            }

        if (num_found)
            swap = true;
    }

    if (!swap && !quiet)
    {
        // Might not be ideal, but it's better than insta-killing
        // the monster... maybe try for a short blink instead? - bwr
        simple_monster_message(mons, " cannot make way for you.");
        // FIXME: AI_HIT_MONSTER isn't ideal.
        interrupt_activity(AI_HIT_MONSTER, mons);
    }

    return swap;
}

// Given an adjacent monster, returns true if the monster can hit it
// (the monster should not be submerged, be submerged in shallow water
// if the monster has a polearm, or be submerged in anything if the
// monster has tentacles).
bool monster_can_hit_monster(monster* mons, const monster* targ)
{
    if (!summon_can_attack(mons, targ))
        return false;

    if (!targ->submerged() || mons->has_damage_type(DVORP_TENTACLE))
        return true;

    if (grd(targ->pos()) != DNGN_SHALLOW_WATER)
        return false;

    const item_def *weapon = mons->weapon();
    return weapon && weapon_skill(*weapon) == SK_POLEARMS;
}

// Friendly summons can't attack out of the player's LOS, it's too abusable.
bool summon_can_attack(const monster* mons)
{
    if (crawl_state.game_is_arena() || crawl_state.game_is_zotdef())
        return true;

    return !mons->friendly() || !mons->is_summoned()
           || you.see_cell_no_trans(mons->pos());
}

bool summon_can_attack(const monster* mons, const coord_def &p)
{
    if (crawl_state.game_is_arena() || crawl_state.game_is_zotdef())
        return true;

    // Spectral weapons only attack their target
    if (mons->type == MONS_SPECTRAL_WEAPON)
    {
        // FIXME: find a way to use check_target_spectral_weapon
        //        without potential info leaks about visibility.
        if (mons->props.exists(SW_TARGET_MID))
        {
            actor *target = actor_by_mid(mons->props[SW_TARGET_MID].get_int());
            return target && target->pos() == p;
        }
        return false;
    }

    if (!mons->friendly() || !mons->is_summoned())
        return true;

    return you.see_cell_no_trans(mons->pos()) && you.see_cell_no_trans(p);
}

bool summon_can_attack(const monster* mons, const actor* targ)
{
    return summon_can_attack(mons, targ->pos());
}

mon_dam_level_type mons_get_damage_level(const monster* mons)
{
    if (!mons_can_display_wounds(mons)
        || !mons_class_can_display_wounds(mons->get_mislead_type()))
    {
        return MDAM_OKAY;
    }

    if (mons->hit_points <= mons->max_hit_points / 5)
        return MDAM_ALMOST_DEAD;
    else if (mons->hit_points <= mons->max_hit_points * 2 / 5)
        return MDAM_SEVERELY_DAMAGED;
    else if (mons->hit_points <= mons->max_hit_points * 3 / 5)
        return MDAM_HEAVILY_DAMAGED;
    else if (mons->hit_points <= mons->max_hit_points * 4 / 5)
        return MDAM_MODERATELY_DAMAGED;
    else if (mons->hit_points < mons->max_hit_points)
        return MDAM_LIGHTLY_DAMAGED;
    else
        return MDAM_OKAY;
}

string get_damage_level_string(mon_holy_type holi, mon_dam_level_type mdam)
{
    ostringstream ss;
    switch (mdam)
    {
    case MDAM_ALMOST_DEAD:
        ss << "almost";
        ss << (_wounded_damaged(holi) ? " destroyed" : " dead");
        return ss.str();
    case MDAM_SEVERELY_DAMAGED:
        ss << "severely";
        break;
    case MDAM_HEAVILY_DAMAGED:
        ss << "heavily";
        break;
    case MDAM_MODERATELY_DAMAGED:
        ss << "moderately";
        break;
    case MDAM_LIGHTLY_DAMAGED:
        ss << "lightly";
        break;
    case MDAM_OKAY:
    default:
        ss << "not";
        break;
    }
    ss << (_wounded_damaged(holi) ? " damaged" : " wounded");
    return ss.str();
}

void print_wounds(const monster* mons)
{
    if (!mons->alive() || mons->hit_points == mons->max_hit_points)
        return;

    if (!mons_can_display_wounds(mons))
        return;

    mon_dam_level_type dam_level = mons_get_damage_level(mons);
    string desc = get_damage_level_string(mons->holiness(), dam_level);

    desc.insert(0, " is ");
    desc += ".";
    simple_monster_message(mons, desc.c_str(), MSGCH_MONSTER_DAMAGE,
                           dam_level);
}

// (true == 'damaged') [constructs, undead, etc.]
// and (false == 'wounded') [living creatures, etc.] {dlb}
static bool _wounded_damaged(mon_holy_type holi)
{
    // this schema needs to be abstracted into real categories {dlb}:
    return holi == MH_UNDEAD || holi == MH_NONLIVING || holi == MH_PLANT;
}

// If _mons_find_level_exits() is ever expanded to handle more grid
// types, this should be expanded along with it.
static void _mons_indicate_level_exit(const monster* mon)
{
    const dungeon_feature_type feat = grd(mon->pos());
    const bool is_shaft = (get_trap_type(mon->pos()) == TRAP_SHAFT);

    if (feat_is_gate(feat))
        simple_monster_message(mon, " passes through the gate.");
    else if (feat_is_travelable_stair(feat))
    {
        command_type dir = feat_stair_direction(feat);
        simple_monster_message(mon,
            make_stringf(" %s the %s.",
                dir == CMD_GO_UPSTAIRS     ? "goes up" :
                dir == CMD_GO_DOWNSTAIRS   ? "goes down"
                                           : "takes",
                feat_is_escape_hatch(feat) ? "escape hatch"
                                           : "stairs").c_str());
    }
    else if (is_shaft)
    {
        simple_monster_message(mon,
            make_stringf(" %s the shaft.",
                mons_flies(mon) ? "goes down"
                                : "jumps into").c_str());
    }
}

void make_mons_leave_level(monster* mon)
{
    if (mon->pacified())
    {
        if (you.can_see(mon))
        {
            _mons_indicate_level_exit(mon);
            remove_unique_annotation(mon);
        }

        // Pacified monsters leaving the level take their stuff with
        // them.
        mon->flags |= MF_HARD_RESET;
        monster_die(mon, KILL_DISMISSED, NON_MONSTER);
    }
}

static bool _can_safely_go_through(const monster * mon, const coord_def p)
{
    ASSERT(map_bounds(p));

    if (!monster_habitable_grid(mon, grd(p)))
        return false;

    // Stupid monsters don't pathfind around shallow water
    // except the clinging ones.
    if (mon->floundering_at(p)
        && (mons_intel(mon) >= I_NORMAL || mon->can_cling_to_walls()))
    {
        return false;
    }

    return true;
}

// Checks whether there is a straight path from p1 to p2 that monster can
// safely passes through.
// If it exists, such a path may be missed; on the other hand, it
// is not guaranteed that p2 is visible from p1 according to LOS rules.
// Not symmetric.
// FIXME: This is used for monster movement. It should instead be
//        something like exists_ray(p1, p2, opacity_monmove(mons)),
//        where opacity_monmove() is fixed to include opacity_immob.
bool can_go_straight(const monster* mon, const coord_def& p1,
                     const coord_def& p2)
{
    // If no distance, then trivially true
    if (p1 == p2)
        return true;

    if (distance2(p1, p2) > los_radius2)
        return false;

    // XXX: Hack to improve results for now. See FIXME above.
    ray_def ray;
    if (!find_ray(p1, p2, ray, opc_immob))
        return false;

    while (ray.advance() && ray.pos() != p2)
        if (!_can_safely_go_through(mon, ray.pos()))
            return false;

    return true;
}

// The default suitable() function for choose_random_nearby_monster().
bool choose_any_monster(const monster* mon)
{
    return !mons_is_projectile(mon->type);
}

// Find a nearby monster and return its index, including you as a
// possibility with probability weight.  suitable() should return true
// for the type of monster wanted.
// If prefer_named is true, named monsters (including uniques) are twice
// as likely to get chosen compared to non-named ones.
// If prefer_priest is true, priestly monsters (including uniques) are
// twice as likely to get chosen compared to non-priestly ones.
monster* choose_random_nearby_monster(int weight,
                                      bool (*suitable)(const monster* mon),
                                      bool prefer_named_or_priest)
{
    monster* chosen = NULL;
    for (radius_iterator ri(you.pos(), LOS_NO_TRANS); ri; ++ri)
    {
        monster* mon = monster_at(*ri);
        if (!mon || !suitable(mon))
            continue;

        // FIXME: if the intent is to favour monsters
        // named by $DEITY, we should set a flag on the
        // monster (something like MF_DEITY_PREFERRED) and
        // use that instead of checking the name, given
        // that other monsters can also have names.

        // True, but it's currently only used for orcs, and
        // Blork and Urug also being preferred to non-named orcs
        // is fine, I think. Once more gods name followers (and
        // prefer them) that should be changed, of course. (jpeg)
        int mon_weight = 1;

        if (prefer_named_or_priest)
            mon_weight += mon->is_named() + mon->is_priest();

        if (x_chance_in_y(mon_weight, weight += mon_weight))
            chosen = mon;
    }

    return chosen;
}

monster* choose_random_monster_on_level(int weight,
                                        bool (*suitable)(const monster* mon))
{
    monster* chosen = NULL;

    for (rectangle_iterator ri(1); ri; ++ri)
    {
        monster* mon = monster_at(*ri);
        if (!mon || !suitable(mon))
            continue;

        // Named or priestly monsters have doubled chances.
        int mon_weight = 1
                       + mon->is_named() + mon->is_priest();

        if (x_chance_in_y(mon_weight, weight += mon_weight))
            chosen = mon;
    }

    return chosen;
}

// Note that this function *completely* blocks messaging for monsters
// distant or invisible to the player ... look elsewhere for a function
// permitting output of "It" messages for the invisible {dlb}
// Intentionally avoids info and str_pass now. - bwr
bool simple_monster_message(const monster* mons, const char *event,
                            msg_channel_type channel,
                            int param,
                            description_level_type descrip)
{
    if (mons_near(mons)
        && (channel == MSGCH_MONSTER_SPELL || channel == MSGCH_FRIEND_SPELL
            || mons->visible_to(&you)))
    {
        string msg = mons->name(descrip);
        msg += event;
        msg = apostrophise_fixup(msg);

        if (channel == MSGCH_PLAIN && mons->wont_attack())
            channel = MSGCH_FRIEND_ACTION;

        mprf(channel, param, "%s", msg.c_str());
        return true;
    }

    return false;
}

static bool _mons_avoids_cloud(const monster* mons, const cloud_struct& cloud,
                               bool placement)
{
    bool extra_careful = placement;
    cloud_type cl_type = cloud.type;

    if (placement)
        extra_careful = true;

    // Berserk monsters are less careful and will blindly plow through any
    // dangerous cloud, just to kill you. {due}
    if (!extra_careful && mons->berserk_or_insane())
        return false;

    if (you_worship(GOD_FEDHAS) && fedhas_protects(mons)
        && (cloud.whose == KC_YOU || cloud.whose == KC_FRIENDLY)
        && (mons->friendly() || mons->neutral()))
    {
        return false;
    }

    switch (cl_type)
    {
    case CLOUD_MIASMA:
        // Even the dumbest monsters will avoid miasma if they can.
        return !mons->res_rotting();

    case CLOUD_FIRE:
    case CLOUD_FOREST_FIRE:
        if (mons->res_fire() > 1)
            return false;

        if (extra_careful)
            return true;

        if (mons_intel(mons) >= I_ANIMAL && mons->res_fire() < 0)
            return true;

        if (mons->hit_points >= 15 + random2avg(46, 5))
            return false;
        break;

    case CLOUD_MEPHITIC:
        if (mons->res_poison() > 0)
            return false;

        if (extra_careful)
            return true;

        if (mons_intel(mons) >= I_ANIMAL && mons->res_poison() < 0)
            return true;

        if (x_chance_in_y(mons->hit_dice - 1, 5))
            return false;

        if (mons->hit_points >= random2avg(19, 2))
            return false;
        break;

    case CLOUD_COLD:
        if (mons->res_cold() > 1)
            return false;

        if (extra_careful)
            return true;

        if (mons_intel(mons) >= I_ANIMAL && mons->res_cold() < 0)
            return true;

        if (mons->hit_points >= 15 + random2avg(46, 5))
            return false;
        break;

    case CLOUD_POISON:
        if (mons->res_poison() > 0)
            return false;

        if (extra_careful)
            return true;

        if (mons_intel(mons) >= I_ANIMAL && mons->res_poison() < 0)
            return true;

        if (mons->hit_points >= random2avg(37, 4))
            return false;
        break;

    case CLOUD_RAIN:
        // Fiery monsters dislike the rain.
        if (mons->is_fiery() && extra_careful)
            return true;

        // We don't care about what's underneath the rain cloud if we can fly.
        if (mons->flight_mode())
            return false;

        // These don't care about deep water.
        if (monster_habitable_grid(mons, DNGN_DEEP_WATER))
            return false;

        // This position could become deep water, and they might drown.
        if (grd(cloud.pos) == DNGN_SHALLOW_WATER)
            return true;
        break;

    case CLOUD_TORNADO:
        // Ball lightnings are not afraid of a _storm_, duh.  Or elementals.
        if (mons->res_wind())
            return false;

        // Locust swarms are too stupid to avoid winds.
        if (mons_intel(mons) >= I_ANIMAL)
            return true;
        break;

    case CLOUD_PETRIFY:
        if (mons->res_petrify())
            return false;

        if (extra_careful)
            return true;

        if (mons_intel(mons) >= I_ANIMAL)
            return true;
        break;

    case CLOUD_GHOSTLY_FLAME:
        if (mons->res_negative_energy() > 2)
            return false;

        if (extra_careful)
            return true;

        if (mons->hit_points >= random2avg(25, 3))
            return false;
        break;

    default:
        break;
    }

    // Exceedingly dumb creatures will wander into harmful clouds.
    if (is_harmless_cloud(cl_type)
        || mons_intel(mons) == I_PLANT && !extra_careful)
    {
        return false;
    }

    // If we get here, the cloud is potentially harmful.
    return true;
}

// Like the above, but allow a monster to move from one damaging cloud
// to another, even if they're of different types.
bool mons_avoids_cloud(const monster* mons, int cloud_num, bool placement)
{
    if (cloud_num == EMPTY_CLOUD)
        return false;

    const cloud_struct &cloud = env.cloud[cloud_num];

    // Is the target cloud okay?
    if (!_mons_avoids_cloud(mons, cloud, placement))
        return false;

    // If we're already in a cloud that we'd want to avoid then moving
    // from one to the other is okay.
    if (!in_bounds(mons->pos()) || mons->pos() == cloud.pos)
        return true;

    const int our_cloud_num = env.cgrid(mons->pos());

    if (our_cloud_num == EMPTY_CLOUD)
        return true;

    const cloud_struct &our_cloud = env.cloud[our_cloud_num];

    return !_mons_avoids_cloud(mons, our_cloud, true);
}

int mons_weapon_damage_rating(const item_def &launcher)
{
    return property(launcher, PWPN_DAMAGE) + launcher.plus2;
}

// Returns a rough estimate of damage from firing/throwing missile.
int mons_missile_damage(monster* mons, const item_def *launch,
                        const item_def *missile)
{
    if (!missile || (!launch && !is_throwable(mons, *missile)))
        return 0;

    const int missile_damage = property(*missile, PWPN_DAMAGE) / 2 + 1;
    const int launch_damage  = launch? property(*launch, PWPN_DAMAGE) : 0;
    return max(0, launch_damage + missile_damage);
}

int mons_usable_missile(monster* mons, item_def **launcher)
{
    *launcher = NULL;
    item_def *launch = NULL;
    for (int i = MSLOT_WEAPON; i <= MSLOT_ALT_WEAPON; ++i)
    {
        if (item_def *item = mons->mslot_item(static_cast<mon_inv_type>(i)))
        {
            if (is_range_weapon(*item))
                launch = item;
        }
    }

    const item_def *missiles = mons->missiles();
    if (launch && missiles && !missiles->launched_by(*launch))
        launch = NULL;

    const int fdam = mons_missile_damage(mons, launch, missiles);

    if (!fdam)
        return NON_ITEM;
    else
    {
        *launcher = launch;
        return missiles->index();
    }
}

// in units of 1/25 hp/turn
int mons_natural_regen_rate(monster* mons)
{
    // A HD divider ranging from 3 (at 1 HD) to 1 (at 8 HD).
    int divider = max(div_rand_round(15 - mons->hit_dice, 4), 1);

    // The undead have a harder time regenerating.  Golems have it worse.
    switch (mons->holiness())
    {
    case MH_UNDEAD:
        divider *= (mons_enslaved_soul(mons)) ? 2 : 4;
        break;

    case MH_NONLIVING:
        divider *= 5;
        break;

    default:
        break;
    }

    return max(div_rand_round(mons->hit_dice, divider), 1);
}

// in units of 1/100 hp/turn
int mons_off_level_regen_rate(monster* mons)
{
    if (!mons_can_regenerate(mons))
        return 0;

    if (mons_class_fast_regen(mons->type) || mons->type == MONS_PLAYER_GHOST)
        return 100;
    // Capped at 0.1 hp/turn.
    return max(mons_natural_regen_rate(mons) * 4, 10);
}

void mons_check_pool(monster* mons, const coord_def &oldpos,
                     killer_type killer, int killnum)
{
    // Flying/clinging monsters don't make contact with the terrain.
    if (!mons->ground_level())
        return;

    dungeon_feature_type grid = grd(mons->pos());
    if ((grid == DNGN_LAVA || grid == DNGN_DEEP_WATER)
        && !monster_habitable_grid(mons, grid))
    {
        const bool message = mons_near(mons);

        // Don't worry about invisibility.  You should be able to see if
        // something has fallen into the lava.
        if (message && (oldpos == mons->pos() || grd(oldpos) != grid))
        {
            mprf("%s falls into the %s!",
                 mons->name(DESC_THE).c_str(),
                 grid == DNGN_LAVA ? "lava" : "water");
        }

        if (grid == DNGN_LAVA && mons->res_fire() >= 2)
            grid = DNGN_DEEP_WATER;

        // Even fire resistant monsters perish in lava, but inanimate
        // monsters can survive deep water.
        if (grid == DNGN_LAVA || mons->can_drown())
        {
            if (message)
            {
                if (grid == DNGN_LAVA)
                {
                    simple_monster_message(mons, " is incinerated.",
                                           MSGCH_MONSTER_DAMAGE, MDAM_DEAD);
                }
                else if (mons_genus(mons->type) == MONS_MUMMY)
                {
                    simple_monster_message(mons, " falls apart.",
                                           MSGCH_MONSTER_DAMAGE, MDAM_DEAD);
                }
                else
                {
                    simple_monster_message(mons, " drowns.",
                                           MSGCH_MONSTER_DAMAGE, MDAM_DEAD);
                }
            }

            if (killer == KILL_NONE)
            {
                // Self-kill.
                killer  = KILL_MON;
                killnum = mons->mindex();
            }

            // Yredelemnul special, redux: It's the only one that can
            // work on drowned monsters.
            if (!_yred_enslave_soul(mons, killer))
                monster_die(mons, killer, killnum, true);
        }
        else
        {
            dprf("undrownable in Davy Jones' locker");
            mons->add_ench(ENCH_SUBMERGED);
        }
    }
}

monster* get_current_target()
{
    if (invalid_monster_index(you.prev_targ))
        return NULL;

    monster* mon = &menv[you.prev_targ];
    if (mon->alive() && you.can_see(mon))
        return mon;
    else
        return NULL;
}

void seen_monster(monster* mons)
{
    // If the monster is in the auto_exclude list, automatically
    // set an exclusion.
    set_auto_exclude(mons);
    set_unique_annotation(mons);

    item_def* weapon = mons->weapon();
    if (weapon && is_range_weapon(*weapon))
        mons->flags |= MF_SEEN_RANGED;

    // Monster was viewed this turn
    mons->flags |= MF_WAS_IN_VIEW;

    if (mons->flags & MF_SEEN)
        return;

    // First time we've seen this particular monster.
    mons->flags |= MF_SEEN;

    if (!mons_is_mimic(mons->type))
    {
        if (crawl_state.game_is_hints())
            hints_monster_seen(*mons);

        if (MONST_INTERESTING(mons))
        {
            string name = mons->name(DESC_A, true);
            if (mons->type == MONS_PLAYER_GHOST)
            {
                name += make_stringf(" (%s)",
                        short_ghost_description(mons, true).c_str());
            }
            take_note(
                      Note(NOTE_SEEN_MONSTER, mons->type, 0,
                           name.c_str()));
        }
    }

    if (!mons->has_ench(ENCH_ABJ)
        && !mons->has_ench(ENCH_FAKE_ABJURATION)
        && !testbits(mons->flags, MF_NO_REWARD)
        && !mons_class_flag(mons->type, M_NO_EXP_GAIN)
        && !crawl_state.game_is_arena())
    {
        did_god_conduct(DID_SEE_MONSTER, mons->hit_dice, true, mons);
    }

    if (mons_allows_beogh(mons))
        env.level_state |= LSTATE_BEOGH;
}

//---------------------------------------------------------------
//
// shift_monster
//
// Moves a monster to approximately p and returns true if
// the monster was moved.
//
//---------------------------------------------------------------
bool shift_monster(monster* mon, coord_def p)
{
    coord_def result;

    int count = 0;

    if (p.origin())
        p = mon->pos();

    for (adjacent_iterator ai(p); ai; ++ai)
    {
        // Don't drop on anything but vanilla floor right now.
        if (grd(*ai) != DNGN_FLOOR)
            continue;

        if (actor_at(*ai))
            continue;

        if (one_chance_in(++count))
            result = *ai;
    }

    if (count > 0)
    {
        mgrd(mon->pos()) = NON_MONSTER;
        mon->moveto(result);
        mgrd(result) = mon->mindex();
    }

    return count > 0;
}

// Make all of the monster's original equipment disappear, unless it's a fixed
// artefact or unrand artefact.
static void _vanish_orig_eq(monster* mons)
{
    for (int i = 0; i < NUM_MONSTER_SLOTS; ++i)
    {
        if (mons->inv[i] == NON_ITEM)
            continue;

        item_def &item(mitm[mons->inv[i]]);

        if (!item.defined())
            continue;

        if (item.orig_place != 0 || item.orig_monnum != 0
            || !item.inscription.empty()
            || is_unrandom_artefact(item)
            || (item.flags & (ISFLAG_DROPPED | ISFLAG_THROWN
                              | ISFLAG_NOTED_GET)))
        {
            continue;
        }
        item.flags |= ISFLAG_SUMMONED;
    }
}

int dismiss_monsters(string pattern)
{
    // Make all of the monsters' original equipment disappear unless "keepitem"
    // is found in the regex (except for fixed arts and unrand arts).
    const bool keep_item = strip_tag(pattern, "keepitem");
    const bool harmful = pattern == "harmful";
    const bool mobile  = pattern == "mobile";

    // Dismiss by regex.
    text_pattern tpat(pattern);
    int ndismissed = 0;
    for (monster_iterator mi; mi; ++mi)
    {
        if (mi->alive()
            && (mobile ? !mons_class_is_stationary(mi->type) :
                harmful ? !mons_is_firewood(*mi) && !mi->wont_attack()
                : tpat.empty() || tpat.matches(mi->name(DESC_PLAIN, true))))
        {
            if (!keep_item)
                _vanish_orig_eq(*mi);
            monster_die(*mi, KILL_DISMISSED, NON_MONSTER, false, true);
            ++ndismissed;
        }
    }

    return ndismissed;
}

// Does the equivalent of KILL_RESET on all monsters in LOS. Should only be
// applied to new games.
void zap_los_monsters(bool items_also)
{
    for (radius_iterator ri(you.pos(), LOS_SOLID); ri; ++ri)
    {
        if (items_also)
        {
            int item = igrd(*ri);

            if (item != NON_ITEM && mitm[item].defined())
                destroy_item(item);
        }

        // If we ever allow starting with a friendly monster,
        // we'll have to check here.
        monster* mon = monster_at(*ri);
        if (mon == NULL || mons_class_flag(mon->type, M_NO_EXP_GAIN))
            continue;

        dprf("Dismissing %s",
             mon->name(DESC_PLAIN, true).c_str());

        // Do a hard reset so the monster's items will be discarded.
        mon->flags |= MF_HARD_RESET;
        // Do a silent, wizard-mode monster_die() just to be extra sure the
        // player sees nothing.
        monster_die(mon, KILL_DISMISSED, NON_MONSTER, true, true);
    }
}

bool is_item_jelly_edible(const item_def &item)
{
    // Don't eat artefacts.
    if (is_artefact(item))
        return false;

    // Don't eat mimics.
    if (item.flags & ISFLAG_MIMIC)
        return false;

    // Shouldn't eat stone things
    //   - but what about wands and rings?
    if (item.base_type == OBJ_MISSILES
        && (item.sub_type == MI_STONE || item.sub_type == MI_LARGE_ROCK))
    {
        return false;
    }

    // Don't eat special game items.
    if (item.base_type == OBJ_ORBS
        || (item.base_type == OBJ_MISCELLANY
            && (item.sub_type == MISC_RUNE_OF_ZOT
                || item.sub_type == MISC_HORN_OF_GERYON)))
    {
        return false;
    }

    return true;
}

bool monster_space_valid(const monster* mons, coord_def target,
                         bool forbid_sanctuary)
{
    if (!in_bounds(target))
        return false;

    // Don't land on top of another monster.
    if (actor_at(target))
        return false;

    if (is_sanctuary(target) && forbid_sanctuary)
        return false;

    // Don't go into no_ctele_into or n_rtele_into cells.
    if (testbits(env.pgrid(target), FPROP_NO_CTELE_INTO))
        return false;
    if (testbits(env.pgrid(target), FPROP_NO_RTELE_INTO))
        return false;

    return monster_habitable_grid(mons, grd(target));
}

static bool _monster_random_space(const monster* mons, coord_def& target,
                                  bool forbid_sanctuary)
{
    int tries = 0;
    while (tries++ < 1000)
    {
        target = random_in_bounds();
        if (monster_space_valid(mons, target, forbid_sanctuary))
            return true;
    }

    return false;
}

void monster_teleport(monster* mons, bool instan, bool silent)
{
    bool was_seen = !silent && you.can_see(mons) && !mons_is_lurking(mons);

    if (!instan)
    {
        if (mons->del_ench(ENCH_TP))
        {
            if (!silent)
                simple_monster_message(mons, " seems more stable.");
        }
        else
        {
            if (!silent)
                simple_monster_message(mons, " looks slightly unstable.");

            mons->add_ench(mon_enchant(ENCH_TP, 0, 0,
                                       random_range(20, 30)));
        }

        return;
    }

    coord_def newpos;

    if (mons_is_shedu(mons) && shedu_pair_alive(mons))
    {
        // find a location close to its mate instead.
        monster* my_pair = get_shedu_pair(mons);
        coord_def pair = my_pair->pos();
        coord_def target;

        int tries = 0;
        while (tries++ < 1000)
        {
            target = coord_def(random_range(pair.x - 5, pair.x + 5),
                               random_range(pair.y - 5, pair.y + 5));

            if (grid_distance(target, pair) >= 10)
                continue;

            if (!monster_space_valid(mons, target, !mons->wont_attack()))
                continue;

            newpos = target;
            break;
        }

        if (newpos.origin())
            dprf("ERROR: Couldn't find a safe location near mate for Shedu!");
    }

    if (newpos.origin())
        _monster_random_space(mons, newpos, !mons->wont_attack());

    // XXX: If the above function didn't find a good spot, return now
    // rather than continue by slotting the monster (presumably)
    // back into its old location (previous behaviour). This seems
    // to be much cleaner and safer than relying on what appears to
    // have been a mistake.
    if (newpos.origin())
        return;

    if (!silent)
        simple_monster_message(mons, " disappears!");

    const coord_def oldplace = mons->pos();

    // Pick the monster up.
    mgrd(oldplace) = NON_MONSTER;

    // Move it to its new home.
    mons->moveto(newpos, true);

    // And slot it back into the grid.
    mgrd(mons->pos()) = mons->mindex();

    const bool now_visible = mons_near(mons);
    if (!silent && now_visible)
    {
        if (was_seen)
            simple_monster_message(mons, " reappears nearby!");
        else
        {
            // Even if it doesn't interrupt an activity (the player isn't
            // delayed, the monster isn't hostile) we still want to give
            // a message.
            activity_interrupt_data ai(mons, SC_TELEPORT_IN);
            if (!interrupt_activity(AI_SEE_MONSTER, ai))
                simple_monster_message(mons, " appears out of thin air!");
        }
    }

    if (mons->visible_to(&you) && now_visible)
        handle_seen_interrupt(mons);

    // Leave a purple cloud.
    // XXX: If silent is true, this is not an actual teleport, but
    //      the game moving a monster out of the way.
    if (!silent && !cell_is_solid(oldplace))
        place_cloud(CLOUD_TLOC_ENERGY, oldplace, 1 + random2(3), mons);

    mons->check_redraw(oldplace);
    mons->apply_location_effects(oldplace);

    mons_relocated(mons);

    shake_off_monsters(mons);
}

void mons_clear_trapping_net(monster* mon)
{
    if (!mon->caught())
        return;

    const int net = get_trapping_net(mon->pos());
    if (net != NON_ITEM)
        remove_item_stationary(mitm[net]);

    mon->del_ench(ENCH_HELD, true);
}

string summoned_poof_msg(const monster* mons, bool plural)
{
    int  summon_type = 0;
    bool valid_mon   = false;
    if (mons != NULL && !invalid_monster(mons))
    {
        (void) mons->is_summoned(NULL, &summon_type);
        valid_mon = true;
    }

    string msg      = "disappear%s in a puff of smoke";
    bool   no_chaos = false;

    switch (summon_type)
    {
    case SPELL_SHADOW_CREATURES:
        msg      = "dissolve%s into shadows";
        no_chaos = true;
        break;

    case MON_SUMM_CHAOS:
        msg = "degenerate%s into a cloud of primal chaos";
        break;

    case MON_SUMM_WRATH:
    case MON_SUMM_AID:
        if (valid_mon && is_good_god(mons->god))
        {
            msg      = "dissolve%s into sparkling lights";
            no_chaos = true;
        }
        break;

    case SPELL_GHOSTLY_FLAMES:
    case SPELL_CALL_LOST_SOUL:
        msg = "fades away";
        break;

    }

    if (valid_mon)
    {
        if (mons->god == GOD_XOM && !no_chaos && one_chance_in(10)
            || mons->type == MONS_CHAOS_SPAWN)
        {
            msg = "degenerate%s into a cloud of primal chaos";
        }

        if (mons->is_holy()
            && summon_type != SPELL_SHADOW_CREATURES
            && summon_type != MON_SUMM_CHAOS)
        {
            msg = "dissolve%s into sparkling lights";
        }

        if (mons_is_slime(mons)
            && mons->god == GOD_JIYVA)
        {
            msg = "dissolve%s into a puddle of slime";
        }
    }

    // Conjugate.
    msg = make_stringf(msg.c_str(), plural ? "" : "s");

    return msg;
}

string summoned_poof_msg(const int midx, const item_def &item)
{
    if (midx == NON_MONSTER)
        return summoned_poof_msg(static_cast<const monster* >(NULL), item);
    else
        return summoned_poof_msg(&menv[midx], item);
}

string summoned_poof_msg(const monster* mons, const item_def &item)
{
    ASSERT(item.flags & ISFLAG_SUMMONED);

    return summoned_poof_msg(mons, item.quantity > 1);
}

static bool _mons_reaped(actor *killer, monster* victim)
{
    beh_type beh;
    unsigned short hitting;

    if (killer->is_player())
    {
        hitting = MHITYOU;
        beh     = BEH_FRIENDLY;
    }
    else
    {
        monster* mon = killer->as_monster();

        beh = SAME_ATTITUDE(mon);

        // Get a new foe for the zombie to target.
        behaviour_event(mon, ME_EVAL);
        hitting = mon->foe;
    }

    monster *zombie = 0;
    if (animate_remains(victim->pos(), CORPSE_BODY, beh, hitting, killer, "",
                        GOD_NO_GOD, true, true, true, &zombie) <= 0)
    {
        return false;
    }

    if (you.can_see(victim))
        mprf("%s turns into a zombie!", victim->name(DESC_THE).c_str());
    else if (you.can_see(zombie))
        mprf("%s appears out of thin air!", zombie->name(DESC_THE).c_str());

    player_angers_monster(zombie);

    return true;
}

beh_type attitude_creation_behavior(mon_attitude_type att)
{
    switch (att)
    {
    case ATT_NEUTRAL:
        return BEH_NEUTRAL;
    case ATT_GOOD_NEUTRAL:
        return BEH_GOOD_NEUTRAL;
    case ATT_STRICT_NEUTRAL:
        return BEH_STRICT_NEUTRAL;
    case ATT_FRIENDLY:
        return BEH_FRIENDLY;
    default:
        return BEH_HOSTILE;
    }

}

// Return the creation behavior type corresponding to the input
// monsters actual attitude (i.e. ignoring monster enchantments).
beh_type actual_same_attitude(const monster& base)
{
    return attitude_creation_behavior(base.attitude);
}

// Called whenever an already existing monster changes its attitude, possibly
// temporarily.
void mons_att_changed(monster* mon)
{
    const mon_attitude_type att = mon->temp_attitude();

    if (mons_is_tentacle_head(mons_base_type(mon))
        || mon->type == MONS_ELDRITCH_TENTACLE)
    {
        for (monster_iterator mi; mi; ++mi)
            if (mi->is_child_tentacle_of(mon))
            {
                mi->attitude = att;
                if (mon->type != MONS_ELDRITCH_TENTACLE)
                {
                    for (monster_iterator connect; connect; ++connect)
                    {
                        if (connect->is_child_tentacle_of(mi->as_monster()))
                            connect->attitude = att;
                    }
                }

                // It's almost always flipping between hostile and friendly;
                // enslaving a pacified starspawn is still a shock.
                mi->stop_constricting_all();
            }
    }

    if (mon->attitude == ATT_HOSTILE
        && (mons_is_god_gift(mon, GOD_BEOGH)
           || mons_is_god_gift(mon, GOD_YREDELEMNUL)))
    {
        remove_companion(mon);
    }
}

void debuff_monster(monster* mon)
{
    // List of magical enchantments which will be dispelled.
    const enchant_type lost_enchantments[] =
    {
        ENCH_SLOW,
        ENCH_HASTE,
        ENCH_SWIFT,
        ENCH_MIGHT,
        ENCH_FEAR,
        ENCH_CONFUSION,
        ENCH_INVIS,
        ENCH_CORONA,
        ENCH_CHARM,
        ENCH_PARALYSIS,
        ENCH_PETRIFYING,
        ENCH_PETRIFIED,
        ENCH_REGENERATION,
        ENCH_STICKY_FLAME,
        ENCH_TP,
        ENCH_INNER_FLAME,
        ENCH_OZOCUBUS_ARMOUR
    };

    bool dispelled = false;

    // Dispel all magical enchantments...
    for (unsigned int i = 0; i < ARRAYSZ(lost_enchantments); ++i)
    {
        if (lost_enchantments[i] == ENCH_INVIS)
        {
            // ...except for natural invisibility.
            if (mons_class_flag(mon->type, M_INVIS))
                continue;
        }
        if (lost_enchantments[i] == ENCH_CONFUSION)
        {
            // Don't dispel permaconfusion.
            if (mons_class_flag(mon->type, M_CONFUSED))
                continue;
        }
        if (lost_enchantments[i] == ENCH_REGENERATION)
        {
            // Don't dispel regen if it's from Trog.
            if (mon->has_ench(ENCH_RAISED_MR))
                continue;
        }

        if (mon->del_ench(lost_enchantments[i], true, true))
            dispelled = true;
    }
    if (dispelled)
        simple_monster_message(mon, "'s magical effects unravel!");
}

// Return the number of monsters of the specified type.
// If friendlyOnly is true, only count friendly
// monsters, otherwise all of them
int count_monsters(monster_type mtyp, bool friendlyOnly)
{
    int count = 0;
    for (int mon = 0; mon < MAX_MONSTERS; mon++)
    {
        monster *mons = &menv[mon];
        if (mons->alive() && mons->type == mtyp
            && (!friendlyOnly || mons->friendly()))
        {
            count++;
        }
    }
    return count;
}

int count_allies()
{
    int count = 0;
    for (int mon = 0; mon < MAX_MONSTERS; mon++)
        if (menv[mon].alive() && menv[mon].friendly())
            count++;

    return count;
}

// Lava orcs!
int temperature()
{
    return (int) you.temperature;
}

int temperature_last()
{
    return (int) you.temperature_last;
}

void temperature_check()
{
    // Whether to ignore caps on incrementing temperature
    bool ignore_cap = you.duration[DUR_BERSERK];

    // These numbers seem to work pretty well, but they're definitely experimental:
    int tension = get_tension(GOD_NO_GOD); // Raw tension

    // It would generally be better to handle this at the tension level and have temperature much more closely tied to tension.

    // For testing, but super handy for that!
    // mprf("Tension value: %d", tension);

    // Increment temp to full if you're in lava.
    if (feat_is_lava(env.grid(you.pos())) && you.ground_level())
    {
        // If you're already very hot, no message,
        // but otherwise it lets you know you're being
        // brought up to max temp.
        if (temperature() <= TEMP_FIRE)
            mpr("The lava instantly superheats you.");
        you.temperature = TEMP_MAX;
        ignore_cap = true;
        // Otherwise, your temperature naturally decays.
    }
    else
        temperature_decay();

    // Follow this up with 1 additional decrement each turn until
    // you're not hot enough to boil water.
    if (feat_is_water(env.grid(you.pos())) && you.ground_level()
        && temperature_effect(LORC_PASSIVE_HEAT))
    {
        temperature_decrement(1);

        for (adjacent_iterator ai(you.pos()); ai; ++ai)
        {
            const coord_def p(*ai);
            if (in_bounds(p)
                && env.cgrid(p) == EMPTY_CLOUD
                && !cell_is_solid(p)
                && one_chance_in(5))
            {
                place_cloud(CLOUD_STEAM, *ai, 2 + random2(5), &you);
            }
        }
    }

    // Next, add temperature from tension. Can override temperature loss from water!
    temperature_increment(tension);

    // Cap net temperature change to 1 per turn if no exceptions.
    float tempchange = you.temperature - you.temperature_last;
    if (!ignore_cap && tempchange > 1)
        you.temperature = you.temperature_last + 1;
    else if (tempchange < -1)
        you.temperature = you.temperature_last - 1;

    // Handle any effects that change with temperature.
    temperature_changed(tempchange);

    // Save your new temp as your new 'old' temperature.
    you.temperature_last = you.temperature;
}

void temperature_increment(float degree)
{
    // No warming up while you're exhausted!
    if (you.duration[DUR_EXHAUSTED])
        return;

    you.temperature += sqrt(degree);
    if (temperature() >= TEMP_MAX)
        you.temperature = TEMP_MAX;
}

void temperature_decrement(float degree)
{
    // No cooling off while you're angry!
    if (you.duration[DUR_BERSERK])
        return;

    you.temperature -= degree;
    if (temperature() <= TEMP_MIN)
        you.temperature = TEMP_MIN;
}

void temperature_changed(float change)
{
    // Arbitrary - how big does a swing in a turn have to be?
    float pos_threshold = .25;
    float neg_threshold = -1 * pos_threshold;

    // For INCREMENTS:

    // Check these no-nos every turn.
    if (you.temperature >= TEMP_WARM)
    {
        // Handles condensation shield, ozo's armour, icemail.
        expose_player_to_element(BEAM_FIRE, 0);

        // Handled separately because normally heat doesn't affect this.
        if (you.form == TRAN_ICE_BEAST || you.form == TRAN_STATUE)
            untransform(true, false);
    }

    // Just reached the temp that kills off stoneskin.
    if (change > pos_threshold && temperature_tier(TEMP_WARM))
    {
        mprf(MSGCH_DURATION, "Your stony skin melts.");
        you.redraw_armour_class = true;
    }

    // Passive heat stuff.
    if (change > pos_threshold && temperature_tier(TEMP_FIRE))
        mprf(MSGCH_DURATION, "You're getting fired up.");

    // Heat aura stuff.
    if (change > pos_threshold && temperature_tier(TEMP_MAX))
    {
        mprf(MSGCH_DURATION, "You blaze with the fury of an erupting volcano!");
        invalidate_agrid(true);
    }

    // For DECREMENTS (reverse order):
    if (change < neg_threshold && temperature_tier(TEMP_MAX))
        mprf(MSGCH_DURATION, "The intensity of your heat diminishes.");

    if (change < neg_threshold && temperature_tier(TEMP_FIRE))
        mprf(MSGCH_DURATION, "You're cooling off.");

    // Cooled down enough for stoneskin to kick in again.
    if (change < neg_threshold && temperature_tier(TEMP_WARM))
    {
            mprf(MSGCH_DURATION, "Your skin cools and hardens.");
            you.redraw_armour_class = true;
    }

    // If we're in this function, temperature changed, anyways.
    you.redraw_temperature = true;

    #ifdef USE_TILE
        init_player_doll();
    #endif

    // Just do this every turn to be safe. Can be fixed later if there
    // any performance issues.
    invalidate_agrid(true);
}

void temperature_decay()
{
    temperature_decrement(you.temperature / 10);
}

// Just a helper function to save space. Returns true if a
// threshold was crossed.
bool temperature_tier (int which)
{
    if (temperature() > which && temperature_last() <= which)
        return true;
    else if (temperature() < which && temperature_last() >= which)
        return true;
    else
        return false;
}

bool temperature_effect(int which)
{
    switch (which)
    {
        case LORC_FIRE_RES_I:
            return true; // 1-15
        case LORC_STONESKIN:
            return temperature() < TEMP_WARM; // 1-8
//      case nothing, right now:
//            return (you.temperature >= TEMP_COOL && you.temperature < TEMP_WARM); // 5-8
        case LORC_LAVA_BOOST:
            return temperature() >= TEMP_WARM && temperature() < TEMP_HOT; // 9-10
        case LORC_FIRE_RES_II:
            return temperature() >= TEMP_WARM; // 9-15
        case LORC_FIRE_RES_III:
        case LORC_FIRE_BOOST:
        case LORC_COLD_VULN:
            return temperature() >= TEMP_HOT; // 11-15
        case LORC_PASSIVE_HEAT:
            return temperature() >= TEMP_FIRE; // 13-15
        case LORC_HEAT_AURA:
            if (you_worship(GOD_BEOGH))
                return false;
            // Deliberate fall-through.
        case LORC_NO_SCROLLS:
            return temperature() >= TEMP_MAX; // 15

        default:
            return false;
    }
}

int temperature_colour(int temp)
{
    return (temp > TEMP_FIRE) ? LIGHTRED  :
           (temp > TEMP_HOT)  ? RED       :
           (temp > TEMP_WARM) ? YELLOW    :
           (temp > TEMP_ROOM) ? WHITE     :
           (temp > TEMP_COOL) ? LIGHTCYAN :
           (temp > TEMP_COLD) ? LIGHTBLUE : BLUE;
}

string temperature_string(int temp)
{
    return (temp > TEMP_FIRE) ? "lightred"  :
           (temp > TEMP_HOT)  ? "red"       :
           (temp > TEMP_WARM) ? "yellow"    :
           (temp > TEMP_ROOM) ? "white"     :
           (temp > TEMP_COOL) ? "lightcyan" :
           (temp > TEMP_COLD) ? "lightblue" : "blue";
}

string temperature_text(int temp)
{
    switch (temp)
    {
        case TEMP_MIN:
            return "rF+";
        case TEMP_COOL:
            return "";
        case TEMP_WARM:
            return "rF++; lava magic boost; Stoneskin melts";
        case TEMP_HOT:
            return "rF+++; rC-; fire magic boost";
        case TEMP_FIRE:
            return "Burn attackers";
        case TEMP_MAX:
            return "Burn surroundings; cannot read scrolls";
        default:
            return "";
    }
}
