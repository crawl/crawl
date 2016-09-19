/**
 * @file
 * @brief Monster information that may be passed to the user.
 *
 * Used to fill the monster pane and to pass monster info to Lua.
**/

#include "AppHdr.h"

#include "mon-info.h"

#include <algorithm>
#include <sstream>

#include "act-iter.h"
#include "artefact.h"
#include "colour.h"
#include "coordit.h"
#include "english.h"
#include "env.h"
#include "fight.h"
#include "godabil.h"
#include "ghost.h"
#include "itemname.h"
#include "itemprop.h"
#include "libutil.h"
#include "los.h"
#include "message.h"
#include "mon-book.h"
#include "mon-death.h" // ELVEN_IS_ENERGIZED_KEY
#include "mon-tentacle.h"
#include "nearby-danger.h"
#include "options.h"
#include "religion.h"
#include "skills.h"
#include "spl-goditem.h" // dispellable_enchantments
#include "spl-summoning.h"
#include "state.h"
#include "stringutil.h"
#ifdef USE_TILE
#include "tilepick.h"
#endif
#include "traps.h"

/// Simple 1:1 mappings between monster enchantments & info flags.
static map<enchant_type, monster_info_flags> trivial_ench_mb_mappings = {
    { ENCH_BERSERK,         MB_BERSERK },
    { ENCH_POISON,          MB_POISONED },
    { ENCH_CORONA,          MB_GLOWING },
    { ENCH_SILVER_CORONA,   MB_GLOWING },
    { ENCH_SLOW,            MB_SLOWED },
    { ENCH_SICK,            MB_SICK },
    { ENCH_INSANE,          MB_INSANE },
    { ENCH_HASTE,           MB_HASTED },
    { ENCH_MIGHT,           MB_STRONG },
    { ENCH_CONFUSION,       MB_CONFUSED },
    { ENCH_INVIS,           MB_INVISIBLE },
    { ENCH_CHARM,           MB_CHARMED },
    { ENCH_STICKY_FLAME,    MB_BURNING },
    { ENCH_PETRIFIED,       MB_PETRIFIED },
    { ENCH_PETRIFYING,      MB_PETRIFYING },
    { ENCH_LOWERED_MR,      MB_VULN_MAGIC },
    { ENCH_SWIFT,           MB_SWIFT },
    { ENCH_SILENCE,         MB_SILENCING },
    { ENCH_PARALYSIS,       MB_PARALYSED },
    { ENCH_SOUL_RIPE,       MB_POSSESSABLE },
    { ENCH_REGENERATION,    MB_REGENERATION },
    { ENCH_RAISED_MR,       MB_RAISED_MR },
    { ENCH_MIRROR_DAMAGE,   MB_MIRROR_DAMAGE },
    { ENCH_FEAR_INSPIRING,  MB_FEAR_INSPIRING },
    { ENCH_DAZED,           MB_DAZED },
    { ENCH_MUTE,            MB_MUTE },
    { ENCH_BLIND,           MB_BLIND },
    { ENCH_DUMB,            MB_DUMB },
    { ENCH_MAD,             MB_MAD },
    { ENCH_INNER_FLAME,     MB_INNER_FLAME },
    { ENCH_BREATH_WEAPON,   MB_BREATH_WEAPON },
    { ENCH_OZOCUBUS_ARMOUR, MB_OZOCUBUS_ARMOUR },
    { ENCH_WRETCHED,        MB_WRETCHED },
    { ENCH_SCREAMED,        MB_SCREAMED },
    { ENCH_WORD_OF_RECALL,  MB_WORD_OF_RECALL },
    { ENCH_INJURY_BOND,     MB_INJURY_BOND },
    { ENCH_FLAYED,          MB_FLAYED },
    { ENCH_WEAK,            MB_WEAK },
    { ENCH_DIMENSION_ANCHOR, MB_DIMENSION_ANCHOR },
    { ENCH_TOXIC_RADIANCE,  MB_TOXIC_RADIANCE },
    { ENCH_GRASPING_ROOTS,  MB_GRASPING_ROOTS },
    { ENCH_FIRE_VULN,       MB_FIRE_VULN },
    { ENCH_TORNADO,         MB_TORNADO },
    { ENCH_TORNADO_COOLDOWN, MB_TORNADO_COOLDOWN },
    { ENCH_BARBS,           MB_BARBS },
    { ENCH_POISON_VULN,     MB_POISON_VULN },
    { ENCH_ICEMAIL,         MB_ICEMAIL },
    { ENCH_AGILE,           MB_AGILE },
    { ENCH_FROZEN,          MB_FROZEN },
    { ENCH_BLACK_MARK,      MB_BLACK_MARK },
    { ENCH_SAP_MAGIC,       MB_SAP_MAGIC },
    { ENCH_SHROUD,          MB_SHROUD },
    { ENCH_CORROSION,       MB_CORROSION },
    { ENCH_REPEL_MISSILES,  MB_REPEL_MSL },
    { ENCH_DEFLECT_MISSILES, MB_DEFLECT_MSL },
    { ENCH_RESISTANCE,      MB_RESISTANCE },
    { ENCH_HEXED,           MB_HEXED },
    { ENCH_BONE_ARMOUR,     MB_BONE_ARMOUR },
    { ENCH_BRILLIANCE_AURA, MB_BRILLIANCE_AURA },
    { ENCH_EMPOWERED_SPELLS, MB_EMPOWERED_SPELLS },
    { ENCH_GOZAG_INCITE,    MB_GOZAG_INCITED },
    { ENCH_PAIN_BOND,       MB_PAIN_BOND },
    { ENCH_IDEALISED,       MB_IDEALISED },
    { ENCH_BOUND_SOUL,      MB_BOUND_SOUL },
    { ENCH_INFESTATION,     MB_INFESTATION },
    { ENCH_STILL_WINDS,     MB_STILL_WINDS },
    { ENCH_SLOWLY_DYING,    MB_SLOWLY_DYING },
};

static monster_info_flags ench_to_mb(const monster& mons, enchant_type ench)
{
    // Suppress silly-looking combinations, even if they're
    // internally valid.
    if (mons.paralysed() && (ench == ENCH_SLOW || ench == ENCH_HASTE
                      || ench == ENCH_SWIFT
                      || ench == ENCH_PETRIFIED
                      || ench == ENCH_PETRIFYING))
    {
        return NUM_MB_FLAGS;
    }

    if (ench == ENCH_PETRIFIED && mons.has_ench(ENCH_PETRIFYING))
        return NUM_MB_FLAGS;

    // Don't claim that naturally 'confused' monsters are especially bewildered
    if (ench == ENCH_CONFUSION && mons_class_flag(mons.type, M_CONFUSED))
        return NUM_MB_FLAGS;

    const monster_info_flags *flag = map_find(trivial_ench_mb_mappings, ench);
    if (flag)
        return *flag;

    switch (ench)
    {
    case ENCH_HELD:
        return get_trapping_net(mons.pos(), true) == NON_ITEM
               ? MB_WEBBED : MB_CAUGHT;
    case ENCH_WATER_HOLD:
        if (mons.res_water_drowning())
            return MB_WATER_HOLD;
        else
            return MB_WATER_HOLD_DROWN;
    case ENCH_DRAINED:
        {
            const bool heavily_drained = mons.get_ench(ench).degree
                                         >= mons.get_experience_level() / 2;
            return heavily_drained ? MB_HEAVILY_DRAINED : MB_LIGHTLY_DRAINED;
        }
    case ENCH_SPELL_CHARGED:
        if (mons.get_ench(ench).degree < max_mons_charge(mons.type))
            return MB_PARTIALLY_CHARGED;
        return MB_FULLY_CHARGED;
    default:
        return NUM_MB_FLAGS;
    }
}

static bool _blocked_ray(const coord_def &where,
                         dungeon_feature_type* feat = nullptr)
{
    if (exists_ray(you.pos(), where, opc_solid_see)
        || !exists_ray(you.pos(), where, opc_default))
    {
        return false;
    }
    if (feat == nullptr)
        return true;
    *feat = ray_blocker(you.pos(), where);
    return true;
}

static bool _is_public_key(string key)
{
    if (key == "helpless"
     || key == "wand_known"
     || key == "feat_type"
     || key == "glyph"
     || key == "dbname"
     || key == "monster_tile"
#ifdef USE_TILE
     || key == TILE_NUM_KEY
#endif
     || key == "tile_idx"
     || key == CUSTOM_SPELLS_KEY
     || key == ELVEN_IS_ENERGIZED_KEY
     || key == MUTANT_BEAST_FACETS
     || key == MUTANT_BEAST_TIER
     || key == DOOM_HOUND_HOWLED_KEY
     || key == MON_GENDER_KEY
     || key == SEEN_SPELLS_KEY)
    {
        return true;
    }

    return false;
}

static int quantise(int value, int stepsize)
{
    return value + stepsize - value % stepsize;
}

// Returns true if using a directional tentacle tile would leak
// information the player doesn't have about a tentacle segment's
// current position.
static bool _tentacle_pos_unknown(const monster *tentacle,
                                  const coord_def orig_pos)
{
    // We can see the segment, no guessing necessary.
    if (!tentacle->submerged())
        return false;

    const coord_def t_pos = tentacle->pos();

    // Checks whether there are any positions adjacent to the
    // original tentacle that might also contain the segment.
    for (adjacent_iterator ai(orig_pos); ai; ++ai)
    {
        if (*ai == t_pos)
            continue;

        if (!in_bounds(*ai))
            continue;

        if (you.pos() == *ai)
            continue;

        // If there's an adjacent deep water tile, the segment
        // might be there instead.
        if (grd(*ai) == DNGN_DEEP_WATER)
        {
            const monster *mon = monster_at(*ai);
            if (mon && you.can_see(*mon))
            {
                // Could originate from the kraken.
                if (mon->type == MONS_KRAKEN)
                    return true;

                // Otherwise, we know the segment can't be there.
                continue;
            }
            return true;
        }

        if (grd(*ai) == DNGN_SHALLOW_WATER)
        {
            const monster *mon = monster_at(*ai);

            // We know there's no segment there.
            if (!mon)
                continue;

            // Disturbance in shallow water -> might be a tentacle.
            if (mon->type == MONS_KRAKEN || mon->submerged())
                return true;
        }
    }

    // Using a directional tile leaks no information.
    return false;
}

static void _translate_tentacle_ref(monster_info& mi, const monster* m,
                                    const string &key)
{
    if (!m->props.exists(key))
        return;

    const monster* other = monster_by_mid(m->props[key].get_int());
    if (other)
    {
        coord_def h_pos = other->pos();
        // If the tentacle and the other segment are no longer adjacent
        // (distortion etc.), just treat them as not connected.
        if (adjacent(m->pos(), h_pos)
            && !mons_is_zombified(*other)
            && !_tentacle_pos_unknown(other, m->pos()))
        {
            mi.props[key] = h_pos - m->pos();
        }
    }
}

/// is the given monster_info a hydra, zombie hydra, lerny, etc?
static bool _is_hydra(const monster_info &mi)
{
    return mons_genus(mi.type) == MONS_HYDRA
           || mons_genus(mi.base_type) == MONS_HYDRA;
}

monster_info::monster_info(monster_type p_type, monster_type p_base_type)
{
    mb.reset();
    attitude = ATT_HOSTILE;
    pos = coord_def(0, 0);

    type = p_type;

    // give 'job' monsters a default race.
    const bool classy_drac = mons_is_draconian_job(type) || type == MONS_TIAMAT;
    base_type = p_base_type != MONS_NO_MONSTER ? p_base_type
                : classy_drac ? MONS_DRACONIAN
                : mons_is_demonspawn_job(type) ? MONS_DEMONSPAWN
                : type;

    if (_is_hydra(*this))
        num_heads = 1;
    else
        number = 0;

    _colour = COLOUR_INHERIT;

    holi = mons_class_holiness(type);

    mintel = mons_class_intel(type);

    hd = mons_class_hit_dice(type);
    ac = get_mons_class_ac(type);
    ev = base_ev = get_mons_class_ev(type);
    mresists = get_mons_class_resists(type);
    mr = mons_class_res_magic(type, base_type);
    can_see_invis = mons_class_sees_invis(type, base_type);

    mitemuse = mons_class_itemuse(type);

    mbase_speed = mons_class_base_speed(type);
    menergy = mons_class_energy(type);

    if (mons_class_flag(type, M_FLIES) || mons_class_flag(base_type, M_FLIES))
        mb.set(MB_AIRBORNE);

    if (mons_class_wields_two_weapons(type)
        || mons_class_wields_two_weapons(base_type))
    {
        mb.set(MB_TWO_WEAPONS);
    }

    if (!mons_class_can_regenerate(type)
        || !mons_class_can_regenerate(base_type))
    {
        mb.set(MB_NO_REGEN);
    }

    threat = MTHRT_UNDEF;

    dam = MDAM_OKAY;

    fire_blocker = DNGN_UNSEEN;

    i_ghost.acting_part = MONS_0;
    if (mons_is_pghost(type))
    {
        i_ghost.species = SP_HUMAN;
        i_ghost.job = JOB_WANDERER;
        i_ghost.religion = GOD_NO_GOD;
        i_ghost.best_skill = SK_FIGHTING;
        i_ghost.best_skill_rank = 2;
        i_ghost.xl_rank = 3;
        hd = ghost_rank_to_level(i_ghost.xl_rank);
        i_ghost.ac = 5;
        i_ghost.damage = 5;
    }

    if (mons_is_job(type))
    {
        ac += get_mons_class_ac(base_type);
        ev += get_mons_class_ev(base_type);
    }

    if (mons_is_unique(type))
    {
        if (type == MONS_LERNAEAN_HYDRA
            || type == MONS_ROYAL_JELLY
            || mons_species(type) == MONS_SERPENT_OF_HELL)
        {
            mb.set(MB_NAME_THE);
        }
        else
        {
            mb.set(MB_NAME_UNQUALIFIED);
            mb.set(MB_NAME_THE);
        }
    }

    for (int i = 0; i < MAX_NUM_ATTACKS; ++i)
    {
        attack[i] = get_monster_data(type)->attack[i];
        attack[i].damage = 0;
    }

    props.clear();
    // At least enough to keep from crashing. TODO: allow specifying these?
    if (type == MONS_MUTANT_BEAST)
    {
        props[MUTANT_BEAST_TIER].get_short() = BT_FIRST;
        for (int i = BF_FIRST; i < NUM_BEAST_FACETS; ++i)
            props[MUTANT_BEAST_FACETS].get_vector().push_back(i);
    }

    client_id = 0;
}

static description_level_type _article_for(const actor* a)
{
    // Player gets DESC_A, but that doesn't really matter.
    const monster * const m = a->as_monster();
    return m && m->friendly() ? DESC_YOUR : DESC_A;
}

monster_info::monster_info(const monster* m, int milev)
{
    ASSERT(m); // TODO: change to const monster &mon
    mb.reset();
    attitude = ATT_HOSTILE;
    pos = m->pos();

    attitude = mons_attitude(*m);

    type = m->type;
    threat = mons_threat_level(*m);

    props.clear();
    // CrawlHashTable::begin() const can fail if the hash is empty.
    if (!m->props.empty())
    {
        for (const auto &entry : m->props)
            if (_is_public_key(entry.first))
                props[entry.first] = entry.second;
    }

    // Translate references to tentacles into just their locations
    if (mons_is_tentacle_or_tentacle_segment(type))
    {
        _translate_tentacle_ref(*this, m, "inwards");
        _translate_tentacle_ref(*this, m, "outwards");
    }

    base_type = m->base_monster;
    if (base_type == MONS_NO_MONSTER)
        base_type = type;

    if (type == MONS_SLIME_CREATURE)
        slime_size = m->blob_size;
    else if (type == MONS_BALLISTOMYCETE)
        is_active = !!m->ballisto_activity;
    else if (_is_hydra(*this))
        num_heads = m->num_heads;
    // others use number for internal information
    else
        number = 0;

    _colour = m->colour;

    if (m->is_summoned()
        && (!m->has_ench(ENCH_PHANTOM_MIRROR) || m->friendly()))
    {
        mb.set(MB_SUMMONED);
    }
    else if (m->is_perm_summoned())
        mb.set(MB_PERM_SUMMON);
    else if (testbits(m->flags, MF_NO_REWARD)
             && mons_class_gives_xp(m->type, true))
    {
        mb.set(MB_NO_REWARD);
    }

    if (m->has_ench(ENCH_SUMMON_CAPPED))
        mb.set(MB_SUMMONED_CAPPED);

    if (mons_is_unique(type))
    {
        if (type == MONS_LERNAEAN_HYDRA
            || type == MONS_ROYAL_JELLY
            || mons_species(type) == MONS_SERPENT_OF_HELL)
        {
            mb.set(MB_NAME_THE);
        }
        else
        {
            mb.set(MB_NAME_UNQUALIFIED);
            mb.set(MB_NAME_THE);
        }
    }

    mname = m->mname;

    const auto name_flags = m->flags & MF_NAME_MASK;

    if (name_flags == MF_NAME_SUFFIX)
        mb.set(MB_NAME_SUFFIX);
    else if (name_flags == MF_NAME_ADJECTIVE)
        mb.set(MB_NAME_ADJECTIVE);
    else if (name_flags == MF_NAME_REPLACE)
        mb.set(MB_NAME_REPLACE);

    const bool need_name_desc =
        name_flags == MF_NAME_SUFFIX
            || name_flags == MF_NAME_ADJECTIVE
            || (m->flags & MF_NAME_DEFINITE);

    if (!mname.empty()
        && !(m->flags & MF_NAME_DESCRIPTOR)
        && !need_name_desc)
    {
        mb.set(MB_NAME_UNQUALIFIED);
        mb.set(MB_NAME_THE);
    }
    else if (m->flags & MF_NAME_DEFINITE)
        mb.set(MB_NAME_THE);
    if (m->flags & MF_NAME_ZOMBIE)
        mb.set(MB_NAME_ZOMBIE);
    if (m->flags & MF_NAME_SPECIES)
        mb.set(MB_NO_NAME_TAG);

    // Ghostliness needed for name
    if (testbits(m->flags, MF_SPECTRALISED))
        mb.set(MB_SPECTRALISED);

    if (milev <= MILEV_NAME)
    {
        if (type == MONS_DANCING_WEAPON
            && m->inv[MSLOT_WEAPON] != NON_ITEM)
        {
            inv[MSLOT_WEAPON].reset(
                new item_def(get_item_info(mitm[m->inv[MSLOT_WEAPON]])));
        }
        return;
    }

    holi = m->holiness();

    mintel = mons_intel(*m);
    hd = m->get_hit_dice();
    ac = m->armour_class(false);
    ev = m->evasion(EV_IGNORE_UNIDED);
    base_ev = m->base_evasion();
    mr = m->res_magic(false);
    can_see_invis = m->can_see_invisible(false);
    mresists = get_mons_resists(*m);
    mitemuse = mons_itemuse(*m);
    mbase_speed = mons_base_speed(*m, true);
    menergy = mons_energy(*m);

    if (m->airborne())
        mb.set(MB_AIRBORNE);
    if (mons_wields_two_weapons(*m))
        mb.set(MB_TWO_WEAPONS);
    if (!mons_can_regenerate(*m))
        mb.set(MB_NO_REGEN);
    if (m->haloed() && !m->umbraed())
        mb.set(MB_HALOED);
    if (!m->haloed() && m->umbraed())
        mb.set(MB_UMBRAED);
    if (mons_looks_stabbable(*m))
        mb.set(MB_STABBABLE);
    if (mons_looks_distracted(*m))
        mb.set(MB_DISTRACTED);
    if (m->liquefied_ground())
        mb.set(MB_SLOW_MOVEMENT);
    if (m->is_wall_clinging())
        mb.set(MB_CLINGING);

    dam = mons_get_damage_level(*m);

    if (mons_is_threatening(*m)) // Firewood, butterflies, etc.
    {
        if (m->asleep())
        {
            if (!m->can_hibernate(true))
                mb.set(MB_DORMANT);
            else
                mb.set(MB_SLEEPING);
        }
        // Applies to both friendlies and hostiles
        else if (mons_is_fleeing(*m))
            mb.set(MB_FLEEING);
        else if (mons_is_wandering(*m) && !mons_is_batty(*m))
        {
            if (m->is_stationary())
                mb.set(MB_UNAWARE);
            else
                mb.set(MB_WANDERING);
        }
        // TODO: is this ever needed?
        else if (m->foe == MHITNOT && !mons_is_batty(*m)
                 && m->attitude == ATT_HOSTILE)
        {
            mb.set(MB_UNAWARE);
        }
    }

    for (auto &entry : m->enchantments)
    {
        monster_info_flags flag = ench_to_mb(*m, entry.first);
        if (flag != NUM_MB_FLAGS)
            mb.set(flag);
    }

    if (type == MONS_SILENT_SPECTRE)
        mb.set(MB_SILENCING);

    if (you.beheld_by(*m))
        mb.set(MB_MESMERIZING);

    // Evilness of attacking
    switch (attitude)
    {
    case ATT_NEUTRAL:
    case ATT_HOSTILE:
        if (you_worship(GOD_SHINING_ONE)
            && !tso_unchivalric_attack_safe_monster(*m)
            && find_stab_type(&you, *m) != STAB_NO_STAB)
        {
            mb.set(MB_EVIL_ATTACK);
        }
        break;
    default:
        break;
    }

    if (testbits(m->flags, MF_ENSLAVED_SOUL))
        mb.set(MB_ENSLAVED);

    if (m->is_shapeshifter() && (m->flags & MF_KNOWN_SHIFTER))
        mb.set(MB_SHAPESHIFTER);

    if (m->known_chaos())
        mb.set(MB_CHAOTIC);

    if (m->submerged())
        mb.set(MB_SUBMERGED);

    if (m->type == MONS_DOOM_HOUND && !m->props.exists(DOOM_HOUND_HOWLED_KEY)
        && !m->is_summoned())
    {
        mb.set(MB_READY_TO_HOWL);
    }

    if (mons_is_pghost(type))
    {
        ASSERT(m->ghost.get());
        ghost_demon& ghost = *m->ghost;
        i_ghost.species = ghost.species;
        if (species_is_draconian(i_ghost.species) && ghost.xl < 7)
            i_ghost.species = SP_BASE_DRACONIAN;
        i_ghost.job = ghost.job;
        i_ghost.religion = ghost.religion;
        i_ghost.best_skill = ghost.best_skill;
        i_ghost.best_skill_rank = get_skill_rank(ghost.best_skill_level);
        i_ghost.xl_rank = ghost_level_to_rank(ghost.xl);
        i_ghost.ac = quantise(ghost.ac, 5);
        i_ghost.damage = quantise(ghost.damage, 5);

        // describe abnormal (branded) ghost weapons
        if (ghost.brand != SPWPN_NORMAL)
            props[SPECIAL_WEAPON_KEY] = ghost_brand_name(ghost.brand);
    }

    // book loading for player ghost and vault monsters
    spells.clear();
    if (m->props.exists(CUSTOM_SPELLS_KEY) || mons_is_pghost(type))
        spells = m->spells;
    if (m->is_priest())
        props["priest"] = true;
    else if (m->is_actual_spellcaster())
        props["actual_spellcaster"] = true;

    for (int i = 0; i < MAX_NUM_ATTACKS; ++i)
    {
        attack[i] = mons_attack_spec(*m, i, true);
        attack[i].damage = 0;
    }

    for (unsigned i = 0; i <= MSLOT_LAST_VISIBLE_SLOT; ++i)
    {
        bool ok;
        if (m->inv[i] == NON_ITEM)
            ok = false;
        else if (i == MSLOT_MISCELLANY)
            ok = false;
        else if (attitude == ATT_FRIENDLY)
            ok = true;
        else if (i == MSLOT_WAND)
            ok = props.exists("wand_known") && props["wand_known"];
        else if (m->props.exists("ash_id") && item_type_known(mitm[m->inv[i]]))
            ok = true;
        else if (i == MSLOT_ALT_WEAPON)
            ok = wields_two_weapons();
        else if (i == MSLOT_MISSILE)
            ok = false;
        else
            ok = true;
        if (ok)
            inv[i].reset(new item_def(get_item_info(mitm[m->inv[i]])));
    }

    fire_blocker = DNGN_UNSEEN;
    if (!crawl_state.arena_suspended
        && m->pos() != you.pos())
    {
        _blocked_ray(m->pos(), &fire_blocker);
    }

    if (m->props.exists("quote"))
        quote = m->props["quote"].get_string();

    if (m->props.exists("description"))
        description = m->props["description"].get_string();

    // init names of constrictor and constrictees
    constrictor_name = "";
    constricting_name.clear();

    // name of what this monster is constricted by, if any
    if (m->is_constricted())
    {
        const actor * const constrictor = actor_by_mid(m->constricted_by);
        if (constrictor)
        {
            constrictor_name = (m->held == HELD_MONSTER ? "held by "
                                                        : "constricted by ")
                               + constrictor->name(_article_for(constrictor),
                                                   true);
        }
    }

    // names of what this monster is constricting, if any
    if (m->constricting)
    {
        const char *gerund = m->constriction_damage() ? "constricting "
                                                      : "holding ";
        for (const auto &entry : *m->constricting)
        {
            if (const actor* const constrictee = actor_by_mid(entry.first))
            {
                constricting_name.push_back(gerund
                                            + constrictee->name(
                                                  _article_for(constrictee),
                                                  true));
            }
        }
    }

    if (mons_has_known_ranged_attack(*m))
        mb.set(MB_RANGED_ATTACK);

    // this must be last because it provides this structure to Lua code
    if (milev > MILEV_SKIP_SAFE)
    {
        if (mons_is_safe(m))
            mb.set(MB_SAFE);
        else
            mb.set(MB_UNSAFE);
        if (mons_is_firewood(*m))
            mb.set(MB_FIREWOOD);
    }

    client_id = m->get_client_id();
}



/**
 * Name the given mutant beast tier.
 *
 * @param xl_tier   The beast_tier in question.
 * @return          The name of the tier; e.g. "juvenile".
 */
static string _mutant_beast_tier_name(short xl_tier)
{
    if (xl_tier < 0 || xl_tier >= NUM_BEAST_TIERS)
        return "buggy";
    return mutant_beast_tier_names[xl_tier];
}

/**
 * Name the given mutant beast facet.
 *
 * @param xl_tier   The beast_facet in question.
 * @return          The name of the facet; e.g. "bat".
 */
static string _mutant_beast_facet(int facet)
{
    if (facet < 0 || facet >= NUM_BEAST_FACETS)
        return "buggy";
    return mutant_beast_facet_names[facet];
}


string monster_info::db_name() const
{
    if (type == MONS_DANCING_WEAPON && inv[MSLOT_WEAPON].get())
    {
        iflags_t ignore_flags = ISFLAG_KNOW_CURSE | ISFLAG_KNOW_PLUSES;
        bool     use_inscrip  = false;
        return inv[MSLOT_WEAPON]->name(DESC_DBNAME, false, false, use_inscrip, false,
                         ignore_flags);
    }

    if (type == MONS_SENSED)
        return get_monster_data(base_type)->name;

    return get_monster_data(type)->name;
}

string monster_info::_core_name() const
{
    monster_type nametype = type;

    if (mons_class_is_zombified(type))
    {
        if (mons_is_unique(base_type))
            nametype = mons_species(base_type);
        else
            nametype = base_type;
    }
    else if (type == MONS_PILLAR_OF_SALT
             || type == MONS_BLOCK_OF_ICE
             || type == MONS_SENSED)
    {
        nametype = base_type;
    }

    string s;

    if (is(MB_NAME_REPLACE))
        s = mname;
    else if (nametype == MONS_LERNAEAN_HYDRA)
        s = "Lernaean hydra"; // TODO: put this into mon-data.h
    else if (nametype == MONS_ROYAL_JELLY)
        s = "Royal Jelly";
    else if (mons_species(nametype) == MONS_SERPENT_OF_HELL)
        s = "Serpent of Hell";
    else if (invalid_monster_type(nametype) && nametype != MONS_PROGRAM_BUG)
        s = "INVALID MONSTER";
    else
    {
        const char* slime_sizes[] = {"buggy ", "", "large ", "very large ",
                                               "enormous ", "titanic "};
        s = get_monster_data(nametype)->name;

        if (mons_is_draconian_job(type) && base_type != MONS_NO_MONSTER)
            s = draconian_colour_name(base_type) + " " + s;
        else if (mons_is_demonspawn_job(type) && base_type != MONS_NO_MONSTER)
            s = demonspawn_base_name(base_type) + " " + s;

        switch (type)
        {
        case MONS_SLIME_CREATURE:
            ASSERT((size_t) slime_size <= ARRAYSZ(slime_sizes));
            s = slime_sizes[slime_size] + s;
            break;
        case MONS_UGLY_THING:
        case MONS_VERY_UGLY_THING:
            s = ugly_thing_colour_name(_colour) + " " + s;
            break;

        case MONS_DANCING_WEAPON:
        case MONS_SPECTRAL_WEAPON:
            if (inv[MSLOT_WEAPON].get())
            {
                iflags_t ignore_flags = ISFLAG_KNOW_CURSE | ISFLAG_KNOW_PLUSES;
                bool     use_inscrip  = true;
                const item_def& item = *inv[MSLOT_WEAPON];
                s = type==MONS_SPECTRAL_WEAPON ? "spectral " : "";
                s += (item.name(DESC_PLAIN, false, false, use_inscrip, false,
                                ignore_flags));
            }
            break;

        case MONS_PLAYER_GHOST:
            s = apostrophise(mname) + " ghost";
            break;
        case MONS_PLAYER_ILLUSION:
            s = apostrophise(mname) + " illusion";
            break;
        case MONS_PANDEMONIUM_LORD:
            s = mname;
            break;
        default:
            break;
        }
    }

    //XXX: Hack to get poly'd TLH's name on death to look right.
    if (is(MB_NAME_SUFFIX) && type != MONS_LERNAEAN_HYDRA)
        s += " " + mname;
    else if (is(MB_NAME_ADJECTIVE))
        s = mname + " " + s;

    return s;
}

string monster_info::_apply_adjusted_description(description_level_type desc,
                                                 const string& s) const
{
    if (desc == DESC_ITS)
        desc = DESC_THE;

    if (is(MB_NAME_THE) && desc == DESC_A)
        desc = DESC_THE;

    if (attitude == ATT_FRIENDLY && desc == DESC_THE)
        desc = DESC_YOUR;

    return apply_description(desc, s);
}

string monster_info::common_name(description_level_type desc) const
{
    const string core = _core_name();
    const bool nocore = mons_class_is_zombified(type)
                          && mons_is_unique(base_type)
                          && base_type == mons_species(base_type)
                        || type == MONS_MUTANT_BEAST && !is(MB_NAME_REPLACE);

    ostringstream ss;

    if (props.exists("helpless"))
        ss << "helpless ";

    if (is(MB_SUBMERGED))
        ss << "submerged ";

    if (type == MONS_SPECTRAL_THING && !is(MB_NAME_ZOMBIE) && !nocore)
        ss << "spectral ";

    if (is(MB_SPECTRALISED))
        ss << "ghostly ";

    if (type == MONS_SENSED && !mons_is_sensed(base_type))
        ss << "sensed ";

    if (type == MONS_BALLISTOMYCETE)
        ss << (is_active ? "active " : "");

    if (_is_hydra(*this)
        && type != MONS_SENSED
        && type != MONS_BLOCK_OF_ICE
        && type != MONS_PILLAR_OF_SALT)
    {
        ASSERT(num_heads > 0);
        if (num_heads < 11)
            ss << number_in_words(num_heads);
        else
            ss << std::to_string(num_heads);

        ss << "-headed ";
    }

    if (type == MONS_MUTANT_BEAST && !is(MB_NAME_REPLACE))
    {
        const int xl = props[MUTANT_BEAST_TIER].get_short();
        const int tier = mutant_beast_tier(xl);
        ss << _mutant_beast_tier_name(tier) << " ";
        for (auto facet : props[MUTANT_BEAST_FACETS].get_vector())
            ss << _mutant_beast_facet(facet.get_int()); // no space between
        ss << " beast";
    }

    if (!nocore)
        ss << core;

    // Add suffixes.
    switch (type)
    {
    case MONS_ZOMBIE:
#if TAG_MAJOR_VERSION == 34
    case MONS_ZOMBIE_SMALL:
    case MONS_ZOMBIE_LARGE:
#endif
        if (!is(MB_NAME_ZOMBIE))
            ss << (nocore ? "" : " ") << "zombie";
        break;
    case MONS_SKELETON:
#if TAG_MAJOR_VERSION == 34
    case MONS_SKELETON_SMALL:
    case MONS_SKELETON_LARGE:
#endif
        if (!is(MB_NAME_ZOMBIE))
            ss << (nocore ? "" : " ") << "skeleton";
        break;
    case MONS_SIMULACRUM:
#if TAG_MAJOR_VERSION == 34
    case MONS_SIMULACRUM_SMALL:
    case MONS_SIMULACRUM_LARGE:
#endif
        if (!is(MB_NAME_ZOMBIE))
            ss << (nocore ? "" : " ") << "simulacrum";
        break;
    case MONS_SPECTRAL_THING:
        if (nocore)
            ss << "spectre";
        break;
    case MONS_PILLAR_OF_SALT:
        ss << (nocore ? "" : " ") << "shaped pillar of salt";
        break;
    case MONS_BLOCK_OF_ICE:
        ss << (nocore ? "" : " ") << "shaped block of ice";
        break;
    default:
        break;
    }

    if (is(MB_SHAPESHIFTER))
    {
        // If momentarily in original form, don't display "shaped
        // shifter".
        if (mons_genus(type) != MONS_SHAPESHIFTER)
            ss << " shaped shifter";
    }

    string s;
    // only respect unqualified if nothing was added ("Sigmund" or "The spectral Sigmund")
    if (!is(MB_NAME_UNQUALIFIED) || has_proper_name() || ss.str() != core)
        s = _apply_adjusted_description(desc, ss.str());
    else
        s = ss.str();

    if (desc == DESC_ITS)
        s = apostrophise(s);

    return s;
}

bool monster_info::has_proper_name() const
{
    return !mname.empty() && !mons_is_ghost_demon(type)
            && !is(MB_NAME_REPLACE) && !is(MB_NAME_ADJECTIVE) && !is(MB_NAME_SUFFIX);
}

string monster_info::proper_name(description_level_type desc) const
{
    if (has_proper_name())
    {
        if (desc == DESC_ITS)
            return apostrophise(mname);
        else
            return mname;
    }
    else
        return common_name(desc);
}

string monster_info::full_name(description_level_type desc) const
{
    if (desc == DESC_NONE)
        return "";

    if (has_proper_name())
    {
        string s = mname + " the " + common_name();
        if (desc == DESC_ITS)
            s = apostrophise(s);
        return s;
    }
    else
        return common_name(desc);
}

// Needed because gcc 4.3 sort does not like comparison functions that take
// more than 2 arguments.
bool monster_info::less_than_wrapper(const monster_info& m1,
                                     const monster_info& m2)
{
    return monster_info::less_than(m1, m2, true);
}

// Sort monsters by (in that order):    attitude, difficulty, type, brand
bool monster_info::less_than(const monster_info& m1, const monster_info& m2,
                             bool zombified, bool fullname)
{
    if (mons_is_hepliaklqana_ancestor(m1.type))
        return true;
    else if (mons_is_hepliaklqana_ancestor(m2.type))
        return false;

    if (m1.attitude < m2.attitude)
        return true;
    else if (m1.attitude > m2.attitude)
        return false;

    // Force plain but different coloured draconians to be treated like the
    // same sub-type.
    if (!zombified
        && mons_is_base_draconian(m1.type)
        && mons_is_base_draconian(m2.type))
    {
        return false;
    }

    // Treat base demonspawn identically, as with draconians.
    if (!zombified && m1.type >= MONS_FIRST_BASE_DEMONSPAWN
        && m1.type <= MONS_LAST_BASE_DEMONSPAWN
        && m2.type >= MONS_FIRST_BASE_DEMONSPAWN
        && m2.type <= MONS_LAST_BASE_DEMONSPAWN)
    {
        return false;
    }

    int diff_delta = mons_avg_hp(m1.type) - mons_avg_hp(m2.type);

    // By descending difficulty
    if (diff_delta > 0)
        return true;
    else if (diff_delta < 0)
        return false;

    if (m1.type < m2.type)
        return true;
    else if (m1.type > m2.type)
        return false;

    // Never distinguish between dancing weapons.
    // The above checks guarantee that *both* monsters are of this type.
    if (m1.type == MONS_DANCING_WEAPON)
        return false;

    if (m1.type == MONS_SLIME_CREATURE)
        return m1.slime_size > m2.slime_size;

    if (m1.type == MONS_BALLISTOMYCETE)
        return m1.is_active && !m2.is_active;

    // Shifters after real monsters of the same type.
    if (m1.is(MB_SHAPESHIFTER) != m2.is(MB_SHAPESHIFTER))
        return m2.is(MB_SHAPESHIFTER);

    // Spectralised after the still-living. There's not terribly much
    // difference, but this keeps us from combining them in the monster
    // list so they all appear to be spectralised.
    if (m1.is(MB_SPECTRALISED) != m2.is(MB_SPECTRALISED))
        return m2.is(MB_SPECTRALISED);

    if (zombified)
    {
        if (mons_class_is_zombified(m1.type))
        {
            // Because of the type checks above, if one of the two is zombified, so
            // is the other, and of the same type.
            if (m1.base_type < m2.base_type)
                return true;
            else if (m1.base_type > m2.base_type)
                return false;
        }

        // Both monsters are hydras or hydra zombies, sort by number of heads.
        if (_is_hydra(m1))
        {
            if (m1.num_heads > m2.num_heads)
                return true;
            else if (m1.num_heads < m2.num_heads)
                return false;
        }
    }

    if (fullname || mons_is_pghost(m1.type))
        return m1.mname < m2.mname;

#if 0 // for now, sort mb together.
    // By descending mb, so no mb sorts to the end
    if (m1.mb > m2.mb)
        return true;
    else if (m1.mb < m2.mb)
        return false;
#endif

    return false;
}

static string _verbose_info0(const monster_info& mi)
{
    if (mi.is(MB_BERSERK))
        return "berserk";
    if (mi.is(MB_INSANE))
        return "insane";
    if (mi.is(MB_INNER_FLAME))
        return "inner flame";
    if (mi.is(MB_DUMB))
        return "stupefied";
    if (mi.is(MB_PARALYSED))
        return "paralysed";
    if (mi.is(MB_CAUGHT))
        return "caught";
    if (mi.is(MB_WEBBED))
        return "webbed";
    if (mi.is(MB_PETRIFIED))
        return "petrified";
    if (mi.is(MB_PETRIFYING))
        return "petrifying";
    if (mi.is(MB_MAD))
        return "mad";
    if (mi.is(MB_CONFUSED))
        return "confused";
    if (mi.is(MB_FLEEING))
        return "fleeing";
    if (mi.is(MB_DORMANT))
        return "dormant";
    if (mi.is(MB_SLEEPING))
        return "sleeping";
    if (mi.is(MB_UNAWARE))
        return "unaware";
    if (mi.is(MB_DAZED))
        return "dazed";
    if (mi.is(MB_MUTE))
        return "mute";
    if (mi.is(MB_BLIND))
        return "blind";
    // avoid jelly (wandering) (fellow slime)
    if (mi.is(MB_WANDERING) && mi.attitude != ATT_STRICT_NEUTRAL)
        return "wandering";
    if (mi.is(MB_BURNING))
        return "burning";
    if (mi.is(MB_INVISIBLE))
        return "invisible";

    return "";
}

static string _verbose_info(const monster_info& mi)
{
    string inf = _verbose_info0(mi);
    if (!inf.empty())
        inf = " (" + inf + ")";
    return inf;
}

string monster_info::pluralised_name(bool fullname) const
{
    // Don't pluralise uniques, ever. Multiple copies of the same unique
    // are unlikely in the dungeon currently, but quite common in the
    // arena. This prevens "4 Gra", etc. {due}
    // Unless it's Mara, who summons illusions of himself.
    if (mons_is_unique(type) && type != MONS_MARA)
        return common_name();
    else if (mons_genus(type) == MONS_DRACONIAN)
        return pluralise_monster(mons_type_name(MONS_DRACONIAN, DESC_PLAIN));
    else if (mons_genus(type) == MONS_DEMONSPAWN)
        return pluralise_monster(mons_type_name(MONS_DEMONSPAWN, DESC_PLAIN));
    else if (type == MONS_UGLY_THING || type == MONS_VERY_UGLY_THING
             || type == MONS_DANCING_WEAPON || type == MONS_SPECTRAL_WEAPON
             || type == MONS_MUTANT_BEAST || !fullname)
    {
        return pluralise_monster(mons_type_name(type, DESC_PLAIN));
    }
    else
        return pluralise_monster(common_name());
}

enum _monster_list_colour_type
{
    _MLC_FRIENDLY, _MLC_NEUTRAL, _MLC_GOOD_NEUTRAL, _MLC_STRICT_NEUTRAL,
    _MLC_TRIVIAL, _MLC_EASY, _MLC_TOUGH, _MLC_NASTY,
    _NUM_MLC
};

static const char * const _monster_list_colour_names[_NUM_MLC] =
{
    "friendly", "neutral", "good_neutral", "strict_neutral",
    "trivial", "easy", "tough", "nasty"
};

static int _monster_list_colours[_NUM_MLC] =
{
    GREEN, BROWN, BROWN, BROWN,
    DARKGREY, LIGHTGREY, YELLOW, LIGHTRED,
};

bool set_monster_list_colour(string key, int colour)
{
    for (int i = 0; i < _NUM_MLC; ++i)
    {
        if (key == _monster_list_colour_names[i])
        {
            _monster_list_colours[i] = colour;
            return true;
        }
    }
    return false;
}

void clear_monster_list_colours()
{
    for (int i = 0; i < _NUM_MLC; ++i)
        _monster_list_colours[i] = -1;
}

void monster_info::to_string(int count, string& desc, int& desc_colour,
                             bool fullname, const char *adj) const
{
    ostringstream out;
    _monster_list_colour_type colour_type = _NUM_MLC;

    string full = count == 1 ? full_name() : pluralised_name(fullname);

    if (adj && starts_with(full, "the "))
        full.erase(0, 4);

    // TODO: this should be done in a much cleaner way, with code to
    // merge multiple monster_infos into a single common structure
    if (count != 1)
        out << count << " ";
    if (adj)
        out << adj << " ";
    out << full;

#ifdef DEBUG_DIAGNOSTICS
    out << " av" << mons_avg_hp(type);
#endif

    if (count == 1)
       out << _verbose_info(*this);

    // Friendliness
    switch (attitude)
    {
    case ATT_FRIENDLY:
        //out << " (friendly)";
        colour_type = _MLC_FRIENDLY;
        break;
    case ATT_GOOD_NEUTRAL:
        //out << " (neutral)";
        colour_type = _MLC_GOOD_NEUTRAL;
        break;
    case ATT_NEUTRAL:
        //out << " (neutral)";
        colour_type = _MLC_NEUTRAL;
        break;
    case ATT_STRICT_NEUTRAL:
        out << " (fellow slime)";
        colour_type = _MLC_STRICT_NEUTRAL;
        break;
    case ATT_HOSTILE:
        // out << " (hostile)";
        switch (threat)
        {
        case MTHRT_TRIVIAL: colour_type = _MLC_TRIVIAL; break;
        case MTHRT_EASY:    colour_type = _MLC_EASY;    break;
        case MTHRT_TOUGH:   colour_type = _MLC_TOUGH;   break;
        case MTHRT_NASTY:   colour_type = _MLC_NASTY;   break;
        default:;
        }
        break;
    }

    if (count == 1 && is(MB_EVIL_ATTACK))
        desc_colour = Options.evil_colour;
    else if (colour_type < _NUM_MLC)
        desc_colour = _monster_list_colours[colour_type];

    // We still need something, or we'd get the last entry's colour.
    if (desc_colour < 0)
        desc_colour = LIGHTGREY;

    desc = out.str();
}

vector<string> monster_info::attributes() const
{
    vector<string> v;

    if (is(MB_BERSERK))
        v.emplace_back("berserk");
    if (is(MB_HASTED) || is(MB_BERSERK))
    {
        if (!is(MB_SLOWED))
            v.emplace_back("fast");
        else
            v.emplace_back("fast+slow");
    }
    else if (is(MB_SLOWED))
        v.emplace_back("slow");
    if (is(MB_STRONG) || is(MB_BERSERK))
        v.emplace_back("unusually strong");

    if (is(MB_POISONED))
        v.emplace_back("poisoned");
    if (is(MB_SICK))
        v.emplace_back("sick");
    if (is(MB_GLOWING))
        v.emplace_back("softly glowing");
    if (is(MB_INSANE))
        v.emplace_back("frenzied and insane");
    if (is(MB_CONFUSED))
        v.emplace_back("confused");
    if (is(MB_INVISIBLE))
        v.emplace_back("slightly transparent");
    if (is(MB_CHARMED))
        v.emplace_back("in your thrall");
    if (is(MB_BURNING))
        v.emplace_back("covered in liquid flames");
    if (is(MB_CAUGHT))
        v.emplace_back("entangled in a net");
    if (is(MB_WEBBED))
        v.emplace_back("entangled in a web");
    if (is(MB_PETRIFIED))
        v.emplace_back("petrified");
    if (is(MB_PETRIFYING))
        v.emplace_back("slowly petrifying");
    if (is(MB_VULN_MAGIC))
        v.emplace_back("susceptible to hostile enchantments");
    if (is(MB_SWIFT))
        v.emplace_back("covering ground quickly");
    if (is(MB_SILENCING))
        v.emplace_back("radiating silence");
    if (is(MB_PARALYSED))
        v.emplace_back("paralysed");
    if (is(MB_REPEL_MSL))
        v.emplace_back("repelling missiles");
    if (is(MB_DEFLECT_MSL))
        v.emplace_back("deflecting missiles");
    if (is(MB_FEAR_INSPIRING))
        v.emplace_back("inspiring fear");
    if (is(MB_BREATH_WEAPON))
    {
        v.push_back(string("catching ")
                    + pronoun(PRONOUN_POSSESSIVE) + " breath");
    }
    if (is(MB_DAZED))
        v.emplace_back("dazed");
    if (is(MB_MUTE))
        v.emplace_back("mute");
    if (is(MB_BLIND))
        v.emplace_back("blind");
    if (is(MB_DUMB))
        v.emplace_back("stupefied");
    if (is(MB_MAD))
        v.emplace_back("lost in madness");
    if (is(MB_REGENERATION))
        v.emplace_back("regenerating");
    if (is(MB_RAISED_MR))
        v.emplace_back("resistant to hostile enchantments");
    if (is(MB_OZOCUBUS_ARMOUR))
        v.emplace_back("covered in an icy film");
    if (is(MB_WRETCHED))
        v.emplace_back("misshapen and mutated");
    if (is(MB_WORD_OF_RECALL))
        v.emplace_back("chanting recall");
    if (is(MB_INJURY_BOND))
        v.emplace_back("sheltered from injuries");
    if (is(MB_WATER_HOLD))
        v.emplace_back("engulfed in water");
    if (is(MB_WATER_HOLD_DROWN))
    {
        v.emplace_back("engulfed in water");
        v.emplace_back("unable to breathe");
    }
    if (is(MB_FLAYED))
        v.emplace_back("covered in terrible wounds");
    if (is(MB_WEAK))
        v.emplace_back("weak");
    if (is(MB_DIMENSION_ANCHOR))
        v.emplace_back("unable to translocate");
    if (is(MB_TOXIC_RADIANCE))
        v.emplace_back("radiating toxic energy");
    if (is(MB_GRASPING_ROOTS))
        v.emplace_back("movement impaired by roots");
    if (is(MB_FIRE_VULN))
        v.emplace_back("more vulnerable to fire");
    if (is(MB_TORNADO))
        v.emplace_back("surrounded by raging winds");
    if (is(MB_TORNADO_COOLDOWN))
        v.emplace_back("surrounded by restless winds");
    if (is(MB_BARBS))
        v.emplace_back("skewered by barbs");
    if (is(MB_POISON_VULN))
        v.emplace_back("more vulnerable to poison");
    if (is(MB_ICEMAIL))
        v.emplace_back("surrounded by an icy envelope");
    if (is(MB_AGILE))
        v.emplace_back("unusually agile");
    if (is(MB_FROZEN))
        v.emplace_back("encased in ice");
    if (is(MB_BLACK_MARK))
        v.emplace_back("absorbing vital energies");
    if (is(MB_SAP_MAGIC))
        v.emplace_back("magic-sapped");
    if (is(MB_SHROUD))
        v.emplace_back("shrouded");
    if (is(MB_CORROSION))
        v.emplace_back("covered in acid");
    if (is(MB_SLOW_MOVEMENT))
        v.emplace_back("covering ground slowly");
    if (is(MB_LIGHTLY_DRAINED))
        v.emplace_back("lightly drained");
    if (is(MB_HEAVILY_DRAINED))
        v.emplace_back("heavily drained");
    if (is(MB_RESISTANCE))
        v.emplace_back("unusually resistant");
    if (is(MB_HEXED))
        v.emplace_back("control wrested from you");
    if (is(MB_BONE_ARMOUR))
        v.emplace_back("corpse armoured");
    if (is(MB_BRILLIANCE_AURA))
        v.emplace_back("aura of brilliance");
    if (is(MB_EMPOWERED_SPELLS))
        v.emplace_back("spells empowered");
    if (is(MB_READY_TO_HOWL))
        v.emplace_back("ready to howl");
    if (is(MB_PARTIALLY_CHARGED))
        v.emplace_back("partially charged");
    if (is(MB_FULLY_CHARGED))
        v.emplace_back("fully charged");
    if (is(MB_GOZAG_INCITED))
        v.emplace_back("incited by Gozag");
    if (is(MB_PAIN_BOND))
    {
        v.push_back(string("sharing ")
                    + pronoun(PRONOUN_POSSESSIVE) + " pain");
    }
    if (is(MB_IDEALISED))
        v.emplace_back("idealised");
    if (is(MB_BOUND_SOUL))
        v.emplace_back("bound soul");
    if (is(MB_INFESTATION))
        v.emplace_back("infested");
    if (is(MB_STILL_WINDS))
        v.emplace_back("stilling the winds");
    return v;
}

string monster_info::wounds_description_sentence() const
{
    const string wounds = wounds_description();
    if (wounds.empty())
        return "";
    else
        return string(pronoun(PRONOUN_SUBJECTIVE)) + " is " + wounds + ".";
}

string monster_info::wounds_description(bool use_colour) const
{
    if (dam == MDAM_OKAY)
        return "";

    string desc = get_damage_level_string(holi, dam);
    if (use_colour)
    {
        const int col = channel_to_colour(MSGCH_MONSTER_DAMAGE, dam);
        desc = colour_string(desc, col);
    }
    return desc;
}

string monster_info::constriction_description() const
{
    string cinfo = "";
    bool bymsg = false;

    if (constrictor_name != "")
    {
        cinfo += constrictor_name;
        bymsg = true;
    }

    string constricting = comma_separated_line(constricting_name.begin(),
                                               constricting_name.end());

    if (constricting != "")
    {
        if (bymsg)
            cinfo += ", ";
        cinfo += constricting;
    }
    return cinfo;
}

int monster_info::randarts(artefact_prop_type ra_prop) const
{
    int ret = 0;

    if (itemuse() >= MONUSE_STARTING_EQUIPMENT)
    {
        item_def* weapon = inv[MSLOT_WEAPON].get();
        item_def* second = inv[MSLOT_ALT_WEAPON].get(); // Two-headed ogres, etc.
        item_def* armour = inv[MSLOT_ARMOUR].get();
        item_def* shield = inv[MSLOT_SHIELD].get();
        item_def* ring   = inv[MSLOT_JEWELLERY].get();

        if (weapon && weapon->base_type == OBJ_WEAPONS && is_artefact(*weapon))
            ret += artefact_property(*weapon, ra_prop);

        if (second && second->base_type == OBJ_WEAPONS && is_artefact(*second))
            ret += artefact_property(*second, ra_prop);

        if (armour && armour->base_type == OBJ_ARMOUR && is_artefact(*armour))
            ret += artefact_property(*armour, ra_prop);

        if (shield && shield->base_type == OBJ_ARMOUR && is_artefact(*shield))
            ret += artefact_property(*shield, ra_prop);

        if (ring && ring->base_type == OBJ_JEWELLERY && is_artefact(*ring))
            ret += artefact_property(*ring, ra_prop);
    }

    return ret;
}

/**
 * Can the monster described by this monster_info see invisible creatures?
 */
bool monster_info::can_see_invisible() const
{
    return can_see_invis;
}

int monster_info::res_magic() const
{
    return mr;
}

string monster_info::speed_description() const
{
    if (mbase_speed < 7)
        return "very slow";
    else if (mbase_speed < 10)
        return "slow";
    else if (mbase_speed > 20)
        return "extremely fast";
    else if (mbase_speed > 15)
        return "very fast";
    else if (mbase_speed > 10)
        return "fast";

    // This only ever displays through Lua.
    return "normal";
}

bool monster_info::wields_two_weapons() const
{
    return is(MB_TWO_WEAPONS);
}

bool monster_info::can_regenerate() const
{
    return !is(MB_NO_REGEN);
}

reach_type monster_info::reach_range() const
{
    const monsterentry *e = get_monster_data(mons_class_is_zombified(type)
                                             ? base_type : type);
    ASSERT(e);

    reach_type range = e->attack[0].flavour == AF_REACH
                       || e->attack[0].flavour == AF_REACH_STING
                          ? REACH_TWO : REACH_NONE;

    const item_def *weapon = inv[MSLOT_WEAPON].get();
    if (weapon)
        range = max(range, weapon_reach(*weapon));

    return range;
}

size_type monster_info::body_size() const
{
    const size_type class_size = mons_class_body_size(base_type);

    // Slime creature size is increased by the number merged.
    if (type == MONS_SLIME_CREATURE)
    {
        if (slime_size == 2)
            return SIZE_MEDIUM;
        else if (slime_size == 3)
            return SIZE_LARGE;
        else if (slime_size == 4)
            return SIZE_BIG;
        else if (slime_size == 5)
            return SIZE_GIANT;
    }

    return class_size;
}

bool monster_info::cannot_move() const
{
    return is(MB_PARALYSED) || is(MB_PETRIFIED);
}

bool monster_info::airborne() const
{
    return is(MB_AIRBORNE);
}

bool monster_info::ground_level() const
{
    return !airborne() && !is(MB_CLINGING);
}

// Only checks for spells from preset monster spellbooks.
// Use monster.h's has_spells for knowing a monster has spells
bool monster_info::has_spells() const
{
    // Some monsters have a special book but may not have any spells anyways.
    if (props.exists(CUSTOM_SPELLS_KEY))
        return spells.size() > 0 && spells[0].spell != SPELL_NO_SPELL;

    if (props.exists(SEEN_SPELLS_KEY))
        return true;

    // Almost all draconians have breath spells.
    if (mons_genus(draco_or_demonspawn_subspecies()) == MONS_DRACONIAN
        && draco_or_demonspawn_subspecies() != MONS_GREY_DRACONIAN
        && draco_or_demonspawn_subspecies() != MONS_DRACONIAN)
    {
        return true;
    }

    const vector<mon_spellbook_type> books = get_spellbooks(*this);

    // Random pan lords don't display their spells.
    if (books.size() == 0 || books[0] == MST_NO_SPELLS
        || type == MONS_PANDEMONIUM_LORD)
    {
        return false;
    }

    // Ghosts have a special book but may not have any spells anyways.
    if (books[0] == MST_GHOST)
        return spells.size() > 0;

    return true;
}

unsigned monster_info::colour(bool base_colour) const
{
    if (!base_colour && Options.mon_glyph_overrides.count(type)
        && Options.mon_glyph_overrides[type].col)
    {
        return Options.mon_glyph_overrides[type].col;
    }
    else if (_colour == COLOUR_INHERIT)
        return mons_class_colour(type);
    else
    {
        ASSERT_RANGE(_colour, 0, NUM_COLOURS);
        return _colour;
    }
}

void monster_info::set_colour(int col)
{
    ASSERT_RANGE(col, -1, NUM_COLOURS);
    _colour = col;
}

/**
 * Does this monster have the given enchantment,
 * to the best of the player's knowledge?
 *
 * Only handles trivially mapped MBs.
 */
bool monster_info::has_trivial_ench(enchant_type ench) const
{
    monster_info_flags *flag = map_find(trivial_ench_mb_mappings, ench);
    return flag && is(*flag);
}

/// Can this monster be debuffed, to the best of the player's knowledge?
bool monster_info::debuffable() const
{
    // NOTE: assumes that all debuffable enchantments are trivially mapped
    // to MBs.

    // can't debuff innately invisible monsters
    if (is(MB_INVISIBLE) && !mons_class_flag(type, M_INVIS))
        return true;

    return any_of(begin(dispellable_enchantments),
                  end(dispellable_enchantments),
                  [this](enchant_type ench) -> bool
                  { return this->has_trivial_ench(ench); });
}

void get_monster_info(vector<monster_info>& mons)
{
    vector<monster* > visible;
    if (crawl_state.game_is_arena())
    {
        for (monster_iterator mi; mi; ++mi)
            visible.push_back(*mi);
    }
    else
        visible = get_nearby_monsters();

    for (monster *mon : visible)
    {
        if (mons_is_threatening(*mon)
            || mon->is_child_tentacle())
        {
            mons.emplace_back(mon);
        }
    }
    sort(mons.begin(), mons.end(), monster_info::less_than_wrapper);
}

monster_type monster_info::draco_or_demonspawn_subspecies() const
{
    if (type == MONS_PLAYER_ILLUSION && mons_genus(type) == MONS_DRACONIAN)
        return player_species_to_mons_species(i_ghost.species);
    return ::draco_or_demonspawn_subspecies(type, base_type);
}

const char *monster_info::pronoun(pronoun_type variant) const
{
    if (props.exists(MON_GENDER_KEY))
    {
        return decline_pronoun((gender_type)props[MON_GENDER_KEY].get_int(),
                               variant);
    }
    return mons_pronoun(type, variant, true);
}
