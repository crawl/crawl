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
#include "directn.h"
#include "env.h"
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
#include "unwind.h"
#include "view.h"

static string _monster_clone_id_for(monster* mons)
{
    return make_stringf("%s%d",
                        mons->name(DESC_PLAIN, true).c_str(),
                        you.num_turns);
}

static bool _monster_clone_exists(monster* mons)
{
    if (!mons->props.exists(CLONE_MASTER_KEY))
        return false;

    const string clone_id = mons->props[CLONE_MASTER_KEY].get_string();
    for (monster_iterator mi; mi; ++mi)
    {
        monster* thing(*mi);
        if (thing->props.exists(CLONE_SLAVE_KEY)
            && thing->props[CLONE_SLAVE_KEY].get_string() == clone_id)
        {
            return true;
        }
    }
    return false;
}

static bool _mons_is_illusion_cloneable(monster* mons)
{
    return !mons_is_conjured(mons->type)
           && !mons_is_tentacle_or_tentacle_segment(mons->type)
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

    // If an enslaved caster creates a clone from a regular hostile,
    // the clone should still be friendly.
    if (monster *clone = clone_mons(foe, true, &cloning_visible,
                                    caster->friendly() ?
                                    ATT_FRIENDLY : caster->attitude))
    {
        const string clone_id = _monster_clone_id_for(foe);
        clone->props[CLONE_SLAVE_KEY] = clone_id;
        foe->props[CLONE_MASTER_KEY] = clone_id;
        mons_add_blame(clone,
                       "woven by " + caster->name(DESC_THE));
        if (!clone->has_ench(ENCH_ABJ))
            clone->mark_summoned(6, true, MON_SUMM_CLONE);
        clone->summoner = caster->mid;

        // Discard unsuitable enchantments.
        clone->del_ench(ENCH_CHARM);
        clone->del_ench(ENCH_STICKY_FLAME);
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
    // the effects of their Necromutation spell. This is important
    // since Necromutation users presumably also have Dispel Undead
    // available to them. :P
    if (form_changed_physiology() && me->holiness & MH_UNDEAD)
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
    case DUR_INVIS:     return ENCH_INVIS;
    case DUR_CONF:      return ENCH_CONFUSION;
    case DUR_PARALYSIS: return ENCH_PARALYSIS;
    case DUR_SLOW:      return ENCH_SLOW;
    case DUR_HASTE:     return ENCH_HASTE;
    case DUR_MIGHT:     return ENCH_MIGHT;
    case DUR_BERSERK:   return ENCH_BERSERK;
    case DUR_POISONING: return ENCH_POISON;

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

void mons_summon_illusion_from(monster* mons, actor *foe,
                               spell_type spell_cast, int card_power)
{
    if (foe->is_player())
    {
        int abj = 6;

        if (card_power >= 0)
        {
          // card effect
          abj = 2 + random2(card_power);
        }

        if (monster *clone = create_monster(
                mgen_data(MONS_PLAYER_ILLUSION, SAME_ATTITUDE(mons),
                          mons->pos(), mons->foe)
                 .set_summoned(mons, abj, spell_cast)))
        {
            if (card_power >= 0)
                mpr("Suddenly you stand beside yourself.");
            else
                mprf(MSGCH_WARN, "There is a horrible, sudden wrenching feeling in your soul!");

            _init_player_illusion_properties(
                get_monster_data(MONS_PLAYER_ILLUSION));
            _mons_load_player_enchantments(mons, clone);
        }
        else if (card_power >= 0)
            mpr("You see a puff of smoke.");
    }
    else
    {
        monster* mfoe = foe->as_monster();
        _mons_summon_monster_illusion(mons, mfoe);
    }
}

bool mons_clonable(const monster* mon, bool needs_adjacent)
{
    // No uniques or ghost demon monsters. Also, figuring out the name
    // for the clone of a named monster isn't worth it, and duplicate
    // battlespheres with the same owner cause problems with the spell
    if (mons_is_unique(mon->type)
        || mons_is_ghost_demon(mon->type)
        || mon->is_named()
        || mon->type == MONS_BATTLESPHERE)
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
                && monster_habitable_grid(mon, grd(*ai)))
            {
                square_found = true;
                break;
            }
        }
        if (!square_found)
            return false;
    }

    // Is the monster carrying an artefact?
    for (mon_inv_iterator ii(const_cast<monster &>(*mon)); ii; ++ii)
        if (is_artefact(*ii))
            return false;

    return true;
}

monster* clone_mons(const monster* orig, bool quiet, bool* obvious,
                    mon_attitude_type mon_att, coord_def pos)
{
    // Is there an open slot in menv?
    monster* mons = get_free_monster();

    if (!mons)
        return nullptr;

    if (!in_bounds(pos))
    {
        for (fair_adjacent_iterator ai(orig->pos()); ai; ++ai)
        {
            if (in_bounds(*ai)
                && !actor_at(*ai)
                && monster_habitable_grid(orig, grd(*ai)))
            {
                pos = *ai;
            }
        }

        if (!in_bounds(pos))
            return nullptr;
    }

    ASSERT(!actor_at(pos));
    ASSERT_IN_BOUNDS(pos);

    *mons          = *orig;
    mons->set_new_monster_id();
    mons->move_to_pos(pos);
    mons->attitude = mon_att == ATT_SAME ?
            orig->attitude : mon_att;

    // The monster copy constructor doesn't copy constriction, so no need to
    // worry about that.

    // Don't copy death triggers - phantom royal jellies should not open the
    // Slime vaults on death.
    if (mons->props.exists(MONSTER_DIES_LUA_KEY))
        mons->props.erase(MONSTER_DIES_LUA_KEY);

    // Duplicate objects, or unequip them if they can't be duplicated.
    for (mon_inv_iterator ii(*mons); ii; ++ii)
    {
        const int old_index = ii->index();

        const int new_index = get_mitm_slot(0);
        if (new_index == NON_ITEM)
        {
            mons->unequip(mitm[old_index], false, true);
            mons->inv[ii.slot()] = NON_ITEM;
            continue;
        }

        mons->inv[ii.slot()] = new_index;
        mitm[new_index] = mitm[old_index];
        mitm[new_index].set_holding_monster(*mons);
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
    }

    if (crawl_state.game_is_arena())
        arena_placed_monster(mons);

    return mons;
}
