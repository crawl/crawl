/**
 * @file
 * @brief Misc monster poly/mimic functions.
**/

#include "AppHdr.h"

#include "mon-poly.h"

#include "artefact.h"
#include "art-enum.h"
#include "attitude-change.h"
#include "delay.h"
#include "describe.h"
#include "dgn-overview.h"
#include "dungeon.h"
#include "fineff.h"
#include "god-conduct.h"
#include "hints.h"
#include "item-prop.h"
#include "item-status-flag-type.h"
#include "items.h"
#include "level-state-type.h"
#include "libutil.h"
#include "message.h"
#include "mon-death.h"
#include "mon-gear.h"
#include "mon-place.h"
#include "mon-tentacle.h"
#include "notes.h"
#include "religion.h"
#include "state.h"
#include "stringutil.h"
#include "terrain.h"
#include "traps.h"
#include "xom.h"

#define ORIG_HD_KEY "orig_hd"

bool feature_mimic_at(const coord_def &c)
{
    return map_masked(c, MMT_MIMIC);
}

item_def* item_mimic_at(const coord_def &c)
{
    for (stack_iterator si(c); si; ++si)
        if (si->flags & ISFLAG_MIMIC)
            return &*si;
    return nullptr;
}

bool mimic_at(const coord_def &c)
{
    return feature_mimic_at(c) || item_mimic_at(c);
}

void monster_drop_things(monster* mons,
                          bool mark_item_origins,
                          bool (*suitable)(const item_def& item))
{
    // Drop weapons and missiles last (i.e., on top), so others pick up.
    for (int i = NUM_MONSTER_SLOTS - 1; i >= 0; --i)
    {
        int item = mons->inv[i];
        if (item == NON_ITEM || !suitable(env.item[item]))
            continue;

        if (testbits(env.item[item].flags, ISFLAG_SUMMONED))
        {
            item_was_destroyed(env.item[item]);
            destroy_item(item);
            mons->inv[i] = NON_ITEM;
            continue;
        }

        if (mark_item_origins && env.item[item].defined())
            origin_set_monster(env.item[item], mons);

        env.item[item].props[DROPPER_MID_KEY].get_int() = mons->mid;

        if (env.item[item].props.exists("autoinscribe"))
        {
            add_inscription(env.item[item],
                env.item[item].props["autoinscribe"].get_string());
            env.item[item].props.erase("autoinscribe");
        }

        // If a monster is swimming, the items are ALREADY underwater.
        move_item_to_grid(&item, mons->pos(), mons->swimming());
        mons->inv[i] = NON_ITEM;
    }
}

static bool _valid_type_morph(const monster &mons, monster_type new_mclass)
{
    // Shapeshifters cannot polymorph into glowing shapeshifters or
    // vice versa.
    if ((new_mclass == MONS_GLOWING_SHAPESHIFTER
             && mons.has_ench(ENCH_SHAPESHIFTER))
         || (new_mclass == MONS_SHAPESHIFTER
             && mons.has_ench(ENCH_GLOWING_SHAPESHIFTER)))
    {
        return false;
    }

    // Various inappropriate polymorph targets.
    if (   new_mclass == MONS_PROGRAM_BUG
        || new_mclass == MONS_NO_MONSTER
        || mons_class_flag(new_mclass, M_UNFINISHED)  // no unfinished monsters
        || mons_class_flag(new_mclass, M_CANT_SPAWN)  // no dummy monsters
        || mons_class_flag(new_mclass, M_NO_POLY_TO)  // explicitly disallowed
        || mons_class_flag(new_mclass, M_UNIQUE)      // no uniques
        || !mons_class_gives_xp(new_mclass)           // no tentacle parts or
                                                      // harmless things
        || !(mons_class_holiness(new_mclass) & mons_class_holiness(mons.type))
        // normally holiness just needs to overlap, but we don't want
        // shapeshifters to become demons
        || mons.is_shapeshifter() && !(mons_class_holiness(new_mclass) & MH_NATURAL)
        || !mons_class_is_threatening(new_mclass)

        // 'morph targets are _always_ "base" classes, not derived ones.
        || new_mclass != mons_species(new_mclass)
        || new_mclass == mons_species(mons.type)
        // They act as separate polymorph classes on their own.
        || mons_class_is_zombified(new_mclass)

        // Pan lords aren't uniques, but they might as well be.
        || new_mclass == MONS_PANDEMONIUM_LORD

        // Other poly-unsuitable things.
        || mons_is_statue(new_mclass)
        || mons_is_projectile(new_mclass)

        // The spell on Prince Ribbit can't be broken so easily.
        || (new_mclass == MONS_HUMAN
            && (mons.type == MONS_PRINCE_RIBBIT
                || mons.mname == "Prince Ribbit")))
    {
        return false;
    }

    return true;
}



static bool _valid_morph(monster* mons, monster_type new_mclass)
{
    if (!_valid_type_morph(*mons, new_mclass))
        return false;

    // [hm] Lower base draconian chances since there are nine of them,
    // and they shouldn't each count for a full chance.
    if (mons_genus(new_mclass) == MONS_DRACONIAN
        && new_mclass != MONS_DRACONIAN
        && !one_chance_in(9))
    {
        return false;
    }

    // Determine if the monster is happy on current tile.
    return monster_habitable_grid(new_mclass, env.grid(mons->pos()));
}

static bool _jiyva_slime_target(monster_type targetc)
{
    return you_worship(GOD_JIYVA)
           && (targetc == MONS_ENDOPLASM
              || targetc == MONS_JELLY
              || targetc == MONS_SLIME_CREATURE
              || targetc == MONS_ACID_BLOB
              || targetc == MONS_AZURE_JELLY);
}

void change_monster_type(monster* mons, monster_type targetc, bool do_seen)
{
    ASSERT(mons); // XXX: change to monster &mons
    bool could_see     = you.can_see(*mons);
    // NOTE(pf): This logic is wrong! FIXME!
    bool slimified = _jiyva_slime_target(targetc);

    // Quietly remove the old monster's invisibility before transforming
    // it. If we don't do this, it'll stay invisible even after losing
    // the invisibility enchantment below.
    mons->del_ench(ENCH_INVIS, false, false);

    // Remove replacement tile, since it probably doesn't work for the
    // new monster.
    mons->props.erase(MONSTER_TILE_NAME_KEY);
    mons->props.erase(MONSTER_TILE_KEY);

    // Even if the monster transforms from one type that can behold the
    // player into a different type which can also behold the player,
    // the polymorph disrupts the beholding process. Do this before
    // changing mons->type, since unbeholding can only happen while
    // the monster is still a siren/merfolk avatar.
    you.remove_beholder(*mons);
    you.remove_fearmonger(mons);

    if (mons_is_tentacle_head(mons_base_type(*mons)))
        destroy_tentacles(mons);

    // trj spills out jellies when polied, as if he'd been hit for mhp.
    if (mons->type == MONS_ROYAL_JELLY)
    {
        simple_monster_message(*mons, "'s form twists and warps, and jellies "
                               "spill out!");
        trj_spawn_fineff::schedule(nullptr, mons, mons->pos(),
                                   mons->hit_points);
    }

    // Inform listeners that the original monster is gone.
    fire_monster_death_event(mons, KILL_MISC, true);

    // the actual polymorphing:
    auto flags =
        mons->flags & ~(MF_SEEN | MF_ATT_CHANGE_ATTEMPT
                           | MF_WAS_IN_VIEW | MF_BAND_FOLLOWER | MF_KNOWN_SHIFTER
                           | MF_MELEE_MASK);
    flags |= MF_POLYMORPHED;
    string name;

    // Preserve the names of uniques and named monsters.
    if (mons->type == MONS_ROYAL_JELLY
        || mons->mname == "shaped Royal Jelly")
    {
        name   = "shaped Royal Jelly";
        flags |= MF_NAME_SUFFIX;
    }
    else if (mons->type == MONS_LERNAEAN_HYDRA
             || mons->mname == "shaped Lernaean hydra")
    {
        name   = "shaped Lernaean hydra";
        flags |= MF_NAME_SUFFIX;
    }
    else if (mons->mons_species() == MONS_SERPENT_OF_HELL
             || mons->mname == "shaped Serpent of Hell")
    {
        name   = "shaped Serpent of Hell";
        flags |= MF_NAME_SUFFIX;
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
        name = mons->name(DESC_PLAIN, true);

        // "Blorkula the orcula" and similar.
        const size_t the_pos = name.find(" the ");
        if (the_pos != string::npos)
            name = name.substr(0, the_pos);
    }

    const god_type old_god        = mons->god;
    const int  old_hp             = mons->hit_points;
    const int  old_hp_max         = mons->max_hit_points;
    const bool old_mon_caught     = mons->caught();
    const char old_ench_countdown = mons->ench_countdown;

    const bool old_mon_unique = mons_is_or_was_unique(*mons);

    if (!mons->props.exists(ORIGINAL_TYPE_KEY))
    {
        const monster_type type = mons_is_draconian_job(mons->type)
                                ? draconian_subspecies(*mons)
                                : mons->type;
        mons->props[ORIGINAL_TYPE_KEY].get_int() = type;
        if (mons->mons_species() == MONS_HYDRA)
            mons->props[OLD_HEADS_KEY].get_int() = mons->num_heads;
    }
    if (!mons->props.exists(ORIG_HD_KEY))
        mons->props[ORIG_HD_KEY] = mons->get_experience_level();

    mon_enchant abj       = mons->get_ench(ENCH_ABJ);
    mon_enchant fabj      = mons->get_ench(ENCH_FAKE_ABJURATION);
    mon_enchant charm     = mons->get_ench(ENCH_CHARM);
    mon_enchant shifter   = mons->get_ench(ENCH_GLOWING_SHAPESHIFTER,
                                           ENCH_SHAPESHIFTER);
    mon_enchant summon    = mons->get_ench(ENCH_SUMMON);
    mon_enchant tp        = mons->get_ench(ENCH_TP);
    mon_enchant vines     = mons->get_ench(ENCH_AWAKEN_VINES);
    mon_enchant forest    = mons->get_ench(ENCH_AWAKEN_FOREST);
    mon_enchant hexed     = mons->get_ench(ENCH_HEXED);
    mon_enchant insanity  = mons->get_ench(ENCH_FRENZIED);
    mon_enchant vengeance = mons->get_ench(ENCH_VENGEANCE_TARGET);

    mons->number       = 0;

    // Note: define_monster(*) will clear out all enchantments! - bwr
    if (!slimified && mons_is_zombified(*mons))
        define_zombie(mons, targetc, mons->type);
    else
    {
        mons->type         = targetc;
        mons->base_monster = MONS_NO_MONSTER;
        define_monster(*mons);
    }

    mons->mname = name;
    mons->props[NO_ANNOTATE_KEY] = slimified && old_mon_unique;
    mons->props.erase(DBNAME_KEY);

    // Forget seen spells, since they are likely to have changed.
    mons->props.erase(SEEN_SPELLS_KEY);

    mons->flags = flags;
    // Line above might clear melee flags; restore.
    mons->bind_melee_flags();

    // Forget various speech/shout Lua functions.
    mons->props.erase(SPEECH_PREFIX_KEY);

    // Make sure we have a god if we've been polymorphed into a priest.
    mons->god = (mons->is_priest() && old_god == GOD_NO_GOD) ? GOD_NAMELESS
                                                             : old_god;

    mons->add_ench(abj);
    mons->add_ench(fabj);
    mons->add_ench(charm);
    mons->add_ench(shifter);
    mons->add_ench(summon);
    mons->add_ench(tp);
    mons->add_ench(vines);
    mons->add_ench(forest);
    mons->add_ench(hexed);
    mons->add_ench(insanity);
    mons->add_ench(vengeance);

    mons->ench_countdown = old_ench_countdown;

    if (mons_class_flag(mons->type, M_INVIS))
        mons->add_ench(ENCH_INVIS);

    mons->hit_points = mons->max_hit_points * old_hp / old_hp_max;

    // Slimifying monsters gets you a fresh, delicious jiggly buddy.
    if (slimified)
        mons->hit_points = mons->max_hit_points;
    // Shapeshifters heal when they shift. Wow, why can't players do that?
    else if (shifter.ench != ENCH_NONE)
        mons->hit_points += random2(mons->max_hit_points);

    mons->hit_points = min(mons->max_hit_points, mons->hit_points);

    // Don't kill it.
    mons->hit_points = max(mons->hit_points, 1);

    mons->speed_increment = 67 + random2(6);

    monster_drop_things(mons);

    // If new monster is visible to player, then we've seen it.
    if (you.can_see(*mons))
    {
        // If the player saw both the beginning and end results of a
        // shifter changing, then s/he knows it must be a shifter.
        if (could_see && shifter.ench != ENCH_NONE)
            discover_shifter(*mons);
        if (do_seen)
            seen_monster(mons);
    }
    // generate a new polymorph set
    mons->props.erase(POLY_SET_KEY);
    init_poly_set(mons);

    if (old_mon_caught)
        check_net_will_hold_monster(mons);

    // Even if the new form can constrict, it might be with a different
    // body part. Likewise, the new form might be too large for its
    // current constrictor. Rather than trying to handle these as special
    // cases, just stop the constriction entirely. The usual message about
    // evaporating and reforming justifies this behaviour.
    mons->stop_constricting_all();
    mons->stop_being_constricted();
    mons->clear_far_engulf(true);
}

// Is the new monster able to live in *any* habitat that the original
// monster could?
// NOTE: this doesn't check for item-based flight, so if we
// poly an item-using flying monster into a non-item-using,
// non-flying monster, they could be in trouble. Not much way
// around this and it's a very very niche case.
// Also assumes the monster is not a derived (zombie etc) monster.
static bool _habitat_matches(bool orig_flies, habitat_type orig_hab,
                             monster_type new_type)
{
    if (monster_class_flies(new_type))
        return true;
    if (orig_flies)
        return false;

    const habitat_type new_hab = mons_habitat_type(new_type, new_type, false);
    switch (orig_hab)
    {
        case HT_AMPHIBIOUS:
        case HT_AMPHIBIOUS_LAVA:
            return new_hab == orig_hab;
        case HT_WATER:
            return new_hab == orig_hab || new_hab == HT_AMPHIBIOUS;
        case HT_LAVA:
            return new_hab == orig_hab || new_hab == HT_AMPHIBIOUS_LAVA;
        case HT_LAND:
            return new_hab == orig_hab
                || new_hab == HT_AMPHIBIOUS
                || new_hab == HT_AMPHIBIOUS_LAVA;
        case NUM_HABITATS:
            break;
    }
    return false; // should never happen
}

static int _goal_hd(int orig_hd, poly_power_type ppt)
{
    switch (ppt)
    {
    case PPT_LESS:
        return max(orig_hd - 3, 1);
    case PPT_MORE:
        return min(orig_hd + 2, 27);
    case PPT_SAME:
    default:
        return orig_hd;
    }
}

static int _poly_weight(int orig_hd, int orig_tier, monster_type new_species,
                        poly_power_type ppt)
{
    const int new_tier = mons_demon_tier(new_species);
    const int new_hd = mons_power(new_species);
    const int goal_hd = _goal_hd(orig_hd, ppt);

    int hd_delta = abs(goal_hd - new_hd);
    if (ppt != PPT_MORE && new_hd > goal_hd)
        hd_delta *= 2; // make upgrades less likely

    const int tier_delta = abs(orig_tier - new_tier);
    const int total_delta = hd_delta + tier_delta;
    const int max_delta = 8;
    if (total_delta > max_delta)
        return 0;
    // halve weight for each HD of difference and each demon tier level apart
    return 1 << (max_delta - total_delta);
}

static void _fill_poly_weights(const monster &mons, poly_power_type ppt,
                               bool hab_match,
                               map<monster_type, int> &weights)
{
    const int orig_tier = mons_demon_tier(mons.type);
    if (orig_tier == -1)
    {
        // panlords & hell lords get poly immunity. why? unclear to me
        // TODO: allow polying panlords into other random panlords, ha
        return;
    }

    const int orig_hd = mons.props.exists(ORIG_HD_KEY) ?
                            mons.props[ORIG_HD_KEY].get_int() :
                            mons.get_experience_level();
    const bool orig_flies = monster_inherently_flies(mons);
    const habitat_type orig_hab
        = mons_habitat_type(mons.type, mons_base_type(mons), false);

    for (monster_type mt = MONS_0; mt < NUM_MONSTERS; ++mt)
    {
        if (invalid_monster_type(mt))
            continue; // no polying into bugs

        const monster_type species = mons_species(mt);
        if (weights.find(species) != weights.end())
            continue; // already saw this one
        if (!_valid_type_morph(mons, species))
            continue; // no polying into statues, same species, etc

        if (hab_match && !_habitat_matches(orig_flies, orig_hab, species))
            continue;

        const int weight = _poly_weight(orig_hd, orig_tier, species, ppt);
        if (weight > 0)
            weights[species] = weight;
    }
}

void init_poly_set(monster *mons)
{
    rng::subgenerator poly_rng;

    if (mons->props.exists(POLY_SET_KEY))
        return;

    const int orig_tier = mons_demon_tier(mons->type);
    if (orig_tier == -1)
    {
        // panlords & hell lords get poly immunity. why? unclear to me
        // TODO: allow polying panlords into other random panlords, ha
        return;
    }

    map<monster_type, int> weights;
    _fill_poly_weights(*mons, PPT_SAME, true, weights);
    CrawlVector &set = mons->props[POLY_SET_KEY];
    for (int i = 0; i < 3; i++)
    {
        if (weights.empty())
            return; // can't choose any more
        const monster_type *chosen = random_choose_weighted<map<monster_type,int>>(weights);
        ASSERT(chosen);
        set.push_back(*chosen);
        weights.erase(*chosen);
    }
}

static monster_type _poly_from_set(monster &mons)
{
    if (!mons.props.exists(POLY_SET_KEY))
        return MONS_NO_MONSTER;
    const CrawlVector &set = mons.props[POLY_SET_KEY];
    if (set.size() <= 0)
        return MONS_NO_MONSTER;
    return (monster_type)set[random2(set.size())].get_int();
}

static monster_type _concretize_target(monster &mons, monster_type target,
                                       poly_power_type ppt)
{
    switch (target)
    {
    case RANDOM_POLYMORPH_MONSTER:
        return _poly_from_set(mons);
    case RANDOM_MONSTER:
    {
        map<monster_type, int> weights;
        _fill_poly_weights(mons, ppt, false, weights);
        if (weights.empty())
            return MONS_NO_MONSTER;
        const monster_type *chosen = random_choose_weighted<map<monster_type,int>>(weights);
        ASSERT(chosen);
        return *chosen;
    }
    default:
        return target;
    }
}

// If targetc == RANDOM_MONSTER, then relpower indicates the desired
// power of the new monster, relative to the current monster.
// Relaxation still takes effect when needed, no matter what relpower
// says.
bool monster_polymorph(monster* mons, monster_type targetc,
                       poly_power_type power)
{
    // Don't attempt to polymorph a monster that is busy using the stairs.
    if (mons->flags & MF_TAKING_STAIRS)
        return false;
    ASSERT(!(mons->flags & MF_BANISHED) || player_in_branch(BRANCH_ABYSS));

    // There's not a single valid target on the '&' demon tier, so unless we
    // make one, let's ban this outright.
    if (mons_demon_tier(mons->type) == -1)
    {
        return simple_monster_message(*mons,
            "'s appearance momentarily alters.");
    }

    targetc = _concretize_target(*mons, targetc, power);
    if (targetc == MONS_NO_MONSTER)
        return simple_monster_message(*mons, " shudders.");

    const bool was_invisible = mons->has_ench(ENCH_INVIS) && !mons->friendly();
    bool could_see = you.can_see(*mons);
    bool need_note = could_see && mons_is_notable(*mons);
    string old_name_a = mons->full_name(DESC_A);
    string old_name_the = mons->full_name(DESC_THE);
    monster_type oldc = mons->type;

    if (power != PPT_SLIME && !_valid_morph(mons, targetc))
        return simple_monster_message(*mons, " looks momentarily different.");

    change_monster_type(mons, targetc, false);

    bool can_see = you.can_see(*mons);

    // Messaging
    bool player_messaged = true;
    if (could_see)
    {
        string verb = "";
        string obj = can_see ? mons_type_name(targetc, DESC_A)
                             : "something you cannot see";

        if (oldc == MONS_OGRE && targetc == MONS_TWO_HEADED_OGRE)
        {
            verb = "grows a second head";
            obj = "";
        }
        else if (mons->is_shapeshifter())
            verb = "changes into ";
        else if (_jiyva_slime_target(targetc))
            verb = "quivers uncontrollably and liquefies into ";
        else
            verb = "evaporates and reforms as ";

        mprf("%s %s%s!", old_name_the.c_str(), verb.c_str(), obj.c_str());
    }
    else if (can_see)
        mprf("%s appears out of thin air!", mons->name(DESC_A).c_str());
    else
        player_messaged = false;

    if (need_note || could_see && can_see && mons_is_notable(*mons))
    {
        string new_name = can_see ? mons->full_name(DESC_A)
                                  : "something unseen";

        take_note(Note(NOTE_POLY_MONSTER, 0, 0, old_name_a, new_name));
    }

    const bool is_invisible = mons->has_ench(ENCH_INVIS) && !mons->friendly();
    if (you.see_cell(mons->pos()))
    {
        if (was_invisible && !is_invisible)
        {
            // If we poly an invisible monster reactivate autopickup.
            // We need to check for actual invisibility rather than
            // whether we can see the monster. There are several edge
            // cases where a monster is visible to the player but we
            // still need to turn autopickup back on, such as
            // TSO's halo or sticky flame.
            autotoggle_autopickup(false);
        }
        else if (could_see && !can_see)
            autotoggle_autopickup(true);
    }

    // do this here, so that any "changes into" notes come first
    if (can_see)
        seen_monster(mons);

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

bool mon_can_be_slimified(const monster* mons)
{
    const mon_holy_type holi = mons->holiness();

    return !mons->is_insubstantial()
           && !mons_is_tentacle_or_tentacle_segment(mons->type)
           && (holi & (MH_UNDEAD | MH_NATURAL) && !mons_is_slime(*mons));
}

static monster_type _slime_target(const monster &mon)
{
    const int hd = mon.get_hit_dice();
    const int target = random_range(hd - 4, hd + 4);
    if (!feat_has_solid_floor(env.grid(mon.pos())))
        return target < 7 ? MONS_JELLY : MONS_SLIME_CREATURE; // Don't drown.

    if (target < 3)
        return MONS_ENDOPLASM;
    if (target < 5)
        return MONS_JELLY;
    if (target < 12)
        return MONS_SLIME_CREATURE;
    if (coinflip())
        return MONS_ACID_BLOB;
    return MONS_AZURE_JELLY;
}

void slimify_monster(monster* mon)
{
    const monster_type target = _slime_target(*mon);

    // Bail out if jellies can't live here.
    if (!monster_habitable_grid(target, env.grid(mon->pos())))
    {
        simple_monster_message(*mon, " quivers momentarily.");
        return;
    }

    record_monster_defeat(mon, KILL_SLIMIFIED);
    remove_unique_annotation(mon);

    monster_polymorph(mon, target, PPT_SLIME);

    mon->attitude = ATT_GOOD_NEUTRAL;

    mons_make_god_gift(*mon, GOD_JIYVA);

    // Don't want shape-shifters to shift into non-slimes.
    mon->del_ench(ENCH_GLOWING_SHAPESHIFTER);
    mon->del_ench(ENCH_SHAPESHIFTER);

    mons_att_changed(mon);

    if (mons_is_elven_twin(mon))
        elven_twin_died(mon, false, KILL_YOU, MID_PLAYER);
}

void seen_monster(monster* mons)
{
    set_unique_annotation(mons);

    // id equipment (do this every time we see them, it may have changed)
    view_monster_equipment(mons);

    // Monster was viewed this turn
    mons->flags |= MF_WAS_IN_VIEW;

    if (mons->flags & MF_SEEN)
        return;

    // First time we've seen this particular monster.
    mons->flags |= MF_SEEN;

    if (crawl_state.game_is_hints())
        hints_monster_seen(*mons);

    if (mons_is_notable(*mons))
    {
        string name = mons->name(DESC_A, true);
        if (mons->type == MONS_PLAYER_GHOST)
        {
            name += make_stringf(" (%s)",
                                 short_ghost_description(mons, true).c_str());
        }
        else if (mons->flags & MF_KNOWN_SHIFTER)
        {
            name += make_stringf(" (%sshapeshifter)",
                mons->has_ench(ENCH_GLOWING_SHAPESHIFTER) ? "glowing " : "");
        }
        take_note(Note(NOTE_SEEN_MONSTER, mons->type, 0, name));
    }

    if (player_equip_unrand(UNRAND_WYRMBANE))
    {
        const item_def *wyrmbane = you.weapon();
        if (wyrmbane && mons->dragon_level() > wyrmbane->plus)
            mpr("<green>Wyrmbane glows as a worthy foe approaches.</green>");
    }

    // attempt any god conversions on first sight
    do_conversions(mons);

    if (!(mons->flags & MF_TSO_SEEN))
    {
        if (mons_gives_xp(*mons, you) && !crawl_state.game_is_arena())
        {
            did_god_conduct(DID_SEE_MONSTER, mons->get_experience_level(),
                            true, mons);
        }
        mons->flags |= MF_TSO_SEEN;
    }

    if (mons_offers_beogh_conversion(*mons))
        env.level_state |= LSTATE_BEOGH;
}
