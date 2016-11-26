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
#include "bloodspatter.h"
#include "butcher.h"
#include "cloud.h"
#include "cluautil.h"
#include "colour.h"
#include "coordit.h"
#include "dactions.h"
#include "database.h"
#include "delay.h"
#include "describe.h"
#include "dgn-overview.h"
#include "english.h"
#include "env.h"
#include "fineff.h"
#include "food.h"
#include "godabil.h"
#include "godblessing.h"
#include "godcompanions.h"
#include "godconduct.h"
#include "godpassive.h" // passive_t::bless_followers, share_exp, convert_orcs, find_ieoh_jian_manifested_weapons
#include "hints.h"
#include "hiscores.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "kills.h"
#include "libutil.h"
#include "mapdef.h"
#include "mapmark.h"
#include "message.h"
#include "mon-abil.h"
#include "mon-behv.h"
#include "mon-gear.h"
#include "mon-place.h"
#include "mon-poly.h"
#include "mon-speak.h"
#include "mon-tentacle.h"
#include "mutation.h"
#include "nearby-danger.h"
#include "notes.h"
#include "output.h"
#include "religion.h"
#include "rot.h"
#include "spl-damage.h"
#include "spl-goditem.h"
#include "spl-miscast.h"
#include "spl-summoning.h"
#include "sprint.h" // SPRINT_MULTIPLIER
#include "state.h"
#include "stepdown.h"
#include "stringutil.h"
#include "target.h"
#include "teleport.h" // random_near_space
#include "terrain.h"
#include "timed_effects.h"
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

    if (mons_genus(mtype) == MONS_DRACONIAN
        || mons_genus(mtype) == MONS_DEMONSPAWN)
    {
        if (mons.type == MONS_TIAMAT)
            corpse_class = MONS_DRACONIAN;
        else
            corpse_class = draco_or_demonspawn_subspecies(mons);
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
    if (mons.props.exists("old_heads"))
        corpse.props[CORPSE_HEADS] = short(mons.props["old_heads"].get_int());
    COMPILE_CHECK(sizeof(mid_t) == sizeof(int));
    corpse.props[MONSTER_MID]      = int(mons.mid);

    monster_info minfo(corpse_class);
    int col = int(minfo.colour());
    if (col == COLOUR_UNDEF)
    {
        // XXX hack to avoid crashing in wiz mode.
        if (mons_is_ghost_demon(mons.type) && !mons.ghost.get())
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

    // 0 mid indicates this is a dummy monster, such as for kiku corpse drop
    if (mons_genus(mons.type) == MONS_ORC && mons.mid != 0)
    {
        auto &saved_mon = corpse.props[ORC_CORPSE_KEY].get_monster();
        saved_mon = mons;

        // Ensure that saved_mon is alive, lest it be cleared on marshall.
        if (saved_mon.max_hit_points <= 0)
            saved_mon.max_hit_points = 1;
        saved_mon.hit_points = saved_mon.max_hit_points;
    }

    return true;
}

static bool _explode_corpse(item_def& corpse, const coord_def& where)
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

    const int max_chunks = max_corpse_chunks(corpse.mon_type);
    const int nchunks = stepdown_value(1 + random2(max_chunks), 4, 4, 12, 12);
    if (corpse.base_type != OBJ_GOLD)
        blood_spray(where, corpse.mon_type, nchunks * 3); // spray some blood

    // Don't let the player evade food conducts by using OOD (!) or /disint
    // Spray blood, but no chunks. (The mighty hand of your God squashes them
    // in mid-flight...!)
    if (is_forbidden_food(corpse))
        return true;

    // turn the corpse into chunks
    if (corpse.base_type != OBJ_GOLD)
    {
        corpse.base_type = OBJ_FOOD;
        corpse.sub_type  = FOOD_CHUNK;
        if (is_bad_food(corpse))
            corpse.flags |= ISFLAG_DROPPED;
    }

    const int total_gold = corpse.quantity;

    // spray chunks everywhere!
    for (int ntries = 0, chunks_made = 0;
         chunks_made < nchunks && ntries < 10000; ++ntries)
    {
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

        ++chunks_made;

        dprf("Success");

        if (corpse.base_type == OBJ_GOLD)
            corpse.quantity = div_rand_round(total_gold, nchunks);
        if (corpse.quantity)
            copy_item_to_grid(corpse, cp);
    }

    return true;
}

static int _calc_monster_experience(monster* victim, killer_type killer,
                                    int killer_index)
{
    const int experience = exper_value(*victim);

    if (!experience || !MON_KILL(killer) || invalid_monster_index(killer_index))
        return 0;

    monster* mon = &menv[killer_index];
    if (!mon->alive() || !mons_gives_xp(*victim, *mon))
        return 0;

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
        if (!have_passive(passive_t::bless_followers) || !one_chance_in(3))
            return;

        // Randomly bless the follower who gained experience.
        if (random2(you.piety) >= piety_breakpoint(2))
            bless_follower(mon);
    }
}

static void _beogh_spread_experience(int exp)
{
    int total_hd = 0;

    for (monster_near_iterator mi(&you); mi; ++mi)
    {
        if (is_orcish_follower(**mi))
            total_hd += mi->get_experience_level();
    }

    if (total_hd <= 0)
        return;

    for (monster_near_iterator mi(&you); mi; ++mi)
        if (is_orcish_follower(**mi))
        {
            _give_monster_experience(exp * mi->get_experience_level() / total_hd,
                                         mi->mindex());
        }
}

static int _calc_player_experience(const monster* mons)
{
    int experience = exper_value(*mons);
    if (!experience)
        return 0;

    const bool already_got_half_xp = testbits(mons->flags, MF_PACIFIED);
    const int half_xp = (experience + 1) / 2;

    if (!mons->damage_total)
    {
        mprf(MSGCH_WARN, "Error, exp for monster with no damage: %s",
             mons->name(DESC_PLAIN, true).c_str());
        return 0;
    }

    experience = (experience * mons->damage_friendly / mons->damage_total
                  + 1) / 2;
    ASSERT(mons->damage_friendly <= 2 * mons->damage_total);

    // Note: This doesn't happen currently since monsters with
    //       MF_PACIFIED have always gone through pacification,
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
    delta.mon_kill_exp += exp_gain;

    you.global_info += delta;
    you.global_info.assert_validity();

    curr_PlaceInfo += delta;
    curr_PlaceInfo.assert_validity();

    // Give a message for monsters dying out of sight.
    if (exp_gain > 0 && !was_visible)
        mpr("You feel a bit more experienced.");

    if (kc == KC_YOU && have_passive(passive_t::share_exp))
        _beogh_spread_experience(experience / 2);
}

static void _give_experience(int player_exp, int monster_exp,
                             killer_type killer, int killer_index,
                             bool pet_kill, bool was_visible)
{
    _give_player_experience(player_exp, killer, pet_kill, was_visible);
    _give_monster_experience(monster_exp, killer_index);
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
    const int dur = corpse.quantity * 2;
    if (dur > you.duration[DUR_GOZAG_GOLD_AURA])
        you.set_duration(DUR_GOZAG_GOLD_AURA, dur);

    // In sprint, increase the amount of gold from corpses (but not
    // the gold aura duration!)
    if (crawl_state.game_is_sprint())
        corpse.quantity *= SPRINT_MULTIPLIER;

    const int chance = you.props[GOZAG_GOLD_AURA_KEY].get_int();
    if (!x_chance_in_y(chance, chance + 9))
        ++you.props[GOZAG_GOLD_AURA_KEY].get_int();
}

static void _create_monster_hide(const item_def &corpse, bool silent)
{
    const monster_type mtyp = corpse.mon_type;
    const armour_type type = hide_for_monster(mons_species(mtyp));
    ASSERT(type != NUM_ARMOURS);

    int o = items(false, OBJ_ARMOUR, type, 0);
    squash_plusses(o);

    if (o == NON_ITEM)
        return;
    item_def& item = mitm[o];

    do_uncurse_item(item);

    const monster_type montype =
        static_cast<monster_type>(corpse.orig_monnum);
    if (!invalid_monster_type(montype) && mons_is_unique(montype))
    {
        item.inscription = mons_type_name(montype, DESC_PLAIN);
        if (montype == MONS_XTAHUA || montype == MONS_SNORG)
            item.plus += random_range(2, 4); // a little nice bonus
    }

    const coord_def pos = item_pos(corpse);
    if (pos.origin())
    {
        set_ident_flags(item, ISFLAG_IDENT_MASK);
        return;
    }

    move_item_to_grid(&o, pos);
    if (you.see_cell(pos) && !silent)
    {
        // XXX: tweak for uniques/named monsters, somehow?
        mprf("%s %s intact enough to wear.",
             item.name(DESC_THE).c_str(),
             mons_genus(mtyp) == MONS_DRAGON ? "are"  // scales are
                                             : "is"); // hide is
                                                      // XXX: refactor
    }

    // after messaging, for better results
    set_ident_flags(item, ISFLAG_IDENT_MASK);
}

static void _maybe_drop_monster_hide(const item_def &corpse, bool silent)
{
    if (mons_class_leaves_hide(corpse.mon_type) && !one_chance_in(3))
        _create_monster_hide(corpse, silent);
}

/**
 * Create this monster's corpse in mitm at its position.
 *
 * @param mons the monster to corpsify
 * @param silent whether to suppress all messages
 * @param force whether to always make a corpse (no 50% chance not to make a
                corpse, no goldification, no hides -- being summoned etc. still
  *             matters, though)
 * @returns a pointer to an item; it may be null, if the monster can't leave a
 *          corpse or if the 50% chance is rolled; it may be gold, if the player
 *          worships Gozag, or it may be the corpse.
 */
item_def* place_monster_corpse(const monster& mons, bool silent, bool force)
{
    if (mons.is_summoned()
        || mons.flags & (MF_BANISHED | MF_HARD_RESET))
    {
        return nullptr;
    }

    // Under Gozag, monsters turn into gold on death.
    // Temporary Tukima's Dance weapons stay as weapons (no free gold),
    // permanent dancing weapons turn to gold like other monsters.
    bool goldify = have_passive(passive_t::goldify_corpses)
                   && mons_gives_xp(mons, you)
                   && !force;

    const bool no_coinflip =
        mons.props.exists("always_corpse")
        || force
        || goldify
        || mons_class_flag(mons.type, M_ALWAYS_CORPSE)
        || mons_is_demonspawn(mons.type)
           && mons_class_flag(draco_or_demonspawn_subspecies(mons),
                              M_ALWAYS_CORPSE);

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
    if (feat_is_solid(grd(mons.pos())) && !force)
        return nullptr;

    // If we were told not to leave a corpse, don't.
    if (mons.props.exists(NEVER_CORPSE_KEY))
        return nullptr;

    int o = get_mitm_slot();

    if (o == NON_ITEM)
        return nullptr;

    item_def& corpse(mitm[o]);
    if (goldify)
    {
        _gold_pile(corpse, mons_species(mons.type));
        // If gold would be destroyed, give it directly to the player instead.
        if (feat_eliminates_items(grd(mons.pos())))
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
        // We already have a spray of chunks.
        item_was_destroyed(corpse);
        destroy_item(o);
        return nullptr;
    }

    if (in_bounds(mons.pos()))
        move_item_to_grid(&o, mons.pos(), !mons.swimming());

    if (o == NON_ITEM)
        return nullptr;

    if (you.see_cell(mons.pos()) && !silent && !goldify)
        hints_dissection_reminder();

    return &mitm[o];
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
           killer == KILL_ENSLAVED ? "enslaved" :
           killer == KILL_SLIMIFIED ? "slimified" : "killed";
}

void record_monster_defeat(const monster* mons, killer_type killer)
{
    if (crawl_state.game_is_arena())
        return;
    if (killer == KILL_RESET || killer == KILL_DISMISSED)
        return;
    if (mons->has_ench(ENCH_FAKE_ABJURATION) || mons->is_summoned())
        return;
    if (mons->is_named() && mons->friendly()
        && !mons_is_hepliaklqana_ancestor(mons->type))
    {
        take_note(Note(NOTE_ALLY_DEATH, 0, 0, mons->mname));
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
    // ditto hep ancestors (sigh)
    if (!invalid_monster_index(killer)
        && (menv[killer].type == MONS_SPECTRAL_WEAPON
            || mons_is_hepliaklqana_ancestor(menv[killer].type))
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
// your holy or natural allies.
static bool _ely_protect_ally(monster* mons, killer_type killer)
{
    ASSERT(mons); // XXX: change to monster &mons
    if (!have_passive(passive_t::protect_ally))
        return false;

    if (!MON_KILL(killer) && !YOU_KILL(killer))
        return false;

    if ( mons->holiness() & ~(MH_HOLY | MH_NATURAL)
        || !mons->friendly()
        || !you.can_see(*mons) // for simplicity
        || !one_chance_in(20))
    {
        return false;
    }

    mons->hit_points = 1;

    const string msg = " protects " + mons->name(DESC_THE) + " from harm!";
    simple_god_message(msg.c_str());

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

        if (!you.see_cell(mons->pos()))
            return false;
    }
    else if (!YOU_KILL(killer))
        return false;

    dprf("monster hp: %d, max hp: %d", mons->hit_points, mons->max_hit_points);

    mons->hit_points = min(1 + random2(ely_penance/3), mons->max_hit_points);

    dprf("new hp: %d, ely penance: %d", mons->hit_points, ely_penance);

    const string msg = make_stringf("%s heals %s%s",
             god_name(god, false).c_str(),
             mons->name(DESC_THE).c_str(),
             mons->hit_points * 2 <= mons->max_hit_points ? "." : "!");

    god_speaks(god, msg.c_str());
    dec_penance(god, 1);

    return true;
}

static bool _yred_enslave_soul(monster* mons, killer_type killer)
{
    if (you_worship(GOD_YREDELEMNUL) && mons_enslaved_body_and_soul(*mons)
        && you.see_cell(mons->pos()) && killer != KILL_RESET
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
        beogh_convert_orc(&mons, MON_KILL(killer) ? conv_t::DEATHBED_FOLLOWER :
                                                    conv_t::DEATHBED);
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
        || !you.see_cell(mons.pos()) || mons_is_god_gift(mons))
    {
        return false;
    }

    if (YOU_KILL(killer))
        return _beogh_forcibly_convert_orc(mons, killer);

    if (MON_KILL(killer) && !invalid_monster_index(killer_index))
    {
        const monster* responsible_monster = &menv[killer_index];
        if (is_follower(*responsible_monster) && !one_chance_in(3))
            return _beogh_forcibly_convert_orc(mons, killer);
    }

    return false;
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
    if (lost_soul_revive(mons, killer))
        return true;

    // Yredelemnul special.
    if (_yred_enslave_soul(mons, killer))
        return true;

    // Beogh special.
    if (_beogh_maybe_convert_orc(*mons, killer, killer_index))
        return true;

    if (mons->hit_points < -25 || mons->hit_points < -mons->max_hit_points)
        return false;

    // Elyvilon specials.
    if (_ely_protect_ally(mons, killer))
        return true;
    if (_ely_heal_monster(mons, killer, killer_index))
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
        god_speaks(GOD_JIYVA, "With an infernal noise, the power ruling this "
                   "place vanishes!");
    }
}

void fire_monster_death_event(monster* mons,
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
            return 3;
        case MONS_MUMMY_PRIEST:
            return 8;
        case MONS_GREATER_MUMMY:
            return 11;
        case MONS_KHUFU:
            return 15;
        default:
            return 0;
    }
}

static void _mummy_curse(monster* mons, int pow, killer_type killer, int index)
{
    if (pow <= 0)
        return;

    switch (killer)
    {
        // Mummy killed by trap or something other than the player or
        // another monster, so no curse.
        case KILL_MISC:
        case KILL_RESET:
        case KILL_DISMISSED:
        // Mummy sent to the Abyss wasn't actually killed, so no curse.
        case KILL_BANISHED:
            return;

        default:
            break;
    }

    actor* target;

    if (YOU_KILL(killer))
        target = &you;
    // Killed by a Zot trap, a god, etc, or suicide.
    else if (invalid_monster_index(index) || index == mons->mindex())
        return;
    else
        target = &menv[index];

    // Mummy was killed by a ballistomycete spore or ball lightning?
    if (!target->alive())
        return;

    // Mummies are smart enough not to waste curses on summons or allies.
    if (target->is_monster() && target->as_monster()->friendly()
        && !crawl_state.game_is_arena())
    {
        target = &you;
    }

    // Stepped from time?
    if (!in_bounds(target->pos()))
        return;

    if (target->is_player())
        mprf(MSGCH_MONSTER_SPELL, "You feel extremely nervous for a moment...");
    else if (you.can_see(*target))
    {
        mprf(MSGCH_MONSTER_SPELL, "A malignant aura surrounds %s.",
             target->name(DESC_THE).c_str());
    }
    const string cause = make_stringf("%s death curse",
                            apostrophise(mons->name(DESC_A)).c_str());
    MiscastEffect(target, mons, MUMMY_MISCAST, SPTYP_NECROMANCY,
                  pow, random2avg(88, 3), cause.c_str());
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

static bool _ballisto_at(const coord_def & target)
{
    monster* mons = monster_at(target);
    return mons && mons->type == MONS_BALLISTOMYCETE
           && mons->alive();
}

static bool _mold_connected(const coord_def & target)
{
    return is_moldy(target) || _ballisto_at(target);
}

// If 'monster' is a ballistomycete or spore, activate some number of
// ballistomycetes on the level.
static void _activate_ballistomycetes(monster* mons, const coord_def& origin,
                                      bool player_kill)
{
    if (!mons || mons->is_summoned()
              || mons->mons_species() != MONS_BALLISTOMYCETE
                 && mons->type != MONS_BALLISTOMYCETE_SPORE)
    {
        return;
    }

    // If a spore or inactive ballisto died we will only activate one
    // other ballisto. If it was an active ballisto we will distribute
    // its count to others on the level.
    int activation_count = 1;
    if (mons->type == MONS_BALLISTOMYCETE)
        activation_count += mons->ballisto_activity;
    if (mons->type == MONS_HYPERACTIVE_BALLISTOMYCETE)
        activation_count = 0;

    int non_activable_count = 0;
    int ballisto_count = 0;

    for (monster_iterator mi; mi; ++mi)
    {
        if (mi->mindex() != mons->mindex() && mi->alive())
        {
            if (mi->type == MONS_BALLISTOMYCETE)
                ballisto_count++;
            else if (mi->type == MONS_BALLISTOMYCETE_SPORE
                     || mi->type == MONS_HYPERACTIVE_BALLISTOMYCETE)
            {
                non_activable_count++;
            }
        }
    }

    bool exhaustive = true;
    bool (*valid_target)(const coord_def &) = _ballisto_at;
    bool (*connecting_square) (const coord_def &) = _mold_connected;

    set<position_node> visited;
    vector<set<position_node>::iterator > candidates;

    _search_dungeon(origin, valid_target, connecting_square, visited,
                    candidates, exhaustive);

    if (candidates.empty())
    {
        if (non_activable_count == 0
            && ballisto_count == 0
            && mons->attitude == ATT_HOSTILE)
        {
            if (player_kill)
                mpr("The fungal colony is destroyed.");

            // Get rid of the mold, so it'll be more useful when new fungi
            // spawn.
            for (rectangle_iterator ri(1); ri; ++ri)
                remove_mold(*ri);
        }

        return;
    }

    // A (very) soft cap on colony growth, no activations if there are
    // already a lot of ballistos on level.
    if (candidates.size() > 25)
        return;

    shuffle_array(candidates);

    int index = 0;

    for (int i = 0; i < activation_count; ++i)
    {
        index = i % candidates.size();

        monster* spawner = monster_at(candidates[index]->pos);

        // This may be the players position, in which case we don't
        // have to mess with spore production on anything
        if (spawner)
        {
            spawner->ballisto_activity++;

            // Change color and start the spore production timer if we
            // are moving from 0 to 1.
            if (spawner->ballisto_activity == 1)
            {
                spawner->colour = LIGHTMAGENTA;
                // Reset the spore production timer.
                spawner->del_ench(ENCH_SPORE_PRODUCTION, false);
                spawner->add_ench(ENCH_SPORE_PRODUCTION);
            }
        }

        const position_node* thread = &(*candidates[index]);
        while (thread)
        {
            if (!one_chance_in(3))
                env.pgrid(thread->pos) |= FPROP_GLOW_MOLD;

            thread = thread->last;
        }
        env.level_state |= LSTATE_GLOW_MOLD;
    }
}

static void _setup_base_explosion(bolt & beam, const monster& origin)
{
    beam.is_tracer    = false;
    beam.is_explosion = true;
    beam.source_id    = origin.mid;
    beam.glyph        = dchar_glyph(DCHAR_FIRED_BURST);
    beam.source       = origin.pos();
    beam.source_name  = origin.base_name(DESC_BASENAME, true);
    beam.target       = origin.pos();
    beam.explode_noise_msg = "You hear an explosion!";

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
    beam.damage    = dice_def(3, 5 + origin.get_hit_dice() * 5 / 4);
    beam.name      = "blast of lightning";
    beam.explode_noise_msg = "You hear a clap of thunder!";
    beam.colour    = LIGHTCYAN;
    beam.ex_size   = x_chance_in_y(origin.get_hit_dice(), 24) ? 3 : 2;
    // Don't credit the player for ally-summoned ball lightning explosions.
    if (origin.summoner && origin.summoner != MID_PLAYER)
        beam.thrower = KILL_MON;
}

static void _setup_prism_explosion(bolt& beam, const monster& origin)
{
    _setup_base_explosion(beam, origin);
    beam.flavour = BEAM_MMISSILE;
    beam.damage  = (origin.prism_charge == 2 ?
                        dice_def(3, 6 + origin.get_hit_dice() * 7 / 4)
                        : dice_def(2, 6 + origin.get_hit_dice() * 7 / 4));
    beam.name    = "blast of energy";
    beam.colour  = MAGENTA;
    beam.ex_size = origin.prism_charge;
}

static void _setup_bennu_explosion(bolt& beam, const monster& origin)
{
    _setup_base_explosion(beam, origin);
    beam.flavour = BEAM_NEG;
    beam.damage  = dice_def(3, 5 + origin.get_hit_dice() * 5 / 4);
    beam.name    = "pyre of ghostly fire";
    beam.explode_noise_msg = "You hear an otherworldly crackling!";
    beam.colour  = CYAN;
    beam.ex_size = 2;
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
    beam.origin_spell = SPELL_INNER_FLAME;
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
    const char* sanct_msg = nullptr;
    actor* agent = mons;

    if (type == MONS_BALLISTOMYCETE_SPORE)
    {
        setup_spore_explosion(beam, *mons);
        sanct_msg    = "By Zin's power, the ballistomycete spore's explosion is "
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
    else if (type == MONS_BENNU)
    {
        _setup_bennu_explosion(beam, *mons);
        sanct_msg = "By Zin's power, the bennu's fires are quelled.";
    }
    else if (mons->has_ench(ENCH_INNER_FLAME))
    {
        mon_enchant i_f = mons->get_ench(ENCH_INNER_FLAME);
        ASSERT(i_f.ench == ENCH_INNER_FLAME);
        agent = actor_by_mid(i_f.source);
        _setup_inner_flame_explosion(beam, *mons, agent);
        // This might need to change if monsters ever get the ability to cast
        // Inner Flame...
        if (agent && agent->is_player())
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
        if (type == MONS_BENNU)
        {
            if (YOU_KILL(killer))
                beam.aux_source = "ignited by themself";
            else if (pet_kill)
                beam.aux_source = "ignited by their pet";
        }
        else
        {
            if (YOU_KILL(killer))
                beam.aux_source = "set off by themself";
            else if (pet_kill)
                beam.aux_source = "set off by their pet";
        }
    }

    bool saw = false;
    if (you.can_see(*mons))
    {
        saw = true;
        viewwindow();
        if (is_sanctuary(mons->pos()))
            mprf(MSGCH_GOD, "%s", sanct_msg);
        else if (type == MONS_BENNU)
            mprf(MSGCH_MONSTER_DAMAGE, MDAM_DEAD, "%s blazes out!",
                 mons->full_name(DESC_THE).c_str());
        else
            mprf(MSGCH_MONSTER_DAMAGE, MDAM_DEAD, "%s explodes!",
                 mons->full_name(DESC_THE).c_str());
    }

    if (is_sanctuary(mons->pos()))
        return false;

    // Explosion side-effects.
    if (type == MONS_LURKING_HORROR)
        torment(mons, TORMENT_LURKING_HORROR, mons->pos());
    else if (mons->has_ench(ENCH_INNER_FLAME))
    {
        for (adjacent_iterator ai(mons->pos(), false); ai; ++ai)
            if (!cell_is_solid(*ai) && !cloud_at(*ai) && !one_chance_in(5))
                place_cloud(CLOUD_FIRE, *ai, 10 + random2(10), agent);
    }

    // Detach monster from the grid first, so it doesn't get hit by
    // its own explosion. (GDL)
    // Unless it's a phoenix, where this isn't much of a concern.
    mgrd(mons->pos()) = NON_MONSTER;

    // The explosion might cause a monster to be placed where the bomb
    // used to be, so make sure that mgrd() doesn't get cleared a second
    // time (causing the new monster to become floating) when
    // mons->reset() is called.
    if (type == MONS_BALLISTOMYCETE_SPORE)
        mons->set_position(coord_def(0,0));

    // Exploding kills the monster a bit earlier than normal.
    mons->hit_points = -16;
    if (saw)
        viewwindow();

    // FIXME: show_more == you.see_cell(mons->pos())
    if (type == MONS_LURKING_HORROR)
    {
        targetter_los hitfunc(mons, LOS_SOLID);
        flash_view_delay(UA_MONSTER, DARKGRAY, 300, &hitfunc);
    }
    else
        beam.explode();

    _activate_ballistomycetes(mons, beam.target, YOU_KILL(beam.killer()));
    // Monster died in explosion, so don't re-attach it to the grid.
    return true;
}

static void _infestation_create_scarab(monster* mons)
{
    if (monster *scarab = create_monster(mgen_data(MONS_DEATH_SCARAB,
                                                   BEH_FRIENDLY, mons->pos(),
                                                   MHITYOU, MG_AUTOFOE)
                                         .set_summoned(&you, 0,
                                                       SPELL_INFESTATION),
                                         false))
    {
        scarab->add_ench(mon_enchant(ENCH_FAKE_ABJURATION, 6));

        if (you.see_cell(mons->pos()) || you.can_see(*scarab))
        {
            mprf("%s bursts from %s!", scarab->name(DESC_A, true).c_str(),
                                       mons->name(DESC_THE).c_str());
        }
        mons->flags |= MF_EXPLODE_KILL;
    }
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
        if (!mons_class_can_leave_corpse(mons_species(mons->type)))
            return;

        prefix = "'s corpse ";
    }

    string msg = summoned_poof_msg(mons) + "!";

    cloud_type cloud = CLOUD_NONE;
    if (msg.find("smoke") != string::npos)
        cloud = random_smoke_type();
    if (msg.find("steel") != string::npos)
        cloud = CLOUD_DUST;
    else if (msg.find("chaos") != string::npos)
        cloud = CLOUD_CHAOS;

    if (!silent)
        simple_monster_message(*mons, (prefix + msg).c_str());

    if (cloud != CLOUD_NONE)
        place_cloud(cloud, mons->pos(), 1 + random2(3), mons);
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
    case KILL_TIMEOUT:
        return "timeout";
#if TAG_MAJOR_VERSION == 34
    case KILL_UNSUMMONED:
        return "unsummoned";
#endif
    case KILL_PACIFIED:
        return "pacified";
    case KILL_ENSLAVED:
        return "enslaved";
    case KILL_SLIMIFIED:
        return "slimified";
    }
    die("invalid killer type");
}

/**
 * Make a spectral thing or simulacrum out of a dying/dead monster.
 *
 * @param mons       the monster that died
 * @param quiet      whether to print flavour messages
 * @param bound_soul whether the undead is from Bind Souls (true) or DChan
 */
static void _make_derived_undead(monster* mons, bool quiet, bool bound_soul)
{
    if (mons->holiness() & MH_NATURAL && mons_can_be_zombified(*mons))
    {
        // Use the original monster type as the zombified type here, to
        // get the proper stats from it.
        mgen_data mg(bound_soul ? MONS_SIMULACRUM : MONS_SPECTRAL_THING,
                     bound_soul ? SAME_ATTITUDE(mons) : BEH_FRIENDLY,
                     mons->pos(),
                     // XXX: is MHITYOU really correct here?
                     crawl_state.game_is_arena() ? MHITNOT : MHITYOU);
        // Simulacra aren't summons, and we want them to stick
        // around even after killing the necromancer.
        mg.set_summoned(bound_soul ? nullptr : &you,
                        0,
                        bound_soul ? SPELL_BIND_SOULS : SPELL_DEATH_CHANNEL,
                        bound_soul ?
                        GOD_NO_GOD : static_cast<god_type>(you.attribute[ATTR_DIVINE_DEATH_CHANNEL]));
        mg.set_base(mons->type);
        if (mons->mons_species() == MONS_HYDRA)
        {
            // No undead 0-headed hydras, sorry.
            if (mons->heads() == 0)
            {
                if (!quiet)
                {
                    mprf("A %s mist gathers momentarily, then fades.",
                         bound_soul ? "freezing" : "glowing");
                }
                return;
            }
            else
                mg.props[MGEN_NUM_HEADS] = mons->heads();
        }

        if (monster *undead = create_monster(mg))
        {
            if (!quiet)
            {
                mprf("A %s mist starts to gather...",
                     bound_soul ? "freezing" : "glowing");
            }

            // If the original monster has been levelled up, its HD might be
            // different from its class HD, in which case its HP should be
            // rerolled to match.
            if (undead->get_experience_level() != mons->get_experience_level())
            {
                undead->set_hit_dice(max(mons->get_experience_level(), 1));
                roll_zombie_hp(undead);
            }

            name_zombie(*undead, *mons);

            undead->add_ench(mon_enchant(ENCH_FAKE_ABJURATION, 6));
            if (bound_soul)
            {
                const auto agent = mons->get_ench(ENCH_BOUND_SOUL).agent();
                if (agent)
                {
                    mons_add_blame(undead,
                        "animated by " + agent->as_monster()->full_name(DESC_A));
                }
            }
        }
    }
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

static bool _mons_reaped(actor *killer, monster* victim)
{
    ASSERT(killer); // XXX: change to actor &killer
    ASSERT(victim); // XXX: change to monster &victim

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

    if (you.can_see(*victim))
        mprf("%s turns into a zombie!", victim->name(DESC_THE).c_str());
    else if (you.can_see(*zombie))
        mprf("%s appears out of thin air!", zombie->name(DESC_THE).c_str());

    player_angers_monster(zombie);

    return true;
}

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

static bool _god_will_bless_follower(monster* victim)
{
    return have_passive(passive_t::bless_followers)
           && random2(you.piety) >= piety_breakpoint(2)
           || have_passive(passive_t::bless_followers_vs_evil)
              && victim->evil()
              && random2(you.piety) >= piety_breakpoint(0);
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
static void _fire_kill_conducts(monster &mons, killer_type killer,
                                int killer_index, bool maybe_good_kill)
{
    const bool your_kill = killer == KILL_YOU ||
                           killer == KILL_YOU_CONF ||
                           killer == KILL_YOU_MISSILE;
    const bool pet_kill = _is_pet_kill(killer, killer_index);

    // Pretend the monster is already dead, so that make_god_gifts_disappear
    // (and similar) don't kill it twice.
    unwind_var<int> fake_hp(mons.hit_points, 0);

    // if you or your pets didn't do it, no one cares
    if (!your_kill && !pet_kill)
        return;

    // player gets credit for reflection kills, but not blame
    const bool blameworthy = god_hates_killing(you.religion, mons)
                             && killer_index != YOU_FAULTLESS;
    // if you can't get piety for it & your god won't give penance/-piety for
    // it, no one cares
    // XXX: this will break holy death curses if they're added back...
    // but tbh that shouldn't really be in conducts anyway
    if (!maybe_good_kill && !blameworthy)
        return;

    mon_holy_type holiness = mons.holiness();

    if (holiness == MH_DEMONIC)
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

    // Jiyva hates you killing slimes, but eyeballs
    // mutation can confuse without you meaning it.
    if (mons_is_slime(mons) && killer != KILL_YOU_CONF)
        did_kill_conduct(DID_KILL_SLIME, mons);

    if (mons.is_holy())
        did_kill_conduct(DID_KILL_HOLY, mons);

    // Fedhas shrooms cause confusion which leads to subsequent
    // confusion kills, sometimes of the player's own plants
    if (fedhas_protects(mons) && killer != KILL_YOU_CONF)
        did_kill_conduct(DID_KILL_PLANT, mons);

    // Cheibriados hates fast monsters.
    if (cheibriados_thinks_mons_is_fast(mons) && !mons.cannot_move())
        did_kill_conduct(DID_KILL_FAST, mons);

    // Dithmenos hates sources of fire.
    // (This is *after* the holy so that the right order of
    //  messages appears.)
    if (mons_is_fiery(mons))
        did_kill_conduct(DID_KILL_FIERY, mons);
}

item_def* monster_die(monster* mons, const actor *killer, bool silent,
                      bool wizard, bool fake)
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

    return monster_die(mons, ktype, kindex, silent, wizard, fake);
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
            const string message = "'s shape twists and changes as "
                                    + mons.pronoun(PRONOUN_SUBJECTIVE)
                                    + " dies.";
            simple_monster_message(mons, message.c_str());
        }

        return;
    }

    // Avoid "Sigmund returns to its original shape as it dies.".
    unwind_var<monster_type> mt(mons.type, orig);
    const int num = mons.mons_species() == MONS_HYDRA
                    ? mons.props["old_heads"].get_int()
                    : mons.number;
    unwind_var<unsigned int> number(mons.number, num);
    const string message = " returns to " +
                            mons.pronoun(PRONOUN_POSSESSIVE) +
                            " original shape as " +
                            mons.pronoun(PRONOUN_SUBJECTIVE) +
                            " dies.";
    simple_monster_message(mons, message.c_str());
}

/**
 * Kill off a monster.
 *
 * @param mons The monster to be killed
 * @param killer The method in which it was killed (XXX: these aren't properly
 *               documented/coded)
 * @param killer_index The mindex of the killer (TODO: always use an actor*)
 * @param silent whether to print any messages about the death
 * @param wizard various switches
 * @param fake   The death of the mount of a mounted monster (spriggan rider).
 * @returns a pointer to the created corpse, possibly null
 */
item_def* monster_die(monster* mons, killer_type killer,
                      int killer_index, bool silent, bool wizard, bool fake)
{
    if (invalid_monster(mons))
        return nullptr;

    const bool was_visible = you.can_see(*mons);

    // If a monster was banished to the Abyss and then killed there,
    // then its death wasn't a banishment.
    if (player_in_branch(BRANCH_ABYSS))
        mons->flags &= ~MF_BANISHED;

    const bool spectralised = testbits(mons->flags, MF_SPECTRALISED);

    if (!silent && !fake
        && _monster_avoided_death(mons, killer, killer_index))
    {
        mons->flags &= ~MF_EXPLODE_KILL;

        // revived by a lost soul?
        if (!spectralised && testbits(mons->flags, MF_SPECTRALISED))
            return place_monster_corpse(*mons, silent);
        return nullptr;
    }

    // If the monster was calling the tide, let go now.
    mons->del_ench(ENCH_TIDE);

    // Same for silencers.
    mons->del_ench(ENCH_SILENCE);

    // ... and liquefiers.
    mons->del_ench(ENCH_LIQUEFYING);

    // ... and wind-stillers.
    mons->del_ench(ENCH_STILL_WINDS, true);

    // Clean up any blood from the flayed effect
    if (mons->has_ench(ENCH_FLAYED))
        heal_flayed_effect(mons, true, true);

    crawl_state.inc_mon_acting(mons);

    ASSERT(!(YOU_KILL(killer) && crawl_state.game_is_arena()));

    if (mons->props.exists(MONSTER_DIES_LUA_KEY))
    {
        lua_stack_cleaner clean(dlua);

        dlua_chunk &chunk = mons->props[MONSTER_DIES_LUA_KEY];

        if (!chunk.load(dlua))
        {
            push_monster(dlua, mons);
            clua_pushcxxstring(dlua, _killer_type_name(killer));
            dlua.callfn(nullptr, 2, 0);
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

    you.remove_beholder(*mons);
    you.remove_fearmonger(mons);
    // Uniques leave notes and milestones, so this information is already leaked.
    remove_unique_annotation(mons);

    // Clear auto exclusion now the monster is killed - if we know about it.
    if (you.see_cell(mons->pos()) || wizard || mons_is_unique(mons->type))
        remove_auto_exclude(mons);

          int  duration      = 0;
    const bool summoned      = mons->is_summoned(&duration);
    const int monster_killed = mons->mindex();
    const bool hard_reset    = testbits(mons->flags, MF_HARD_RESET);
    const bool timeout       = killer == KILL_TIMEOUT;
    const bool fake_abjure   = mons->has_ench(ENCH_FAKE_ABJURATION);
    const bool gives_player_xp = mons_gives_xp(*mons, you);
    bool drop_items          = !hard_reset;
    const bool submerged     = mons->submerged();
    bool in_transit          = false;
    const bool was_banished  = (killer == KILL_BANISHED);
    const bool mons_reset    = (killer == KILL_RESET
                                || killer == KILL_DISMISSED);
    const bool leaves_corpse = !summoned && !fake_abjure && !timeout
                               && !mons_reset
                               && !mons_is_tentacle_segment(mons->type);
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
        bool arena = crawl_state.game_is_arena();
        mon_enchant ench = mons->get_ench(ENCH_CHARM);
        if (ench.who == KC_YOU || (!arena && ench.who == KC_FRIENDLY))
        {
            ASSERT(!arena);
            killer = KILL_YOU_CONF; // Well, it was confused in a sense... (jpeg)
        }
    }

    // Kills by the spectral weapon are considered as kills by the player
    // instead. Ditto Dithmenos shadow melee and shadow throw. 
    // Ditto Ieoh Jian Council weapons.
    if (MON_KILL(killer)
        && !invalid_monster_index(killer_index)
        && ((menv[killer_index].type == MONS_SPECTRAL_WEAPON
             && menv[killer_index].summoner == MID_PLAYER)
            || mons_is_player_shadow(menv[killer_index])
            || mons_is_ieoh_jian_weapon(menv[killer_index])))
    {
        killer_index = you.mindex();
    }

    // Set an appropriate killer; besides the cases in the preceding if,
    // this handles Dithmenos shadow spells, which look like they come from
    // you because the shadow's mid is MID_PLAYER.
    if (MON_KILL(killer) && killer_index == you.mindex())
        killer = (killer == KILL_MON_MISSILE) ? KILL_YOU_MISSILE : KILL_YOU;

    // Take notes and mark milestones.
    record_monster_defeat(mons, killer);

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
        else if (player_equip_unrand(UNRAND_BLOODLUST) && coinflip())
        {
            const int bonus = (2 + random2(4)) / 2;
            you.increase_duration(DUR_BERSERK, bonus);
            mpr("The necklace of Bloodlust glows a violent red.");
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

    if (mons->type == MONS_BALLISTOMYCETE_SPORE
        || mons->type == MONS_BALL_LIGHTNING
        || mons->type == MONS_LURKING_HORROR
        || (mons->type == MONS_FULMINANT_PRISM && mons->prism_charge > 0)
        || mons->type == MONS_BENNU
        || mons->has_ench(ENCH_INNER_FLAME))
    {
        did_death_message =
            _explode_monster(mons, killer, killer_index, pet_kill, wizard);
    }
    else if (mons->type == MONS_FULMINANT_PRISM && mons->prism_charge == 0)
    {
        if (!silent && !hard_reset && !was_banished)
        {
            simple_monster_message(*mons, " detonates feebly.",
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
            simple_monster_message(*mons, " dissipates!",
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
            simple_monster_message(*mons, " vapourises!",
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
            // Under Gozag, permanent dancing weapons get turned to gold.
            if (!summoned_it
                && (!have_passive(passive_t::goldify_corpses)
                    || mons->has_ench(ENCH_ABJ)))
            {
                simple_monster_message(*mons, " falls from the air.",
                                       MSGCH_MONSTER_DAMAGE, MDAM_DEAD);
                silent = true;
            }
            else
                killer = KILL_RESET;
        }

        if (was_banished && !summoned_it && !hard_reset
            && mons->has_ench(ENCH_ABJ))
        {
            if (is_unrandom_artefact(mitm[w_idx]))
                set_unique_item_status(mitm[w_idx], UNIQ_LOST_IN_ABYSS);

            destroy_item(w_idx);
        }
    }
    else if (mons->type == MONS_IEOH_JIAN_WEAPON)
    {
        bool reformed = false;
        bool was_active = mons->has_ench(ENCH_IEOH_JIAN_COMBAT_ACTIVE);
        if (killer == KILL_RESET)
        {
            if (you.can_see(*mons) && !silent && mons->weapon() && !mons->weapon()->props.exists(IEOH_JIAN_SLOT))
                mprf("%s bounces wildly and falls to the ground.", mons->weapon()->name(DESC_THE, false, true).c_str());

            // All slot indices are updated, so the age order is always respected.
            auto monsters = find_ieoh_jian_manifested_weapons(false);
            size_t i;

            for (i = 0; i != monsters.size(); i++)
                monsters[i]->weapon()->props[IEOH_JIAN_SLOT] = (int)i;

            if (you.weapon() && you.weapon()->props.exists(IEOH_JIAN_SLOT))
                you.weapon()->props[IEOH_JIAN_SLOT] = (int)i;
        }
        else
        {
            // If the kill wasn't divine, the weapon is immediately reformed in LOS.
            coord_def reform_location;
            random_near_space(mons, mons->pos(), reform_location, true, false, true);
            monster* new_mons = ieoh_jian_manifest_weapon_monster(reform_location, *(mons->weapon()));

            if (!new_mons)
                dprf("Failed to reform Ieoh Jian weapon");
            else
            {
                reformed = true;
                if (was_active)
                {
                    float invo_duration_factor = you.skill(SK_INVOCATIONS,1,false) / 15.0;
                    mon_enchant combat_active(ENCH_IEOH_JIAN_COMBAT_ACTIVE, 1, &you, IEOH_JIAN_ATTENTION_SPAN * (1 + invo_duration_factor));
                    new_mons->add_ench(combat_active);
                }
            }

        }

        if (!silent)
            check_place_cloud(CLOUD_DUST, mons->pos(), 2 + random2(4), mons, 5 + random2(15), -1);

        silent = true;

        int w_idx = mons->inv[MSLOT_WEAPON];
        if (mons->weapon() && (w_idx != NON_ITEM))
        {
            // Something went wrong with resummoning the player's weapon, so it must be dropped.
            if (killer == KILL_RESET && !mons->weapon()->props.exists(IEOH_JIAN_SLOT))
                mons->drop_item(MSLOT_WEAPON, false);
            else if (killer != KILL_RESET && !mons->weapon()->props.exists(IEOH_JIAN_SLOT) && !reformed)
                mons->drop_item(MSLOT_WEAPON, false);
            else
                destroy_item(w_idx);
        }

    }
    else if (mons->type == MONS_ELDRITCH_TENTACLE)
    {
        if (!silent && !mons_reset && !mons->has_ench(ENCH_SEVERED)
            && !was_banished)
        {
            if (you.can_see(*mons))
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
        if (!wizard && !mons_reset && !was_banished
            && !cell_is_solid(mons->pos()))
        {
            place_cloud(CLOUD_MAGIC_TRAIL, mons->pos(), 3 + random2(3), mons);
        }
        end_battlesphere(mons, true);
    }
    else if (mons->type == MONS_BRIAR_PATCH)
    {
        if (timeout && !silent)
            simple_monster_message(*mons, " crumbles away.");
    }
    else if (mons->type == MONS_SPECTRAL_WEAPON)
    {
        end_spectral_weapon(mons, true, killer == KILL_RESET);
        silent = true;
    }
    else if (mons->type == MONS_DROWNED_SOUL)
    {
        // Suppress death message if 'killed' by touching something
        if (mons->hit_points == -1000)
            silent = true;
    }
    else if (mons->type == MONS_SPRIGGAN_DRUID && !silent && !was_banished
             && !wizard && !mons_reset)
    {
        _druid_final_boon(mons);
    }
    else if (mons->type == MONS_ELEMENTAL_WELLSPRING
             && mons->mindex() == killer_index)
    {
        if (!silent)
            simple_monster_message(*mons, " exhausts itself and dries up.");
        silent = true;
    }

    const bool death_message = !silent && !did_death_message
                               && you.can_see(*mons);
    const bool exploded {mons->flags & MF_EXPLODE_KILL};
    bool anon = (killer_index == ANON_FRIENDLY_MONSTER);
    const mon_holy_type targ_holy = mons->holiness();

    // Adjust song of slaying bonus. Kills by relevant avatars are adjusted by
    // now to KILL_YOU and are counted.
    if (you.duration[DUR_SONG_OF_SLAYING]
        && killer == KILL_YOU
        && gives_player_xp)
    {
        const int sos_bonus = you.props[SONG_OF_SLAYING_KEY].get_int();
        if (sos_bonus <= 8) // cap at +9 slay
            you.props[SONG_OF_SLAYING_KEY] = sos_bonus + 1;
    }

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
                         mons->name(DESC_THE).c_str(),
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
                         mons->name(DESC_THE).c_str());
                }
                // If this monster would otherwise give xp but didn't because
                // it grants no reward or was neutral, give a message.
                if (!gives_player_xp
                    && mons_class_gives_xp(mons->type)
                    && !summoned
                    && !fake_abjure
                    && !mons->friendly())
                {
                    mpr("That felt strangely unrewarding.");
                }
            }

            // Killing triggers hints mode lesson.
            if (gives_player_xp)
                _hints_inspect_kill();

            _fire_kill_conducts(*mons, killer, killer_index, gives_player_xp);

            // Divine health and mana restoration doesn't happen when
            // killing born-friendly monsters.
            if (gives_player_xp
                && (have_passive(passive_t::restore_hp)
                    || have_passive(passive_t::mp_on_kill)
                    || have_passive(passive_t::restore_hp_mp_vs_evil)
                       && mons->evil())
                && !mons_is_object(mons->type)
                && !player_under_penance()
                && (you_worship(GOD_PAKELLAS)
                    || random2(you.piety) >= piety_breakpoint(0)))
            {
                int hp_heal = 0, mp_heal = 0;

                if (have_passive(passive_t::restore_hp))
                {
                    hp_heal = mons->get_experience_level()
                        + random2(mons->get_experience_level());
                }
                if (have_passive(passive_t::restore_hp_mp_vs_evil))
                {
                    hp_heal = random2(1 + 2 * mons->get_experience_level());
                    mp_heal = random2(2 + mons->get_experience_level() / 3);
                }

                if (have_passive(passive_t::mp_on_kill))
                {
                    switch (you.religion)
                    {
                    case GOD_PAKELLAS:
                        mp_heal = random2(2 + mons->get_experience_level() / 6);
                        break;
                    case GOD_VEHUMET:
                    default:
                        mp_heal = 1 + random2(mons->get_experience_level() / 2);
                        break;
                    }
                }

#if TAG_MAJOR_VERSION == 34
                if (you.species == SP_DJINNI)
                    hp_heal = max(hp_heal, mp_heal * 2), mp_heal = 0;
#endif
                if (hp_heal && you.hp < you.hp_max
                    && !you.duration[DUR_DEATHS_DOOR])
                {
                    canned_msg(MSG_GAIN_HEALTH);
                    inc_hp(hp_heal);
                }

                if (mp_heal && you.magic_points < you.max_magic_points)
                {
                    int tmp = min(you.max_magic_points - you.magic_points,
                                  mp_heal);
                    canned_msg(MSG_GAIN_MAGIC);
                    inc_mp(mp_heal);
                    mp_heal -= tmp;
                }

                // perhaps this should go to its own function
                if (mp_heal
                    && have_passive(passive_t::bottle_mp)
                    && !you_foodless_normally())
                {
                    simple_god_message(" collects the excess magic power.");
                    you.attribute[ATTR_PAKELLAS_EXTRA_MP] -= mp_heal;

                    if (you.attribute[ATTR_PAKELLAS_EXTRA_MP] <= 0
                        && (feat_has_solid_floor(grd(you.pos()))
                            || feat_is_watery(grd(you.pos()))
                               && species_likes_water(you.species)))
                    {
                        int thing_created = items(true, OBJ_POTIONS,
                                                  POT_MAGIC, 1, 0,
                                                  GOD_PAKELLAS);
                        if (thing_created != NON_ITEM)
                        {
                            move_item_to_grid(&thing_created, you.pos(), true);
                            mitm[thing_created].quantity = 1;
                            mitm[thing_created].flags |= ISFLAG_KNOW_TYPE;
                            // not a conventional gift, but use the same
                            // messaging
                            simple_god_message(" grants you a gift!");
                            you.attribute[ATTR_PAKELLAS_EXTRA_MP]
                                += POT_MAGIC_MP;
                        }
                    }
                }
            }

            if (gives_player_xp && you_worship(GOD_RU) && you.piety < 200
                && one_chance_in(2))
            {
                ASSERT(you.props.exists(RU_SACRIFICE_PROGRESS_KEY));
                int current_progress =
                        you.props[RU_SACRIFICE_PROGRESS_KEY].get_int();
                you.props[RU_SACRIFICE_PROGRESS_KEY] = current_progress + 1;
            }

            // Randomly bless a follower.
            if (gives_player_xp
                && !mons_is_object(mons->type)
                && _god_will_bless_follower(mons))
            {
                bless_follower();
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
                simple_monster_message(*mons, msg, MSGCH_MONSTER_DAMAGE,
                                       MDAM_DEAD);
            }

            if (crawl_state.game_is_arena())
                break;

            _fire_kill_conducts(*mons, killer, killer_index, gives_player_xp);

            // Kill conducts do not assess piety loss for friends
            // killed by other monsters.
            if (mons->friendly())
            {
                const bool sentient = mons_class_intel(mons->type) >= I_HUMAN;
                // plant HD aren't very meaningful. (fedhas hack)
                const int severity = mons->holiness() & MH_PLANT ?
                                     1 :
                                     1 + (mons->get_experience_level() / 4);

                did_god_conduct(sentient ? DID_SOULED_FRIEND_DIED
                                         : DID_FRIEND_DIED,
                                severity, true, mons);
            }

            // Trying to prevent summoning abuse here, so we're trying to
            // prevent summoned creatures from being done_good kills. Only
            // affects creatures which were friendly when summoned.
            if (!gives_player_xp
                || !pet_kill
                || !anon && invalid_monster_index(killer_index))
            {
                break;
            }

            monster* killer_mon = nullptr;
            if (!anon)
                killer_mon = &menv[killer_index];

            if (!invalid_monster_index(killer_index)
                && _god_will_bless_follower(mons))
            {
                // Randomly bless the follower who killed.
                bless_follower(killer_mon);
            }
            break;
        }

        // Monster killed by trap/inanimate thing/itself/poison not from you.
        case KILL_MISC:
        case KILL_MISCAST:
            if (death_message)
            {
                if (fake_abjure)
                {
                    // Sticks to Snakes
                    if (mons_genus(mons->type) == MONS_SNAKE)
                        simple_monster_message(*mons, " withers and dies!");
                    // Death Channel
                    else if (mons->type == MONS_SPECTRAL_THING)
                        simple_monster_message(*mons, " fades into mist!");
                    // Animate Skeleton/Animate Dead/Infestation
                    else if (mons->type == MONS_ZOMBIE
                             || mons->type == MONS_SKELETON
                             || mons->type == MONS_DEATH_SCARAB)
                    {
                        simple_monster_message(*mons, " crumbles into dust!");
                    }
                    else
                    {
                        string msg = " " + summoned_poof_msg(mons) + "!";
                        simple_monster_message(*mons, msg.c_str());
                    }
                }
                else
                {
                    const char* msg =
                        exploded                     ? " is blown up!" :
                        wounded_damaged(targ_holy)   ? " is destroyed!"
                                                     : " dies!";
                    simple_monster_message(*mons, msg, MSGCH_MONSTER_DAMAGE,
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
        {
            // Make sure Natasha gets her say even if she got polymorphed.
            const monster_type orig =
                mons_is_mons_class(mons, MONS_NATASHA) ? MONS_NATASHA
                                                       : mons->type;
            unwind_var<monster_type> mt(mons->type, orig);
            mons_speaks(mons);
        }
    }

    // None of these effects should trigger on illusory copies.
    if (!mons->is_illusion())
    {
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
        else if (mons_is_mons_class(mons, MONS_KIRKE)
                 && !in_transit
                 && !testbits(mons->flags, MF_WAS_NEUTRAL))
        {
            hogs_to_humans();
        }
        else if ((mons_is_mons_class(mons, MONS_NATASHA)
                  || mons_genus(mons->type) == MONS_FELID)
                 && !in_transit && !mons->pacified()
                 && mons_felid_can_revive(mons))
        {
            drop_items = false;

            // Like Boris, but her vault can't come back
            if (mons_is_mons_class(mons, MONS_NATASHA))
                you.unique_creatures.set(MONS_NATASHA, false);
            if (!mons_reset && !wizard)
                mons_felid_revive(mons);
        }
        else if (mons_is_mons_class(mons, MONS_PIKEL))
        {
            // His slaves don't care if he's dead or not, just whether or not
            // he goes away.
            pikel_band_neutralise();
        }
        else if (mons_is_elven_twin(mons))
            elven_twin_died(mons, in_transit, killer, killer_index);
        else if (mons->type == MONS_BENNU && !in_transit && !was_banished
                 && !mons_reset && !mons->pacified()
                 && (!summoned || duration > 0) && !wizard
                 && mons_bennu_can_revive(mons))
        {
            // All this information may be lost by the time the monster revives.
            const int revives = (mons->props.exists("bennu_revives"))
                              ? mons->props["bennu_revives"].get_byte() : 0;
            const beh_type att = mons->has_ench(ENCH_CHARM)
                                     ? BEH_HOSTILE : SAME_ATTITUDE(mons);

            bennu_revive_fineff::schedule(mons->pos(), revives, att, mons->foe);
        }
    }

    if (mons_is_tentacle_head(mons_base_type(*mons)))
    {
        if (destroy_tentacles(mons)
            && !in_transit
            && you.see_cell(mons->pos())
            && !was_banished)
        {
            if (mons_base_type(*mons) == MONS_KRAKEN)
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
        destroy_tentacle(mons);
    }
    else if (mons->type == MONS_ELDRITCH_TENTACLE_SEGMENT
             && killer != KILL_MISC)
    {
       monster_die(monster_by_mid(mons->tentacle_connect), killer,
                   killer_index, silent, wizard, fake);
    }
    else if (mons->type == MONS_FLAYED_GHOST)
        end_flayed_effect(mons);
    // Give the treant a last chance to release its hornets if it is killed in a
    // single blow from above half health
    else if (mons->type == MONS_SHAMBLING_MANGROVE && !was_banished
             && !mons->pacified() && (!summoned || duration > 0) && !wizard
             && !mons_reset)
    {
        treant_release_fauna(mons);
    }
    else if (!mons->is_summoned() && mummy_curse_power(mons->type) > 0)
        _mummy_curse(mons, mummy_curse_power(mons->type), killer, killer_index);

    if (mons->has_ench(ENCH_INFESTATION) && !was_banished && !mons_reset)
        _infestation_create_scarab(mons);

    if (mons->mons_species() == MONS_BALLISTOMYCETE)
    {
        _activate_ballistomycetes(mons, mons->pos(),
                                  YOU_KILL(killer) || pet_kill);
    }

    if (!wizard && !submerged && !was_banished)
    {
        _monster_die_cloud(mons, !fake_abjure && !timeout && !mons_reset,
                           silent, summoned);
    }

    item_def* corpse = nullptr;
    if (leaves_corpse && !was_banished && !spectralised)
    {
        // Have to add case for disintegration effect here? {dlb}
        item_def* daddy_corpse = nullptr;

        if (mons->type == MONS_SPRIGGAN_RIDER)
        {
            daddy_corpse = mounted_kill(mons, MONS_HORNET, killer, killer_index);
            mons->type = MONS_SPRIGGAN;
        }
        corpse = place_monster_corpse(*mons, silent);
        if (!corpse)
            corpse = daddy_corpse;
    }
    if (corpse && mons->has_ench(ENCH_BOUND_SOUL))
        _make_derived_undead(mons, !death_message, true);
    if (you.duration[DUR_DEATH_CHANNEL] && was_visible && gives_player_xp)
        _make_derived_undead(mons, !death_message, false);

    const unsigned int player_xp = gives_player_xp
        ? _calc_player_experience(mons) : 0;
    const unsigned int monster_xp = _calc_monster_experience(mons, killer,
                                                             killer_index);

    // Player Powered by Death
    if (gives_player_xp && player_mutation_level(MUT_POWERED_BY_DEATH)
        && (killer == KILL_YOU
            || killer == KILL_YOU_MISSILE
            || killer == KILL_YOU_CONF
            || pet_kill))
    {
        // Enable the status
        reset_powered_by_death_duration();

        // Maybe increase strength. The chance decreases with number
        // of existing stacks.
        const int pbd_level = player_mutation_level(MUT_POWERED_BY_DEATH);
        const int pbd_str = you.props[POWERED_BY_DEATH_KEY].get_int();
        if (x_chance_in_y(10 - pbd_str, 10))
        {
            const int pbd_inc = random2(1 + pbd_level);
            you.props[POWERED_BY_DEATH_KEY] = pbd_str + pbd_inc;
            dprf("Powered by Death strength +%d=%d", pbd_inc,
                 pbd_str + pbd_inc);
        }
    }

    if (!crawl_state.game_is_arena() && leaves_corpse && !in_transit)
        you.kills.record_kill(mons, killer, pet_kill);

    if (fake)
    {
        if (corpse && _reaping(mons))
            corpse = nullptr;
        _give_experience(player_xp, monster_xp, killer, killer_index,
                         pet_kill, was_visible);
        crawl_state.dec_mon_acting(mons);

        return corpse;
    }

    mons_remove_from_grid(*mons);
    fire_monster_death_event(mons, killer, killer_index, false);

    if (crawl_state.game_is_arena())
        arena_monster_died(mons, killer, killer_index, silent, corpse);

    const coord_def mwhere = mons->pos();
    if (drop_items)
    {
        // monster_drop_things may lead to a level excursion (via
        // god_id_item -> ... -> ShoppingList::item_type_identified),
        // which fails to save/restore the dead monster. Keep it alive
        // since we still need it.
        unwind_var<int> fakehp(mons->hit_points, 1);
        monster_drop_things(mons, YOU_KILL(killer) || pet_kill);
    }
    else
    {
        // Destroy the items belonging to MF_HARD_RESET monsters so they
        // don't clutter up mitm[].
        mons->destroy_inventory();
    }

    if (leaves_corpse && corpse)
    {
        if (!silent && !wizard)
            _special_corpse_messaging(*mons);
        // message ordering... :(
        if (corpse->base_type == OBJ_CORPSES) // not gold
            _maybe_drop_monster_hide(*corpse, silent);
    }

    if (mons->is_divine_companion()
        && killer != KILL_RESET
        && !(mons->flags & MF_BANISHED))
    {
        remove_companion(mons);
        if (mons_is_hepliaklqana_ancestor(mons->type))
        {
            ASSERT(hepliaklqana_ancestor() == MID_NOBODY);
            if (!you.can_see(*mons))
            {
                mprf("%s has departed this plane of existence.",
                     hepliaklqana_ally_name().c_str());
            }
            // respawn in ~30-60 turns
            you.duration[DUR_ANCESTOR_DELAY] = random_range(300, 600);
        }
    }

    // If we kill an invisible monster reactivate autopickup.
    // We need to check for actual invisibility rather than whether we
    // can see the monster. There are several edge cases where a monster
    // is visible to the player but we still need to turn autopickup
    // back on, such as TSO's halo or sticky flame. (jpeg)
    if (you.see_cell(mons->pos()) && mons->has_ench(ENCH_INVIS)
        && !mons->friendly())
    {
        autotoggle_autopickup(false);
    }

    if (corpse && _reaping(mons))
        corpse = nullptr;

    crawl_state.dec_mon_acting(mons);
    monster_cleanup(mons);

    // Force redraw for monsters that die.
    if (in_bounds(mwhere) && you.see_cell(mwhere))
    {
        view_update_at(mwhere);
        update_screen();
    }

    if (!mons_reset)
    {
        _give_experience(player_xp, monster_xp, killer, killer_index, pet_kill,
                         was_visible);
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
            if (you.can_see(**mi))
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

        act->heal(act->props["flay_damage"].get_int());
        act->props.erase("flay_damage");
    }

    for (const CrawlStoreValue& store : act->props["flay_blood"].get_vector())
        env.pgrid(store.get_coord()) &= ~FPROP_BLOODY;
    act->props.erase("flay_blood");
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

    // So proper messages are printed
    if (mons->has_ench(ENCH_GRASPING_ROOTS_SOURCE))
        mons->del_ench(ENCH_GRASPING_ROOTS_SOURCE);

    // Monsters haloes should be removed when they die.
    if (mons->halo_radius()
        || mons->umbra_radius()
        || mons->silence_radius())
    {
        invalidate_agrid();
    }

    // May have been constricting something. No message because that depends
    // on the order in which things are cleaned up: If the constrictee is
    // cleaned up first, we wouldn't get a message anyway.
    mons->stop_constricting_all(false, true);

    if (mons_is_tentacle_head(mons_base_type(*mons)))
        destroy_tentacles(mons);

    const mid_t mid = mons->mid;
    env.mid_cache.erase(mid);
    unsigned int monster_killed = mons->mindex();
    mons->reset();

    for (monster_iterator mi; mi; ++mi)
    {
        if (mi->foe == monster_killed)
            mi->foe = MHITNOT;

        int sumtype = 0;
        if (mi->summoner == mid
            && (mi->is_summoned(nullptr, &sumtype)
                || sumtype == MON_SUMM_CLONE))
        {
            mi->del_ench(ENCH_ABJ);
        }
    }

    if (you.pet_target == monster_killed)
        you.pet_target = MHITNOT;
}

item_def* mounted_kill(monster* daddy, monster_type mc, killer_type killer,
                       int killer_index)
{
    monster mon;
    mon.type = mc;
    mon.moveto(daddy->pos());
    define_monster(mon); // assumes mc is not a zombie
    mon.flags = daddy->flags;

    // Need to copy ENCH_ABJ etc. or we could get real XP/meat from a summon.
    mon.enchantments = daddy->enchantments;
    mon.ench_cache = daddy->ench_cache;

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
    // Flying/clinging monsters don't make contact with the terrain.
    if (!mons->ground_level())
        return;

    dungeon_feature_type grid = grd(mons->pos());
    if (grid != DNGN_LAVA && grid != DNGN_DEEP_WATER
        || monster_habitable_grid(mons, grid))
    {
        return;
    }


    // Don't worry about invisibility. You should be able to see if
    // something has fallen into the lava.
    if (you.see_cell(mons->pos()) && (oldpos == mons->pos() || grd(oldpos) != grid))
    {
         mprf("%s falls into the %s!",
             mons->name(DESC_THE).c_str(),
             grid == DNGN_LAVA ? "lava" : "water");
    }

    // Even fire resistant monsters perish in lava.
    if (grid == DNGN_LAVA && mons->res_fire() < 2)
    {
        simple_monster_message(*mons, " is incinerated.",
                               MSGCH_MONSTER_DAMAGE, MDAM_DEAD);
    }
    else if (mons->can_drown())
    {
        simple_monster_message(*mons, " drowns.",
                               MSGCH_MONSTER_DAMAGE, MDAM_DEAD);
    }
    else
    {
        simple_monster_message(*mons, " falls apart.",
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
    if (!_yred_enslave_soul(mons, killer))
        monster_die(mons, killer, killnum, true);
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
            monster_die(*mi, KILL_DISMISSED, NON_MONSTER, false, true);
            ++ndismissed;
        }
    }

    return ndismissed;
}

string summoned_poof_msg(const monster* mons, bool plural)
{
    int  summon_type = 0;
    bool valid_mon   = false;
    if (mons != nullptr && !invalid_monster(mons))
    {
        (void) mons->is_summoned(nullptr, &summon_type);
        valid_mon = true;
    }

    string msg      = "disappear%s in a puff of smoke";
    bool   no_chaos = false;

    switch (summon_type)
    {
    case SPELL_SHADOW_CREATURES:
    case SPELL_WEAVE_SHADOWS:
    case MON_SUMM_SCROLL:
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

    case SPELL_SPECTRAL_CLOUD:
    case SPELL_CALL_LOST_SOUL:
        msg = "fade%s away";
        break;
    }

    if (valid_mon)
    {
        if (mons->god == GOD_XOM && !no_chaos && one_chance_in(10)
            || mons->type == MONS_CHAOS_SPAWN)
        {
            msg = "degenerate%s into a cloud of primal chaos";
        }

        if (mons->type == MONS_IEOH_JIAN_WEAPON)
            msg = "shatter%s in a cloud of steel fragments";

        if (mons->is_holy()
            && summon_type != SPELL_SHADOW_CREATURES
            && summon_type != MON_SUMM_CHAOS)
        {
            msg = "dissolve%s into sparkling lights";
        }

        if (mons_is_slime(*mons)
            && mons->god == GOD_JIYVA)
        {
            msg = "dissolve%s into a puddle of slime";
        }

        if (mons->type == MONS_DROWNED_SOUL)
            msg = "return%s to the deep";

        if (mons->has_ench(ENCH_PHANTOM_MIRROR))
            msg = "shimmer%s and vanish" + string(plural ? "" : "es"); // Ugh
    }

    // Conjugate.
    msg = make_stringf(msg.c_str(), plural ? "" : "s");

    return msg;
}

string summoned_poof_msg(const monster* mons, const item_def &item)
{
    ASSERT(item.flags & ISFLAG_SUMMONED);

    return summoned_poof_msg(mons, item.quantity > 1);
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
 * enslaved, when Pikel dies, when Pikel is banished.
 * It is handled by a daction (as a fineff) to preserve across levels.
 **/
void pikel_band_neutralise()
{
    int visible_slaves = 0;
    for (monster_iterator mi; mi; ++mi)
    {
        if (mi->type == MONS_SLAVE
            && testbits(mi->flags, MF_BAND_MEMBER)
            && mi->props.exists("pikel_band")
            && mi->mname != "freed slave"
            && mi->observable())
        {
            visible_slaves++;
        }
    }
    string final_msg;
    if (visible_slaves > 0)
    {
        if (player_mutation_level(MUT_NO_LOVE))
        {
            const char *substr = visible_slaves > 1 ? "slaves" : "slave";
            final_msg = make_stringf("Pikel's spell is broken, but the former "
                                     "%s can only feel hate for you!", substr);
        }
        else
        {
            const char *substr = visible_slaves > 1
                ? "slaves thank you for their"
                : "slave thanks you for its";
            final_msg = make_stringf("With Pikel's spell broken, the former %s "
                                     "freedom.", substr);
        }
    }
    delayed_action_fineff::schedule(DACT_PIKEL_SLAVES, final_msg);
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

        if (!mi->props.exists("kirke_band")
            && !mi->props.exists(ORIG_MONSTER_KEY))
        {
            continue;
        }

        // Shapeshifters will stop being a hog when they feel like it.
        if (mi->is_shapeshifter())
            continue;

        const bool could_see = you.can_see(**mi);

        if (could_see) any++;

        if (!mi->props.exists(ORIG_MONSTER_KEY) && could_see)
            human++;
    }

    string final_msg;
    if (any > 0)
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
    // Sometimes, if you pacify one twin near a staircase, they leave
    // in the same turn. Convert, in those instances.
    if (twin->neutral() && !twin->has_ench(ENCH_INSANE))
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
    mons->props["can_climb"] = true;
    if (!in_transit)
        mons->props["speech_prefix"] = "twin_died";
    else
        mons->props["speech_prefix"] = "twin_banished";

    // If you've stabbed one of them, the other one is likely asleep still.
    if (mons->asleep())
        behaviour_event(mons, ME_DISTURB, 0, mons->pos());

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
        mons->props["speech_prefix"] = "twin_ikilled";
    }

    // Drop the final '_'.
    key.erase(key.length() - 1);

    string death_message = getSpeakString(key);

    // Check if they can speak or not: they may have been polymorphed.
    if (you.see_cell(mons->pos()) && !death_message.empty() && mons->can_speak())
        mons_speaks_msg(mons, death_message, MSGCH_TALK, silenced(you.pos()));
    else if (mons->can_speak())
        mpr(death_message);

    // Upgrade the spellbook here, as elven_twin_energize
    // may not be called due to lack of visibility.
    if (mons_is_mons_class(mons, MONS_DOWAN))
    {
        mons->spells[0].spell = SPELL_STONE_ARROW;
        mons->spells[1].spell = SPELL_THROW_ICICLE;
        mons->spells[3].spell = SPELL_BLINK;
        mons->spells[4].spell = SPELL_HASTE;
        // Nothing with 6.

        // Indicate that he has an updated spellbook.
        mons->props[CUSTOM_SPELLS_KEY] = true;
    }

    // Finally give them new energy
    if (mons->can_see(you) && !mons->has_ench(ENCH_INSANE))
        elven_twin_energize(mons);
    else
        mons->props[ELVEN_ENERGIZE_KEY] = true;
}

void elven_twin_energize(monster* mons)
{
    if (mons_is_mons_class(mons, MONS_DUVESSA))
        mons->go_berserk(true);
    else
    {
        ASSERT(mons_is_mons_class(mons, MONS_DOWAN));
        if (mons->observable())
            simple_monster_message(*mons, " seems to find hidden reserves of power!");

        mons->add_ench(ENCH_HASTE);
    }

    mons->props[ELVEN_IS_ENERGIZED_KEY] = true;
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

    // Don't consider already neutralised monsters.
    if (!mons->neutral() || mons->has_ench(ENCH_INSANE))
        return;

    behaviour_event(mons, ME_WHACK, &you, you.pos(), false);
}

bool mons_felid_can_revive(const monster* mons)
{
    return !mons->props.exists("felid_revives")
           || mons->props["felid_revives"].get_byte() < 2;
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
            || grd(revive_place) != DNGN_FLOOR
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
    const int revives = (mons->props.exists("felid_revives"))
                        ? mons->props["felid_revives"].get_byte() + 1
                        : 1;

    monster *newmons =
        create_monster(
            mgen_data(type, (mons->has_ench(ENCH_CHARM) ? BEH_HOSTILE
                             : SAME_ATTITUDE(mons)), revive_place, mons->foe));

    if (newmons)
    {
        for (mon_inv_iterator ii(*mons); ii; ++ii)
        {
            give_specific_item(newmons, *ii);
            destroy_item(ii->index());
        }

        newmons->props["felid_revives"].get_byte() = revives;
    }
}

bool mons_bennu_can_revive(const monster* mons)
{
    return !mons->props.exists("bennu_revives")
           || mons->props["bennu_revives"].get_byte() < 1;
}

bool ieoh_jian_kill_oldest_weapon()
{
    auto monsters = find_ieoh_jian_manifested_weapons(false);
    if (monsters.empty()) 
        return false;

    monster_die(monsters.at(0), KILL_RESET, NON_MONSTER);
    return true;
}

