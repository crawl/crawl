/**
 * @file
 * @brief Monster information that may be passed to the user.
 *
 * Used to fill the monster pane and to pass monster info to Lua.
**/

#include "AppHdr.h"

#include "mon-info.h"

#include "act-iter.h"
#include "areas.h"
#include "artefact.h"
#include "coord.h"
#include "coordit.h"
#include "env.h"
#include "fight.h"
#include "ghost.h"
#include "itemname.h"
#include "itemprop.h"
#include "libutil.h"
#include "los.h"
#include "message.h"
#include "misc.h"
#include "mon-chimera.h"
#include "mon-util.h"
#include "monster.h"
#include "options.h"
#include "religion.h"
#include "skills2.h"
#include "spl-summoning.h"
#include "state.h"
#include "terrain.h"
#include "traps.h"

#include <algorithm>
#include <sstream>

static monster_info_flags ench_to_mb(const monster& mons, enchant_type ench)
{
    // Suppress silly-looking combinations, even if they're
    // internally valid.
    if (mons.paralysed() && (ench == ENCH_SLOW || ench == ENCH_HASTE
                      || ench == ENCH_SWIFT
                      || ench == ENCH_PETRIFIED
                      || ench == ENCH_PETRIFYING))
        return NUM_MB_FLAGS;

    if (ench == ENCH_HASTE && mons.has_ench(ENCH_SLOW))
        return NUM_MB_FLAGS;

    if (ench == ENCH_SLOW && mons.has_ench(ENCH_HASTE))
        return NUM_MB_FLAGS;

    if (ench == ENCH_PETRIFIED && mons.has_ench(ENCH_PETRIFYING))
        return NUM_MB_FLAGS;

    // Don't claim that naturally 'confused' monsters are especially bewildered
    if (ench == ENCH_CONFUSION && mons_class_flag(mons.type, M_CONFUSED))
        return NUM_MB_FLAGS;

    switch (ench)
    {
    case ENCH_BERSERK:
        return MB_BERSERK;
    case ENCH_POISON:
        return MB_POISONED;
    case ENCH_SICK:
        return MB_SICK;
    case ENCH_ROT:
        return MB_ROTTING;
    case ENCH_CORONA:
    case ENCH_SILVER_CORONA:
        return MB_GLOWING;
    case ENCH_SLOW:
        return MB_SLOWED;
    case ENCH_INSANE:
        return MB_INSANE;
    case ENCH_BATTLE_FRENZY:
        return MB_FRENZIED;
    case ENCH_ROUSED:
        return MB_ROUSED;
    case ENCH_HASTE:
        return MB_HASTED;
    case ENCH_MIGHT:
        return MB_STRONG;
    case ENCH_CONFUSION:
        return MB_CONFUSED;
    case ENCH_INVIS:
    {
        you.seen_invis = true;
        return MB_INVISIBLE;
    }
    case ENCH_CHARM:
        return MB_CHARMED;
    case ENCH_STICKY_FLAME:
        return MB_BURNING;
    case ENCH_HELD:
        return get_trapping_net(mons.pos(), true) == NON_ITEM
               ? MB_WEBBED : MB_CAUGHT;
    case ENCH_PETRIFIED:
        return MB_PETRIFIED;
    case ENCH_PETRIFYING:
        return MB_PETRIFYING;
    case ENCH_LOWERED_MR:
        return MB_VULN_MAGIC;
    case ENCH_SWIFT:
        return MB_SWIFT;
    case ENCH_SILENCE:
        return MB_SILENCING;
    case ENCH_PARALYSIS:
        return MB_PARALYSED;
    case ENCH_SOUL_RIPE:
        return MB_POSSESSABLE;
    case ENCH_PREPARING_RESURRECT:
        return MB_PREP_RESURRECT;
    case ENCH_REGENERATION:
        return MB_REGENERATION;
    case ENCH_RAISED_MR:
        return MB_RAISED_MR;
    case ENCH_MIRROR_DAMAGE:
        return MB_MIRROR_DAMAGE;
    case ENCH_FEAR_INSPIRING:
        return MB_FEAR_INSPIRING;
    case ENCH_WITHDRAWN:
        return MB_WITHDRAWN;
    case ENCH_ATTACHED:
        return MB_ATTACHED;
    case ENCH_BLEED:
        return MB_BLEEDING;
    case ENCH_DAZED:
        return MB_DAZED;
    case ENCH_MUTE:
        return MB_MUTE;
    case ENCH_BLIND:
        return MB_BLIND;
    case ENCH_DUMB:
        return MB_DUMB;
    case ENCH_MAD:
        return MB_MAD;
    case ENCH_INNER_FLAME:
        return MB_INNER_FLAME;
    case ENCH_BREATH_WEAPON:
        return MB_BREATH_WEAPON;
    case ENCH_DEATHS_DOOR:
        return MB_DEATHS_DOOR;
    case ENCH_ROLLING:
        return MB_ROLLING;
    case ENCH_STONESKIN:
        return MB_STONESKIN;
    case ENCH_OZOCUBUS_ARMOUR:
        return MB_OZOCUBUS_ARMOUR;
    case ENCH_WRETCHED:
        return MB_WRETCHED;
    case ENCH_SCREAMED:
        return MB_SCREAMED;
    case ENCH_WORD_OF_RECALL:
        return MB_WORD_OF_RECALL;
    case ENCH_INJURY_BOND:
        return MB_INJURY_BOND;
    case ENCH_WATER_HOLD:
        if (mons.res_water_drowning())
            return MB_WATER_HOLD;
        else
            return MB_WATER_HOLD_DROWN;
    case ENCH_FLAYED:
        return MB_FLAYED;
    case ENCH_RETCHING:
        return MB_RETCHING;
    case ENCH_WEAK:
        return MB_WEAK;
    case ENCH_DIMENSION_ANCHOR:
        return MB_DIMENSION_ANCHOR;
    case ENCH_CONTROL_WINDS:
        return MB_CONTROL_WINDS;
    case ENCH_WIND_AIDED:
        return MB_WIND_AIDED;
    case ENCH_TOXIC_RADIANCE:
        return MB_TOXIC_RADIANCE;
    case ENCH_GRASPING_ROOTS:
        return MB_GRASPING_ROOTS;
    case ENCH_FIRE_VULN:
        return MB_FIRE_VULN;
    case ENCH_TORNADO:
        return MB_TORNADO;
    case ENCH_TORNADO_COOLDOWN:
        return MB_TORNADO_COOLDOWN;
    default:
        return NUM_MB_FLAGS;
    }
}

static bool _blocked_ray(const coord_def &where,
                         dungeon_feature_type* feat = NULL)
{
    if (exists_ray(you.pos(), where, opc_solid_see)
        || !exists_ray(you.pos(), where, opc_default))
    {
        return false;
    }
    if (feat == NULL)
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
     || key == "tile_num"
     || key == "tile_idx"
     || key == "chimera_part_2"
     || key == "chimera_part_3"
     || key == "chimera_batty"
     || key == "chimera_wings"
     || key == "chimera_legs")
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
            if (mon && you.can_see(mon))
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

    const int h_idx = m->props[key].get_int();
    monster *other = NULL; // previous or next segment
    if (h_idx != -1)
    {
        ASSERT(!invalid_monster_index(h_idx));
        other = &menv[h_idx];
        coord_def h_pos = other->pos();
        // If the tentacle and the other segment are no longer adjacent
        // (distortion etc.), just treat them as not connected.
        if (adjacent(m->pos(), h_pos)
            && other->type != MONS_KRAKEN
            && other->type != MONS_ZOMBIE
            && other->type != MONS_SPECTRAL_THING
            && other->type != MONS_SIMULACRUM
            && !_tentacle_pos_unknown(other, m->pos()))
        {
            mi.props[key] = h_pos - m->pos();
        }
    }
}

monster_info::monster_info(monster_type p_type, monster_type p_base_type)
{
    mb.reset();
    attitude = ATT_HOSTILE;
    pos = coord_def(0, 0);

    type = p_type;
    base_type = p_base_type;

    draco_type = mons_genus(type) == MONS_DRACONIAN ? MONS_DRACONIAN : type;

    number = 0;
    colour = mons_class_colour(type);

    holi = mons_class_holiness(type);

    mintel = mons_class_intel(type);

    mresists = get_mons_class_resists(type);

    mitemuse = mons_class_itemuse(type);

    mbase_speed = mons_class_base_speed(type);

    fly = max(mons_class_flies(type), mons_class_flies(base_type));

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

    u.ghost.acting_part = MONS_0;
    if (mons_is_pghost(type))
    {
        u.ghost.species = SP_HUMAN;
        u.ghost.job = JOB_WANDERER;
        u.ghost.religion = GOD_NO_GOD;
        u.ghost.best_skill = SK_FIGHTING;
        u.ghost.best_skill_rank = 2;
        u.ghost.xl_rank = 3;
        u.ghost.ac = 5;
        u.ghost.damage = 5;
    }

    if (base_type == MONS_NO_MONSTER)
        base_type = type;

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

    props.clear();

    client_id = 0;
}

monster_info::monster_info(const monster* m, int milev)
{
    mb.reset();
    attitude = ATT_HOSTILE;
    pos = m->pos();

    attitude = mons_attitude(m);

    bool nomsg_wounds = false;

    // friendly fake Rakshasas/Maras are known
    if ((m->type == MONS_RAKSHASA_FAKE || m->type == MONS_MARA_FAKE)
        && attitude != ATT_FRIENDLY && monster_by_mid(m->summoner))
    {
        const monster* real = monster_by_mid(m->summoner);
        type = real->type;
        threat = mons_threat_level(real);
    }
    else
    {
        type = m->type;
        threat = mons_threat_level(m);
    }

    props.clear();
    if (!m->props.empty())
    {
        CrawlHashTable::hash_map_type::const_iterator i = m->props.begin();
        for (; i != m->props.end(); ++i)
            if (_is_public_key(i->first))
                props[i->first] = i->second;
    }

    // Translate references to tentacles into just their locations
    if (mons_is_tentacle_or_tentacle_segment(type))
    {
        _translate_tentacle_ref(*this, m, "inwards");
        _translate_tentacle_ref(*this, m, "outwards");
    }

    draco_type =
        mons_genus(type) == MONS_DRACONIAN ? ::draco_subspecies(m) : type;

    if (!mons_can_display_wounds(m)
        || !mons_class_can_display_wounds(type))
    {
        nomsg_wounds = true;
    }

    base_type = m->base_monster;
    if (base_type == MONS_NO_MONSTER)
        base_type = type;

    // these use number for internal information
    if (type == MONS_MANTICORE
        || type == MONS_SIXFIRHY
        || type == MONS_JIANGSHI
        || type == MONS_SHEDU
        || type == MONS_KRAKEN_TENTACLE
        || type == MONS_KRAKEN_TENTACLE_SEGMENT
        || type == MONS_ELDRITCH_TENTACLE_SEGMENT
        || type == MONS_STARSPAWN_TENTACLE
        || type == MONS_STARSPAWN_TENTACLE_SEGMENT)
    {
        number = 0;
    }
    else
        number = m->number;
    colour = m->colour;

    int stype = 0;
    if (m->is_summoned(0, &stype)
        && m->type != MONS_RAKSHASA_FAKE && m->type != MONS_MARA_FAKE)
    {
        mb.set(MB_SUMMONED);
        if (stype > 0 && stype < NUM_SPELLS
            && summons_are_capped(static_cast<spell_type>(stype)))
        {
            mb.set(MB_SUMMONED_NO_STAIRS);
        }
    }
    else if (m->is_perm_summoned())
        mb.set(MB_PERM_SUMMON);

    if (m->has_ench(ENCH_SUMMON_CAPPED))
        mb.set(MB_SUMMONED_CAPPED);

    if (mons_is_unique(type))
    {
        if (type == MONS_LERNAEAN_HYDRA
            || type == MONS_ROYAL_JELLY
            || type == MONS_SERPENT_OF_HELL)
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

    const uint64_t name_flags = m->flags & MF_NAME_MASK;

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

    // Chimera acting head needed for name
    u.ghost.acting_part = MONS_0;
    if (mons_class_is_chimeric(type))
    {
        ASSERT(m->ghost.get());
        ghost_demon& ghost = *m->ghost;
        u.ghost.acting_part = ghost.acting_part;
    }

    if (milev <= MILEV_NAME)
    {
        if (type == MONS_DANCING_WEAPON
            && m->inv[MSLOT_WEAPON] != NON_ITEM)
        {
            inv[MSLOT_WEAPON].reset(
                new item_def(get_item_info(mitm[m->inv[MSLOT_WEAPON]])));
        }
        if (mons_is_item_mimic(type))
        {
            ASSERT(m->inv[MSLOT_MISCELLANY] != NON_ITEM);
            inv[MSLOT_MISCELLANY].reset(
                new item_def(get_item_info(mitm[m->inv[MSLOT_MISCELLANY]])));
        }
        return;
    }

    holi = m->holiness();

    mintel = mons_intel(m);
    mresists = get_mons_resists(m);
    mitemuse = mons_itemuse(m);
    mbase_speed = mons_base_speed(m);
    fly = mons_flies(m);

    if (mons_wields_two_weapons(m))
        mb.set(MB_TWO_WEAPONS);
    if (!mons_can_regenerate(m))
        mb.set(MB_NO_REGEN);
    if (m->haloed() && !m->umbraed())
        mb.set(MB_HALOED);
    if (!m->haloed() && m->umbraed())
        mb.set(MB_UMBRAED);
    if (mons_looks_stabbable(m))
        mb.set(MB_STABBABLE);
    if (mons_looks_distracted(m))
        mb.set(MB_DISTRACTED);
    if (m->liquefied_ground())
        mb.set(MB_SLOWED);
    if (m->is_wall_clinging())
        mb.set(MB_CLINGING);

    dam = mons_get_damage_level(m);

    // If no messages about wounds, don't display damage level either.
    if (nomsg_wounds)
        dam = MDAM_OKAY;

    if (!mons_class_flag(m->type, M_NO_EXP_GAIN)) // Firewood, butterflies, etc.
    {
        if (m->asleep())
        {
            if (!m->can_hibernate(true))
                mb.set(MB_DORMANT);
            else
                mb.set(MB_SLEEPING);
        }
        // Applies to both friendlies and hostiles
        else if (mons_is_fleeing(m))
            mb.set(MB_FLEEING);
        else if (mons_is_wandering(m) && !mons_is_batty(m))
        {
            if (m->is_stationary())
                mb.set(MB_UNAWARE);
            else
                mb.set(MB_WANDERING);
        }
        // TODO: is this ever needed?
        else if (m->foe == MHITNOT && !mons_is_batty(m)
                 && m->attitude == ATT_HOSTILE)
        {
            mb.set(MB_UNAWARE);
        }
    }

    for (mon_enchant_list::const_iterator e = m->enchantments.begin();
         e != m->enchantments.end(); ++e)
    {
        monster_info_flags flag = ench_to_mb(*m, e->first);
        if (flag != NUM_MB_FLAGS)
            mb.set(flag);
    }

    // fake enchantment (permanent)
    if (mons_class_flag(type, M_DEFLECT_MISSILES))
        mb.set(MB_DEFLECT_MSL);

    if (type == MONS_SILENT_SPECTRE)
        mb.set(MB_SILENCING);

    if (you.beheld_by(m))
        mb.set(MB_MESMERIZING);

    // Evilness of attacking
    switch (attitude)
    {
    case ATT_NEUTRAL:
    case ATT_HOSTILE:
        if (you_worship(GOD_SHINING_ONE)
            && !tso_unchivalric_attack_safe_monster(m)
            && is_unchivalric_attack(&you, m))
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

    if (m->is_known_chaotic())
        mb.set(MB_CHAOTIC);

    if (m->submerged())
        mb.set(MB_SUBMERGED);

    if (mons_is_pghost(type))
    {
        ASSERT(m->ghost.get());
        ghost_demon& ghost = *m->ghost;
        u.ghost.species = ghost.species;
        if (species_genus(u.ghost.species) == GENPC_DRACONIAN && ghost.xl < 7)
            u.ghost.species = SP_BASE_DRACONIAN;
        u.ghost.job = ghost.job;
        u.ghost.religion = ghost.religion;
        u.ghost.best_skill = ghost.best_skill;
        u.ghost.best_skill_rank = get_skill_rank(ghost.best_skill_level);
        u.ghost.xl_rank = ghost_level_to_rank(ghost.xl);
        u.ghost.ac = quantise(ghost.ac, 5);
        u.ghost.damage = quantise(ghost.damage, 5);
    }

    for (unsigned i = 0; i <= MSLOT_LAST_VISIBLE_SLOT; ++i)
    {
        bool ok;
        if (m->inv[i] == NON_ITEM)
            ok = false;
        else if (i == MSLOT_MISCELLANY)
            ok = mons_is_mimic(type);
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
        actor * const constrictor = actor_by_mid(m->constricted_by);
        if (constrictor)
        {
            constrictor_name = (m->held == HELD_MONSTER ? "held by "
                                                        : "constricted by ")
                               + constrictor->name(DESC_A, true);
        }
    }

    // names of what this monster is constricting, if any
    if (m->constricting)
    {
        actor::constricting_t::const_iterator i;
        for (i = m->constricting->begin(); i != m->constricting->end(); ++i)
        {
            actor* const constrictee = actor_by_mid(i->first);

            if (constrictee)
            {
                constricting_name.push_back(
                                (m->constriction_damage() ? "constricting "
                                                          : "holding ")
                                + constrictee->name(DESC_A, true));
            }
        }
    }

    if (mons_has_known_ranged_attack(m))
        mb.set(MB_RANGED_ATTACK);

    // this must be last because it provides this structure to Lua code
    if (milev > MILEV_SKIP_SAFE)
    {
        if (mons_is_safe(m))
            mb.set(MB_SAFE);
        else
            mb.set(MB_UNSAFE);
        if (mons_is_firewood(m))
            mb.set(MB_FIREWOOD);
    }

    client_id = m->get_client_id();
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

    switch (type)
    {
    case MONS_ZOMBIE:
    case MONS_SKELETON:
    case MONS_SIMULACRUM:
#if TAG_MAJOR_VERSION == 34
    case MONS_ZOMBIE_SMALL:     case MONS_ZOMBIE_LARGE:
    case MONS_SKELETON_SMALL:   case MONS_SKELETON_LARGE:
    case MONS_SIMULACRUM_SMALL: case MONS_SIMULACRUM_LARGE:
#endif
    case MONS_SPECTRAL_THING:   case MONS_PILLAR_OF_SALT:
    case MONS_CHIMERA:
    case MONS_SENSED:
        nametype = base_type;
        break;

    default:
        break;
    }

    string s;

    if (is(MB_NAME_REPLACE))
        s = mname;
    else if (nametype == MONS_LERNAEAN_HYDRA)
        s = "Lernaean hydra"; // TODO: put this into mon-data.h
    else if (nametype == MONS_ROYAL_JELLY)
        s = "royal jelly";
    else if (nametype == MONS_SERPENT_OF_HELL)
        s = "Serpent of Hell";
    else if (mons_is_mimic(nametype))
        s = mimic_name();
    else if (invalid_monster_type(nametype) && nametype != MONS_PROGRAM_BUG)
        s = "INVALID MONSTER";
    else
    {
        const char* slime_sizes[] = {"buggy ", "", "large ", "very large ",
                                               "enormous ", "titanic "};
        s = get_monster_data(nametype)->name;

        switch (type)
        {
        case MONS_SLIME_CREATURE:
            ASSERT(number <= 5);
            s = slime_sizes[number] + s;
            break;
        case MONS_UGLY_THING:
        case MONS_VERY_UGLY_THING:
            s = ugly_thing_colour_name(colour) + " " + s;
            break;

        case MONS_DRACONIAN_CALLER:
        case MONS_DRACONIAN_MONK:
        case MONS_DRACONIAN_ZEALOT:
        case MONS_DRACONIAN_SHIFTER:
        case MONS_DRACONIAN_ANNIHILATOR:
        case MONS_DRACONIAN_KNIGHT:
        case MONS_DRACONIAN_SCORCHER:
            if (base_type != MONS_NO_MONSTER)
                s = draconian_colour_name(base_type) + " " + s;
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

    if (is(MB_NAME_SUFFIX))
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
                        || mons_class_is_chimeric(type);

    ostringstream ss;

    if (props.exists("helpless"))
        ss << "helpless ";

    if (is(MB_SUBMERGED))
        ss << "submerged ";

    if (type == MONS_SPECTRAL_THING && !is(MB_NAME_ZOMBIE) && !nocore)
        ss << "spectral ";

    if (type == MONS_SENSED && !mons_is_sensed(base_type))
        ss << "sensed ";

    if (type == MONS_BALLISTOMYCETE)
        ss << (number ? "active " : "");

    if ((mons_genus(type) == MONS_HYDRA || mons_genus(base_type) == MONS_HYDRA)
        && number > 0)
    {
        if (number < 11)
        {
            const char* cardinals[] = {"one", "two", "three", "four", "five",
                                       "six", "seven", "eight", "nine", "ten"};
            ss << cardinals[number - 1];
        }
        else
            ss << make_stringf("%d", number);

        ss << "-headed ";
    }

    if (mons_class_is_chimeric(type))
    {
        ss << "chimera";
        monsterentry *me = NULL;
        if (u.ghost.acting_part != MONS_0
            && (me = get_monster_data(u.ghost.acting_part)))
        {
            // Specify an acting head
            ss << "'s " << me->name << " head";
        }
        else
            // Suffix parts in brackets
            // XXX: Should have a desc level that disables this
            ss << " (" << core << chimera_part_names() << ")";
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

dungeon_feature_type monster_info::get_mimic_feature() const
{
    if (!props.exists("feat_type"))
        return DNGN_UNSEEN;
    return static_cast<dungeon_feature_type>(props["feat_type"].get_short());
}

const item_def* monster_info::get_mimic_item() const
{
    ASSERT(mons_is_item_mimic(type));

    return inv[MSLOT_MISCELLANY].get();
}

string monster_info::mimic_name() const
{
    string s;
    if (type == MONS_INEPT_ITEM_MIMIC || type == MONS_INEPT_FEATURE_MIMIC)
        s = "inept ";
    if (type == MONS_RAVENOUS_ITEM_MIMIC || type == MONS_RAVENOUS_FEATURE_MIMIC)
        s = "ravenous ";
#if TAG_MAJOR_VERSION == 34
    if (type == MONS_MONSTROUS_ITEM_MIMIC)
        s = "monstrous ";
#endif

    if (props.exists("feat_type"))
        s += feat_type_name(get_mimic_feature());
    else if (item_def* item = inv[MSLOT_MISCELLANY].get())
    {
        if (item->base_type == OBJ_GOLD)
            s += "pile of gold";
        else if (item->base_type == OBJ_MISCELLANY
                 && item->sub_type == MISC_RUNE_OF_ZOT)
        {
            s += "rune";
        }
        else if (item->base_type == OBJ_ORBS)
            s += "orb";
        else
            s += item->name(DESC_DBNAME);
    }

    if (!s.empty())
        s += " ";

    return s + "mimic";
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

string monster_info::full_name(description_level_type desc, bool use_comma) const
{
    if (desc == DESC_NONE)
        return "";

    if (has_proper_name())
    {
        string s = mname + (use_comma ? ", the " : " the ") + common_name();
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
    if (m1.attitude < m2.attitude)
        return true;
    else if (m1.attitude > m2.attitude)
        return false;

    // Force plain but different coloured draconians to be treated like the
    // same sub-type.
    if (!zombified && m1.type >= MONS_DRACONIAN
        && m1.type <= MONS_PALE_DRACONIAN
        && m2.type >= MONS_DRACONIAN
        && m2.type <= MONS_PALE_DRACONIAN)
    {
        return false;
    }

    int diff_delta = mons_avg_hp(m1.type) - mons_avg_hp(m2.type);

    // By descending difficulty
    if (diff_delta > 0)
        return true;
    else if (diff_delta < 0)
        return false;

    // Force mimics of different types to be treated like the same one.
    if (mons_is_mimic(m1.type) && mons_is_mimic(m2.type))
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
        return m1.number > m2.number;

    if (m1.type == MONS_BALLISTOMYCETE)
        return (m1.number > 0) > (m2.number > 0);

    // Shifters after real monsters of the same type.
    if (m1.is(MB_SHAPESHIFTER) != m2.is(MB_SHAPESHIFTER))
        return m2.is(MB_SHAPESHIFTER);

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

        if (m1.type == MONS_CHIMERA)
        {
            for (int part = 1; part <= 3; part++)
            {
                const monster_type p1 = get_chimera_part(&m1, part);
                const monster_type p2 = get_chimera_part(&m2, part);

                if (p1 < p2)
                    return true;
                else if (p1 > p2)
                    return false;
            }
        }

        // Both monsters are hydras or hydra zombies, sort by number of heads.
        if (mons_genus(m1.type) == MONS_HYDRA || mons_genus(m1.base_type) == MONS_HYDRA)
        {
            if (m1.number > m2.number)
                return true;
            else if (m1.number < m2.number)
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
    if (mi.is(MB_FRENZIED))
        return "frenzied";
    if (mi.is(MB_ROUSED))
        return "roused";
    if (mi.is(MB_INNER_FLAME))
        return "inner flame";
    if (mi.is(MB_DUMB))
        return "dumb";
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
    if (mi.is(MB_WITHDRAWN))
        return "withdrawn";
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
    if (mi.is(MB_ROTTING))
        return "rotting";
    if (mi.is(MB_BLEEDING))
        return "bleeding";
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
    // Don't pluralise uniques, ever.  Multiple copies of the same unique
    // are unlikely in the dungeon currently, but quite common in the
    // arena.  This prevens "4 Gra", etc. {due}
    // Unless it's Mara, who summons illusions of himself.
    if (mons_is_unique(type) && type != MONS_MARA)
        return common_name();
    // Specialcase mimics, so they don't get described as piles of gold
    // when that would be inappropriate. (HACK)
    else if (mons_is_mimic(type))
        return "mimics";
    else if (mons_genus(type) == MONS_DRACONIAN)
        return pluralise(mons_type_name(MONS_DRACONIAN, DESC_PLAIN));
    else if (type == MONS_UGLY_THING || type == MONS_VERY_UGLY_THING
             || type == MONS_DANCING_WEAPON || !fullname)
    {
        return pluralise(mons_type_name(type, DESC_PLAIN));
    }
    else
        return pluralise(common_name());
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
    if (is(MB_POISONED))
        v.push_back("poisoned");
    if (is(MB_SICK))
        v.push_back("sick");
    if (is(MB_ROTTING))
        v.push_back("rotting away"); //jmf: "covered in sores"?
    if (is(MB_GLOWING))
        v.push_back("softly glowing");
    if (is(MB_SLOWED))
        v.push_back("moving slowly");
    if (is(MB_INSANE))
        v.push_back("frenzied and insane");
    if (is(MB_BERSERK))
        v.push_back("berserk");
    if (is(MB_FRENZIED))
        v.push_back("consumed by blood-lust");
    if (is(MB_ROUSED))
        v.push_back("inspired to greatness");
    if (is(MB_HASTED))
        v.push_back("moving very quickly");
    if (is(MB_STRONG))
        v.push_back("unusually strong");
    if (is(MB_CONFUSED))
        v.push_back("bewildered and confused");
    if (is(MB_INVISIBLE))
        v.push_back("slightly transparent");
    if (is(MB_CHARMED))
        v.push_back("in your thrall");
    if (is(MB_BURNING))
        v.push_back("covered in liquid flames");
    if (is(MB_CAUGHT))
        v.push_back("entangled in a net");
    if (is(MB_WEBBED))
        v.push_back("entangled in a web");
    if (is(MB_PETRIFIED))
        v.push_back("petrified");
    if (is(MB_PETRIFYING))
        v.push_back("slowly petrifying");
    if (is(MB_VULN_MAGIC))
        v.push_back("susceptible to magic");
    if (is(MB_SWIFT))
        v.push_back("moving somewhat quickly");
    if (is(MB_SILENCING))
        v.push_back("radiating silence");
    if (is(MB_PARALYSED))
        v.push_back("paralysed");
    if (is(MB_BLEEDING))
        v.push_back("bleeding");
    if (is(MB_DEFLECT_MSL))
        v.push_back("deflecting missiles");
    if (is(MB_PREP_RESURRECT))
        v.push_back("quietly preparing");
    if (is(MB_FEAR_INSPIRING))
        v.push_back("inspiring fear");
    if (is(MB_BREATH_WEAPON))
    {
        v.push_back(string("catching ")
                    + pronoun(PRONOUN_POSSESSIVE) + " breath");
    }
    if (is(MB_WITHDRAWN))
    {
        v.push_back("regenerating health quickly");
        v.push_back(string("protected by ")
                    + pronoun(PRONOUN_POSSESSIVE) + " shell");
    }
    if (is(MB_ATTACHED))
        v.push_back("attached and sucking blood");
    if (is(MB_DAZED))
        v.push_back("dazed");
    if (is(MB_MUTE))
        v.push_back("permanently mute");
    if (is(MB_BLIND))
        v.push_back("blind");
    if (is(MB_DUMB))
        v.push_back("stupefied");
    if (is(MB_MAD))
        v.push_back("lost in madness");
    if (is(MB_DEATHS_DOOR))
        v.push_back("standing in death's doorway");
    if (is(MB_REGENERATION))
        v.push_back("regenerating");
    if (is(MB_ROLLING))
        v.push_back("rolling");
    if (is(MB_STONESKIN))
        v.push_back("stone skin");
    if (is(MB_OZOCUBUS_ARMOUR))
        v.push_back("covered in an icy film");
    if (is(MB_WRETCHED))
        v.push_back("misshapen and mutated");
    if (is(MB_WORD_OF_RECALL))
        v.push_back("chanting recall");
    if (is(MB_INJURY_BOND))
        v.push_back("sheltered from injuries");
    if (is(MB_WATER_HOLD))
        v.push_back("engulfed in water");
    if (is(MB_WATER_HOLD_DROWN))
    {
        v.push_back("engulfed in water");
        v.push_back("unable to breathe");
    }
    if (is(MB_FLAYED))
        v.push_back("covered in terrible wounds");
    if (is(MB_RETCHING))
        v.push_back("retching with violent nausea");
    if (is(MB_WEAK))
        v.push_back("weak");
    if (is(MB_DIMENSION_ANCHOR))
        v.push_back("unable to translocate");
    if (is(MB_CONTROL_WINDS))
        v.push_back("controlling the winds");
    if (is(MB_WIND_AIDED))
        v.push_back("aim guided by the winds");
    if (is(MB_TOXIC_RADIANCE))
        v.push_back("radiating toxic energy");
    if (is(MB_GRASPING_ROOTS))
        v.push_back("movement impaired by roots");
    if (is(MB_FIRE_VULN))
        v.push_back("more vulnerable to fire");
    if (is(MB_TORNADO))
        v.push_back("surrounded by raging winds");
    if (is(MB_TORNADO_COOLDOWN))
        v.push_back("surrounded by restless winds");
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
            ret += artefact_wpn_property(*weapon, ra_prop);

        if (second && second->base_type == OBJ_WEAPONS && is_artefact(*second))
            ret += artefact_wpn_property(*second, ra_prop);

        if (armour && armour->base_type == OBJ_ARMOUR && is_artefact(*armour))
            ret += artefact_wpn_property(*armour, ra_prop);

        if (shield && shield->base_type == OBJ_ARMOUR && is_artefact(*shield))
            ret += artefact_wpn_property(*shield, ra_prop);

        if (ring && ring->base_type == OBJ_JEWELLERY && is_artefact(*ring))
            ret += artefact_wpn_property(*ring, ra_prop);
    }

    return ret;
}

int monster_info::res_magic() const
{
    int mr = (get_monster_data(type))->resist_magic;
    if (mr == MAG_IMMUNE)
        return MAG_IMMUNE;

    // Negative values get multiplied with monster hit dice.
    if (mr < 0)
        mr = mons_class_hit_dice(type) * (-mr) * 4 / 3;

    // Randarts have a multiplicative effect.
    mr *= (randarts(ARTP_MAGIC) + 100);
    mr /= 100;

    // ego armour resistance
    if (inv[MSLOT_ARMOUR].get()
        && get_armour_ego_type(*inv[MSLOT_ARMOUR]) == SPARM_MAGIC_RESISTANCE)
    {
        mr += 30;
    }

    if (inv[MSLOT_SHIELD].get()
        && get_armour_ego_type(*inv[MSLOT_SHIELD]) == SPARM_MAGIC_RESISTANCE)
    {
        mr += 30;
    }

    item_def *jewellery = inv[MSLOT_JEWELLERY].get();

    if (jewellery && (jewellery->base_type == OBJ_JEWELLERY)
        && (jewellery->sub_type == RING_PROTECTION_FROM_MAGIC))
    {
        mr += 40;
    }

    if (is(MB_VULN_MAGIC))
        mr /= 2;

    return mr;
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
                       || e->attack[0].type == AT_REACH_STING
                          ? REACH_TWO : REACH_NONE;

    const item_def *weapon = inv[MSLOT_WEAPON].get();
    if (weapon)
        range = max(range, weapon_reach(*weapon));

    return range;
}

size_type monster_info::body_size() const
{
    // Using base_type to get the right size for zombies, skeletons and such.
    // For normal monsters, base_type is set to type in the constructor.
    const monsterentry *e = get_monster_data(base_type);
    size_type ret = (e ? e->size : SIZE_MEDIUM);

    // Slime creature size is increased by the number merged.
    if (type == MONS_SLIME_CREATURE)
    {
        if (number == 2)
            ret = SIZE_MEDIUM;
        else if (number == 3)
            ret = SIZE_LARGE;
        else if (number == 4)
            ret = SIZE_BIG;
        else if (number == 5)
            ret = SIZE_GIANT;
    }
    else if (mons_is_item_mimic(type))
    {
        const int mass = item_mass(*inv[MSLOT_MISCELLANY].get());
        if (mass < 50)
            ret = SIZE_TINY;
        else if (mass < 100)
            ret = SIZE_LITTLE;
        else if (mass < 200)
            ret = SIZE_SMALL;
        else
            ret = SIZE_MEDIUM;
    }

    return ret;
}

bool monster_info::cannot_move() const
{
    return is(MB_PARALYSED) || is(MB_PETRIFIED) || is(MB_PREP_RESURRECT);
}

bool monster_info::airborne() const
{
    return (fly == FL_LEVITATE) || (fly == FL_WINGED && !cannot_move());
}

bool monster_info::ground_level() const
{
    return !airborne() && !is(MB_CLINGING);
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

    for (unsigned int i = 0; i < visible.size(); i++)
    {
        if (!mons_class_flag(visible[i]->type, M_NO_EXP_GAIN)
            || visible[i]->is_child_tentacle()
            || visible[i]->type == MONS_BALLISTOMYCETE
                && visible[i]->number > 0)
        {
            mons.push_back(monster_info(visible[i]));
        }
    }
    sort(mons.begin(), mons.end(), monster_info::less_than_wrapper);
}
