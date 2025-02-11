/**
 * @file
 * @brief Contains monster death functionality, including unique code.
**/

#include "AppHdr.h"

#include "mon-death.h"

#include "act-iter.h"
#include "areas.h"
#include "arena.h"
#include "artefact.h"
#include "art-enum.h"
#include "attitude-change.h"
#include "beam.h"
#include "bloodspatter.h"
#include "cloud.h"
#include "cluautil.h"
#include "colour.h"
#include "coordit.h"
#include "corpse.h"
#include "dactions.h"
#include "database.h"
#include "delay.h"
#include "describe.h"
#include "dgn-overview.h"
#include "english.h"
#include "env.h"
#include "fineff.h"
#include "god-abil.h"
#include "god-blessing.h"
#include "god-companions.h"
#include "god-conduct.h"
#include "god-passive.h" // passive_t::bless_followers, convert_orcs
#include "hints.h"
#include "hiscores.h"
#include "item-name.h"
#include "item-prop.h"
#include "item-status-flag-type.h"
#include "items.h"
#include "kills.h"
#include "libutil.h"
#include "mapdef.h"
#include "mapmark.h"
#include "message.h"
#include "mon-abil.h"
#include "mon-behv.h"
#include "mon-cast.h"
#include "mon-explode.h"
#include "mon-gear.h"
#include "mon-place.h"
#include "mon-poly.h"
#include "mon-speak.h"
#include "mon-tentacle.h"
#include "mutation.h"
#include "nearby-danger.h"
#include "notes.h"
#include "religion.h"
#include "shout.h"
#include "spl-damage.h"
#include "spl-monench.h"
#include "spl-other.h"
#include "spl-summoning.h"
#include "spl-selfench.h"
#include "sprint.h" // SPRINT_MULTIPLIER
#include "state.h"
#include "stepdown.h"
#include "stringutil.h"
#include "tag-version.h"
#include "target.h"
#include "terrain.h"
#ifdef USE_TILE
#include "rltiles/tiledef-player.h"
#endif
#include "tilepick.h"
#include "timed-effects.h"
#include "transform.h"
#include "traps.h"
#include "unwind.h"
#include "viewchar.h"
#include "view.h"

/**
 * Initialise a corpse item.
 *
 * @param mons The monster to use as a template.
 * @param corpse[out] The item that's filled in; no properties it already has
 *                    are checked.
 * @returns whether a corpse could be created.
 */
static bool _fill_out_corpse(const monster& mons, item_def& corpse)
{
    corpse.clear();

    monster_type mtype = mons.type;
    monster_type corpse_class = mons_species(mtype);

    ASSERT(!invalid_monster_type(mtype));
    ASSERT(!invalid_monster_type(corpse_class));

    if (mons_genus(mtype) == MONS_DRACONIAN)
    {
        if (mons.type == MONS_TIAMAT)
            corpse_class = MONS_DRACONIAN;
        else
            corpse_class = draconian_subspecies(mons);
    }

    if (mons.props.exists(ORIGINAL_TYPE_KEY))
    {
        // Shapeshifters too.
        mtype = (monster_type) mons.props[ORIGINAL_TYPE_KEY].get_int();
        corpse_class = mons_species(mtype);
    }

    if (!mons_class_can_leave_corpse(corpse_class))
        return false;

    corpse.base_type      = OBJ_CORPSES;
    corpse.mon_type       = corpse_class;
    corpse.sub_type       = CORPSE_BODY;
    corpse.freshness      = FRESHEST_CORPSE;  // rot time
    corpse.quantity       = 1;
    corpse.rnd            = 1 + random2(255);
    corpse.orig_monnum    = mtype;

    corpse.props[MONSTER_HIT_DICE] = short(mons.get_experience_level());
    if (mons.mons_species() == MONS_HYDRA)
        corpse.props[CORPSE_HEADS] = short(mons.heads());
    if (mons.props.exists(OLD_HEADS_KEY))
        corpse.props[CORPSE_HEADS] = short(mons.props[OLD_HEADS_KEY].get_int());
    COMPILE_CHECK(sizeof(mid_t) == sizeof(int));
    corpse.props[MONSTER_MID]      = int(mons.mid);

    monster_info minfo(corpse_class);
    int col = int(minfo.colour());
    if (col == COLOUR_UNDEF)
    {
        // XXX hack to avoid crashing in wiz mode.
        if (mons_is_ghost_demon(mons.type) && !mons.ghost)
            col = LIGHTRED;
        else
        {
            minfo = monster_info(&mons);
            col = int(minfo.colour());
        }
    }
    if (col == COLOUR_UNDEF)
        col = int(random_colour());
    corpse.props[FORCED_ITEM_COLOUR_KEY] = col;

    if (!mons.mname.empty() && !(mons.flags & MF_NAME_NOCORPSE))
    {
        corpse.props[CORPSE_NAME_KEY] = mons.mname;
        corpse.props[CORPSE_NAME_TYPE_KEY].get_int64() = mons.flags.flags;
    }
    else if (mons_is_unique(mtype))
    {
        corpse.props[CORPSE_NAME_KEY] = mons_type_name(mtype, DESC_PLAIN);
        corpse.props[CORPSE_NAME_TYPE_KEY].get_int64() = 0;
    }

    // Store mid of dead orc apostles so we can find their corpse again later.
    // (And note the floor they died on)
    if (mons.type == MONS_ORC_APOSTLE)
    {
        corpse.props[CORPSE_MID_KEY].get_int() = mons.mid;
        if (mons.is_divine_companion())
            corpse.props[BEOGH_BFB_VALID_KEY] = true;
    }

    return true;
}

static bool _explode_corpse(item_def& corpse, const coord_def& where)
{
    // Don't want results to show up behind the player.
    los_def ld(where, opc_no_actor);

    if (mons_class_leaves_organ(corpse.mon_type))
    {
        // Uh... magical organs are tough stuff and it keeps the monster in
        // one piece?  More importantly, it prevents a flavour feature
        // from becoming a trap for the unwary.

        return false;
    }

    ld.update();

    const int max_chunks = max_corpse_chunks(corpse.mon_type);
    const int nchunks = stepdown_value(1 + random2(max_chunks), 4, 4, 12, 12);

    if (corpse.base_type != OBJ_GOLD)
    {
        blood_spray(where, corpse.mon_type, nchunks * 3); // spray some blood
        return true;
    }

    const int total_gold = corpse.quantity;

    // spray gold everywhere!
    for (int ntries = 0, chunks_made = 0;
         chunks_made < nchunks && ntries < 10000; ++ntries)
    {
        coord_def cp = where;
        cp.x += random_range(-LOS_DEFAULT_RANGE, LOS_DEFAULT_RANGE);
        cp.y += random_range(-LOS_DEFAULT_RANGE, LOS_DEFAULT_RANGE);

        dprf("Trying to scatter gold to %d, %d...", cp.x, cp.y);

        if (!in_bounds(cp))
            continue;

        if (!ld.see_cell(cp))
            continue;

        dprf("Cell is visible...");

        if (cell_is_solid(cp) || actor_at(cp))
            continue;

        ++chunks_made;

        dprf("Success");

        if (corpse.base_type == OBJ_GOLD)
        {
            corpse.quantity = max(1, div_rand_round(total_gold, nchunks));
            copy_item_to_grid(corpse, cp);
        }
    }

    return true;
}

static int _calc_player_experience(const monster* mons)
{
    int experience = exper_value(*mons);
    if (!experience)
        return 0;

    // Already got the XP here.
    if (testbits(mons->flags, MF_PACIFIED))
        return 0;

    if (!mons->damage_total)
    {
        mprf(MSGCH_WARN, "Error, exp for monster with no damage: %s",
             mons->name(DESC_PLAIN, true).c_str());
        return 0;
    }

    experience = experience * mons->damage_friendly / mons->damage_total;
    ASSERT(mons->damage_friendly <= mons->damage_total);

    return experience;
}

static void _give_player_experience(int experience, killer_type killer,
                                    bool pet_kill, bool was_visible,
                                    xp_tracking_type xp_tracking)
{
    if (experience <= 0 || crawl_state.game_is_arena())
        return;

    const unsigned int exp_gain = gain_exp(experience);

    kill_category kc =
            (killer == KILL_YOU || killer == KILL_YOU_MISSILE) ? KC_YOU :
            (pet_kill)                                         ? KC_FRIENDLY :
                                                                 KC_OTHER;
    PlaceInfo& curr_PlaceInfo = you.get_place_info();
    PlaceInfo  delta;

    delta.mon_kill_num[kc]++;
    delta.mon_kill_exp += exp_gain;

    you.global_info += delta;
    you.global_info.assert_validity();

    curr_PlaceInfo += delta;
    curr_PlaceInfo.assert_validity();

    LevelXPInfo& curr_xp_info = you.get_level_xp_info();
    LevelXPInfo xp_delta;

    if (xp_tracking == XP_NON_VAULT)
    {
        xp_delta.non_vault_xp += exp_gain;
        xp_delta.non_vault_count++;
    }
    else if (xp_tracking == XP_VAULT)
    {
        xp_delta.vault_xp += exp_gain;
        xp_delta.vault_count++;
    }

    you.global_xp_info += xp_delta;
    you.global_xp_info.assert_validity();

    curr_xp_info += xp_delta;
    curr_xp_info.assert_validity();

    // Give a message for monsters dying out of sight.
    if (exp_gain > 0 && !was_visible)
        mpr("You feel a bit more experienced.");
}

/**
 * Makes an item into gold. Praise Gozag!
 *
 * Gold is random, but correlates weakly with monster mass.
 *
 * Also sets the gold distraction timer on the player.
 *
 * @param corpse[out] The item to be Midasified.
 */
static void _gold_pile(item_def &corpse, monster_type corpse_class)
{
    corpse.clear();

    int base_gold = 7;
    // monsters with more chunks than SIZE_MEDIUM give more than base gold
    const int extra_chunks = (max_corpse_chunks(corpse_class)
                              - max_corpse_chunks(MONS_HUMAN)) * 2;
    if (extra_chunks > 0)
        base_gold += extra_chunks;

    corpse.base_type = OBJ_GOLD;
    corpse.mon_type = corpse_class; // for _explode_corpse
    corpse.quantity = base_gold / 2 + random2avg(base_gold, 2);
    item_colour(corpse);

    // Apply the gold aura effect to the player.
    const int dur = 6 + random2avg(14, 2);
    if (dur > you.duration[DUR_GOZAG_GOLD_AURA])
        you.set_duration(DUR_GOZAG_GOLD_AURA, dur);

    // In sprint, increase the amount of gold from corpses (but not
    // the gold aura duration!)
    if (crawl_state.game_is_sprint())
        corpse.quantity *= SPRINT_MULTIPLIER;

    if (you.props[GOZAG_GOLD_AURA_KEY].get_int() < GOZAG_GOLD_AURA_MAX)
        ++you.props[GOZAG_GOLD_AURA_KEY].get_int();
    you.redraw_title = true;
}

static void _create_monster_hide(monster_type mtyp, monster_type montype,
                                 coord_def pos, bool silent)
{
    const armour_type type = hide_for_monster(mons_species(mtyp));
    ASSERT(type != NUM_ARMOURS);

    int o = items(false, OBJ_ARMOUR, type, 0);
    squash_plusses(o);

    if (o == NON_ITEM)
        return;
    item_def& item = env.item[o];

    if (!invalid_monster_type(montype) && mons_is_unique(montype))
        item.inscription = mons_type_name(montype, DESC_PLAIN);

    /// Slightly randomized bonus enchantment for certain uniques' hides
    static const map<monster_type, int> hide_avg_plusses = {
        { MONS_SNORG, 2 },
        { MONS_XTAHUA, 3 },
        { MONS_BAI_SUZHEN, 3 },
        { MONS_BAI_SUZHEN_DRAGON, 3 },
    };

    if (mons_species(mtyp) == MONS_DEEP_TROLL)
    {
        item.props[ITEM_TILE_NAME_KEY] = "deep_troll_leather";
        item.props[WORN_TILE_NAME_KEY] = "deep_troll_leather";
        bind_item_tile(item);
    }
    else if (mtyp == MONS_IRON_TROLL)
    {
        item.props[ITEM_TILE_NAME_KEY] = "iron_troll_leather";
        item.props[WORN_TILE_NAME_KEY] = "iron_troll_leather";
        bind_item_tile(item);
    }

    const int* bonus_plus = map_find(hide_avg_plusses, montype);
    if (bonus_plus)
        item.plus += random_range(*bonus_plus * 2/3, *bonus_plus * 3/2);

    if (pos.origin())
    {
        item.flags |= ISFLAG_IDENTIFIED;
        return;
    }

    move_item_to_grid(&o, pos);

    // Don't display this message if the scales were dropped over
    // lava/deep water, because then they are hardly intact.
    if (you.see_cell(pos) && !silent && !feat_eliminates_items(env.grid(pos)))
    {
        // XXX: tweak for uniques/named monsters, somehow?
        mprf("%s %s intact enough to wear.",
             item.name(DESC_THE).c_str(),
             mons_genus(mtyp) == MONS_DRAGON ? "are"  // scales are
                                             : "is"); // troll armour is
                                                      // XXX: refactor
    }

    item.flags |= ISFLAG_IDENTIFIED;
}

static void _create_monster_wand(monster_type mtyp, coord_def pos, bool silent)
{
    if (pos.origin())
        return;

    int w = items(false, OBJ_WANDS, OBJ_RANDOM,
                  mons_class_hit_dice(mtyp));

    if (w == NON_ITEM)
        return;
    item_def& item = env.item[w];
    move_item_to_grid(&w, pos);

    item.plus *= 2;

    if (you.see_cell(pos) && !silent && !feat_eliminates_items(env.grid(pos)))
    {
        mprf("%s bone magically twists into %s.",
             mons_type_name(mtyp, DESC_A).c_str(),
             item.name(DESC_A).c_str());
    }

    // Don't immediately gain knowledge of the wand if we died out of sight.
    item.flags |= ISFLAG_IDENTIFIED;
}

void maybe_drop_monster_organ(monster_type mon, monster_type orig,
                              coord_def pos, bool silent)
{
    if (mons_class_leaves_hide(mon) && !one_chance_in(3))
        _create_monster_hide(mon, orig, pos, silent);

    // corpse RNG is enough for these right now
    if (mons_class_leaves_wand(mon))
        _create_monster_wand(mon, pos, silent);
}

/**
 * Create this monster's corpse in env.item at its position.
 *
 * @param mons the monster to corpsify
 * @param silent whether to suppress all messages
 * @param force whether to always make a corpse (no 50% chance not to make a
                corpse, no goldification, no organs -- being summoned etc. still
  *             matters, though)
 * @returns a pointer to an item; it may be null, if the monster can't leave a
 *          corpse or if the 50% chance is rolled; it may be gold, if the player
 *          worships Gozag, or it may be the corpse.
 */
item_def* place_monster_corpse(const monster& mons, bool force)
{
    if (mons.is_abjurable()
        || mons.flags & MF_BANISHED
        // Follower apostles should drop corpses (but nothing else)
        || mons.flags & MF_HARD_RESET && !mons.is_divine_companion()
        || mons.props.exists(PIKEL_BAND_KEY))
    {
        return nullptr;
    }

    bool goldify = mons_will_goldify(mons) && !force;

    const bool no_coinflip = mons.props.exists(ALWAYS_CORPSE_KEY)
                             || force
                             || goldify;

    // 50/50 chance of getting a corpse, usually.
    if (!no_coinflip && coinflip())
        return nullptr;

    // The game can attempt to place a corpse for an out-of-bounds monster
    // if a shifter turns into a ballistomycete spore and explodes. In this
    // case we place no corpse since the explosion means anything left
    // over would be scattered, tiny chunks of shifter.
    if (!in_bounds(mons.pos()) && !force)
        return nullptr;

    // Don't attempt to place corpses within walls, either.
    if (feat_is_solid(env.grid(mons.pos())) && !force)
        return nullptr;

    // If we were told not to leave a corpse, don't.
    if (mons.props.exists(NEVER_CORPSE_KEY))
        return nullptr;

    int o = get_mitm_slot();

    if (o == NON_ITEM)
        return nullptr;

    item_def& corpse(env.item[o]);
    if (goldify)
    {
        _gold_pile(corpse, mons_species(mons.type));
        // If gold would be destroyed, give it directly to the player instead.
        if (feat_eliminates_items(env.grid(mons.pos())))
        {
            get_gold(corpse, corpse.quantity, false);
            destroy_item(corpse, true);
            return nullptr;
        }
    }
    else if (!_fill_out_corpse(mons, corpse))
        return nullptr;

    origin_set_monster(corpse, &mons);

    if ((mons.flags & MF_EXPLODE_KILL) && _explode_corpse(corpse, mons.pos()))
    {
        // The corpse itself shouldn't remain.
        item_was_destroyed(corpse);
        destroy_item(o);
        return nullptr;
    }

    if (in_bounds(mons.pos()))
        move_item_to_grid(&o, mons.pos(), !mons.swimming());

    if (o == NON_ITEM)
        return nullptr;

    // Preserve the corpses of your followers (helps to tell where they died)
    if (mons.type == MONS_ORC_APOSTLE)
        corpse.props[CORPSE_NEVER_DECAYS] = true;

    return &env.item[o];
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
           killer == KILL_BOUND ? "bound" :
           killer == KILL_SLIMIFIED ? "slimified" : "killed";
}

void record_monster_defeat(const monster* mons, killer_type killer)
{
    if (crawl_state.game_is_arena())
        return;
    if (RESET_KILL(killer))
        return;
    if (mons->is_summoned())
        return;
    if (mons->is_named() && mons->friendly()
        && !mons_is_hepliaklqana_ancestor(mons->type))
    {
        take_note(Note(NOTE_ALLY_DEATH, 0, 0, mons->name(DESC_PLAIN, true)));
    }
    else if (mons_is_notable(*mons))
    {
        take_note(Note(NOTE_DEFEAT_MONSTER, mons->type, mons->friendly(),
                       mons->full_name(DESC_A).c_str(),
                       _milestone_kill_verb(killer).c_str()));
    }
    if (mons->type == MONS_PLAYER_GHOST)
    {
        monster_info mi(mons);
        string milestone = _milestone_kill_verb(killer) + " the ghost of ";
        milestone += get_ghost_description(mi, true);
        milestone += ".";
        mark_milestone("ghost", milestone);
    }
    if (mons_is_or_was_unique(*mons) && !testbits(mons->flags, MF_SPECTRALISED))
    {
        mark_milestone("uniq",
                       _milestone_kill_verb(killer)
                       + " "
                       + mons->name(DESC_THE, true)
                       + ".");
    }
}

static bool _is_pet_kill(killer_type killer, int i)
{
    if (!MON_KILL(killer))
        return false;

    if (i == ANON_FRIENDLY_MONSTER)
        return true;

    if (invalid_monster_index(i))
        return false;

    const monster* m = &env.mons[i];
    // This includes charmed monsters.
    if (m->friendly() || m->attitude == ATT_MARIONETTE)
        return true;

    // Check if the monster was confused by you or a friendly, which
    // makes casualties to this monster collateral kills.
    const mon_enchant me = m->get_ench(ENCH_CONFUSION);
    const mon_enchant me2 = m->get_ench(ENCH_FRENZIED);
    return me.ench == ENCH_CONFUSION
           && (me.who == KC_YOU || me.who == KC_FRIENDLY)
           || me2.ench == ENCH_FRENZIED
              && (me2.who == KC_YOU || me2.who == KC_FRIENDLY);
}

// Returns whether damage from a given agent counts as a 'player source' for
// purposes of the player gaining XP from damage/kills they cause.
bool damage_contributes_xp(const actor& agent)
{
    const int killer = agent.mindex();

    if (killer == MHITYOU || killer == YOU_FAULTLESS)
        return true;

    if (_is_pet_kill(KILL_MON, killer))
        return true;

    return false;
}

// Elyvilon will occasionally (5% chance) protect the life of one of
// your holy or living allies.
static bool _ely_protect_ally(monster* mons, killer_type killer)
{
    ASSERT(mons); // XXX: change to monster &mons
    if (!have_passive(passive_t::protect_ally))
        return false;

    if (!MON_KILL(killer) && !YOU_KILL(killer))
        return false;

    if (mons->holiness() & ~(MH_HOLY | MH_NATURAL | MH_PLANT)
        || !mons->friendly()
        || !you.can_see(*mons) // for simplicity
        || !one_chance_in(20))
    {
        return false;
    }

    mons->hit_points = 1;

    const string msg = " protects " + mons->name(DESC_THE) + " from harm!";
    simple_god_message(msg.c_str(), false);

    return true;
}

// Elyvilon retribution effect: Heal hostile monsters that were about to
// be killed by you or one of your friends.
static bool _ely_heal_monster(monster* mons, killer_type killer, int i)
{
    god_type god = GOD_ELYVILON;

    if (!player_under_penance(god) || !god_hates_your_god(god))
        return false;

    if (mons->wont_attack()
        || mons->is_peripheral()
        || mons->props.exists(ELY_WRATH_HEALED_KEY)
        || mons->get_experience_level() < random2(you.experience_level)
        || !one_chance_in(3))
    {
        return false;
    }

    actor *act = &you;

    if (MON_KILL(killer) && !invalid_monster_index(i))
    {
        monster* mon = &env.mons[i];
        if (!mon->friendly())
            return false;

        if (!you.see_cell(mons->pos()))
            return false;

        act = mon;
    }
    else if (!YOU_KILL(killer))
        return false;

    dprf("monster hp: %d, max hp: %d", mons->hit_points, mons->max_hit_points);

    mons->hit_points = 1 + random2(mons->max_hit_points);
    mons->props[ELY_WRATH_HEALED_KEY] = true;

    dprf("new hp: %d", mons->hit_points);

    const string msg = make_stringf("%s heals %s%s",
             god_name(god, false).c_str(),
             mons->name(DESC_THE).c_str(),
             mons->hit_points * 2 <= mons->max_hit_points ? "." : "!");

    god_speaks(god, msg.c_str());

    behaviour_event(mons, ME_WHACK, act);

    lugonu_meddle_fineff::schedule();

    return true;
}

static bool _yred_bind_soul(monster* mons, killer_type killer)
{
    if (you_worship(GOD_YREDELEMNUL) && mons->has_ench(ENCH_SOUL_RIPE)
        && you.see_cell(mons->pos()) && !RESET_KILL(killer)
        // You can't deliberately bind a summon, but this also handles mirror copies
        && !mons->is_summoned())
    {
        record_monster_defeat(mons, killer);
        record_monster_defeat(mons, KILL_BOUND);
        yred_make_bound_soul(mons, player_under_penance());
        return true;
    }

    return false;
}

static bool _vampire_make_thrall(monster* mons)
{
    if (!mons->props.exists(VAMPIRIC_THRALL_KEY) || you.allies_forbidden())
        return false;

    // Check if another thrall is already alive
    for (monster_iterator mi; mi; ++mi)
        if (mi->was_created_by(MON_SUMM_THRALL))
            return false;

    // Okay, let's try to make them for real!
    mprf("%s rises to serve you!", mons->name(DESC_THE).c_str());
    record_monster_defeat(mons, KILL_YOU);

    mons->hit_points = mons->max_hit_points;
    mons->flags |= (MF_NO_REWARD | MF_FAKE_UNDEAD);

    if (mons->is_actual_spellcaster())
    {
        mons->spells.push_back({SPELL_VAMPIRIC_DRAINING, 50, MON_SPELL_WIZARD});
        mons->props[CUSTOM_SPELLS_KEY] = true;
    }

    mons->attitude = ATT_FRIENDLY;
    mons->add_ench(mon_enchant(ENCH_VAMPIRE_THRALL, 0, &you, INFINITE_DURATION));

    const int pow = get_form(transformation::vampire)->get_level(10);
    const int dur = random_range(pow, pow * 2) + 30;

    mons->mark_summoned(MON_SUMM_THRALL, 0, false);
    mons->add_ench(mon_enchant(ENCH_SUMMON_TIMER, 0, &you, dur));
    mons_att_changed(mons);
    gain_exp(exper_value(*mons));

    // End constriction.
    mons->stop_constricting_all();
    mons->stop_being_constricted();

    // Cancel fleeing and such.
    mons->behaviour = BEH_SEEK;

    // Remove level annotation.
    mons->props[NO_ANNOTATE_KEY] = true;
    remove_unique_annotation(mons);

    behaviour_event(mons, ME_EVAL);

    return true;
}


/**
 * Attempt to get a deathbed conversion for the given orc.
 *
 * @param mons          A dying orc.
 * @param killer        The way in which the monster was killed (or 'killed').
 * @return              Whether the monster's life was saved (praise Beogh)
 */
static bool _beogh_forcibly_convert_orc(monster &mons, killer_type killer)
{
    // Orcs may convert to Beogh under threat of death, either from
    // you or, less often, your followers. In both cases, the
    // checks are made against your stats. You're the potential
    // messiah, after all.
#ifdef DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS, "Death convert attempt on %s, HD: %d, "
         "your xl: %d",
         mons.name(DESC_PLAIN).c_str(),
         mons.get_hit_dice(),
         you.experience_level);
#endif
    if (random2(you.piety) >= piety_breakpoint(0)
        && random2(you.experience_level) >= random2(mons.get_hit_dice())
        // Bias beaten-up-conversion towards the stronger orcs.
        && random2(mons.get_experience_level()) > 2)
    {
        const bool follower = MON_KILL(killer);
        conv_t ctype = follower ? conv_t::deathbed_follower
                                : conv_t::deathbed;
        if (mons.has_ench(ENCH_VENGEANCE_TARGET))
        {
            ctype = follower ? conv_t::vengeance_follower
                             : conv_t::vengeance;
        }

        beogh_convert_orc(&mons, ctype);
        return true;
    }

    return false;
}

/**
 * Attempt to get a deathbed conversion for the given monster.
 *
 * @param mons          A dying monster (not necessarily an orc)
 * @param killer        The way in which the monster was killed (or 'killed').
 * @param killer_index  The mindex of the killer, if known.
 * @return              Whether the monster's life was saved (praise Beogh)
 */
static bool _beogh_maybe_convert_orc(monster &mons, killer_type killer,
                                    int killer_index)
{
    if (!have_passive(passive_t::convert_orcs)
        || mons_genus(mons.type) != MONS_ORC
        || mons.is_summoned() || mons.is_shapeshifter()
        || !you.see_cell(mons.pos()) || mons_is_god_gift(mons)
        // Don't attempt a deathbed conversion of an already converted orc
        || mons.flags & MF_PACIFIED
        || mons.flags & MF_APOSTLE_BAND
        || mons.type == MONS_ORC_APOSTLE)
    {
        return false;
    }

    if (YOU_KILL(killer))
        return _beogh_forcibly_convert_orc(mons, killer);

    if (MON_KILL(killer) && !invalid_monster_index(killer_index))
    {
        const monster* responsible_monster = &env.mons[killer_index];
        if (is_follower(*responsible_monster) && !one_chance_in(3))
            return _beogh_forcibly_convert_orc(mons, killer);
    }

    return false;
}

static bool _blorkula_bat_split(monster& blorkula, killer_type ktype)
{
    // Can't recover from these
    if (RESET_KILL(ktype))
        return false;

    // XXX: Summoned Blorkulas (ie: from phantom mirror) will cease to be when
    // timeout_enchantments() below is called, leading to a crash. If some
    // refactoring is done which allows this to not be the case, feel free to
    // remove this line.
    if (blorkula.is_summoned())
        return false;

    if (blorkula.props.exists(BLORKULA_DIE_FOR_REAL_KEY))
        return false;

    // If our death escape is still on cooldown, just let him die.
    if (blorkula.has_ench(ENCH_BREATH_WEAPON))
    {
        if (you.can_see(blorkula))
        {
            mprf("%s attempts to avoid another deathblow, but is too exhausted to transform.",
                blorkula.name(DESC_THE).c_str());
        }
        return false;
    }

    const int num_bats = random_range(4, 5);
    const int revive_timer = you.elapsed_time + random_range(150, 190);
    {
        // Suppress messages about status effects wearing off
        msg::suppress msg;
        blorkula.heal(blorkula.max_hit_points);
        blorkula.del_ench(ENCH_CONFUSION, true);    // Don't blink at random
        blorkula.stop_being_constricted(true);
        blorkula.timeout_enchantments(1000);
    }
    blorkula.add_ench(mon_enchant(ENCH_BREATH_WEAPON, 1, &blorkula,
                                  random_range(450, 900) * BASELINE_DELAY));

#ifdef USE_TILE
    static vector<int> bat_colours =
    {
        TILEP_MONS_VAMPIRE_BAT_GREEN,
        TILEP_MONS_VAMPIRE_BAT_ORANGE,
        TILEP_MONS_VAMPIRE_BAT_RED,
        TILEP_MONS_VAMPIRE_BAT_PURPLE,
        TILEP_MONS_VAMPIRE_BAT_BLUE,
    };
    shuffle_array(bat_colours);
#endif

    follower saved_blork = follower(blorkula);
    bool placed_bat = false;
    for (int i = 0; i < num_bats; ++i)
    {
        monster *bat =
        create_monster(
            mgen_data(MONS_VAMPIRE_BAT, SAME_ATTITUDE(&blorkula),
                      blorkula.pos(), blorkula.foe));

        if (bat)
        {
            bat->props[BLORKULA_REVIVAL_TIMER_KEY] = revive_timer;
#ifdef USE_TILE
            bat->props[MONSTER_TILE_KEY] = bat_colours[i];
#endif
            saved_blork.write_to_prop(bat->props[SAVED_BLORKULA_KEY].get_vector());
            mons_add_blame(bat, "manifested out of " + blorkula.name(DESC_A, true));
            bat->flags |= (MF_NO_REWARD | MF_WAS_IN_VIEW);
            placed_bat = true;
        }
    }

    if (!placed_bat)
    {
        simple_monster_message(blorkula, " attempts to avoid the deathblow, but fails!");
        return false;
    }

    if (you.can_see(blorkula))
    {
        mprf(MSGCH_MONSTER_SPELL,
            "%s avoids the killing blow by scattering into a rainbow of bats!",
            blorkula.name(DESC_THE).c_str());
    }

    monster_die(blorkula, KILL_RESET, NON_MONSTER, true);

    return true;
}

static monster* _retrieve_saved_blorkula(monster& bat)
{
    follower saved_blork;
    saved_blork.read_from_prop(bat.props[SAVED_BLORKULA_KEY].get_vector());
    monster* blork = saved_blork.place();
    return blork;
}

static void _blorkula_bat_death(monster& bat, killer_type killer, int killer_index)
{
    // Check if any other bats are still alive. If they are not, formally kill Blorkula.
    for (monster_iterator mi; mi; ++mi)
    {
        if (mi->type == MONS_VAMPIRE_BAT && *mi != &bat && mi->props.exists(BLORKULA_REVIVAL_TIMER_KEY))
            return;
    }

    // No other bats left, so pass this death onto Blorkula as 'real'
    monster* blork = _retrieve_saved_blorkula(bat);
    blork->swap_with(&bat);
    blork->props[BLORKULA_DIE_FOR_REAL_KEY] = true;

    // Otherwise we won't get proper XP or piety for banishing Blork via banishing
    // the final bat, if worshipping Lugonu. There is a mild possibility that in at
    // least a decade of people playing this game, this may occur once.
    if (killer == KILL_BANISHED && killer_index == MHITYOU)
    {
            did_god_conduct(DID_BANISH, blork->get_experience_level(), true, blork);
            blork->damage_friendly = blork->hit_points;
    }

    monster_die(*blork, killer, killer_index);
}

void blorkula_bat_merge(monster& bat)
{
    if (!bat.props.exists(BLORKULA_REVIVAL_TIMER_KEY)
        || bat.props[BLORKULA_REVIVAL_TIMER_KEY].get_int() > you.elapsed_time)
    {
        return;
    }

    monster* blork = _retrieve_saved_blorkula(bat);
    const coord_def pos = bat.pos();

    int bat_count = 0;
    for (monster_iterator mi; mi; ++mi)
    {
        if (mi->type == MONS_VAMPIRE_BAT && mi->props.exists(BLORKULA_REVIVAL_TIMER_KEY))
        {
            monster_die(**mi, KILL_RESET, NON_MONSTER, true);
            ++bat_count;
        }
    }

    // If not all the bats are still alive, injure Blork proportionally
    if (bat_count < 4)
    {
        blork->hit_points = min(blork->max_hit_points,
                                (blork->max_hit_points * 2 / 7) * bat_count);
    }
    blork->move_to_pos(pos);

    if (you.can_see(*blork))
    {
        mprf(MSGCH_MONSTER_SPELL,
             "The bats swarm back together and %s reappears in a puff of iridescent mist.",
             blork->name(DESC_THE).c_str());
    }
}

/**
 * Attempt to save the given monster's life at the last moment.
 *
 * Checks lost souls & various divine effects (Yred, Beogh, Ely).
 *
 * @param mons          A dying monster.
 * @param killer        The way in which the monster was killed (or 'killed').
 * @param killer_index  The mindex of the killer, if known.
 */
static bool _monster_avoided_death(monster* mons, killer_type killer,
                                   int killer_index)
{
    if (mons->max_hit_points <= 0 || mons->get_hit_dice() < 1)
        return false;

    // Before the hp check since this should not care about the power of the
    // finishing blow
    if (lost_soul_revive(*mons, killer))
        return true;

    // Yredelemnul special.
    if (_yred_bind_soul(mons, killer))
        return true;

    // Beogh special.
    if (mons->type == MONS_ORC_APOSTLE && you_worship(GOD_BEOGH))
    {
        if (mons->has_ench(ENCH_TOUCH_OF_BEOGH))
        {
            if (killer == KILL_BANISHED)
            {
                simple_god_message(" pulls their child back from the Abyss.",
                                   false, GOD_BEOGH);
            }

            win_apostle_challenge(*mons);
            mons->heal(mons->max_hit_points);
            // monster::banish sets damage_friendly and not resetting that could
            // crash. monster::heal resets it but not if it was at full health.
            mons->damage_total = mons->damage_friendly = 0;
            avoided_death_fineff::schedule(mons);
            return true;
        }
    }
    else if (_beogh_maybe_convert_orc(*mons, killer, killer_index))
        return true;

    if (mons->type == MONS_BLORKULA_THE_ORCULA)
        return _blorkula_bat_split(*mons, killer);

    if (mons->hit_points < -25 || mons->hit_points < -mons->max_hit_points)
        return false;

    // Elyvilon specials.
    if (_ely_protect_ally(mons, killer))
        return true;
    if (_ely_heal_monster(mons, killer, killer_index))
        return true;

    if (_vampire_make_thrall(mons))
        return true;

    return false;
}

static void _jiyva_died()
{
    if (you_worship(GOD_JIYVA))
        return;

    add_daction(DACT_JIYVA_DEAD);

    if (!player_in_branch(BRANCH_SLIME))
        return;

    if (silenced(you.pos()))
    {
        god_speaks(GOD_JIYVA, "With an infernal shudder, the power ruling "
                   "this place vanishes!");
    }
    else
    {
        god_speaks(GOD_JIYVA, "With an infernal noise, the power ruling this "
                   "place vanishes!");
    }
}

void fire_monster_death_event(monster* mons,
                              killer_type killer,
                              bool polymorph)
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

    bool terrain_changed = false;

    for (map_marker *mark : env.markers.get_all(MAT_TERRAIN_CHANGE))
    {
        map_terrain_change_marker *marker =
                dynamic_cast<map_terrain_change_marker*>(mark);

        if (marker->mon_num != 0 && monster_by_mid(marker->mon_num) == mons)
        {
            terrain_changed = true;
            marker->duration = 0;
        }
    }

    if (terrain_changed)
        timeout_terrain_changes(0, true);

    if (killer == KILL_BANISHED)
        return;

    los_monster_died(mons);

    if (type == MONS_ROYAL_JELLY && !mons->is_summoned() && !polymorph)
    {
        you.royal_jelly_dead = true;

        if (jiyva_is_dead())
            _jiyva_died();
    }
}

int mummy_curse_power(monster_type type)
{
    // Plain mummies (and Menkaure) are too weak to curse you!
    switch (type)
    {
        case MONS_GUARDIAN_MUMMY:
        case MONS_MUMMY_PRIEST:
        case MONS_ROYAL_MUMMY:
        case MONS_KHUFU:
            return mons_class_hit_dice(type);
        default:
            return 0;
    }
}

template<typename valid_T, typename connect_T>
static void _search_dungeon(const coord_def & start,
                    valid_T & valid_target,
                    connect_T & connecting_square,
                    set<position_node> & visited,
                    vector<set<position_node>::iterator> & candidates,
                    bool exhaustive = true,
                    int connect_mode = 8)
{
    if (connect_mode < 1 || connect_mode > 8)
        connect_mode = 8;

    // Ordering the default compass index this way gives us the non
    // diagonal directions as the first four elements - so by just
    // using the first 4 elements instead of the whole array we
    // can have 4-connectivity.
    int compass_idx[] = {0, 2, 4, 6, 1, 3, 5, 7};

    position_node temp_node;
    temp_node.pos = start;
    temp_node.last = nullptr;

    queue<set<position_node>::iterator > fringe;

    auto current = visited.insert(temp_node).first;
    fringe.push(current);

    while (!fringe.empty())
    {
        current = fringe.front();
        fringe.pop();

        shuffle_array(compass_idx, connect_mode);

        for (int i=0; i < connect_mode; ++i)
        {
            coord_def adjacent = current->pos + Compass[compass_idx[i]];
            if (in_bounds(adjacent))
            {
                temp_node.pos = adjacent;
                temp_node.last = &(*current);
                auto res = visited.insert(temp_node);

                if (!res.second)
                    continue;

                if (valid_target(adjacent))
                {
                    candidates.push_back(res.first);
                    if (!exhaustive)
                        return;
                }

                if (connecting_square(adjacent))
                    fringe.push(res.first);
            }
        }
    }
}

static void _infestation_create_scarab(monster* mons)
{
    mons->flags |= MF_EXPLODE_KILL;
    infestation_death_fineff::schedule(mons->pos(), mons->name(DESC_THE));
}

static void _pharaoh_ant_bind_souls(monster *mons)
{
    bool bound = false;
    for (monster_near_iterator mi(mons, LOS_NO_TRANS); mi; ++mi)
    {
        if (!mons_can_bind_soul(mons, *mi))
            continue;
        if (!bound)
            simple_monster_message(*mons, " binds the souls of nearby monsters.");
        bound = true;
        mi->add_ench(
            mon_enchant(ENCH_BOUND_SOUL, 0, mons,
                        random_range(10, 30) * BASELINE_DELAY));
    }
}

static void _print_summon_poof_message(const monster& mons, bool corpse = false)
{
    // XXX: Chaos spawn leave a chaos cloud in this manner, whether they're
    //      summoned or not.
    if (!mons.is_abjurable() && mons.type != MONS_CHAOS_SPAWN)
        return;

    string prefix = " ";
    bool need_possessive = false;

    if (corpse && mons_class_can_leave_corpse(mons_species(mons.type)))
    {
        prefix = " corpse ";
        need_possessive = true;
    }

    string msg = summoned_poof_msg(mons) + "!";

    simple_monster_message(mons, (prefix + msg).c_str(), need_possessive, MSGCH_MONSTER_TIMEOUT);
}

static void _monster_die_cloud(const monster& mons, bool real_death)
{
    if (cell_is_solid(mons.pos()))
        return;

    if (real_death)
    {
        switch (mons.type)
        {
            case MONS_SIMULACRUM:
                place_cloud(CLOUD_COLD, mons.pos(), 2 + random2(4), &mons);
                return;

            case MONS_PILE_OF_DEBRIS:
                place_cloud(CLOUD_DUST, mons.pos(), 2 + random2(4), &mons);
                return;

            case MONS_FIRE_VORTEX:
                place_cloud(CLOUD_FIRE, mons.pos(), 2 + random2(4), &mons);
                return;

            case MONS_BATTLESPHERE:
                place_cloud(CLOUD_MAGIC_TRAIL, mons.pos(), 3 + random2(3), &mons);
                return;

            default:
                break;
        }
    }

    if (!real_death && !mons.is_abjurable())
        return;

    // After this point, we're placing clouds for vanishing summons

    // Don't bother placing a cloud for living spells.
    if (mons.type == MONS_LIVING_SPELL)
        return;

    cloud_type cloud = CLOUD_NONE;
    string msg = summoned_poof_msg(mons);
    if (msg.find("smoke") != string::npos)
        cloud = random_smoke_type();
    else if (msg.find("chaos") != string::npos)
        cloud = CLOUD_CHAOS;

    if (cloud != CLOUD_NONE)
        place_cloud(cloud, mons.pos(), 1 + random2(3), &mons);
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
#if TAG_MAJOR_VERSION == 34
    case KILL_MISCAST:
        return "miscast";
#endif
    case KILL_NON_ACTOR:
        return "non_actor";
    case KILL_RESET:
        return "reset";
    case KILL_RESET_KEEP_ITEMS:
        return "reset_keep_items";
    case KILL_BANISHED:
        return "banished";
    case KILL_TIMEOUT:
        return "timeout";
#if TAG_MAJOR_VERSION == 34
    case KILL_UNSUMMONED:
        return "unsummoned";
#endif
    case KILL_PACIFIED:
        return "pacified";
    case KILL_BOUND:
        return "bound";
    case KILL_SLIMIFIED:
        return "slimified";
    case KILL_TENTACLE_CLEANUP:
        return "tentacle cleanup";
    }
    die("invalid killer type");
}

static string _derived_undead_message(const monster &mons, monster_type which_z,
                                      string msg)
{
    switch (which_z)
    {
    case MONS_SPECTRAL_THING:
    case MONS_SIMULACRUM:
        // XXX: print immediately instead?
        return msg;
    case MONS_SKELETON:
    case MONS_ZOMBIE:
        break;
    default:
        return "A buggy dead thing appears!";
    }

    const auto habitat = mons_class_primary_habitat(mons.type);
    if (habitat == HT_WATER || habitat == HT_LAVA)
        return "The dead are swimming!";

    if (mons_class_flag(mons.type, M_FLIES))
        return "The dead are flying!";

    const auto shape = get_mon_shape(mons);
    if (shape == MON_SHAPE_SNAKE || shape == MON_SHAPE_SNAIL)
        return "The dead are slithering!";
    if (shape == MON_SHAPE_ARACHNID || shape == MON_SHAPE_CENTIPEDE)
        return "The dead are crawling!"; // to say nothing of creeping

    const monster_type genus = mons_genus(mons.type);
    if (genus == MONS_FROG || genus == MONS_QUOKKA)
        return "The dead are hopping!";

    return "The dead are walking!"; // a classic for sure
}

/**
 * Make derived undead out of a dying/dead monster.
 *
 * @param mons       the monster that died
 * @param quiet      whether to print flavour messages
 * @param which_z    the kind of zombie
 * @param beh        the zombie's behavior
 * @param spell      the spell or summon type used (if any)
 * @param god        the god involved (if any)
 */
static void _make_derived_undead(monster* mons, bool quiet,
                                 monster_type which_z, beh_type beh,
                                 int spell, god_type god,
                                 string msg = "", string fail_msg = "")
{
    bool requires_corpse = which_z == MONS_ZOMBIE || which_z == MONS_SKELETON;
    // This function is used by several different sorts of things, each with
    // their own validity conditions that are enforced here
    // - Bind Souls, Death Channel, Yred reaping of unzombifiable things, and
    //   kills with reaping-branded items:
    if (!requires_corpse
        && !mons_can_be_spectralised(*mons, god == GOD_YREDELEMNUL))
    {
        return;
    }
    // - Monster Bind Souls
    if (spell == SPELL_BIND_SOULS && !(mons->holiness() & MH_NATURAL
                                       && mons_can_be_zombified(*mons)))
    {
        return;
    }
    // - Yred reaping of living monsters
    if (requires_corpse && !mons_can_be_zombified(*mons))
        return;

    // Use the original monster type as the zombified type here, to
    // get the proper stats from it.
    mgen_data mg(which_z,
                 beh,
                 mons->pos(),
                 // XXX: is MHITYOU really correct here?
                 crawl_state.game_is_arena() ? MHITNOT : MHITYOU,
                 MG_NONE,
                 god);
    // Don't link monster-created derived undead to the summoner, they
    // shouldn't poof
    mg.set_summoned(beh == BEH_FRIENDLY ? &you : nullptr, spell, 0, false);
    mg.set_base(mons->type);
    // Prefer to be created wherever the dead monster was, but allow placing up
    // to 2 spaces away, if needbe.
    mg.set_range(0, 2);

    if (spell == MON_SUMM_WPN_REAP)
        mg.summon_duration = random_range(200, 400);

    if (!mons->mname.empty() && !(mons->flags & MF_NAME_NOCORPSE))
        mg.mname = mons->mname;
    else if (mons_is_unique(mons->type))
        mg.mname = mons_type_name(mons->type, DESC_PLAIN);
    mg.extra_flags = mons->flags & (MF_NAME_SUFFIX
                                      | MF_NAME_ADJECTIVE
                                      | MF_NAME_DESCRIPTOR);

    // Kiku wrath and Bind Soul simulacrum are permanent and shouldn't give rewards
    if (god == GOD_KIKUBAAQUDGHA || spell == SPELL_BIND_SOULS)
        mg.extra_flags |= (MF_NO_REWARD | MF_HARD_RESET);

    const string mist = which_z == MONS_SIMULACRUM ? "freezing" :
                            god == GOD_YREDELEMNUL ? "black"
                                                   : "glowing";

    if (msg.empty())
        msg = "A " + mist + " mist starts to gather...";

    if (fail_msg.empty())
        fail_msg = "A " + mist + " mist gathers momentarily, then fades.";

    if (mons->mons_species() == MONS_HYDRA)
    {
        // No undead 0-headed hydras, sorry.
        if (mons->heads() == 0)
        {
            if (!quiet && which_z != MONS_SKELETON)
                mpr(fail_msg);
            return;
        }
        else
            mg.props[MGEN_NUM_HEADS] = mons->heads();
    }

    string agent_name = "";
    if (mons->has_ench(ENCH_BOUND_SOUL))
    {
        const auto agent = mons->get_ench(ENCH_BOUND_SOUL).agent();
        if (agent)
            agent_name = agent->as_monster()->full_name(DESC_A);
    }

    const string message = quiet ? "" :
                           god == GOD_KIKUBAAQUDGHA ? "Kikubaaqudgha cackles." :
                           _derived_undead_message(*mons, which_z, msg);
    make_derived_undead_fineff::schedule(mons->pos(), mg,
            mons->get_experience_level(), agent_name, message);
}

static void _druid_final_boon(const monster* mons)
{
    vector<monster*> beasts;
    for (monster_near_iterator mi(mons); mi; ++mi)
    {
        if (mons_is_beast(mons_base_type(**mi)) && mons_aligned(mons, *mi))
            beasts.push_back(*mi);
    }

    if (!beasts.size())
        return;

    if (you.can_see(*mons))
    {
        mprf(MSGCH_MONSTER_SPELL, "With its final breath, %s offers up its power "
                                  "to the beasts of the wild!",
                                  mons->name(DESC_THE).c_str());
    }

    shuffle_array(beasts);
    int num = min((int)beasts.size(),
                  random_range(mons->get_hit_dice() / 3,
                               mons->get_hit_dice() / 2 + 1));

    // Healing and empowering done in two separate loops for tidier messages
    for (int i = 0; i < num; ++i)
    {
        if (beasts[i]->heal(roll_dice(3, mons->get_hit_dice()))
            && you.can_see(*beasts[i]))
        {
            mprf("%s %s healed.", beasts[i]->name(DESC_THE).c_str(),
                                  beasts[i]->conj_verb("are").c_str());
        }
    }

    for (int i = 0; i < num; ++i)
    {
        simple_monster_message(*beasts[i], " seems to grow more fierce.");
        beasts[i]->add_ench(mon_enchant(ENCH_MIGHT, 1, mons,
                                        random_range(100, 160)));
    }
}

static void _orb_of_mayhem(actor& maniac, const monster& victim)
{
    vector<monster *> witnesses;
    for (monster_near_iterator mi(&victim, LOS_NO_TRANS); mi; ++mi)
        if (*mi != &victim && mi->can_see(maniac) && mi->can_go_frenzy())
            witnesses.push_back(*mi);

    if (coinflip() && !witnesses.empty())
    {
        (*random_iterator(witnesses))->go_frenzy(&maniac);
        did_god_conduct(DID_HASTY, 8, true);
    }
}

static void _protean_explosion(monster* mons)
{
    // This is slightly hacky, but we determine which thing to turn into by
    // creating a dummy monster of the right hd, then making a poly set for it
    // and picking the first one.
    monster dummy;
    dummy.type = MONS_SHAPESHIFTER;
    define_monster(dummy);
    dummy.set_hit_dice(12);
    init_poly_set(&dummy);
    const CrawlVector &set = dummy.props[POLY_SET_KEY];
    monster_type target = (monster_type)set[0].get_int();

    // It may be thematic to become a shapeshifter, but this also tosses off the
    // might and haste immediately. (We can just pick the second polyset target
    // since they should be guaranteed to be different.)
    if (target == MONS_GLOWING_SHAPESHIFTER)
        target = (monster_type)set[1].get_int();

    if (you.can_see(*mons))
    {
        mprf(MSGCH_MONSTER_WARNING, "For just a moment, %s begins to "
                                    "look like %s, then it explodes!",
                                    mons->name(DESC_THE).c_str(),
                                    mons_type_name(target, DESC_A).c_str());
    }

    // Determine number of children based on the HD of what we roll.
    // HD >= 12 generates 2, HD 11 generates 2-3,
    // HD 10-9 generates 3, HD < 9 generates 4.
    // (Going down that far should be extremely rare, but
    //  polymorph code is weird.)
    int num_children = 2;
    if (mons_class_hit_dice(target) < 9)
        num_children += 2;
    else if (mons_class_hit_dice(target) < 11)
        ++num_children;
    else if (mons_class_hit_dice(target) < 12 && coinflip())
        ++num_children;

    int summoned_duration = 0;
    int summon_type = 0;
    bool is_summoned = mons->is_summoned();
    if (is_summoned)
    {
        mon_enchant summ = mons->get_ench(ENCH_SUMMON);
        summoned_duration = summ.duration;
        summon_type = summ.degree;
    }

    // Then create and scatter the piles around
    int delay = random_range(2, 4) * BASELINE_DELAY;
    for (int i = 0; i < num_children; ++i)
    {
        // Try to find a spot within 3 tiles. If that fails, expand up to 6 tiles.
        // If that also fails, stop trying to place children; the player got off
        // easy this time.
        //
        // XXX: This is somewhat imperfect, since it checks for the habitat of
        //      what the flesh will *become*, which has a very slim chance of
        //      being somewhere the flesh itself cannot survive if - say - we
        //      roll turning into a salamander and there's lava nearby. So it
        //      may think we have a valid tile when we don't. It's very awkward
        //      to prevent that without also limiting the possible spawn pool to
        //      ONLY monsters that can survive in deep water, though.
        const coord_def spot = find_newmons_square(target, mons->pos(), 3, 6);
        if (spot.origin())
            return;

        mgen_data mg = mgen_data(MONS_ASPIRING_FLESH, SAME_ATTITUDE(mons),
                                 spot, MHITNOT, MG_FORCE_PLACE | MG_FORCE_BEH,
                                 mons->god);
        if (is_summoned)
        {
            const actor* const summoner = actor_by_mid(mons->summoner);
            mg.set_summoned(summoner, summon_type, 1 /* dummy value*/);
        }
        monster *child = create_monster(std::move(mg));

        if (child)
        {
            child->props[PROTEAN_TARGET_KEY] = target;
            child->add_ench(mon_enchant(ENCH_PROTEAN_SHAPESHIFTING, 0, 0, delay));
            child->flags |= MF_WAS_IN_VIEW;

            // Prevent them from being trivially unaware of the player
            child->foe = mons->foe;
            child->behaviour = BEH_SEEK;

            mons_add_blame(child, "spawned from " + mons->name(DESC_A, true), true);

            if (is_summoned)
            {
                // Match the original summoned progenitor's duration.
                mon_enchant summon_duration_ench(ENCH_SUMMON_TIMER, 0, nullptr, summoned_duration);
                child->update_ench(summon_duration_ench);
            }

            // Make each one shift a little later than the last
            delay += random_range(1, 2) * BASELINE_DELAY;
        }
    }
}

static void _martyr_death_wail(monster &mons)
{
    if (you.can_see(mons))
    {
        if (mons.friendly())
        {
            mprf(MSGCH_FRIEND_SPELL,
                 "%s wails in agony as it relives its own death.",
                 mons.name(DESC_YOUR).c_str());
        }
        else
        {
            mprf(MSGCH_MONSTER_SPELL,
                 "%s wails in agony as it relives its own death.",
                 mons.name(DESC_THE).c_str());
        }
    }

    // Save the HD of our shade, because it will otherwise be reset on changing
    int old_hd = mons.get_hit_dice();
    change_monster_type(&mons, MONS_FLAYED_GHOST);
    mons.max_hit_points = mons.max_hit_points * old_hd / mons.get_hit_dice();
    mons.set_hit_dice(old_hd);

    // Reset duration on its summoning, but move it out of martyr's summon cap
    mons.del_ench(ENCH_SUMMON_TIMER, true, false);
    mons.del_ench(ENCH_SUMMON, true, false);
    mons.mark_summoned(SPELL_NO_SPELL, summ_dur(2));
    mons.heal(50000);

    // Show brief animation
    bolt visual;
    visual.target = mons.pos();
    visual.source = mons.pos();
    visual.aimed_at_spot = true;
    visual.colour = ETC_DARK;
    visual.glyph      = '*';
    visual.draw_delay = 100;
    visual.flavour = BEAM_VISUAL;
    visual.fire();

    // Have it instantly flay a few nearby things
    vector <actor*> targets;
    for (actor_near_iterator ai(mons.pos(), LOS_NO_TRANS); ai; ++ai)
    {
        if (!mons_aligned(&mons, *ai) && !!(ai->holiness() & MH_NATURAL))
            targets.push_back(*ai);
    }
    shuffle_array(targets);

    int num_victims = min((int)targets.size(), random_range(2, 3));

    // Dummy arguments
    mon_spell_slot slot = { SPELL_FLAY, 0, MON_SPELL_MAGICAL };
    bolt _bolt;

    for (int i = 0; i < num_victims; ++i)
    {
        mons.foe = targets[i]->mindex();
        mons_cast_flay(mons, slot ,_bolt);
    }

    // Remove injury bond from all monsters that were guarded by this
    for (monster_iterator mi; mi; ++mi)
    {
        if (mi->has_ench(ENCH_INJURY_BOND)
            && mi->get_ench(ENCH_INJURY_BOND).agent() == &mons)
        {
            mi->del_ench(ENCH_INJURY_BOND);
        }
    }

    return;
}

static bool _mons_reaped(actor &killer, monster& victim)
{
    beh_type beh;

    if (killer.is_player())
        beh     = BEH_FRIENDLY;
    else
    {
        monster* mon = killer.as_monster();
        beh = SAME_ATTITUDE(mon);
    }

    string msg = victim.name(DESC_ITS) + " spirit is torn from " +
                     victim.pronoun(PRONOUN_POSSESSIVE) + " body!";
    string fail_msg = victim.name(DESC_ITS) + " spirit is momentarily torn from " +
                          victim.pronoun(PRONOUN_POSSESSIVE) + " body, then fades!";
    _make_derived_undead(&victim, !you.can_see(victim), MONS_SPECTRAL_THING, beh,
                         MON_SUMM_WPN_REAP, GOD_NO_GOD, msg, fail_msg);

    return true;
}

static void _yred_reap(monster &mons, bool uncorpsed)
{
    monster_type which_z = !uncorpsed && mons_can_be_zombified(mons) ? MONS_ZOMBIE :
                           MONS_SPECTRAL_THING;

    _make_derived_undead(&mons, false, which_z, BEH_FRIENDLY,
                         MON_SUMM_YRED_REAP, you.religion);
}

static bool _animate_dead_reap(monster &mons)
{
    if (!you.duration[DUR_ANIMATE_DEAD])
        return false;
    const int pow = you.props[ANIMATE_DEAD_POWER_KEY].get_int();
    if (!x_chance_in_y(150 + div_rand_round(pow, 2), 200))
        return false;

    _make_derived_undead(&mons, false, MONS_ZOMBIE, BEH_FRIENDLY,
                         SPELL_ANIMATE_DEAD, GOD_NO_GOD);
    return true;
}

static bool _reaping(monster &mons)
{
    if (!mons.props.exists(REAPING_DAMAGE_KEY))
        return false;

    int rd = mons.props[REAPING_DAMAGE_KEY].get_int();
    const int denom = mons.damage_total * 3 / 2;
    dprf("Reaping chance: %d/%d", rd, denom);
    if (!x_chance_in_y(rd, denom))
        return false;

    actor *killer = actor_by_mid(mons.props[REAPER_KEY].get_int());
    if (!killer)
        return false;
    if (killer->is_player() && you.allies_forbidden())
        return false;
    return _mons_reaped(*killer, mons);
}

static void _kiku_wrath_raise(monster &mons, bool quiet, bool corpse_gone)
{
    monster_type which_z = MONS_SPECTRAL_THING;
    if (!corpse_gone && mons_can_be_zombified(mons) && !one_chance_in(3))
        which_z = coinflip() ? MONS_ZOMBIE : MONS_SIMULACRUM;
    _make_derived_undead(&mons, quiet, which_z, BEH_HOSTILE,
                         SPELL_NO_SPELL, GOD_KIKUBAAQUDGHA);
}

static bool _apply_necromancy(monster &mons, bool quiet, bool corpse_gone,
                              bool in_los, bool corpseworthy)
{
    // This is a hostile effect, and monsters are dirty cheaters. Sorry!
    if (mons.has_ench(ENCH_BOUND_SOUL)
        && !have_passive(passive_t::goldify_corpses))
    {
        _make_derived_undead(&mons, quiet, MONS_SIMULACRUM,
                             SAME_ATTITUDE(&mons),
                             SPELL_BIND_SOULS, GOD_NO_GOD);
        return true;
    }

    if (corpseworthy && you.penance[GOD_KIKUBAAQUDGHA] && one_chance_in(3))
    {
        _kiku_wrath_raise(mons, quiet, corpse_gone);
        return true;
    }

    // Players' corpse-consuming effects. Go from best to worst.
    if (mons.has_ench(ENCH_INFESTATION))
    {
        _infestation_create_scarab(&mons);
        return true;
    }

    if (!corpseworthy)
        return false;

    // Yred takes priority over everything but Infestation.
    if (in_los && have_passive(passive_t::reaping)
        && mons.umbraed())
    {
        if (yred_reap_chance())
            _yred_reap(mons, corpse_gone);
        return true;
    }

    if (corpse_gone || have_passive(passive_t::goldify_corpses))
        return false;

    if (in_los && (_animate_dead_reap(mons) || _reaping(mons)))
        return true;

    return false;
}

static bool _god_will_bless_follower(monster* victim)
{
    return have_passive(passive_t::bless_followers_vs_evil)
           && victim->evil()
           && random2(you.piety) >= piety_breakpoint(0);
}

static bool should_blame_you_for_kill(int killer_index, bool pet_kill) noexcept
{
    if (killer_index == YOU_FAULTLESS)
        return false;

    if (pet_kill && !invalid_monster_index(killer_index))
    {
        const monster& m = env.mons[killer_index];

        // always blame the player for marionette kills
        if (m.attitude == ATT_MARIONETTE)
            return true;

        const mon_enchant ench = m.get_ench(ENCH_CONFUSION);
        bool confused_by_non_ally = ench.ench == ENCH_CONFUSION
            && (ench.who != KC_YOU && ench.who != KC_FRIENDLY);
        if (confused_by_non_ally)
            return false;
    }

    return true;
}

/**
 * Trigger the appropriate god conducts for a monster's death.
 *
 * @param mons              The dying monster.
 * @param killer            The responsibility for the death.
 *                          (KILL_YOU, KILL_MON...)
 * @param killer_index      The mindex of the killer, if known.
 * @param maybe_good_kill   Whether the kill can be rewarding in piety.
 *                          (Not summoned, etc)
 */
static void _fire_kill_conducts(const monster &mons, killer_type killer,
                                int killer_index, bool maybe_good_kill)
{
    const bool your_kill = killer == KILL_YOU ||
                           killer == KILL_YOU_CONF ||
                           killer == KILL_YOU_MISSILE ||
                           killer_index == YOU_FAULTLESS;
    const bool pet_kill = _is_pet_kill(killer, killer_index);

    // if you or your pets didn't do it, no one cares
    if (!your_kill && !pet_kill)
        return;

    // player gets credit for reflection and confused ally kills, but not blame
    const bool blameworthy = god_hates_killing(you.religion, mons)
                             && should_blame_you_for_kill(killer_index,
                                 pet_kill);

    // if you can't get piety for it & your god won't give penance/-piety for
    // it, no one cares
    // XXX: this will break holy death curses if they're added back...
    // but tbh that shouldn't really be in conducts anyway
    if (!maybe_good_kill && !blameworthy)
        return;

    mon_holy_type holiness = mons.holiness();

    if (holiness & MH_DEMONIC)
        did_kill_conduct(DID_KILL_DEMON, mons);
    else if (holiness & (MH_NATURAL | MH_PLANT))
    {
        did_kill_conduct(DID_KILL_LIVING, mons);

        // TSO hates natural evil and unholy beings.
        if (mons.evil())
            did_kill_conduct(DID_KILL_NATURAL_EVIL, mons);
    }
    else if (holiness & MH_UNDEAD)
        did_kill_conduct(DID_KILL_UNDEAD, mons);
    else if (holiness & MH_NONLIVING)
        did_kill_conduct(DID_KILL_NONLIVING, mons);

    // Zin hates unclean and chaotic beings.
    if (mons.how_unclean())
        did_kill_conduct(DID_KILL_UNCLEAN, mons);
    else if (mons.how_chaotic())
        did_kill_conduct(DID_KILL_CHAOTIC, mons);

    // jmf: Trog hates wizards.
    if (mons.is_actual_spellcaster())
        did_kill_conduct(DID_KILL_WIZARD, mons);

    // Beogh hates priests of other gods.
    if (mons.is_priest())
        did_kill_conduct(DID_KILL_PRIEST, mons);

    if (mons.is_holy())
        did_kill_conduct(DID_KILL_HOLY, mons);

    // Cheibriados hates fast monsters.
    if (cheibriados_thinks_mons_is_fast(mons) && !mons.cannot_act())
        did_kill_conduct(DID_KILL_FAST, mons);
}

item_def* monster_die(monster& mons, const actor *killer, bool silent, bool mount_death)
{
    killer_type ktype = KILL_YOU;
    int kindex = NON_MONSTER;

    if (!killer)
        ktype = KILL_NONE;
    else if (killer->is_monster())
    {
        const monster *kmons = killer->as_monster();
        ktype = kmons->confused_by_you() ? KILL_YOU_CONF : KILL_MON;
        kindex = kmons->mindex();
    }

    return monster_die(mons, ktype, kindex, silent, mount_death);
}

/**
 * Print messages for dead monsters returning to their 'true form' on death.
 *
 * @param mons      The monster currently dying.
 */
static void _special_corpse_messaging(monster &mons)
{
    if (!mons.props.exists(ORIGINAL_TYPE_KEY) && mons.type != MONS_BAI_SUZHEN)
        return;

    const monster_type orig
        = mons.type == MONS_BAI_SUZHEN ? mons.type :
                (monster_type) mons.props[ORIGINAL_TYPE_KEY].get_int();

    if (orig == MONS_SHAPESHIFTER || orig == MONS_GLOWING_SHAPESHIFTER)
    {
        // No message for known shifters, unless they were originally
        // something else.
        if (!(mons.flags & MF_KNOWN_SHIFTER))
        {
            const string message = " shape twists and changes as "
                + mons.pronoun(PRONOUN_SUBJECTIVE) + " "
                + conjugate_verb("die", mons.pronoun_plurality()) + ".";
            simple_monster_message(mons, message.c_str(), true);
        }

        return;
    }

    // Avoid "Sigmund returns to its original shape as it dies.".
    unwind_var<monster_type> mt(mons.type, orig);
    const int num = mons.mons_species() == MONS_HYDRA
                    ? mons.props[OLD_HEADS_KEY].get_int()
                    : mons.number;
    unwind_var<unsigned int> number(mons.number, num);
    const string message = " returns to " +
                            mons.pronoun(PRONOUN_POSSESSIVE) +
                            " original shape as " +
                            mons.pronoun(PRONOUN_SUBJECTIVE) + " " +
                            conjugate_verb("die", mons.pronoun_plurality()) +
                            ".";
    simple_monster_message(mons, message.c_str());
}

bool mons_will_goldify(const monster &mons)
{
    // Under Gozag, monsters turn into gold on death.
    // Temporary Tukima's Dance weapons stay as weapons (no free gold),
    // permanent dancing weapons turn to gold like other monsters.
    return have_passive(passive_t::goldify_corpses) && mons_gives_xp(mons, you);
}

void handle_monster_dies_lua(monster& mons, killer_type killer)
{
    if (mons.props.exists(MONSTER_DIES_LUA_KEY))
    {
        lua_stack_cleaner clean(dlua);

        dlua_chunk &chunk = mons.props[MONSTER_DIES_LUA_KEY];

        if (!chunk.load(dlua))
        {
            push_monster(dlua, &mons);
            clua_pushcxxstring(dlua, _killer_type_name(killer));
            dlua.callfn(nullptr, 2, 0);
        }
        else
        {
            mprf(MSGCH_ERROR,
                 "Lua death function for monster '%s' didn't load: %s",
                 mons.full_name(DESC_PLAIN).c_str(),
                 dlua.error.c_str());
        }
    }
}

static void _maybe_set_monster_foe(monster& mons, int killer_index)
{
    if (mons.foe == MHITNOT)
    {
        if (!mons.wont_attack() && !crawl_state.game_is_arena())
            mons.foe = MHITYOU;
        else if (!invalid_monster_index(killer_index))
            mons.foe = killer_index;
    }
}

/**
 * Handles giving various mutation/god/equipment-based benefits to the player
 * that trigger when they (or sometimes their pets) kill a monster.
 *
 * @param mons             The monster that died.
 * @param killer           The type of death it suffered.
 * @param gives_player_xp  Whether this death will give the player XP.
 * @param pet_kill         Whether this kill was made by a player ally.
 */
static void _player_on_kill_effects(monster& mons, killer_type killer,
                                    bool gives_player_xp, bool pet_kill)
{
    // Various sources of heal-on-kill
    if (YOU_KILL(killer) && gives_player_xp)
    {
        int hp_heal = 0, mp_heal = 0;
        // Chance scales from 30% at 1* to 80% at 6*
        const bool can_divine_heal =
            (gives_player_xp
                || you_worship(GOD_MAKHLEB)
                   && player_in_branch(BRANCH_CRUCIBLE))
            && !player_under_penance()
            && (x_chance_in_y(50 * (min(piety_breakpoint(5), (int)you.piety) - 30)
                                / (piety_breakpoint(5) - piety_breakpoint(0)) + 30, 100)
                || mons.props.exists(MAKHLEB_BLOODRITE_KILL_KEY));

        if (can_divine_heal && have_passive(passive_t::restore_hp))
        {
            hp_heal += (1 + mons.get_experience_level()) / 2
                    + random2(mons.get_experience_level() / 2);

            if (you.form == transformation::slaughter)
                hp_heal *= 2;
        }
        if (can_divine_heal
            && have_passive(passive_t::restore_hp_mp_vs_evil)
            && mons.evil())
        {
            hp_heal += random2(1 + 2 * mons.get_experience_level());
            mp_heal += random2(2 + mons.get_experience_level() / 3);
        }
        if (can_divine_heal && have_passive(passive_t::mp_on_kill))
            mp_heal += 1 + random2(mons.get_experience_level() / 2);

#if TAG_MAJOR_VERSION == 34
        if (you.has_mutation(MUT_DEVOUR_ON_KILL)
            && mons.holiness() & (MH_NATURAL | MH_PLANT)
            && coinflip())
        {
            hp_heal += 1 + random2avg(1 + you.experience_level, 3);
        }
#endif

        if (hp_heal && you.hp < you.hp_max
            && !you.duration[DUR_DEATHS_DOOR])
        {
            canned_msg(MSG_GAIN_HEALTH);
            inc_hp(hp_heal);
        }

        if (mp_heal && you.magic_points < you.max_magic_points)
        {
            canned_msg(MSG_GAIN_MAGIC);
            inc_mp(mp_heal);
        }
    }

    if (YOU_KILL(killer) && gives_player_xp)
    {
        // TSO follower blessing.
        if (_god_will_bless_follower(&mons))
            bless_follower();

        if (you.wearing_ego(OBJ_ARMOUR, SPARM_MAYHEM))
            _orb_of_mayhem(you, mons);
    }

    // Various sources of berserk extension on kills.
    if (killer == KILL_YOU && you.berserk())
    {
        if (have_passive(passive_t::extend_berserk)
            && you.piety > random2(1000))
        {
            const int bonus = (3 + random2avg(10, 2)) / 2;

            you.increase_duration(DUR_BERSERK, bonus);

            mprf(MSGCH_GOD, you.religion,
                 "You feel the power of %s in you as your rage grows.",
                 uppercase_first(god_name(you.religion)).c_str());
        }
        else if (you.unrand_equipped(UNRAND_BLOODLUST) && coinflip())
        {
            const int bonus = (2 + random2(4)) / 2;
            you.increase_duration(DUR_BERSERK, bonus);
            mpr("The necklace of Bloodlust glows a violent red.");
        }
        else if (you.unrand_equipped(UNRAND_TROG) && coinflip())
        {
            const int bonus = (2 + random2(4)) / 2;
            you.increase_duration(DUR_BERSERK, bonus);
            mpr("You feel the ancient rage of your axe.");
        }
    }

    // Adjust fugue of the fallen bonus. This includes both kills by you and
    // also by your allies.
    if (you.duration[DUR_FUGUE]
        && ((gives_player_xp
            && (killer == KILL_YOU || killer == KILL_YOU_MISSILE || pet_kill))
            || mons.props.exists(KIKU_WRETCH_KEY)))
    {
        const int slaying_bonus = you.props[FUGUE_KEY].get_int();
        // cap at +7 slay (at which point you do bonus negative energy damage
        // around targets hit)
        if (slaying_bonus < FUGUE_MAX_STACKS)
        {
            you.props[FUGUE_KEY] = slaying_bonus + 1;

            // Give a message for hitting max stacks
            if (slaying_bonus + 1 == FUGUE_MAX_STACKS)
                mpr("The wailing of the fallen reaches a fever pitch!");
        }
    }

    if (you.has_mutation(MUT_MAKHLEB_MARK_TYRANT)
        && gives_player_xp
        && (killer == KILL_YOU || killer == KILL_YOU_MISSILE)
        && !one_chance_in(3))
    {
        makhleb_tyrant_buff();
    }

    // Apply unrand effects.
    unrand_death_effects(&mons, killer);

    // Player Powered by Death
    if (gives_player_xp && you.get_mutation_level(MUT_POWERED_BY_DEATH)
        && (YOU_KILL(killer) || pet_kill))
    {
        // Enable the status
        reset_powered_by_death_duration();

        // Maybe increase strength. The chance decreases with number
        // of existing stacks.
        const int pbd_level = you.get_mutation_level(MUT_POWERED_BY_DEATH);
        const int pbd_str = you.props[POWERED_BY_DEATH_KEY].get_int();
        if (x_chance_in_y(10 - pbd_str, 10))
        {
            const int pbd_inc = random2(1 + pbd_level);
            you.props[POWERED_BY_DEATH_KEY] = pbd_str + pbd_inc;
            dprf("Powered by Death strength +%d=%d", pbd_inc,
                 pbd_str + pbd_inc);
        }
    }

    // Revenant kill bonus
    if (gives_player_xp && you.has_mutation(MUT_MNEMOPHAGE)
        && you.see_cell_no_trans(mons.pos()))
    {
        maybe_harvest_memory(mons);
    }

    // Various kill progress tracking
    if (player_in_branch(BRANCH_CRUCIBLE) && !mons.is_summoned()
        && (YOU_KILL(killer) || pet_kill))
    {
        makhleb_crucible_kill(mons);
    }
}

/**
 * Kill off a monster.
 *
 * @param mons The monster to be killed
 * @param killer The method in which it was killed (XXX: these aren't properly
 *               documented/coded)
 * @param killer_index The mindex of the killer (TODO: always use an actor*)
 * @param silent whether to print any messages about the death
 * @param mount_death The death of the mount of a mounted monster (spriggan rider).
 * @returns a pointer to the created corpse, possibly null
 */
item_def* monster_die(monster& mons, killer_type killer,
                      int killer_index, bool silent, bool mount_death)
{
    ASSERT(!invalid_monster(&mons));

    // trying to die again after scheduling an avoided_death fineff
    if (testbits(mons.flags, MF_PENDING_REVIVAL))
        return nullptr;

    const bool was_visible = you.can_see(mons);

    // If a monster was banished to the Abyss and then killed there,
    // then its death wasn't a banishment.
    if (player_in_branch(BRANCH_ABYSS))
        mons.flags &= ~MF_BANISHED;

    const bool spectralised = testbits(mons.flags, MF_SPECTRALISED);

    if (!silent && !mount_death
        && _monster_avoided_death(&mons, killer, killer_index))
    {
        mons.flags &= ~MF_EXPLODE_KILL;

        // revived by a lost soul?
        if (!spectralised && testbits(mons.flags, MF_SPECTRALISED))
            return place_monster_corpse(mons);
        return nullptr;
    }

    // If the monster was calling the tide, let go now.
    mons.del_ench(ENCH_TIDE);

    // Same for silencers.
    mons.del_ench(ENCH_SILENCE);

    // ... and liquefiers.
    mons.del_ench(ENCH_LIQUEFYING);

    // ... and wind-stillers.
    mons.del_ench(ENCH_STILL_WINDS, true);

    // and webbed monsters
    monster_web_cleanup(mons, true);

    // Lose our bullseye target
    mons.del_ench(ENCH_BULLSEYE_TARGET, true);

    // Restore old items, if appropriate
    mons.del_ench(ENCH_ARMED, true);

    // Clean up any blood from the flayed effect
    if (mons.has_ench(ENCH_FLAYED))
        heal_flayed_effect(&mons, true, true);

    crawl_state.inc_mon_acting(&mons);

    ASSERT(!(YOU_KILL(killer) && crawl_state.game_is_arena()));

    handle_monster_dies_lua(mons, killer);
    mons_clear_trapping_net(&mons);
    mons.stop_constricting_all();
    mons.stop_being_constricted();

    you.remove_beholder(mons);
    you.remove_fearmonger(&mons);
    // Uniques leave notes and milestones, so this information is already leaked.
    remove_unique_annotation(&mons);

    const bool summoned      = mons.is_summoned();
    int  duration            = summoned ? mons.get_ench(ENCH_SUMMON_TIMER).duration : 0;
    const int monster_killed = mons.mindex();
    const bool hard_reset    = testbits(mons.flags, MF_HARD_RESET);
    const bool timeout       = killer == KILL_TIMEOUT;
    const bool gives_player_xp = mons_gives_xp(mons, you);
    bool drop_items          = !hard_reset;
    bool in_transit          = false;
    const bool was_banished  = (killer == KILL_BANISHED);
    const bool mons_reset    = RESET_KILL(killer);
    bool leaves_corpse = !summoned && !timeout
                            && !mons_reset
                            && !mons_is_tentacle_segment(mons.type);
    const bool real_death    = !(timeout && mons.is_abjurable())
                               && !mons_reset && !was_banished;
    // Award experience for suicide if the suicide was caused by the
    // player.
    if (MON_KILL(killer) && monster_killed == killer_index)
    {
        if (mons.confused_by_you())
        {
            ASSERT(!crawl_state.game_is_arena());
            killer = KILL_YOU_CONF;
        }
    }
    else if (MON_KILL(killer) && mons.has_ench(ENCH_CHARM))
    {
        bool arena = crawl_state.game_is_arena();
        mon_enchant ench = mons.get_ench(ENCH_CHARM);
        if (ench.who == KC_YOU || (!arena && ench.who == KC_FRIENDLY))
        {
            ASSERT(!arena);
            killer = KILL_YOU_CONF; // Well, it was confused in a sense... (jpeg)
        }
    }

    // Kills by the spectral weapon are considered as kills by the player
    // instead. Ditto Dithmenos shadow actions.
    if (MON_KILL(killer)
        && !invalid_monster_index(killer_index)
        && ((env.mons[killer_index].type == MONS_SPECTRAL_WEAPON
             && env.mons[killer_index].summoner == MID_PLAYER)
            || mons_is_player_shadow(env.mons[killer_index])))
    {
        killer_index = you.mindex();
        killer = (killer == KILL_MON_MISSILE) ? KILL_YOU_MISSILE : KILL_YOU;
    }

    // Take notes and mark milestones.
    record_monster_defeat(&mons, killer);

    // Chance to cause monsters you kill yourself to explode with Mark of Haemoclasm
    if (YOU_KILL(killer) && you.has_mutation(MUT_MAKHLEB_MARK_HAEMOCLASM)
        && makhleb_haemoclasm_trigger_check(mons))
    {
        mons.props[MAKHLEB_HAEMOCLASM_KEY] = true;
    }

    if (you.prev_targ == mons.mid)
    {
        you.prev_targ = MID_NOBODY;
        crawl_state.cancel_cmd_repeat();
    }

    if (killer == KILL_YOU)
        crawl_state.cancel_cmd_repeat();

    const bool pet_kill = _is_pet_kill(killer, killer_index);
    bool did_death_message = false;

    // We do some of these BEFORE checking for explosions from inner flame,
    // if we don't want to prevent inner flame from doing certain effects of
    // cleanup.
    //
    // (It's possible some other things should be moved here, but dead code that
    // deals primarily with messaging seems fine to override by exploding)
    if (mons.type == MONS_PROTEAN_PROGENITOR && real_death)
    {
        _protean_explosion(&mons);
        silent = true;
    }
    else if (mons.type == MONS_SPRIGGAN_DRUID && !silent && real_death)
        _druid_final_boon(&mons);
    else if (mons.type == MONS_VAMPIRE_BAT && !silent && !mons_reset
             && mons.props.exists(BLORKULA_REVIVAL_TIMER_KEY))
    {
        _blorkula_bat_death(mons, killer, killer_index);
    }
    else if (mons.type == MONS_SOUL_WISP)
    {
        actor* source = mons.get_ench(ENCH_HAUNTING).agent();
        if (source && source->alive())
        {
            if (!silent)
                simple_monster_message(mons, " returns to where it belongs.");

            if (source->is_monster())
            {
                source->as_monster()->del_ench(ENCH_WEAK);
                source->props.erase(SOUL_SPLINTERED_KEY);
            }
            else if (source->is_player())
            {
                you.duration[DUR_WEAK] = 0;
                mprf(MSGCH_RECOVERY, "You feel your strength returning.");
            }

            silent = true;
        }
    }
    // Only transform if we 'died' to timeout. Something simply dealing damage
    // to us can still shatter us.
    else if (mons.type == MONS_BLOCK_OF_ICE
             && mons.has_ench(ENCH_SIMULACRUM_SCULPTING)
             && timeout)
    {
        mgen_data simu = mgen_data(MONS_SIMULACRUM, BEH_COPY, mons.pos(),
                            BEH_FRIENDLY, MG_AUTOFOE | MG_FORCE_PLACE)
                         .set_summoned(&you, SPELL_SIMULACRUM, summ_dur(3), false);
        simu.base_type = (monster_type)mons.props[SIMULACRUM_TYPE_KEY].get_int();

        // If the monster we want to create cannot occupy the tile the block of
        // ice is on, try to find some nearby spot where it can.
        // (Mostly this is an issue with kraken simulacra, at present.)
        if (!monster_habitable_grid(simu.base_type, mons.pos()))
            find_habitable_spot_near(mons.pos(), simu.base_type, 3, simu.pos, 0);

        monster_type real_simu_type = simu.base_type;
        // Don't use uniques' names here; their simulacra won't use them either.
        if (mons_is_unique(simu.base_type))
            real_simu_type = mons_species(simu.base_type);

        string msg = "Your " + mons_type_name(real_simu_type, DESC_PLAIN) +
                     " simulacrum begins to move.";
        make_derived_undead_fineff::schedule(simu.pos, simu,
                                             get_monster_data(simu.base_type)->HD,
                                             "the player",
                                             msg.c_str());

        silent = true;
    }
    else if (mons.type == MONS_BLAZEHEART_GOLEM && real_death && !timeout)
    {
        // Only blow up if non-dormant
        if (grid_distance(mons.pos(), you.pos()) <= 1)
        {
            mprf(MSGCH_WARN, "%s falls apart, revealing its core!",
                 mons.name(DESC_YOUR).c_str());

            int old_hd = mons.get_hit_dice();
            change_monster_type(&mons, MONS_BLAZEHEART_CORE);
            mons.set_hit_dice(old_hd);

            // Cores should not count as summons and either expire or be removed
            // by recasting golem itself.
            mons.del_ench(ENCH_SUMMON_TIMER, true, false);
            mons.heal(50000);
            mons.flags |= MF_PERSISTS;
            mons.flags &= ~MF_ACTUAL_SUMMON;

            // Give exactly enough energy to act immediately after the player's
            // next action, but never blow up during the same action that the
            // golem died.
            mons.speed_increment = 79;

            // Short-circuiting death, since we didn't 'die'
            return nullptr;
        }
        else
        {
            simple_monster_message(mons, " falls apart and the last of its fire"
                                         " goes out.");
            silent = true;
        }
    }
    else if (mons.type == MONS_MARTYRED_SHADE && !silent && real_death)
    {
        // Don't cause transformation on the player killing their own shade.
        // (Angering them will normally make them disappear, but if you do
        // enough damage in one hit, you can still get here)
        if (!YOU_KILL(killer) || mons.summoner != MID_PLAYER)
        {
            _martyr_death_wail(mons);

            // Short-circuit this death
            return nullptr;
        }
    }
    else if (mons.type == MONS_HOARFROST_CANNON && real_death
             && env.grid(mons.pos()) == DNGN_FLOOR)
    {
        temp_change_terrain(mons.pos(), DNGN_SHALLOW_WATER, random_range(50, 80),
                            TERRAIN_CHANGE_FLOOD);
    }
    else if (mons.type == MONS_SPLINTERFROST_BARRICADE && real_death
             && !timeout)
    {
        coord_def aim;
        if (!invalid_monster_index(killer_index) && env.mons[killer_index].alive())
            aim = env.mons[killer_index].pos();
        else if (killer_index == MHITYOU)
            aim = you.pos();

        if (!aim.origin())
        {
            if (splinterfrost_block_fragment(mons, aim))
                silent = true;
        }
    }
    else if (mons.type == MONS_INUGAMI && real_death)
    {
        if (&mons == find_canine_familiar())
        {
            // Prevent you from resummoning it for a little while.
            you.duration[DUR_CANINE_FAMILIAR_DEAD] = random_range(13, 21)
                                                        * BASELINE_DELAY;
        }
    }
    else if (mons.type == MONS_BATTLESPHERE)
        end_battlesphere(&mons, true);
    else if (mons.type == MONS_SPECTRAL_WEAPON)
        end_spectral_weapon(&mons, true, true);
    else if (mons.type == MONS_RENDING_BLADE)
    {
        mprf(MSGCH_DURATION, "Your magic returns to you!");
        inc_mp(you.props[RENDING_BLADE_MP_KEY].get_int());
        you.props.erase(RENDING_BLADE_MP_KEY);
    }
    else if (mons.type == MONS_FLAYED_GHOST)
        end_flayed_effect(&mons);
    else if (mons.type == MONS_PLAYER_SHADOW)
        dithmenos_cleanup_player_shadow(&mons);

    if (mons.has_ench(ENCH_MAGNETISED))
    {
        place_cloud(CLOUD_MAGNETISED_DUST, mons.pos(),
                        random_range(7, 11),
                        mons.get_ench(ENCH_MAGNETISED).agent());
    }

    if (mons.has_ench(ENCH_VENGEANCE_TARGET))
        beogh_progress_vengeance();

    if (leaves_corpse && mons.has_ench(ENCH_RIMEBLIGHT)
        && !silent && !was_banished && !mons_reset
        && mons.props.exists(RIMEBLIGHT_DEATH_KEY))
    {
        // If we died due to the rimeblight instakill threshold, leave a pillar
        // of rime behind.
        leaves_corpse = false;
        did_death_message = true;
        if (you.see_cell(mons.pos()))
        {
            mprf(MSGCH_MONSTER_DAMAGE, MDAM_DEAD,
                 "Tendrils of ice devour %s body!", mons.name(DESC_ITS).c_str());
        }
        death_spawn_fineff::schedule(MONS_PILLAR_OF_RIME,
                                    mons.pos(),
                                    random_range(4, 14) * BASELINE_DELAY,
                                    SPELL_RIMEBLIGHT);
    }

    if (monster_explodes(mons))
    {
        did_death_message =
            explode_monster(&mons, killer, pet_kill);
    }
    else if ((mons.type == MONS_FULMINANT_PRISM || mons.type == MONS_SHADOW_PRISM)
             && mons.prism_charge == 0)
    {
        if (!silent && !hard_reset && !was_banished)
        {
            simple_monster_message(mons, " detonates feebly.",
                                   false, MSGCH_MONSTER_DAMAGE, MDAM_DEAD);
            silent = true;
        }
    }
    else if (mons.type == MONS_SIMULACRUM)
    {
        if (!silent && !mons_reset && !was_banished)
        {
            simple_monster_message(mons, " vaporises!",
                                   false, MSGCH_MONSTER_DAMAGE, MDAM_DEAD);
            silent = true;
            did_death_message = true;
        }
    }

    if (mons.type == MONS_HAUNTED_ARMOUR && real_death)
    {
        // Making the current sensible assumption that these are only ever
        // created by Cacophony.
        simple_monster_message(mons, " is driven back to you.", false,
                                MSGCH_MONSTER_DAMAGE, MDAM_DEAD);
        did_death_message = true;
    }
    if (mons.type == MONS_DANCING_WEAPON)
    {
        int w_idx = mons.inv[MSLOT_WEAPON];
        ASSERT(w_idx != NON_ITEM);

        bool summoned_it = mons.is_abjurable();

        if (mons.was_created_by(SPELL_FLASHING_BALESTRA) && !silent && !was_banished)
        {
            if (monster* armoury = monster_by_mid(mons.summoner))
            {
                simple_monster_message(mons, " returns to the armoury!",
                                        false, MSGCH_MONSTER_DAMAGE, MDAM_DEAD);
                silent = true;
                did_death_message = true;

                if (armoury->alive() && armoury->see_cell_no_trans(mons.pos()))
                {
                    bolt visual;
                    visual.source = mons.pos();
                    visual.target = armoury->pos();
                    visual.flavour = BEAM_VISUAL;
                    visual.range = LOS_RADIUS;
                    visual.aimed_at_spot = true;
                    visual.fire();
                }
            }
        }
        // Let summoned dancing weapons be handled like normal summoned creatures.
        else if (!was_banished && !summoned_it && !silent && !hard_reset)
        {
            // Under Gozag, permanent dancing weapons get turned to gold.
            // Exception: Tukima'd weapons; we don't want to trickily punish Gozagites
            // for using Tukima's Dance.
            if (have_passive(passive_t::goldify_corpses)
                && !mons.props.exists(TUKIMA_WEAPON))
            {
                if (mons_will_goldify(mons))
                {
                    simple_monster_message(mons,
                                       " turns to gold and falls from the air.",
                                       false, MSGCH_MONSTER_DAMAGE, MDAM_DEAD);
                }
                else
                {
                    // something(??) is suppressing goldify
                    simple_monster_message(mons,
                                       " briefly glints gold and then vanishes.",
                                       false, MSGCH_MONSTER_DAMAGE, MDAM_DEAD);
                }
                drop_items = false;
                silent = true;
                did_death_message = true;
            }
            else
            {
                simple_monster_message(mons, " falls from the air.",
                                       false, MSGCH_MONSTER_DAMAGE, MDAM_DEAD);
                silent = true;
                did_death_message = true;
            }
        }

        if (was_banished && !summoned_it && !hard_reset
            && mons.has_ench(ENCH_SUMMON_TIMER)) // temp animated but not summoned
        {
            // if this is set, it (in principle) allows the unrand to
            // show up in the abyss. We don't want to set this for a banished
            // permanent dancing weapon, which will show up as itself.
            if (is_unrandom_artefact(env.item[w_idx]))
                set_unique_item_status(env.item[w_idx], UNIQ_LOST_IN_ABYSS);

            destroy_item(w_idx);
        }
    }
    else if (mons.type == MONS_ELDRITCH_TENTACLE)
    {
        if (!silent && !mons_reset && !mons.has_ench(ENCH_SEVERED)
            && !was_banished)
        {
            if (you.can_see(mons))
            {
                mprf(MSGCH_MONSTER_DAMAGE, MDAM_DEAD, silenced(mons.pos()) ?
                    "The tentacle is hauled back through the portal!" :
                    "With a roar, the tentacle is hauled back through the portal!");
            }
            silent = true;
        }
    }
    else if (mons.type == MONS_DROWNED_SOUL)
    {
        // Suppress death message if 'killed' by touching something
        if (mons.hit_points == -1000)
            silent = true;
    }
    else if (mons.type == MONS_ARMOUR_ECHO)
        drop_items = false;

    const bool death_message = !silent && !did_death_message
                               && you.can_see(mons);
    const bool exploded {mons.flags & MF_EXPLODE_KILL};
    bool anon = (killer_index == ANON_FRIENDLY_MONSTER);
    const mon_holy_type targ_holy = mons.holiness();

    // Print standard death messages, handle god conducts and piety gain, and
    // perform other killer_type specific actions (like handling banishment).
    switch (killer)
    {
        case KILL_YOU:          // You kill in combat.
        case KILL_YOU_MISSILE:  // You kill by missile or beam.
        case KILL_YOU_CONF:     // You kill by confusion.
        {
            if (death_message)
            {
                if (killer == KILL_YOU_CONF
                    && (anon || !invalid_monster_index(killer_index)))
                {
                    mprf(MSGCH_MONSTER_DAMAGE, MDAM_DEAD, "%s is %s!",
                         mons.name(DESC_THE).c_str(),
                         exploded                        ? "blown up" :
                         wounded_damaged(targ_holy)      ? "destroyed"
                                                         : "killed");
                }
                else
                {
                    mprf(MSGCH_MONSTER_DAMAGE, MDAM_DEAD, "You %s %s!",
                         exploded                        ? "blow up" :
                         wounded_damaged(targ_holy)      ? "destroy"
                                                         : "kill",
                         mons.name(DESC_THE).c_str());
                }
            }

            // do this for some of the `silent` cases, if they explicitly set
            // a death message earlier
            if (death_message || did_death_message)
            {
                // If this monster would otherwise give xp but didn't for some
                // reason that wasn't clearly announced, give a message.
                if (!gives_player_xp
                    && mons_class_gives_xp(mons.type)
                    && !summoned
                    && !mons.is_unrewarding()
                    && !mons.friendly())
                {
                    mpr("That felt strangely unrewarding.");
                }
            }

            // Killing triggers hints mode lesson.
            if (gives_player_xp)
                _hints_inspect_kill();

            _fire_kill_conducts(mons, killer, killer_index, gives_player_xp);

            if (gives_player_xp && you_worship(GOD_RU) && you.piety < 200
                && one_chance_in(2))
            {
                ASSERT(you.props.exists(RU_SACRIFICE_PROGRESS_KEY));
                int current_progress =
                        you.props[RU_SACRIFICE_PROGRESS_KEY].get_int();
                you.props[RU_SACRIFICE_PROGRESS_KEY] = current_progress + 1;
            }

            break;
        }

        case KILL_MON:          // Monster kills in combat.
        case KILL_MON_MISSILE:  // Monster kills by missile or beam.
        {
            if (death_message)
            {
                const char* msg =
                    exploded                   ? " is blown up!" :
                    wounded_damaged(targ_holy) ? " is destroyed!"
                                               : " dies!";
                simple_monster_message(mons, msg, false, MSGCH_MONSTER_DAMAGE,
                                       MDAM_DEAD);
            }

            if (crawl_state.game_is_arena())
                break;

            _fire_kill_conducts(mons, killer, killer_index, gives_player_xp);

            // Check for applicability before looking to see if a blessing or
            // on-kill effect should happen.
            if (anon
                || invalid_monster_index(killer_index)
                || !gives_player_xp)
            {
                break;
            }

            monster* killer_mon = &env.mons[killer_index];

            if (killer_mon->wearing_ego(OBJ_ARMOUR, SPARM_MAYHEM))
                _orb_of_mayhem(*killer_mon, mons);

            if (pet_kill && _god_will_bless_follower(&mons))
                bless_follower(killer_mon);

            break;
        }

        // Monster killed by trap/inanimate thing/itself/poison not from you.
        case KILL_NON_ACTOR:
            if (death_message)
            {
                const char* msg =
                    exploded                     ? " is blown up!" :
                    wounded_damaged(targ_holy)   ? " is destroyed!"
                                                    : " dies!";
                simple_monster_message(mons, msg, false, MSGCH_MONSTER_DAMAGE,
                                        MDAM_DEAD);
            }
            break;

        case KILL_BANISHED:
            // Monster goes to the Abyss.
            mons.flags |= MF_BANISHED;
            if (mons.pacified() || !mons.needs_abyss_transit())
            {
                // A banished monster that doesn't go on the transit list
                // loses all items.
                if (!mons.is_summoned())
                    drop_items = false;
                break;
            }

            {
                unwind_var<int> dt(mons.damage_total, 0);
                unwind_var<int> df(mons.damage_friendly, 0);
                mons.set_transit(level_id(BRANCH_ABYSS));
            }
            set_unique_annotation(&mons, BRANCH_ABYSS);
            in_transit = true;
            drop_items = false;
            mons.firing_pos.reset();
            // Make monster stop patrolling and/or travelling.
            mons.patrol_point.reset();
            mons.travel_path.clear();
            mons.travel_target = MTRAV_NONE;
            break;

        case KILL_RESET:
            drop_items = false;
            break;

        case KILL_TIMEOUT:
        {
            if (!death_message || mons.is_abjurable())
                break;

            // Only print messages here for non-abjurable monsters expiring by
            // timeout. Poof messages for abjurable summons (which happen
            // regardless of how the summon was killed) are handled later, in
            // _print_summon_poof_message

            string msg;
            if (mons.has_ench(ENCH_VAMPIRE_THRALL))
                msg = " turns to dust.";
            // ratskin cloak
            else if (mons_genus(mons.type) == MONS_RAT)
                msg = " returns to the shadows of the Dungeon.";
            // Death Channel / Soul Splinter
            else if (mons.type == MONS_SPECTRAL_THING
                     || mons.type == MONS_SOUL_WISP)
            {
                msg = " fades into mist!";
            }
            // Animate Dead/Infestation
            else if (mons.type == MONS_ZOMBIE
                        || mons.type == MONS_SKELETON
                        || mons.type == MONS_DEATH_SCARAB)
            {
                msg = " crumbles into dust!";
            }
            else if (mons.type == MONS_PILE_OF_DEBRIS)
                msg = " collapses into dust.";
            else if (mons.type == MONS_PILLAR_OF_SALT
                    || mons.type == MONS_WITHERED_PLANT
                    || mons.type == MONS_BRIAR_PATCH)
            {
                msg = " crumbles away.";
            }
            else if (mons.type == MONS_SNAPLASHER_VINE)
                msg = " falls limply to the ground.";
            else if (mons.type == MONS_HOARFROST_CANNON
                     || mons.type == MONS_BLOCK_OF_ICE
                     || mons.type == MONS_SPLINTERFROST_BARRICADE)
            {
                msg = " melts away.";
            }
            else if (mons.type == MONS_FIRE_VORTEX
                     || mons.type == MONS_SPATIAL_VORTEX
                     || mons.type == MONS_TWISTER
                     || mons.type == MONS_FOXFIRE)
            {
                msg = " dissipates.";
            }
            else if (mons.type == MONS_CLOCKWORK_BEE)
                msg = " runs out of power.";
            else if (mons.type == MONS_CLOCKWORK_BEE_INACTIVE)
                msg = " falls apart.";
            else if (mons.type == MONS_PLATINUM_PARAGON)
                msg = " expends the last of its power.";
            else if (mons.type == MONS_RENDING_BLADE)
                msg = " implodes with a snap.";
            else
            {
                if (mons.props.exists(KIKU_WRETCH_KEY))
                    mprf("A nearby %s perishes wretchedly.", mons.name(DESC_PLAIN, false).c_str());
                else if (mons_class_is_fragile(mons.type))
                {
                    mprf(MSGCH_MONSTER_TIMEOUT, "A nearby %s withers and dies.",
                         mons.name(DESC_PLAIN, false).c_str());
                }
                // Default message so that at least *something* is printed.
                else
                    msg = " fades away.";
            }

            if (!msg.empty())
                simple_monster_message(mons, msg.c_str(), false, MSGCH_MONSTER_TIMEOUT);
        }
        break;

        case KILL_RESET_KEEP_ITEMS:
            break;

        default:
            drop_items = false;
            break;
    }

    // Make sure Boris has a foe to address before speaking.
    _maybe_set_monster_foe(mons, killer_index);

    // Make sure that the monster looks dead.
    if (mons.alive() && (!summoned || duration > 0))
    {
        dprf("Non-damage %s of %s.", mons_reset ? "reset" : "kill",
                                        mons.name(DESC_A, true).c_str());
        if (YOU_KILL(killer) || pet_kill)
            mons.damage_friendly += mons.hit_points;
        mons.damage_total += mons.hit_points;

        if (!in_transit) // banishment only
            mons.hit_points = -1;
    }

    // Allow the monster to speak (or emote) about its own death.
    if (!silent && you.see_cell(mons.pos()))
    {
        // Make sure that the monster looks dead to mons_speaks, so that it can
        // look up death speach.
        if (mons.alive() && !in_transit && (!summoned || duration > 0))
            mons.hit_points = -1;

        // XXX: Likewise, we do this so mons_speaks will recognize timeouts.
        //      (But maybe the function should actually take arguments?)
        if (timeout && mons.is_summoned())
        {
            mon_enchant summ = mons.get_ench(ENCH_SUMMON);
            summ.duration = -1;
            mons.update_ench(summ);
        }

        // Hack: with cleanup_dead=false, a tentacle [segment] of a dead
        // [malign] kraken has no valid head reference.
        if (!mons_is_tentacle_or_tentacle_segment(mons.type))
        {
            // Make sure Natasha gets her say even if she got polymorphed.
            const monster_type orig =
                mons_is_mons_class(&mons, MONS_NATASHA) ? MONS_NATASHA
                                                       : mons.type;
            unwind_var<monster_type> mt(mons.type, orig);
            mons_speaks(&mons);
        }
    }

    // None of these effects should trigger on illusory copies.
    if (!mons.is_illusion())
    {
        if (mons.type == MONS_BORIS && !in_transit && !mons.pacified())
        {
            // XXX: Actual blood curse effect for Boris? - bwr

            // Now that Boris is dead, he can be replaced when new levels
            // are generated.
            you.unique_creatures.set(mons.type, false);
            you.props[KILLED_BORIS_KEY] = true;
        }
        if (mons.type == MONS_JORY && !in_transit)
            blood_spray(mons.pos(), MONS_JORY, 50);
        else if (mons_is_mons_class(&mons, MONS_KIRKE)
                 && !in_transit
                 && !testbits(mons.flags, MF_WAS_NEUTRAL))
        {
            hogs_to_humans();
        }
        else if ((mons_is_mons_class(&mons, MONS_NATASHA))
                 && !in_transit && !mons.pacified()
                 && mons_felid_can_revive(&mons))
        {
            drop_items = false;

            // Like Boris, but regenerates immediately
            if (mons_is_mons_class(&mons, MONS_NATASHA))
                you.unique_creatures.set(MONS_NATASHA, false);
            if (!mons_reset)
                mons_felid_revive(&mons);
        }
        else if (mons_is_mons_class(&mons, MONS_PIKEL))
        {
            // His band doesn't care if he's dead or not, just whether or not
            // he goes away.
            pikel_band_neutralise();
        }
        else if (mons_is_elven_twin(&mons))
            elven_twin_died(&mons, in_transit, killer, killer_index);
        else if (mons.type == MONS_BENNU && !mons.pacified() && real_death
                 && mons_bennu_can_revive(&mons))
        {
            // All this information may be lost by the time the monster revives.
            const int revives = (mons.props.exists(BENNU_REVIVES_KEY))
                                ? mons.props[BENNU_REVIVES_KEY].get_byte() : 0;
            const bool duel = mons.props.exists(OKAWARU_DUEL_CURRENT_KEY);
            const beh_type att = mons.has_ench(ENCH_CHARM)
                                 ? BEH_HOSTILE : SAME_ATTITUDE(&mons);

            // Carry over bribe enchantments (as otherwise revived bribed
            // bennu will follow the player out of their branch)
            const mon_enchant gozag_bribe = mons.get_ench(ENCH_NEUTRAL_BRIBED,
                                                          ENCH_FRIENDLY_BRIBED);

            // Don't consider this a victory yet, and duel the new bennu.
            if (duel)
                mons.props.erase(OKAWARU_DUEL_CURRENT_KEY);

            bennu_revive_fineff::schedule(mons.pos(), revives, att, mons.foe,
                                          duel, gozag_bribe);
        }
    }

    if (mons_is_tentacle_head(mons_base_type(mons)))
    {
        if (destroy_tentacles(&mons)
            && !in_transit
            && you.see_cell(mons.pos())
            && !was_banished)
        {
            if (mons_base_type(mons) == MONS_KRAKEN)
                mpr("The kraken's tentacles disappear.");
            else if (mons.type == MONS_TENTACLED_STARSPAWN)
                mpr("The starspawn's tentacles wither and die.");
        }
    }
    else if (mons_is_tentacle_or_tentacle_segment(mons.type)
             && killer != KILL_TENTACLE_CLEANUP
                 || mons.type == MONS_ELDRITCH_TENTACLE
                 || mons.type == MONS_SNAPLASHER_VINE)
    {
        // XXX: Make sure this segment looks dead, or destroy_tentacle may
        //      reset it before this function completes
        mons.hit_points = -1;
        destroy_tentacle(&mons);
    }
    else if (mons.type == MONS_ELDRITCH_TENTACLE_SEGMENT
             && killer != KILL_TENTACLE_CLEANUP)
    {
       monster_die(*monster_by_mid(mons.tentacle_connect), killer,
                   killer_index, silent, mount_death);
    }
    // Give the treant a last chance to release its hornets if it is killed in a
    // single blow from above half health
    else if (mons.type == MONS_SHAMBLING_MANGROVE && real_death)
        treant_release_fauna(mons);
    else if (mons.type == MONS_PHARAOH_ANT && real_death)
        _pharaoh_ant_bind_souls(&mons);
    else if (!mons.is_summoned() && mummy_curse_power(mons.type) > 0)
    {
        // TODO: set attacker better? (Player attacker is handled by checking
        // killer when running the fineff.)
        mummy_death_curse_fineff::schedule(
                invalid_monster_index(killer_index)
                                            ? nullptr : &env.mons[killer_index],
                &mons,
                killer,
                mummy_curse_power(mons.type));
    }

    // Activate various on-kill effects for the player (like divine healing,
    // Powered by Death, berserk extension, etc.)
    _player_on_kill_effects(mons, killer, gives_player_xp, pet_kill);

    if (mons.has_ench(ENCH_RIMEBLIGHT) && !was_banished && !mons_reset)
    {
        if (you.can_see(mons))
            mprf("Plague seeps from the dead %s.", mons.name(DESC_PLAIN).c_str());

        // Potentially infect everyone around the dead monster.
        // (100% chance at range 1, 50% chance at range 2)
        for (radius_iterator ri(mons.pos(), 2, C_SQUARE, LOS_NO_TRANS); ri; ++ri)
        {
            monster* victim = monster_at(*ri);
            if (victim && !victim->friendly())
            {
                if (grid_distance(*ri, mons.pos()) == 1 || coinflip())
                {
                    maybe_spread_rimeblight(*victim,
                                            mons.props[RIMEBLIGHT_POWER_KEY].get_int());
                }
            }
        }
    }

    // Necromancy
    bool corpse_consumed = false;
    if (!was_banished && !mons_reset)
    {
        const bool in_los = you.see_cell(mons.pos());
        const bool wretch = mons.props.exists(KIKU_WRETCH_KEY);
        const bool corpseworthy = gives_player_xp || wretch;
        const bool corpse_gone = exploded || mons.props.exists(NEVER_CORPSE_KEY);

        // no doubling up with death channel and yred.
        // otherwise, death channel can work with other corpse-consuming spells.
        if (in_los
            && corpseworthy
            && you.duration[DUR_DEATH_CHANNEL]
            && !have_passive(passive_t::reaping))
        {
            _make_derived_undead(&mons, !death_message, MONS_SPECTRAL_THING,
                                 BEH_FRIENDLY,
                                 SPELL_DEATH_CHANNEL,
                                 static_cast<god_type>(you.attribute[ATTR_DIVINE_DEATH_CHANNEL]));
        }

        if (in_los && corpseworthy && yred_torch_is_raised())
            yred_feed_torch(&mons);

        corpse_consumed = _apply_necromancy(mons, !death_message, corpse_gone,
                                            in_los, corpseworthy);

        if (in_los && corpseworthy && you.duration[DUR_GRAVE_CLAW_RECHARGE]
            && (mons.holiness() & MH_NATURAL))
        {
            gain_grave_claw_soul();
        }
    }

    // Poof messages for summoned things exploded are stored in explosion_fineff
    // to print later.
    if (!silent && !mons_reset && !exploded)
        _print_summon_poof_message(mons, real_death);

    if (!was_banished && !mons_reset && !exploded)
        _monster_die_cloud(mons, real_death);

    item_def* corpse = nullptr;
    if (leaves_corpse && !was_banished && !spectralised && !corpse_consumed)
    {
        // Have to add case for disintegration effect here? {dlb}
        item_def* daddy_corpse = nullptr;

        if (mons.type == MONS_SPRIGGAN_RIDER)
        {
            daddy_corpse = mounted_kill(&mons, MONS_HORNET, killer, killer_index);
            mons.type = MONS_SPRIGGAN;
        }
        corpse = place_monster_corpse(mons);
        if (!corpse)
            corpse = daddy_corpse;
    }

    const unsigned int player_xp = gives_player_xp
        ? _calc_player_experience(&mons) : 0;

    if (!crawl_state.game_is_arena() && leaves_corpse && !in_transit)
        you.kills.record_kill(&mons, killer, pet_kill);

    if (mount_death)
    {
        _give_player_experience(player_xp, killer, pet_kill,
                                was_visible, mons.xp_tracking);
        crawl_state.dec_mon_acting(&mons);

        return corpse;
    }

    // If there are other duel targets alive (due to a slime splitting), don't
    // count this as winning the duel.
    if (mons.props.exists(OKAWARU_DUEL_CURRENT_KEY))
    {
        for (monster_iterator mi; mi; ++mi)
        {
            if (mi->props.exists(OKAWARU_DUEL_CURRENT_KEY) && *mi != &mons)
            {
                mons.props.erase(OKAWARU_DUEL_CURRENT_KEY);
                break;
            }
        }
    }

    mons_remove_from_grid(mons);
    fire_monster_death_event(&mons, killer, false);

    if (crawl_state.game_is_arena())
        arena_monster_died(&mons, killer, killer_index, silent, corpse);

    const coord_def mwhere = mons.pos();
    if (drop_items)
    {
        // monster_drop_things may lead to a level excursion (via
        // ash_id_item -> ... -> ShoppingList::item_type_identified),
        // which fails to save/restore the dead monster. Keep it alive
        // since we still need it.
        unwind_var<int> fakehp(mons.hit_points, 1);
        monster_drop_things(&mons, YOU_KILL(killer) || pet_kill);
    }
    else
    {
        // Destroy the items belonging to MF_HARD_RESET monsters so they
        // don't clutter up env.item[].
        mons.destroy_inventory();
    }

    if (leaves_corpse && corpse)
    {
        if (!silent)
            _special_corpse_messaging(mons);
        // message ordering... :(
        if (corpse->base_type == OBJ_CORPSES // not gold
            && !mons.props.exists(KIKU_WRETCH_KEY))
        {
            const monster_type orig = static_cast<monster_type>(corpse->orig_monnum);
            // Avoid a possible crash with level excursions
            // (See previous code block comment ^)
            unwind_var<int> fakehp(mons.hit_points, 1);
            maybe_drop_monster_organ(corpse->mon_type, orig,
                                     item_pos(*corpse), silent);
        }
    }

    ASSERT(mons.type != MONS_NO_MONSTER);

    if (mons.is_divine_companion() && real_death)
    {
        remove_companion(&mons);
        if (mons_is_hepliaklqana_ancestor(mons.type))
        {
            if (!you.can_see(mons))
            {
                mprf("%s has departed this plane of existence.",
                     hepliaklqana_ally_name().c_str());
            }

            // respawn in ~30-60 turns, if there wasn't another ancestor through
            // some strange circumstance (wizmode? bug?)
            if (hepliaklqana_ancestor() == MID_NOBODY)
                you.duration[DUR_ANCESTOR_DELAY] = random_range(300, 600);
        }
        else if (mons.type == MONS_ORC_APOSTLE)
            beogh_swear_vegeance(mons);
    }
    else if (mons.is_divine_companion()
             && killer == KILL_BANISHED
             && mons.type == MONS_ORC_APOSTLE)
    {
        beogh_follower_banished(mons);
    }

    // If we kill an invisible monster reactivate autopickup.
    // We need to check for actual invisibility rather than whether we
    // can see the monster. There are several edge cases where a monster
    // is visible to the player but we still need to turn autopickup
    // back on, such as TSO's halo or sticky flame. (jpeg)
    if (you.see_cell(mons.pos()) && mons.has_ench(ENCH_INVIS)
        && !mons.friendly())
    {
        autotoggle_autopickup(false);
    }

    crawl_state.dec_mon_acting(&mons);
    monster_cleanup(&mons);

    // Force redraw for monsters that die.
    if (in_bounds(mwhere) && you.see_cell(mwhere))
    {
        view_update_at(mwhere);
        update_screen();
    }

    if (!mons_reset)
    {
        _give_player_experience(player_xp, killer, pet_kill, was_visible,
                                mons.xp_tracking);
    }
    return corpse;
}

void heal_flayed_effect(actor* act, bool quiet, bool blood_only)
{
    ASSERT(act); // XXX: change to actor &act
    if (!blood_only)
    {
        if (act->is_player())
            you.duration[DUR_FLAYED] = 0;
        else
            act->as_monster()->del_ench(ENCH_FLAYED, true, false);

        if (you.can_see(*act) && !quiet)
        {
            mprf("The terrible wounds on %s body vanish.",
                 act->name(DESC_ITS).c_str());
        }

        act->heal(act->props[FLAY_DAMAGE_KEY].get_int());
        act->props.erase(FLAY_DAMAGE_KEY);
    }

    for (const CrawlStoreValue& store : act->props[FLAY_BLOOD_KEY].get_vector())
        env.pgrid(store.get_coord()) &= ~FPROP_BLOODY;
    act->props.erase(FLAY_BLOOD_KEY);
}

void end_flayed_effect(monster* ghost)
{
    if (you.duration[DUR_FLAYED] && !ghost->wont_attack())
        heal_flayed_effect(&you);

    for (monster_iterator mi; mi; ++mi)
    {
        if (mi->has_ench(ENCH_FLAYED) && !mons_aligned(ghost, *mi))
            heal_flayed_effect(*mi);
    }
}

// Clean up a monster that's stopped existing on the current floor (whether
// because they died or because they're transiting to a new floor).
void monster_cleanup(monster* mons)
{
    crawl_state.mon_gone(mons);

    ASSERT(mons->type != MONS_NO_MONSTER);

    if (mons->has_ench(ENCH_AWAKEN_FOREST))
    {
        forest_message(mons->pos(), "The forest abruptly stops moving.");
        env.forest_awoken_until = 0;
    }

    // Monsters' haloes should be removed when they die.
    if (mons->halo_radius() >= 0
        || mons->umbra_radius() >= 0
        || mons->silence_radius() >= 0
        || mons->liquefying_radius() >= 0)
    {
        invalidate_agrid();
    }

    if (mons->type == MONS_PLATINUM_PARAGON)
        you.duration[DUR_PARAGON_ACTIVE] = 0;

    // May have been constricting something. No message because that depends
    // on the order in which things are cleaned up: If the constrictee is
    // cleaned up first, we wouldn't get a message anyway.
    mons->stop_constricting_all(false, true);

    mons->clear_far_engulf();

    if (mons_is_tentacle_head(mons_base_type(*mons)))
        destroy_tentacles(mons);

    const mid_t mid = mons->mid;
    env.mid_cache.erase(mid);

    mons->remove_summons();

    unsigned int monster_killed = mons->mindex();
    for (monster_iterator mi; mi; ++mi)
    {
        if (mi->foe == monster_killed)
            mi->foe = MHITNOT;
    }

    if (you.pet_target == monster_killed)
        you.pet_target = MHITNOT;

    mons->reset();
}

item_def* mounted_kill(monster* daddy, monster_type mc, killer_type killer,
                       int killer_index)
{
    monster mon;
    mon.type = mc;
    mon.moveto(daddy->pos());
    define_monster(mon); // assumes mc is not a zombie
    mon.flags = daddy->flags;

    // Need to copy ENCH_SUMMON_TIMER etc. or we could get real XP/meat from a summon.
    mon.enchantments = daddy->enchantments;
    mon.ench_cache = daddy->ench_cache;

    mon.attitude = daddy->attitude;
    mon.damage_friendly = daddy->damage_friendly;
    mon.damage_total = daddy->damage_total;
    // Keep the rider's name, if it had one (Mercenary card).
    if (!daddy->mname.empty() && mon.type == MONS_SPRIGGAN)
        mon.mname = daddy->mname;
    if (daddy->props.exists(REAPING_DAMAGE_KEY))
    {
        dprf("Mounted kill: marking the other monster as reaped as well.");
        mon.props[REAPING_DAMAGE_KEY].get_int() = daddy->props[REAPING_DAMAGE_KEY].get_int();
        mon.props[REAPER_KEY].get_int() = daddy->props[REAPER_KEY].get_int();
    }

    return monster_die(mon, killer, killer_index, false, true);
}

/**
 * Applies harmful environmental effects from the current tile to monsters.
 *
 * @param mons      The monster to maybe drown/incinerate.
 * @param oldpos    Their previous tile, before landing up here.
 * @param killer    Who's responsible for killing them, if they die here.
 * @param killnum   The mindex of the killer, if any.
 */
void mons_check_pool(monster* mons, const coord_def &oldpos,
                     killer_type killer, int killnum)
{
    // Flying monsters don't make contact with the terrain.
    if (!mons->ground_level())
        return;

    dungeon_feature_type grid = env.grid(mons->pos());
    if (grid != DNGN_LAVA && grid != DNGN_DEEP_WATER
        || monster_habitable_feat(mons, grid))
    {
        return;
    }

    // Don't worry about invisibility. You should be able to see if
    // something has fallen into the lava.
    if (you.see_cell(mons->pos()) && (oldpos == mons->pos() || env.grid(oldpos) != grid))
    {
         mprf("%s falls into the %s!",
             mons->name(DESC_THE).c_str(),
             grid == DNGN_LAVA ? "lava" : "water");
    }

    // Even fire resistant monsters perish in lava.
    if (grid == DNGN_LAVA && mons->res_fire() < 2)
    {
        simple_monster_message(*mons, " is incinerated.", false,
                               MSGCH_MONSTER_DAMAGE, MDAM_DEAD);
    }
    else if (mons->can_drown())
    {
        simple_monster_message(*mons, " drowns.", false,
                               MSGCH_MONSTER_DAMAGE, MDAM_DEAD);
    }
    else if (mons->type == MONS_BOULDER)
    {
        simple_monster_message(*mons, " sinks to the bottom.", false,
                               MSGCH_MONSTER_DAMAGE, MDAM_DEAD);
    }
    else
    {
        simple_monster_message(*mons, " falls apart.", false,
                               MSGCH_MONSTER_DAMAGE, MDAM_DEAD);
    }

    if (killer == KILL_NONE)
    {
        // Self-kill.
        killer  = KILL_MON;
        killnum = mons->mindex();
    }

    // Yredelemnul special, redux: It's the only one that can
    // work on drowned monsters.
    if (!_yred_bind_soul(mons, killer))
        monster_die(*mons, killer, killnum, true);
}

// Make all of the monster's original equipment disappear, unless it's a fixed
// artefact or unrand artefact.
static void _vanish_orig_eq(monster* mons)
{
    for (mon_inv_iterator ii(*mons); ii; ++ii)
    {
        if (origin_known(*ii) || ii->orig_monnum != 0
            || !ii->inscription.empty()
            || is_unrandom_artefact(*ii)
            || (ii->flags & (ISFLAG_DROPPED | ISFLAG_THROWN
                              | ISFLAG_NOTED_GET)))
        {
            continue;
        }
        ii->flags |= ISFLAG_SUMMONED;
    }
}

int dismiss_monsters(string pattern)
{
    // Make all of the monsters' original equipment disappear unless "keepitem"
    // is found in the regex (except for fixed arts and unrand arts).
    const bool keep_item = strip_tag(pattern, "keepitem");
    const bool harmful = pattern == "harmful";
    const bool mobile  = pattern == "mobile";
    const bool los     = pattern == "los";

    // Dismiss by regex.
    text_pattern tpat(pattern);
    int ndismissed = 0;
    for (monster_iterator mi; mi; ++mi)
    {
        if (mi->alive()
            && (mobile ? !mons_class_is_stationary(mi->type) :
                harmful ? mons_is_threatening(**mi) && !mi->wont_attack() :
                los ? you.see_cell(mi->pos())
                : tpat.empty() || tpat.matches(mi->name(DESC_PLAIN, true))))
        {
            if (!keep_item)
                _vanish_orig_eq(*mi);
            monster_die(**mi, KILL_RESET_KEEP_ITEMS, NON_MONSTER, true);
            ++ndismissed;
        }
    }

    return ndismissed;
}

// Returns a special message for an abjurable summon disappearing. This message
// is printed when the monster expires due to timeout or, additionally, after
// the normal death message when it is destroyed directly.
//
// For special death messages for non-abjurable summons (printed *only* on
// timeout), see the block under KILL_TIMEOUT in monster_die().
string summoned_poof_msg(const monster& mons)
{
    if (invalid_monster(&mons))
        return "";

    // XXX: chaos spawn death clouds use these messages, whether they were
    // summoned or not.
    if (!mons.is_abjurable() && mons.type != MONS_CHAOS_SPAWN)
        return "";

    int summon_type = mons.get_ench(ENCH_SUMMON).degree;

    string msg = "disappears in a puff of smoke";

    switch (summon_type)
    {
    case SPELL_SHADOW_CREATURES:
    case MON_SUMM_SCROLL:
        msg = "dissolves into shadows";
        break;

    case MON_SUMM_BUTTERFLIES:
        msg = "disappears in a burst of colours";
        break;

    case MON_SUMM_CHAOS:
        msg = "degenerates into a cloud of primal chaos";
        break;

    case MON_SUMM_WRATH:
    case MON_SUMM_AID:
        if (is_good_god(mons.god))
            msg = "dissolves into sparkling lights";
        else if (mons.god == GOD_YREDELEMNUL)
            msg = "returns to the grave";
        break;

    case SPELL_SPECTRAL_CLOUD:
    case SPELL_CALL_LOST_SOULS:
        msg = "fades away";
        break;

    case SPELL_STICKS_TO_SNAKES:
        msg = "turns back into a lifeless stick";
        break;
    }

    if (mons.god == GOD_XOM && one_chance_in(10)
        || mons.type == MONS_CHAOS_SPAWN)
    {
        msg = "degenerates into a cloud of primal chaos";
    }

    if (mons.is_holy()
        && summon_type != SPELL_SHADOW_CREATURES
        && summon_type != MON_SUMM_CHAOS)
    {
        msg = "dissolves into sparkling lights";
    }

    if (mons_is_slime(mons)
        && mons.god == GOD_JIYVA)
    {
        msg = "dissolves into a puddle of slime";
    }

    if (mons.type == MONS_DROWNED_SOUL)
        msg = "returns to the deep";

    if (mons.has_ench(ENCH_PHANTOM_MIRROR))
        msg = "shimmers and vanishes";

    if (mons.type == MONS_LIVING_SPELL)
        msg = "disperses";

    return msg;
}

/**
 * Determine if a specified monster is or was a specified monster type.
 *
 * Checks both the monster type and the ORIGINAL_TYPE_KEY prop, thus allowing
 * the type to be transferred through polymorph.
 *
 * @param mons    The monster to be checked.
 * @param type    The type it might be.
 * @return        True if the monster was or is the type, otherwise false.
**/
bool mons_is_mons_class(const monster* mons, monster_type type)
{
    return mons->type == type
           || mons->props.exists(ORIGINAL_TYPE_KEY)
              && mons->props[ORIGINAL_TYPE_KEY].get_int() == type;
}

/**
 * Perform neutralisation for members of Pikel's band upon Pikel's 'death'.
 *
 * This neutralisation occurs in multiple instances: when Pikel is neutralised,
 * charmed, when Pikel dies, when Pikel is banished.
 * It is handled by a daction (as a fineff) to preserve across levels.
 **/
void pikel_band_neutralise()
{
    int visible_minions = 0;
    for (monster_iterator mi; mi; ++mi)
    {
        if (mi->type == MONS_LEMURE
            && mi->props.exists(PIKEL_BAND_KEY)
            && mi->observable())
        {
            visible_minions++;
        }
    }
    string final_msg;
    if (visible_minions > 0 && you.num_turns > 0)
    {
        if (you.get_mutation_level(MUT_NO_LOVE))
        {
            const char *substr = visible_minions > 1 ? "minions" : "minion";
            final_msg = make_stringf("Pikel's spell is broken, but his former "
                                     "%s can only feel hate for you!", substr);
        }
        else
        {
            const char *substr = visible_minions > 1
                ? "minions thank you for their"
                : "minion thanks you for its";
            final_msg = make_stringf("With Pikel's spell broken, his former %s "
                                     "freedom.", substr);
        }
    }
    delayed_action_fineff::schedule(DACT_PIKEL_MINIONS, final_msg);
}

/**
 * Revert porkalated hogs.
 *
 * Called upon Kirke's death. Hogs either from Kirke's band or
 * subsequently porkalated should be reverted to their original form.
 * This takes place as a daction to preserve behaviour across levels;
 * this function simply checks if any are visible and raises a fineff
 * containing an appropriate message. The fineff raises the actual
 * daction.
 */
void hogs_to_humans()
{
    int any = 0, human = 0;

    for (monster_iterator mi; mi; ++mi)
    {
        if (mons_genus(mi->type) != MONS_HOG)
            continue;

        if (!mi->props.exists(KIRKE_BAND_KEY)
            && !mi->props.exists(ORIG_MONSTER_KEY))
        {
            continue;
        }

        // Shapeshifters will stop being a hog when they feel like it.
        if (mi->is_shapeshifter())
            continue;

        const bool could_see = you.can_see(**mi);

        if (could_see)
            any++;

        if (!mi->props.exists(ORIG_MONSTER_KEY) && could_see)
            human++;
    }

    string final_msg;
    if (any > 0 && you.num_turns > 0)
    {
        final_msg = make_stringf("No longer under Kirke's spell, the %s %s %s!",
                                 any > 1 ? "hogs return to their"
                                         : "hog returns to its",
                                 any == human ? "human" : "original",
                                 any > 1 ? "forms" : "form");
    }
    kirke_death_fineff::schedule(final_msg);
}

/**
 * Determine if a monster is either Dowan or Duvessa.
 *
 * Tracks through type and ORIGINAL_TYPE_KEY, thus tracking through polymorph.
 * Used to determine if a death function should be called for the monster
 * in question.
 *
 * @param mons    The monster to check.
 * @return        True if either Dowan or Duvessa, otherwise false.
**/
bool mons_is_elven_twin(const monster* mons)
{
    return mons_is_mons_class(mons, MONS_DOWAN)
           || mons_is_mons_class(mons, MONS_DUVESSA);
}

/**
 * Find Duvessa or Dowan, given the other.
 *
 * @param mons    The monster whose twin we seek.
 * @return        A pointer to the other elven twin, or nullptr if the twin
 *                no longer exists, or if given a mons that is not an elven twin.
**/
monster* mons_find_elven_twin_of(const monster* mons)
{
    if (!mons_is_elven_twin(mons))
        return nullptr;

    for (monster_iterator mi; mi; ++mi)
    {
        if (*mi == mons)
            continue;

        if (mons_is_elven_twin(*mi))
            return *mi;
    }

    return nullptr;
}

/**
 * Perform functional changes Dowan or Duvessa upon the other's death.
 *
 * This functional is called when either Dowan or Duvessa are killed or
 * banished. It performs a variety of changes in both attitude, spells, flavour,
 * speech, etc.
 *
 * @param twin          The monster who died.
 * @param in_transit    True if banished, otherwise false.
 * @param killer        The kill-type related to twin.
 * @param killer_index  The index of the actor who killed twin.
**/
void elven_twin_died(monster* twin, bool in_transit, killer_type killer, int killer_index)
{
    if (RESET_KILL(killer))
        return;

    // Sometimes, if you pacify one twin near a staircase, they leave
    // in the same turn. Convert, in those instances. The fellow_slime check
    // is intended to cover the slimify case, we don't want to pacify the other
    // if a slimified twin dies.
    if (twin->neutral()
        && !twin->has_ench(ENCH_FRENZIED)
        && !is_fellow_slime(*twin))
    {
        elven_twins_pacify(twin);
        return;
    }

    monster* mons = mons_find_elven_twin_of(twin);

    if (!mons)
        return;

    // Don't consider already neutralised monsters.
    if (mons->good_neutral())
        return;

    // Okay, let them climb stairs now.
    mons->props[CAN_CLIMB_KEY] = true;
    if (is_fellow_slime(*twin))
        mons->props[SPEECH_PREFIX_KEY] = "twin_slimified";
    else if (!in_transit)
        mons->props[SPEECH_PREFIX_KEY] = "twin_died";
    else
        mons->props[SPEECH_PREFIX_KEY] = "twin_banished";

    // If you've stabbed one of them, the other one is likely asleep still.
    if (mons->asleep())
        behaviour_event(mons, ME_DISTURB, 0, mons->pos());

    // If the other didn't have a foe yet, it sure does now!
    _maybe_set_monster_foe(*mons, killer_index);

    // Will generate strings such as 'Duvessa_Duvessa_dies' or, alternately
    // 'Dowan_Dowan_dies', but as neither will match, these can safely be
    // ignored.
    string key = mons->name(DESC_THE, true) + "_"
                 + twin->name(DESC_THE, true) + "_dies_";

    if (you.see_cell(mons->pos()))
    {
        if (!mons->visible_to(&you))
            key += "invisible_";
    }
    else
        key += "distance_";

    bool i_killed = ((killer == KILL_MON || killer == KILL_MON_MISSILE)
                      && mons->mindex() == killer_index);

    if (i_killed)
    {
        key += "bytwin_";
        mons->props[SPEECH_PREFIX_KEY] = "twin_ikilled";
    }

    // Drop the final '_'.
    key.erase(key.length() - 1);

    string death_message = getSpeakString(key);

    if (!death_message.empty() && !invalid_msg(*mons, death_message))
    {
        // Check if they can speak or not: they may have been polymorphed.
        if (you.see_cell(mons->pos()) && mons->can_speak())
            mons_speaks_msg(mons, death_message, MSGCH_TALK, silenced(you.pos()));
        else if (mons->can_speak())
            mpr(death_message);
    }

    // Upgrade the spellbook here, as elven_twin_energize
    // may not be called due to lack of visibility.
    if (mons_is_mons_class(mons, MONS_DOWAN)
                                        && !(mons->flags & MF_POLYMORPHED))
    {
        // Don't mess with Dowan's spells if he's been polymorphed: most
        // possible forms have no spells, and the few that do (e.g. boggart)
        // have way more fun spells than this. If this ever changes, the
        // following code would need to be rewritten, as it'll crash.
        // TODO: this is a fairly brittle way of upgrading Dowan...
        ASSERT(mons->spells.size() >= 5);
        mons->spells[0].spell = SPELL_STONE_ARROW;
        mons->spells[1].spell = SPELL_THROW_ICICLE;
        mons->spells[3].spell = SPELL_BLINK;
        // Nothing with 6.

        // Indicate that he has an updated spellbook.
        mons->props[CUSTOM_SPELLS_KEY] = true;
    }

    // Finally give them new energy
    if (mons->can_see(you) && !mons->has_ench(ENCH_FRENZIED))
        elven_twin_energize(mons);
    else
        mons->props[ELVEN_ENERGIZE_KEY] = true;
}

void elven_twin_energize(monster* mons)
{
    if (mons_is_mons_class(mons, MONS_DUVESSA))
    {
        if (mons->go_berserk(true))
        {
            // only mark Duvessa energized once she's successfully gone berserk
            mons->props[ELVEN_IS_ENERGIZED_KEY] = true;
            mon_enchant zerk = mons->get_ench(ENCH_BERSERK);
            zerk.duration = INFINITE_DURATION;
            mons->update_ench(zerk);
        }
        else
            mons->props[ELVEN_ENERGIZE_KEY] = true;
    }
    else
    {
        ASSERT(mons_is_mons_class(mons, MONS_DOWAN));
        if (mons->observable())
            simple_monster_message(*mons, " seems to find hidden reserves of power!");

        mons->add_ench(mon_enchant(ENCH_HASTE, 1, mons, INFINITE_DURATION));
        mons->props[ELVEN_IS_ENERGIZED_KEY] = true;
    }
}

/**
 * Pacification effects for Dowan and Duvessa.
 *
 * As twins, pacifying one pacifies the other.
 *
 * @param twin    The original monster pacified.
**/
void elven_twins_pacify(monster* twin)
{
    monster* mons = mons_find_elven_twin_of(twin);

    if (!mons)
        return;

    // Don't consider already neutralised monsters.
    if (mons->neutral())
        return;

    simple_monster_message(*mons, " likewise turns neutral.");

    record_monster_defeat(mons, KILL_PACIFIED);
    mons_pacify(*mons, ATT_NEUTRAL);
}

/**
 * Unpacification effects for Dowan and Duvessa.
 *
 * If they are both pacified and you attack one, the other will not remain
 * neutral. This is both for flavour (they do things together), and
 * functionality (so Dowan does not begin beating on Duvessa, etc).
 *
 * @param twin    The monster attacked.
**/
void elven_twins_unpacify(monster* twin)
{
    monster* mons = mons_find_elven_twin_of(twin);

    if (!mons)
        return;

    // Don't consider already un-neutralised monsters or slimified twins.
    if (!mons->neutral() || mons->has_ench(ENCH_FRENZIED)
        || is_fellow_slime(*mons))
    {
        return;
    }

    simple_monster_message(*mons, " gets angry again!");

    behaviour_event(mons, ME_WHACK, &you, you.pos(), false);
}

bool mons_felid_can_revive(const monster* mons)
{
    return !mons->props.exists(FELID_REVIVES_KEY)
           || mons->props[FELID_REVIVES_KEY].get_byte() < 2;
}

void mons_felid_revive(monster* mons)
{
    // FIXME: this should be a fineff like bennu_revive_fineff. But that
    // is tricky because the original actor will be dead (and not carrying
    // its items) by the time the fineff fires.

    // Mostly adapted from bring_to_safety()
    coord_def revive_place;
    int tries = 10000;
    while (tries > 0) // Don't try too hard.
    {
        revive_place.x = random2(GXM);
        revive_place.y = random2(GYM);
        if (!in_bounds(revive_place)
            || env.grid(revive_place) != DNGN_FLOOR
            || cloud_at(revive_place)
            || monster_at(revive_place)
            || env.pgrid(revive_place) & FPROP_NO_TELE_INTO
            || grid_distance(revive_place, mons->pos()) < 9)
        {
            tries--;
            continue;
        }
        else
            break;
    }
    if (tries == 0)
        return;

    monster_type type = mons_is_mons_class(mons, MONS_NATASHA) ? MONS_NATASHA
                                                               : mons->type;
    const int revives = (mons->props.exists(FELID_REVIVES_KEY))
                        ? mons->props[FELID_REVIVES_KEY].get_byte() + 1
                        : 1;

    monster *newmons =
        create_monster(
            mgen_data(type, (mons->has_ench(ENCH_CHARM)
                             || mons->has_ench(ENCH_FRENZIED) ? BEH_HOSTILE
                             : SAME_ATTITUDE(mons)), revive_place, mons->foe));

    if (newmons)
    {
        for (mon_inv_iterator ii(*mons); ii; ++ii)
        {
            give_specific_item(newmons, *ii);
            destroy_item(ii->index());
        }

        newmons->props[FELID_REVIVES_KEY].get_byte() = revives;
    }
}

bool mons_bennu_can_revive(const monster* mons)
{
    return !mons->props.exists(BENNU_REVIVES_KEY)
           || mons->props[BENNU_REVIVES_KEY].get_byte() < 1;
}
