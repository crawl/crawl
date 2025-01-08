/**
 * @file
 * @brief Code to clone existing monsters and to create player illusions.
**/

#include "AppHdr.h"

#include "mon-clone.h"

#include "act-iter.h"
#include "arena.h"
#include "artefact.h"
#include "coordit.h"
#include "env.h"
#include "god-abil.h"
#include "items.h"
#include "message.h"
#include "mgen-data.h"
#include "mon-behv.h"
#include "mon-death.h"
#include "mon-place.h"
#include "mon-tentacle.h"
#include "state.h"
#include "stringutil.h"
#include "terrain.h"
#include "transform.h"
#include "view.h"

static string _monster_clone_id_for(monster* mons)
{
    return make_stringf("%s%d",
                        mons->name(DESC_PLAIN, true).c_str(),
                        you.num_turns);
}

static bool _monster_clone_exists(monster* mons)
{
    if (!mons->props.exists(CLONE_PRIMARY_KEY))
        return false;

    const string clone_id = mons->props[CLONE_PRIMARY_KEY].get_string();
    for (monster_iterator mi; mi; ++mi)
    {
        monster* thing(*mi);
        if (thing->props.exists(CLONE_REPLICA_KEY)
            && thing->props[CLONE_REPLICA_KEY].get_string() == clone_id)
        {
            return true;
        }
    }
    return false;
}

static bool _mons_is_illusion_cloneable(monster* mons)
{
    return !mons->is_peripheral()
           && !mons->is_illusion()
           && !_monster_clone_exists(mons);
}

static bool _player_is_illusion_cloneable()
{
    for (monster_iterator mi; mi; ++mi)
    {
        if (mi->type == MONS_PLAYER_ILLUSION && mi->mname == you.your_name)
            return false;
    }
    return true;
}

bool actor_is_illusion_cloneable(actor *target)
{
    if (target->is_player())
        return _player_is_illusion_cloneable();
    else
        return _mons_is_illusion_cloneable(target->as_monster());
}

static void _mons_summon_monster_illusion(monster* caster,
                                          monster* foe)
{
    // If the monster's clone is still kicking around, don't clone it again.
    if (!_mons_is_illusion_cloneable(foe))
        return;

    bool cloning_visible = false;

    // If a charmed caster creates a clone from a regular hostile,
    // the clone should still be friendly.
    if (monster *clone = clone_mons(foe, true, &cloning_visible,
                                    caster->friendly() ?
                                    ATT_FRIENDLY : caster->attitude))
    {
        const string clone_id = _monster_clone_id_for(foe);
        clone->props[CLONE_REPLICA_KEY] = clone_id;
        foe->props[CLONE_PRIMARY_KEY] = clone_id;
        mons_add_blame(clone,
                       "woven by " + caster->name(DESC_THE));
        if (!clone->has_ench(ENCH_SUMMON_TIMER))
            clone->mark_summoned(MON_SUMM_CLONE, summ_dur(6), true, true);
        // If isn't very ambiguous if a monster makes a hostile clone out of a
        // friendly monster, so don't try to hide it.
        if (!foe->friendly())
            clone->add_ench(ENCH_PHANTOM_MIRROR);
        clone->summoner = caster->mid;

        // Discard unsuitable enchantments.
        clone->del_ench(ENCH_CHARM);
        clone->del_ench(ENCH_STICKY_FLAME);
        clone->del_ench(ENCH_CONTAM);
        clone->del_ench(ENCH_CORONA);
        clone->del_ench(ENCH_SILVER_CORONA);
        clone->del_ench(ENCH_HEXED);

        behaviour_event(clone, ME_ALERT, 0, caster->pos());

        if (cloning_visible)
        {
            if (!you.can_see(*caster))
            {
                mprf("%s seems to step out of %s!",
                     foe->name(DESC_THE).c_str(),
                     foe->pronoun(PRONOUN_REFLEXIVE).c_str());
            }
            else
                mprf("%s seems to draw %s out of %s!",
                     caster->name(DESC_THE).c_str(),
                     foe->name(DESC_THE).c_str(),
                     foe->pronoun(PRONOUN_REFLEXIVE).c_str());
        }
    }
}

static void _init_player_illusion_properties(monsterentry *me)
{
    me->holiness = you.holiness();
    // [ds] If we're cloning the player, use their base holiness, not
    // the effects of their Necromutation form. This was 'important'
    // since Necromutation spell-users presumably also had Dispel
    // Undead available to them, but now...?!
    if (form_changes_physiology() && me->holiness & MH_UNDEAD)
        me->holiness = MH_NATURAL;
}

// [ds] Not *all* appropriate enchantments are mapped -- only things
// that are (presumably) internal to the body, like haste and
// poisoning, and specifically not external effects like corona and
// sticky flame.
static enchant_type _player_duration_to_mons_enchantment(duration_type dur)
{
    switch (dur)
    {
    case DUR_BARBS:            return ENCH_BARBS;
    case DUR_BERSERK:          return ENCH_BERSERK;
    case DUR_BRILLIANCE:       return ENCH_EMPOWERED_SPELLS;
    case DUR_BLIND:            return ENCH_BLIND;
    case DUR_CONF:             return ENCH_CONFUSION;
    case DUR_CORROSION:        return ENCH_CORROSION;
    case DUR_DIMENSION_ANCHOR: return ENCH_DIMENSION_ANCHOR;
    case DUR_FIRE_VULN:        return ENCH_FIRE_VULN;
    case DUR_FROZEN:           return ENCH_FROZEN;
    case DUR_HASTE:            return ENCH_HASTE;
    case DUR_INVIS:            return ENCH_INVIS;
    case DUR_LOWERED_WL:       return ENCH_LOWERED_WL;
    case DUR_MIGHT:            return ENCH_MIGHT;
    case DUR_NO_MOMENTUM:      return ENCH_BOUND;
    case DUR_PARALYSIS:        return ENCH_PARALYSIS;
    case DUR_PETRIFYING:       return ENCH_PETRIFYING;
    case DUR_PETRIFIED:        return ENCH_PETRIFIED;
    case DUR_POISONING:        return ENCH_POISON;
    case DUR_POISON_VULN:      return ENCH_POISON_VULN;
    case DUR_RESISTANCE:       return ENCH_RESISTANCE;
    case DUR_SAP_MAGIC:        return ENCH_SAP_MAGIC;
    case DUR_SIGN_OF_RUIN:     return ENCH_SIGN_OF_RUIN;
    case DUR_SLOW:             return ENCH_SLOW;
    case DUR_STICKY_FLAME:     return ENCH_STICKY_FLAME;
    case DUR_SWIFTNESS:        return ENCH_SWIFT;
    case DUR_TOXIC_RADIANCE:   return ENCH_TOXIC_RADIANCE;
    case DUR_TROGS_HAND:
    case DUR_ENLIGHTENED:      return ENCH_STRONG_WILLED;
    case DUR_VITRIFIED:        return ENCH_VITRIFIED;
    case DUR_WEAK:             return ENCH_WEAK;

    default:            return ENCH_NONE;
    }
}

static void _mons_load_player_enchantments(monster* creator, monster* target)
{
    for (int i = 0; i < NUM_DURATIONS; ++i)
    {
        if (you.duration[i] > 0)
        {
            const duration_type dur(static_cast<duration_type>(i));
            const enchant_type ench =
                _player_duration_to_mons_enchantment(dur);
            if (ench == ENCH_NONE)
                continue;
            target->add_ench(mon_enchant(ench,
                                         0,
                                         creator,
                                         you.duration[i]));
        }
    }
}

int mons_summon_illusion_from(monster* mons, actor *foe,
                              spell_type spell_cast, int card_power, bool xom)
{
    if (foe->is_player())
    {
        int dur = 6;

        if (xom)
            dur = 2;
        else if (card_power >= 0)
        {
          // card effect
          dur = 2 + random2(card_power);
        }

        if (monster *clone = create_monster(
                mgen_data(MONS_PLAYER_ILLUSION, SAME_ATTITUDE(mons),
                          mons->pos(), mons->foe)
                 .set_summoned(mons, spell_cast, summ_dur(dur))))
        {
            if (card_power >= 0)
                mpr("Suddenly you stand beside yourself.");
            else
                mprf(MSGCH_WARN, "There is a horrible, sudden wrenching feeling in your soul!");

            _init_player_illusion_properties(
                get_monster_data(MONS_PLAYER_ILLUSION));
            _mons_load_player_enchantments(mons, clone);

            return 1;
        }
        else if (card_power >= 0)
        {
            mpr("You see a puff of smoke.");
            return 0;
        }
    }
    else
    {
        monster* mfoe = foe->as_monster();
        _mons_summon_monster_illusion(mons, mfoe);
    }

    return 1;
}

bool mons_clonable(const monster* mon, bool needs_adjacent)
{
    // No uniques or inugami. Also, figuring out the name for the clone
    // of a named monster isn't worth it, and duplicate battlespheres
    // with the same owner cause problems with the spell.
    if (mons_is_unique(mon->type)
        || mon->type == MONS_INUGAMI
        || mon->is_named()
        || mon->is_peripheral())
    {
        return false;
    }

    if (needs_adjacent)
    {
        // Is there space for the clone?
        bool square_found = false;
        for (adjacent_iterator ai(mon->pos()); ai; ++ai)
        {
            if (in_bounds(*ai)
                && !actor_at(*ai)
                && monster_habitable_grid(mon, *ai))
            {
                square_found = true;
                break;
            }
        }
        if (!square_found)
            return false;
    }

    return true;
}

/*
 * @param orig          The original monster to clone.
 * @param quiet         If true, suppress messages
 * @param obvious       If true, player can see the orig & cloned monster
 * @return              Returns the cloned monster
 */
monster* clone_mons(const monster* orig, bool quiet, bool* obvious)
{
    // Pass temp_attitude to handle charmed monsters cloning monsters
    return clone_mons(orig, quiet, obvious, orig->temp_attitude());
}

/**
 * @param orig          The original monster to clone.
 * @param quiet         If true, suppress messages
 * @param obvious       If true, player can see the orig & cloned monster
 * @param mon_att       The attitude to set for the cloned monster
 * @param place         If set, place it somewhere instead of near the source
 * @return              Returns the cloned monster
 */
monster* clone_mons(const monster* orig, bool quiet, bool* obvious,
                    mon_attitude_type mon_att, coord_def place)
{
    // Is there an open slot in env.mons?
    monster* mons = get_free_monster();
    coord_def pos(0, 0);

    if (!mons)
        return nullptr;

    if (place.origin())
    {
        for (fair_adjacent_iterator ai(orig->pos()); ai; ++ai)
        {
            if (in_bounds(*ai)
                && !actor_at(*ai)
                && monster_habitable_grid(orig, *ai))
            {
                pos = *ai;
            }
        }

        if (!in_bounds(pos))
            return nullptr;
    }
    else
       pos = place;

    ASSERT(!actor_at(pos));

    *mons          = *orig;
    mons->set_new_monster_id();
    mons->move_to_pos(pos);
    mons->attitude = mon_att;

    // The monster copy constructor doesn't copy constriction, so no need to
    // worry about that. We do need to worry about the enchantments associated
    // with direct constriction, though.
    if (mons->has_ench(ENCH_VILE_CLUTCH))
        mons->del_ench(ENCH_VILE_CLUTCH);

    if (mons->has_ench(ENCH_GRASPING_ROOTS))
        mons->del_ench(ENCH_GRASPING_ROOTS);

    // Don't copy death triggers - phantom royal jellies should not open the
    // Slime vaults on death.
    if (mons->props.exists(MONSTER_DIES_LUA_KEY))
        mons->props.erase(MONSTER_DIES_LUA_KEY);

    // Clear all duel-related keys from clones.
    if (mons->props.exists(OKAWARU_DUEL_TARGET_KEY))
    {
        mons->props.erase(OKAWARU_DUEL_TARGET_KEY);
        mons->props.erase(OKAWARU_DUEL_CURRENT_KEY);
        mons->props.erase(OKAWARU_DUEL_ABANDONED_KEY);
    }

    // Don't display non-functional bullseye targets
    if (mons->has_ench(ENCH_BULLSEYE_TARGET))
        mons->del_ench(ENCH_BULLSEYE_TARGET);

    // Remove Beogh's Touch from an apostle of Beogh may get very confused when they die
    if (mons->has_ench(ENCH_TOUCH_OF_BEOGH))
        mons->del_ench(ENCH_TOUCH_OF_BEOGH);

    if (mons->has_ench(ENCH_VENGEANCE_TARGET))
        you.duration[DUR_BEOGH_SEEKING_VENGEANCE] += 1;

    // Duplicate objects, or unequip them if they can't be duplicated.
    for (mon_inv_iterator ii(*mons); ii; ++ii)
    {
        const int old_index = ii->index();

        const int new_index = get_mitm_slot(0);
        if (new_index == NON_ITEM)
        {
            mons->unequip(env.item[old_index], false, true);
            mons->inv[ii.slot()] = NON_ITEM;
            continue;
        }

        mons->inv[ii.slot()] = new_index;
        env.item[new_index] = env.item[old_index];
        env.item[new_index].set_holding_monster(*mons);
    }

    bool _obvious;
    if (obvious == nullptr)
        obvious = &_obvious;
    *obvious = false;

    if (you.can_see(*orig) && you.can_see(*mons))
    {
        if (!quiet)
            simple_monster_message(*orig, " is duplicated!");
        *obvious = true;
    }

    if (you.can_see(*mons))
    {
        handle_seen_interrupt(mons);
        viewwindow();
        update_screen();
    }

    if (crawl_state.game_is_arena())
        arena_placed_monster(mons);

    return mons;
}
