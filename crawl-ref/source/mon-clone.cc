/*  File:       mon-clone.cc
 *  Summary:    Code to clone existing monsters and to create player illusions.
 */

#include "AppHdr.h"

#include "arena.h"
#include "artefact.h"
#include "coord.h"
#include "directn.h"
#include "externs.h"
#include "env.h"
#include "items.h"
#include "libutil.h"
#include "mgen_data.h"
#include "monster.h"
#include "mon-behv.h"
#include "mon-clone.h"
#include "mon-enum.h"
#include "mon-iter.h"
#include "mon-place.h"
#include "mon-stuff.h"
#include "mon-util.h"
#include "player.h"
#include "random.h"
#include "terrain.h"
#include "transform.h"
#include "view.h"

const std::string clone_master_key = "mcloneorig";
const std::string clone_slave_key  = "mclonedupe";

static std::string _monster_clone_id_for(monsters *mons)
{
    return make_stringf("%s%d",
                        mons->name(DESC_PLAIN, true).c_str(),
                        you.num_turns);
}

static bool _monster_clone_exists(monsters *mons)
{
    if (!mons->props.exists(clone_master_key))
        return (false);

    const std::string clone_id = mons->props[clone_master_key].get_string();
    for (monster_iterator mi; mi; ++mi)
    {
        monsters *thing(*mi);
        if (thing->props.exists(clone_slave_key)
            && thing->props[clone_slave_key].get_string() == clone_id)
            return (true);
    }
    return (false);
}

bool mons_is_illusion(monsters *mons)
{
    return (mons->type == MONS_PLAYER_ILLUSION
            || mons->type == MONS_MARA_FAKE
            || mons->props.exists(clone_slave_key));
}

bool mons_is_illusion_cloneable(monsters *mons)
{
    return (!mons_is_illusion(mons) && !_monster_clone_exists(mons));
}

bool player_is_illusion_cloneable()
{
    for (monster_iterator mi; mi; ++mi)
    {
        if (mi->type == MONS_PLAYER_ILLUSION && mi->mname == you.your_name)
            return (false);
    }
    return (true);
}

bool actor_is_illusion_cloneable(actor *target)
{
    if (target->atype() == ACT_PLAYER)
    {
        ASSERT(target == &you);
        return player_is_illusion_cloneable();
    }
    else
    {
        return mons_is_illusion_cloneable(target->as_monster());
    }
}

static void _mons_summon_monster_illusion(monsters *caster,
                                          monsters *foe)
{
    // If the monster's clone is still kicking around, don't clone it again.
    if (!mons_is_illusion_cloneable(foe))
        return;

    // [ds] Bind the original target's attitude before calling
    // clone_mons, since clone_mons also updates arena bookkeeping.
    //
    // If an enslaved caster creates a clone from a regular hostile,
    // the clone should still be friendly:
    const mon_attitude_type clone_att =
        caster->friendly() ? ATT_FRIENDLY : caster->attitude;

    unwind_var<mon_attitude_type> att(foe->attitude, clone_att);
    bool cloning_visible = false;
    const int clone_idx = clone_mons(foe, true, &cloning_visible);
    if (clone_idx != NON_MONSTER)
    {
        const std::string clone_id = _monster_clone_id_for(foe);
        monsters *clone = &menv[clone_idx];
        clone->props[clone_slave_key] = clone_id;
        foe->props[clone_master_key] = clone_id;
        mons_add_blame(clone,
                       "woven by " + caster->name(DESC_NOCAP_THE));
        if (!clone->has_ench(ENCH_ABJ))
            clone->mark_summoned(6, true, MON_SUMM_CLONE);

        // Discard unsuitable enchantments.
        clone->del_ench(ENCH_CHARM);
        clone->del_ench(ENCH_STICKY_FLAME);
        clone->del_ench(ENCH_CORONA);

        behaviour_event(clone, ME_ALERT, MHITNOT, caster->pos());

        if (cloning_visible)
        {
            if (!you.can_see(caster))
                mprf("%s seems to step out of %s!",
                     foe->name(DESC_CAP_THE).c_str(),
                     foe->pronoun(PRONOUN_REFLEXIVE).c_str());
            else
                mprf("%s seems to draw %s out of %s!",
                     caster->name(DESC_CAP_THE).c_str(),
                     foe->name(DESC_NOCAP_THE).c_str(),
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
    if (transform_changed_physiology() && me->holiness == MH_UNDEAD)
        me->holiness = MH_NATURAL;
}

// [ds] Not *all* appropriate enchantments are mapped -- only things
// that are (presumably) internal to the body, like haste and
// poisoning, and specifically not external effects like corona and
// sticky flame.
enchant_type player_duration_to_mons_enchantment(duration_type dur)
{
    switch (dur)
    {
    case DUR_INVIS:     return ENCH_INVIS;
    case DUR_CONF:      return ENCH_CONFUSION;
    case DUR_PARALYSIS: return ENCH_PARALYSIS;
    case DUR_SLOW:      return ENCH_SLOW;
    case DUR_HASTE:     return ENCH_HASTE;
    case DUR_MIGHT:     return ENCH_MIGHT;
    case DUR_BERSERKER: return ENCH_BERSERK;
    case DUR_POISONING: return ENCH_POISON;
    case DUR_SWIFTNESS: return ENCH_SWIFT;

    default:            return ENCH_NONE;
    }
}

static void _mons_load_player_enchantments(monsters *creator, monsters *target)
{
    for (int i = 0; i < NUM_DURATIONS; ++i)
    {
        if (you.duration[i] > 0)
        {
            const duration_type dur(static_cast<duration_type>(i));
            const enchant_type ench =
                player_duration_to_mons_enchantment(dur);
            if (ench == ENCH_NONE)
                continue;
            target->add_ench(mon_enchant(ench,
                                         0,
                                         creator->kill_alignment(),
                                         you.duration[i]));
        }
    }
}

void mons_summon_illusion_from(monsters *monster, actor *foe,
                               spell_type spell_cast)
{
    if (foe->atype() == ACT_PLAYER)
    {
        ASSERT(foe == &you);
        const int midx =
            create_monster(
                mgen_data(MONS_PLAYER_ILLUSION, SAME_ATTITUDE(monster), monster,
                          6, spell_cast, monster->pos(), monster->foe, 0));
        if (midx != -1)
        {
            mpr("There is a horrible, sudden wrenching feeling in your soul!",
                MSGCH_WARN);

            monsters *clone = &menv[midx];
            // Change type from player ghost.
            clone->type = MONS_PLAYER_ILLUSION;
            _init_player_illusion_properties(
                get_monster_data(MONS_PLAYER_ILLUSION));
            _mons_load_player_enchantments(monster, clone);
        }
    }
    else
    {
        monsters *mfoe = foe->as_monster();
        _mons_summon_monster_illusion(monster, mfoe);
    }
}

bool mons_clonable(const monsters* mon, bool needs_adjacent)
{
    // No uniques or ghost demon monsters.  Also, figuring out the name
    // for the clone of a named monster isn't worth it.
    if (mons_is_unique(mon->type)
        || mons_is_ghost_demon(mon->type)
        || mon->is_named())
    {
        return (false);
    }

    if (needs_adjacent)
    {
        // Is there space for the clone?
        bool square_found = false;
        for (int i = 0; i < 8; i++)
        {
            const coord_def p = mon->pos() + Compass[i];

            if (in_bounds(p)
                && !actor_at(p)
                && monster_habitable_grid(mon, grd(p)))
            {
                square_found = true;
                break;
            }
        }
        if (!square_found)
            return (false);
    }

    // Is the monster carrying an artefact?
    for (int i = 0; i < NUM_MONSTER_SLOTS; i++)
    {
        const int index = mon->inv[i];

        if (index == NON_ITEM)
            continue;

        if (is_artefact(mitm[index]))
            return (false);
    }

    return (true);
}

int clone_mons(const monsters* orig, bool quiet, bool* obvious,
               coord_def pos)
{
    // Is there an open slot in menv?
    monsters* mons = get_free_monster();

    if (!mons)
        return (NON_MONSTER);

    if (!in_bounds(pos))
    {
        // Find an adjacent square.
        int squares = 0;
        for (int i = 0; i < 8; i++)
        {
            const coord_def p = orig->pos() + Compass[i];

            if (in_bounds(p)
                && !actor_at(p)
                && monster_habitable_grid(orig, grd(p)))
            {
                if (one_chance_in(++squares))
                    pos = p;
            }
        }

        if (squares == 0)
            return (NON_MONSTER);
    }

    ASSERT( !actor_at(pos) );

    *mons          = *orig;
    mons->set_position(pos);
    mgrd(pos)    = mons->mindex();

    // Duplicate objects, or unequip them if they can't be duplicated.
    for (int i = 0; i < NUM_MONSTER_SLOTS; i++)
    {
        const int old_index = orig->inv[i];

        if (old_index == NON_ITEM)
            continue;

        const int new_index = get_item_slot(0);
        if (new_index == NON_ITEM)
        {
            mons->unequip(mitm[old_index], i, 0, true);
            mons->inv[i] = NON_ITEM;
            continue;
        }

        mons->inv[i]      = new_index;
        mitm[new_index] = mitm[old_index];
        mitm[new_index].set_holding_monster(mons->mindex());
    }

    bool _obvious;
    if (obvious == NULL)
        obvious = &_obvious;
    *obvious = false;

    if (you.can_see(orig) && you.can_see(mons))
    {
        if (!quiet)
            simple_monster_message(orig, " is duplicated!");
        *obvious = true;
    }

    mark_interesting_monst(mons, mons->behaviour);
    if (you.can_see(mons))
    {
        handle_seen_interrupt(mons);
        viewwindow();
    }

    if (crawl_state.game_is_arena())
        arena_placed_monster(mons);

    return (mons->mindex());
}
