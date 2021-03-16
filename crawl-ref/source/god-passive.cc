#include "AppHdr.h"

#include "god-passive.h"

#include <algorithm>
#include <cmath>

#include "act-iter.h"
#include "areas.h"
#include "artefact.h"
#include "art-enum.h"
#include "branch.h"
#include "chardump.h"
#include "cloud.h"
#include "coordit.h"
#include "directn.h"
#include "env.h"
#include "fight.h"
#include "files.h"
#include "fprop.h"
#include "god-abil.h"
#include "god-prayer.h"
#include "invent.h" // in_inventory
#include "item-name.h"
#include "item-prop.h"
#include "item-status-flag-type.h"
#include "items.h"
#include "libutil.h"
#include "map-knowledge.h"
#include "melee-attack.h"
#include "message.h"
#include "mon-cast.h"
#include "mon-place.h"
#include "mon-util.h"
#include "output.h"
#include "player.h"
#include "religion.h"
#include "shout.h"
#include "skills.h"
#include "spl-clouds.h"
#include "state.h"
#include "stringutil.h"
#include "tag-version.h"
#include "terrain.h"
#include "throw.h"
#include "unwind.h"

// TODO: template out the differences between this and god_power.
// TODO: use the display method rather than dummy powers in god_powers.
// TODO: finish using these for implementing passive abilities.
struct god_passive
{
    // 1-6 means it unlocks at that many stars of piety;
    // 0 means it is always present when in good standing with the god;
    // -1 means it is present even under penance;
    int rank;
    passive_t pasv;
    /** Message to be given on gain of this passive.
     *
     * If the string begins with an uppercase letter, it is treated as
     * a complete sentence. Otherwise, if it contains the substring " NOW ",
     * the string "You " is prepended. Otherwise, the string "You NOW " is
     * prepended to the message, with the " NOW " then being replaced.
     *
     * The substring "GOD" is replaced with the name of the god.
     * The substring " NOW " is replaced with " now " for messages about
     * gaining the ability, or " " for messages about having the ability.
     *
     * Examples:
     *   "have super powers"
     *     => "You have super powers", "You now have super powers."
     *   "are NOW powerful"
     *     => "You are powerful", "You are now powerful."
     *   "Your power is NOW great"
     *     => "Your power is great", "Your power is now great"
     *   "GOD NOW makes you powerful"
     *     => "Moloch makes you powerful", "Moloch now makes you powerful"
     *   "GOD grants you a boon"
     *     => "Moloch grants you a boon" (in all situations)
     *
     * FIXME: This member is currently unused.
     *
     * @see god_passive::loss.
     */
    const char* gain;
    /** Message to be given on loss of this passive.
     *
     * If empty, use the gain message. This makes sense only if the message
     * contains " NOW ", either explicitly or implicitly through not
     * beginning with a capital letter.
     *
     * The substring "GOD" is replaced with the name of the god.
     * The substring " NOW " is replaced with " no longer ".
     *
     * Examples:
     *   "have super powers"
     *     => "You no longer have super powers"
     *   "are NOW powerful"
     *     => "You are no longer powerful"
     *   "Your power is NOW great"
     *     => "Your power is no longer great"
     *   "GOD NOW makes you powerful"
     *     => "Moloch no longer makes you powerful"
     *   "GOD grants you a boon"
     *     => "Moloch grants you a boon" (probably incorrect)
     *
     * FIXME: This member is currently unused.
     *
     * @see god_passive::gain.
     */
    const char* loss;

    god_passive(int rank_, passive_t pasv_, const char* gain_,
                const char* loss_ = "")
        : rank{rank_}, pasv{pasv_}, gain{gain_}, loss{*loss_ ? loss_ : gain_}
    { }

    god_passive(int rank_, const char* gain_, const char* loss_ = "")
        : god_passive(rank_, passive_t::none, gain_, loss_)
    { }

    void display(bool gaining, const char* fmt) const
    {
        const char * const str = gaining ? gain : loss;
        if (isupper(str[0]))
            god_speaks(you.religion, str);
        else
            god_speaks(you.religion, make_stringf(fmt, str).c_str());
    }
};

static const vector<god_passive> god_passives[] =
{
    // no god
    { },

    // Zin
    {
        { -1, passive_t::protect_from_harm,
              "GOD sometimes watches over you",
              "GOD no longer watches over you"
        },
        { -1, passive_t::resist_mutation,
              "GOD can shield you from mutations",
              "GOD NOW shields you from mutations"
        },
        { -1, passive_t::resist_polymorph,
              "GOD can protect you from unnatural transformations",
              "GOD NOW protects you from unnatural transformations",
        },
        { -1, passive_t::resist_hell_effects,
              "GOD can protect you from effects of Hell",
              "GOD NOW protects you from effects of Hell"
        },
        { -1, passive_t::warn_shapeshifter,
              "GOD will NOW warn you about shapeshifters"
        },
        {
          6, passive_t::cleanse_mut_potions,
              "GOD cleanses your potions of mutation",
              "GOD no longer cleanses your potions of mutation",
        }
    },

    // TSO
    {
        { -1, passive_t::protect_from_harm,
              "GOD sometimes watches over you",
              "GOD no longer watches over you"
        },
        { -1, passive_t::abjuration_protection_hd,
              "GOD NOW protects your summons from abjuration" },
        { -1, passive_t::bless_followers_vs_evil,
              "GOD NOW blesses your followers when they kill evil beings" },
        { -1, passive_t::restore_hp_mp_vs_evil,
              "gain health and magic from killing evil beings" },
        { -1, passive_t::no_stabbing,
              "are NOW prevented from stabbing unaware creatures" },
        {  0, passive_t::halo, "are NOW surrounded by divine halo" },
    },

    // Kikubaaqudgha
    {
        {  2, passive_t::miscast_protection_necromancy,
              "GOD NOW protects you from necromancy miscasts"
              " and mummy death curses"
        },
        {  4, passive_t::resist_torment,
              "GOD NOW protects you from torment" },
    },

    // Yredelemnul
    {
        {  3, passive_t::nightvision, "can NOW see well in the dark" },
    },

    // Xom
    { },

    // Vehumet
    {
        { -1, passive_t::mp_on_kill,
              "have a chance to gain magical power from killing" },
        {  3, passive_t::spells_success,
              "are NOW less likely to miscast destructive spells" },
        {  4, passive_t::spells_range,
              "can NOW cast destructive spells farther" },
    },

    // Okawaru
    {
        // None
    },

    // Makhleb
    {
        { -1, passive_t::restore_hp, "gain health from killing" },
    },

    // Sif Muna
    { },

    // Trog
    {
        { -1, passive_t::abjuration_protection,
              "GOD NOW protects your allies from abjuration"
        },
        {  0, passive_t::extend_berserk,
              "can NOW maintain berserk longer, and are less likely to pass out",
              "can NOW maintain berserk as long, and are more likely to pass out"
        },
    },

    // Nemelex
    {
        // None
    },

    // Elyvilon
    {
        { -1, passive_t::protect_from_harm,
              "GOD sometimes watches over you",
              "GOD no longer watches over you"
        },
        { -1, passive_t::protect_ally,
              "GOD can protect the life of your allies",
              "GOD NOW protects the life of your allies"
        },
    },

    // Lugonu
    {
        { -1, passive_t::safe_distortion,
              "are NOW protected from distortion unwield effects" },
        { -1, passive_t::map_rot_res_abyss,
              "remember the shape of the Abyss better" },
        {  5, passive_t::attract_abyssal_rune,
              "GOD will NOW help you find the Abyssal rune" },
    },

    // Beogh
    {
        { -1, passive_t::share_exp, "share experience with your followers" },
        {  3, passive_t::convert_orcs, "inspire orcs to join your side" },
        {  3, passive_t::bless_followers,
              "GOD will bless your followers",
              "GOD will no longer bless your followers"
        },
        {  5, passive_t::water_walk, "walk on water" },
    },

    // Jiyva
    {
        { -1, passive_t::neutral_slimes,
              "Slimes and eye monsters are NOW neutral towards you" },
        { -1, passive_t::jellies_army,
              "GOD NOW summons jellies to protect you" },
        { -1, passive_t::jelly_eating,
              "GOD NOW allows jellies to devour items" },
        { -1, passive_t::fluid_stats,
              "GOD NOW adjusts your attributes periodically" },
        {  0, passive_t::slime_wall_immune,
              "are NOW immune to slime covered walls" },
        {  2, passive_t::slime_feed,
              "Your fellow slimes NOW consume items" },
        {  3, passive_t::resist_corrosion,
              "GOD NOW protects you from corrosion" },
        {  4, passive_t::slime_mp,
              "Items consumed by your fellow slimes NOW restore"
              " your magical power"
        },
        {  5, passive_t::slime_hp,
              "Items consumed by your fellow slimes NOW restore"
              " your health"
        },
        {  6, passive_t::spawn_slimes_on_hit,
              "spawn slimes when struck by massive blows" },
        {  6, passive_t::unlock_slime_vaults,
              "GOD NOW grants you access to the hidden treasures"
              " of the Slime Pits"
        },
    },

    // Fedhas
    {
        { -1, passive_t::pass_through_plants,
              "can NOW walk through plants" },
        { -1, passive_t::shoot_through_plants,
              "can NOW safely fire through allied plants" },
        {  0, passive_t::friendly_plants,
              "Allied plants are NOW friendly towards you" },
    },

    // Cheibriados
    {
        { -1, passive_t::no_haste,
              "are NOW protected from inadvertent hurry" },
        { -1, passive_t::slowed, "move less quickly" },
        {  0, passive_t::slow_orb_run,
              "GOD will NOW aid your escape with the Orb of Zot",
        },
        {  0, passive_t::stat_boost,
              "GOD NOW supports your attributes"
        },
        {  0, passive_t::slow_abyss,
              "GOD will NOW slow the Abyss"
        },
        // TODO: this one should work regardless of penance, maybe?
        {  0, passive_t::slow_zot,
              "GOD will NOW slow Zot's hunt for you"
        },
        {  1, passive_t::slow_poison, "process poison slowly" },
    },

    // Ashenzari
    {
        { -1, passive_t::want_curses, "prefer cursed items" },
        { -1, passive_t::detect_portals, "sense portals" },
        { -1, passive_t::identify_items, "sense the properties of items" },
        {  0, passive_t::auto_map, "have improved mapping abilities" },
        {  0, passive_t::detect_montier, "sense threats" },
        {  0, passive_t::detect_items, "sense items" },
        {  0, passive_t::avoid_traps,
              "avoid traps" },
        {  0, passive_t::bondage_skill_boost,
              "get a skill boost from cursed items" },
        {  2, passive_t::sinv, "are NOW clear of vision" },
        {  3, passive_t::clarity, "are NOW clear of mind" },
        {  4, passive_t::xray_vision, "GOD NOW grants you astral sight" },
    },

    // Dithmenos
    {
        {  1, passive_t::nightvision, "can NOW see well in the dark" },
        {  1, passive_t::umbra, "are NOW surrounded by an umbra" },
        // TODO: this one should work regardless of penance.
        {  3, passive_t::hit_smoke, "emit smoke when hit" },
        {  4, passive_t::shadow_attacks,
              "Your attacks are NOW mimicked by a shadow" },
        {  4, passive_t::shadow_spells,
              "Your attack spells are NOW mimicked by a shadow" },
    },

    // Gozag
    {
        { -1, passive_t::detect_gold, "detect gold" },
        {  0, passive_t::goldify_corpses,
              "GOD NOW turns all corpses to gold" },
        {  0, passive_t::gold_aura, "have a gold aura" },
    },

    // Qazlal
    {
        {  0, passive_t::cloud_immunity, "and your divine allies are ADV immune to clouds" },
        {  1, passive_t::storm_shield,
              "generate elemental clouds to protect yourself" },
        {  4, passive_t::upgraded_storm_shield,
              "Your chances to be struck by projectiles are NOW reduced" },
        {  5, passive_t::elemental_adaptation,
              "Elemental attacks NOW leave you somewhat more resistant"
              " to them"
        }
    },

    // Ru
    {
        {  1, passive_t::aura_of_power,
              "Your enemies will sometimes fail their attack or even hit themselves",
              "Your enemies will NOW fail their attack or hit themselves"
        },
        {  2, passive_t::upgraded_aura_of_power,
              "Enemies that inflict damage upon you will sometimes receive"
              " a detrimental status effect",
              "Enemies that inflict damage upon you will NOW receive"
              " a detrimental status effect"
        },
    },

#if TAG_MAJOR_VERSION == 34
    // Pakellas
    {
        { -1, passive_t::no_mp_regen,
              "GOD NOW prevents you from regenerating your magical power" },
        { -1, passive_t::mp_on_kill, "have a chance to gain magical power from"
                                     " killing" },
        {  1, passive_t::bottle_mp,
              "GOD NOW collects and distills excess magic from your kills"
        },
    },
#endif

    // Uskayaw
    { },

    // Hepliaklqana
    {
        {  1, passive_t::frail,
              "GOD NOW siphons a part of your essence into your ancestor" },
        {  5, passive_t::transfer_drain,
              "drain nearby creatures when transferring your ancestor" },
    },

    // Wu Jian
    {
        { 0, passive_t::wu_jian_lunge, "perform damaging attacks by moving towards foes." },
        { 1, passive_t::wu_jian_whirlwind, "lightly attack monsters by moving around them." },
        { 2, passive_t::wu_jian_wall_jump, "perform airborne attacks in an area by jumping off a solid obstacle." },
    },
};
COMPILE_CHECK(ARRAYSZ(god_passives) == NUM_GODS);

static bool _god_gives_passive_if(god_type god, passive_t passive,
                                  function<bool(const god_passive&)> condition)
{
    const auto &pasvec = god_passives[god];
    return any_of(begin(pasvec), end(pasvec),
                  [&] (const god_passive &p) -> bool
                  { return p.pasv == passive && condition(p); });
}

bool god_gives_passive(god_type god, passive_t passive)
{
  return _god_gives_passive_if(god, passive,
                               [] (god_passive /*p*/) { return true; });
}

bool have_passive(passive_t passive)
{
    return _god_gives_passive_if(you.religion, passive,
                                 [] (god_passive p)
                                 {
                                     return piety_rank() >= p.rank
                                         && (!player_under_penance()
                                             || p.rank < 0);
                                 });
}

bool will_have_passive(passive_t passive)
{
  return _god_gives_passive_if(you.religion, passive,
                               [] (god_passive/*p*/) { return true; });
}

// Returns a large number (10) if we will never get this passive.
int rank_for_passive(passive_t passive)
{
    const auto &pasvec = god_passives[you.religion];
    const auto found = find_if(begin(pasvec), end(pasvec),
                              [passive] (const god_passive &p) -> bool
                              {
                                  return p.pasv == passive;
                              });
    return found == end(pasvec) ? 10 : found->rank;
}

int chei_stat_boost(int piety)
{
    if (!have_passive(passive_t::stat_boost))
        return 0;
    if (piety < piety_breakpoint(0))  // Since you've already begun to slow down.
        return 1;
    if (piety >= piety_breakpoint(5))
        return 15;
    return (piety - 10) / 10;
}

// Eat from one random off-level item stack.
void jiyva_eat_offlevel_items()
{
    // For wizard mode 'J' command
    if (!have_passive(passive_t::jelly_eating))
        return;

    if (crawl_state.game_is_sprint())
        return;

    while (true)
    {
        if (one_chance_in(200))
            break;

        const int branch = random2(NUM_BRANCHES);

        // Choose level based on main dungeon depth so that levels of
        // short branches aren't picked more often.
        ASSERT(brdepth[branch] <= MAX_BRANCH_DEPTH);
        const int level = random2(MAX_BRANCH_DEPTH) + 1;

        const level_id lid(static_cast<branch_type>(branch), level);

        if (lid == level_id::current() || !you.level_visited(lid))
            continue;

        dprf("Checking %s", lid.describe().c_str());

        level_excursion le;
        le.go_to(lid);
        while (true)
        {
            if (one_chance_in(200))
                break;

            const coord_def p = random_in_bounds();

            if (env.igrid(p) == NON_ITEM || testbits(env.pgrid(p), FPROP_NO_JIYVA))
                continue;

            for (stack_iterator si(p); si; ++si)
            {
                if (!item_is_jelly_edible(*si) || one_chance_in(4))
                    continue;

                if (one_chance_in(4))
                    break;

                dprf("Eating %s on %s",
                     si->name(DESC_PLAIN).c_str(), lid.describe().c_str());

                // Needs a message now to explain possible hp or mp
                // gain from jiyva_slurp_bonus()
                mpr("You hear a distant slurping noise.");
                jiyva_slurp_item_stack(*si);
                item_was_destroyed(*si);
                destroy_item(si.index());
            }
            return;
        }
    }
}

int ash_scry_radius()
{
    if (!have_passive(passive_t::xray_vision))
        return 0;

    // Radius 2 starting at 4* increasing to 4 at 6*
    return piety_rank() - 2;
}

static bool _two_handed()
{
    const item_def* wpn = you.slot_item(EQ_WEAPON, true);
    if (!wpn)
        return false;

    hands_reqd_type wep_type = you.hands_reqd(*wpn, true);
    return wep_type == HANDS_TWO;
}

static void _curse_boost_skills(const item_def &item)
{
    if (!item.props.exists(CURSE_KNOWLEDGE_KEY))
        return;

    for (auto& curse : item.props[CURSE_KNOWLEDGE_KEY].get_vector())
    {
        for (skill_type sk : curse_skills(curse))
        {
            if (you.skill_boost.count(sk))
                you.skill_boost[sk]++;
            else
                you.skill_boost[sk] = 1;
        }
    }
}

// Checks bondage and sets ash piety
void ash_check_bondage()
{
    if (!will_have_passive(passive_t::bondage_skill_boost))
        return;

#if TAG_MAJOR_VERSION == 34
    // Save compatibility for the new ash tag minor forgot to do this
    initialize_ashenzari_props();
#endif

    int num_cursed = 0, num_slots = 0;

    you.skill_boost.clear();
    for (int j = EQ_FIRST_EQUIP; j < NUM_EQUIP; j++)
    {
        const equipment_type i = static_cast<equipment_type>(j);

        // handles missing hand, octopode ring slots, finger necklace, species
        // armour restrictions, etc. Finger necklace slot counts.
        if (you_can_wear(i) == MB_FALSE)
            continue;

        // transformed away slots are still considered to be possibly bound
        num_slots++;
        if (you.equip[i] != -1)
        {
            const item_def& item = you.inv[you.equip[i]];
            if (item.cursed() && (i != EQ_WEAPON || is_weapon(item)))
            {
                if (i == EQ_WEAPON && _two_handed())
                    num_cursed += 2;
                else
                {
                    num_cursed++;
                    if (i == EQ_BODY_ARMOUR
                        && is_unrandom_artefact(item, UNRAND_LEAR))
                    {
                        num_cursed += 3;
                    }
                }
                if (!item_is_melded(item))
                    _curse_boost_skills(item);
            }
        }
    }

    set_piety(ASHENZARI_BASE_PIETY
              + (num_cursed * ASHENZARI_PIETY_SCALE) / num_slots);

    calc_hp(true);
    calc_mp(true);
}

// XXX: If this is called on an item in inventory, then auto_assign_item_slot
// needs to be called subsequently. However, moving an item in inventory
// invalidates its reference, which is a different behavior than for floor
// items, so we don't do it in this function.
bool god_id_item(item_def& item, bool silent)
{
    iflags_t old_ided = item.flags & ISFLAG_IDENT_MASK;

    if (have_passive(passive_t::identify_items))
    {
        // Don't identify runes or the orb, since this has no gameplay purpose
        // and might mess up other things.
        if (item.base_type == OBJ_RUNES || item_is_orb(item))
            return false;

        if ((item.base_type == OBJ_JEWELLERY || item.base_type == OBJ_STAVES)
            && item_needs_autopickup(item))
        {
            item.props["needs_autopickup"] = true;
        }
        set_ident_type(item, true);
        set_ident_flags(item, ISFLAG_IDENT_MASK);
    }

    iflags_t ided = item.flags & ISFLAG_IDENT_MASK;

    if (ided & ~old_ided)
    {
        if (item.props.exists("needs_autopickup") && is_useless_item(item))
            item.props.erase("needs_autopickup");

        if (&item == you.weapon())
            you.wield_change = true;

        if (!silent)
            mprf_nocap("%s", item.name(DESC_INVENTORY_EQUIP).c_str());

        seen_item(item);
        return true;
    }

    // nothing new
    return false;
}

static bool is_ash_portal(dungeon_feature_type feat)
{
    if (feat_is_portal_entrance(feat))
        return true;
    switch (feat)
    {
    case DNGN_ENTER_HELL:
    case DNGN_ENTER_ABYSS: // for completeness
    case DNGN_EXIT_THROUGH_ABYSS:
    case DNGN_EXIT_ABYSS:
    case DNGN_ENTER_PANDEMONIUM:
    case DNGN_EXIT_PANDEMONIUM:
    // DNGN_TRANSIT_PANDEMONIUM is too mundane
        return true;
    default:
        return false;
    }
}

// Yay for rectangle_iterator and radius_iterator not sharing a base type
static bool _check_portal(coord_def where)
{
    const dungeon_feature_type feat = env.grid(where);
    if (feat != env.map_knowledge(where).feat() && is_ash_portal(feat))
    {
        env.map_knowledge(where).set_feature(feat);
        set_terrain_mapped(where);

        if (!testbits(env.pgrid(where), FPROP_SEEN_OR_NOEXP))
        {
            env.pgrid(where) |= FPROP_SEEN_OR_NOEXP;
            if (!you.see_cell(where))
                return true;
        }
    }
    return false;
}

int ash_detect_portals(bool all)
{
    if (!have_passive(passive_t::detect_portals))
        return 0;

    int portals_found = 0;
    const int map_radius = LOS_DEFAULT_RANGE + 1;

    if (all)
    {
        for (rectangle_iterator ri(0); ri; ++ri)
        {
            if (_check_portal(*ri))
                portals_found++;
        }
    }
    else
    {
        for (radius_iterator ri(you.pos(), map_radius, C_SQUARE); ri; ++ri)
        {
            if (_check_portal(*ri))
                portals_found++;
        }
    }

    you.seen_portals += portals_found;
    return portals_found;
}

monster_type ash_monster_tier(const monster *mon)
{
    return monster_type(MONS_SENSED_TRIVIAL + monster_info(mon).threat);
}

/**
 * Does the player have an ash skill boost for a particular skill?
 */
bool ash_has_skill_boost(skill_type sk)
{
    return have_passive(passive_t::bondage_skill_boost)
           && you.skill_boost.count(sk) && you.skill_boost.find(sk)->second;
}

/**
 * Calculate the ash skill point boost for skill sk.
 *
 * @param scaled_skill the skill level to calculate it for, scaled by 10.
 *
 * @return the skill point bonus to use.
 */
unsigned int ash_skill_point_boost(skill_type sk, int scaled_skill)
{
    unsigned int skill_points = 0;

    skill_points += (you.skill_boost[sk] * 2 + 1) * (piety_rank() + 1)
                    * max(scaled_skill, 1) * species_apt_factor(sk);
    return skill_points;
}

int ash_skill_boost(skill_type sk, int scale)
{
    // It gives a bonus to skill points. The formula is:
    // ( curses * 2 + 1 ) * (piety_rank + 1) * skill_level

    unsigned int skill_points = you.skill_points[sk]
                  + get_crosstrain_points(sk)
                  + ash_skill_point_boost(sk, you.skill(sk, 10, true));

    int level = you.skills[sk];
    while (level < MAX_SKILL_LEVEL && skill_points >= skill_exp_needed(level + 1, sk))
        ++level;

    level = level * scale + get_skill_progress(sk, level, skill_points, scale);

    return min(level, MAX_SKILL_LEVEL * scale);
}

int gozag_gold_in_los(actor *whom)
{
    if (!have_passive(passive_t::gold_aura))
        return 0;

    int gold_count = 0;

    for (radius_iterator ri(whom->pos(), LOS_RADIUS, C_SQUARE, LOS_DEFAULT);
         ri; ++ri)
    {
        for (stack_iterator j(*ri); j; ++j)
        {
            if (j->base_type == OBJ_GOLD)
                ++gold_count;
        }
    }

    return gold_count;
}

void gozag_detect_level_gold(bool count)
{
    ASSERT(you.on_current_level);
    vector<item_def *> gold_piles;
    vector<coord_def> gold_places;
    int gold = 0;
    for (rectangle_iterator ri(0); ri; ++ri)
    {
        for (stack_iterator j(*ri); j; ++j)
        {
            if (j->base_type == OBJ_GOLD && !(j->flags & ISFLAG_UNOBTAINABLE))
            {
                gold += j->quantity;
                gold_piles.push_back(&(*j));
                gold_places.push_back(*ri);
            }
        }
    }

    if (!player_in_branch(BRANCH_ABYSS) && count)
        you.attribute[ATTR_GOLD_GENERATED] += gold;

    if (have_passive(passive_t::detect_gold))
    {
        for (unsigned int i = 0; i < gold_places.size(); i++)
        {
            int dummy = gold_piles[i]->index();
            coord_def &pos = gold_places[i];
            unlink_item(dummy);
            move_item_to_grid(&dummy, pos, true);
            if ((!env.map_knowledge(pos).item()
                 || env.map_knowledge(pos).item()->base_type != OBJ_GOLD
                 && you.visible_igrd(pos) != NON_ITEM))
            {
                env.map_knowledge(pos).set_item(
                        get_item_known_info(*gold_piles[i]),
                        !!env.map_knowledge(pos).item());
                env.map_knowledge(pos).flags |= MAP_DETECTED_ITEM;
#ifdef USE_TILE
                // force an update for gold generated during Abyss shifts
                tiles.update_minimap(pos);
#endif
            }
        }
    }
}

int qazlal_sh_boost(int piety)
{
    if (!have_passive(passive_t::storm_shield))
        return 0;

    return min(piety, piety_breakpoint(5)) / 10;
}

// Not actually passive, but placing it here so that it can be easily compared
// with Qazlal's boost.
int tso_sh_boost()
{
    if (!you.duration[DUR_DIVINE_SHIELD])
        return 0;

    return you.attribute[ATTR_DIVINE_SHIELD];
}

void qazlal_storm_clouds()
{
    if (!have_passive(passive_t::storm_shield))
        return;

    // You are a *storm*. You are pretty loud!
    noisy(min((int)you.piety, piety_breakpoint(5)) / 10, you.pos());

    const int radius = you.piety >= piety_breakpoint(3) ? 2 : 1;

    vector<coord_def> candidates;
    for (radius_iterator ri(you.pos(), radius, C_SQUARE, LOS_SOLID, true);
         ri; ++ri)
    {
        int count = 0;
        if (cell_is_solid(*ri) || cloud_at(*ri))
            continue;

        // Don't create clouds over firewood
        const monster * mon = monster_at(*ri);
        if (mon != nullptr && mons_is_firewood(*mon))
            continue;

        // No clouds in corridors.
        for (adjacent_iterator ai(*ri); ai; ++ai)
            if (!cell_is_solid(*ai))
                count++;

        if (count >= 5)
            candidates.push_back(*ri);
    }
    const int count =
        div_rand_round(min((int)you.piety, piety_breakpoint(5))
                       * candidates.size() * you.time_taken,
                       piety_breakpoint(5) * 7 * BASELINE_DELAY);
    if (count < 0)
        return;
    shuffle_array(candidates);
    int placed = 0;
    for (unsigned int i = 0; placed < count && i < candidates.size(); i++)
    {
        bool water = false;
        for (adjacent_iterator ai(candidates[i]); ai; ++ai)
        {
            if (feat_is_watery(env.grid(*ai)))
                water = true;
        }

        // No flame clouds over water to avoid steam generation.
        cloud_type ctype;
        do
        {
            ctype = random_choose(CLOUD_FIRE, CLOUD_COLD, CLOUD_STORM,
                                  CLOUD_DUST);
        } while (water && ctype == CLOUD_FIRE);

        place_cloud(ctype, candidates[i], random_range(3, 5), &you);
        placed++;
    }
}

/**
 * Handle Qazlal's elemental adaptation.
 * This should be called (exactly once) for physical, fire, cold, and electrical damage.
 * Right now, it is called only from expose_player_to_element. This may merit refactoring.
 *
 * @param flavour the beam type.
 * @param strength The adaptations will trigger strength in (11 - piety_rank()) times. In practice, this is mostly called with a value of 2.
 */
void qazlal_element_adapt(beam_type flavour, int strength)
{
    if (strength <= 0
        || !have_passive(passive_t::elemental_adaptation)
        || !x_chance_in_y(strength, 11 - piety_rank()))
    {
        return;
    }

    beam_type what = BEAM_NONE;
    duration_type dur = NUM_DURATIONS;
    string descript = "";
    switch (flavour)
    {
        case BEAM_FIRE:
        case BEAM_LAVA:
        case BEAM_STICKY_FLAME:
        case BEAM_STEAM:
            what = BEAM_FIRE;
            dur = DUR_QAZLAL_FIRE_RES;
            descript = "fire";
            break;
        case BEAM_COLD:
        case BEAM_ICE:
            what = BEAM_COLD;
            dur = DUR_QAZLAL_COLD_RES;
            descript = "cold";
            break;
        case BEAM_ELECTRICITY:
            what = BEAM_ELECTRICITY;
            dur = DUR_QAZLAL_ELEC_RES;
            descript = "electricity";
            break;
        case BEAM_MMISSILE: // for LCS, iron shot
        case BEAM_MISSILE:
        case BEAM_FRAG:
            what = BEAM_MISSILE;
            dur = DUR_QAZLAL_AC;
            descript = "physical attacks";
            break;
        default:
            return;
    }

    if (what != BEAM_FIRE && you.duration[DUR_QAZLAL_FIRE_RES])
    {
        mprf(MSGCH_DURATION, "Your resistance to fire fades away.");
        you.duration[DUR_QAZLAL_FIRE_RES] = 0;
    }

    if (what != BEAM_COLD && you.duration[DUR_QAZLAL_COLD_RES])
    {
        mprf(MSGCH_DURATION, "Your resistance to cold fades away.");
        you.duration[DUR_QAZLAL_COLD_RES] = 0;
    }

    if (what != BEAM_ELECTRICITY && you.duration[DUR_QAZLAL_ELEC_RES])
    {
        mprf(MSGCH_DURATION, "Your resistance to electricity fades away.");
        you.duration[DUR_QAZLAL_ELEC_RES] = 0;
    }

    if (what != BEAM_MISSILE && you.duration[DUR_QAZLAL_AC])
    {
        mprf(MSGCH_DURATION, "Your resistance to physical damage fades away.");
        you.duration[DUR_QAZLAL_AC] = 0;
        you.redraw_armour_class = true;
    }

    mprf(MSGCH_GOD, "You feel %sprotected from %s.",
         you.duration[dur] > 0 ? "more " : "", descript.c_str());

    // was scaled by 10 * strength. But the strength parameter is used so inconsistently that
    // it seems like a constant would be better, based on the typical value of 2.
    you.increase_duration(dur, 20, 80);

    if (what == BEAM_MISSILE)
        you.redraw_armour_class = true;
}

/**
 * Determine whether a Ru worshipper will attempt to interfere with an attack
 * against the player.
 *
 * @return bool Whether or not whether the worshipper will attempt to interfere.
 */
bool does_ru_wanna_redirect(monster* mon)
{
    return have_passive(passive_t::aura_of_power)
            && !mon->friendly()
            && you.see_cell_no_trans(mon->pos())
            && !mons_is_firewood(*mon)
            && !mon->submerged()
            && !mons_is_projectile(mon->type);
}

/**
 * Determine which, if any, action Ru takes on a possible attack.
 *
 * @return ru_interference
 */
ru_interference get_ru_attack_interference_level()
{
    int r = random2(100);
    int chance = div_rand_round(you.piety, 16);

    // 10% chance of stopping any attack at max piety
    if (r < chance)
        return DO_BLOCK_ATTACK;

    // 5% chance of redirect at max piety
    else if (r < chance + div_rand_round(chance, 2))
        return DO_REDIRECT_ATTACK;

    else
        return DO_NOTHING;
}

static bool _shadow_acts(bool spell)
{
    const passive_t pasv = spell ? passive_t::shadow_spells
                                 : passive_t::shadow_attacks;
    if (!have_passive(pasv))
        return false;

    const int minpiety = piety_breakpoint(rank_for_passive(pasv) - 1);

    // 10% chance at minimum piety; 50% chance at 200 piety.
    const int range = MAX_PIETY - minpiety;
    const int min   = range / 5;
    return x_chance_in_y(min + ((range - min)
                                * (you.piety - minpiety)
                                / (MAX_PIETY - minpiety)),
                         2 * range);
}

monster* shadow_monster(bool equip)
{
    if (monster_at(you.pos()))
        return nullptr;

    int wpn_index  = NON_ITEM;

    // Do a basic clone of the weapon.
    item_def* wpn = you.weapon();
    if (equip
        && wpn
        && is_weapon(*wpn))
    {
        wpn_index = get_mitm_slot(10);
        if (wpn_index == NON_ITEM)
            return nullptr;
        item_def& new_item = env.item[wpn_index];
        if (wpn->base_type == OBJ_STAVES)
        {
            new_item.base_type = OBJ_WEAPONS;
            new_item.sub_type  = WPN_STAFF;
        }
        else
        {
            new_item.base_type = wpn->base_type;
            new_item.sub_type  = wpn->sub_type;
        }
        new_item.quantity = 1;
        new_item.rnd = 1;
        new_item.flags   |= ISFLAG_SUMMONED;
    }

    monster* mon = get_free_monster();
    if (!mon)
    {
        if (wpn_index)
            destroy_item(wpn_index);
        return nullptr;
    }

    mon->type       = MONS_PLAYER_SHADOW;
    mon->behaviour  = BEH_SEEK;
    mon->attitude   = ATT_FRIENDLY;
    mon->flags      = MF_NO_REWARD | MF_JUST_SUMMONED | MF_SEEN
                    | MF_WAS_IN_VIEW | MF_HARD_RESET;
    mon->hit_points = you.hp;
    mon->set_hit_dice(min(27, max(1,
                                  you.skill_rdiv(wpn_index != NON_ITEM
                                                 ? item_attack_skill(env.item[wpn_index])
                                                 : SK_UNARMED_COMBAT, 10, 20)
                                  + you.skill_rdiv(SK_FIGHTING, 10, 20))));
    mon->set_position(you.pos());
    mon->mid        = MID_PLAYER;
    mon->inv[MSLOT_WEAPON]  = wpn_index;
    mon->inv[MSLOT_MISSILE] = NON_ITEM;

    env.mgrid(you.pos()) = mon->mindex();

    return mon;
}

void shadow_monster_reset(monster *mon)
{
    if (mon->inv[MSLOT_WEAPON] != NON_ITEM)
        destroy_item(mon->inv[MSLOT_WEAPON]);
    // in case the shadow unwields for some reason, e.g. you clumsily bash with
    // a ranged weapon:
    if (mon->inv[MSLOT_ALT_WEAPON] != NON_ITEM)
        destroy_item(mon->inv[MSLOT_ALT_WEAPON]);
    if (mon->inv[MSLOT_MISSILE] != NON_ITEM)
        destroy_item(mon->inv[MSLOT_MISSILE]);

    mon->reset();
}

/**
 * Check if the player is in melee range of the target.
 *
 * Certain effects, e.g. distortion blink, can cause monsters to leave melee
 * range between the initial hit & the shadow mimic.
 *
 * XXX: refactor this with attack/fight code!
 *
 * @param target    The creature to be struck.
 * @return          Whether the player is melee range of the target, using
 *                  their current weapon.
 */
static bool _in_melee_range(actor* target)
{
    const int dist = (you.pos() - target->pos()).rdist();
    return dist <= you.reach_range();
}

void dithmenos_shadow_melee(actor* target)
{
    if (!target
        || !target->alive()
        || !_in_melee_range(target)
        || !_shadow_acts(false))
    {
        return;
    }

    monster* mon = shadow_monster();
    if (!mon)
        return;

    mon->target     = target->pos();
    mon->foe        = target->mindex();

    fight_melee(mon, target);

    shadow_monster_reset(mon);
}

void dithmenos_shadow_throw(const dist &d, const item_def &item)
{
    ASSERT(d.isValid);
    if (!_shadow_acts(false))
        return;

    monster* mon = shadow_monster();
    if (!mon)
        return;

    int ammo_index = get_mitm_slot(10);
    if (ammo_index != NON_ITEM)
    {
        item_def& new_item = env.item[ammo_index];
        new_item.base_type = item.base_type;
        new_item.sub_type  = item.sub_type;
        new_item.quantity  = 1;
        new_item.rnd = 1;
        new_item.flags    |= ISFLAG_SUMMONED;
        mon->inv[MSLOT_MISSILE] = ammo_index;

        mon->target = clamp_in_bounds(d.target);

        bolt beem;
        beem.set_target(d);
        setup_monster_throw_beam(mon, beem);
        beem.item = &env.item[mon->inv[MSLOT_MISSILE]];
        mons_throw(mon, beem, mon->inv[MSLOT_MISSILE]);
    }

    shadow_monster_reset(mon);
}

void dithmenos_shadow_spell(bolt* orig_beam, spell_type spell)
{
    if (!orig_beam)
        return;

    const coord_def target = orig_beam->target;

    if (orig_beam->target.origin()
        || (orig_beam->is_enchantment() && !is_valid_mon_spell(spell))
        || orig_beam->flavour == BEAM_CHARM
           && monster_at(target) && monster_at(target)->friendly()
        || !_shadow_acts(true))
    {
        return;
    }

    monster* mon = shadow_monster();
    if (!mon)
        return;

    // Don't let shadow spells get too powerful.
    mon->set_hit_dice(max(1,
                          min(3 * spell_difficulty(spell),
                              you.experience_level) / 2));

    mon->target = clamp_in_bounds(target);
    if (actor_at(target))
        mon->foe = actor_at(target)->mindex();

    spell_type shadow_spell = spell;
    if (!orig_beam->is_enchantment())
    {
        shadow_spell = (orig_beam->pierce) ? SPELL_SHADOW_BOLT
                                           : SPELL_SHADOW_SHARD;
    }

    bolt beem;
    beem.target = target;
    beem.aimed_at_spot = orig_beam->aimed_at_spot;

    mprf(MSGCH_FRIEND_SPELL, "%s mimicks your spell!",
         mon->name(DESC_THE).c_str());
    mons_cast(mon, beem, shadow_spell, MON_SPELL_WIZARD, false);

    shadow_monster_reset(mon);
}

static void _wu_jian_trigger_serpents_lash(const coord_def& old_pos,
                                           bool wall_jump)
{
    if (you.attribute[ATTR_SERPENTS_LASH] == 0)
       return;

    if (wall_jump && you.attribute[ATTR_SERPENTS_LASH] == 1)
    {
        // No turn manipulation, since we are only refunding half a wall jump's
        // time (the walk speed modifier for this special case is already
        // factored in main.cc)
        you.attribute[ATTR_SERPENTS_LASH] = 0;
    }
    else
    {
        you.turn_is_over = false;
        you.elapsed_time_at_last_input = you.elapsed_time;
        you.attribute[ATTR_SERPENTS_LASH] -= wall_jump ? 2 : 1;
        you.redraw_status_lights = true;
        update_turn_count();
    }

    if (you.attribute[ATTR_SERPENTS_LASH] == 0)
    {
        you.increase_duration(DUR_EXHAUSTED, 12 + random2(5));
        mpr("Your supernatural speed expires.");
    }

    if (!cell_is_solid(old_pos))
        check_place_cloud(CLOUD_DUST, old_pos, 2 + random2(3) , &you, 1, -1);
}

static void _wu_jian_increment_heavenly_storm()
{
    int storm = you.props[WU_JIAN_HEAVENLY_STORM_KEY].get_int();
    if (storm < WU_JIAN_HEAVENLY_STORM_MAX)
    {
        you.props[WU_JIAN_HEAVENLY_STORM_KEY].get_int()++;
        you.redraw_evasion = true;
    }
}

void wu_jian_heaven_tick()
{
    for (radius_iterator ai(you.pos(), 2, C_SQUARE, LOS_SOLID); ai; ++ai)
        if (!cell_is_solid(*ai))
            place_cloud(CLOUD_GOLD_DUST, *ai, 5 + random2(5), &you);

    noisy(12, you.pos());
}

void wu_jian_decrement_heavenly_storm()
{
    int storm = you.props[WU_JIAN_HEAVENLY_STORM_KEY].get_int();

    if (storm > 1)
    {
        you.props[WU_JIAN_HEAVENLY_STORM_KEY].get_int()--;
        you.set_duration(DUR_HEAVENLY_STORM, random_range(2, 3));
        you.redraw_evasion = true;
    }
    else
        wu_jian_end_heavenly_storm();
}

// TODO: why isn't this implemented as a duration end effect?
void wu_jian_end_heavenly_storm()
{
    you.props.erase(WU_JIAN_HEAVENLY_STORM_KEY);
    you.duration[DUR_HEAVENLY_STORM] = 0;
    you.redraw_evasion = true;
    invalidate_agrid(true);
    mprf(MSGCH_GOD, "The heavenly storm settles.");
}

bool wu_jian_has_momentum(wu_jian_attack_type attack_type)
{
    return you.attribute[ATTR_SERPENTS_LASH]
           && attack_type != WU_JIAN_ATTACK_NONE
           && attack_type != WU_JIAN_ATTACK_TRIGGERED_AUX;
}

static bool _can_attack_martial(const monster* mons)
{
    return !(mons->wont_attack()
             || mons_is_firewood(*mons)
             || mons_is_projectile(mons->type)
             || !you.can_see(*mons));
}

// A mismatch between attack speed and move speed may cause any particular
// martial attack to be doubled, tripled, or not happen at all. Given enough
// time moving, you would have made the same amount of attacks as tabbing.
static int _wu_jian_number_of_attacks(bool wall_jump)
{
    // Under the effect of serpent's lash, move delay is normalized to
    // 10 aut for every character, to avoid punishing fast races.
    const int move_delay = you.attribute[ATTR_SERPENTS_LASH]
                           ? 100
                           : player_movement_speed() * player_speed();

    int attack_delay;

    {
        // attack_delay() is dependent on you.time_taken, which won't be set
        // appropriately during a movement turn. This temporarily resets
        // you.time_taken to the initial value (see `_prep_input`) used for
        // basic, simple, melee attacks.
        // TODO: can `attack_delay` be changed to not depend on you.time_taken?
        unwind_var<int> reset_speed(you.time_taken, player_speed());
        attack_delay = you.attack_delay().roll();
    }

    return div_rand_round(wall_jump ? 2 * move_delay : move_delay,
                          attack_delay * BASELINE_DELAY);
}

static bool _wu_jian_lunge(const coord_def& old_pos)
{
    coord_def lunge_direction = (you.pos() - old_pos).sgn();
    coord_def potential_target = you.pos() + lunge_direction;
    monster* mons = monster_at(potential_target);

    if (!mons || !_can_attack_martial(mons) || !mons->alive())
        return false;

    if (you.props.exists(WU_JIAN_HEAVENLY_STORM_KEY))
        _wu_jian_increment_heavenly_storm();

    you.apply_berserk_penalty = false;

    const int number_of_attacks = _wu_jian_number_of_attacks(false);

    if (number_of_attacks == 0)
    {
        mprf("You lunge at %s, but your attack speed is too slow for a blow "
             "to land.", mons->name(DESC_THE).c_str());
        return false;
    }
    else
    {
        mprf("You lunge%s at %s%s.",
             wu_jian_has_momentum(WU_JIAN_ATTACK_LUNGE) ?
                 " with incredible momentum" : "",
             mons->name(DESC_THE).c_str(),
             number_of_attacks > 1 ? ", in a flurry of attacks" : "");
    }

    count_action(CACT_INVOKE, ABIL_WU_JIAN_LUNGE);

    for (int i = 0; i < number_of_attacks; i++)
    {
        if (!mons->alive())
            break;
        melee_attack lunge(&you, mons);
        lunge.wu_jian_attack = WU_JIAN_ATTACK_LUNGE;
        lunge.attack();
    }

    return true;
}

// Monsters adjacent to the given pos that are valid targets for whirlwind.
static vector<monster*> _get_whirlwind_targets(coord_def pos)
{
    vector<monster*> targets;
    for (adjacent_iterator ai(pos, true); ai; ++ai)
        if (monster_at(*ai) && _can_attack_martial(monster_at(*ai)))
            targets.push_back(monster_at(*ai));
    sort(targets.begin(), targets.end());
    return targets;
}

static bool _wu_jian_whirlwind(const coord_def& old_pos)
{
    bool did_at_least_one_attack = false;

    const vector<monster*> targets = _get_whirlwind_targets(you.pos());
    if (targets.empty())
        return did_at_least_one_attack;

    const vector<monster*> old_targets = _get_whirlwind_targets(old_pos);
    vector<monster*> common_targets;
    set_intersection(targets.begin(), targets.end(),
                     old_targets.begin(), old_targets.end(),
                     back_inserter(common_targets));

    for (auto mons : common_targets)
    {
        if (!mons->alive())
            continue;

        if (you.props.exists(WU_JIAN_HEAVENLY_STORM_KEY))
            _wu_jian_increment_heavenly_storm();

        you.apply_berserk_penalty = false;

        const int number_of_attacks = _wu_jian_number_of_attacks(false);
        if (number_of_attacks == 0)
        {
            mprf("You spin to attack %s, but your attack speed is too slow for "
                 "a blow to land.", mons->name(DESC_THE).c_str());
            continue;
        }
        else
        {
            mprf("You spin and attack %s%s%s.",
                 mons->name(DESC_THE).c_str(),
                 number_of_attacks > 1 ? " repeatedly" : "",
                 wu_jian_has_momentum(WU_JIAN_ATTACK_WHIRLWIND) ?
                     ", with incredible momentum" : "");
        }

        count_action(CACT_INVOKE, ABIL_WU_JIAN_WHIRLWIND);

        for (int i = 0; i < number_of_attacks; i++)
        {
            if (!mons->alive())
                break;
            melee_attack whirlwind(&you, mons);
            whirlwind.wu_jian_attack = WU_JIAN_ATTACK_WHIRLWIND;
            whirlwind.wu_jian_number_of_targets = common_targets.size();
            whirlwind.attack();
            if (!did_at_least_one_attack)
              did_at_least_one_attack = true;
        }
    }

    return did_at_least_one_attack;
}

static bool _wu_jian_trigger_martial_arts(const coord_def& old_pos)
{
    bool did_wu_jian_attacks = false;

    if (you.pos() == old_pos || you.duration[DUR_CONF])
        return did_wu_jian_attacks;

    if (have_passive(passive_t::wu_jian_lunge))
        did_wu_jian_attacks = _wu_jian_lunge(old_pos);

    if (have_passive(passive_t::wu_jian_whirlwind))
        did_wu_jian_attacks |= _wu_jian_whirlwind(old_pos);

    return did_wu_jian_attacks;
}

void wu_jian_wall_jump_effects()
{
    vector<monster*> targets;
    for (adjacent_iterator ai(you.pos(), true); ai; ++ai)
    {
        monster* target = monster_at(*ai);
        if (target && _can_attack_martial(target) && target->alive())
            targets.push_back(target);

        if (!cell_is_solid(*ai))
            check_place_cloud(CLOUD_DUST, *ai, 1 + random2(3) , &you, 0, -1);
    }

    for (auto target : targets)
    {
        if (!target->alive())
            continue;

        if (you.props.exists(WU_JIAN_HEAVENLY_STORM_KEY))
            _wu_jian_increment_heavenly_storm();

        you.apply_berserk_penalty = false;

        // Twice the attacks as Wall Jump spends twice the time
        const int number_of_attacks = _wu_jian_number_of_attacks(true);
        if (number_of_attacks == 0)
        {
            mprf("You attack %s from above, but your attack speed is too slow"
                 " for a blow to land.", target->name(DESC_THE).c_str());
            continue;
        }
        else
        {
            mprf("You %sattack %s from above%s.",
                 number_of_attacks > 1 ? "repeatedly " : "",
                 target->name(DESC_THE).c_str(),
                 wu_jian_has_momentum(WU_JIAN_ATTACK_WALL_JUMP) ?
                     ", with incredible momentum" : "");
        }

        for (int i = 0; i < number_of_attacks; i++)
        {
            if (!target->alive())
                break;

            melee_attack aerial(&you, target);
            aerial.wu_jian_attack = WU_JIAN_ATTACK_WALL_JUMP;
            aerial.wu_jian_number_of_targets = targets.size();
            aerial.attack();
        }
    }
}

bool wu_jian_post_move_effects(bool did_wall_jump,
                               const coord_def& initial_position)
{
    bool did_wu_jian_attacks = false;

    if (!did_wall_jump)
        did_wu_jian_attacks = _wu_jian_trigger_martial_arts(initial_position);

    if (you.turn_is_over)
        _wu_jian_trigger_serpents_lash(initial_position, did_wall_jump);

    return did_wu_jian_attacks;
}

/**
 * check if the monster in this cell exists and is a valid target for Uskayaw
 */
static int _check_for_uskayaw_targets(coord_def where)
{
    if (!cell_has_valid_target(where))
        return 0;
    monster* mons = monster_at(where);
    ASSERT(mons);

    if (mons_is_firewood(*mons))
        return 0;

    return 1;
}

/**
 * Paralyse the monster in this cell, assuming one exists.
 *
 * Duration increases with invocations and experience level, and decreases
 * with target HD. The duration is pretty low, maxing out at 40 AUT.
 */
static int _prepare_audience(coord_def where)
{
    if (!_check_for_uskayaw_targets(where))
        return 0;

    monster* mons = monster_at(where);

    int power =  max(1, random2(1 + you.skill(SK_INVOCATIONS, 2))
                 + you.experience_level - mons->get_hit_dice());
    int duration = min(max(10, 5 + power), 40);
    mons->add_ench(mon_enchant(ENCH_PARALYSIS, 1, &you, duration));

    return 1;
}

/**
 * On hitting *** piety, all the monsters are paralysed by their appreciation
 * for your dance.
 */
void uskayaw_prepares_audience()
{
    int count = apply_area_visible(_check_for_uskayaw_targets, you.pos());
    if (count > 0)
    {
        simple_god_message(" prepares the audience for your solo!");
        apply_area_visible(_prepare_audience, you.pos());

        // Increment a delay timer to prevent players from spamming this ability
        // via piety loss and gain. Timer is in AUT.
        you.props[USKAYAW_AUDIENCE_TIMER] = 300 + random2(201);
    }
    else // Reset the timer because we didn't actually execute.
        you.props[USKAYAW_AUDIENCE_TIMER] = 0;
}

/**
 * Apply pain bond to the monster in this cell.
 */
static int _bond_audience(coord_def where)
{
    if (!_check_for_uskayaw_targets(where))
        return 0;

    monster* mons = monster_at(where);

    // Don't pain bond monsters that aren't invested in fighting the player
    if (mons->wont_attack())
        return 0;

    int power = you.skill(SK_INVOCATIONS, 7) + you.experience_level
                 - mons->get_hit_dice();
    int duration = 20 + random2avg(power, 2);
    mons->add_ench(mon_enchant(ENCH_PAIN_BOND, 1, &you, duration));

    return 1;
}

/**
 * On hitting **** piety, all the monsters are pain bonded.
 */
void uskayaw_bonds_audience()
{
    int count = apply_area_visible(_check_for_uskayaw_targets, you.pos());
    if (count > 1)
    {
        simple_god_message(" links your audience in an emotional bond!");
        apply_area_visible(_bond_audience, you.pos());

        // Increment a delay timer to prevent players from spamming this ability
        // via piety loss and gain. Timer is in AUT.
        you.props[USKAYAW_BOND_TIMER] = 300 + random2(201);
    }
    else // Reset the timer because we didn't actually execute.
        you.props[USKAYAW_BOND_TIMER] = 0;
}
