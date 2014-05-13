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
#include "mon-act.h"
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
        return tgt_pow > src_pow - 3 + relax * 3 / 2
                || (power == PPT_LESS && tgt_pow < src_pow - relax / 2);
    case PPT_MORE:
        return tgt_pow < src_pow + 2 - relax
                || (power == PPT_MORE && tgt_pow > src_pow + relax);
    default:
    case PPT_SAME:
        return tgt_pow < src_pow - relax
                || tgt_pow > src_pow + relax * 3 / 2;
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
        destroy_tentacles(mons);

    // Inform listeners that the original monster is gone.
    fire_monster_death_event(mons, KILL_MISC, NON_MONSTER, true);

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
    mon_enchant vines     = mons->get_ench(ENCH_AWAKEN_VINES);
    mon_enchant forest    = mons->get_ench(ENCH_AWAKEN_FOREST);

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

    // Don't allow polymorphing monsters for hides.
    mons->props["no_hide"] = true;

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
    mons->add_ench(vines);
    mons->add_ench(forest);

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
        target = MONS_SLIME_CREATURE;
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
    // Don't corrode spectral weapons.
    if (mons_is_avatar(mons->type))
        return;

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
        if (mons->type == MONS_WANDERING_MUSHROOM
            && monster_at(loc)->type == MONS_TOADSTOOL)
        {
            monster_swaps_places(mons, loc - mons->pos());
            return true;
        }
        else
        {
            mpr("Something prevents you from swapping places.");
            return false;
        }
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
        return false;

    // Don't move onto dangerous terrain.
    if (is_feat_dangerous(grd(mons->pos())))
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

    if (monster_at(loc)
        && monster_at(loc)->type == MONS_TOADSTOOL
        && mons->type == MONS_WANDERING_MUSHROOM)
    {
        swap = monster_habitable_grid(mons, grd(loc));
    }

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
    if (!mons_can_display_wounds(mons))
        return MDAM_OKAY;

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
        ss << (wounded_damaged(holi) ? " destroyed" : " dead");
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
    ss << (wounded_damaged(holi) ? " damaged" : " wounded");
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
bool wounded_damaged(mon_holy_type holi)
{
    // this schema needs to be abstracted into real categories {dlb}:
    return holi == MH_UNDEAD || holi == MH_NONLIVING || holi == MH_PLANT;
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

    if (!(mons->flags & MF_TSO_SEEN))
    {
        if (!mons->has_ench(ENCH_ABJ)
            && !mons->has_ench(ENCH_FAKE_ABJURATION)
            && !testbits(mons->flags, MF_NO_REWARD)
            && !mons_class_flag(mons->type, M_NO_EXP_GAIN)
            && !crawl_state.game_is_arena())
        {
            did_god_conduct(DID_SEE_MONSTER, mons->hit_dice, true, mons);
        }
        mons->flags |= MF_TSO_SEEN;
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
        free_stationary_net(net);

    mon->del_ench(ENCH_HELD, true);
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
    mon->align_avatars();
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
