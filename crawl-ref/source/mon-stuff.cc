/*
 *  File:       mon-stuff.cc
 *  Summary:    Misc monster related functions.
 *  Written by: Linley Henzell
 */

#include "AppHdr.h"
#include "mon-stuff.h"

#include "areas.h"
#include "arena.h"
#include "artefact.h"
#include "attitude-change.h"
#include "cloud.h"
#include "cluautil.h"
#include "coord.h"
#include "coordit.h"
#include "database.h"
#include "delay.h"
#include "dgn-actions.h"
#include "dgnevent.h"
#include "directn.h"
#include "dlua.h"
#include "env.h"
#include "exclude.h"
#include "fprop.h"
#include "files.h"
#include "food.h"
#include "godabil.h"
#include "godconduct.h"
#include "hints.h"
#include "hiscores.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "kills.h"
#include "libutil.h"
#include "makeitem.h"
#include "message.h"
#include "mgen_data.h"
#include "misc.h"
#include "mon-abil.h"
#include "mon-behv.h"
#include "mon-death.h"
#include "mon-iter.h"
#include "mon-place.h"
#include "mon-speak.h"
#include "mon-util.h"
#include "notes.h"
#include "player.h"
#include "random.h"
#include "religion.h"
#include "shout.h"
#include "spl-miscast.h"
#include "spl-summoning.h"
#include "spl-util.h"
#include "stash.h"
#include "state.h"
#include "stuff.h"
#include "tagstring.h"
#include "terrain.h"
#include "transform.h"
#include "traps.h"
#include "view.h"
#include "viewchar.h"
#include "xom.h"

static bool _wounded_damaged(mon_holy_type holi);

static int _make_mimic_item(monster_type type)
{
    int it = items(0, OBJ_RANDOM, OBJ_RANDOM, true, 0, 0);

    if (it == NON_ITEM)
        return NON_ITEM;

    item_def &item = mitm[it];

    item.base_type = OBJ_UNASSIGNED;
    item.sub_type  = 0;
    item.special   = 0;
    item.colour    = 0;
    item.flags     = 0;
    item.quantity  = 1;
    item.plus      = 0;
    item.plus2     = 0;
    item.link      = NON_ITEM;

    int prop;
    switch (type)
    {
    case MONS_WEAPON_MIMIC:
        item.base_type = OBJ_WEAPONS;
        item.sub_type = random2(WPN_MAX_NONBLESSED + 1);

        prop = random2(100);

        if (prop < 20)
            make_item_randart(item);
        else if (prop < 50)
            set_equip_desc(item, ISFLAG_GLOWING);
        else if (prop < 80)
            set_equip_desc(item, ISFLAG_RUNED);
        else if (prop < 85)
            set_equip_race(item, ISFLAG_ORCISH);
        else if (prop < 90)
            set_equip_race(item, ISFLAG_DWARVEN);
        else if (prop < 95)
            set_equip_race(item, ISFLAG_ELVEN);
        break;

    case MONS_ARMOUR_MIMIC:
        item.base_type = OBJ_ARMOUR;
        do
            item.sub_type = random2(NUM_ARMOURS);
        while (armour_is_hide(item));

        prop = random2(100);

        if (prop < 20)
            make_item_randart(item);
        else if (prop < 40)
            set_equip_desc(item, ISFLAG_GLOWING);
        else if (prop < 60)
            set_equip_desc(item, ISFLAG_RUNED);
        else if (prop < 80)
            set_equip_desc(item, ISFLAG_EMBROIDERED_SHINY);
        else if (prop < 85)
            set_equip_race(item, ISFLAG_ORCISH);
        else if (prop < 90)
            set_equip_race(item, ISFLAG_DWARVEN);
        else if (prop < 95)
            set_equip_race(item, ISFLAG_ELVEN);
        break;

    case MONS_SCROLL_MIMIC:
        item.base_type = OBJ_SCROLLS;
        item.sub_type = random2(NUM_SCROLLS);
        break;

    case MONS_POTION_MIMIC:
        item.base_type = OBJ_POTIONS;
        do
            item.sub_type = random2(NUM_POTIONS);
        while (is_blood_potion(item));
        break;

    case MONS_GOLD_MIMIC:
    default:
        item.base_type = OBJ_GOLD;
        item.quantity = 5 + random2(1000);
        break;
    }

    item_colour(item); // also sets special vals for scrolls/potions

    return (it);
}

const item_def *give_mimic_item(monster* mimic)
{
    ASSERT(mimic != NULL && mons_is_item_mimic(mimic->type));

    mimic->destroy_inventory();
    int it = _make_mimic_item(mimic->type);
    if (it == NON_ITEM)
        return 0;
    if (!mimic->pickup_misc(mitm[it], 0))
        ASSERT("Mimic failed to pickup its item.");
    ASSERT(mimic->inv[MSLOT_MISCELLANY] != NON_ITEM);
    return (&mitm[mimic->inv[MSLOT_MISCELLANY]]);
}

const item_def &get_mimic_item(const monster* mimic)
{
    ASSERT(mimic != NULL && mons_is_item_mimic(mimic->type));

    ASSERT(mimic->inv[MSLOT_MISCELLANY] != NON_ITEM);

    return (mitm[mimic->inv[MSLOT_MISCELLANY]]);
}

dungeon_feature_type get_mimic_feat (const monster* mimic)
{
    switch (mimic->type)
    {
    case MONS_DOOR_MIMIC:
        return (DNGN_CLOSED_DOOR);
    case MONS_PORTAL_MIMIC:
        if (mimic->props.exists("portal_desc"))
            return (DNGN_ENTER_PORTAL_VAULT);
        else
            return (DNGN_ENTER_LABYRINTH);
    case MONS_TRAP_MIMIC:
        return (DNGN_TRAP_MECHANICAL);
    case MONS_STAIR_MIMIC:
        if (mimic->props.exists("stair_type"))
        {
            return static_cast<dungeon_feature_type>(mimic->props[
                "stair_type"].get_short());
        }
        else
        {
            return (DNGN_STONE_STAIRS_DOWN_I);
        }
    case MONS_SHOP_MIMIC:
        return (DNGN_ENTER_SHOP);
    case MONS_FOUNTAIN_MIMIC:
        if (mimic->props.exists("fountain_type"))
        {
            return static_cast<dungeon_feature_type>(mimic->props[
                "fountain_type"].get_short());
        }
        else
        {
            return (DNGN_FOUNTAIN_BLUE);
        }
    default:
        ASSERT(false);
        break;
    }

    return (DNGN_UNSEEN);
}

bool feature_mimic_at (const coord_def &c)
{
    const monster* mons = monster_at(c);
    if (mons != NULL)
        return mons_is_feat_mimic(mons->type);

    return (false);
}

// Sets the colour of a mimic to match its description... should be called
// whenever a mimic is created or teleported. - bwr
int get_mimic_colour(const monster* mimic)
{
    ASSERT(mimic != NULL);

    if (mons_is_item_mimic(mimic->type))
        return (get_mimic_item(mimic).colour);
    else
        return (mimic->colour);
}

// Monster curses a random player inventory item.
bool curse_an_item(bool decay_potions, bool quiet)
{
    // allowing these would enable mummy scumming
    if (you.religion == GOD_ASHENZARI)
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

        if (you.inv[i].base_type == OBJ_WEAPONS
            || you.inv[i].base_type == OBJ_ARMOUR
            || you.inv[i].base_type == OBJ_JEWELLERY
            || you.inv[i].base_type == OBJ_POTIONS)
        {
            if (you.inv[i] .cursed())
                continue;

            if (you.inv[i].base_type != OBJ_POTIONS
                && item_is_melded(you.inv[i]))
            {
                // Melded items cannot be cursed.
                continue;
            }

            if (you.inv[i].base_type == OBJ_POTIONS
                && (!decay_potions || you.inv[i].sub_type == POT_DECAY))
            {
                continue;
            }

            // Item is valid for cursing, so we'll give it a chance.
            count++;
            if (one_chance_in(count))
                item = i;
        }
    }

    // Any item to curse?
    if (item == ENDOFPACK)
        return (false);

    // Curse item.
    if (decay_potions && !quiet) // Just for mummies.
        mpr("You feel nervous for a moment...", MSGCH_MONSTER_SPELL);

    if (you.inv[item].base_type == OBJ_POTIONS)
    {
        int amount;
        // Decay at least two of the stack.
        if (you.inv[item].quantity <= 2)
            amount = you.inv[item].quantity;
        else
            amount = 2 + random2(you.inv[item].quantity - 1);

        split_potions_into_decay(item, amount);

        if (item_value(you.inv[item], true) / amount > 2)
            xom_is_stimulated(32 * amount);
    }
    else
    {
        do_curse_item(you.inv[item], false);
    }

    return (true);
}

// The default suitable() function for monster_drop_things().
bool is_any_item(const item_def& item)
{
    return (true);
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

                // If a monster is swimming, the items are ALREADY
                // underwater.
                move_item_to_grid(&item, mons->pos(), mons->swimming());
            }

            mons->inv[i] = NON_ITEM;
        }
    }
}

// Initializes a corpse item using the given monster and monster type.
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
        return (MONS_NO_MONSTER);
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

        if (mons->has_ench(ENCH_SHAPESHIFTER))
            corpse_class = MONS_SHAPESHIFTER;
        else if (mons->has_ench(ENCH_GLOWING_SHAPESHIFTER))
            corpse_class = MONS_GLOWING_SHAPESHIFTER;
    }

    // Doesn't leave a corpse.
    if (!mons_class_can_leave_corpse(corpse_class) && !force_corpse)
        return (MONS_NO_MONSTER);

    corpse.flags       = 0;
    corpse.base_type   = OBJ_CORPSES;
    corpse.plus        = corpse_class;
    corpse.plus2       = 0;    // butcher work done
    corpse.sub_type    = CORPSE_BODY;
    corpse.special     = FRESHEST_CORPSE;  // rot time
    corpse.quantity    = 1;
    corpse.orig_monnum = mtype + 1;

    if (mtype == MONS_ROTTING_HULK)
        corpse.special = ROTTING_CORPSE;

    if (mons)
    {
        corpse.props[MONSTER_HIT_DICE] = short(mons->hit_dice);
        corpse.props[MONSTER_NUMBER]   = short(mons->number);
    }

    corpse.colour = mons_class_colour(corpse_class);
    if (corpse.colour == BLACK)
    {
        if (mons)
        {
            corpse.colour = mons->colour;
        }
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

    if (mons && !mons->mname.empty())
    {
        corpse.props[CORPSE_NAME_KEY] = mons->mname;
        corpse.props[CORPSE_NAME_TYPE_KEY].get_int() = mons->flags;
    }
    else if (mons_is_unique(mtype))
    {
        corpse.props[CORPSE_NAME_KEY] = mons_type_name(mtype, DESC_PLAIN);
        corpse.props[CORPSE_NAME_TYPE_KEY].get_int() = 0;
    }

    return (corpse_class);
}

bool explode_corpse(item_def& corpse, const coord_def& where)
{
    // Don't want chunks to show up behind the player.
    los_def ld(where, opc_no_actor);

    if (monster_descriptor(corpse.plus, MDSC_LEAVES_HIDE)
        && mons_genus(corpse.plus) == MONS_DRAGON)
    {
        // Uh... dragon hide is tough stuff and it keeps the monster in
        // one piece?  More importantly, it prevents a flavor feature
        // from becoming a trap for the unwary.

        return (false);
    }

    ld.update();

    const int max_chunks = get_max_corpse_chunks(corpse.plus);

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

    blood_spray(where, static_cast<monster_type>(corpse.plus), blood);

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

        if (feat_is_solid(grd(cp)) || actor_at(cp))
            continue;

        --nchunks;

        dprf("Success");

        copy_item_to_grid(corpse, cp);
    }

    return (true);
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
        return (-1);

    // Don't attempt to place corpses within walls, either.
    // Currently, this only applies to (shapeshifter) rock worms.
    if (feat_is_wall(grd(mons->pos())))
        return (-1);

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
        || (!force && !vault_forced && coinflip()))
    {
        return (-1);
    }

    int o = get_item_slot();
    if (o == NON_ITEM)
    {
        item_was_destroyed(corpse);
        return (-1);
    }

    mitm[o] = corpse;

    origin_set_monster(mitm[o], mons);

    if ((mons->flags & MF_EXPLODE_KILL)
        && explode_corpse(corpse, mons->pos()))
    {
        // We already have a spray of chunks.
        destroy_item(o);
        return (-1);
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
                     mitm[o].name(DESC_CAP_A).c_str());
            }
        }
        const bool poison = (chunk_is_poisonous(mons_corpse_effect(corpse_class))
                             && player_res_poison() <= 0);

        if (o != NON_ITEM)
            hints_dissection_reminder(!poison);
    }

    return (o == NON_ITEM ? -1 : o);
}

static void _hints_inspect_kill()
{
    if (Hints.hints_events[HINT_KILLED_MONSTER])
        learned_something_new(HINT_KILLED_MONSTER);
}

static std::string _milestone_kill_verb(killer_type killer)
{
    return (killer == KILL_RESET ? "banished " : "killed ");
}

static void _check_kill_milestone(const monster* mons,
                                  killer_type killer, int i)
{
    // XXX: See comment in monster_polymorph.
    bool is_unique = mons_is_unique(mons->type);
    if (mons->props.exists("original_was_unique"))
        is_unique = mons->props["original_was_unique"].get_bool();

    if (mons->type == MONS_PLAYER_GHOST)
    {
        monster_info mi(mons);
        std::string milestone = _milestone_kill_verb(killer) + "the ghost of ";
        milestone += get_ghost_description(mi, true);
        milestone += ".";
        mark_milestone("ghost", milestone);
    }
    // Or summoned uniques, which a summoned ghost is treated as {due}
    else if (is_unique && !mons->is_summoned())
    {
        mark_milestone("uniq",
                       _milestone_kill_verb(killer)
                       + mons->name(DESC_NOCAP_THE, true)
                       + ".");
    }
}

static int _calc_monster_experience(monster* victim, killer_type killer,
                                    int killer_index)
{
    const int experience = exper_value(victim);
    const bool no_xp = victim->has_ench(ENCH_ABJ) || !experience;
    const bool created_friendly = testbits(victim->flags, MF_NO_REWARD);

    if (no_xp || !MON_KILL(killer) || invalid_monster_index(killer_index))
        return (0);

    monster* mon = &menv[killer_index];
    if (!mon->alive())
        return (0);

    if ((created_friendly && mon->friendly())
        || mons_aligned(mon, victim))
    {
        return (0);
    }

    return (experience);
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
        if (you.religion != GOD_SHINING_ONE && you.religion != GOD_BEOGH
            || player_under_penance()
            || !one_chance_in(3))
        {
            return;
        }

        // Randomly bless the follower who gained experience.
        if (you.religion == GOD_SHINING_ONE
                && random2(you.piety) >= piety_breakpoint(0)
            || you.religion == GOD_BEOGH
                && random2(you.piety) >= piety_breakpoint(2))
        {
            bless_follower(mon);
        }
    }
}

static int _calc_player_experience(monster* mons, killer_type killer,
                                   bool pet_kill, int killer_index)
{
    const int experience = exper_value(mons);

    const bool created_friendly = testbits(mons->flags, MF_NO_REWARD);
    const bool was_neutral = testbits(mons->flags, MF_WAS_NEUTRAL);
    const bool no_xp = mons->has_ench(ENCH_ABJ) || !experience;
    const bool already_got_half_xp = testbits(mons->flags, MF_GOT_HALF_XP);
    const int half_xp = (experience + 1) / 2;

    if (created_friendly || was_neutral || no_xp)
        return (0); // No xp if monster was created friendly or summoned.
    else if (YOU_KILL(killer))
    {
        if (already_got_half_xp)
            // Note: This doesn't happen currently since monsters with
            //       MF_GOT_HALF_XP have always gone through pacification,
            //       hence also have MF_WAS_NEUTRAL. [rob]
            return (experience - half_xp);
        else
            return (experience);
    }
    else if (pet_kill && !already_got_half_xp)
        return (half_xp);
    else
        return (0);
}

static void _give_player_experience(int experience, killer_type killer,
                                    bool pet_kill, bool was_visible)
{
    if (experience <= 0 || crawl_state.game_is_arena())
        return;

    unsigned int exp_gain = 0;
    unsigned int avail_gain = 0;;
    gain_exp(experience, &exp_gain, &avail_gain);

    kill_category kc =
            (killer == KILL_YOU || killer == KILL_YOU_MISSILE) ? KC_YOU :
            (pet_kill)                                         ? KC_FRIENDLY :
                                                                 KC_OTHER;
    PlaceInfo& curr_PlaceInfo = you.get_place_info();
    PlaceInfo  delta;

    delta.mon_kill_num[kc]++;
    delta.mon_kill_exp       += exp_gain;
    delta.mon_kill_exp_avail += avail_gain;

    you.global_info += delta;
    you.global_info.assert_validity();

    curr_PlaceInfo += delta;
    curr_PlaceInfo.assert_validity();

    // Give a message for monsters dying out of sight.
    if (exp_gain > 0 && !was_visible)
        mpr("You feel a bit more experienced.");
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
        return (false);

    if (i == ANON_FRIENDLY_MONSTER)
        return (true);

    if (invalid_monster_index(i))
        return (false);

    const monster* m = &menv[i];
    if (m->friendly()) // This includes enslaved monsters.
        return (true);

    // Check if the monster was confused by you or a friendly, which
    // makes casualties to this monster collateral kills.
    const mon_enchant me = m->get_ench(ENCH_CONFUSION);
    return (me.ench == ENCH_CONFUSION
            && (me.who == KC_YOU || me.who == KC_FRIENDLY));
}

int exp_rate(int killer)
{
    if (killer == MHITYOU)
        return 2;

    if (_is_pet_kill(KILL_MON, killer))
        return 1;

    return 0;
}

// Elyvilon will occasionally (5% chance) protect the life of one of
// your allies.
static bool _ely_protect_ally(monster* mons)
{
    if (you.religion != GOD_ELYVILON)
        return (false);

    if (!mons->is_holy()
            && mons->holiness() != MH_NATURAL
        || !mons->friendly()
        || !you.can_see(mons) // for simplicity
        || !one_chance_in(20))
    {
        return (false);
    }

    mons->hit_points = 1;

    snprintf(info, INFO_SIZE, " protects %s from harm!%s",
             mons->name(DESC_NOCAP_THE).c_str(),
             coinflip() ? "" : " You feel responsible.");

    simple_god_message(info);
    lose_piety(1);

    return (true);
}

// Elyvilon retribution effect: Heal hostile monsters that were about to
// be killed by you or one of your friends.
static bool _ely_heal_monster(monster* mons, killer_type killer, int i)
{
    if (you.religion == GOD_ELYVILON)
        return (false);

    god_type god = GOD_ELYVILON;

    if (!you.penance[god] || !god_hates_your_god(god))
        return (false);

    const int ely_penance = you.penance[god];

    if (mons->friendly() || !one_chance_in(10))
        return (false);

    if (MON_KILL(killer) && !invalid_monster_index(i))
    {
        monster* mon = &menv[i];
        if (!mon->friendly() || !one_chance_in(3))
            return (false);

        if (!mons_near(mons))
            return (false);
    }
    else if (!YOU_KILL(killer))
        return (false);

    dprf("monster hp: %d, max hp: %d", mons->hit_points, mons->max_hit_points);

    mons->hit_points = std::min(1 + random2(ely_penance/3),
                                   mons->max_hit_points);

    dprf("new hp: %d, ely penance: %d", mons->hit_points, ely_penance);

    snprintf(info, INFO_SIZE, "%s heals %s%s",
             god_name(god, false).c_str(),
             mons->name(DESC_NOCAP_THE).c_str(),
             mons->hit_points * 2 <= mons->max_hit_points ? "." : "!");

    god_speaks(god, info);
    dec_penance(god, 1 + random2(mons->hit_points/2));

    return (true);
}

static bool _yred_enslave_soul(monster* mons, killer_type killer)
{
    if (you.religion == GOD_YREDELEMNUL && mons_enslaved_body_and_soul(mons)
        && mons_near(mons) && killer != KILL_RESET
        && killer != KILL_DISMISSED)
    {
        yred_make_enslaved_soul(mons, player_under_penance());
        return (true);
    }

    return (false);
}

static bool _beogh_forcibly_convert_orc(monster* mons, killer_type killer,
                                        int i)
{
    if (you.religion == GOD_BEOGH
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
                return (true);
            }
        }
    }

    return (false);
}

static bool _monster_avoided_death(monster* mons, killer_type killer, int i)
{
    if (mons->hit_points < -25
        || mons->hit_points < -mons->max_hit_points
        || mons->max_hit_points <= 0
        || mons->hit_dice < 1)
    {
        return (false);
    }

    // Elyvilon specials.
    if (_ely_protect_ally(mons))
        return (true);
    if (_ely_heal_monster(mons, killer, i))
        return (true);

    // Yredelemnul special.
    if (_yred_enslave_soul(mons, killer))
        return (true);

    // Beogh special.
    if (_beogh_forcibly_convert_orc(mons, killer, i))
        return (true);

    return (false);
}

static void _jiyva_died()
{
    if (you.religion == GOD_JIYVA)
        return;

    add_daction(DACT_REMOVE_JIYVA_ALTARS);

    if (!player_in_branch(BRANCH_SLIME_PITS))
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

    // Banished monsters aren't technically dead, so no death event
    // for them.
    if (killer == KILL_RESET)
        return;

    dungeon_events.fire_event(
        dgn_event(DET_MONSTER_DIED, mons->pos(), 0,
                  mons->mindex(), killer));
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
            mpr("Unknown mummy type.", MSGCH_DIAGNOSTICS);
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
        if (you.religion == GOD_KIKUBAAQUDGHA && !player_under_penance()
            && you.piety >= piety_breakpoint(1))
        {
            simple_god_message(" averts the curse.");
            return;
        }

        curse_an_item(true);
    }
    else
    {
        if (index == NON_MONSTER)
        {
            mpr("You feel extremely nervous for a moment...",
                MSGCH_MONSTER_SPELL);
        }
        else if (you.can_see(target))
        {
            mprf(MSGCH_MONSTER_SPELL, "A malignant aura surrounds %s.",
                 target->name(DESC_NOCAP_THE).c_str());
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
    beam.source_name  = origin.base_name(DESC_BASENAME);
    beam.target       = origin.pos();

    if (!crawl_state.game_is_arena() && origin.attitude == ATT_FRIENDLY
        && !origin.is_summoned())
    {
        beam.thrower = KILL_YOU;
    }
    else
    {
        beam.thrower = KILL_MON;
    }

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

void setup_lightning_explosion(bolt & beam, const monster& origin)
{
    _setup_base_explosion(beam, origin);
    beam.flavour = BEAM_ELECTRICITY;
    beam.damage  = dice_def(3, 20);
    beam.name    = "blast of lightning";
    beam.colour  = LIGHTCYAN;
    beam.ex_size = coinflip() ? 3 : 2;
}

static bool _spore_goes_pop(monster* mons, killer_type killer,
                            int killer_index, bool pet_kill, bool wizard)
{
    if (mons->hit_points > 0 || mons->hit_points <= -15 || wizard
        || killer == KILL_RESET || killer == KILL_DISMISSED)
    {
        return (false);
    }

    bolt beam;
    const int type = mons->type;

    const char* msg       = NULL;
    const char* sanct_msg = NULL;
    if (type == MONS_GIANT_SPORE)
    {
        setup_spore_explosion(beam, *mons);
        msg          = "The giant spore explodes!";
        sanct_msg    = "By Zin's power, the giant spore's explosion is "
                       "contained.";
    }
    else if (type == MONS_BALL_LIGHTNING)
    {
        setup_lightning_explosion(beam, *mons);
        msg          = "The ball lightning explodes!";
        sanct_msg    = "By Zin's power, the ball lightning's explosion "
                       "is contained.";
    }
    else
    {
        msg::streams(MSGCH_DIAGNOSTICS) << "Unknown spore type: "
                                        << static_cast<int>(type)
                                        << std::endl;
        return (false);
    }

    if (YOU_KILL(killer))
        beam.aux_source = "set off by themselves";
    else if (pet_kill)
        beam.aux_source = "set off by their pet";

    bool saw = false;
    if (you.can_see(mons))
    {
        saw = true;
        viewwindow();
        if (is_sanctuary(mons->pos()))
            mpr(sanct_msg, MSGCH_GOD);
        else
            mprf(MSGCH_MONSTER_DAMAGE, MDAM_DEAD, msg);
    }

    if (is_sanctuary(mons->pos()))
        return (false);

    // Detach monster from the grid first, so it doesn't get hit by
    // its own explosion. (GDL)
    mgrd(mons->pos()) = NON_MONSTER;

    // The explosion might cause a monster to be placed where the spore
    // used to be, so make sure that mgrd() doesn't get cleared a second
    // time (causing the new monster to become floating) when
    // mons->reset() is called.
    mons->set_position(coord_def(0,0));

    // Exploding kills the monster a bit earlier than normal.
    mons->hit_points = -16;
    if (saw)
        viewwindow();

    // FIXME: show_more == mons_near(mons)
    beam.explode();

    activate_ballistomycetes(mons, beam.target, YOU_KILL(beam.killer()));
    // Monster died in explosion, so don't re-attach it to the grid.
    return (true);
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

    std::string prefix = " ";
    if (corpse)
    {
        if (mons_weight(mons_species(mons->type)) == 0)
            return;

        prefix = "'s corpse ";
    }

    std::string msg = summoned_poof_msg(mons) + "!";

    cloud_type cloud = CLOUD_NONE;
    if (msg.find("smoke") != std::string::npos)
        cloud = random_smoke_type();
    else if (msg.find("chaos") != std::string::npos)
        cloud = CLOUD_CHAOS;

    if (!silent)
        simple_monster_message(mons, (prefix + msg).c_str());

    if (cloud != CLOUD_NONE)
    {
        place_cloud(cloud, mons->pos(), 1 + random2(3),
                    mons->kill_alignment(), KILL_MON_MISSILE);
    }
}

void mons_relocated(monster* mons)
{

    // If the main body teleports get rid of the tentacles
    if (mons_base_type(mons) == MONS_KRAKEN)
    {
        int headnum = mons->mindex();

        if (invalid_monster_index(headnum))
            return;

        for (monster_iterator mi; mi; ++mi)
        {
            if (mi->type == MONS_KRAKEN_TENTACLE
                && (int)mi->number == headnum)
            {
                for (monster_iterator connect; connect; ++connect)
                {
                    if (connect->type == MONS_KRAKEN_TENTACLE_SEGMENT
                        && (int) connect->number == mi->mindex())
                    {
                        monster_die(*connect, KILL_RESET, -1, true, false);
                    }
                }
                monster_die(*mi, KILL_RESET, -1, true, false);
            }
        }
    }
    // If a tentacle/segment is relocated just kill the tentacle
    else if (mons->type == MONS_KRAKEN_TENTACLE
             || mons->type == MONS_KRAKEN_TENTACLE_SEGMENT)
    {
        int base_id = mons->mindex();

        if (mons->type == MONS_KRAKEN_TENTACLE_SEGMENT)
        {
            base_id = mons->number;
        }

        for (monster_iterator connect; connect; ++connect)
        {
            if (connect->type == MONS_KRAKEN_TENTACLE_SEGMENT
                && (int) connect->number == base_id)
            {
                monster_die(*connect, KILL_RESET, -1, true, false);
            }
        }

        if (!::invalid_monster_index(base_id)
            && menv[base_id].type == MONS_KRAKEN_TENTACLE)
        {
            monster_die(&menv[base_id], KILL_RESET, -1, true, false);
        }
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
}

static int _destroy_tentacle(int tentacle_idx, monster* origin)
{
    int seen = 0;

    if (invalid_monster_index(tentacle_idx))
        return (0);


    // Some issue with using monster_die leading to DEAD_MONSTER
    // or w/e. Using hurt seems to cause more problems though.
    for (monster_iterator mi; mi; ++mi)
    {
        if (mi->type == MONS_KRAKEN_TENTACLE_SEGMENT
            && (int)mi->number == tentacle_idx
            && mi->mindex() != origin->mindex())
        {
            if (mons_near(*mi))
                seen++;
            //mi->hurt(*mi, INSTANT_DEATH);
            monster_die(*mi, KILL_MISC, NON_MONSTER, true);
        }
    }

    if (origin->mindex() != tentacle_idx)
    {
        if (mons_near(&menv[tentacle_idx]))
            seen++;

        //mprf("killing base, %d %d", origin->mindex(), tentacle_idx);
        //menv[tentacle_idx].hurt(&menv[tentacle_idx], INSTANT_DEATH);
        monster_die(&menv[tentacle_idx], KILL_MISC, NON_MONSTER, true);
    }

    return (seen);
}

static int _destroy_tentacles(monster* head)
{
    int tent = 0;
    int headnum = head->mindex();

    if (invalid_monster_index(headnum))
        return 0;

    for (monster_iterator mi; mi; ++mi)
    {
        if (mi->type == MONS_KRAKEN_TENTACLE
            && (int)mi->number == headnum)
        {
            for (monster_iterator connect; connect; ++connect)
            {
                if (connect->type == MONS_KRAKEN_TENTACLE_SEGMENT
                    && (int) connect->number == mi->mindex())
                {
                    connect->hurt(*connect, INSTANT_DEATH);
                }
            }
            if (mons_near(*mi))
                tent++;
            mi->hurt(*mi, INSTANT_DEATH);
        }
    }
    return tent;
}

static std::string _killer_type_name(killer_type killer)
{
    switch(killer)
    {
    case KILL_NONE:
        return ("none");
    case KILL_YOU:
        return ("you");
    case KILL_MON:
        return ("mon");
    case KILL_YOU_MISSILE:
        return ("you_missile");
    case KILL_MON_MISSILE:
        return ("mon_missile");
    case KILL_YOU_CONF:
        return ("you_conf");
    case KILL_MISCAST:
        return ("miscast");
    case KILL_MISC:
        return ("misc");
    case KILL_RESET:
        return ("reset");
    case KILL_DISMISSED:
        return ("dismissed");
    }
    ASSERT(false);
    return("");
}

static void _make_spectral_thing(monster* mons, bool quiet)
{
    if (mons->holiness() == MH_NATURAL && mons_can_be_zombified(mons))
    {
        const monster_type spectre_type = mons_species(mons->type);

        // Headless hydras cannot be made spectral hydras, sorry.
        if (spectre_type == MONS_HYDRA && mons->number == 0)
        {
            mprf("A glowing mist gathers momentarily, then fades.");
            return;
        }

        // Use the original monster type as the zombified type here, to
        // get the proper stats from it.
        const int spectre =
            create_monster(
                mgen_data(MONS_SPECTRAL_THING, BEH_FRIENDLY, &you,
                    0, 0, mons->pos(), MHITYOU,
                    0, static_cast<god_type>(you.attribute[ATTR_DIVINE_DEATH_CHANNEL]),
                    mons->type, mons->number));

        if (spectre != -1)
        {
            if (!quiet)
                mpr("A glowing mist starts to gather...");

            // If the original monster has been drained or levelled up,
            // its HD might be different from its class HD, in which
            // case its HP should be rerolled to match.
            if (menv[spectre].hit_dice != mons->hit_dice)
            {
                menv[spectre].hit_dice = std::max(mons->hit_dice, 1);
                roll_zombie_hp(&menv[spectre]);
            }

            name_zombie(&menv[spectre], mons);
        }
    }
}

// Returns the slot of a possibly generated corpse or -1.
int monster_die(monster* mons, killer_type killer,
                int killer_index, bool silent, bool wizard, bool fake)
{
    if (invalid_monster(mons))
        return (-1);

    const bool was_visible = you.can_see(mons);

    // If a monster was banished to the Abyss and then killed there,
    // then its death wasn't a banishment.
    if (you.level_type == LEVEL_ABYSS)
        mons->flags &= ~MF_BANISHED;

    if (!silent && !fake
        && _monster_avoided_death(mons, killer, killer_index))
    {
        mons->flags &= ~MF_EXPLODE_KILL;
        return (-1);
    }

    // If the monster was calling the tide, let go now.
    mons->del_ench(ENCH_TIDE);

    // Same for silencers.
    mons->del_ench(ENCH_SILENCE);

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
            lua_pushnumber(dlua, killer_index);
            lua_pushboolean(dlua, silent);
            lua_pushboolean(dlua, wizard);
            dlua.callfn(NULL, 5, 0);
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

    you.remove_beholder(mons);
    you.remove_fearmonger(mons);

    // Clear auto exclusion now the monster is killed - if we know about it.
    if (mons_near(mons) || wizard)
        remove_auto_exclude(mons);

          int  summon_type   = 0;
          int  duration      = 0;
    const bool summoned      = mons->is_summoned(&duration, &summon_type);
    const int monster_killed = mons->mindex();
    const bool hard_reset    = testbits(mons->flags, MF_HARD_RESET);
    const bool gives_xp      = (!summoned && !mons_class_flag(mons->type,
                                                              M_NO_EXP_GAIN));

    bool drop_items    = !hard_reset;

    const bool mons_reset(killer == KILL_RESET || killer == KILL_DISMISSED);

    const bool submerged     = mons->submerged();

    bool in_transit          = false;

    if (!crawl_state.game_is_arena())
        _check_kill_milestone(mons, killer, killer_index);

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

    // Take note!
    if (!mons_reset && !crawl_state.game_is_arena() && MONST_INTERESTING(mons))
    {
        take_note(Note(NOTE_KILL_MONSTER,
                       mons->type, mons->friendly(),
                       mons->full_name(DESC_NOCAP_A).c_str()));
    }

    // From time to time Trog gives you a little bonus.
    if (killer == KILL_YOU && you.berserk())
    {
        if (you.religion == GOD_TROG
            && !player_under_penance() && you.piety > random2(1000))
        {
            const int bonus = (3 + random2avg(10, 2)) / 2;

            you.increase_duration(DUR_BERSERK, bonus);

            mpr("You feel the power of Trog in you as your rage grows.",
                MSGCH_GOD, GOD_TROG);
        }
        else if (wearing_amulet(AMU_RAGE) && one_chance_in(30))
        {
            const int bonus = (2 + random2(4)) / 2;;

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
        || mons->type == MONS_BALL_LIGHTNING)
    {
        did_death_message =
            _spore_goes_pop(mons, killer, killer_index, pet_kill, wizard);
    }
    else if (mons->type == MONS_FIRE_VORTEX
             || mons->type == MONS_SPATIAL_VORTEX)
    {
        if (!silent && !mons_reset)
        {
            simple_monster_message(mons, " dissipates!",
                                   MSGCH_MONSTER_DAMAGE, MDAM_DEAD);
            silent = true;
        }

        if (mons->type == MONS_FIRE_VORTEX && !wizard && !mons_reset
            && !submerged)
        {
            place_cloud(CLOUD_FIRE, mons->pos(), 2 + random2(4),
                        mons->kill_alignment(), KILL_MON_MISSILE);
        }

        if (killer == KILL_RESET)
            killer = KILL_DISMISSED;
    }
    else if (mons->type == MONS_SIMULACRUM_SMALL
             || mons->type == MONS_SIMULACRUM_LARGE)
    {
        if (!silent && !mons_reset)
        {
            simple_monster_message(mons, " vapourises!",
                                   MSGCH_MONSTER_DAMAGE,  MDAM_DEAD);
            silent = true;
        }

        if (!wizard && !mons_reset && !submerged)
        {
            place_cloud(CLOUD_COLD, mons->pos(), 2 + random2(4),
                        mons->kill_alignment(), KILL_MON_MISSILE);
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

        if (!silent && !hard_reset)
        {
            int w_idx = mons->inv[MSLOT_WEAPON];
            if (w_idx != NON_ITEM && !(mitm[w_idx].flags & ISFLAG_SUMMONED))
            {
                simple_monster_message(mons, " falls from the air.",
                                       MSGCH_MONSTER_DAMAGE, MDAM_DEAD);
                silent = true;
            }
            else
                killer = KILL_RESET;
        }
    }
    else if (mons->type == MONS_ELDRITCH_TENTACLE)
    {
        if (!silent && !mons_reset && !mons->has_ench(ENCH_SEVERED))
        {
            mpr("With a roar, the tentacle is hauled back through the portal!",
                MSGCH_MONSTER_DAMAGE, MDAM_DEAD);
            silent = true;
        }

        if (killer == KILL_RESET)
            killer = KILL_DISMISSED;
    }

    const bool death_message = !silent && !did_death_message
                               && mons_near(mons)
                               && mons->visible_to(&you);
    const bool exploded      = mons->flags & MF_EXPLODE_KILL;

    const bool created_friendly = testbits(mons->flags, MF_NO_REWARD);
          bool anon = (killer_index == ANON_FRIENDLY_MONSTER);
    const mon_holy_type targ_holy = mons->holiness();

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
                    && (targ_holy == MH_NATURAL || targ_holy == MH_HOLY));

            if (death_message)
            {
                if (killer == KILL_YOU_CONF
                    && (anon || !invalid_monster_index(killer_index)))
                {
                    mprf(MSGCH_MONSTER_DAMAGE, MDAM_DEAD, "%s is %s!",
                         mons->name(DESC_CAP_THE).c_str(),
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
                         mons->name(DESC_NOCAP_THE).c_str());
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

                    if (mons->is_unholy())
                    {
                        did_god_conduct(DID_KILL_NATURAL_UNHOLY,
                                        mons->hit_dice, true, mons);
                    }

                    if (mons->is_evil())
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

                if (mons->is_chaotic())
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

                if (mons_is_slime(mons))
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
                && (you.religion == GOD_MAKHLEB
                    || you.religion == GOD_SHINING_ONE
                       && (mons->is_evil() || mons->is_unholy()))
                && !player_under_penance()
                && random2(you.piety) >= piety_breakpoint(0)
                && !you.duration[DUR_DEATHS_DOOR])
            {
                if (you.hp < you.hp_max)
                {
                    mpr("You feel a little better.");
                    inc_hp(mons->hit_dice + random2(mons->hit_dice),
                           false);
                }
            }

            if (good_kill
                && (you.religion == GOD_MAKHLEB
                    || you.religion == GOD_VEHUMET
                    || you.religion == GOD_SHINING_ONE
                       && (mons->is_evil() || mons->is_unholy()))
                && !player_under_penance()
                && random2(you.piety) >= piety_breakpoint(0))
            {
                if (you.magic_points < you.max_magic_points)
                {
                    mpr("You feel your power returning.");
                    inc_mp(1 + random2(mons->hit_dice / 2), false);
                }
            }

            // Randomly bless a follower.
            if (!created_friendly
                && gives_xp
                && (you.religion == GOD_BEOGH
                    && random2(you.piety) >= piety_breakpoint(2))
                && !player_under_penance())
            {
                bless_follower();
            }

            if (you.duration[DUR_DEATH_CHANNEL]
                && gives_xp)
            {
                _make_spectral_thing(mons, !death_message);
            }
            break;
        }

        case KILL_MON:          // Monster kills in combat.
        case KILL_MON_MISSILE:  // Monster kills by missile or beam.
            if (!silent)
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
            if (mons->friendly()
                && mons->type != MONS_DANCING_WEAPON)
            {
                const int mon_intel = mons_class_intel(mons->type) - I_ANIMAL;

                if (mon_intel > 0)
                    did_god_conduct(DID_SOULED_FRIEND_DIED, 1 + (mons->hit_dice / 2),
                                    true, mons);
                else
                    did_god_conduct(DID_FRIEND_DIED, 1 + (mons->hit_dice / 2),
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

                if (you.religion == GOD_ZIN
                    || you.religion == GOD_SHINING_ONE
                    || you.religion == GOD_YREDELEMNUL
                    || you.religion == GOD_KIKUBAAQUDGHA
                    || you.religion == GOD_VEHUMET
                    || you.religion == GOD_MAKHLEB
                    || you.religion == GOD_LUGONU
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
                                      mons->hit_dice);
                    }
                    else
                        notice |= did_god_conduct(DID_HOLY_KILLED_BY_SERVANT,
                                                  mons->hit_dice);
                }

                if (you.religion == GOD_VEHUMET
                    && notice
                    && !player_under_penance()
                    && random2(you.piety) >= piety_breakpoint(0))
                {
                    // Vehumet - only for non-undead servants (coding
                    // convenience, no real reason except that Vehumet
                    // prefers demons).
                    if (you.magic_points < you.max_magic_points)
                    {
                        mpr("You feel your power returning.");
                        inc_mp(1 + random2(mons->hit_dice / 2), false);
                    }
                }

                if (you.religion == GOD_SHINING_ONE
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

                if (you.religion == GOD_BEOGH
                    && random2(you.piety) >= piety_breakpoint(2)
                    && !player_under_penance()
                    && !one_chance_in(3)
                    && !invalid_monster_index(killer_index))
                {
                    // Randomly bless the follower who killed.
                    bless_follower(killer_mon);
                }
            }
            break;

        // Monster killed by trap/inanimate thing/itself/poison not from you.
        case KILL_MISC:
            if (!silent)
            {
                const char* msg =
                    exploded                     ? " is blown up!" :
                    _wounded_damaged(targ_holy)  ? " is destroyed!"
                                                 : " dies!";
                simple_monster_message(mons, msg, MSGCH_MONSTER_DAMAGE,
                                       MDAM_DEAD);
            }
            break;

        case KILL_RESET:
            // Monster doesn't die, just goes back to wherever it came from.
            // This must only be called by monsters running out of time (or
            // abjuration), because it uses the beam variables!  Or does it???
            // Pacified monsters leave the level when this happens.

            // KILL_RESET monsters no longer lose their whole inventory, only
            // items they were generated with.
            if (mons->pacified() || !mons->needs_transit())
            {
                // A banished monster that doesn't go on the transit list
                // loses all items.
                if (!mons->is_summoned())
                    drop_items = false;
                break;
            }

            // Monster goes to the Abyss.
            mons->flags |= MF_BANISHED;
            mons->set_transit(level_id(LEVEL_ABYSS));
            in_transit = true;
            drop_items = false;
            // Make monster stop patrolling and/or travelling.
            mons->patrol_point.reset();
            mons->travel_path.clear();
            mons->travel_target = MTRAV_NONE;
            break;

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

    if (!silent && !wizard && you.see_cell(mons->pos()))
    {
        // Make sure that the monster looks dead.
        if (mons->alive() && !in_transit && (!summoned || duration > 0))
            mons->hit_points = -1;
        mons_speaks(mons);
    }

    if (mons->type == MONS_BORIS && !in_transit)
    {
        // XXX: Actual blood curse effect for Boris? - bwr

        // Now that Boris is dead, he's a valid target for monster
        // creation again. - bwr
        you.unique_creatures[mons->type] = false;
        // And his vault can be placed again.
        you.uniq_map_names.erase("uniq_boris");
    }
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
    else if (mons_base_type(mons) == MONS_KRAKEN)
    {
        if (_destroy_tentacles(mons) && !in_transit)
        {
            mpr("The kraken is slain, and its tentacles slide back "
                "into the water like the carrion they now are.");
        }
    }
    else if ((mons->type == MONS_KRAKEN_TENTACLE_SEGMENT
                  || mons->type == MONS_KRAKEN_TENTACLE)
              && killer != KILL_MISC)
    {
        int t_idx = mons->type == MONS_KRAKEN_TENTACLE
                    ? mons->mindex() : mons->number;
        if (_destroy_tentacle(t_idx, mons) && !in_transit)
        {
            //mprf("A tentacle died?");
        }

    }
    else if (mons->type == MONS_ELDRITCH_TENTACLE)
    {
        for (monster_iterator mit; mit; ++mit)
        {
            if (mit->alive()
                && mit->type == MONS_ELDRITCH_TENTACLE_SEGMENT
                && mit->number == unsigned(mons->mindex()))
            {
                monster_die(*mit, KILL_MISC, NON_MONSTER, true);
            }
        }
        if (mons->has_ench(ENCH_PORTAL_TIMER))
        {
            coord_def base_pos = mons->props["base_position"].get_coord();

            if (env.grid(base_pos) == DNGN_TEMP_PORTAL)
            {
                env.grid(base_pos) = DNGN_FLOOR;
            }
        }
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
    {
        elven_twin_died(mons, in_transit, killer, killer_index);
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

    if (!wizard && !submerged)
        _monster_die_cloud(mons, !mons_reset, silent, summoned);

    int corpse = -1;
    if (!mons_reset && !summoned)
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
             && corpse > 0 && player_mutation_level(MUT_POWERED_BY_DEATH))
    {
        const int pbd_dur = player_mutation_level(MUT_POWERED_BY_DEATH) * 8
                            + roll_dice(2, 8);
        if (pbd_dur > you.duration[DUR_POWERED_BY_DEATH])
            you.set_duration(DUR_POWERED_BY_DEATH, pbd_dur);
    }

    unsigned int player_exp = 0, monster_exp = 0;
    if (!mons_reset)
    {
        player_exp = _calc_player_experience(mons, killer, pet_kill,
                                             killer_index);
        monster_exp = _calc_monster_experience(mons, killer, killer_index);
    }

    if (!mons_reset && !crawl_state.game_is_arena())
        you.kills->record_kill(mons, killer, pet_kill);

    if (fake)
    {
        _give_experience(player_exp, monster_exp, killer, killer_index,
                         pet_kill, was_visible);
        crawl_state.dec_mon_acting(mons);
        return (corpse);
    }

    mons_remove_from_grid(mons);
    _fire_monster_death_event(mons, killer, killer_index, false);

    if (crawl_state.game_is_arena())
        arena_monster_died(mons, killer, killer_index, silent, corpse);

    // Monsters haloes should be removed when they die.
    if (mons->holiness() == MH_HOLY)
        invalidate_agrid();
    // Likewise silence
    if (mons->type == MONS_SILENT_SPECTRE)
        invalidate_agrid();

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
        && !(mons->flags & MF_KNOWN_MIMIC)
        && mons->is_shapeshifter())
    {
        simple_monster_message(mons, "'s shape twists and changes as "
                               "it dies.");
    }

    // If we kill an invisible monster reactivate autopickup.
    // We need to check for actual invisibility rather than whether we
    // can see the monster. There are several edge cases where a monster
    // is visible to the player but we still need to turn autopickup
    // back on, such as TSO's halo or sticky flame. (jpeg)
    if (mons_near(mons) && mons->has_ench(ENCH_INVIS))
        autotoggle_autopickup(false);

    crawl_state.dec_mon_acting(mons);
    monster_cleanup(mons);

    // Force redraw for monsters that die.
    if (in_bounds(mwhere) && you.see_cell(mwhere))
    {
        view_update_at(mwhere);
        update_screen();
    }

    _give_experience(player_exp, monster_exp, killer, killer_index,
                     pet_kill, was_visible);

    return (corpse);
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
    for (monster_iterator mi(you.get_los()); mi; ++mi)
        if (!mi->asleep())
             behaviour_event(*mi, ME_ALERT, MHITYOU);
}

static bool _valid_morph(monster* mons, monster_type new_mclass)
{
    const dungeon_feature_type current_tile = grd(mons->pos());

    // 'morph targets are _always_ "base" classes, not derived ones.
    new_mclass = mons_species(new_mclass);

    // [ds] Non-base draconians are much more trouble than their HD
    // suggests.
    if (mons_genus(new_mclass) == MONS_DRACONIAN
        && new_mclass != MONS_DRACONIAN
        && !player_in_branch(BRANCH_HALL_OF_ZOT)
        && !one_chance_in(10))
    {
        return (false);
    }

    // Various inappropriate polymorph targets.
    if (mons_class_holiness(new_mclass) != mons->holiness()
        || mons_class_flag(new_mclass, M_NO_POLY_TO)  // explicitly disallowed
        || mons_class_flag(new_mclass, M_UNIQUE)      // no uniques
        || mons_class_flag(new_mclass, M_NO_EXP_GAIN) // not helpless
        || new_mclass == mons_species(mons->type)  // must be different
        || new_mclass == MONS_PROGRAM_BUG

        // These require manual setting of mons.base_monster to indicate
        // what they are a skeleton/zombie/simulacrum/spectral thing of,
        // which we currently aren't smart enough to handle.
        || mons_class_is_zombified(new_mclass)

        // These require manual setting of the ghost demon struct to
        // indicate their characteristics, which we currently aren't
        // smart enough to handle.
        || mons_is_ghost_demon(new_mclass)

        // Other poly-unsuitable things.
        || mons_is_statue(new_mclass)
        || mons_is_projectile(new_mclass)

        // The spell on Prince Ribbit can't be broken so easily.
        || (new_mclass == MONS_HUMAN && mons->type == MONS_PRINCE_RIBBIT))
    {
        return (false);
    }

    // Determine if the monster is happy on current tile.
    return (monster_habitable_grid(new_mclass, current_tile));
}

static bool _is_poly_power_unsuitable(poly_power_type power,
                                       int src_pow, int tgt_pow, int relax)
{
    switch (power)
    {
    case PPT_LESS:
        return (tgt_pow > src_pow - 3 + (relax * 3) / 2)
                || (power == PPT_LESS && (tgt_pow < src_pow - (relax / 2)));
    case PPT_MORE:
        return (tgt_pow < src_pow + 2 - relax)
                || (power == PPT_MORE && (tgt_pow > src_pow + relax));
    default:
    case PPT_SAME:
        return (tgt_pow < src_pow - relax)
                || (tgt_pow > src_pow + (relax * 3) / 2);
    }
}

// If targetc == RANDOM_MONSTER, then relpower indicates the desired
// power of the new monster, relative to the current monster.
// Relaxation still takes effect when needed, no matter what relpower
// says.
bool monster_polymorph(monster* mons, monster_type targetc,
                       poly_power_type power,
                       bool force_beh)
{
    ASSERT(!(mons->flags & MF_TAKING_STAIRS));
    ASSERT(!(mons->flags & MF_BANISHED) || you.level_type == LEVEL_ABYSS);

    std::string str_polymon;
    int source_power, target_power, relax;
    int tries = 1000;

    // Used to be mons_power, but that just returns hit_dice
    // for the monster class.  By using the current hit dice
    // the player gets the opportunity to use draining more
    // effectively against shapeshifters. - bwr
    source_power = mons->hit_dice;
    relax = 1;

    if (targetc == RANDOM_MONSTER)
    {
        do
        {
            // Pick a monster that's guaranteed happy at this grid.
            targetc = random_monster_at_grid(mons->pos());

            // Valid targets are always base classes ([ds] which is unfortunate
            // in that well-populated monster classes will dominate polymorphs).
            targetc = mons_species(targetc);

            target_power = mons_power(targetc);

            if (one_chance_in(200))
                relax++;

            if (relax > 50)
                return (simple_monster_message(mons, " shudders."));
        }
        while (tries-- && (!_valid_morph(mons, targetc)
                           || _is_poly_power_unsuitable(power, source_power,
                                                        target_power, relax)));
    }

    if (!_valid_morph(mons, targetc))
    {
        return (simple_monster_message(mons,
                                       " looks momentarily different."));
    }

    // Messaging.
    bool can_see     = you.can_see(mons);
    bool can_see_new = !mons_class_flag(targetc, M_INVIS) || you.can_see_invisible();

    bool need_note = false;
    std::string old_name = mons->full_name(DESC_CAP_A);

    // If old monster is visible to the player, and is interesting,
    // then note why the interesting monster went away.
    if (can_see && MONST_INTERESTING(mons))
        need_note = true;

    std::string new_name = "";
    if (mons->type == MONS_OGRE && targetc == MONS_TWO_HEADED_OGRE)
        str_polymon = " grows a second head";
    else
    {
        if (mons->is_shapeshifter())
            str_polymon = " changes into ";
        else if (targetc == MONS_PULSATING_LUMP)
            str_polymon = " degenerates into ";
        else if (you.religion == GOD_JIYVA
                 && (targetc == MONS_DEATH_OOZE
                     || targetc == MONS_OOZE
                     || targetc == MONS_JELLY
                     || targetc == MONS_BROWN_OOZE
                     || targetc == MONS_SLIME_CREATURE
                     || targetc == MONS_GIANT_AMOEBA
                     || targetc == MONS_ACID_BLOB
                     || targetc == MONS_AZURE_JELLY))
        {
            // Message used for the Slimify ability.
            str_polymon = " quivers uncontrollably and liquefies into ";
        }
        else
            str_polymon = " evaporates and reforms as ";

        if (!can_see_new)
        {
            new_name = "something unseen";
            str_polymon += "something you cannot see";
        }
        else
        {
            str_polymon += mons_type_name(targetc, DESC_NOCAP_A);

            if (targetc == MONS_PULSATING_LUMP)
                str_polymon += " of flesh";
        }
    }
    str_polymon += "!";

    bool player_messaged = can_see
                       && simple_monster_message(mons, str_polymon.c_str());

    // Even if the monster transforms from one type that can behold the
    // player into a different type which can also behold the player,
    // the polymorph disrupts the beholding process.  Do this before
    // changing mons->type, since unbeholding can only happen while
    // the monster is still a mermaid/siren.
    you.remove_beholder(mons);
    you.remove_fearmonger(mons);

    if (mons->type == MONS_KRAKEN)
        _destroy_tentacles(mons);

    // Inform listeners that the original monster is gone.
    _fire_monster_death_event(mons, KILL_MISC, NON_MONSTER, true);

    // the actual polymorphing:
    uint64_t flags =
        mons->flags & ~(MF_INTERESTING | MF_SEEN | MF_ATT_CHANGE_ATTEMPT
                           | MF_WAS_IN_VIEW | MF_BAND_MEMBER | MF_KNOWN_MIMIC);

    std::string name;

    // Preserve the names of uniques and named monsters.
    if (!mons->mname.empty())
        name = mons->mname;
    else if (mons_is_unique(mons->type))
    {
        flags |= MF_INTERESTING;

        name = mons->name(DESC_PLAIN, true);
        if (mons->type == MONS_ROYAL_JELLY)
        {
            name   = "shaped Royal Jelly";
            flags |= MF_NAME_SUFFIX;
        }
        else if (mons->type == MONS_LERNAEAN_HYDRA)
        {
            name   = "shaped Lernaean hydra";
            flags |= MF_NAME_SUFFIX;
        }

        // "Blork the orc" and similar.
        const size_t the_pos = name.find(" the ");
        if (the_pos != std::string::npos)
            name = name.substr(0, the_pos);
    }

    const monster_type real_targetc =
        (mons->has_ench(ENCH_GLOWING_SHAPESHIFTER)) ? MONS_GLOWING_SHAPESHIFTER :
        (mons->has_ench(ENCH_SHAPESHIFTER))         ? MONS_SHAPESHIFTER
                                                       : targetc;

    const god_type god =
        (player_will_anger_monster(real_targetc)
            || (you.religion == GOD_BEOGH
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
    // See also _check_kill_milestone.
    bool old_mon_unique           = mons_is_unique(mons->type);
    if (mons->props.exists("original_was_unique"))
        if (mons->props["original_was_unique"].get_bool())
            old_mon_unique = true;

    mon_enchant abj       = mons->get_ench(ENCH_ABJ);
    mon_enchant charm     = mons->get_ench(ENCH_CHARM);
    mon_enchant temp_pacif= mons->get_ench(ENCH_TEMP_PACIF);
    mon_enchant shifter   = mons->get_ench(ENCH_GLOWING_SHAPESHIFTER,
                                              ENCH_SHAPESHIFTER);
    mon_enchant sub       = mons->get_ench(ENCH_SUBMERGED);
    mon_enchant summon    = mons->get_ench(ENCH_SUMMON);
    mon_enchant tp        = mons->get_ench(ENCH_TP);

    monster_spells spl    = mons->spells;
    const bool need_save_spells
            = (!name.empty()
               && (!mons->can_use_spells()
                   || mons->is_actual_spellcaster()));

    // deal with mons_sec
    mons->type         = targetc;
    mons->base_monster = MONS_NO_MONSTER;
    mons->number       = 0;

    // Note: define_monster() will clear out all enchantments! - bwr
    define_monster(mons);

    mons->mname = name;
    mons->props["original_name"] = name;
    mons->props["original_was_unique"] = old_mon_unique;
    mons->god   = god;

    mons->flags = flags;
    // Line above might clear melee and/or spell flags; restore.
    mons->bind_melee_flags();
    mons->bind_spell_flags();

    // Forget various speech/shout Lua functions.
    mons->props.erase("speech_key");
    mons->props.erase("speech_prefix");
    mons->props.erase("speech_func");
    mons->props.erase("shout_func");

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
    mons->add_ench(charm);
    mons->add_ench(temp_pacif);
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

    if (!player_messaged && you.can_see(mons))
    {
        mprf("%s appears out of thin air!", mons->name(DESC_CAP_A).c_str());
        autotoggle_autopickup(false);
        player_messaged = true;
    }

    mons->hit_points = mons->max_hit_points
                                * ((old_hp * 100) / old_hp_max) / 100
                                + random2(mons->max_hit_points);

    mons->hit_points = std::min(mons->max_hit_points,
                                   mons->hit_points);

    // Don't kill it.
    mons->hit_points = std::max(mons->hit_points, 1);

    mons->speed_increment = 67 + random2(6);

    monster_drop_things(mons);

    // New monster type might be interesting.
    mark_interesting_monst(mons);
    if (new_name.empty())
        new_name = mons->full_name(DESC_NOCAP_A);

    if (need_note
        || can_see && you.can_see(mons) && MONST_INTERESTING(mons))
    {
        take_note(Note(NOTE_POLY_MONSTER, 0, 0, old_name.c_str(),
                       new_name.c_str()));

        if (you.can_see(mons))
            mons->flags |= MF_SEEN;
    }

    // If new monster is visible to player, then we've seen it.
    if (you.can_see(mons))
    {
        seen_monster(mons);
        // If the player saw both the beginning and end results of a
        // shifter changing, then s/he knows it must be a shifter.
        if (can_see && shifter.ench != ENCH_NONE)
            mons->flags |= MF_KNOWN_MIMIC;
    }

    if (old_mon_caught)
        check_net_will_hold_monster(mons);

    if (!force_beh)
        player_angers_monster(mons);

    // Xom likes watching monsters being polymorphed.
    xom_is_stimulated(mons->is_shapeshifter()               ? 16 :
                      power == PPT_LESS || mons->friendly() ? 32 :
                      power == PPT_SAME                           ? 64 : 128);

    return (player_messaged);
}

// If the returned value is mon.pos(), then nothing was found.
static coord_def _random_monster_nearby_habitable_space(const monster& mon,
                                                        bool allow_adjacent,
                                                        bool respect_los)
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

        if (delta.rdist() == 1 && !allow_adjacent)
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

        // Check that we didn't go through clear walls.
        if (num_feats_between(mon.pos(), target,
                              DNGN_MINSEE,
                              DNGN_MAX_NONREACH,
                              true, true) > 0)
        {
            continue;
        }

        if (respect_los && !mon.see_cell(target))
            continue;

        // Survived everything, break out (with a good value of target.)
        break;
    }

    if (tries == 150)
        target = mon.pos();

    return (target);
}

bool monster_blink(monster* mons, bool quiet)
{
    coord_def near = _random_monster_nearby_habitable_space(*mons, false,
                                                            true);

    return (mons->blink_to(near, quiet));
}

bool mon_can_be_slimified(monster* mons)
{
    const mon_holy_type holi = mons->holiness();

    return (!(mons->flags & MF_GOD_GIFT)
            && !mons->is_insubstantial()
            && (holi == MH_UNDEAD
                 || holi == MH_NATURAL && !mons_is_slime(mons))
          );
}

void slimify_monster(monster* mon, bool hostile)
{
    if (mon->holiness() == MH_UNDEAD)
        monster_polymorph(mon, MONS_DEATH_OOZE);
    else
    {
        const int x = mon->hit_dice + (coinflip() ? 1 : -1) * random2(5);

        if (x < 3)
            monster_polymorph(mon, MONS_OOZE);
        else if (x >= 3 && x < 5)
            monster_polymorph(mon, MONS_JELLY);
        else if (x >= 5 && x < 7)
            monster_polymorph(mon, MONS_BROWN_OOZE);
        else if (x >= 7 && x <= 11)
        {
            if (coinflip())
                monster_polymorph(mon, MONS_SLIME_CREATURE);
            else
                monster_polymorph(mon, MONS_GIANT_AMOEBA);
        }
        else
        {
            if (coinflip())
                monster_polymorph(mon, MONS_ACID_BLOB);
            else
                monster_polymorph(mon, MONS_AZURE_JELLY);
        }
    }

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

void corrode_monster(monster* mons)
{
    item_def *has_shield = mons->mslot_item(MSLOT_SHIELD);
    item_def *has_armour = mons->mslot_item(MSLOT_ARMOUR);

    if (!one_chance_in(3) && (has_shield || has_armour))
    {
        item_def &thing_chosen = (has_armour ? *has_armour : *has_shield);
        if (is_artefact(thing_chosen)
           || (get_equip_race(thing_chosen) == ISFLAG_DWARVEN
              && one_chance_in(5)))
        {
            return;
        }
        else
        {
            // same formula as for players
            bool resists = false;
            int enchant = abs(thing_chosen.plus);

            if (enchant >= 0 && enchant <= 4)
                resists = x_chance_in_y(2 + (4 << enchant) + enchant * 8, 100);
            else
                resists = true;

            if (!resists)
            {
                thing_chosen.plus--;
                mons->ac--;
                if (you.can_see(mons))
                {
                    mprf("The acid corrodes %s %s!",
                         apostrophise(mons->name(DESC_NOCAP_THE)).c_str(),
                         thing_chosen.name(DESC_PLAIN).c_str());
                }
            }
        }
    }
    else if (!one_chance_in(3) && !(has_shield || has_armour)
             && mons->can_bleed() && !mons->res_acid())
    {
                mons->add_ench(mon_enchant(ENCH_BLEED, 3, KC_OTHER, (5 + random2(5))*10));

                if (you.can_see(mons))
                {
                        mprf("%s writhes in agony as %s flesh is eaten away!",
                             mons->name(DESC_CAP_THE).c_str(),
                             mons->pronoun(PRONOUN_NOCAP_POSSESSIVE).c_str());
            }
        }
}

static bool _habitat_okay(const monster* mons, dungeon_feature_type targ)
{
    return (monster_habitable_grid(mons, targ));
}

// This doesn't really swap places, it just sets the monster's
// position equal to the player (the player has to be moved afterwards).
// It also has a slight problem with the fact that if the player is
// levitating over an inhospitable habitat for the monster the monster
// will be put in a place it normally couldn't go (this could be a
// feature because it prevents insta-killing).  In order to prevent
// that little problem, we go looking for a square for the monster
// to "scatter" to instead... and if we can't find one the monster
// just refuses to be swapped (not a bug, this is intentionally
// avoiding the insta-kill).  Another option is to look a bit
// wider for a vaild square (either by a last attempt blink, or
// by looking at a wider radius)...  insta-killing should be a
// last resort in this function (especially since Tome, Dig, and
// Summoning can be used to set up death traps).  If worse comes
// to worse, at least consider making the Swap spell not work
// when the player is over lava or water (if the player wants to
// swap pets to their death, we can let that go). - bwr
bool swap_places(monster* mons)
{
    coord_def loc;
    if (swap_check(mons, loc))
    {
        swap_places(mons, loc);
        return true;
    }
    return false;
}

// Swap monster to this location.  Player is swapped elsewhere.
bool swap_places(monster* mons, const coord_def &loc)
{
    ASSERT(map_bounds(loc));
    ASSERT(_habitat_okay(mons, grd(loc)));

    if (monster_at(loc))
    {
        mpr("Something prevents you from swapping places.");
        return (false);
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

    // Don't move onto dangerous terrain.
    if (is_feat_dangerous(grd(mons->pos())) && !you.can_cling_to(mons->pos()))
    {
        canned_msg(MSG_UNTHINKING_ACT);
        return (false);
    }

    if (mons_is_projectile(mons->type))
    {
        if (!quiet)
            mpr("It's unwise to walk into this.");
        return (false);
    }

    if (mons->caught())
    {
        if (!quiet)
            simple_monster_message(mons, " is held in a net!");
        return (false);
    }

    // First try: move monster onto your position.
    bool swap = !monster_at(loc) && _habitat_okay(mons, grd(loc));

    // Choose an appropriate habitat square at random around the target.
    if (!swap)
    {
        int num_found = 0;

        for (adjacent_iterator ai(mons->pos()); ai; ++ai)
            if (!monster_at(*ai) && _habitat_okay(mons, grd(*ai))
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
        simple_monster_message(mons, " resists.");
        // FIXME: AI_HIT_MONSTER isn't ideal.
        interrupt_activity(AI_HIT_MONSTER, mons);
    }

    return (swap);
}

// Given an adjacent monster, returns true if the monster can hit it
// (the monster should not be submerged, be submerged in shallow water
// if the monster has a polearm, or be submerged in anything if the
// monster has tentacles).
bool monster_can_hit_monster(monster* mons, const monster* targ)
{
    if (!targ->submerged() || mons->has_damage_type(DVORP_TENTACLE))
        return (true);

    if (grd(targ->pos()) != DNGN_SHALLOW_WATER)
        return (false);

    const item_def *weapon = mons->weapon();
    return (weapon && weapon_skill(*weapon) == SK_POLEARMS);
}

mon_dam_level_type mons_get_damage_level(const monster* mons)
{
    if (!mons_can_display_wounds(mons)
        || !mons_class_can_display_wounds(mons->get_mislead_type())
        || mons_is_unknown_mimic(mons))
    {
        return MDAM_OKAY;
    }

    if (mons->hit_points <= mons->max_hit_points / 6)
        return MDAM_ALMOST_DEAD;
    else if (mons->hit_points <= mons->max_hit_points / 4)
        return MDAM_SEVERELY_DAMAGED;
    else if (mons->hit_points <= mons->max_hit_points / 3)
        return MDAM_HEAVILY_DAMAGED;
    else if (mons->hit_points <= mons->max_hit_points * 3 / 4)
        return MDAM_MODERATELY_DAMAGED;
    else if (mons->hit_points < mons->max_hit_points)
        return MDAM_LIGHTLY_DAMAGED;
    else
        return MDAM_OKAY;
}

std::string get_damage_level_string(mon_holy_type holi,
                                    mon_dam_level_type mdam)
{
    std::ostringstream ss;
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

std::string get_wounds_description_sentence(const monster* mons)
{
    const std::string wounds = get_wounds_description(mons);
    if (wounds.empty())
        return "";
    else
        return mons->pronoun(PRONOUN_CAP) + " is " + wounds + ".";
}

std::string get_wounds_description(const monster* mons, bool colour)
{
    if (!mons->alive() || mons->hit_points == mons->max_hit_points)
        return "";

    if (!mons_can_display_wounds(mons))
        return "";

    mon_dam_level_type dam_level = mons_get_damage_level(mons);
    std::string desc = get_damage_level_string(mons->holiness(), dam_level);
    if (colour)
    {
        const int col = channel_to_colour(MSGCH_MONSTER_DAMAGE, dam_level);
        desc = colour_string(desc, col);
    }
    return desc;
}

void print_wounds(const monster* mons)
{
    if (!mons->alive() || mons->hit_points == mons->max_hit_points)
        return;

    if (!mons_can_display_wounds(mons))
        return;

    mon_dam_level_type dam_level = mons_get_damage_level(mons);
    std::string desc = get_damage_level_string(mons->holiness(), dam_level);

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
    return (holi == MH_UNDEAD || holi == MH_NONLIVING || holi == MH_PLANT);
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
            _mons_indicate_level_exit(mon);

        // Pacified monsters leaving the level take their stuff with
        // them.
        mon->flags |= MF_HARD_RESET;
        monster_die(mon, KILL_DISMISSED, NON_MONSTER);
    }
}

// Checks whether there is a straight path from p1 to p2 that passes
// through features >= allowed.
// If it exists, such a path may be missed; on the other hand, it
// is not guaranteed that p2 is visible from p1 according to LOS rules.
// Not symmetric.
// FIXME: This is used for monster movement. It should instead be
//        something like exists_ray(p1, p2, opacity_monmove(mons)),
//        where opacity_monmove() is fixed to include opacity_immob.
bool can_go_straight(const coord_def& p1, const coord_def& p2,
                     dungeon_feature_type allowed)
{
    if (p1 == p2)
        return (true);

    if (distance(p1, p2) > get_los_radius_sq())
        return (false);

    // XXX: Hack to improve results for now. See FIXME above.
    ray_def ray;
    if (!find_ray(p1, p2, ray, opc_immob))
        return (false);

    dungeon_feature_type max_disallowed = DNGN_MAXOPAQUE;
    if (allowed != DNGN_UNSEEN)
        max_disallowed = static_cast<dungeon_feature_type>(allowed - 1);

    for (ray.advance(); ray.pos() != p2; ray.advance())
    {
        ASSERT(map_bounds(ray.pos()));
        dungeon_feature_type feat = env.grid(ray.pos());
        if (feat >= DNGN_UNSEEN && feat <= max_disallowed)
            return (false);
    }

    return (true);
}

// The default suitable() function for choose_random_nearby_monster().
bool choose_any_monster(const monster* mon)
{
    return (!mons_is_projectile(mon->type));
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
                                       bool in_sight, bool prefer_named,
                                       bool prefer_priest)
{
    return choose_random_monster_on_level(weight, suitable, in_sight, true,
                                          prefer_named, prefer_priest);
}

monster* choose_random_monster_on_level(int weight,
                                         bool (*suitable)(const monster* mon),
                                         bool in_sight, bool near_by,
                                         bool prefer_named, bool prefer_priest)
{
    monster* chosen = NULL;

    // A radius_iterator with radius == max(GXM, GYM) will sweep the
    // whole level.
    radius_iterator ri(you.pos(), near_by ? 9 : std::max(GXM, GYM),
                       true, in_sight);

    for (; ri; ++ri)
    {
        if (monster* mon = monster_at(*ri))
        {
            if (suitable(mon))
            {
                // FIXME: if the intent is to favour monsters
                // named by $DEITY, we should set a flag on the
                // monster (something like MF_DEITY_PREFERRED) and
                // use that instead of checking the name, given
                // that other monsters can also have names.

                // True, but it's currently only used for orcs, and
                // Blork and Urug also being preferred to non-named orcs
                // is fine, I think. Once more gods name followers (and
                // prefer them) that should be changed, of course. (jpeg)

                // Named or priestly monsters have doubled chances.
                int mon_weight = 1;

                if (prefer_named && mon->is_named())
                    mon_weight++;

                if (prefer_priest && mon->is_priest())
                    mon_weight++;

                if (x_chance_in_y(mon_weight, (weight += mon_weight)))
                    chosen = mon;
            }
        }
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
        std::string msg = mons->name(descrip);
        msg += event;
        msg = apostrophise_fixup(msg);

        if (channel == MSGCH_PLAIN && mons->wont_attack())
            channel = MSGCH_FRIEND_ACTION;

        mpr(msg.c_str(), channel, param);
        return (true);
    }

    return (false);
}

bool mons_avoids_cloud(const monster* mons, const cloud_struct& cloud,
                       bool placement)
{
    bool extra_careful = placement;
    cloud_type cl_type = cloud.type;

    if (placement)
        extra_careful = true;

    // Berserk monsters are less careful and will blindly plow through any
    // dangerous cloud, just to kill you. {due}
    if (!extra_careful && mons->berserk())
        return (false);

    switch (cl_type)
    {
    case CLOUD_MIASMA:
        // Even the dumbest monsters will avoid miasma if they can.
        return (!mons->res_rotting());

    case CLOUD_FIRE:
    case CLOUD_FOREST_FIRE:
        if (mons->res_fire() > 1)
            return (false);

        if (extra_careful)
            return (true);

        if (mons_intel(mons) >= I_ANIMAL && mons->res_fire() < 0)
            return (true);

        if (mons->hit_points >= 15 + random2avg(46, 5))
            return (false);
        break;

    case CLOUD_STINK:
        if (mons->res_poison() > 0)
            return (false);

        if (extra_careful)
            return (true);

        if (mons_intel(mons) >= I_ANIMAL && mons->res_poison() < 0)
            return (true);

        if (x_chance_in_y(mons->hit_dice - 1, 5))
            return (false);

        if (mons->hit_points >= random2avg(19, 2))
            return (false);
        break;

    case CLOUD_COLD:
        if (mons->res_cold() > 1)
            return (false);

        if (extra_careful)
            return (true);

        if (mons_intel(mons) >= I_ANIMAL && mons->res_cold() < 0)
            return (true);

        if (mons->hit_points >= 15 + random2avg(46, 5))
            return (false);
        break;

    case CLOUD_POISON:
        if (mons->res_poison() > 0)
            return (false);

        if (extra_careful)
            return (true);

        if (mons_intel(mons) >= I_ANIMAL && mons->res_poison() < 0)
            return (true);

        if (mons->hit_points >= random2avg(37, 4))
            return (false);
        break;

    case CLOUD_GREY_SMOKE:
        if (placement)
            return (false);

        // This isn't harmful, but dumb critters might think so.
        if (mons_intel(mons) > I_ANIMAL || coinflip())
            return (false);

        if (mons->res_fire() > 0)
            return (false);

        if (mons->hit_points >= random2avg(19, 2))
            return (false);
        break;

    case CLOUD_RAIN:
        // Fiery monsters dislike the rain.
        if (mons->is_fiery() && extra_careful)
            return (true);

        // We don't care about what's underneath the rain cloud if we can fly.
        if (mons->flight_mode() != FL_NONE)
            return (false);

        // These don't care about deep water.
        if (monster_habitable_grid(mons, DNGN_DEEP_WATER))
            return (false);

        // This position could become deep water, and they might drown.
        if (grd(cloud.pos) == DNGN_SHALLOW_WATER)
            return (true);

        // Otherwise, it's safe for everyone else.
        return (false);

        break;

    case CLOUD_TORNADO:
        // Ball lightnings are not afraid of a _storm_, duh.  Or elementals.
        if (mons->res_wind())
            return (false);

        // Locust swarms are too stupid to avoid winds.
        return (mons_intel(mons) >= I_ANIMAL);

    default:
        break;
    }

    // Exceedingly dumb creatures will wander into harmful clouds.
    if (is_harmless_cloud(cl_type)
        || mons_intel(mons) == I_PLANT && !extra_careful)
    {
        return (false);
    }

    // If we get here, the cloud is potentially harmful.
    return (true);
}

// Like the above, but allow a monster to move from one damaging cloud
// to another, even if they're of different types.
bool mons_avoids_cloud(const monster* mons, int cloud_num, bool placement)
{
    if (cloud_num == EMPTY_CLOUD)
        return (false);

    const cloud_struct &cloud = env.cloud[cloud_num];

    // Is the target cloud okay?
    if (!mons_avoids_cloud(mons, cloud, placement))
        return (false);

    // If we're already in a cloud that we'd want to avoid then moving
    // from one to the other is okay.
    if (!in_bounds(mons->pos()) || mons->pos() == cloud.pos)
        return (true);

    const int our_cloud_num = env.cgrid(mons->pos());

    if (our_cloud_num == EMPTY_CLOUD)
        return (true);

    const cloud_struct &our_cloud = env.cloud[our_cloud_num];

    return (!mons_avoids_cloud(mons, our_cloud, true));
}

// Returns a rough estimate of damage from throwing the wielded weapon.
int mons_thrown_weapon_damage(const item_def *weap,
                              bool only_returning_weapons)
{
    if (!weap ||
        (only_returning_weapons && get_weapon_brand(*weap) != SPWPN_RETURNING))
    {
        return (0);
    }

    return std::max(0, (property(*weap, PWPN_DAMAGE) + weap->plus2 / 2));
}

int mons_weapon_damage_rating(const item_def &launcher)
{
    return (property(launcher, PWPN_DAMAGE) + launcher.plus2);
}

// Returns a rough estimate of damage from firing/throwing missile.
int mons_missile_damage(monster* mons, const item_def *launch,
                        const item_def *missile)
{
    if (!missile || (!launch && !is_throwable(mons, *missile)))
        return (0);

    const int missile_damage = property(*missile, PWPN_DAMAGE) / 2 + 1;
    const int launch_damage  = launch? property(*launch, PWPN_DAMAGE) : 0;
    return std::max(0, launch_damage + missile_damage);
}

// Given the monster's current weapon and alt weapon (either or both of
// which may be NULL), works out whether using missiles or throwing the
// main weapon (with returning brand) is better. If using missiles that
// need a launcher, sets *launcher to the launcher.
//
// If the monster has no ranged weapon attack, returns NON_ITEM.
//
int mons_pick_best_missile(monster* mons, item_def **launcher,
                           bool ignore_melee)
{
    *launcher = NULL;
    item_def *melee = NULL, *launch = NULL;
    int melee_weapon_count = 0;
    for (int i = MSLOT_WEAPON; i <= MSLOT_ALT_WEAPON; ++i)
    {
        if (item_def *item = mons->mslot_item(static_cast<mon_inv_type>(i)))
        {
            if (is_range_weapon(*item))
                launch = item;
            else if (!ignore_melee)
            {
                melee = item;
                ++melee_weapon_count;
            }
        }
    }

    const item_def *missiles = mons->missiles();
    if (launch && missiles && !missiles->launched_by(*launch))
        launch = NULL;

    const int n_usable_melee_weapons(mons_wields_two_weapons(mons) ? 2 : 1);
    const bool only = melee_weapon_count == n_usable_melee_weapons
                      && melee->quantity == 1;
    const int tdam = (melee && is_throwable(mons, *melee)) ?
                         mons_thrown_weapon_damage(melee, only) : 0;
    const int fdam = mons_missile_damage(mons, launch, missiles);

    if (!tdam && !fdam)
        return (NON_ITEM);
    else if (tdam >= fdam)
        return (melee->index());
    else
    {
        *launcher = launch;
        return (missiles->index());
    }
}

int mons_natural_regen_rate(monster* mons)
{
    // A HD divider ranging from 3 (at 1 HD) to 1 (at 8 HD).
    int divider =
        std::max(div_rand_round(15 - mons->hit_dice, 4), 1);

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

    return (std::max(div_rand_round(mons->hit_dice, divider), 1));
}

void mons_check_pool(monster* mons, const coord_def &oldpos,
                     killer_type killer, int killnum)
{
    // Levitating/flying monsters don't make contact with the terrain.
    if (mons->airborne() || mons->can_cling_to(oldpos))
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
                 mons->name(DESC_CAP_THE).c_str(),
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
    }
}

bool monster_descriptor(int which_class, mon_desc_type which_descriptor)
{
    if (which_descriptor == MDSC_LEAVES_HIDE)
    {
        switch (which_class)
        {
        case MONS_DRAGON:
        case MONS_TROLL:
        case MONS_ICE_DRAGON:
        case MONS_STEAM_DRAGON:
        case MONS_MOTTLED_DRAGON:
        case MONS_STORM_DRAGON:
        case MONS_GOLDEN_DRAGON:
        case MONS_SWAMP_DRAGON:
        case MONS_YAK:
        case MONS_SHEEP:
            return (true);
        default:
            return (false);
        }
    }

    if (which_descriptor == MDSC_REGENERATES)
    {
        switch (which_class)
        {
        case MONS_CACODEMON:
        case MONS_DEEP_TROLL:
        case MONS_HELLWING:
        case MONS_IMP:
        case MONS_IRON_TROLL:
        case MONS_LEMURE:
        case MONS_ROCK_TROLL:
        case MONS_SLIME_CREATURE:
        case MONS_SNORG:
        case MONS_PURGY:
        case MONS_TROLL:
        case MONS_HYDRA:
        case MONS_KILLER_KLOWN:
        case MONS_LERNAEAN_HYDRA:
        case MONS_DISSOLUTION:
        case MONS_TEST_SPAWNER:
            return (true);
        default:
            return (false);
        }
    }

    if (which_descriptor == MDSC_NOMSG_WOUNDS)
    {
        // Zombified monsters other than spectral things don't show
        // wounds.
        if (mons_class_is_zombified(which_class)
            && which_class != MONS_SPECTRAL_THING)
        {
            return (true);
        }

        switch (which_class)
        {
        case MONS_RAKSHASA:
        case MONS_RAKSHASA_FAKE:
            return (true);
        default:
            return (false);
        }
    }

    return (false);
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

    // Monster was viewed this turn
    mons->flags |= MF_WAS_IN_VIEW;

    if (mons->flags & MF_SEEN)
        return;

    // First time we've seen this particular monster.
    mons->flags |= MF_SEEN;

    if (!mons_is_mimic(mons->type))
    {
        if (Hints.hints_left)
            hints_monster_seen(*mons);

        if (MONST_INTERESTING(mons))
        {
            take_note(
                      Note(NOTE_SEEN_MONSTER, mons->type, 0,
                           mons->name(DESC_NOCAP_A, true).c_str()));
        }
    }
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

    return (count > 0);
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
            || (item.flags & (ISFLAG_DROPPED | ISFLAG_THROWN | ISFLAG_NOTED_GET
                              | ISFLAG_BEEN_IN_INV)))
        {
            continue;
        }
        item.flags |= ISFLAG_SUMMONED;
    }
}

int dismiss_monsters(std::string pattern)
{
    // Make all of the monsters' original equipment disappear unless "keepitem"
    // is found in the regex (except for fixed arts and unrand arts).
    const bool keep_item = strip_tag(pattern, "keepitem");

    // Dismiss by regex.
    text_pattern tpat(pattern);
    int ndismissed = 0;
    for (monster_iterator mi; mi; ++mi)
    {
        if (mi->alive()
            && (tpat.empty() || tpat.matches(mi->name(DESC_PLAIN, true))))
        {
            if (!keep_item)
                _vanish_orig_eq(*mi);
            monster_die(*mi, KILL_DISMISSED, NON_MONSTER, false, true);
            ++ndismissed;
        }
    }

    return (ndismissed);
}

bool is_item_jelly_edible(const item_def &item)
{
    // Don't eat artefacts.
    if (is_artefact(item))
        return (false);

    // Shouldn't eat stone things
    //   - but what about wands and rings?
    if (item.base_type == OBJ_MISSILES
        && (item.sub_type == MI_STONE || item.sub_type == MI_LARGE_ROCK))
    {
        return (false);
    }

    // Don't eat special game items.
    if (item.base_type == OBJ_ORBS
        || (item.base_type == OBJ_MISCELLANY
            && (item.sub_type == MISC_RUNE_OF_ZOT
                || item.sub_type == MISC_HORN_OF_GERYON)))
    {
        return (false);
    }

    return (true);
}

bool monster_random_space(const monster* mons, coord_def& target,
                          bool forbid_sanctuary)
{
    int tries = 0;
    while (tries++ < 1000)
    {
        target = random_in_bounds();

        // Don't land on top of another monster.
        if (actor_at(target))
            continue;

        if (is_sanctuary(target) && forbid_sanctuary)
            continue;

        if (monster_habitable_grid(mons, grd(target)))
            return (true);
    }

    return (false);
}

bool monster_random_space(monster_type mon, coord_def& target,
                          bool forbid_sanctuary)
{
    monster dummy;
    dummy.type = mon;

    return (monster_random_space(&dummy, target, forbid_sanctuary));
}

void monster_teleport(monster* mons, bool instan, bool silent)
{
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

            mons->add_ench(mon_enchant(ENCH_TP, 0, KC_OTHER,
                                           random_range(20, 30)));
        }

        return;
    }

    bool was_seen = !silent
        && you.can_see(mons) && !mons_is_lurking(mons);

    if (!silent)
        simple_monster_message(mons, " disappears!");

    const coord_def oldplace = mons->pos();

    // Pick the monster up.
    mgrd(oldplace) = NON_MONSTER;

    coord_def newpos;
    if (monster_random_space(mons, newpos, !mons->wont_attack()))
        mons->moveto(newpos);

    mgrd(mons->pos()) = mons->mindex();

    // Mimics change form/colour when teleported.
    if (mons_is_mimic(mons->type))
    {
        monster_type old_type = mons->type;
        mons->type   = static_cast<monster_type>(
                                         MONS_GOLD_MIMIC + random2(5));
        mons->destroy_inventory();
        give_mimic_item(mons);
        mons->colour = get_mimic_colour(mons);
        was_seen = false;

        // If it's changed form, you won't recognise it.
        // This assumes that a non-gold mimic turning into another item of
        // the same description is really, really unlikely.
        if (old_type != MONS_GOLD_MIMIC || mons->type != MONS_GOLD_MIMIC)
            was_seen = false;
    }

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
            activity_interrupt_data ai(mons, "thin air");
            if (!interrupt_activity(AI_SEE_MONSTER, ai))
                simple_monster_message(mons, " appears out of thin air!");
        }
    }

    if (mons->visible_to(&you) && now_visible)
        handle_seen_interrupt(mons);

    // Leave a purple cloud.
    // XXX: If silent is true, this is not an actual teleport, but
    //      the game moving a monster out of the way.
    if (!silent)
    {
        place_cloud(CLOUD_TLOC_ENERGY, oldplace, 1 + random2(3),
                    mons->kill_alignment(), KILL_MON_MISSILE);
    }

    mons->check_redraw(oldplace);
    mons->apply_location_effects(oldplace);

    mons_relocated(mons);

    // Teleporting mimics change form - if they reappear out of LOS, they are
    // no longer known.
    if (mons_is_mimic(mons->type))
    {
        if (now_visible)
            mons->flags |= MF_KNOWN_MIMIC;
        else
            mons->flags &= ~MF_KNOWN_MIMIC;
    }
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

std::string summoned_poof_msg(const monster* mons, bool plural)
{
    int  summon_type = 0;
    bool valid_mon   = false;
    if (mons != NULL && !invalid_monster(mons))
    {
        (void) mons->is_summoned(NULL, &summon_type);
        valid_mon = true;
    }

    std::string msg      = "disappear%s in a puff of smoke";
    bool        no_chaos = false;

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

    return (msg);
}

std::string summoned_poof_msg(const int midx, const item_def &item)
{
    if (midx == NON_MONSTER)
        return summoned_poof_msg(static_cast<const monster* >(NULL), item);
    else
        return summoned_poof_msg(&menv[midx], item);
}

std::string summoned_poof_msg(const monster* mons, const item_def &item)
{
    ASSERT(item.flags & ISFLAG_SUMMONED);

    return summoned_poof_msg(mons, item.quantity > 1);
}

bool mons_reaped(actor *killer, monster* victim)
{
    beh_type beh;
    unsigned short hitting;

    if (killer->atype() == ACT_PLAYER)
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

    int midx = NON_MONSTER;
    if (animate_remains(victim->pos(), CORPSE_BODY, beh, hitting, killer, "",
                        GOD_NO_GOD, true, true, true, &midx) <= 0)
    {
        return (false);
    }

    monster* zombie = &menv[midx];

    if (you.can_see(victim))
        mprf("%s turns into a zombie!", victim->name(DESC_CAP_THE).c_str());
    else if (you.can_see(zombie))
        mprf("%s appears out of thin air!", zombie->name(DESC_CAP_THE).c_str());

    player_angers_monster(zombie);

    return (true);
}

beh_type attitude_creation_behavior(mon_attitude_type att)
{
    switch (att)
    {
    case ATT_NEUTRAL:
        return (BEH_NEUTRAL);
    case ATT_GOOD_NEUTRAL:
        return (BEH_GOOD_NEUTRAL);
    case ATT_STRICT_NEUTRAL:
        return (BEH_STRICT_NEUTRAL);
    case ATT_FRIENDLY:
        return (BEH_FRIENDLY);
    default:
        return (BEH_HOSTILE);
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

    if (mons_base_type(mon) == MONS_KRAKEN)
    {
        const int headnum = mon->mindex();

        for (monster_iterator mi; mi; ++mi)
            if (mi->type == MONS_KRAKEN_TENTACLE
                && (int)mi->number == headnum)
            {
                mi->attitude = att;
                for (monster_iterator connect; connect; ++connect)
                {
                    if (connect->type == MONS_KRAKEN_TENTACLE_SEGMENT
                        && (int) connect->number == mi->mindex())
                    {
                        connect->attitude = att;
                    }
                }
            }
    }
    if (mon->type == MONS_ELDRITCH_TENTACLE_SEGMENT
        || mon->type == MONS_ELDRITCH_TENTACLE)
    {
        int base_idx = mon->type == MONS_ELDRITCH_TENTACLE ? mon->mindex() : mon->number;

        menv[base_idx].attitude = att;
        for (monster_iterator mi; mi; ++mi)
        {
            mi->attitude = att;
        }
    }
}

// Find an enemy who would suffer from Awaken Forest.
actor* forest_near_enemy(const actor *mon)
{
    const coord_def pos = mon->pos();

    for (radius_iterator ri(pos, LOS_RADIUS); ri; ++ri)
    {
        actor* foe = actor_at(*ri);
        if (!foe || mons_aligned(foe, mon))
            continue;

        for (adjacent_iterator ai(*ri); ai; ++ai)
            if (grd(*ai) == DNGN_TREE && cell_see_cell(pos, *ai, LOS_DEFAULT))
                return (foe);
    }

    return (NULL);
}

// Print a message only if you can see any affected trees.
void forest_message(const coord_def pos, const std::string msg, msg_channel_type ch)
{
    for (radius_iterator ri(pos, LOS_RADIUS); ri; ++ri)
        if (grd(*ri) == DNGN_TREE
            && cell_see_cell(you.pos(), *ri, LOS_DEFAULT)
            && cell_see_cell(pos, *ri, LOS_DEFAULT))
        {
            mpr(msg, ch);
            return;
        }
}

void forest_damage(const actor *mon)
{
    const coord_def pos = mon->pos();

    if (one_chance_in(4))
        forest_message(pos, random_choose_string(
            "The trees move their gnarly branches around.",
            "You feel roots moving beneath the ground.",
            "Branches wave dangerously above you.",
            "Trunks creak and shift.",
            "Tree limbs sway around you.",
            0), MSGCH_TALK_VISUAL);

    for (radius_iterator ri(pos, LOS_RADIUS); ri; ++ri)
    {
        actor* foe = actor_at(*ri);
        if (!foe || mons_aligned(foe, mon))
            continue;

        for (adjacent_iterator ai(*ri); ai; ++ai)
            if (grd(*ai) == DNGN_TREE && cell_see_cell(pos, *ai, LOS_DEFAULT))
            {
                const int damage = 5 + random2(10);
                if (foe->atype() == ACT_PLAYER)
                {
                    mpr(random_choose_string(
                        "You are hit by a branch!",
                        "A tree reaches out and hits you!",
                        "A root smacks you from below.",
                        0));
                    ouch(damage, mon->mindex(), KILLED_BY_BEAM,
                         "angry trees", true);
                }
                else
                {
                    if (you.see_cell(foe->pos()))
                    {
                        const char *msg = random_choose_string(
                            "%s is hit by a branch!",
                            "A tree reaches out and hits %s!",
                            "A root smacks %s from below.",
                            0);
                        const bool up = *msg == '%';
                        // "it" looks butt-ugly here...
                        mprf(msg, foe->visible_to(&you) ?
                                      foe->name(up ? DESC_CAP_THE
                                                   : DESC_NOCAP_THE).c_str()
                                    : up ? "Something" : "something");
                    }
                    foe->hurt(mon, damage);
                }
                break;
            }
    }
}

void debuff_monster(monster* mon)
{
        // List of magical enchantments which will be dispelled.
    const enchant_type lost_enchantments[] = {
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
        ENCH_PETRIFIED
    };

     // Dispel all magical enchantments.
     for (unsigned int i = 0; i < ARRAYSZ(lost_enchantments); ++i)
          mon->del_ench(lost_enchantments[i], true, true);
}
