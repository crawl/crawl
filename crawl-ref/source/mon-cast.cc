/**
 * @file
 * @brief Monster spell casting.
**/

#include "AppHdr.h"

#include "mon-cast.h"

#include <algorithm>
#include <math.h>

#include "act-iter.h"
#include "areas.h"
#include "branch.h"
#include "cloud.h"
#include "colour.h"
#include "coordit.h"
#include "database.h"
#include "delay.h"
#include "effects.h"
#include "english.h"
#include "env.h"
#include "evoke.h"
#include "exclude.h"
#include "fight.h"
#include "fprop.h"
#include "godpassive.h"
#include "items.h"
#include "libutil.h"
#include "losglobal.h"
#include "mapmark.h"
#include "message.h"
#include "misc.h"
#include "mon-act.h"
#include "mon-behv.h"
#include "mon-book.h"
#include "mon-clone.h"
#include "mon-death.h"
#include "mon-gear.h"
#include "mon-pathfind.h"
#include "mon-pick.h"
#include "mon-place.h"
#include "mon-project.h"
#include "mon-speak.h"
#include "mon-tentacle.h"
#include "mutation.h"
#include "random-weight.h"
#include "religion.h"
#include "shout.h"
#include "spl-clouds.h"
#include "spl-damage.h"
#include "spl-monench.h"
#include "spl-summoning.h"
#include "spl-util.h"
#include "state.h"
#include "stepdown.h"
#include "stringutil.h"
#include "target.h"
#include "teleport.h"
#include "terrain.h"
#ifdef USE_TILE
#include "tiledef-dngn.h"
#endif
#include "traps.h"
#include "view.h"
#include "viewchar.h"
#include "xom.h"

// tentacle stuff
const int MAX_ACTIVE_KRAKEN_TENTACLES = 4;
const int MAX_ACTIVE_STARSPAWN_TENTACLES = 2;

static bool _valid_mon_spells[NUM_SPELLS];

static int  _mons_mesmerise(monster* mons, bool actual = true);
static int  _mons_cause_fear(monster* mons, bool actual = true);
static int  _mons_mass_confuse(monster* mons, bool actual = true);
static int _mons_available_tentacles(monster* head);
static coord_def _mons_fragment_target(monster *mons);
static bool _mons_consider_tentacle_throwing(const monster &mons);
static bool _tentacle_toss(const monster &thrower, actor &victim, int pow);
static int _throw_site_score(const monster &thrower, const actor &victim,
                             const coord_def &site);
static void _siren_sing(monster* mons, bool avatar);
static bool _ms_waste_of_time(monster* mon, mon_spell_slot slot);

void init_mons_spells()
{
    monster fake_mon;
    fake_mon.type       = MONS_BLACK_DRACONIAN;
    fake_mon.hit_points = 1;

    bolt pbolt;

    for (int i = 0; i < NUM_SPELLS; i++)
    {
        spell_type spell = (spell_type) i;

        _valid_mon_spells[i] = false;

        if (!is_valid_spell(spell))
            continue;

        if (spell == SPELL_MELEE
            || setup_mons_cast(&fake_mon, pbolt, spell, true))
        {
            _valid_mon_spells[i] = true;
        }
    }
}

bool is_valid_mon_spell(spell_type spell)
{
    if (spell < 0 || spell >= NUM_SPELLS)
        return false;

    return _valid_mon_spells[spell];
}

static bool _flavour_benefits_monster(beam_type flavour, monster& monster)
{
    switch (flavour)
    {
    case BEAM_HASTE:
        return !monster.has_ench(ENCH_HASTE);

    case BEAM_MIGHT:
        return !monster.has_ench(ENCH_MIGHT);

    case BEAM_INVISIBILITY:
        return !monster.has_ench(ENCH_INVIS);

    case BEAM_HEALING:
        return monster.hit_points != monster.max_hit_points;

    case BEAM_AGILITY:
        return !monster.has_ench(ENCH_AGILE);

    default:
        return false;
    }
}

// Find an allied monster to cast a beneficial beam spell at.
static bool _set_allied_target(monster* caster, bolt& pbolt, bool ignore_genus)
{
    monster* selected_target = NULL;
    int min_distance = INT_MAX;

    const monster_type caster_genus = mons_genus(caster->type);

    for (monster_near_iterator targ(caster, LOS_NO_TRANS); targ; ++targ)
    {
        if (*targ == caster)
            continue;

        const int targ_distance = grid_distance(targ->pos(), caster->pos());

        bool got_target = false;

        if ((mons_genus(targ->type) == caster_genus
                 || mons_genus(targ->base_monster) == caster_genus
                 || targ->is_holy() && caster->is_holy()
                 || mons_enslaved_soul(caster)
                 || ignore_genus)
            && mons_aligned(*targ, caster)
            && !targ->has_ench(ENCH_CHARM)
            && !mons_is_firewood(*targ)
            && _flavour_benefits_monster(pbolt.flavour, **targ))
        {
            got_target = true;
        }

        if (got_target && targ_distance < min_distance
            && targ_distance < pbolt.range)
        {
            // Make sure we won't hit an invalid target with this aim.
            pbolt.target = targ->pos();
            fire_tracer(caster, pbolt);
            if (!mons_should_fire(pbolt)
                || pbolt.path_taken.back() != pbolt.target)
            {
                continue;
            }

            min_distance = targ_distance;
            selected_target = *targ;
        }
    }

    if (selected_target)
    {
        pbolt.target = selected_target->pos();
        return true;
    }

    // Didn't find a target.
    return false;
}

bolt mons_spell_beam(monster* mons, spell_type spell_cast, int power,
                     bool check_validity)
{
    ASSERT(power > 0);

    bolt beam;

    // Initialise to some bogus values so we can catch problems.
    beam.name         = "****";
    beam.colour       = 255;
    beam.hit          = -1;
    beam.damage       = dice_def(1, 0);
    beam.ench_power   = -1;
    beam.glyph        = 0;
    beam.flavour      = BEAM_NONE;
    beam.thrower      = KILL_MISC;
    beam.is_beam      = false;
    beam.is_explosion = false;

    switch (spell_cast)
    { // add touch or range-setting spells here
        case SPELL_SANDBLAST:
            break;
        case SPELL_FLAME_TONGUE:
            // HD:1 monsters would get range 2, HD:2 -- 3, other 4, let's
            // use the mighty Throw Flame for big ranges.
            // Here, we have HD:1 -- 1, HD:2+ -- 2.
            beam.range = (power >= 20) ? 2 : 1;
            break;
        default:
        beam.range = spell_range(spell_cast, power, false);
    }

    spell_type real_spell = spell_cast;

    if (spell_cast == SPELL_MAJOR_DESTRUCTION)
    {
        real_spell = random_choose(SPELL_BOLT_OF_FIRE,
                                   SPELL_FIREBALL,
                                   SPELL_LIGHTNING_BOLT,
                                   SPELL_STICKY_FLAME,
                                   SPELL_IRON_SHOT,
                                   SPELL_BOLT_OF_DRAINING,
                                   SPELL_ORB_OF_ELECTRICITY,
                                   -1);
    }
    else if (spell_cast == SPELL_RANDOM_BOLT)
    {
        real_spell = random_choose(SPELL_VENOM_BOLT,
                                   SPELL_BOLT_OF_DRAINING,
                                   SPELL_BOLT_OF_FIRE,
                                   SPELL_LIGHTNING_BOLT,
                                   SPELL_QUICKSILVER_BOLT,
                                   SPELL_CRYSTAL_BOLT,
                                   SPELL_CORROSIVE_BOLT,
                                   -1);
    }
    else if (spell_cast == SPELL_LEGENDARY_DESTRUCTION)
    {
        // ones with ranges too small are fixed in setup_mons_cast
        real_spell = random_choose_weighted(10, SPELL_ORB_OF_ELECTRICITY,
                                            10, SPELL_LEHUDIBS_CRYSTAL_SPEAR,
                                             2, SPELL_IOOD,
                                             5, SPELL_GHOSTLY_FIREBALL,
                                            10, SPELL_FIREBALL,
                                            10, SPELL_FLASH_FREEZE,
                                             0);
    }
    else if (spell_cast == SPELL_SERPENT_OF_HELL_BREATH)
    {
        // this will be fixed up later in mons_cast
        real_spell = SPELL_FIRE_BREATH;
    }
    beam.glyph = dchar_glyph(DCHAR_FIRED_ZAP); // default
    beam.thrower = KILL_MON_MISSILE;
    beam.origin_spell = real_spell;
    beam.source_id = mons->mid;
    beam.source_name = mons->name(DESC_A, true);

    // FIXME: this should use the zap_data[] struct from beam.cc!
    switch (real_spell)
    {
    case SPELL_MAGIC_DART:
        beam.colour   = LIGHTMAGENTA;
        beam.name     = "magic dart";
        beam.damage   = dice_def(3, 4 + (power / 100));
        beam.hit      = AUTOMATIC_HIT;
        beam.flavour  = BEAM_MMISSILE;
        break;

    case SPELL_THROW_FLAME:
        beam.colour   = RED;
        beam.name     = "puff of flame";
        beam.damage   = dice_def(3, 5 + (power / 40));
        beam.hit      = 25 + power / 40;
        beam.flavour  = BEAM_FIRE;
        break;

    case SPELL_THROW_FROST:
        beam.colour   = WHITE;
        beam.name     = "puff of frost";
        beam.damage   = dice_def(3, 5 + (power / 40));
        beam.hit      = 25 + power / 40;
        beam.flavour  = BEAM_COLD;
        break;

    case SPELL_SANDBLAST:
        beam.colour   = BROWN;
        beam.name     = "rocky blast";
        beam.damage   = dice_def(3, 5 + (power / 40));
        beam.hit      = 20 + power / 40;
        beam.flavour  = BEAM_FRAG;
        beam.range    = 2;      // spell_range() is wrong here
        break;

    case SPELL_DISPEL_UNDEAD:
        beam.flavour  = BEAM_DISPEL_UNDEAD;
        beam.damage   = dice_def(3, min(6 + power / 10, 40));
        beam.is_beam  = true;
        break;

    case SPELL_PARALYSE:
        beam.flavour  = BEAM_PARALYSIS;
        beam.is_beam  = true;
        break;

    case SPELL_PETRIFY:
        beam.flavour  = BEAM_PETRIFY;
        beam.is_beam  = true;
        break;

    case SPELL_SLOW:
        beam.flavour  = BEAM_SLOW;
        beam.is_beam  = true;
        break;

    case SPELL_HASTE_OTHER:
    case SPELL_HASTE:
        beam.flavour  = BEAM_HASTE;
        break;

    case SPELL_MIGHT_OTHER:
    case SPELL_MIGHT:
        beam.flavour  = BEAM_MIGHT;
        break;

    case SPELL_CORONA:
        beam.flavour  = BEAM_CORONA;
        beam.is_beam  = true;
        break;

    case SPELL_CONFUSE:
        beam.flavour  = BEAM_CONFUSION;
        beam.is_beam  = true;
        break;

    case SPELL_HIBERNATION:
        beam.flavour  = BEAM_HIBERNATION;
        beam.is_beam  = true;
        break;

    case SPELL_SLEEP:
        beam.flavour    = BEAM_SLEEP;
        beam.is_beam    = true;
        beam.ench_power = 6 * mons->spell_hd(real_spell);
        break;

    case SPELL_POLYMORPH:
        beam.flavour  = BEAM_POLYMORPH;
        beam.is_beam  = true;
        break;

    case SPELL_MALMUTATE:
        beam.flavour  = BEAM_MALMUTATE;
        beam.is_beam  = true;
        /*
          // Be careful with this one.
          // Having allies mutate you is infuriating.
          beam.foe_ratio = 1000;
        What's the point of this?  Enchantments always hit...
        */
        break;

    case SPELL_FLAME_TONGUE:
        beam.name     = "flame tongue";
        beam.damage   = dice_def(3, 3 + power / 12);
        beam.colour   = RED;
        beam.flavour  = BEAM_FIRE;
        beam.hit      = 7 + power / 6;
        beam.is_beam  = true;
        break;

    case SPELL_VENOM_BOLT:
        beam.name     = "bolt of poison";
        beam.damage   = dice_def(3, 6 + power / 13);
        beam.colour   = LIGHTGREEN;
        beam.flavour  = BEAM_POISON;
        beam.hit      = 19 + power / 20;
        beam.is_beam  = true;
        break;

    case SPELL_POISON_ARROW:
        beam.name     = "poison arrow";
        beam.damage   = dice_def(3, 7 + power / 12);
        beam.colour   = LIGHTGREEN;
        beam.glyph    = dchar_glyph(DCHAR_FIRED_MISSILE);
        beam.flavour  = BEAM_POISON_ARROW;
        beam.hit      = 20 + power / 25;
        break;

    case SPELL_BOLT_OF_MAGMA:
        beam.name     = "bolt of magma";
        beam.damage   = dice_def(3, 8 + power / 11);
        beam.colour   = RED;
        beam.flavour  = BEAM_LAVA;
        beam.hit      = 17 + power / 25;
        beam.is_beam  = true;
        break;

    case SPELL_BOLT_OF_FIRE:
        beam.name     = "bolt of fire";
        beam.damage   = dice_def(3, 8 + power / 11);
        beam.colour   = RED;
        beam.flavour  = BEAM_FIRE;
        beam.hit      = 17 + power / 25;
        beam.is_beam  = true;
        break;

    case SPELL_THROW_ICICLE:
        beam.name     = "shard of ice";
        beam.damage   = dice_def(3, 8 + power / 11);
        beam.colour   = WHITE;
        beam.flavour  = BEAM_ICE;
        beam.hit      = 17 + power / 25;
        break;

    case SPELL_BOLT_OF_COLD:
        beam.name     = "bolt of cold";
        beam.damage   = dice_def(3, 8 + power / 11);
        beam.colour   = WHITE;
        beam.flavour  = BEAM_COLD;
        beam.hit      = 17 + power / 25;
        beam.is_beam  = true;
        break;

    case SPELL_BOLT_OF_INACCURACY:
        beam.name     = "narrow beam of energy";
        beam.damage   = calc_dice(12, 40 + 3 * power / 2);
        beam.colour   = YELLOW;
        beam.flavour  = BEAM_ENERGY;
        beam.hit      = 1;
        beam.is_beam  = true;
        break;

    case SPELL_PRIMAL_WAVE:
        beam.name     = "great wave of water";
        // Water attack is weaker than the pure elemental damage
        // attacks, but also less resistible.
        beam.damage   = dice_def(3, 6 + power / 12);
        beam.colour   = LIGHTBLUE;
        beam.flavour  = BEAM_WATER;
        // Huge wave of water is hard to dodge.
        beam.hit      = 20 + power / 20;
        beam.is_beam  = false;
        beam.glyph    = dchar_glyph(DCHAR_WAVY);
        break;

    case SPELL_FREEZING_CLOUD:
        beam.name     = "freezing blast";
        beam.damage   = dice_def(2, 9 + power / 11);
        beam.colour   = WHITE;
        beam.flavour  = BEAM_COLD;
        beam.hit      = 17 + power / 25;
        beam.is_beam  = true;
        beam.is_big_cloud = true;
        break;

    case SPELL_SHOCK:
        beam.name     = "zap";
        beam.damage   = dice_def(1, 8 + (power / 20));
        beam.colour   = LIGHTCYAN;
        beam.flavour  = BEAM_ELECTRICITY;
        beam.hit      = 17 + power / 20;
        beam.is_beam  = true;
        break;

    case SPELL_BLINKBOLT:
        beam.name     = "living lightning";
        beam.damage   = dice_def(2, 10 + power / 17);
        beam.colour   = LIGHTCYAN;
        beam.flavour  = BEAM_ELECTRICITY;
        beam.hit      = 16 + power / 40;
        beam.is_beam  = true;
        break;

    case SPELL_LIGHTNING_BOLT:
        beam.name     = "bolt of lightning";
        beam.damage   = dice_def(3, 10 + power / 17);
        beam.colour   = LIGHTCYAN;
        beam.flavour  = BEAM_ELECTRICITY;
        beam.hit      = 16 + power / 40;
        beam.is_beam  = true;
        break;

    case SPELL_INVISIBILITY:
    case SPELL_INVISIBILITY_OTHER:
        beam.flavour  = BEAM_INVISIBILITY;
        break;

    case SPELL_FIREBALL:
        beam.colour   = RED;
        beam.name     = "fireball";
        beam.damage   = dice_def(3, 7 + power / 10);
        beam.hit      = 40;
        beam.flavour  = BEAM_FIRE;
        beam.foe_ratio = 80;
        beam.is_explosion = true;
        break;

    case SPELL_FIRE_STORM:
        setup_fire_storm(mons, power / 2, beam);
        beam.foe_ratio = random_range(40, 55);
        break;

    case SPELL_HELLFIRE_BURST:
        beam.aux_source   = "burst of hellfire";
        beam.name         = "burst of hellfire";
        beam.ex_size      = 1;
        beam.flavour      = BEAM_HELLFIRE;
        beam.is_explosion = true;
        beam.colour       = LIGHTRED;
        beam.aux_source.clear();
        beam.is_tracer    = false;
        beam.hit          = 20;
        beam.damage       = dice_def(3, 15);
        break;

    case SPELL_HEAL_OTHER:
    case SPELL_MINOR_HEALING:
        beam.damage   = dice_def(2, mons->spell_hd(real_spell) / 2);
        beam.flavour  = BEAM_HEALING;
        beam.hit      = 25 + (power / 5);
        break;

    case SPELL_TELEPORT_SELF:
        beam.flavour    = BEAM_TELEPORT;
        beam.ench_power = 2000;
        break;

    case SPELL_TELEPORT_OTHER:
        beam.flavour  = BEAM_TELEPORT;
        beam.is_beam  = true;
        break;

    case SPELL_LEHUDIBS_CRYSTAL_SPEAR:      // was splinters
        beam.name     = "crystal spear";
        beam.damage   = dice_def(3, 16 + power / 10);
        beam.colour   = WHITE;
        beam.glyph    = dchar_glyph(DCHAR_FIRED_MISSILE);
        beam.flavour  = BEAM_MMISSILE;
        beam.hit      = 22 + power / 20;
        break;

    case SPELL_DIG:
        beam.flavour  = BEAM_DIGGING;
        beam.is_beam  = true;
        break;

    case SPELL_BOLT_OF_DRAINING:      // negative energy
        beam.name     = "bolt of negative energy";
        beam.damage   = dice_def(3, 9 + power / 13);
        beam.colour   = DARKGREY;
        beam.flavour  = BEAM_NEG;
        beam.hit      = 16 + power / 35;
        beam.is_beam  = true;
        break;

    case SPELL_ISKENDERUNS_MYSTIC_BLAST: // mystic blast
        beam.colour     = LIGHTMAGENTA;
        beam.name       = "orb of energy";
        beam.short_name = "energy";
        beam.damage     = dice_def(3, 7 + (power / 14));
        beam.hit        = 20 + (power / 20);
        beam.flavour    = BEAM_MMISSILE;
        break;

    case SPELL_DAZZLING_SPRAY:
        beam.colour     = LIGHTMAGENTA;
        beam.name       = "spray of energy";
        beam.short_name = "energy";
        beam.damage     = dice_def(3, 5 + (power / 17));
        beam.hit        = 16 + (power / 22);
        beam.flavour    = BEAM_MMISSILE;
        break;

    case SPELL_STEAM_BALL:
        beam.colour   = LIGHTGREY;
        beam.name     = "ball of steam";
        beam.damage   = dice_def(3, 7 + (power / 15));
        beam.hit      = 20 + power / 20;
        beam.flavour  = BEAM_STEAM;
        break;

    case SPELL_PAIN:
        beam.flavour    = BEAM_PAIN;
        beam.damage     = dice_def(1, 7 + (power / 20));
        beam.ench_power = max(50, 8 * mons->spell_hd(real_spell));
        beam.is_beam    = true;
        break;

    case SPELL_AGONY:
        beam.flavour    = BEAM_PAIN;
        beam.ench_power = mons->spell_hd(real_spell) * 6;
        beam.is_beam    = true;
        break;

    case SPELL_STICKY_FLAME:
        beam.hit = AUTOMATIC_HIT;
    case SPELL_STICKY_FLAME_SPLASH:
    case SPELL_STICKY_FLAME_RANGE:
        if (real_spell != SPELL_STICKY_FLAME)
            beam.hit      = 18 + power / 15;
        beam.colour   = RED;
        beam.name     = "sticky flame";
        // FIXME: splash needs to be "splash of liquid fire"
        beam.damage   = dice_def(3, 3 + power / 50);
        beam.flavour  = BEAM_FIRE;
        break;

    case SPELL_NOXIOUS_CLOUD:
        beam.name     = "noxious blast";
        beam.damage   = dice_def(1, 0);
        beam.colour   = GREEN;
        beam.flavour  = BEAM_MEPHITIC;
        beam.hit      = 18 + power / 25;
        beam.is_beam  = true;
        beam.is_big_cloud = true;
        break;

    case SPELL_POISONOUS_CLOUD:
        beam.name     = "blast of poison";
        beam.damage   = dice_def(3, 3 + power / 25);
        beam.colour   = LIGHTGREEN;
        beam.flavour  = BEAM_POISON;
        beam.hit      = 18 + power / 25;
        beam.is_beam  = true;
        beam.is_big_cloud = true;
        break;

    case SPELL_ENERGY_BOLT:        // eye of devastation
        beam.colour     = YELLOW;
        beam.name       = "bolt of energy";
        beam.short_name = "energy";
        beam.damage     = dice_def(3, 20);
        beam.hit        = 15 + power / 30;
        beam.flavour    = BEAM_DEVASTATION; // DEVASTATION is BEAM_MMISSILE
        beam.is_beam    = true;             // except it also destroys walls
        break;

    case SPELL_STING:              // sting
        beam.colour   = GREEN;
        beam.name     = "sting";
        beam.damage   = dice_def(1, 6 + power / 25);
        beam.hit      = 60;
        beam.flavour  = BEAM_POISON;
        break;

    case SPELL_IRON_SHOT:
        beam.colour   = LIGHTCYAN;
        beam.name     = "iron shot";
        beam.damage   = dice_def(3, 8 + (power / 9));
        beam.hit      = 20 + (power / 25);
        beam.glyph    = dchar_glyph(DCHAR_FIRED_MISSILE);
        beam.flavour  = BEAM_MMISSILE;   // similarly unresisted thing
        break;

    case SPELL_STONE_ARROW:
        beam.colour   = LIGHTGREY;
        beam.name     = "stone arrow";
        beam.damage   = dice_def(3, 5 + (power / 10));
        beam.hit      = 14 + power / 35;
        beam.glyph    = dchar_glyph(DCHAR_FIRED_MISSILE);
        beam.flavour  = BEAM_MMISSILE;   // similarly unresisted thing
        break;

    case SPELL_SPIT_POISON:
        beam.colour   = GREEN;
        beam.name     = "splash of poison";
        beam.damage   = dice_def(1, 4 + power / 10);
        beam.hit      = 16 + power / 20;
        beam.flavour  = BEAM_POISON;
        break;

    case SPELL_SPIT_ACID:
        beam.colour   = YELLOW;
        beam.name     = "splash of acid";
        beam.damage   = dice_def(3, 7);

        // Zotdef change: make acid splash dmg dependent on power
        // Oklob saplings pwr=48, oklobs pwr=120, acid blobs pwr=216
        //  =>             3d3        3d6            3d9
        if (crawl_state.game_is_zotdef())
            beam.damage   = dice_def(3, 2 + (power / 30));

        // Natural ability, so don't use spell_hd here
        beam.hit      = 20 + (3 * mons->get_hit_dice());
        beam.flavour  = BEAM_ACID;
        break;

    case SPELL_DISINTEGRATE:
        beam.flavour    = BEAM_DISINTEGRATION;
        beam.ench_power = 50;
        beam.damage     = dice_def(1, 30 + (power / 10));
        beam.is_beam    = true;
        break;

    case SPELL_MEPHITIC_CLOUD:
        beam.name     = "foul vapour";
        beam.damage   = dice_def(1, 0);
        beam.colour   = GREEN;
        beam.flavour  = BEAM_MEPHITIC;
        beam.hit      = 14 + power / 30;
        beam.ench_power = power; // probably meaningless
        beam.is_explosion = true;
        beam.is_big_cloud = true;
        break;

    case SPELL_MIASMA_BREATH:      // death drake
        beam.name     = "foul vapour";
        beam.damage   = dice_def(3, 5 + power / 24);
        beam.colour   = DARKGREY;
        beam.flavour  = BEAM_MIASMA;
        beam.hit      = 17 + power / 20;
        beam.is_beam  = true;
        beam.is_big_cloud = true;
        break;

    case SPELL_QUICKSILVER_BOLT:   // Quicksilver dragon and purple draconian
        beam.colour     = random_colour();
        beam.name       = "bolt of dispelling energy";
        beam.short_name = "energy";
        beam.damage     = dice_def(3, 20);
        beam.hit        = 16 + power / 25;
        beam.flavour    = BEAM_MMISSILE;
        break;

    case SPELL_HELLFIRE:           // fiend's hellfire
        beam.name         = "blast of hellfire";
        beam.aux_source   = "blast of hellfire";
        beam.colour       = LIGHTRED;
        beam.damage       = dice_def(3, 20);
        beam.hit          = 24;
        beam.flavour      = BEAM_HELLFIRE;
        beam.is_beam      = true;
        beam.is_explosion = true;
        break;

    case SPELL_METAL_SPLINTERS:
        beam.name       = "spray of metal splinters";
        beam.short_name = "metal splinters";
        beam.damage     = dice_def(3, 20 + power / 20);
        beam.colour     = CYAN;
        beam.flavour    = BEAM_FRAG;
        beam.hit        = 19 + power / 30;
        beam.is_beam    = true;
        break;

    case SPELL_BANISHMENT:
        beam.flavour  = BEAM_BANISH;
        beam.is_beam  = true;
        break;

    case SPELL_BLINK_OTHER:
        beam.flavour    = BEAM_BLINK;
        beam.is_beam    = true;
        break;

    case SPELL_BLINK_OTHER_CLOSE:
        beam.flavour    = BEAM_BLINK_CLOSE;
        beam.is_beam    = true;
        break;

    case SPELL_FIRE_BREATH:
        beam.name       = "blast of flame";
        beam.aux_source = "blast of fiery breath";
        beam.short_name = "flames";
        beam.damage     = dice_def(3, (mons->get_hit_dice() * 2));
        beam.colour     = RED;
        beam.hit        = 30;
        beam.flavour    = BEAM_FIRE;
        beam.is_beam    = true;
        if (mons_genus(mons->type) == MONS_DRACONIAN
            && draco_or_demonspawn_subspecies(mons) == MONS_RED_DRACONIAN)
        {
            beam.name        = "searing blast";
            beam.aux_source  = "blast of searing breath";
            beam.damage.size = 65 * beam.damage.size / 100;
        }
        break;

    case SPELL_CHAOS_BREATH:
        beam.name         = "blast of chaos";
        beam.aux_source   = "blast of chaotic breath";
        beam.damage       = dice_def(1, 3 * mons->get_hit_dice() / 2);
        beam.colour       = ETC_RANDOM;
        beam.hit          = 30;
        beam.flavour      = BEAM_CHAOS;
        beam.is_beam      = true;
        beam.is_big_cloud = true;
        break;

    case SPELL_COLD_BREATH:
        beam.name       = "blast of cold";
        beam.aux_source = "blast of icy breath";
        beam.short_name = "frost";
        beam.damage     = dice_def(3, (mons->get_hit_dice() * 2));
        beam.colour     = WHITE;
        beam.hit        = 30;
        beam.flavour    = BEAM_COLD;
        beam.is_beam    = true;
        if (mons_genus(mons->type) == MONS_DRACONIAN
            && draco_or_demonspawn_subspecies(mons) == MONS_WHITE_DRACONIAN)
        {
            beam.name        = "chilling blast";
            beam.aux_source  = "blast of chilling breath";
            beam.short_name  = "frost";
            beam.damage.size = 65 * beam.damage.size / 100;
            beam.ac_rule     = AC_NONE;
        }
        break;

    case SPELL_HOLY_BREATH:
        beam.name     = "blast of cleansing flame";
        beam.damage   = dice_def(3, (mons->get_hit_dice() * 2));
        beam.colour   = ETC_HOLY;
        beam.flavour  = BEAM_HOLY;
        beam.hit      = 18 + power / 25;
        beam.is_beam  = true;
        beam.is_big_cloud = true;
        break;

    case SPELL_PORKALATOR:
        beam.name     = "porkalator";
        beam.glyph    = 0;
        beam.flavour  = BEAM_PORKALATOR;
        beam.thrower  = KILL_MON_MISSILE;
        beam.is_beam  = true;
        break;

    case SPELL_IOOD:                  // tracer only
    case SPELL_LRD:                   // for noise generation purposes
    case SPELL_PORTAL_PROJECTILE:     // ditto
    case SPELL_GLACIATE:              // ditto
    case SPELL_CLOUD_CONE:            // ditto
        beam.flavour  = BEAM_DEVASTATION;
        beam.is_beam  = true;
        // Doesn't take distance into account, but this is just a tracer so
        // we'll ignore that.  We need some damage on the tracer so the monster
        // doesn't think the spell is useless against other monsters.
        beam.damage   = dice_def(42, 1);
        beam.range    = LOS_RADIUS;
        break;

    case SPELL_PETRIFYING_CLOUD:
        beam.name     = "blast of calcifying dust";
        beam.colour   = WHITE;
        beam.damage   = dice_def(2, 6);
        beam.hit      = AUTOMATIC_HIT;
        beam.flavour  = BEAM_PETRIFYING_CLOUD;
        beam.foe_ratio = 30;
        beam.is_beam  = true;
        break;

#if TAG_MAJOR_VERSION == 34
    case SPELL_HOLY_LIGHT:
    case SPELL_SILVER_BLAST:
        beam.name     = "beam of golden light";
        beam.damage   = dice_def(3, 8 + power / 11);
        beam.colour   = ETC_HOLY;
        beam.flavour  = BEAM_HOLY;
        beam.hit      = 17 + power / 25;
        beam.is_beam  = true;
        break;
#endif

    case SPELL_ENSNARE:
        beam.name     = "stream of webbing";
        beam.colour   = WHITE;
        beam.glyph    = dchar_glyph(DCHAR_FIRED_MISSILE);
        beam.flavour  = BEAM_ENSNARE;
        beam.hit      = 22 + power / 20;
        break;

    case SPELL_FORCE_LANCE:
        beam.colour   = LIGHTGREY;
        beam.name     = "lance of force";
        beam.damage   = dice_def(3, 6 + (power / 15));
        beam.hit      = 20 + power / 20;
        beam.flavour  = BEAM_MMISSILE;
        break;

    case SPELL_SENTINEL_MARK:
        beam.ench_power = 125; //Difficult to resist
        beam.flavour    = BEAM_SENTINEL_MARK;
        beam.is_beam    = true;
        break;

    case SPELL_GHOSTLY_FLAMES:
        beam.name     = "ghostly flame";
        beam.damage   = dice_def(0, 1);
        beam.colour   = CYAN;
        beam.flavour  = BEAM_GHOSTLY_FLAME;
        beam.hit      = AUTOMATIC_HIT;
        beam.is_beam  = true;
        beam.is_big_cloud = true;
        break;

    case SPELL_GHOSTLY_FIREBALL:
        beam.colour   = CYAN;
        beam.name     = "ghostly fireball";
        beam.damage   = dice_def(3, 7 + power / 13);
        beam.hit      = 40;
        beam.flavour  = BEAM_GHOSTLY_FLAME;
        beam.is_explosion = true;
        break;

    case SPELL_DIMENSION_ANCHOR:
        beam.ench_power = power / 2;
        beam.flavour    = BEAM_DIMENSION_ANCHOR;
        beam.is_beam    = true;
        break;

    case SPELL_THORN_VOLLEY:
        beam.colour   = BROWN;
        beam.name     = "volley of thorns";
        beam.damage   = dice_def(3, 5 + (power / 13));
        beam.hit      = 20 + power / 15;
        beam.flavour  = BEAM_MMISSILE;
        break;

    case SPELL_STRIP_RESISTANCE:
        beam.ench_power = mons->spell_hd(real_spell) * 6;
        beam.flavour    = BEAM_VULNERABILITY;
        beam.is_beam    = true;
        break;

    // XXX: This seems needed to give proper spellcasting messages, even though
    //      damage is done via another means
    case SPELL_FREEZE:
        beam.flavour    = BEAM_COLD;
        break;

    case SPELL_MALIGN_OFFERING:
        beam.flavour    = BEAM_MALIGN_OFFERING;
        beam.damage     = dice_def(2, 7 + (power / 13));
        beam.is_beam    = true;
        break;

    case SPELL_VIRULENCE:
        beam.flavour    = BEAM_VIRULENCE;
        beam.ench_power = mons->spell_hd(real_spell) * 6;
        beam.is_beam    = true;
        break;

    case SPELL_IGNITE_POISON_SINGLE:
        beam.flavour    = BEAM_IGNITE_POISON;
        beam.ench_power = mons->spell_hd(real_spell) * 12;
        beam.is_beam    = true;
        break;

    case SPELL_ORB_OF_ELECTRICITY:
        beam.name           = "orb of electricity";
        beam.colour         = LIGHTBLUE;
        beam.damage         = dice_def(3, 7 + power / 9);
        beam.hit            = 40;
        beam.flavour        = BEAM_ELECTRICITY;
        beam.is_explosion   = true;
        beam.foe_ratio      = random_range(40, 55);
        break;

    case SPELL_EXPLOSIVE_BOLT:
        beam.name       = "explosive bolt";
        beam.damage     = dice_def(1, 0); // deals damage through explosions
        beam.colour     = RED;
        beam.flavour    = BEAM_FIRE;
        beam.hit        = 17 + power / 25;
        beam.ench_power = power;
        beam.is_beam    = true;
        break;

    case SPELL_FLASH_FREEZE:
        beam.name     = "flash freeze";
        beam.damage   = dice_def(3, 9 + (power / 13));
        beam.colour   = WHITE;
        beam.flavour  = BEAM_ICE;
        beam.hit      = AUTOMATIC_HIT;
        break;

    case SPELL_SAP_MAGIC:
        beam.ench_power = mons->spell_hd(real_spell) * 10;
        beam.flavour    = BEAM_SAP_MAGIC;
        beam.is_beam    = true;
        break;

    case SPELL_CORRUPT_BODY:
        beam.flavour    = BEAM_CORRUPT_BODY;
        beam.is_beam    = true;
        break;

    case SPELL_SHADOW_BOLT:
        beam.name     = "shadow bolt";
        beam.is_beam  = true;
        // deliberate fall-through
    case SPELL_SHADOW_SHARD:
        if (real_spell == SPELL_SHADOW_SHARD)
            beam.name  = "shadow shard";
        beam.damage   = dice_def(3, 8 + power / 11);
        beam.colour   = MAGENTA;
        beam.flavour  = BEAM_MMISSILE;
        beam.hit      = 17 + power / 25;
        break;

    case SPELL_CRYSTAL_BOLT:
        beam.name     = "crystal bolt";
        beam.damage   = dice_def(3, 8 + power / 11);
        beam.colour   = GREEN;
        beam.flavour  = BEAM_CRYSTAL;
        beam.hit      = 17 + power / 25;
        beam.is_beam  = true;
        break;

    case SPELL_DRAIN_MAGIC:
        beam.ench_power = mons->spell_hd(real_spell) * 6;
        beam.flavour    = BEAM_DRAIN_MAGIC;
        break;

    case SPELL_CORROSIVE_BOLT:
        beam.colour   = YELLOW;
        beam.name     = "bolt of acid";
        beam.damage   = dice_def(3, 9 + power / 17);
        beam.flavour  = BEAM_ACID;
        beam.hit      = 17 + power / 25;
        beam.is_beam  = true;
        break;

    case SPELL_SPIT_LAVA:
        beam.name        = "glob of lava";
        beam.damage      = dice_def(3, 10);
        beam.hit         = 20;
        beam.colour      = RED;
        beam.glyph       = dchar_glyph(DCHAR_FIRED_ZAP);
        beam.flavour     = BEAM_LAVA;
        break;

    case SPELL_ELECTRICAL_BOLT:
        beam.name        = "bolt of electricity";
        beam.damage      = dice_def(3, 3 + mons->get_hit_dice());
        beam.hit         = 35;
        beam.colour      = LIGHTCYAN;
        beam.glyph       = dchar_glyph(DCHAR_FIRED_ZAP);
        beam.flavour     = BEAM_ELECTRICITY;
        break;

    case SPELL_FLAMING_CLOUD:
        beam.name         = "blast of flame";
        beam.aux_source   = "blast of fiery breath";
        beam.short_name   = "flames";
        beam.damage       = dice_def(1, 3 * mons->get_hit_dice() / 2);
        beam.colour       = RED;
        beam.hit          = 30;
        beam.flavour      = BEAM_FIRE;
        beam.is_beam      = true;
        beam.is_big_cloud = true;
        break;

    case SPELL_THROW_BARBS:
        beam.name        = "volley of spikes";
        beam.aux_source  = "volley of spikes";
        beam.hit_verb    = "skewers";
        beam.hit         = 27;
        beam.damage      = dice_def(2, 13);
        beam.glyph       = dchar_glyph(DCHAR_FIRED_MISSILE);
        beam.colour      = LIGHTGREY;
        beam.flavour     = BEAM_MISSILE;
        break;

    case SPELL_DEATH_RATTLE:
        beam.name     = "vile air";
        beam.colour   = DARKGREY;
        beam.damage   = dice_def(2, 4);
        beam.hit      = AUTOMATIC_HIT;
        beam.flavour  = BEAM_DEATH_RATTLE;
        beam.foe_ratio = 30;
        beam.is_beam  = true;
        break;

    default:
        if (check_validity)
        {
            beam.flavour = NUM_BEAMS;
            return beam;
        }

        if (!is_valid_spell(real_spell))
        {
            die("Invalid spell #%d cast by %s", (int) real_spell,
                     mons->name(DESC_PLAIN, true).c_str());
        }

        die("Unknown monster spell '%s' cast by %s",
                 spell_title(real_spell),
                 mons->name(DESC_PLAIN, true).c_str());
    }

    if (beam.is_enchantment())
    {
        beam.glyph = 0;
        beam.name = "";
    }

    if (spell_cast == SPELL_AGONY)
        beam.name = "agony";

    return beam;
}

static bool _los_free_spell(spell_type spell_cast)
{
    return spell_cast == SPELL_HELLFIRE_BURST
       || spell_cast == SPELL_BRAIN_FEED
       || spell_cast == SPELL_SMITING
       || spell_cast == SPELL_HAUNT
       || spell_cast == SPELL_FIRE_STORM
       || spell_cast == SPELL_AIRSTRIKE
       || spell_cast == SPELL_WATERSTRIKE
       || spell_cast == SPELL_HOLY_FLAMES
       || spell_cast == SPELL_SUMMON_SPECTRAL_ORCS
       || spell_cast == SPELL_CHAOTIC_MIRROR
       || spell_cast == SPELL_FLAY
       || spell_cast == SPELL_PARALYSIS_GAZE
       || spell_cast == SPELL_CONFUSION_GAZE
       || spell_cast == SPELL_DRAINING_GAZE;
}

// Set up bolt structure for monster spell casting.
bool setup_mons_cast(monster* mons, bolt &pbolt, spell_type spell_cast,
                     bool check_validity)
{
    // always set these -- used by things other than fire_beam()

    // [ds] Used to be 12 * MHD and later buggily forced to -1 downstairs.
    // Setting this to a more realistic number now that that bug is
    // squashed.
    pbolt.ench_power = 4 * mons->spell_hd(spell_cast);
    pbolt.source_id = mons->mid;

    // Convenience for the hapless innocent who assumes that this
    // damn function does all possible setup. [ds]
    if (pbolt.target.origin())
        pbolt.target = mons->target;

    // Set bolt type and range.
    if (_los_free_spell(spell_cast))
    {
        pbolt.range = 0;
        pbolt.glyph = 0;
        switch (spell_cast)
        {
        case SPELL_BRAIN_FEED:
        case SPELL_SMITING:
        case SPELL_AIRSTRIKE:
        case SPELL_WATERSTRIKE:
        case SPELL_HOLY_FLAMES:
        case SPELL_CHAOTIC_MIRROR:
        case SPELL_FLAY:
        case SPELL_PARALYSIS_GAZE:
        case SPELL_CONFUSION_GAZE:
        case SPELL_DRAINING_GAZE:
            return true;
        default:
            // Other spells get normal setup:
            break;
        }
    }

    // The below are no-ops since they don't involve direct_effect,
    // fire_tracer, or beam.
    switch (spell_cast)
    {
    case SPELL_STICKS_TO_SNAKES:
    case SPELL_SUMMON_SMALL_MAMMAL:
    case SPELL_VAMPIRIC_DRAINING:
    case SPELL_INJURY_MIRROR:
    case SPELL_MAJOR_HEALING:
#if TAG_MAJOR_VERSION == 34
    case SPELL_VAMPIRE_SUMMON:
#endif
    case SPELL_SHADOW_CREATURES:       // summon anything appropriate for level
    case SPELL_WEAVE_SHADOWS:
#if TAG_MAJOR_VERSION == 34
    case SPELL_FAKE_RAKSHASA_SUMMON:
#endif
    case SPELL_FAKE_MARA_SUMMON:
    case SPELL_SUMMON_ILLUSION:
    case SPELL_SUMMON_DEMON:
    case SPELL_MONSTROUS_MENAGERIE:
    case SPELL_ANIMATE_DEAD:
    case SPELL_TWISTED_RESURRECTION:
    case SPELL_SIMULACRUM:
    case SPELL_CALL_IMP:
    case SPELL_SUMMON_MINOR_DEMON:
#if TAG_MAJOR_VERSION == 34
    case SPELL_SUMMON_SCORPIONS:
#endif
    case SPELL_SUMMON_SWARM:
    case SPELL_SUMMON_UFETUBUS:
    case SPELL_SUMMON_HELL_BEAST:  // Geryon
    case SPELL_SUMMON_UNDEAD:
    case SPELL_SUMMON_ICE_BEAST:
    case SPELL_SUMMON_MUSHROOMS:
    case SPELL_CONJURE_BALL_LIGHTNING:
    case SPELL_SUMMON_DRAKES:
    case SPELL_SUMMON_HORRIBLE_THINGS:
    case SPELL_MALIGN_GATEWAY:
    case SPELL_HAUNT:
    case SPELL_SYMBOL_OF_TORMENT:
    case SPELL_CAUSE_FEAR:
    case SPELL_MESMERISE:
    case SPELL_DRAIN_LIFE:
    case SPELL_SUMMON_GREATER_DEMON:
    case SPELL_CANTRIP:
    case SPELL_BROTHERS_IN_ARMS:
    case SPELL_BERSERKER_RAGE:
    case SPELL_TROGS_HAND:
    case SPELL_SWIFTNESS:
    case SPELL_STONESKIN:
    case SPELL_WATER_ELEMENTALS:
    case SPELL_FIRE_ELEMENTALS:
    case SPELL_AIR_ELEMENTALS:
    case SPELL_EARTH_ELEMENTALS:
    case SPELL_IRON_ELEMENTALS:
#if TAG_MAJOR_VERSION == 34
    case SPELL_SUMMON_ELEMENTAL:
#endif
    case SPELL_CREATE_TENTACLES:
    case SPELL_BLINK:
    case SPELL_CONTROLLED_BLINK:
    case SPELL_BLINK_RANGE:
    case SPELL_BLINK_AWAY:
    case SPELL_BLINK_CLOSE:
    case SPELL_TOMB_OF_DOROKLOHE:
    case SPELL_CHAIN_LIGHTNING:    // the only user is reckless
    case SPELL_SUMMON_EYEBALLS:
    case SPELL_SUMMON_BUTTERFLIES:
#if TAG_MAJOR_VERSION == 34
    case SPELL_MISLEAD:
#endif
    case SPELL_CALL_TIDE:
    case SPELL_INK_CLOUD:
    case SPELL_SILENCE:
    case SPELL_AWAKEN_FOREST:
    case SPELL_DRUIDS_CALL:
    case SPELL_SUMMON_SPECTRAL_ORCS:
    case SPELL_REGENERATION:
    case SPELL_CORPSE_ROT:
    case SPELL_LEDAS_LIQUEFACTION:
    case SPELL_SUMMON_DRAGON:
    case SPELL_SUMMON_HYDRA:
    case SPELL_FIRE_SUMMON:
    case SPELL_DEATHS_DOOR:
    case SPELL_OZOCUBUS_ARMOUR:
    case SPELL_OZOCUBUS_REFRIGERATION:
    case SPELL_OLGREBS_TOXIC_RADIANCE:
    case SPELL_SHATTER:
    case SPELL_FRENZY:
#if TAG_MAJOR_VERSION == 34
    case SPELL_SUMMON_TWISTER:
#endif
    case SPELL_BATTLESPHERE:
    case SPELL_SPECTRAL_WEAPON:
    case SPELL_WORD_OF_RECALL:
    case SPELL_INJURY_BOND:
    case SPELL_CALL_LOST_SOUL:
    case SPELL_BLINK_ALLIES_ENCIRCLE:
    case SPELL_MASS_CONFUSION:
    case SPELL_ENGLACIATION:
#if TAG_MAJOR_VERSION == 34
    case SPELL_SHAFT_SELF:
#endif
    case SPELL_AWAKEN_VINES:
    case SPELL_CONTROL_WINDS:
    case SPELL_WALL_OF_BRAMBLES:
    case SPELL_HASTE_PLANTS:
    case SPELL_WIND_BLAST:
    case SPELL_SUMMON_VERMIN:
    case SPELL_TORNADO:
    case SPELL_DISCHARGE:
    case SPELL_IGNITE_POISON:
    case SPELL_EPHEMERAL_INFUSION:
    case SPELL_FORCEFUL_INVITATION:
    case SPELL_PLANEREND:
    case SPELL_CHAIN_OF_CHAOS:
    case SPELL_BLACK_MARK:
    case SPELL_GRAND_AVATAR:
#if TAG_MAJOR_VERSION == 34
    case SPELL_REARRANGE_PIECES:
#endif
    case SPELL_BLINK_ALLIES_AWAY:
    case SPELL_SHROUD_OF_GOLUBRIA:
    case SPELL_PHANTOM_MIRROR:
    case SPELL_SUMMON_MANA_VIPER:
    case SPELL_SUMMON_EMPEROR_SCORPIONS:
    case SPELL_BATTLECRY:
    case SPELL_SIGNAL_HORN:
    case SPELL_SEAL_DOORS:
    case SPELL_BERSERK_OTHER:
    case SPELL_SPELLFORGED_SERVITOR:
    case SPELL_TENTACLE_THROW:
    case SPELL_CORRUPTING_PULSE:
    case SPELL_SIREN_SONG:
    case SPELL_AVATAR_SONG:
    case SPELL_REPEL_MISSILES:
    case SPELL_DEFLECT_MISSILES:
    case SPELL_SUMMON_SCARABS:
    case SPELL_HUNTING_CRY:
        return true;
    default:
        if (check_validity)
        {
            bolt beam = mons_spell_beam(mons, spell_cast, 1, true);
            return beam.flavour != NUM_BEAMS;
        }
        break;
    }

    int power = 12 * mons->spell_hd(spell_cast);

    bolt theBeam = mons_spell_beam(mons, spell_cast, power);

    if (spell_cast == SPELL_LEGENDARY_DESTRUCTION)
    {
        int range = spell_range(theBeam.origin_spell,
                                12 * mons->spell_hd(theBeam.origin_spell),
                                false);
        while (distance2(mons->pos(), mons->target) > dist_range(range))
        {
            theBeam = mons_spell_beam(mons, spell_cast, power);
            range = spell_range(theBeam.origin_spell,
                                12 * mons->spell_hd(theBeam.origin_spell),
                                false);
        }
    }
    bolt_parent_init(&theBeam, &pbolt);
    pbolt.source = mons->pos();
    pbolt.is_tracer = false;
    if (!pbolt.is_enchantment())
        pbolt.aux_source = pbolt.name;
    else
        pbolt.aux_source.clear();

    // Your shadow can target these spells at other monsters;
    // other monsters can't.
    if (mons->mid != MID_PLAYER)
    {
        if (spell_cast == SPELL_HASTE
            || spell_cast == SPELL_MIGHT
            || spell_cast == SPELL_INVISIBILITY
            || spell_cast == SPELL_MINOR_HEALING
            || spell_cast == SPELL_TELEPORT_SELF
            || spell_cast == SPELL_SILENCE
            || spell_cast == SPELL_FRENZY)
        {
            pbolt.target = mons->pos();
        }
        else if (spell_cast == SPELL_LRD)
        {
            pbolt.target = _mons_fragment_target(mons);
            pbolt.aimed_at_spot = true; // to get noise to work properly
        }
     }

    return true;
}

// Function should return false if friendlies shouldn't animate any dead.
// Currently, this only happens if the player is in the middle of butchering
// a corpse (infuriating), or if they are less than satiated.  Only applies
// to friendly corpse animators. {due}
static bool _animate_dead_okay(spell_type spell)
{
    // It's always okay in the arena.
    if (crawl_state.game_is_arena())
        return true;

    if (is_butchering() || is_vampire_feeding())
        return false;

    if (you.hunger_state < HS_SATIATED && you.mutation[MUT_HERBIVOROUS] < 3)
        return false;

    if (god_hates_spell(spell, you.religion))
        return false;

    return true;
}

// Returns true if the spell is something you wouldn't want done if
// you had a friendly target...  only returns a meaningful value for
// non-beam spells.
static bool _ms_direct_nasty(spell_type monspell)
{
    return !(get_spell_flags(monspell) & SPFLAG_UTILITY
             || spell_typematch(monspell, SPTYP_SUMMONING));
}

// Can be affected by the 'Haste Plants' spell
static bool _is_hastable_plant(const monster* mons)
{
    return mons->holiness() == MH_PLANT
           && !mons_is_firewood(mons)
           && mons->type != MONS_SNAPLASHER_VINE
           && mons->type != MONS_SNAPLASHER_VINE_SEGMENT;
}

// Checks if the foe *appears* to be immune to negative energy.  We
// can't just use foe->res_negative_energy(), because that'll mean
// monsters will just "know" whether a player is fully life-protected.
static bool _foe_should_res_negative_energy(const actor* foe)
{
    if (foe->is_player())
    {
        switch (you.undead_state())
        {
        case US_ALIVE:
            // Demonspawn are not demons, and statue form grants only
            // partial resistance.
            return false;
        case US_SEMI_UNDEAD:
            // Non-bloodless vampires do not appear immune.
            return you.hunger_state == HS_STARVING;
        default:
            return true;
        }
    }

    return foe->holiness() != MH_NATURAL;
}

static bool _valid_blink_ally(const monster* caster, const monster* target)
{
    return mons_aligned(caster, target) && caster != target
           && !target->no_tele(true, false, true);
}

static bool _valid_encircle_ally(const monster* caster, const monster* target,
                                 const coord_def foepos)
{
    return _valid_blink_ally(caster, target)
           && target->pos().distance_from(foepos) > 1;
}

static bool _valid_blink_away_ally(const monster* caster, const monster* target,
                                   const coord_def foepos)
{
    return _valid_blink_ally(caster, target)
           && target->pos().distance_from(foepos) < 3;
}

static bool _valid_druids_call_target(const monster* caller, const monster* callee)
{
    return mons_aligned(caller, callee) && mons_is_beast(callee->type)
           && callee->get_experience_level() <= 20
           && !callee->is_shapeshifter()
           && !caller->see_cell(callee->pos())
           && mons_habitat(callee) != HT_WATER
           && mons_habitat(callee) != HT_LAVA
           && !callee->is_travelling();
}

static bool _mirrorable(const monster* agent, const monster* mon)
{
    return mon != agent
           && mons_aligned(mon, agent)
           && !mon->is_stationary()
           && !mon->is_summoned()
           && !mons_is_conjured(mon->type)
           && !mons_is_unique(mon->type);
}

enum battlecry_type
{
    BATTLECRY_ORC,
    BATTLECRY_HOLY,
    BATTLECRY_NATURAL,
    NUM_BATTLECRIES
};

static bool _is_battlecry_compatible(monster* mons, battlecry_type type)
{
    switch (type)
    {
        case BATTLECRY_ORC:
            return mons_genus(mons->type) == MONS_ORC;
        case BATTLECRY_HOLY:
            return mons->holiness() == MH_HOLY;
        case BATTLECRY_NATURAL:
            return mons->holiness() == MH_NATURAL;
        default:
            return false;
    }
}

static int _battle_cry(const monster* chief, bool check_only = false)
{
    battlecry_type type =
        (mons_genus(chief->type) == MONS_ORC) ? BATTLECRY_ORC :
        (chief->holiness() == MH_HOLY)        ? BATTLECRY_HOLY
                                              : BATTLECRY_NATURAL;
    const actor *foe = chief->get_foe();
    int affected = 0;

    enchant_type battlecry = (type == BATTLECRY_ORC ? ENCH_BATTLE_FRENZY
                                                    : ENCH_ROUSED);

    // Columns 0 and 1 should have one instance of %s (for the monster),
    // column 2 two (for a determiner and the monsters), and column 3 none.
    enum { AFFECT_ONE, AFFECT_MANY, GENERIC_ALLIES };
    static const char * const messages[][3] =
    {
        {
            "%s goes into a battle-frenzy!",
            "%s %s go into a battle-frenzy!",
            "orcs"
        },
        {
            "%s is roused by the hymn!",
            "%s %s are roused to righteous anger!",
            "holy creatures"
        },
        {
            "%s is stirred to greatness!",
            "%s %s are stirred to greatness!",
            "satyr's allies"
        },
    };
    COMPILE_CHECK(ARRAYSZ(messages) == NUM_BATTLECRIES);

    if (foe
        && (!foe->is_player() || !chief->friendly())
        && !silenced(chief->pos())
        && !chief->has_ench(ENCH_MUTE)
        && chief->can_see(foe))
    {
        const int level = chief->get_hit_dice() > 12? 2 : 1;
        vector<monster* > seen_affected;
        for (monster_near_iterator mi(chief, LOS_NO_TRANS); mi; ++mi)
        {
            if (*mi != chief
                && _is_battlecry_compatible(*mi, type)
                && mons_aligned(chief, *mi)
                && !mi->berserk_or_insane()
                && !mi->has_ench(ENCH_MIGHT)
                && !mi->cannot_move()
                && !mi->confused()
                && (mi->get_hit_dice() < chief->get_hit_dice()
                    || type == BATTLECRY_HOLY))
            {
                mon_enchant ench = mi->get_ench(battlecry);
                if (ench.ench == ENCH_NONE || ench.degree < level)
                {
                    if (check_only)
                        return 1; // just need to check

                    const int dur =
                        random_range(12, 20) * speed_to_duration(mi->speed);

                    if (ench.ench != ENCH_NONE)
                    {
                        ench.degree   = level;
                        ench.duration = max(ench.duration, dur);
                        mi->update_ench(ench);
                    }
                    else
                        mi->add_ench(mon_enchant(battlecry, level, chief, dur));

                    affected++;
                    if (you.can_see(*mi))
                        seen_affected.push_back(*mi);

                    if (mi->asleep())
                        behaviour_event(*mi, ME_DISTURB, 0, chief->pos());
                }
            }
        }

        if (affected)
        {
            // The yell happens whether you happen to see it or not.
            noisy(LOS_RADIUS, chief->pos(), chief->mid);

            // Disabling detailed frenzy announcement because it's so spammy.
            const msg_channel_type channel =
                        chief->friendly() ? MSGCH_MONSTER_ENCHANT
                                          : MSGCH_FRIEND_ENCHANT;

            if (!seen_affected.empty())
            {
                string who;
                if (seen_affected.size() == 1)
                {
                    who = seen_affected[0]->name(DESC_THE);
                    mprf(channel, messages[type][AFFECT_ONE], who.c_str());
                }
                else
                {
                    bool generic = false;
                    monster_type mon_type = seen_affected[0]->type;
                    for (unsigned int i = 0; i < seen_affected.size(); i++)
                    {
                        if (seen_affected[i]->type != mon_type)
                        {
                            // not homogeneous - use the generic term instead
                            generic = true;
                            break;
                        }
                    }
                    who = get_monster_data(mon_type)->name;

                    mprf(channel, messages[type][AFFECT_MANY],
                         chief->friendly() ? "Your" : "The",
                         (!generic ? pluralise(who).c_str()
                                   : messages[type][GENERIC_ALLIES]));
                }
            }
        }
    }

    return affected;
}

static void _set_door(set<coord_def> door, dungeon_feature_type feat)
{
    for (set<coord_def>::const_iterator i = door.begin();
         i != door.end(); ++i)
    {
        grd(*i) = feat;
        set_terrain_changed(*i);
    }
}

static bool _can_force_door_shut(const coord_def& door)
{
    if (grd(door) != DNGN_OPEN_DOOR)
        return false;

    set<coord_def> all_door;
    vector<coord_def> veto_spots;
    find_connected_identical(door, all_door);
    copy(all_door.begin(), all_door.end(), back_inserter(veto_spots));

    for (set<coord_def>::const_iterator i = all_door.begin();
         i != all_door.end(); ++i)
    {
        // Only attempt to push players and non-hostile monsters out of
        // doorways
        actor* act = actor_at(*i);
        if (act)
        {
            if (act->is_player()
                || act->is_monster()
                    && act->as_monster()->attitude != ATT_HOSTILE)
            {
                coord_def newpos;
                if (!get_push_space(*i, newpos, act, true, &veto_spots))
                    return false;
                else
                    veto_spots.push_back(newpos);
            }
            else
                return false;
        }
        // If there are items in the way, see if there's room to push them
        // out of the way
        else if (igrd(*i) != NON_ITEM)
        {
            if (!has_push_space(*i, 0))
                return false;
        }
    }

    // Didn't find any items we couldn't displace
    return true;
}

static bool _should_force_door_shut(const coord_def& door)
{
    if (grd(door) != DNGN_OPEN_DOOR)
        return false;

    dungeon_feature_type old_feat = grd(door);
    int cur_tension = get_tension(GOD_NO_GOD);

    set<coord_def> all_door;
    vector<coord_def> veto_spots;
    find_connected_identical(door, all_door);
    copy(all_door.begin(), all_door.end(), back_inserter(veto_spots));

    bool player_in_door = false;
    for (set<coord_def>::const_iterator i = all_door.begin();
        i != all_door.end(); ++i)
    {
        if (you.pos() == *i)
        {
            player_in_door = true;
            break;
        }
    }

    int new_tension;
    if (player_in_door)
    {
        coord_def newpos;
        coord_def oldpos = you.pos();
        get_push_space(oldpos, newpos, &you, false, &veto_spots);
        you.move_to_pos(newpos);
        _set_door(all_door, DNGN_CLOSED_DOOR);
        new_tension = get_tension(GOD_NO_GOD);
        _set_door(all_door, old_feat);
        you.move_to_pos(oldpos);
    }
    else
    {
        _set_door(all_door, DNGN_CLOSED_DOOR);
        new_tension = get_tension(GOD_NO_GOD);
        _set_door(all_door, old_feat);
    }

    // If closing the door would reduce player tension by too much, probably
    // it is scarier for the player to leave it open and thus it should be left
    // open

    // Currently won't allow tension to be lowered by more than 33%
    return ((cur_tension - new_tension) * 3) <= cur_tension;
}

// Find an adjacent space to displace a stack of items or a creature
// (If act is null, we are just moving items and not an actor)
bool get_push_space(const coord_def& pos, coord_def& newpos, actor* act,
                    bool ignore_tension, const vector<coord_def>* excluded)
{
    if (act && act->is_stationary())
        return false;

    int max_tension = -1;
    coord_def best_spot(-1, -1);
    bool can_push = false;
    for (adjacent_iterator ai(pos); ai; ++ai)
    {
        dungeon_feature_type feat = grd(*ai);
        if (feat_has_solid_floor(feat))
        {
            // Extra checks if we're moving a monster instead of an item
            if (act)
            {
                if (actor_at(*ai)
                    || !act->can_pass_through(*ai)
                    || !act->is_habitable(*ai))
                {
                    continue;
                }

                bool spot_vetoed = false;
                if (excluded)
                {
                    for (unsigned int i = 0; i < excluded->size(); ++i)
                        if (excluded->at(i) == *ai)
                        {
                            spot_vetoed = true;
                            break;
                        }
                }
                if (spot_vetoed)
                    continue;

                // If we don't care about tension, first valid spot is acceptable
                if (ignore_tension)
                {
                    newpos = *ai;
                    return true;
                }
                else // Calculate tension with monster at new location
                {
                    set<coord_def> all_door;
                    find_connected_identical(pos, all_door);
                    dungeon_feature_type old_feat = grd(pos);

                    act->move_to_pos(*ai);
                    _set_door(all_door, DNGN_CLOSED_DOOR);
                    int new_tension = get_tension(GOD_NO_GOD);
                    _set_door(all_door, old_feat);
                    act->move_to_pos(pos);

                    if (new_tension > max_tension)
                    {
                        max_tension = new_tension;
                        best_spot = *ai;
                        can_push = true;
                    }
                }
            }
            else //If we're not moving a creature, the first open spot is enough
            {
                newpos = *ai;
                return true;
            }
        }
    }

    if (can_push)
        newpos = best_spot;
    return can_push;
}

bool has_push_space(const coord_def& pos, actor* act,
                    const vector<coord_def>* excluded)
{
    coord_def dummy(-1, -1);
    return get_push_space(pos, dummy, act, true, excluded);
}

static bool _seal_doors_and_stairs(const monster* warden,
                                   bool check_only = false)
{
    ASSERT(warden);

    int num_closed = 0;
    int seal_duration = 80 + random2(80);
    bool player_pushed = false;
    bool had_effect = false;

    for (radius_iterator ri(you.pos(), LOS_RADIUS, C_ROUND);
                 ri; ++ri)
    {
        if (grd(*ri) == DNGN_OPEN_DOOR)
        {
            if (!_can_force_door_shut(*ri))
                continue;

            // If it's scarier to leave this door open, do so
            if (!_should_force_door_shut(*ri))
                continue;

            if (check_only)
                return true;

            set<coord_def> all_door;
            vector<coord_def> veto_spots;
            find_connected_identical(*ri, all_door);
            copy(all_door.begin(), all_door.end(), back_inserter(veto_spots));
            for (set<coord_def>::const_iterator i = all_door.begin();
                 i != all_door.end(); ++i)
            {
                // If there are things in the way, push them aside
                actor* act = actor_at(*i);
                if (igrd(*i) != NON_ITEM || act)
                {
                    coord_def newpos;
                    // If we don't find a spot, try again ignoring tension.
                    const bool success =
                        get_push_space(*i, newpos, act, false, &veto_spots)
                        || get_push_space(*i, newpos, act, true, &veto_spots);
                    ASSERTM(success, "No push space from (%d,%d)", i->x, i->y);

                    move_items(*i, newpos);
                    if (act)
                    {
                        actor_at(*i)->move_to_pos(newpos);
                        if (act->is_player())
                            player_pushed = true;
                        veto_spots.push_back(newpos);
                    }
                }
            }

            // Close the door
            bool seen = false;
            vector<coord_def> excludes;
            for (set<coord_def>::const_iterator i = all_door.begin();
                 i != all_door.end(); ++i)
            {
                const coord_def& dc = *i;
                grd(dc) = DNGN_CLOSED_DOOR;
                set_terrain_changed(dc);
                dungeon_events.fire_position_event(DET_DOOR_CLOSED, dc);

                if (is_excluded(dc))
                    excludes.push_back(dc);

                if (you.see_cell(dc))
                    seen = true;

                had_effect = true;
            }

            if (seen)
            {
                for (set<coord_def>::const_iterator i = all_door.begin();
                     i != all_door.end(); ++i)
                {
                    if (env.map_knowledge(*i).seen())
                    {
                        env.map_knowledge(*i).set_feature(DNGN_CLOSED_DOOR);
#ifdef USE_TILE
                        env.tile_bk_bg(*i) = TILE_DNGN_CLOSED_DOOR;
#endif
                    }
                }

                update_exclusion_los(excludes);
                ++num_closed;
            }
        }

        // Try to seal the door
        if (grd(*ri) == DNGN_CLOSED_DOOR || grd(*ri) == DNGN_RUNED_DOOR)
        {
            set<coord_def> all_door;
            find_connected_identical(*ri, all_door);
            for (set<coord_def>::const_iterator i = all_door.begin();
                 i != all_door.end(); ++i)
            {
                temp_change_terrain(*i, DNGN_SEALED_DOOR, seal_duration,
                                    TERRAIN_CHANGE_DOOR_SEAL, warden);
                had_effect = true;
            }
        }
        else if (feat_is_travelable_stair(grd(*ri)))
        {
            dungeon_feature_type stype;
            if (feat_stair_direction(grd(*ri)) == CMD_GO_UPSTAIRS)
                stype = DNGN_SEALED_STAIRS_UP;
            else
                stype = DNGN_SEALED_STAIRS_DOWN;

            temp_change_terrain(*ri, stype, seal_duration,
                                TERRAIN_CHANGE_DOOR_SEAL, warden);
            had_effect = true;
        }
    }

    if (had_effect)
    {
        mprf(MSGCH_MONSTER_SPELL, "%s activates a sealing rune.",
                (warden->visible_to(&you) ? warden->name(DESC_THE, true).c_str()
                                          : "Someone"));
        if (num_closed > 1)
            mpr("The doors slam shut!");
        else if (num_closed == 1)
            mpr("A door slams shut!");

        if (player_pushed)
            mpr("You are pushed out of the doorway!");

        return true;
    }

    return false;
}

static bool _make_monster_angry(const monster* mon, monster* targ, bool actual)
{
    if (mon->friendly() != targ->friendly())
        return false;

    // targ is guaranteed to have a foe (needs_berserk checks this).
    // Now targ needs to be closer to *its* foe than mon is (otherwise
    // mon might be in the way).

    coord_def victim;
    if (targ->foe == MHITYOU)
        victim = you.pos();
    else if (targ->foe != MHITNOT)
    {
        const monster* vmons = &menv[targ->foe];
        if (!vmons->alive())
            return false;
        victim = vmons->pos();
    }
    else
    {
        // Should be impossible. needs_berserk should find this case.
        die("angered by no foe");
    }

    // If mon may be blocking targ from its victim, don't try.
    if (victim.distance_from(targ->pos()) > victim.distance_from(mon->pos()))
        return false;

    if (!actual)
        return true;

    if (you.can_see(mon))
    {
        if (mon->type == MONS_QUEEN_BEE && targ->type == MONS_KILLER_BEE)
        {
            mprf("%s calls on %s to defend %s!",
                mon->name(DESC_THE).c_str(),
                targ->name(DESC_THE).c_str(),
                mon->pronoun(PRONOUN_OBJECTIVE).c_str());
        }
        else
            mprf("%s goads %s on!", mon->name(DESC_THE).c_str(),
                 targ->name(DESC_THE).c_str());
    }

    targ->go_berserk(false);

    return true;
}

static bool _incite_monsters(const monster* mon, bool actual)
{
    if (is_sanctuary(you.pos()) || is_sanctuary(mon->pos()))
        return false;

    // Only things both in LOS of the inciter and within radius 4.
    const int radius_sq = 4 * 4 + 1;
    int goaded = 0;
    for (monster_near_iterator mi(mon, LOS_NO_TRANS); mi; ++mi)
    {
        if (*mi == mon || !mi->needs_berserk())
            continue;

        if (is_sanctuary(mi->pos()))
            continue;

        // Cannot goad other moths of wrath!
        if (mon->type == MONS_MOTH_OF_WRATH
            && mi->type == MONS_MOTH_OF_WRATH
        // Queen bees can only incite killer bees.
            || mon->type == MONS_QUEEN_BEE
               && mi->type != MONS_KILLER_BEE)
        {
            continue;
        }

        if (distance2(mon->pos(), mi->pos()) > radius_sq)
            continue;

        const bool worked = _make_monster_angry(mon, *mi, actual);
        if (worked && (!actual || !one_chance_in(3 * ++goaded)))
            return true;
    }

    return goaded > 0;
}

// Spells a monster may want to cast if fleeing from the player, and
// the player is not in sight.
static bool _ms_useful_fleeing_out_of_sight(monster* mon,
                                            mon_spell_slot slot)
{
    spell_type monspell = slot.spell;
    if (monspell == SPELL_NO_SPELL || _ms_waste_of_time(mon, slot))
        return false;

    switch (monspell)
    {
    case SPELL_HASTE:
    case SPELL_SWIFTNESS:
    case SPELL_INVISIBILITY:
    case SPELL_MINOR_HEALING:
    case SPELL_MAJOR_HEALING:
    case SPELL_ANIMATE_DEAD:
    case SPELL_TWISTED_RESURRECTION:
        return true;

    default:
        if (spell_typematch(monspell, SPTYP_SUMMONING) && one_chance_in(4))
            return true;
        break;
    }

    return false;
}

static bool _ms_low_hitpoint_cast(monster* mon, mon_spell_slot slot)
{
    spell_type monspell = slot.spell;
    if (_ms_waste_of_time(mon, slot))
        return false;

    bool targ_adj      = false;
    bool targ_sanct    = false;
    bool targ_friendly = false;
    bool targ_undead   = false;

    if (mon->foe == MHITYOU || mon->foe == MHITNOT)
    {
        if (adjacent(you.pos(), mon->pos()))
            targ_adj = true;
        if (is_sanctuary(you.pos()))
            targ_sanct = true;
        if (you.undead_or_demonic())
            targ_undead = true;
    }
    else
    {
        if (adjacent(menv[mon->foe].pos(), mon->pos()))
            targ_adj = true;
        if (is_sanctuary(menv[mon->foe].pos()))
            targ_sanct = true;
        if (menv[mon->foe].undead_or_demonic())
            targ_undead = true;
    }

    targ_friendly = (mon->foe == MHITYOU
                     ? mon->wont_attack()
                     : mons_aligned(mon, mon->get_foe()));

    switch (monspell)
    {
    case SPELL_HEAL_OTHER:
        return targ_friendly && mon->foe != MHITYOU;
    case SPELL_TELEPORT_OTHER:
        return !targ_sanct && !targ_friendly;
    case SPELL_MINOR_HEALING:
    case SPELL_MAJOR_HEALING:
    case SPELL_INVISIBILITY:
    case SPELL_TELEPORT_SELF:
    case SPELL_HASTE:
    case SPELL_DEATHS_DOOR:
    case SPELL_BERSERKER_RAGE:
    case SPELL_FRENZY:
    case SPELL_MIGHT:
    case SPELL_WIND_BLAST:
    case SPELL_EPHEMERAL_INFUSION:
        return true;
    case SPELL_VAMPIRIC_DRAINING:
        return !targ_sanct && targ_adj && !targ_friendly && !targ_undead;
    case SPELL_BLINK_AWAY:
    case SPELL_BLINK_RANGE:
    case SPELL_INJURY_MIRROR:
        return !targ_friendly;
    case SPELL_BLINK_OTHER:
        return !targ_sanct && targ_adj && !targ_friendly;
    case SPELL_CONFUSE:
    case SPELL_DRAIN_LIFE:
    case SPELL_BANISHMENT:
    case SPELL_HELLFIRE_BURST:
    case SPELL_FIREBALL:
    case SPELL_AIRSTRIKE:
    case SPELL_IOOD:
    case SPELL_ENSNARE:
    case SPELL_THROW_FLAME:
    case SPELL_MEPHITIC_CLOUD:
    case SPELL_DEATH_RATTLE:
        return !targ_friendly && !targ_sanct;
    case SPELL_BLINK:
    case SPELL_CONTROLLED_BLINK:
        return targ_adj;
    case SPELL_TOMB_OF_DOROKLOHE:
        return true;
    case SPELL_NO_SPELL:
        return false;
    case SPELL_INK_CLOUD:
        if (mon->type == MONS_KRAKEN)
            return true;
    default:
        return !targ_adj && spell_typematch(monspell, SPTYP_SUMMONING);
    }
}

// Spells for a quick get-away.
// Currently only used to get out of a net.
static bool _ms_quick_get_away(const monster* mon, spell_type monspell)
{
    switch (monspell)
    {
    case SPELL_TELEPORT_SELF:
        // Don't cast again if already about to teleport.
        if (mon->has_ench(ENCH_TP))
            return false;
        // intentional fall-through
    case SPELL_BLINK:
        return true;
    default:
        return false;
    }
}

// Is it worth bothering to invoke recall? (Currently defined by there being at
// least 3 things we could actually recall, and then with a probability inversely
// proportional to how many HD of allies are current nearby)
static bool _should_recall(monster* caller)
{
    // It's a long recitation - if we're winded, we can't use it.
    if (caller->has_ench(ENCH_BREATH_WEAPON))
        return false;

    int num = 0;
    for (monster_iterator mi; mi; ++mi)
    {
        if (mons_is_recallable(caller, *mi) && !caller->can_see(*mi))
            ++num;
    }

    // Since there are reinforcements we could recall, do we think we need them?
    if (num > 2)
    {
        int ally_hd = 0;
        for (monster_iterator mi; mi; ++mi)
        {
            if (*mi != caller && caller->can_see(*mi) && mons_aligned(caller, *mi)
                && !mons_is_firewood(*mi))
            {
                ally_hd += mi->get_experience_level();
            }
        }
        return 25 + roll_dice(2, 22) > ally_hd;
    }
    else
        return false;
}

/**
 * Recall a bunch of monsters!
 *
 * @param mons[in] the monster doing the recall
 * @param recall_target the max number of monsters to recall.
 * @returns whether anything was recalled.
 */
bool mons_word_of_recall(monster* mons, int recall_target)
{
    unsigned short num_recalled = 0;
    vector<monster* > mon_list;

    // Build the list of recallable monsters and randomize
    for (monster_iterator mi; mi; ++mi)
    {
        // Don't recall ourselves
        if (*mi == mons)
            continue;

        if (!mons_is_recallable(mons, *mi))
            continue;

        // Don't recall things that are already close to us
        if ((mons && mons->can_see(*mi))
            || (!mons && you.can_see(*mi)))
        {
            continue;
        }

        mon_list.push_back(*mi);
    }
    shuffle_array(mon_list);

    coord_def target   = (mons) ? mons->pos() : you.pos();
    unsigned short foe = (mons) ? mons->foe   : MHITYOU;

    // Now actually recall things
    for (unsigned int i = 0; i < mon_list.size(); ++i)
    {
        coord_def empty;
        if (find_habitable_spot_near(target, mons_base_type(mon_list[i]),
                                     3, false, empty)
            && mon_list[i]->move_to_pos(empty))
        {
            mon_list[i]->behaviour = BEH_SEEK;
            mon_list[i]->foe = foe;
            ++num_recalled;
            simple_monster_message(mon_list[i], " is recalled.");
        }
        // Can only recall a couple things at once
        if (num_recalled == recall_target)
            break;
    }
    return num_recalled;
}

static bool _valid_vine_spot(coord_def p)
{
    if (actor_at(p) || !monster_habitable_grid(MONS_PLANT, grd(p)))
        return false;

    int num_trees = 0;
    bool valid_trees = false;
    for (adjacent_iterator ai(p); ai; ++ai)
    {
        if (feat_is_tree(grd(*ai)))
        {
            // Make sure this spot is not on a diagonal to its only adjacent
            // tree (so that the vines can pull back against the tree properly)
            if (num_trees || !((*ai-p).sgn().x != 0 && (*ai-p).sgn().y != 0))
            {
                valid_trees = true;
                break;
            }
            else
                ++num_trees;
        }
    }

    if (!valid_trees)
        return false;

    // Now the connectivity check
    return !plant_forbidden_at(p, true);
}

static bool _awaken_vines(monster* mon, bool test_only = false)
{
    vector<coord_def> spots;
    for (radius_iterator ri(mon->pos(), LOS_NO_TRANS); ri; ++ri)
    {
        if (_valid_vine_spot(*ri))
            spots.push_back(*ri);
    }

    shuffle_array(spots);

    actor* foe = mon->get_foe();

    int num_vines = 1 + random2(3);
    if (mon->props.exists("vines_awakened"))
        num_vines = min(num_vines, 3 - mon->props["vines_awakened"].get_int());
    bool seen = false;

    for (unsigned int i = 0; i < spots.size(); ++i)
    {
        // Don't place vines where they can't see our target
        if (!cell_see_cell(spots[i], foe->pos(), LOS_NO_TRANS))
            continue;

        // Don't place a vine too near to another existing one
        bool too_close = false;
        for (distance_iterator di(spots[i], false, true, 3); di; ++di)
        {
            monster* m = monster_at(*di);
            if (m && m->type == MONS_SNAPLASHER_VINE)
            {
                too_close = true;
                break;
            }
        }
        if (too_close)
            continue;

        // We've found at least one valid spot, so the spell should be castable
        if (test_only)
            return true;

        // Actually place the vine and update properties
        if (monster* vine = create_monster(
            mgen_data(MONS_SNAPLASHER_VINE, SAME_ATTITUDE(mon), mon,
                        0, SPELL_AWAKEN_VINES, spots[i], mon->foe,
                        MG_FORCE_PLACE, mon->god, MONS_NO_MONSTER)))
        {
            vine->props["vine_awakener"].get_int() = mon->mid;
            mon->props["vines_awakened"].get_int()++;
            mon->add_ench(mon_enchant(ENCH_AWAKEN_VINES, 1, NULL, 200));
            --num_vines;
            if (you.can_see(vine))
                seen = true;
        }

        // We've finished placing all our vines
        if (num_vines == 0)
            break;
    }

    if (test_only)
        return false;
    else
    {
        if (seen)
            mpr("Vines fly forth from the trees!");
        return true;
    }
}

static bool _place_druids_call_beast(const monster* druid, monster* beast,
                                     const actor* target)
{
    for (int t = 0; t < 20; ++t)
    {
        // Attempt to find some random spot out of the target's los to place
        // the beast (but not too far away).
        coord_def area = clamp_in_bounds(target->pos() + coord_def(random_range(-11, 11),
                                                                   random_range(-11, 11)));
        if (cell_see_cell(target->pos(), area, LOS_DEFAULT))
            continue;

        coord_def base_spot;
        int tries = 0;
        while (tries < 10 && base_spot.origin())
        {
            find_habitable_spot_near(area, mons_base_type(beast), 3, false, base_spot);
            if (cell_see_cell(target->pos(), base_spot, LOS_DEFAULT))
                base_spot.reset();
            ++tries;
        }

        if (base_spot.origin())
            continue;

        beast->move_to_pos(base_spot);

        // Wake the beast up and calculate a path to the druid's target.
        // (Note that both BEH_WANDER and MTRAV_PATROL are necessary for it
        // to follow the given path and also not randomly wander off instead)
        beast->behaviour = BEH_WANDER;
        beast->foe = druid->foe;

        monster_pathfind mp;
        if (mp.init_pathfind(beast, target->pos()))
        {
            beast->travel_path = mp.calc_waypoints();
            if (!beast->travel_path.empty())
            {
                beast->target = beast->travel_path[0];
                beast->travel_target = MTRAV_PATROL;
            }
        }

        // Assign blame (for statistical purposes, mostly)
        mons_add_blame(beast, "called by " + druid->name(DESC_A, true));

        return true;
    }

    return false;
}

static void _cast_druids_call(const monster* mon)
{
    vector<monster*> mon_list;
    for (monster_iterator mi; mi; ++mi)
    {
        if (_valid_druids_call_target(mon, *mi))
            mon_list.push_back(*mi);
    }

    shuffle_array(mon_list);

    const actor* target = mon->get_foe();
    const int num = min((int)mon_list.size(),
                        mon->get_experience_level() > 10 ? random_range(2, 3)
                                                      : random_range(1, 2));

    for (int i = 0; i < num; ++i)
        _place_druids_call_beast(mon, mon_list[i], target);
}

static double _angle_between(coord_def origin, coord_def p1, coord_def p2)
{
    double ang0 = atan2(p1.x - origin.x, p1.y - origin.y);
    double ang  = atan2(p2.x - origin.x, p2.y - origin.y);
    return min(fabs(ang - ang0), fabs(ang - ang0 + 2 * PI));
}

// Does there already appear to be a bramble wall in this direction?
// We approximate this by seeing if there are at least two briar patches in
// a ray between us and our target, which turns out to be a pretty decent
// metric in practice.
static bool _already_bramble_wall(const monster* mons, coord_def targ)
{
    bolt tracer;
    tracer.source = mons->pos();
    tracer.target = targ;
    tracer.range = 12;
    tracer.is_tracer = true;
    tracer.is_beam = true;
    tracer.fire();

    int briar_count = 0;
    bool targ_reached = false;
    for (unsigned int i = 0; i < tracer.path_taken.size(); ++i)
    {
        coord_def p = tracer.path_taken[i];

        if (!targ_reached && p == targ)
            targ_reached = true;
        else if (!targ_reached)
            continue;

        if (monster_at(p) && monster_at(p)->type == MONS_BRIAR_PATCH)
            ++briar_count;
    }

    return briar_count > 1;
}

static bool _wall_of_brambles(monster* mons)
{
    mgen_data briar_mg = mgen_data(MONS_BRIAR_PATCH, SAME_ATTITUDE(mons),
                                   mons, 0, 0, coord_def(-1, -1), MHITNOT,
                                   MG_FORCE_PLACE);

    // We want to raise a defensive wall if we think our foe is moving to attack
    // us, and otherwise raise a wall further away to block off their escape.
    // (Each wall type uses different parameters)
    bool defensive = mons->props["foe_approaching"].get_bool();

    coord_def aim_pos = you.pos();
    coord_def targ_pos = mons->pos();

    // A defensive wall cannot provide any cover if our target is already
    // adjacent, so don't bother creating one.
    if (defensive && mons->pos().distance_from(aim_pos) == 1)
        return false;

    // Don't raise a non-defensive wall if it looks like there's an existing one
    // in the same direction already (this looks rather silly to see walls
    // springing up in the distance behind already-closed paths, and probably
    // is more likely to aid the player than the monster)
    if (!defensive)
    {
        if (_already_bramble_wall(mons, aim_pos))
            return false;
    }

    // Select a random radius for the circle used draw an arc from (affects
    // both shape and distance of the resulting wall)
    int rad = (defensive ? random_range(3, 5)
                         : min(11, mons->pos().distance_from(you.pos()) + 6));

    // Adjust the center of the circle used to draw the arc of the wall if
    // we're raising one defensively, based on both its radius and foe distance.
    // (The idea is the ensure that our foe will end up on the other side of it
    // without always raising the wall in exactly the same shape and position)
    if (defensive)
    {
        coord_def adjust = (targ_pos - aim_pos).sgn();

        targ_pos += adjust;
        if (rad == 5)
            targ_pos += adjust;
        if (mons->pos().distance_from(aim_pos) == 2)
            targ_pos += adjust;
    }

    // XXX: There is almost certainly a better way to calculate the points
    //      along the desired arcs, though this code produces the proper look.
    vector<coord_def> points;
    for (distance_iterator di(targ_pos, false, false, rad); di; ++di)
    {
        if (di.radius() == rad || di.radius() == rad - 1)
        {
            if (!actor_at(*di) && !cell_is_solid(*di))
            {
                if (defensive && _angle_between(targ_pos, aim_pos, *di) <= PI/4.0
                    || (!defensive
                        && _angle_between(targ_pos, aim_pos, *di) <= PI/(4.2 + rad/6.0)))
                {
                    points.push_back(*di);
                }
            }
        }
    }

    bool seen = false;
    for (unsigned int i = 0; i < points.size(); ++i)
    {
        briar_mg.pos = points[i];
        monster* briar = create_monster(briar_mg, false);
        if (briar)
        {
            briar->add_ench(mon_enchant(ENCH_SHORT_LIVED, 1, NULL, 80 + random2(100)));
            if (you.can_see(briar))
                seen = true;
        }
    }

    if (seen)
        mpr("Thorny briars emerge from the ground!");

    return true;
}

/**
 * Make the given monster cast the spell "Corrupting Pulse", corrupting
 * (temporarily malmutating) all creatures in LOS.
 *
 * @param mons  The monster in question.
 */
static void _corrupting_pulse(monster *mons)
{
    if (cell_see_cell(you.pos(), mons->pos(), LOS_DEFAULT))
    {
        targetter_los hitfunc(mons, LOS_SOLID);
        flash_view_delay(UA_MONSTER, MAGENTA, 300, &hitfunc);

        if (!is_sanctuary(you.pos())
            && cell_see_cell(you.pos(), mons->pos(), LOS_SOLID))
        {
            int num_mutations = 2 + random2(3);
            for (int i = 0; i < num_mutations; ++i)
                temp_mutate(RANDOM_BAD_MUTATION, "wretched star");
        }
    }

    for (radius_iterator ri(mons->pos(), LOS_RADIUS, C_ROUND); ri; ++ri)
    {
        monster *m = monster_at(*ri);
        if (m && cell_see_cell(mons->pos(), *ri, LOS_SOLID_SEE))
            m->corrupt();
    }
}

// Returns the clone just created (null otherwise)
monster* cast_phantom_mirror(monster* mons, monster* targ, int hp_perc, int summ_type)
{
    // Create clone.
    monster *mirror = clone_mons(targ, true);

    // Abort if we failed to place the monster for some reason.
    if (!mirror)
        return NULL;

    // Don't leak the real one with the targeting interface.
    if (you.prev_targ == mons->mindex())
    {
        you.prev_targ = MHITNOT;
        crawl_state.cancel_cmd_repeat();
    }
    mons->reset_client_id();

    mirror->mark_summoned(5, true, summ_type);
    mirror->add_ench(ENCH_PHANTOM_MIRROR);
    mirror->summoner = mons->mid;
    mirror->hit_points = mirror->hit_points * 100 / hp_perc;
    mirror->max_hit_points = mirror->max_hit_points * 100 / hp_perc;

    // Sometimes swap the two monsters, so as to disguise the original and the copy.
    if (coinflip())
    {
        // We can skip some habitability checks here, since the monsters are
        // known to be identical.
        coord_def p1 = targ->pos();
        coord_def p2 = mirror->pos();

        targ->set_position(p2);
        mirror->set_position(p1);

        mgrd(p1) = mirror->mindex();
        mgrd(p2) = targ->mindex();

        // Possibly constriction should be preserved so as to not sometimes leak
        // info on whether the clone or the original is adjacent to you, but
        // there's a fair amount of complication in this for very narrow benefit.
        targ->clear_far_constrictions();
        mirror->clear_far_constrictions();
    }

    return mirror;
}

static bool _trace_los(monster* agent, bool (*vulnerable)(actor*))
{
    bolt tracer;
    tracer.foe_ratio = 0;
    for (actor_near_iterator ai(agent, LOS_NO_TRANS); ai; ++ai)
    {
        if (agent == *ai || !agent->can_see(*ai) || !vulnerable(*ai))
            continue;

        if (mons_aligned(agent, *ai))
        {
            tracer.friend_info.count++;
            tracer.friend_info.power +=
                    ai->is_player() ? you.experience_level
                                    : ai->as_monster()->get_experience_level();
        }
        else
        {
            tracer.foe_info.count++;
            tracer.foe_info.power +=
                    ai->is_player() ? you.experience_level
                                    : ai->as_monster()->get_experience_level();
        }
    }
    return mons_should_fire(tracer);
}

static bool _tornado_vulnerable(actor* victim)
{
    return !victim->res_wind();
}

static bool _torment_vulnerable(actor* victim)
{
    if (victim->is_player())
        return !player_res_torment(false);

    return !victim->res_torment();
}

static bool _elec_vulnerable(actor* victim)
{
    return victim->res_elec() < 3;
}

static bool _mutation_vulnerable(actor* victim)
{
    return victim->can_mutate();
}

static bool _dummy_vulnerable(actor* victim)
{
    return true;
}

static bool _should_ephemeral_infusion(monster* agent)
{
    for (actor_near_iterator ai(agent, LOS_NO_TRANS); ai; ++ai)
    {
        if (agent == *ai || !ai->visible_to(agent)
            || ai->is_player() || !mons_aligned(*ai, agent))
        {
            continue;
        }
        monster* mon = ai->as_monster();
        if (!mon->has_ench(ENCH_EPHEMERAL_INFUSION)
            && mon->hit_points * 3 <= mon->max_hit_points * 2)
        {
            return true;
        }
    }
    return agent->hit_points * 3 <= agent->max_hit_points;
}

static void _cast_ephemeral_infusion(monster* agent)
{
    for (actor_near_iterator ai(agent, LOS_NO_TRANS); ai; ++ai)
    {
        if (!ai->visible_to(agent)
            || ai->is_player()
            || !mons_aligned(*ai, agent))
        {
            continue;
        }
        monster* mon = ai->as_monster();
        if (!mon->has_ench(ENCH_EPHEMERAL_INFUSION)
            && mon->hit_points < mon->max_hit_points)
        {
            const int dur =
                random2avg(agent->spell_hd(SPELL_EPHEMERAL_INFUSION), 2)
                * BASELINE_DELAY;
            mon->add_ench(
                mon_enchant(ENCH_EPHEMERAL_INFUSION,
                            4 * agent->spell_hd(SPELL_EPHEMERAL_INFUSION),
                            agent, dur));
        }
    }
}

static void _cast_black_mark(monster* agent)
{
    for (actor_near_iterator ai(agent, LOS_NO_TRANS); ai; ++ai)
    {
        if (!ai->visible_to(agent)
            || ai->is_player()
            || !mons_aligned(*ai, agent))
        {
            continue;
        }
        monster* mon = ai->as_monster();
        if (!mon->has_ench(ENCH_BLACK_MARK) && !mons_is_firewood(mon))
        {
            mon->add_ench(ENCH_BLACK_MARK);
            simple_monster_message(mon, " begins absorbing vital energies!");
        }
    }
}

static bool _glaciate_tracer(monster *caster, int pow, coord_def aim)
{
    targetter_cone hitfunc(caster, spell_range(SPELL_GLACIATE, pow));
    hitfunc.set_aim(aim);

    mon_attitude_type castatt = caster->temp_attitude();
    int friendly = 0, enemy = 0;

    for (map<coord_def, aff_type>::const_iterator p = hitfunc.zapped.begin();
         p != hitfunc.zapped.end(); ++p)
    {
        if (p->second <= 0)
            continue;

        const actor *victim = actor_at(p->first);
        if (!victim)
            continue;

        if (mons_atts_aligned(castatt, victim->temp_attitude()))
            friendly += victim->get_experience_level();
        else
            enemy += victim->get_experience_level();
    }

    return enemy > friendly;
}

bool mons_should_cloud_cone(monster* agent, int power, const coord_def pos)
{
    targetter_shotgun hitfunc(agent,
                              spell_range(SPELL_CLOUD_CONE, power));

    hitfunc.set_aim(pos);

    bolt tracer;
    tracer.foe_ratio = 80;
    tracer.source_id = agent->mid;
    tracer.target = pos;
    for (actor_near_iterator ai(agent, LOS_NO_TRANS); ai; ++ai)
    {
        if (hitfunc.is_affected(ai->pos()) == AFF_NO || !agent->can_see(*ai))
            continue;

        if (mons_aligned(agent, *ai))
        {
            tracer.friend_info.count++;
            tracer.friend_info.power += ai->get_experience_level();
        }
        else
        {
            tracer.foe_info.count++;
            tracer.foe_info.power += ai->get_experience_level();
        }
    }

    return mons_should_fire(tracer);
}

static bool _spray_tracer(monster *caster, int pow, bolt parent_beam, spell_type spell)
{
    vector<bolt> beams = get_spray_rays(caster, parent_beam.target,
                                        spell_range(spell, pow), 3);
    if (beams.size() == 0)
        return false;

    bolt beam;

    for (unsigned int i = 0; i < beams.size(); ++i)
    {
        bolt_parent_init(&parent_beam, &(beams[i]));
        fire_tracer(caster, beams[i]);
        beam.friend_info += beams[i].friend_info;
        beam.foe_info    += beams[i].foe_info;
    }

    return mons_should_fire(beam);
}

/** Chooses a matching spell from this spell list, based on frequency.
 *
 *  @param[in]  spells     the monster spell list to search
 *  @param[in]  flag       what SPFLAG_ the spell should match
 *  @param[out] slot_flags the flags of the slot the spell was found in
 */
static spell_type _pick_spell_from_list(const monster_spells &spells,
                                        int flag, unsigned short &slot_flags)
{
    spell_type spell_cast = SPELL_NO_SPELL;
    int weight = 0;
    for (unsigned int i = 0; i < spells.size(); i++)
    {
        int flags = get_spell_flags(spells[i].spell);
        if (!(flags & flag))
            continue;

        weight += spells[i].freq;
        if (x_chance_in_y(spells[i].freq, weight))
        {
            spell_cast = spells[i].spell;
            slot_flags = spells[i].flags;
        }
    }

    return spell_cast;
}

//---------------------------------------------------------------
//
// handle_mon_spell
//
// Give the monster a chance to cast a spell. Returns true if
// a spell was cast.
//
//---------------------------------------------------------------
bool handle_mon_spell(monster* mons, bolt &beem)
{
    bool monsterNearby = mons_near(mons);
    bool finalAnswer   = false;   // as in: "Is that your...?" {dlb}
    actor *foe = mons->get_foe();

    if (is_sanctuary(mons->pos()) && !mons->wont_attack())
        return false;

    // Yes, there is a logic to this ordering {dlb}:
    // .. berserk check is necessary for out-of-sequence actions like emergency
    // slot spells {blue}
    if (mons->asleep()
        || mons->submerged()
        || mons->berserk_or_insane()
        || mons_is_confused(mons, false)
        || !mons->has_spells())
    {
        return false;
    }

    unsigned short flags = MON_SPELL_NO_FLAGS;
    spell_type spell_cast = SPELL_NO_SPELL;

    monster_spells hspell_pass(mons->spells);

    if (mons->is_silenced()
        // Shapeshifters don't get 'real' spells.
        || mons->is_shapeshifter())
    {
        for (monster_spells::iterator it = hspell_pass.begin();
             it != hspell_pass.end(); it++)
        {
            if (it->flags & MON_SPELL_SILENCE_MASK)
            {
                hspell_pass.erase(it);
                it = hspell_pass.begin() - 1;
            }
        }

        if (!hspell_pass.size())
            return false;
    }

    if (!mon_enemies_around(mons))
    {
        // Force the casting of dig when the player is not visible -
        // this is EVIL!
        // only do this for monsters that are actually seeking out a
        // hostile target -doy
        if (mons->has_spell(SPELL_DIG)
            && mons_is_seeking(mons)
            && !(mons->wont_attack() && mons->foe == MHITYOU))
        {
            spell_cast = SPELL_DIG;
            flags = mons->spell_slot_flags(SPELL_DIG);
            finalAnswer = true;
        }
        else if ((mons->has_spell(SPELL_MINOR_HEALING)
                  || mons->has_spell(SPELL_MAJOR_HEALING))
                 && mons->hit_points < mons->max_hit_points)
        {
            // The player's out of sight!
            // Quick, let's take a turn to heal ourselves. -- bwr
            spell_cast = mons->has_spell(SPELL_MAJOR_HEALING) ?
                         SPELL_MAJOR_HEALING : SPELL_MINOR_HEALING;
            flags = mons->spell_slot_flags(spell_cast);
            finalAnswer = true;
        }
        else if (mons_is_fleeing(mons) || mons->pacified())
        {
            // Since the player isn't around, we'll extend the monster's
            // normal choices to include the self-enchant slot.
            if (one_chance_in(4))
            {
                int foundcount = 0;
                for (int i = hspell_pass.size() - 1; i >= 0; --i)
                {
                    if (_ms_useful_fleeing_out_of_sight(mons, hspell_pass[i])
                        && one_chance_in(++foundcount))
                    {
                        spell_cast = hspell_pass[i].spell;
                        flags = hspell_pass[i].flags;
                        finalAnswer = true;
                    }
                }
            }
        }
        else if (mons->foe == MHITYOU && !monsterNearby)
            return false;
    }

    // Monsters caught in a net try to get away.
    // This is only urgent if enemies are around.
    if (!finalAnswer && mon_enemies_around(mons)
        && mons->caught() && one_chance_in(15))
    {
        for (unsigned int i = 0; i < hspell_pass.size(); ++i)
        {
            if (_ms_quick_get_away(mons, hspell_pass[i].spell))
            {
                spell_cast = hspell_pass[i].spell;
                flags = hspell_pass[i].flags;
                finalAnswer = true;
                break;
            }
        }
    }

    const bool emergency = mons->hit_points < mons->max_hit_points / 3;

    // Promote the casting of useful spells for low-HP monsters.
    // (kraken should always cast their escape spell of inky).
    if (!finalAnswer && emergency
        && one_chance_in(mons->type == MONS_KRAKEN ? 4 : 8))
    {
        // Note: There should always be at least some chance we don't
        // get here... even if the monster is on its last HP.  That
        // way we don't have to worry about monsters infinitely casting
        // Healing on themselves (e.g. orc high priests).
        int found_spell = 0;
        for (unsigned int i = 0; i < hspell_pass.size(); ++i)
        {
            if (_ms_low_hitpoint_cast(mons, hspell_pass[i])
                && one_chance_in(++found_spell))
            {
                spell_cast = hspell_pass[i].spell;
                flags = hspell_pass[i].flags;
                finalAnswer = true;
            }
        }
    }

    bool ignore_good_idea = false;
    if (does_ru_wanna_redirect(mons))
    {
        int r = random2(100);
        int chance = div_rand_round(you.piety, 16);
        // 10% chance of stopping any attack
        if (r < chance)
        {
            if (mons->is_actual_spellcaster())
            {
                simple_monster_message(mons,
                    " begins to cast a spell, but is stunned by your will!",
                    MSGCH_GOD);
            }
            else if (mons->is_priest())
            {
                simple_monster_message(mons,
                    " begins to pray, but is stunned by your will!",
                    MSGCH_GOD);
            }
            else
            {
                simple_monster_message(mons,
                    " begins to attack, but is stunned by your will!",
                    MSGCH_GOD);
            }
            mons->lose_energy(EUT_SPELL);
            return true;
        }
        // 5% chance of redirect
        else if (r < chance + div_rand_round(chance, 2))
        {
            mprf(MSGCH_GOD, "You redirect %s's attack!",
                    mons->name(DESC_THE).c_str());
            int pfound = 0;
            for (radius_iterator ri(you.pos(),
                LOS_DEFAULT); ri; ++ri)
            {
                monster* new_target = monster_at(*ri);

                if (new_target == NULL
                    || mons_is_projectile(new_target->type)
                    || mons_is_firewood(new_target))
                {
                    continue;
                }

                ASSERT(new_target);

                if (one_chance_in(++pfound))
                {
                    mons->target = new_target->pos();
                    mons->foe = new_target->mindex();
                    beem.target = mons->target;
                    ignore_good_idea = true;
                }
            }
        }
    }

    if (!finalAnswer)
    {
        // If nothing found by now, safe friendlies and good
        // neutrals will rarely cast.
        if (mons->wont_attack() && !mon_enemies_around(mons)
            && !one_chance_in(10))
        {
            return false;
        }

        // Remove healing/invis/haste if we don't need them.
        for (monster_spells::iterator it = hspell_pass.begin();
             it != hspell_pass.end(); it++)
        {
            if (_ms_waste_of_time(mons, *it)
                // Should monster not have selected dig by now,
                // it never will.
                || it->spell == SPELL_DIG)
            {
                hspell_pass.erase(it);
                it = hspell_pass.begin() - 1;
            }
        }

        // If no useful spells... cast no spell.
        if (!hspell_pass.size())
            return false;

        const bolt orig_beem = beem;

        for (int attempt = 0; attempt < 2; attempt++)
        {
            beem = orig_beem;

            bool spellOK = false;

            // Setup spell.
            // If we didn't find a spell on the first pass, try a
            // self-enchantment.
            if (attempt > 0 && coinflip())
            {
                spell_cast = _pick_spell_from_list(hspell_pass,
                                                   SPFLAG_SELFENCH,
                                                   flags);
            }
            // Monsters that are fleeing or pacified and leaving the
            // level will always try to choose an emergency spell.
            else if (mons_is_fleeing(mons) || mons->pacified())
            {
                spell_cast = _pick_spell_from_list(hspell_pass,
                                                   SPFLAG_EMERGENCY,
                                                   flags);

                if (crawl_state.game_is_zotdef()
                    && mons->type == MONS_ICE_STATUE)
                {
                    // Don't spam ice beasts when wounded.
                    spell_cast = SPELL_NO_SPELL;
                }

                // Pacified monsters leaving the level will only
                // try and cast escape spells.
                if (spell_cast != SPELL_NO_SPELL
                    && mons->pacified()
                    && !testbits(get_spell_flags(spell_cast), SPFLAG_ESCAPE))
                {
                    spell_cast = SPELL_NO_SPELL;
                }
            }
            else
            {
                spell_cast = SPELL_NO_SPELL;

                unsigned what = random2(200);
                unsigned int i = 0;
                for (; i < hspell_pass.size(); i++)
                {
                    if (hspell_pass[i].flags & MON_SPELL_EMERGENCY
                        && !emergency)
                    {
                        continue;
                    }

                    if (hspell_pass[i].freq >= what)
                        break;
                    what -= hspell_pass[i].freq;
                }

                // If we roll above the weight of the spell list,
                // don't cast a spell at all.
                if (i == hspell_pass.size())
                    return false;

                spell_cast = hspell_pass[i].spell;
                flags = hspell_pass[i].flags;
            }

            // Setup the spell.
            if (spell_cast != SPELL_MELEE && spell_cast != SPELL_NO_SPELL)
                setup_mons_cast(mons, beem, spell_cast);

            // Try to find a nearby ally to haste, heal, might,
            // or make invisible.
            if ((spell_cast == SPELL_HASTE_OTHER
                 || spell_cast == SPELL_HEAL_OTHER
                 || spell_cast == SPELL_MIGHT_OTHER
                 || spell_cast == SPELL_INVISIBILITY_OTHER)
                    && !_set_allied_target(mons, beem,
                           mons->type == MONS_IRONBRAND_CONVOKER))
            {
                spell_cast = SPELL_NO_SPELL;
                continue;
            }

            // Alligators shouldn't spam swiftness.
            // (not in _ms_waste_of_time since it is not deterministic)
            if (spell_cast == SPELL_SWIFTNESS
                && mons->type == MONS_ALLIGATOR
                && (mons->swift_cooldown + random2avg(170, 5) >=
                    you.num_turns))
            {
                spell_cast = SPELL_NO_SPELL;
                continue;
            }

            // Don't knockback something we're trying to constrict.
            const actor *victim = actor_at(beem.target);
            if (victim &&
                beem.can_knockback(victim)
                && mons->is_constricting()
                && mons->constricting->count(victim->mid))
            {
                spell_cast = SPELL_NO_SPELL;
                continue;
            }

            if (spell_cast == SPELL_LRD && !in_bounds(beem.target))
            {
                spell_cast = SPELL_NO_SPELL;
                continue;
            }

            if (spell_cast == SPELL_DAZZLING_SPRAY
                && (!foe
                    || !_spray_tracer(mons, 12 * mons->spell_hd(spell_cast),
                                      beem, spell_cast)))
            {
                spell_cast = SPELL_NO_SPELL;
                continue;
            }

            // beam-type spells requiring tracers
            if (get_spell_flags(spell_cast) & SPFLAG_NEEDS_TRACER)
            {
                const bool explode =
                    spell_is_direct_explosion(spell_cast);
                fire_tracer(mons, beem, explode);
                // Good idea?
                if (mons_should_fire(beem, ignore_good_idea))
                    spellOK = true;
            }
            else
            {
                // All direct-effect/summoning/self-enchantments/etc.
                spellOK = true;

                if (_ms_direct_nasty(spell_cast)
                    && mons_aligned(mons, (mons->foe == MHITYOU) ?
                       &you : foe)) // foe=get_foe() is NULL for friendlies
                {                   // targeting you, which is bad here.
                    spellOK = false;
                }
                else if (mons->foe == MHITYOU || mons->foe == MHITNOT)
                {
                    // XXX: Note the crude hack so that monsters can
                    // use ME_ALERT to target (we should really have
                    // a measure of time instead of peeking to see
                    // if the player is still there). -- bwr
                    if (!you.visible_to(mons)
                        && (mons->target != you.pos() || coinflip()))
                    {
                        spellOK = false;
                    }
                }
                else if (!mons->can_see(&menv[mons->foe]))
                    spellOK = false;
                else if (mons->type == MONS_DAEVA
                         && mons->god == GOD_SHINING_ONE)
                {
                    const monster* mon = &menv[mons->foe];

                    // Don't allow TSO-worshipping daevas to make
                    // unchivalric magic attacks, except against
                    // appropriate monsters.
                    if (find_stab_type(mons, mon) != STAB_NO_STAB
                        && !tso_unchivalric_attack_safe_monster(mon))
                    {
                        spellOK = false;
                    }
                }
            }

            if (!spellOK)
                spell_cast = SPELL_NO_SPELL;

            if (spell_cast != SPELL_NO_SPELL)
                break;
        }
    }

    // Should the monster *still* not have a spell, well, too bad {dlb}:
    if (spell_cast == SPELL_NO_SPELL || spell_cast == SPELL_MELEE)
        return false;

    // Check for antimagic if casting a spell spell.
    if (mons->has_ench(ENCH_ANTIMAGIC) && flags & MON_SPELL_ANTIMAGIC_MASK
        && !x_chance_in_y(4 * BASELINE_DELAY,
                          4 * BASELINE_DELAY
                          + mons->get_ench(ENCH_ANTIMAGIC).duration))
    {
        // This may be a bad idea -- if we decide monsters shouldn't
        // lose a turn like players do not, please make this just return.
        simple_monster_message(mons, " falters for a moment.");
        mons->lose_energy(EUT_SPELL);
        return true;
    }

    if (mons->type == MONS_BALL_LIGHTNING)
        mons->suicide();

    // Dragons now have a time-out on their breath weapons, draconians too!
    if (flags & MON_SPELL_BREATH)
        setup_breath_timeout(mons);

    // FINALLY! determine primary spell effects {dlb}:
    if (spell_cast == SPELL_BLINK || spell_cast == SPELL_CONTROLLED_BLINK)
    {
        // Why only cast blink if nearby? {dlb}
        if (monsterNearby)
        {
            mons_cast_noise(mons, beem, spell_cast, flags);
            monster_blink(mons);
        }
        else
            return false;
    }
    else if (spell_cast == SPELL_BLINK_RANGE)
        blink_range(mons);
    else if (spell_cast == SPELL_BLINK_AWAY)
        blink_away(mons, true);
    else if (spell_cast == SPELL_BLINK_CLOSE)
        blink_close(mons);
    else
    {
        const int orig_hp = (foe) ? foe->stat_hp() : 0;
        const bool battlesphere = mons->props.exists("battlesphere");
        if (!(get_spell_flags(spell_cast) & SPFLAG_UTILITY))
            make_mons_stop_fleeing(mons);

        if (battlesphere)
            aim_battlesphere(mons, spell_cast, beem.ench_power, beem);
        mons_cast(mons, beem, spell_cast, flags);
        if (battlesphere)
            trigger_battlesphere(mons, beem);
        if (mons->has_ench(ENCH_GRAND_AVATAR))
            trigger_grand_avatar(mons, foe, spell_cast, orig_hp);
        if (flags & MON_SPELL_WIZARD && mons->has_ench(ENCH_SAP_MAGIC))
        {
            mons->add_ench(mon_enchant(ENCH_ANTIMAGIC, 0,
                                       mons->get_ench(ENCH_SAP_MAGIC)
                                             .agent(),
                                       BASELINE_DELAY
                                       * spell_difficulty(spell_cast)));
        }
        // Wellsprings "cast" from their own hp.
        if (spell_cast == SPELL_PRIMAL_WAVE
            && mons->type == MONS_ELEMENTAL_WELLSPRING)
        {
            mons->hurt(mons, 5 + random2(15));
            if (mons->alive())
            {
                create_monster(
                    mgen_data(MONS_WATER_ELEMENTAL, SAME_ATTITUDE(mons), mons,
                    3, spell_cast, mons->pos(), mons->foe, 0));
            }
        }
    }

    if (!(flags & MON_SPELL_INSTANT))
    {
        mons->lose_energy(EUT_SPELL);
        return true;
    }

    return false; // to let them do something else
}

static int _monster_abjure_target(monster* target, int pow, bool actual)
{
    int duration;

    if (!target->is_summoned(&duration))
        return 0;

    pow = max(20, fuzz_value(pow, 40, 25));

    if (!actual)
        return pow > 40 || pow >= duration;

    // TSO and Trog's abjuration protection.
    bool shielded = false;
    if (you_worship(GOD_SHINING_ONE))
    {
        pow = pow * (30 - target->get_hit_dice()) / 30;
        if (pow < duration)
        {
            simple_god_message(" protects your fellow warrior from evil "
                               "magic!");
            shielded = true;
        }
    }
    else if (you_worship(GOD_TROG))
    {
        pow = pow / 2;
        if (pow < duration)
        {
            simple_god_message(" shields your ally from puny magic!");
            shielded = true;
        }
    }
    else if (is_sanctuary(target->pos()))
    {
        pow = 0;
        mprf(MSGCH_GOD, "Zin's power protects your fellow warrior from evil magic!");
        shielded = true;
    }

    dprf("Abj: dur: %d, pow: %d, ndur: %d", duration, pow, duration - pow);

    mon_enchant abj = target->get_ench(ENCH_ABJ);
    if (!target->lose_ench_duration(abj, pow))
    {
        if (!shielded)
            simple_monster_message(target, " shudders.");
        return 1;
    }

    return 0;
}

static int _monster_abjuration(const monster* caster, bool actual)
{
    int maffected = 0;

    if (actual)
        mpr("Send 'em back where they came from!");

    int pow = caster->spell_hd(SPELL_ABJURATION) * 20;

    for (monster_near_iterator mi(caster->pos(), LOS_NO_TRANS); mi; ++mi)
    {
        if (!mons_aligned(caster, *mi))
            maffected += _monster_abjure_target(*mi, pow, actual);
    }

    return maffected;
}

static bool _mons_abjured(monster* mons, bool nearby)
{
    if (nearby && _monster_abjuration(mons, false) > 0
        && one_chance_in(3))
    {
        _monster_abjuration(mons, true);
        return true;
    }

    return false;
}

static monster_type _pick_swarmer()
{
    static monster_type swarmers[] =
    {
        MONS_KILLER_BEE,     MONS_KILLER_BEE,    MONS_KILLER_BEE,
        MONS_SCORPION,       MONS_WORM,          MONS_VAMPIRE_MOSQUITO,
        MONS_GOLIATH_BEETLE, MONS_SPIDER,        MONS_BUTTERFLY,
        MONS_YELLOW_WASP,    MONS_WORKER_ANT,    MONS_WORKER_ANT,
        MONS_WORKER_ANT
    };

    return RANDOM_ELEMENT(swarmers);
}

static monster_type _pick_random_wraith()
{
    static monster_type wraiths[] =
    {
        MONS_WRAITH, MONS_SHADOW_WRAITH, MONS_FREEZING_WRAITH,
        MONS_PHANTASMAL_WARRIOR, MONS_PHANTOM, MONS_HUNGRY_GHOST
    };

    return RANDOM_ELEMENT(wraiths);
}

static void _haunt_fixup(monster* summon, coord_def pos)
{
    actor* victim = actor_at(pos);
    if (victim && victim != summon)
    {
        summon->add_ench(mon_enchant(ENCH_HAUNTING, 1, victim,
                                     INFINITE_DURATION));
        summon->foe = victim->mindex();
    }
}

static monster_type _pick_horrible_thing()
{
    return one_chance_in(4) ? MONS_TENTACLED_MONSTROSITY
                            : MONS_ABOMINATION_LARGE;
}

static monster_type _pick_undead_summon()
{
    static monster_type undead[] =
    {
        MONS_NECROPHAGE, MONS_JIANGSHI, MONS_HUNGRY_GHOST, MONS_FLAYED_GHOST,
        MONS_ZOMBIE, MONS_SKELETON, MONS_SIMULACRUM, MONS_SPECTRAL_THING,
        MONS_FLYING_SKULL, MONS_MUMMY, MONS_VAMPIRE, MONS_WIGHT, MONS_WRAITH,
        MONS_SHADOW_WRAITH, MONS_FREEZING_WRAITH, MONS_PHANTASMAL_WARRIOR, MONS_SHADOW
    };

    return RANDOM_ELEMENT(undead);
}

static monster_type _pick_vermin()
{
    return random_choose_weighted(8, MONS_ORANGE_RAT,
                                  5, MONS_REDBACK,
                                  2, MONS_TARANTELLA,
                                  2, MONS_JUMPING_SPIDER,
                                  3, MONS_DEMONIC_CRAWLER,
                                  0);
}

static void _do_high_level_summon(monster* mons, bool monsterNearby,
                                  spell_type spell_cast,
                                  monster_type (*mpicker)(), int nsummons,
                                  god_type god, coord_def *target = NULL,
                                  void (*post_hook)(monster*, coord_def) = NULL,
                                  bool allow_abjure = true)
{
    if (allow_abjure && _mons_abjured(mons, monsterNearby))
        return;

    const int duration = min(2 + mons->spell_hd(spell_cast) / 5, 6);

    for (int i = 0; i < nsummons; ++i)
    {
        monster_type which_mons = mpicker();

        if (which_mons == MONS_NO_MONSTER)
            continue;

        monster* summon = create_monster(
            mgen_data(which_mons, SAME_ATTITUDE(mons), mons,
                      duration, spell_cast, target ? *target : mons->pos(),
                      mons->foe, 0, god));
        if (summon && post_hook)
            post_hook(summon, target ? *target : mons->pos());
    }
}

static void _mons_summon_elemental(monster* mons,
                                   bool monsterNearby,
                                   spell_type spell_cast,
                                   monster_type elemental,
                                   god_type god)
{
    if (_mons_abjured(mons, monsterNearby))
        return;

    const int count = 1 + (mons->spell_hd(spell_cast) > 15)
                      + random2(mons->spell_hd(spell_cast) / 7 + 1);

    for (int i = 0; i < count; i++)
    {
        create_monster(
            mgen_data(elemental, SAME_ATTITUDE(mons), mons,
                      3, spell_cast, mons->pos(), mons->foe, 0, god));
    }
    return;
}

// Returns true if a message referring to the player's legs makes sense.
static bool _legs_msg_applicable()
{
    return you.species != SP_NAGA && !you.fishtail;
}

void mons_cast_haunt(monster* mons)
{
    coord_def fpos;

    switch (mons->foe)
    {
    case MHITNOT:
        return;

    case MHITYOU:
        fpos = you.pos();
        break;

    default:
        fpos = menv[mons->foe].pos();
    }

    _do_high_level_summon(mons, mons_near(mons), SPELL_HAUNT,
                          _pick_random_wraith, random_range(2, 4),
                          GOD_NO_GOD, &fpos, _haunt_fixup);
}

static void _mons_cast_summon_illusion(monster* mons, spell_type spell)
{
    actor *foe = mons->get_foe();
    if (!foe || !actor_is_illusion_cloneable(foe))
        return;

    mons_summon_illusion_from(mons, foe, spell);
}

void mons_cast_spectral_orcs(monster* mons)
{
    coord_def fpos;

    switch (mons->foe)
    {
    case MHITNOT:
        return;

    case MHITYOU:
        fpos = you.pos();
        break;

    default:
        fpos = menv[mons->foe].pos();
    }

    const int abj = 3;

    for (int i = random2(3) + 1; i > 0; --i)
    {
        monster_type mon = MONS_ORC;
        if (coinflip())
            mon = MONS_ORC_WARRIOR;
        else if (one_chance_in(3))
            mon = MONS_ORC_KNIGHT;
        else if (one_chance_in(10))
            mon = MONS_ORC_WARLORD;

        // Use the original monster type as the zombified type here, to
        // get the proper stats from it.
        if (monster *orc = create_monster(
                  mgen_data(MONS_SPECTRAL_THING, SAME_ATTITUDE(mons), mons,
                          abj, SPELL_SUMMON_SPECTRAL_ORCS, fpos, mons->foe,
                          0, mons->god, mon)))
        {
            // set which base type this orc is pretending to be for gear
            // purposes
            if (mon != MONS_ORC)
            {
                orc->mname = mons_type_name(mon, DESC_PLAIN);
                orc->flags |= MF_NAME_REPLACE | MF_NAME_DESCRIPTOR;
            }
            orc->orc_type = mon;

            // give gear using the base type
            give_item(orc, env.absdepth0, true, true);

            // set gear as summoned
            orc->mark_summoned(abj, true, SPELL_SUMMON_SPECTRAL_ORCS);
        }
    }
}

static bool _mons_vampiric_drain(monster *mons)
{
    actor *target = mons->get_foe();
    if (!target)
        return false;
    if (grid_distance(mons->pos(), target->pos()) > 1)
        return false;

    int pow = mons->spell_hd(SPELL_VAMPIRIC_DRAINING) * 12;
    int hp_cost = 3 + random2avg(9, 2) + 1 + random2(pow) / 7;

    hp_cost = min(hp_cost, target->stat_hp());
    hp_cost = min(hp_cost, mons->max_hit_points - mons->hit_points);
    if (target->res_negative_energy() > 0)
        hp_cost -= hp_cost * target->res_negative_energy() / 3;

    if (!hp_cost)
    {
        simple_monster_message(mons,
                               " is infused with unholy energy, but nothing happens.",
                               MSGCH_MONSTER_SPELL);
        return false;
    }

    dprf("vamp draining: %d damage, %d healing", hp_cost, hp_cost/2);

    if (you.can_see(mons))
    {
        simple_monster_message(mons,
                               " is infused with unholy energy.",
                               MSGCH_MONSTER_SPELL);
    }
    else
        mpr("Unholy energy fills the air.");

    if (target->is_player())
    {
        ouch(hp_cost, mons->mindex(), KILLED_BY_BEAM, "by vampiric draining");
        if (mons->heal(hp_cost * 2 / 3))
        {
            simple_monster_message(mons,
                " draws life force from you and is healed!");
        }
    }
    else
    {
        monster* mtarget = target->as_monster();
        const string targname = mtarget->name(DESC_THE);
        mtarget->hurt(mons, hp_cost);
        if (mons->heal(hp_cost * 2 / 3))
        {
            simple_monster_message(mons,
                make_stringf(" draws life force from %s and is healed!",
                targname.c_str()).c_str());
        }
        if (mtarget->alive())
            print_wounds(mtarget);
    }

    return true;
}

static bool _mons_cast_freeze(monster* mons)
{
    actor *target = mons->get_foe();
    if (!target)
        return false;
    if (grid_distance(mons->pos(), target->pos()) > 1)
        return false;

    const int pow = mons->spell_hd(SPELL_FREEZE) * 6;

    const int base_damage = roll_dice(1, 3 + pow / 3);
    int damage = 0;

    if (target->is_player())
    {
        damage = resist_adjust_damage(&you, BEAM_COLD, player_res_cold(),
                                      base_damage, true);
    }
    else
    {
        bolt beam;
        beam.flavour = BEAM_COLD;
        damage = mons_adjust_flavoured(target->as_monster(), beam, base_damage);
    }

    if (you.can_see(target))
    {
        mprf("%s %s frozen.", target->name(DESC_THE).c_str(),
                              target->conj_verb("are").c_str());
    }

    target->hurt(mons, damage);

    if (target->alive())
    {
        target->expose_to_element(BEAM_COLD, damage);

        if (target->is_monster() && target->res_cold() <= 0)
        {
            const int stun = (1 - target->res_cold()) * random2(min(7, 2 + pow/12));
            target->as_monster()->speed_increment -= stun;
        }
    }

    return true;
}

void setup_breath_timeout(monster* mons)
{
    if (mons->has_ench(ENCH_BREATH_WEAPON))
        return;

    int timeout = roll_dice(1, 5);

    dprf("breath timeout: %d", timeout);

    mon_enchant breath_timeout = mon_enchant(ENCH_BREATH_WEAPON, 1, mons, timeout*10);
    mons->add_ench(breath_timeout);
}

/**
 * Maybe mesmerise the player.
 *
 * This function decides whether or not it is possible for the player to become
 * mesmerised by mons. It will return a variety of values depending on whether
 * or not this can succeed or has succeeded; finally, it will add mons to the
 * player's list of beholders.
 *
 * @param mons      The monster doing the mesmerisation.
 * @param actual    Whether or not we are actually casting the spell. If false,
 *                  no messages are emitted.
 * @return          0 if the player could be mesmerised but wasn't, 1 if the
 *                  player was mesmerised, -1 if the player couldn't be
 *                  mesmerised.
**/
static int _mons_mesmerise(monster* mons, bool actual)
{
    bool already_mesmerised = you.beheld_by(mons);

    if (!you.visible_to(mons)             // Don't mesmerise while invisible.
        || (!you.can_see(mons)            // Or if we are, and you're aren't
            && !already_mesmerised)       // already mesmerised by us.
        || !player_can_hear(mons->pos())  // Or if you're silenced, or we are.
        || you.berserk()                  // Or if you're berserk.
        || mons->has_ench(ENCH_CONFUSION) // Or we're confused,
        || mons_is_fleeing(mons)          // fleeing,
        || mons->pacified()               // pacified,
        || mons->friendly())              // or friendly!
    {
        return -1;
    }

    if (actual)
    {
        if (!already_mesmerised)
        {
            simple_monster_message(mons, " attempts to bespell you!");
            flash_view(UA_MONSTER, LIGHTMAGENTA);
        }
        else
        {
            mprf("%s draws you further into %s thrall.",
                    mons->name(DESC_THE).c_str(),
                    mons->pronoun(PRONOUN_POSSESSIVE).c_str());
        }
    }

    const int pow = min(mons->spell_hd(SPELL_MESMERISE) * 10, 200);

    // Don't mesmerise if you pass an MR check or have clarity.
    // If you're already mesmerised, you cannot resist further.
    if ((you.check_res_magic(pow) > 0 || you.clarity()
         || you.duration[DUR_MESMERISE_IMMUNE]) && !already_mesmerised)
    {
        if (actual)
        {
            if (you.clarity())
                canned_msg(MSG_YOU_UNAFFECTED);
            else
                canned_msg(MSG_YOU_RESIST);
        }

        return 0;
    }

    you.add_beholder(mons);

    return 1;
}

// Check whether targets might be scared.
// Returns 0, if targets can be scared but the attempt failed or wasn't made.
// Returns 1, if targets are scared.
// Returns -1, if targets can never be scared.
static int _mons_cause_fear(monster* mons, bool actual)
{
    if (actual)
    {
        if (you.can_see(mons))
            simple_monster_message(mons, " radiates an aura of fear!");
        else if (you.see_cell(mons->pos()))
            mpr("An aura of fear fills the air!");
    }

    int retval = -1;

    const int pow = min(mons->spell_hd(SPELL_CAUSE_FEAR) * 18, 200);

    if (mons->can_see(&you) && !mons->wont_attack() && !you.afraid_of(mons))
    {
        if (you.holiness() != MH_NATURAL)
        {
            if (actual)
                canned_msg(MSG_YOU_UNAFFECTED);
        }
        else if (you.check_res_magic(pow) > 0)
        {
            if (actual)
                canned_msg(MSG_YOU_RESIST);
        }
        else if (actual && you.add_fearmonger(mons))
        {
            retval = 1;

            you.increase_duration(DUR_AFRAID, 10 + random2avg(pow / 10, 4));

            if (!mons->has_ench(ENCH_FEAR_INSPIRING))
                mons->add_ench(ENCH_FEAR_INSPIRING);
        }
        else
          retval = 0;
    }

    for (monster_near_iterator mi(mons->pos()); mi; ++mi)
    {
        if (*mi == mons)
            continue;

        // Magic-immune, unnatural and "firewood" monsters are
        // immune to being scared. Same-aligned monsters are
        // never affected, even though they aren't immune.
        // Will not further scare a monster that is already afraid.
        if (mons_immune_magic(*mi)
            || mi->holiness() != MH_NATURAL
            || mons_is_firewood(*mi)
            || mons_atts_aligned(mi->attitude, mons->attitude)
            || mi->has_ench(ENCH_FEAR))
        {
            continue;
        }

        retval = 0;

        // It's possible to scare this monster. If its magic
        // resistance fails, do so.
        int res_margin = mi->check_res_magic(pow);
        if (res_margin > 0)
        {
            if (actual)
                simple_monster_message(*mi, mons_resist_string(*mi, res_margin));
            continue;
        }

        if (actual
            && mi->add_ench(mon_enchant(ENCH_FEAR, 0, mons)))
        {
            retval = 1;

            if (you.can_see(*mi))
                simple_monster_message(*mi, " looks frightened!");

            behaviour_event(*mi, ME_SCARE, mons);

            if (!mons->has_ench(ENCH_FEAR_INSPIRING))
                mons->add_ench(ENCH_FEAR_INSPIRING);
        }
    }

    if (actual && retval == 1 && you.see_cell(mons->pos()))
        flash_view_delay(UA_MONSTER, DARKGREY, 300);

    return retval;
}

static int _mons_mass_confuse(monster* mons, bool actual)
{
    int retval = -1;

    const int pow = min(mons->spell_hd(SPELL_MASS_CONFUSION) * 8, 200);

    if (mons->can_see(&you) && !mons->wont_attack())
    {
        retval = 0;

        if (actual)
            if (you.check_res_magic(pow) > 0)
                canned_msg(MSG_YOU_RESIST);
            else
            {
                you.confuse(mons, 2 + random2(5));
                retval = 1;
            }
    }

    for (monster_near_iterator mi(mons->pos()); mi; ++mi)
    {
        if (*mi == mons)
            continue;

        if (mons_immune_magic(*mi)
            || mons_is_firewood(*mi)
            || mons_atts_aligned(mi->attitude, mons->attitude))
        {
            continue;
        }

        retval = 0;

        int res_margin = mi->check_res_magic(pow);
        if (res_margin > 0)
        {
            if (actual)
                simple_monster_message(*mi, mons_resist_string(*mi, res_margin));
            continue;
        }
        if (actual)
        {
            retval = 1;
            mi->confuse(mons, 2 + random2(5));
        }
    }

    return retval;
}

static coord_def _mons_fragment_target(monster *mons)
{
    coord_def target(GXM+1, GYM+1);
    int pow = 6 * mons->spell_hd(SPELL_LRD);

    // Shadow casting should try to affect the same tile as the player.
    if (mons_is_player_shadow(mons))
    {
        bool temp;
        bolt beam;
        if (!setup_fragmentation_beam(beam, pow, mons, mons->target, false,
                                      true, true, NULL, temp, temp))
        {
            return target;
        }
        return mons->target;
    }

    int range = spell_range(SPELL_LRD, pow, false);
    int maxpower = 0;
    for (distance_iterator di(mons->pos(), true, true, range); di; ++di)
    {
        bool temp;

        if (!cell_see_cell(mons->pos(), *di, LOS_SOLID))
            continue;

        bolt beam;
        if (!setup_fragmentation_beam(beam, pow, mons, *di, false, true, true,
                                      NULL, temp, temp))
        {
            continue;
        }

        beam.range = range;
        fire_tracer(mons, beam, true);
        if (!mons_should_fire(beam))
            continue;

        bolt beam2;
        if (!setup_fragmentation_beam(beam2, pow, mons, *di, false, false, true,
                                      NULL, temp, temp))
        {
            continue;
        }

        beam2.range = range;
        fire_tracer(mons, beam2, true);

        if (beam2.foe_info.count > 0
            && beam2.foe_info.power > maxpower)
        {
            maxpower = beam2.foe_info.power;
            target = *di;
        }
    }

    return target;
}

static void _blink_allies_encircle(const monster* mon)
{
    vector<monster*> allies;
    const coord_def foepos = mon->get_foe()->pos();

    for (monster_near_iterator mi(mon, LOS_NO_TRANS); mi; ++mi)
    {
        if (_valid_encircle_ally(mon, *mi, foepos))
            allies.push_back(*mi);
    }
    shuffle_array(allies);

    int count = max(1, mon->spell_hd(SPELL_BLINK_ALLIES_ENCIRCLE) / 8
                       + random2(mon->spell_hd(SPELL_BLINK_ALLIES_ENCIRCLE) / 4));

    for (unsigned int i = 0; i < allies.size() && count; ++i)
    {
        coord_def empty;
        if (find_habitable_spot_near(foepos, mons_base_type(allies[i]), 1, false, empty))
        {
            if (allies[i]->blink_to(empty))
            {
                // XXX: This seems an awkward way to give a message for something
                // blinking from out of sight into sight. Probably could use a
                // more general solution.
                if (!(allies[i]->flags & MF_WAS_IN_VIEW)
                    && allies[i]->flags & MF_SEEN)
                {
                    simple_monster_message(allies[i], " blinks into view!");
                }
                allies[i]->behaviour = BEH_SEEK;
                allies[i]->foe = mon->foe;
                count--;
            }
        }
    }
}

static void _blink_allies_away(const monster* mon)
{
    vector<monster*> allies;
    const coord_def foepos = mon->get_foe()->pos();

    for (monster_near_iterator mi(mon, LOS_NO_TRANS); mi; ++mi)
    {
        if (_valid_blink_away_ally(mon, *mi, foepos))
            allies.push_back(*mi);
    }
    shuffle_array(allies);

    int count = max(1, mon->spell_hd(SPELL_BLINK_ALLIES_AWAY) / 8
                       + random2(mon->spell_hd(SPELL_BLINK_ALLIES_AWAY) / 4));

    for (unsigned int i = 0; i < allies.size() && count; ++i)
    {
        if (blink_away(allies[i], &you, false))
            count--;
    }
}

static int _max_tentacles(const monster* mon)
{
    if (mons_base_type(mon) == MONS_KRAKEN)
        return MAX_ACTIVE_KRAKEN_TENTACLES;
    else if (mon->type == MONS_TENTACLED_STARSPAWN)
        return MAX_ACTIVE_STARSPAWN_TENTACLES;
    else
        return 0;
}

static int _mons_available_tentacles(monster* head)
{
    int tentacle_count = 0;

    for (monster_iterator mi; mi; ++mi)
    {
        if (mi->is_child_tentacle_of(head))
            tentacle_count++;
    }

    return _max_tentacles(head) - tentacle_count;
}

static void _mons_create_tentacles(monster* head)
{
    int head_index = head->mindex();
    if (invalid_monster_index(head_index))
    {
        mprf(MSGCH_ERROR, "Error! Tentacle head is not a part of the current environment!");
        return;
    }

    int possible_count = _mons_available_tentacles(head);

    if (possible_count <= 0)
        return;

    monster_type tent_type = mons_tentacle_child_type(head);

    vector<coord_def> adj_squares;

    // Collect open adjacent squares. Candidate squares must be
    // unoccupied.
    for (adjacent_iterator adj_it(head->pos()); adj_it; ++adj_it)
    {
        if (monster_habitable_grid(tent_type, grd(*adj_it))
            && !actor_at(*adj_it))
        {
            adj_squares.push_back(*adj_it);
        }
    }

    if (unsigned(possible_count) > adj_squares.size())
        possible_count = adj_squares.size();
    else if (adj_squares.size() > unsigned(possible_count))
        shuffle_array(adj_squares);

    int visible_count = 0;

    for (int i = 0 ; i < possible_count; ++i)
    {
        if (monster *tentacle = create_monster(
            mgen_data(tent_type, SAME_ATTITUDE(head), head,
                        0, 0, adj_squares[i], head->foe,
                        MG_FORCE_PLACE, head->god, MONS_NO_MONSTER, head_index,
                        head->colour, PROX_CLOSE_TO_PLAYER)))
        {
            if (you.can_see(tentacle))
                visible_count++;

            tentacle->props["inwards"].get_int() = head_index;

            if (head->holiness() == MH_UNDEAD)
                tentacle->flags |= MF_FAKE_UNDEAD;
        }
    }

    if (mons_base_type(head) == MONS_KRAKEN)
    {
        if (visible_count == 1)
            mpr("A tentacle rises from the water!");
        else if (visible_count > 1)
            mpr("Tentacles burst out of the water!");
    }
    else if (head->type == MONS_TENTACLED_STARSPAWN)
    {
        if (visible_count == 1)
            mpr("A tentacle flies out from the starspawn's body!");
        else if (visible_count > 1)
            mpr("Tentacles burst from the starspawn's body!");
    }
    return;
}

struct branch_summon_pair
{
    branch_type     origin;
    const pop_entry *pop;
};

static const pop_entry _invitation_lair[] =
{ // Lair enemies
  {  1,   1,   80, FLAT, MONS_YAK },
  {  1,   1,   60, FLAT, MONS_BLINK_FROG },
  {  1,   1,   40, FLAT, MONS_ELEPHANT },
  {  1,   1,   20, FLAT, MONS_SPINY_FROG },
  { 0,0,0,FLAT,MONS_0 }
};

static const pop_entry _invitation_snake[] =
{ // Snake enemies
  {  1,   1,   90, FLAT, MONS_NAGA },
  {  1,   1,   70, FLAT, MONS_WATER_MOCCASIN },
  {  1,   1,   30, FLAT, MONS_BLACK_MAMBA },
  {  1,   1,   10, FLAT, MONS_GUARDIAN_SERPENT },
  { 0,0,0,FLAT,MONS_0 }
};

static const pop_entry _invitation_spider[] =
{ // Spider enemies
  {  1,   1,   80, FLAT, MONS_SPIDER },
  {  1,   1,   60, FLAT, MONS_JUMPING_SPIDER },
  {  1,   1,   40, FLAT, MONS_BORING_BEETLE },
  {  1,   1,   20, FLAT, MONS_ORB_SPIDER },
  { 0,0,0,FLAT,MONS_0 }
};

static const pop_entry _invitation_swamp[] =
{ // Swamp enemies
  {  1,   1,   60, FLAT, MONS_RAVEN },
  {  1,   1,   60, FLAT, MONS_SWAMP_DRAKE },
  {  1,   1,   40, FLAT, MONS_VAMPIRE_MOSQUITO },
  {  1,   1,   40, FLAT, MONS_INSUBSTANTIAL_WISP },
  {  1,   1,   20, FLAT, MONS_BABY_ALLIGATOR },
  { 0,0,0,FLAT,MONS_0 }
};

static const pop_entry _invitation_shoals[] =
{ // Swamp enemies
  {  1,   1,   80, FLAT, MONS_MERFOLK },
  {  1,   1,   60, FLAT, MONS_SIREN },
  {  1,   1,   40, FLAT, MONS_MANTICORE },
  {  1,   1,   20, FLAT, MONS_SNAPPING_TURTLE },
  { 0,0,0,FLAT,MONS_0 }
};

static const pop_entry _invitation_orc[] =
{ // Orc enemies
  {  1,   1,   90, FLAT, MONS_ORC_PRIEST },
  {  1,   1,   70, FLAT, MONS_ORC_WARRIOR },
  {  1,   1,   30, FLAT, MONS_WARG },
  {  1,   1,   10, FLAT, MONS_TROLL },
  { 0,0,0,FLAT,MONS_0 }
};

static const pop_entry _invitation_elf[] =
{ // Elf enemies
  {  1,   1,   90, FLAT, MONS_DEEP_ELF_FIGHTER },
  {  1,   1,   70, FLAT, MONS_DEEP_ELF_PRIEST },
  {  1,   1,   40, FLAT, MONS_DEEP_ELF_MAGE },
  { 0,0,0,FLAT,MONS_0 }
};

static const pop_entry _invitation_vaults[] =
{ // Vaults enemies
  {  1,   1,   80, FLAT, MONS_HUMAN },
  {  1,   1,   60, FLAT, MONS_YAKTAUR },
  {  1,   1,   40, FLAT, MONS_IRONHEART_PRESERVER },
  {  1,   1,   20, FLAT, MONS_VAULT_SENTINEL },
  { 0,0,0,FLAT,MONS_0 }
};

static const pop_entry _invitation_crypt[] =
{ // Crypt enemies
  {  1,   1,   80, FLAT, MONS_WRAITH },
  {  1,   1,   60, FLAT, MONS_SHADOW },
  {  1,   1,   40, FLAT, MONS_VAMPIRE },
  {  1,   1,   20, FLAT, MONS_SHADOW_WRAITH },
  { 0,0,0,FLAT,MONS_0 }
};

static branch_summon_pair _invitation_summons[] =
{
  { BRANCH_LAIR,   _invitation_lair },
  { BRANCH_SNAKE,  _invitation_snake },
  { BRANCH_SPIDER, _invitation_spider },
  { BRANCH_SWAMP,  _invitation_swamp },
  { BRANCH_SHOALS, _invitation_shoals },
  { BRANCH_ORC,    _invitation_orc },
  { BRANCH_ELF,    _invitation_elf },
  { BRANCH_VAULTS, _invitation_vaults },
  { BRANCH_CRYPT,  _invitation_crypt }
};

static const pop_entry _planerend_lair[] =
{ // Lair enemies
  {  1,   1,  100, FLAT, MONS_CATOBLEPAS },
  {  1,   1,  100, FLAT, MONS_DIRE_ELEPHANT },
  {  1,   1,   60, FLAT, MONS_TORPOR_SNAIL },
  { 0,0,0,FLAT,MONS_0 }
};

static const pop_entry _planerend_snake[] =
{ // Snake enemies
  {  1,   1,   40, FLAT, MONS_ANACONDA },
  {  1,   1,   60, FLAT, MONS_SALAMANDER_FIREBRAND },
  {  1,   1,  100, FLAT, MONS_GUARDIAN_SERPENT },
  {  1,   1,  100, FLAT, MONS_GREATER_NAGA },
  { 0,0,0,FLAT,MONS_0 }
};

static const pop_entry _planerend_spider[] =
{ // Spider enemies
  {  1,   1,  100, FLAT, MONS_GHOST_MOTH },
  {  1,   1,  100, FLAT, MONS_EMPEROR_SCORPION },
  {  1,   1,   20, FLAT, MONS_RED_WASP },
  {  1,   1,   60, FLAT, MONS_ORB_SPIDER },
  {  1,   1,   20, FLAT, MONS_TARANTELLA },
  { 0,0,0,FLAT,MONS_0 }
};

static const pop_entry _planerend_swamp[] =
{ // Swamp enemies
  {  1,   1,   80, FLAT, MONS_SWAMP_DRAGON },
  {  1,   1,   80, FLAT, MONS_HYDRA },
  {  1,   1,  100, FLAT, MONS_SHAMBLING_MANGROVE },
  {  1,   1,   40, FLAT, MONS_THORN_HUNTER },
  { 0,0,0,FLAT,MONS_0 }
};

static const pop_entry _planerend_shoals[] =
{ // Shoals enemies
  {  1,   1,   50, FLAT, MONS_WATER_NYMPH },
  {  1,   1,  100, FLAT, MONS_MERFOLK_JAVELINEER },
  {  1,   1,  100, FLAT, MONS_MERFOLK_AQUAMANCER },
  {  1,   1,   80, FLAT, MONS_ALLIGATOR_SNAPPING_TURTLE },
  { 0,0,0,FLAT,MONS_0 }
};

static const pop_entry _planerend_slime[] =
{ // Slime enemies
  {  1,   1,  100, FLAT, MONS_ACID_BLOB },
  {  1,   1,  100, FLAT, MONS_AZURE_JELLY },
  {  1,   1,  100, FLAT, MONS_SLIME_CREATURE }, // changed to titanic below
  {  1,   1,   80, FLAT, MONS_GIANT_ORANGE_BRAIN },
  { 0,0,0,FLAT,MONS_0 }
};

static const pop_entry _planerend_orc[] =
{ // Orc enemies
  {  1,   1,  100, FLAT, MONS_ORC_WARLORD },
  {  1,   1,   80, FLAT, MONS_ORC_SORCERER },
  {  1,   1,   80, FLAT, MONS_ORC_HIGH_PRIEST },
  {  1,   1,   40, FLAT, MONS_STONE_GIANT },
  {  1,   1,   60, FLAT, MONS_OGRE_MAGE },
  { 0,0,0,FLAT,MONS_0 }
};

static const pop_entry _planerend_elf[] =
{ // Elf enemies
  {  1,   1,  100, FLAT, MONS_DEEP_ELF_SORCERER },
  {  1,   1,  100, FLAT, MONS_DEEP_ELF_HIGH_PRIEST },
  {  1,   1,   60, FLAT, MONS_DEEP_ELF_MASTER_ARCHER },
  {  1,   1,   60, FLAT, MONS_DEEP_ELF_BLADEMASTER },
  { 0,0,0,FLAT,MONS_0 }
};

static const pop_entry _planerend_vaults[] =
{ // Vaults enemies
  {  1,   1,   80, FLAT, MONS_VAULT_SENTINEL },
  {  1,   1,   80, FLAT, MONS_DANCING_WEAPON },
  {  1,   1,   60, FLAT, MONS_IRONBRAND_CONVOKER },
  {  1,   1,  100, FLAT, MONS_WAR_GARGOYLE },
  { 0,0,0,FLAT,MONS_0 }
};

static const pop_entry _planerend_crypt[] =
{ // Crypt enemies
  {  1,   1,  100, FLAT, MONS_VAMPIRE_KNIGHT },
  {  1,   1,  100, FLAT, MONS_FLAYED_GHOST },
  {  1,   1,   80, FLAT, MONS_REVENANT },
  {  1,   1,   80, FLAT, MONS_DEEP_ELF_DEATH_MAGE },
  { 0,0,0,FLAT,MONS_0 }
};

static const pop_entry _planerend_tomb[] =
{ // Tomb enemies
  {  1,   1,   60, FLAT, MONS_ANCIENT_CHAMPION },
  {  1,   1,  100, FLAT, MONS_SPHINX },
  {  1,   1,  100, FLAT, MONS_MUMMY_PRIEST },
  { 0,0,0,FLAT,MONS_0 }
};

static const pop_entry _planerend_abyss[] =
{ // Abyss enemies
  {  1,   1,  100, FLAT, MONS_STARCURSED_MASS },
  {  1,   1,   80, FLAT, MONS_APOCALYPSE_CRAB },
  {  1,   1,   50, FLAT, MONS_THRASHING_HORROR },
  {  1,   1,   50, FLAT, MONS_VERY_UGLY_THING },
  { 0,0,0,FLAT,MONS_0 }
};

static const pop_entry _planerend_zot[] =
{ // Zot enemies
  {  1,   1,   80, FLAT, MONS_DRACONIAN_SHIFTER },
  {  1,   1,   80, FLAT, MONS_DRACONIAN_SCORCHER },
  {  1,   1,   80, FLAT, MONS_DRACONIAN_ZEALOT },
  {  1,   1,  100, FLAT, MONS_GOLDEN_DRAGON },
  {  1,   1,  100, FLAT, MONS_MOTH_OF_WRATH },
  { 0,0,0,FLAT,MONS_0 }
};

static branch_summon_pair _planerend_summons[] =
{
  { BRANCH_LAIR,   _planerend_lair },
  { BRANCH_SNAKE,  _planerend_snake },
  { BRANCH_SPIDER, _planerend_spider },
  { BRANCH_SWAMP,  _planerend_swamp },
  { BRANCH_SHOALS, _planerend_shoals },
  { BRANCH_SLIME,  _planerend_slime },
  { BRANCH_ORC,    _planerend_orc },
  { BRANCH_ELF,    _planerend_elf },
  { BRANCH_VAULTS, _planerend_vaults },
  { BRANCH_CRYPT,  _planerend_crypt },
  { BRANCH_TOMB,   _planerend_tomb },
  { BRANCH_ABYSS,  _planerend_abyss },
  { BRANCH_ZOT,    _planerend_zot }
};

static void _branch_summon_helper(monster* mons, spell_type spell_cast,
                                  branch_summon_pair *summon_list,
                                  const size_t list_size, int count)
{
    int which = 0;
    // XXX: should this not depend on the specific spell?
    if (spell_cast == SPELL_FORCEFUL_INVITATION
        && mons->props.exists("invitation_branch"))
    {
        which = mons->props["invitation_branch"].get_byte();
    }
    else
    {
        which = random2(list_size);
        if (spell_cast == SPELL_FORCEFUL_INVITATION)
            mons->props["invitation_branch"].get_byte() = which;
    }

    if (you.see_cell(mons->pos()))
    {
        string msg = getSpeakString("branch summon cast prefix");
        if (!msg.empty())
        {
            msg  = replace_all(msg, "@The_monster@", mons->name(DESC_THE));
            msg += " ";
            msg += branches[summon_list[which].origin].longname;
            msg += "!";
            mprf(mons->wont_attack() ? MSGCH_FRIEND_ENCHANT
                 : MSGCH_MONSTER_ENCHANT, "%s", msg.c_str());
        }
    }

    for (int i = 0; i < count; i++)
    {
        monster_type type = pick_monster_from(summon_list[which].pop, 1);
        if (type == MONS_NO_MONSTER)
            continue;

        create_monster(
            mgen_data(type, SAME_ATTITUDE(mons), mons,
                      1, spell_cast, mons->pos(), mons->foe, 0, GOD_NO_GOD,
                      MONS_NO_MONSTER, type == MONS_SLIME_CREATURE ? 5 : 0));
    }
}

// marking this extern (const defaults to static) so that monster can link to it
extern const spell_type serpent_of_hell_breaths[][3] =
{
    // Geh
    {
        SPELL_FIRE_BREATH,
        SPELL_FIREBALL,
        SPELL_HELLFIRE
    },
    // Coc
    {
        SPELL_COLD_BREATH,
        SPELL_FREEZING_CLOUD,
        SPELL_FLASH_FREEZE
    },
    // Dis
    {
        SPELL_METAL_SPLINTERS,
        SPELL_QUICKSILVER_BOLT,
        SPELL_LEHUDIBS_CRYSTAL_SPEAR
    },
    // Tar
    {
        SPELL_BOLT_OF_DRAINING,
        SPELL_MIASMA_BREATH,
        SPELL_CORROSIVE_BOLT
    }
};

void mons_cast(monster* mons, bolt &pbolt, spell_type spell_cast,
               unsigned short slot_flags, bool do_noise)
{
    if (spell_cast == SPELL_SERPENT_OF_HELL_BREATH)
    {
        ASSERT(mons->type >= MONS_SERPENT_OF_HELL);
        ASSERT(mons->type <= MONS_SERPENT_OF_HELL_TARTARUS);

#if TAG_MAJOR_VERSION > 34
        const int idx = mons->type - MONS_SERPENT_OF_HELL;
#else
        const int idx =
            mons->type == MONS_SERPENT_OF_HELL          ? 0
          : mons->type == MONS_SERPENT_OF_HELL_COCYTUS  ? 1
          : mons->type == MONS_SERPENT_OF_HELL_DIS      ? 2
          : mons->type == MONS_SERPENT_OF_HELL_TARTARUS ? 3
          :                                               -1;
#endif
        ASSERT(idx < (int)ARRAYSZ(serpent_of_hell_breaths));
        ASSERT(idx >= 0);
        ASSERT(mons->heads() == ARRAYSZ(serpent_of_hell_breaths[idx]));

        for (int i = 0; i < mons->heads(); ++i)
        {
            spell_type head_spell = serpent_of_hell_breaths[idx][i];
            setup_mons_cast(mons, pbolt, head_spell);
            mons_cast(mons, pbolt, head_spell, slot_flags, do_noise);
        }

        return;
    }
    // Always do setup.  It might be done already, but it doesn't hurt
    // to do it again (cheap).
    setup_mons_cast(mons, pbolt, spell_cast);

    // single calculation permissible {dlb}
    bool monsterNearby = mons_near(mons);

    int sumcount = 0;
    int sumcount2;
    int duration = 0;

    dprf("Mon #%d casts %s (#%d)",
         mons->mindex(), spell_title(spell_cast), spell_cast);

    bool orig_noise = do_noise;

    if (spell_cast == SPELL_CANTRIP
        || spell_cast == SPELL_VAMPIRIC_DRAINING
        || spell_cast == SPELL_IOOD
        || spell_cast == SPELL_INJURY_MIRROR
        || spell_cast == SPELL_DRAIN_LIFE
        || spell_cast == SPELL_TROGS_HAND
        || spell_cast == SPELL_LEDAS_LIQUEFACTION
        || spell_cast == SPELL_PORTAL_PROJECTILE
        || spell_cast == SPELL_FORCEFUL_INVITATION
        || spell_cast == SPELL_PLANEREND)
    {
        do_noise = false;       // Spell itself does the messaging.
    }

    if (_los_free_spell(spell_cast) && !spell_is_direct_explosion(spell_cast))
    {
        if (mons->foe == MHITYOU || mons->foe == MHITNOT)
        {
            if (monsterNearby)
            {
                if (do_noise)
                    mons_cast_noise(mons, pbolt, spell_cast, slot_flags);
                direct_effect(mons, spell_cast, pbolt, &you);
            }
            return;
        }

        if (do_noise)
            mons_cast_noise(mons, pbolt, spell_cast, slot_flags);
        direct_effect(mons, spell_cast, pbolt, mons->get_foe());
        return;
    }

#ifdef ASSERTS
    const unsigned int flags = get_spell_flags(spell_cast);

    ASSERT(!(flags & SPFLAG_TESTING));

    // Targeted spells need a valid target.
    // Wizard-mode cast monster spells may target the boundary (shift-dir).
    ASSERT(!(flags & SPFLAG_TARGETING_MASK) || map_bounds(pbolt.target));
#endif

    if (do_noise)
        mons_cast_noise(mons, pbolt, spell_cast, slot_flags);

    // If this is a wizard spell, summons won't  necessarily have the
    // same god. But intrinsic/priestly summons should.
    god_type god = slot_flags & MON_SPELL_WIZARD ? GOD_NO_GOD : mons->god;

    // Permanent wizard summons of Yred should have the same god even
    // though they aren't priests. This is so that e.g. the zombies of
    // Yred's enslaved souls will properly turn on you if you abandon
    // Yred.
    if (mons->god == GOD_YREDELEMNUL)
        god = mons->god;

    switch (spell_cast)
    {
    default:
        break;

    case SPELL_MAJOR_HEALING:
        if (mons->heal(50 + random2avg(mons->spell_hd(spell_cast) * 10, 2)))
            simple_monster_message(mons, " is healed.");
        return;

    case SPELL_INJURY_MIRROR:
        simple_monster_message(mons,
                               make_stringf(" offers %s to %s, and fills with unholy energy.",
                                   mons->pronoun(PRONOUN_REFLEXIVE).c_str(),
                                   god_name(mons->god).c_str()).c_str(),
                               MSGCH_MONSTER_SPELL);
        mons->add_ench(mon_enchant(ENCH_MIRROR_DAMAGE, 0, mons, random_range(7, 9) * BASELINE_DELAY));
        mons->props["mirror_recast_time"].get_int() = you.elapsed_time + 150 + random2(60);
        return;

    case SPELL_VAMPIRIC_DRAINING:
        _mons_vampiric_drain(mons);
        return;

    case SPELL_BERSERKER_RAGE:
        mons->props.erase("brothers_count");
        mons->go_berserk(true);
        return;

    case SPELL_FRENZY:
        mons->go_frenzy(mons);
        return;

    case SPELL_TROGS_HAND:
    {
        simple_monster_message(mons,
                               make_stringf(" invokes %s protection!",
                                   apostrophise(god_name(mons->god)).c_str()).c_str(),
                               MSGCH_MONSTER_SPELL);
        // Not spell_hd(spell_cast); this is an invocation
        const int dur = BASELINE_DELAY
            * min(5 + roll_dice(2, (mons->get_hit_dice() * 10) / 3 + 1), 100);
        mons->add_ench(mon_enchant(ENCH_RAISED_MR, 0, mons, dur));
        mons->add_ench(mon_enchant(ENCH_REGENERATION, 0, mons, dur));
        dprf("Trog's Hand cast (dur: %d aut)", dur);
        return;
    }

    case SPELL_SWIFTNESS:
        mons->add_ench(ENCH_SWIFT);
        if (mons->type == MONS_ALLIGATOR)
        {
            mons->swift_cooldown = you.num_turns;
            simple_monster_message(mons, " puts on a burst of speed!");
        }
        else
            simple_monster_message(mons, " seems to move somewhat quicker.");
        return;

    case SPELL_STONESKIN:
    {
        if (you.can_see(mons))
        {
            mprf("%s skin hardens.",
                 apostrophise(mons->name(DESC_THE)).c_str());
        }
        const int power = (mons->spell_hd(spell_cast) * 15) / 10;
        mons->add_ench(mon_enchant(ENCH_STONESKIN, 0, mons,
                       BASELINE_DELAY * (10 + (2 * random2(power)))));
        return;
    }

    case SPELL_SILENCE:
        mons->add_ench(ENCH_SILENCE);
        invalidate_agrid(true);
        simple_monster_message(mons, "'s surroundings become eerily quiet.");
        return;

    case SPELL_CALL_TIDE:
        if (player_in_branch(BRANCH_SHOALS))
        {
            const int tide_duration = BASELINE_DELAY
                * random_range(80, 200, 2);
            mons->add_ench(mon_enchant(ENCH_TIDE, 0, mons,
                                       tide_duration));
            mons->props[TIDE_CALL_TURN].get_int() = you.num_turns;
            if (simple_monster_message(
                    mons,
                    " sings a water chant to call the tide!"))
            {
                flash_view_delay(UA_MONSTER, ETC_WATER, 300);
            }
        }
        return;

    case SPELL_INK_CLOUD:
        if (!feat_is_watery(grd(mons->pos())))
            return;

        big_cloud(CLOUD_INK, mons, mons->pos(), 30, 30);

        simple_monster_message(
            mons,
            " squirts a massive cloud of ink into the water!");
        return;

#if TAG_MAJOR_VERSION == 34
    case SPELL_VAMPIRE_SUMMON:
#endif
    case SPELL_SUMMON_SMALL_MAMMAL:
        sumcount2 = 1 + random2(3);

        for (sumcount = 0; sumcount < sumcount2; ++sumcount)
        {
            monster_type rats[] = { MONS_QUOKKA, MONS_GREEN_RAT, MONS_RAT };

            const monster_type mon = (one_chance_in(3) ? MONS_BAT
                                                       : RANDOM_ELEMENT(rats));
            create_monster(
                mgen_data(mon, SAME_ATTITUDE(mons), mons,
                          5, spell_cast, mons->pos(), mons->foe, 0, god));
        }
        return;

    case SPELL_STICKS_TO_SNAKES:
    {
        const int pow = (mons->spell_hd(spell_cast) * 15) / 10;
        int cnt = 1 + random2(1 + pow / 4);
        monster_type sum;
        for (int i = 0; i < cnt; i++)
        {
            if (random2(mons->spell_hd(spell_cast)) > 27
                || one_chance_in(5 - min(4, div_rand_round(pow * 2, 25))))
            {
                sum = x_chance_in_y(pow / 3, 100) ? MONS_WATER_MOCCASIN
                                                  : MONS_ADDER;
            }
            else
                sum = MONS_BALL_PYTHON;

            if (create_monster(
                    mgen_data(sum, SAME_ATTITUDE(mons), mons,
                              5, spell_cast, mons->pos(), mons->foe,
                              0, god)))
            {
                i++;
            }
        }
        return;
    }

    case SPELL_SHADOW_CREATURES:       // summon anything appropriate for level
    case SPELL_WEAVE_SHADOWS:
    {
        if (spell_cast == SPELL_SHADOW_CREATURES
            && _mons_abjured(mons, monsterNearby))
        {
            return;
        }

        level_id place = (spell_cast == SPELL_SHADOW_CREATURES)
                         ? level_id::current()
                         : level_id(BRANCH_DUNGEON,
                                    min(27, max(1, mons->spell_hd(spell_cast))));

        sumcount2 = 1 + random2(mons->spell_hd(spell_cast) / 5 + 1);

        for (sumcount = 0; sumcount < sumcount2; ++sumcount)
        {
            create_monster(
                mgen_data(RANDOM_MOBILE_MONSTER, SAME_ATTITUDE(mons), mons,
                          5, spell_cast, mons->pos(), mons->foe, 0, god,
                          MONS_NO_MONSTER, 0, BLACK, PROX_ANYWHERE, place));
        }
        return;
    }

    case SPELL_WATER_ELEMENTALS:
        _mons_summon_elemental(mons, monsterNearby, spell_cast,
                               MONS_WATER_ELEMENTAL, god);
        return;

    case SPELL_EARTH_ELEMENTALS:
        _mons_summon_elemental(mons, monsterNearby, spell_cast,
                               MONS_EARTH_ELEMENTAL, god);
        return;

    case SPELL_IRON_ELEMENTALS:
        _mons_summon_elemental(mons, monsterNearby, spell_cast,
                               MONS_IRON_ELEMENTAL, god);
        return;

    case SPELL_AIR_ELEMENTALS:
        _mons_summon_elemental(mons, monsterNearby, spell_cast,
                               MONS_AIR_ELEMENTAL, god);
        return;

    case SPELL_FIRE_ELEMENTALS:
        _mons_summon_elemental(mons, monsterNearby, spell_cast,
                               MONS_FIRE_ELEMENTAL, god);
        return;

    case SPELL_SUMMON_ILLUSION:
        _mons_cast_summon_illusion(mons, spell_cast);
        return;

    case SPELL_CREATE_TENTACLES:
        _mons_create_tentacles(mons);
        return;

    case SPELL_FAKE_MARA_SUMMON:
        // We only want there to be two fakes, which, plus Mara, means
        // a total of three Maras; if we already have two, give up, otherwise
        // we want to summon either one more or two more.
        sumcount2 = 2 - count_summons(mons, SPELL_FAKE_MARA_SUMMON);
        if (sumcount2 <= 0)
            return;

        for (sumcount = 0; sumcount < sumcount2; sumcount++)
            cast_phantom_mirror(mons, mons, 50, SPELL_FAKE_MARA_SUMMON);

        if (you.can_see(mons))
        {
            mprf("%s shimmers and seems to become %s!", mons->name(DESC_THE).c_str(),
                                                        sumcount2 == 1 ? "two"
                                                                       : "three");
        }

        return;

    case SPELL_SUMMON_DEMON: // class 2-4 demons
        if (_mons_abjured(mons, monsterNearby))
            return;

        sumcount2 = 1 + random2(mons->spell_hd(spell_cast) / 10 + 1);

        duration  = min(2 + mons->spell_hd(spell_cast) / 10, 6);
        for (sumcount = 0; sumcount < sumcount2; sumcount++)
        {
            create_monster(
                mgen_data(summon_any_demon(RANDOM_DEMON_COMMON),
                          SAME_ATTITUDE(mons), mons, duration, spell_cast,
                          mons->pos(), mons->foe, 0, god));
        }
        return;

    case SPELL_MONSTROUS_MENAGERIE:
        if (_mons_abjured(mons, monsterNearby))
            return;

        cast_monstrous_menagerie(mons, mons->spell_hd(spell_cast) * 6, mons->god);
        return;

    case SPELL_ANIMATE_DEAD:
        animate_dead(mons, 5 + random2(5), SAME_ATTITUDE(mons),
                     mons->foe, mons, "", god);
        return;

    case SPELL_TWISTED_RESURRECTION:
        // Double efficiency compared to maxed out player spell: one
        // elf corpse gives 4.5 HD.
        twisted_resurrection(mons, 500, SAME_ATTITUDE(mons),
                             mons->foe, god);
        return;

    case SPELL_SIMULACRUM:
        monster_simulacrum(mons, true);
        return;

    case SPELL_CALL_IMP:
        duration  = min(2 + mons->spell_hd(spell_cast) / 5, 6);
            create_monster(
                mgen_data(random_choose_weighted(
                            1, MONS_IRON_IMP,
                            2, MONS_SHADOW_IMP,
                            2, MONS_WHITE_IMP,
                            4, MONS_CRIMSON_IMP,
                            0),
                          SAME_ATTITUDE(mons), mons,
                          duration, spell_cast, mons->pos(), mons->foe, 0,
                          god));
        return;

    case SPELL_SUMMON_MINOR_DEMON: // class 5 demons
        sumcount2 = 1 + random2(3);

        duration  = min(2 + mons->spell_hd(spell_cast) / 5, 6);
        for (sumcount = 0; sumcount < sumcount2; ++sumcount)
        {
            create_monster(
                mgen_data(summon_any_demon(RANDOM_DEMON_LESSER),
                          SAME_ATTITUDE(mons), mons,
                          duration, spell_cast, mons->pos(), mons->foe, 0,
                          god));
        }
        return;

    case SPELL_SUMMON_SWARM:
        _do_high_level_summon(mons, monsterNearby, spell_cast,
                              _pick_swarmer, random_range(3, 6), god,
                              NULL, NULL, false);
        return;

    case SPELL_SUMMON_UFETUBUS:
        sumcount2 = 2 + random2(2);

        duration  = min(2 + mons->spell_hd(spell_cast) / 5, 6);

        for (sumcount = 0; sumcount < sumcount2; ++sumcount)
        {
            create_monster(
                mgen_data(MONS_UFETUBUS, SAME_ATTITUDE(mons), mons,
                          duration, spell_cast, mons->pos(), mons->foe, 0,
                          god));
        }
        return;

    case SPELL_SUMMON_HELL_BEAST:  // Geryon
        create_monster(
            mgen_data(MONS_HELL_BEAST, SAME_ATTITUDE(mons), mons,
                      4, spell_cast, mons->pos(), mons->foe, 0, god));
        return;

    case SPELL_SUMMON_ICE_BEAST:
        // Zotdef: reduce ice beast frequency, and reduce duration to 3
        if (!crawl_state.game_is_zotdef() || !one_chance_in(3))
        {
            int dur = crawl_state.game_is_zotdef() ? 3 : 5;
            create_monster(
                mgen_data(MONS_ICE_BEAST, SAME_ATTITUDE(mons), mons,
                          dur, spell_cast, mons->pos(), mons->foe, 0, god));
        }
        return;

    case SPELL_SUMMON_MUSHROOMS:   // Summon a ring of icky crawling fungi.
        if (_mons_abjured(mons, monsterNearby))
            return;

        sumcount2 = 2 + random2(mons->spell_hd(spell_cast) / 4 + 1);
        duration  = min(2 + mons->spell_hd(spell_cast) / 5, 6);
        for (int i = 0; i < sumcount2; ++i)
        {
            // Attempt to place adjacent to target first, and only at a wider
            // radius if no adjacent spots can be found
            coord_def empty;
            find_habitable_spot_near(mons->get_foe()->pos(),
                                     MONS_WANDERING_MUSHROOM, 1, false, empty);
            if (empty.origin())
            {
                find_habitable_spot_near(mons->get_foe()->pos(),
                                         MONS_WANDERING_MUSHROOM, 2, false, empty);
            }

            // Can't find any room, so stop trying
            if (empty.origin())
                return;

            create_monster(
                mgen_data(one_chance_in(3) ? MONS_DEATHCAP
                                           : MONS_WANDERING_MUSHROOM,
                          SAME_ATTITUDE(mons), mons, duration, spell_cast,
                          empty, mons->foe, MG_FORCE_PLACE, god));
        }
        return;

    case SPELL_SUMMON_HORRIBLE_THINGS:
        _do_high_level_summon(mons, monsterNearby, spell_cast,
                              _pick_horrible_thing, random_range(3, 5), god);
        return;

    case SPELL_MALIGN_GATEWAY:
        if (!can_cast_malign_gateway())
        {
            dprf("ERROR: %s can't cast malign gateway, but is casting anyway! "
                 "Counted %d gateways.", mons->name(DESC_THE).c_str(),
                 count_malign_gateways());
        }
        cast_malign_gateway(mons, 200);
        return;

    case SPELL_CONJURE_BALL_LIGHTNING:
    {
        const int n = min(8, 2 + random2avg(mons->spell_hd(spell_cast) / 4, 2));
        for (int i = 0; i < n; ++i)
        {
            if (monster *ball = create_monster(
                    mgen_data(MONS_BALL_LIGHTNING, SAME_ATTITUDE(mons),
                              mons, 0, spell_cast, mons->pos(), mons->foe,
                              0, god)))
            {
                ball->add_ench(ENCH_SHORT_LIVED);
            }
        }
        return;
    }

    case SPELL_SUMMON_UNDEAD:
        _do_high_level_summon(mons, monsterNearby, spell_cast,
                              _pick_undead_summon,
                              2 + random2(mons->spell_hd(spell_cast) / 5 + 1), god);
        return;

    case SPELL_BROTHERS_IN_ARMS:
    {
        // Invocation; don't use spell_hd
        const int power = (mons->get_hit_dice() * 20)
                          + random2(mons->get_hit_dice() * 5)
                          - random2(mons->get_hit_dice() * 5);
        monster_type to_summon;

        if (mons->type == MONS_SPRIGGAN_BERSERKER)
        {
            monster_type berserkers[] = { MONS_POLAR_BEAR, MONS_ELEPHANT,
                                          MONS_DEATH_YAK };
            to_summon = RANDOM_ELEMENT(berserkers);
        }
        else
        {
            monster_type berserkers[] = { MONS_BLACK_BEAR, MONS_OGRE, MONS_TROLL,
                                           MONS_HILL_GIANT, MONS_DEEP_TROLL,
                                           MONS_TWO_HEADED_OGRE};
            to_summon = RANDOM_ELEMENT(berserkers);
        }

        summon_berserker(power, mons, to_summon);
        mons->props["brothers_count"].get_int()++;
        return;
    }

    case SPELL_SYMBOL_OF_TORMENT:
        torment(mons, mons->mindex(), mons->pos());
        return;

    case SPELL_MESMERISE:
        _mons_mesmerise(mons);
        return;

    case SPELL_CAUSE_FEAR:
        _mons_cause_fear(mons);
        return;

    case SPELL_DRAIN_LIFE:
        cast_los_attack_spell(spell_cast, mons->spell_hd(spell_cast), mons, true);
        return;

    case SPELL_OZOCUBUS_REFRIGERATION:
        cast_los_attack_spell(spell_cast, mons->spell_hd(spell_cast) * 5,
                              mons, true);
        return;

    case SPELL_OLGREBS_TOXIC_RADIANCE:
        cast_toxic_radiance(mons, mons->spell_hd(spell_cast) * 8);
        return;

    case SPELL_LRD:
    {
        if (in_bounds(pbolt.target))
        {
           cast_fragmentation(6 * mons->spell_hd(spell_cast), mons,
                              pbolt.target, false);
        }
        else if (you.can_see(mons))
            canned_msg(MSG_NOTHING_HAPPENS);

        return;
    }

    case SPELL_SHATTER:
        mons_shatter(mons);
        return;

    case SPELL_LEDAS_LIQUEFACTION:
        if (!mons->has_ench(ENCH_LIQUEFYING) && you.can_see(mons))
        {
            mprf("%s liquefies the ground around %s!", mons->name(DESC_THE).c_str(),
                 mons->pronoun(PRONOUN_REFLEXIVE).c_str());
            flash_view_delay(UA_MONSTER, BROWN, 80);
        }

        mons->add_ench(ENCH_LIQUEFYING);
        invalidate_agrid(true);
        return;

    case SPELL_CORPSE_ROT:
        corpse_rot(mons);
        return;

    case SPELL_SUMMON_GREATER_DEMON:
        if (_mons_abjured(mons, monsterNearby))
            return;

        duration  = min(2 + mons->spell_hd(spell_cast) / 10, 6);

        create_monster(
            mgen_data(summon_any_demon(RANDOM_DEMON_GREATER),
                      SAME_ATTITUDE(mons), mons,
                      duration, spell_cast,
                      mons->pos(), mons->foe, 0, god));
        return;

    // Journey -- Added in Summon Lizards and Draconians
    case SPELL_SUMMON_DRAKES:
        if (_mons_abjured(mons, monsterNearby))
            return;

        sumcount2 = 1 + random2(mons->spell_hd(spell_cast) / 5 + 1);

        duration  = min(2 + mons->spell_hd(spell_cast) / 10, 6);

        {
            vector<monster_type> monsters;

            for (sumcount = 0; sumcount < sumcount2; ++sumcount)
            {
                bool drag = false;
                monster_type mon = summon_any_dragon(DRAGON_LIZARD);

                if (mon == MONS_DRAGON)
                {
                    drag = true;
                    mon = summon_any_dragon(DRAGON_DRAGON);
                }

                monsters.push_back(mon);

                if (drag)
                    break;
            }

            for (unsigned int i = 0, size = monsters.size(); i < size; ++i)
            {
                create_monster(
                    mgen_data(monsters[i], SAME_ATTITUDE(mons), mons,
                              duration, spell_cast,
                              mons->pos(), mons->foe, 0, god));
            }
        }
        return;

    case SPELL_DRUIDS_CALL:
        _cast_druids_call(mons);
        return;

    case SPELL_BATTLESPHERE:
        cast_battlesphere(mons, min(6 * mons->spell_hd(spell_cast), 200), mons->god, false);
        return;

    case SPELL_SPECTRAL_WEAPON:
        cast_spectral_weapon(mons, min(6 * mons->spell_hd(spell_cast), 200), mons->god, false);
        return;

    case SPELL_TORNADO:
    {
        int dur = 60;
        if (you.can_see(mons))
        {
            bool flying = mons->flight_mode();
            mprf("A great vortex of raging winds appears %s%s%s!",
                 flying ? "around " : "and lifts ",
                 mons->name(DESC_THE).c_str(),
                 flying ? "" : " up!");
        }
        else if (you.see_cell(mons->pos()))
            mpr("A great vortex of raging winds appears out of thin air!");
        mons->props["tornado_since"].get_int() = you.elapsed_time;
        mon_enchant me(ENCH_TORNADO, 0, mons, dur);
        mons->add_ench(me);
        if (mons->has_ench(ENCH_FLIGHT))
        {
            mon_enchant me2 = mons->get_ench(ENCH_FLIGHT);
            me2.duration = me.duration;
            mons->update_ench(me2);
        }
        else
            mons->add_ench(mon_enchant(ENCH_FLIGHT, 0, mons, dur));
        return;
    }

    case SPELL_EPHEMERAL_INFUSION:
        _cast_ephemeral_infusion(mons);
        return;

    // TODO: Outsource the cantrip messages and allow specification of
    //       special cantrip spells per monster, like for speech, both as
    //       "self buffs" and "player enchantments".
    case SPELL_CANTRIP:
    {
        // Monster spell of uselessness, just prints a message.
        // This spell exists so that some monsters with really strong
        // spells (ie orc priest) can be toned down a bit. -- bwr
        //
        // XXX: Needs expansion, and perhaps different priest/mage flavours.

        // Don't give any message if the monster isn't nearby.
        // (Otherwise you could get them from halfway across the level.)
        if (!mons_near(mons))
            return;

        const bool friendly  = mons->friendly();
        const bool buff_only = !friendly && is_sanctuary(you.pos());
        const msg_channel_type channel = (friendly) ? MSGCH_FRIEND_ENCHANT
                                                    : MSGCH_MONSTER_ENCHANT;

        if (mons->type == MONS_GASTRONOK)
        {
            bool has_mon_foe = !invalid_monster_index(mons->foe);
            string slugform = "";
            if (buff_only || crawl_state.game_is_arena() && !has_mon_foe
                || friendly && !has_mon_foe || coinflip())
            {
                slugform = getSpeakString("gastronok_self_buff");
                if (!slugform.empty())
                {
                    slugform = replace_all(slugform, "@The_monster@",
                                           mons->name(DESC_THE));
                    mprf(channel, "%s", slugform.c_str());
                }
            }
            else if (!friendly && !has_mon_foe)
            {
                mons_cast_noise(mons, pbolt, spell_cast, slot_flags);

                // "Enchant" the player.
                slugform = getSpeakString("gastronok_debuff");
                if (!slugform.empty()
                    && (slugform.find("legs") == string::npos
                        || _legs_msg_applicable()))
                {
                    mpr(slugform.c_str());
                }
            }
            else
            {
                // "Enchant" another monster.
                const monster* foe      = mons->get_foe()->as_monster();
                slugform = getSpeakString("gastronok_other_buff");
                if (!slugform.empty())
                {
                    slugform = replace_all(slugform, "@The_monster@",
                                           foe->name(DESC_THE));
                    mprf(MSGCH_MONSTER_ENCHANT, "%s", slugform.c_str());
                }
            }
        }
        else
        {
            // Messages about the monster influencing itself.
            const char* buff_msgs[] =
            {
                " glows brightly for a moment.",
                " looks braver.",
                " becomes somewhat translucent.",
                "'s eyes start to glow.",
            };

            // Messages about the monster influencing you.
            const char* other_msgs[] =
            {
                "You feel troubled.",
                "You feel a wave of unholy energy pass over you.",
            };

            if (buff_only || crawl_state.game_is_arena() || x_chance_in_y(2,3))
            {
                simple_monster_message(mons, RANDOM_ELEMENT(buff_msgs),
                                       channel);
            }
            else if (friendly)
            {
                simple_monster_message(mons, " shimmers for a moment.",
                                       channel);
            }
            else // "Enchant" the player.
            {
                mons_cast_noise(mons, pbolt, spell_cast, slot_flags);
                mpr(RANDOM_ELEMENT(other_msgs));
            }
        }
        return;
    }
    case SPELL_BLINK_OTHER:
    {
        // Allow the caster to comment on moving the foe.
        string msg = getSpeakString(mons->name(DESC_PLAIN) + " blink_other");
        if (!msg.empty() && msg != "__NONE")
        {
            mons_speaks_msg(mons, msg, MSGCH_TALK,
                            silenced(you.pos()) || silenced(mons->pos()));
        }
        break;
    }
    case SPELL_BLINK_OTHER_CLOSE:
    {
        // Allow the caster to comment on moving the foe.
        string msg = getSpeakString(mons->name(DESC_PLAIN)
                                    + " blink_other_close");
        if (!msg.empty() && msg != "__NONE")
        {
            mons_speaks_msg(mons, msg, MSGCH_TALK,
                            silenced(you.pos()) || silenced(mons->pos()));
        }
        break;
    }
    case SPELL_TOMB_OF_DOROKLOHE:
    {
        sumcount = 0;

        const int hp_lost = mons->max_hit_points - mons->hit_points;

        if (!hp_lost)
            sumcount++;

        const dungeon_feature_type safe_tiles[] =
        {
            DNGN_SHALLOW_WATER, DNGN_FLOOR, DNGN_OPEN_DOOR,
        };

        bool proceed;

        for (adjacent_iterator ai(mons->pos()); ai; ++ai)
        {
            const actor* act = actor_at(*ai);

            // We can blink away the crowd, but only our allies.
            if (act
                && (act->is_player()
                    || (act->is_monster()
                        && act->as_monster()->attitude != mons->attitude)))
            {
                sumcount++;
            }

            // Make sure we have a legitimate tile.
            proceed = false;
            for (unsigned int i = 0; i < ARRAYSZ(safe_tiles) && !proceed; ++i)
                if (grd(*ai) == safe_tiles[i] || feat_is_trap(grd(*ai)))
                    proceed = true;

            if (!proceed && feat_is_reachable_past(grd(*ai)))
                sumcount++;
        }

        if (sumcount)
        {
            mons->blink();
            return;
        }

        sumcount = 0;
        for (adjacent_iterator ai(mons->pos()); ai; ++ai)
        {
            if (monster_at(*ai))
            {
                monster_at(*ai)->blink();
                if (monster_at(*ai))
                {
                    monster_at(*ai)->teleport(true);
                    if (monster_at(*ai))
                        continue;
                }
            }

            // Make sure we have a legitimate tile.
            proceed = false;
            for (unsigned int i = 0; i < ARRAYSZ(safe_tiles) && !proceed; ++i)
                if (grd(*ai) == safe_tiles[i] || feat_is_trap(grd(*ai)))
                    proceed = true;

            if (proceed)
            {
                // All items are moved inside.
                if (igrd(*ai) != NON_ITEM)
                    move_items(*ai, mons->pos());

                // All clouds are destroyed.
                if (env.cgrid(*ai) != EMPTY_CLOUD)
                    delete_cloud(env.cgrid(*ai));

                // All traps are destroyed.
                if (trap_def *ptrap = find_trap(*ai))
                    ptrap->destroy();

                // Actually place the wall.
                temp_change_terrain(*ai, DNGN_ROCK_WALL, INFINITE_DURATION,
                                    TERRAIN_CHANGE_TOMB, mons);
                sumcount++;
            }
        }

        if (sumcount)
        {
            mpr("Walls emerge from the floor!");

            // XXX: Assume that the entombed monster can regenerate.
            // Also, base the regeneration rate on HD to avoid
            // randomness.
            const int tomb_duration = BASELINE_DELAY
                * hp_lost * max(1, mons->spell_hd(spell_cast) / 3);
            int mon_index = mons->mindex();
            env.markers.add(new map_tomb_marker(mons->pos(),
                                                tomb_duration,
                                                mon_index,
                                                mon_index));
            env.markers.clear_need_activate(); // doesn't need activation
        }
        return;
    }
    case SPELL_CHAIN_LIGHTNING:
    case SPELL_CHAIN_OF_CHAOS:
        cast_chain_spell(spell_cast, 4 * mons->spell_hd(spell_cast), mons);
        return;
    case SPELL_SUMMON_EYEBALLS:
        if (mons->type != MONS_DISSOLUTION
            && _mons_abjured(mons, monsterNearby))
        {
            return;
        }

        sumcount2 = 1 + random2(mons->spell_hd(spell_cast) / 7 + 1);

        duration = min(2 + mons->spell_hd(spell_cast) / 10, 6);

        for (sumcount = 0; sumcount < sumcount2; sumcount++)
        {
            const monster_type mon = random_choose_weighted(
                                       100, MONS_GIANT_EYEBALL,
                                        80, MONS_EYE_OF_DRAINING,
                                        60, MONS_GOLDEN_EYE,
                                        40, MONS_SHINING_EYE,
                                        20, MONS_GREAT_ORB_OF_EYES,
                                        10, MONS_EYE_OF_DEVASTATION,
                                         0);

            create_monster(
                mgen_data(mon, SAME_ATTITUDE(mons), mons, duration,
                          spell_cast, mons->pos(), mons->foe, 0, god));
        }
        return;
    case SPELL_SUMMON_BUTTERFLIES:
        duration = min(2 + mons->spell_hd(spell_cast) / 5, 6);
        for (int i = 0; i < 10; ++i)
        {
            create_monster(
                mgen_data(MONS_BUTTERFLY, SAME_ATTITUDE(mons),
                          mons, duration, spell_cast, mons->pos(),
                          mons->foe, 0, god));
        }
        return;
    case SPELL_IOOD:
        if (mons->type == MONS_ORB_SPIDER && !mons->has_ench(ENCH_IOOD_CHARGED))
        {
            mons->add_ench(ENCH_IOOD_CHARGED);

            if (!monsterNearby)
                return;
            string msg = getSpeakString("orb spider charge");
            if (!msg.empty())
            {
                msg = replace_all(msg, "@The_monster@", mons->name(DESC_THE));
                mprf(mons->wont_attack() ? MSGCH_FRIEND_ENCHANT
                     : MSGCH_MONSTER_ENCHANT, "%s", msg.c_str());
            }
            return;
        }
        mons->del_ench(ENCH_IOOD_CHARGED);
        if (orig_noise)
            mons_cast_noise(mons, pbolt, spell_cast, slot_flags);
        cast_iood(mons, 6 * mons->spell_hd(spell_cast), &pbolt);
        return;
    case SPELL_AWAKEN_FOREST:
        duration = 50 + random2(mons->spell_hd(spell_cast) * 20);

        mons->add_ench(mon_enchant(ENCH_AWAKEN_FOREST, 0, mons, duration));
        // Actually, it's a boolean marker... save for a sanity check.
        env.forest_awoken_until = you.elapsed_time + duration;

        // You may be unable to see the monster, but notice an affected tree.
        forest_message(mons->pos(), "The forest starts to sway and rumble!");
        return;

    case SPELL_SUMMON_DRAGON:
        if (_mons_abjured(mons, monsterNearby))
            return;

        cast_summon_dragon(mons, mons->spell_hd(spell_cast) * 5, god);
        return;
    case SPELL_SUMMON_HYDRA:
        if (_mons_abjured(mons, monsterNearby))
            return;

        cast_summon_hydra(mons, mons->spell_hd(spell_cast) * 5, god);
        return;
    case SPELL_FIRE_SUMMON:
        if (_mons_abjured(mons, monsterNearby))
            return;

        sumcount2 = 1 + random2(mons->spell_hd(spell_cast) / 5 + 1);

        duration = min(2 + mons->spell_hd(spell_cast) / 10, 6);

        for (sumcount = 0; sumcount < sumcount2; sumcount++)
        {
            const monster_type mon = random_choose_weighted(
                                       3, MONS_EFREET,
                                       3, MONS_SUN_DEMON,
                                       3, MONS_BALRUG,
                                       2, MONS_HELLION,
                                       1, MONS_BRIMSTONE_FIEND,
                                       0);

            create_monster(
                mgen_data(mon, SAME_ATTITUDE(mons), mons, duration,
                          spell_cast, mons->pos(), mons->foe, 0, god));
        }
        return;

    case SPELL_DEATHS_DOOR:
        if (!mons->has_ench(ENCH_DEATHS_DOOR))
        {
            const int dur = BASELINE_DELAY * 2 * mons->skill(SK_NECROMANCY);
            simple_monster_message(mons,
                                   " stands defiantly in death's doorway!");
            mons->hit_points = max(min(mons->hit_points,
                                       mons->skill(SK_NECROMANCY)), 1);
            mons->add_ench(mon_enchant(ENCH_DEATHS_DOOR, 0, mons, dur));
        }
        return;

    case SPELL_REGENERATION:
    {
        simple_monster_message(mons,
                               "'s wounds begin to heal before your eyes!");
        const int dur = BASELINE_DELAY
            * min(5 + roll_dice(2, (mons->spell_hd(spell_cast) * 10) / 3 + 1), 100);
        mons->add_ench(mon_enchant(ENCH_REGENERATION, 0, mons, dur));
        return;
    }

    case SPELL_OZOCUBUS_ARMOUR:
    {
        if (you.can_see(mons))
        {
            mprf("A film of ice covers %s body!",
                 apostrophise(mons->name(DESC_THE)).c_str());
        }
        const int power = (mons->spell_hd(spell_cast) * 15) / 10;
        mons->add_ench(mon_enchant(ENCH_OZOCUBUS_ARMOUR,
                                   20 + random2(power) + random2(power),
                                   mons));

        return;
    }

    case SPELL_WORD_OF_RECALL:
    {
        mon_enchant recall_timer =
            mon_enchant(ENCH_WORD_OF_RECALL, 1, mons, 30);
        mons->add_ench(recall_timer);
        mons->speed_increment -= 30;

        return;
    }

    case SPELL_INJURY_BOND:
    {
        simple_monster_message(mons,
            make_stringf(" begins to accept %s allies' injuries.",
                         mons->pronoun(PRONOUN_POSSESSIVE).c_str()).c_str());
        // FIXME: allies preservers vs the player
        for (monster_near_iterator mi(mons, LOS_NO_TRANS); mi; ++mi)
        {
            if (mons_aligned(mons, *mi) && !mi->has_ench(ENCH_CHARM)
                && *mi != mons)
            {
                mon_enchant bond = mon_enchant(ENCH_INJURY_BOND, 1, mons,
                                               40 + random2(80));
                mi->add_ench(bond);
            }
        }

        return;
    }

    case SPELL_CALL_LOST_SOUL:
        create_monster(mgen_data(MONS_LOST_SOUL, SAME_ATTITUDE(mons),
                                 mons, 2, spell_cast, mons->pos(),
                                 mons->foe, 0, god));
        return;

    case SPELL_BLINK_ALLIES_ENCIRCLE:
        _blink_allies_encircle(mons);
        return;

    case SPELL_MASS_CONFUSION:
        _mons_mass_confuse(mons);
        return;

    case SPELL_ENGLACIATION:
        if (you.can_see(mons))
            simple_monster_message(mons, " radiates an aura of cold.");
        else if (mons->see_cell_no_trans(you.pos()))
            mpr("A wave of cold passes over you.");
        apply_area_visible(englaciate, min(12 * mons->spell_hd(spell_cast), 200), mons);
        return;

    case SPELL_AWAKEN_VINES:
        _awaken_vines(mons);
        return;

    case SPELL_CONTROL_WINDS:
        if (you.can_see(mons))
            mprf("The winds start to flow at %s will.", mons->name(DESC_ITS).c_str());
        mons->add_ench(mon_enchant(ENCH_CONTROL_WINDS, 1, mons, 200 + random2(150)));
        return;

    case SPELL_WALL_OF_BRAMBLES:
        // If we can't cast this for some reason (can be expensive to determine
        // at every call to _ms_waste_of_time), refund the energy for it so that
        // the caster can do something else
        if (!_wall_of_brambles(mons))
        {
            mons->speed_increment +=
                get_monster_data(mons->type)->energy_usage.spell;
        }
        return;

    case SPELL_HASTE_PLANTS:
    {
        int num = 2 + random2(3);
        for (monster_near_iterator mi(mons, LOS_NO_TRANS); mi && num > 0; ++mi)
        {
            if (mons_aligned(*mi, mons)
                && _is_hastable_plant(*mi)
                && !mi->has_ench(ENCH_HASTE))
            {
                mi->add_ench(ENCH_HASTE);
                simple_monster_message(*mi, " seems to speed up.");
                --num;
            }
        }
        return;
    }

    case SPELL_WIND_BLAST:
    {
        actor *foe = mons->get_foe();
        if (foe && mons->can_see(foe))
        {
            simple_monster_message(mons, " summons a great blast of wind!");
            wind_blast(mons, 12 * mons->spell_hd(spell_cast), foe->pos());
        }
        return;
    }

    case SPELL_FREEZE:
        _mons_cast_freeze(mons);
        return;

    case SPELL_SUMMON_VERMIN:
        _do_high_level_summon(mons, monsterNearby, spell_cast,
                              _pick_vermin, one_chance_in(4) ? 3 : 2 , god);
        return;

    case SPELL_DISCHARGE:
    {
        const int power = min(200, mons->spell_hd(spell_cast) * 12);
        const int num_targs = 1 + random2(random_range(1, 3) + power / 20);
        const int dam = apply_random_around_square(discharge_monsters,
                                                   mons->pos(), true, power,
                                                   num_targs, mons);
        if (dam == 0)
        {
            if (!you.can_see(mons))
                mpr("You hear crackling.");
            else if (coinflip())
            {
                mprf("The air around %s crackles with electrical energy.",
                     mons->name(DESC_THE).c_str());
            }
            else
            {
                const bool plural = coinflip();
                mprf("%s blue arc%s ground%s harmlessly %s %s.",
                     plural ? "Some" : "A",
                     plural ? "s" : "",
                     plural ? " themselves" : "s itself",
                     plural ? "around" : (coinflip() ? "beside" :
                                          coinflip() ? "behind" : "before"),
                     mons->name(DESC_THE).c_str());
            }
        }
        return;
    }
    case SPELL_PORTAL_PROJECTILE:
    {
        // Swap weapons if necessary so that that happens before the spell
        // casting message.
        item_def *launcher = NULL;
        mons_usable_missile(mons, &launcher);
        const item_def *weapon = mons->mslot_item(MSLOT_WEAPON);
        if (launcher && launcher != weapon)
            mons->swap_weapons();
        mons_cast_noise(mons, pbolt, spell_cast, slot_flags);
        handle_throw(mons, pbolt, true, false);
        return;
    }

    case SPELL_IGNITE_POISON:
        cast_ignite_poison(mons, mons->spell_hd(spell_cast) * 6, false);
        return;

    case SPELL_LEGENDARY_DESTRUCTION:
    {
        mons->hurt(mons, 10);
        if (pbolt.origin_spell == SPELL_IOOD)
        {
            cast_iood(mons, 6 * mons->spell_hd(spell_cast), &pbolt);
            return;
        }
        // Don't return yet, we want to actually fire the random beam later
        break;
    }

    case SPELL_FORCEFUL_INVITATION:
        _branch_summon_helper(mons, spell_cast, _invitation_summons,
                              ARRAYSZ(_invitation_summons), 1 + random2(3));
        return;

    case SPELL_PLANEREND:
        _branch_summon_helper(mons, spell_cast, _planerend_summons,
                              ARRAYSZ(_planerend_summons), 1 + random2(3));
        return;

    case SPELL_BLACK_MARK:
        _cast_black_mark(mons);
        return;

    case SPELL_GRAND_AVATAR:
    {
        duration = min(2 + mons->spell_hd(spell_cast) / 10, 6);

        monster* avatar =
            create_monster(
                mgen_data(MONS_GRAND_AVATAR, SAME_ATTITUDE(mons), mons,
                          duration, spell_cast, mons->pos(), mons->foe, 0,
                          god, MONS_NO_MONSTER, 0, BLACK, PROX_ANYWHERE,
                          level_id::current(), mons->spell_hd(spell_cast)));
        if (avatar)
        {
            simple_monster_message(mons, " calls forth a grand avatar!");
            mons->add_ench(mon_enchant(ENCH_GRAND_AVATAR, 1, avatar));
            for (monster_near_iterator mi(mons, LOS_NO_TRANS); mi; ++mi)
            {
                if (*mi != mons && mons_aligned(mons, *mi)
                    && !mi->has_ench(ENCH_CHARM)
                    && !mons_is_avatar((*mi)->type))
                {
                    mi->add_ench(mon_enchant(ENCH_GRAND_AVATAR, 1, avatar));
                }
            }
        }
        return;
    }
#if TAG_MAJOR_VERSION == 34
    case SPELL_REARRANGE_PIECES:
    {
        bool did_message = false;
        vector<actor* > victims;
        for (actor_near_iterator ai(mons, LOS_NO_TRANS); ai; ++ai)
            victims.push_back(*ai);
        shuffle_array(victims);
        for (vector<actor* >::iterator it = victims.begin();
             it != victims.end(); it++)
        {
            actor* victim1 = *it;
            it++;
            if (it == victims.end())
                break;
            actor* victim2 = *it;
            if (victim1->is_player())
                swap_with_monster(victim2->as_monster());
            else if (victim2->is_player())
                swap_with_monster(victim1->as_monster());
            else
            {
                if (!did_message
                    && (you.can_see(victim1)
                        || you.can_see(victim2)))
                {
                    mpr("Some monsters swap places.");
                    did_message = true;
                }

                swap_monsters(victim1->as_monster(), victim2->as_monster());
            }
        }
        return;
    }
#endif

    case SPELL_BLINK_ALLIES_AWAY:
        _blink_allies_away(mons);
        return;

    case SPELL_SHROUD_OF_GOLUBRIA:
        if (you.can_see(mons))
        {
            mprf("Space distorts along a thin shroud covering %s body.",
                 apostrophise(mons->name(DESC_THE)).c_str());
        }
        mons->add_ench(mon_enchant(ENCH_SHROUD));
        return;

    case SPELL_DAZZLING_SPRAY:
    {
        vector<bolt> beams = get_spray_rays(mons, pbolt.target, pbolt.range, 3,
                                            ZAP_DAZZLING_SPRAY);
        for (unsigned int i = 0; i < beams.size(); ++i)
        {
            bolt_parent_init(&pbolt, &(beams[i]));
            beams[i].fire();
        }
        return;
    }

    case SPELL_GLACIATE:
    {
        actor *foe = mons->get_foe();
        ASSERT(foe);
        cast_glaciate(mons, 12 * mons->spell_hd(spell_cast), foe->pos());
        return;
    }

    case SPELL_CLOUD_CONE:
    {
        ASSERT(mons->get_foe());
        cast_cloud_cone(mons, 12 * mons->spell_hd(spell_cast),
                        mons->get_foe()->pos());
        return;
    }

    case SPELL_PHANTOM_MIRROR:
    {
        // Find appropriate ally to clone.
        vector<monster*> targets;
        for (monster_near_iterator mi(mons); mi; ++mi)
        {
            if (_mirrorable(mons, *mi))
                targets.push_back(*mi);
        }

        // If we've found something, mirror it.
        if (targets.size())
        {
            monster* targ = targets[random2(targets.size())];
            if (cast_phantom_mirror(mons, targ))
                simple_monster_message(targ, " shimmers and seems to become two!");
        }
        return;
    }


    case SPELL_SUMMON_MANA_VIPER:
        if (_mons_abjured(mons, monsterNearby))
            return;

        sumcount2 = 1 + random2(mons->spell_hd(spell_cast) / 5 + 1);

        for (sumcount = 0; sumcount < sumcount2; sumcount++)
        {
        create_monster(mgen_data(MONS_MANA_VIPER, SAME_ATTITUDE(mons),
                                 mons, 2, spell_cast, mons->pos(),
                                 mons->foe, 0, god));
        }
        return;

    case SPELL_SUMMON_EMPEROR_SCORPIONS:
    {
        if (_mons_abjured(mons, monsterNearby))
            return;

        sumcount2 = 1 + random2(mons->spell_hd(spell_cast) / 5 + 1);

        for (sumcount = 0; sumcount < sumcount2; sumcount++)
        {
            create_monster(mgen_data(MONS_EMPEROR_SCORPION, SAME_ATTITUDE(mons),
                                     mons, 5, spell_cast, mons->pos(),
                                     mons->foe, 0, god));
        }
        return;
    }

    case SPELL_BATTLECRY:
        _battle_cry(mons);
        return;

    case SPELL_SIGNAL_HORN:
        return; // the entire point is the noise, handled elsewhere

    case SPELL_SEAL_DOORS:
        _seal_doors_and_stairs(mons);
        return;

    case SPELL_BERSERK_OTHER:
        _incite_monsters(mons, true);
        return;

    case SPELL_SPELLFORGED_SERVITOR:
    {
        monster* servitor = create_monster(
            mgen_data(MONS_SPELLFORGED_SERVITOR, SAME_ATTITUDE(mons),
                      mons, 4, spell_cast, mons->pos(), mons->foe, 0, god));
        if (servitor)
            init_servitor(servitor, mons);
        else if (you.can_see(mons))
            canned_msg(MSG_NOTHING_HAPPENS);
        return;
    }

    case SPELL_CORRUPTING_PULSE:
        _corrupting_pulse(mons);
        return;

    case SPELL_TENTACLE_THROW:
        _mons_consider_tentacle_throwing(*mons);
        return;

    case SPELL_SIREN_SONG:
        _siren_sing(mons, false);
        return;

    case SPELL_AVATAR_SONG:
        _siren_sing(mons, true);
        return;

    case SPELL_REPEL_MISSILES:
        simple_monster_message(mons, " begins repelling missiles!");
        mons->add_ench(mon_enchant(ENCH_REPEL_MISSILES));
        return;

    case SPELL_DEFLECT_MISSILES:
        simple_monster_message(mons, " begins deflecting missiles!");
        mons->add_ench(mon_enchant(ENCH_DEFLECT_MISSILES));
        return;

    case SPELL_SUMMON_SCARABS:
    {
        if (_mons_abjured(mons, monsterNearby))
            return;

        sumcount2 = 3 + random2(mons->spell_hd(spell_cast) / 5 + 1);

        for (sumcount = 0; sumcount < sumcount2; sumcount++)
        {
            create_monster(mgen_data(MONS_DEATH_SCARAB, SAME_ATTITUDE(mons),
                                     mons, 2, spell_cast, mons->pos(),
                                     mons->foe, 0, god));
        }
        return;
    }

    case SPELL_HUNTING_CRY:
        return;
    }

    // If a monster just came into view and immediately cast a spell,
    // we need to refresh the screen before drawing the beam.
    viewwindow();
    if (spell_is_direct_explosion(spell_cast))
    {
        const actor *foe = mons->get_foe();
        const bool need_more = foe && (foe->is_player() || you.see_cell(foe->pos()));
        pbolt.in_explosion_phase = false;
        pbolt.refine_for_explosion();
        pbolt.explode(need_more);
    }
    else
        pbolt.fire();
}

static int _noise_level(const monster* mons, spell_type spell,
                        bool silent, unsigned short slot_flags)
{
    const unsigned int flags = get_spell_flags(spell);

    int noise;

    if (silent
        || (slot_flags & MON_SPELL_INNATE_MASK
            && !(slot_flags & MON_SPELL_NOISY)
            && !(flags & SPFLAG_NOISY)))
    {
        noise = 0;
    }
    else if (mons_genus(mons->type) == MONS_DRAGON)
        noise = get_shout_noise_level(S_ROAR);
    else
        noise = spell_noise(spell);

    return noise;
}

static unsigned int _speech_keys(vector<string>& key_list,
                                 const monster* mons, const bolt& pbolt,
                                 spell_type spell,
                                 const unsigned short slot_flags,
                                 bool targeted)
{
    const string cast_str = " cast";

    const bool wizard = slot_flags & MON_SPELL_WIZARD;
    const bool priest = slot_flags & MON_SPELL_PRIEST;
    const bool innate = slot_flags & MON_SPELL_INNATE_MASK;
    const bool demon  = slot_flags & MON_SPELL_DEMONIC;

    const mon_body_shape shape = get_mon_shape(mons);
    const string    spell_name = spell_title(spell);
    const bool      real_spell = !innate && (priest || wizard);

    // Before just using generic per-spell and per-monster casts, try
    // per-monster, per-spell, with the monster type name, then the
    // species name, then the genus name, then wizard/priest/demon.
    // We don't include "real" or "gestures" here since that can be
    // be determined from the monster type; or "targeted" since that
    // can be determined from the spell.
    key_list.push_back(spell_name + " "
                       + mons_type_name(mons->type, DESC_PLAIN) + cast_str);
    key_list.push_back(spell_name + " "
                       + mons_type_name(mons_species(mons->type), DESC_PLAIN)
                       + cast_str);
    key_list.push_back(spell_name + " "
                       + mons_type_name(mons_genus(mons->type), DESC_PLAIN)
                       + cast_str);
    if (wizard)
    {
        key_list.push_back(make_stringf("%s %swizard%s",
                               spell_name.c_str(),
                               shape <= MON_SHAPE_NAGA ? "" : "non-humanoid ",
                               cast_str.c_str()));
    }
    else if (priest)
        key_list.push_back(spell_name + " priest" + cast_str);
    else if (demon)
        key_list.push_back(spell_name + " demon" + cast_str);
    else if (innate)
        key_list.push_back(spell_name + " innate" + cast_str);


    // Now try just the spell's name.
    if (shape <= MON_SHAPE_NAGA)
    {
        if (real_spell)
            key_list.push_back(spell_name + cast_str + " real");
        if (mons_intel(mons) >= I_NORMAL)
            key_list.push_back(spell_name + cast_str + " gestures");
    }
    else if (real_spell)
    {
        // A real spell being cast by something with no hands?  Maybe
        // it's a polymorphed spellcaster which kept its original spells.
        // If so, the cast message for its new type/species/genus probably
        // won't look right.
        if (!mons_class_flag(mons->type, M_ACTUAL_SPELLS | M_PRIEST))
        {
            // XXX: We should probably include the monster's shape,
            // to get a variety of messages.
            if (wizard)
            {
                string key = "polymorphed wizard" + cast_str;
                if (targeted)
                    key_list.push_back(key + " targeted");
                key_list.push_back(key);
            }
            else if (priest)
            {
                string key = "polymorphed priest" + cast_str;
                if (targeted)
                    key_list.push_back(key + " targeted");
                key_list.push_back(key);
            }
        }
    }

    key_list.push_back(spell_name + cast_str);

    const unsigned int num_spell_keys = key_list.size();

    // Next the monster type name, then species name, then genus name.
    key_list.push_back(mons_type_name(mons->type, DESC_PLAIN) + cast_str);
    key_list.push_back(mons_type_name(mons_species(mons->type), DESC_PLAIN)
                       + cast_str);
    key_list.push_back(mons_type_name(mons_genus(mons->type), DESC_PLAIN)
                       + cast_str);

    // Last, generic wizard, priest or demon.
    if (wizard)
    {
        key_list.push_back(make_stringf("%swizard%s",
                               shape <= MON_SHAPE_NAGA ? "" : "non-humanoid ",
                               cast_str.c_str()));
    }
    else if (priest)
        key_list.push_back("priest" + cast_str);
    else if (demon)
        key_list.push_back("demon" + cast_str);

    if (targeted)
    {
        // For targeted spells, try with the targeted suffix first.
        for (unsigned int i = key_list.size() - 1; i >= num_spell_keys; i--)
        {
            string str = key_list[i] + " targeted";
            key_list.insert(key_list.begin() + i, str);
        }

        // Generic beam messages.
        if (pbolt.visible())
        {
            key_list.push_back(pbolt.get_short_name() + " beam " + cast_str);
            key_list.push_back("beam catchall cast");
        }
    }

    return num_spell_keys;
}

static string _speech_message(const vector<string>& key_list,
                              unsigned int num_spell_keys,
                              bool silent, bool unseen)
{
    string prefix;
    if (silent)
        prefix = "silent ";
    else if (unseen)
        prefix = "unseen ";

    string msg;
    for (unsigned int i = 0; i < key_list.size(); i++)
    {
        const string key = key_list[i];

#ifdef DEBUG_MONSPEAK
        dprf(DIAG_SPEECH, "monster casting lookup: %s%s",
             prefix.c_str(), key.c_str());
#endif

        msg = getSpeakString(prefix + key);
        if (msg == "__NONE")
        {
            msg = "";
            break;
        }
        else if (msg == "__NEXT")
        {
            msg = "";
            if (i < num_spell_keys)
                i = num_spell_keys - 1;
            else if (ends_with(key, " targeted"))
                i++;
            continue;
        }
        else if (!msg.empty())
            break;

        // If we got no message and we're using the silent prefix, then
        // try again without the prefix.
        if (prefix != "silent ")
            continue;

        msg = getSpeakString(key);
        if (msg == "__NONE")
        {
            msg = "";
            break;
        }
        else if (msg == "__NEXT")
        {
            msg = "";
            if (i < num_spell_keys)
                i = num_spell_keys - 1;
            else if (ends_with(key, " targeted"))
                i++;
            continue;
        }
        else if (!msg.empty())
            break;
    }

    return msg;
}

static void _speech_fill_target(string& targ_prep, string& target,
                                const monster* mons, const bolt& pbolt,
                                bool gestured)
{
    targ_prep = "at";
    target    = "nothing";

    bolt tracer = pbolt;
    // For a targeted but rangeless spell make the range positive so that
    // fire_tracer() will fill out path_taken.
    if (pbolt.range == 0 && pbolt.target != mons->pos())
        tracer.range = ENV_SHOW_DIAMETER;
    fire_tracer(mons, tracer);

    if (pbolt.target == you.pos())
        target = "you";
    else if (pbolt.target == mons->pos())
        target = mons->pronoun(PRONOUN_REFLEXIVE);
    // Monsters should only use targeted spells while foe == MHITNOT
    // if they're targeting themselves.
    else if (mons->foe == MHITNOT && !mons_is_confused(mons, true))
        target = "NONEXISTENT FOE";
    else if (!invalid_monster_index(mons->foe)
             && menv[mons->foe].type == MONS_NO_MONSTER)
    {
        target = "DEAD FOE";
    }
    else if (in_bounds(pbolt.target) && you.see_cell(pbolt.target))
    {
        if (const monster* mtarg = monster_at(pbolt.target))
        {
            if (you.can_see(mtarg))
                target = mtarg->name(DESC_THE);
        }
    }

    const bool visible_path      = pbolt.visible() || gestured;

    // Monster might be aiming past the real target, or maybe some fuzz has
    // been applied because the target is invisible.
    if (target == "nothing")
    {
        if (pbolt.aimed_at_spot || pbolt.origin_spell == SPELL_DIG)
        {
            int count = 0;
            for (adjacent_iterator ai(pbolt.target); ai; ++ai)
            {
                const actor* act = actor_at(*ai);
                if (act && act != mons && you.can_see(act))
                {
                    targ_prep = "next to";

                    if (act->is_player() || one_chance_in(++count))
                        target = act->name(DESC_THE);

                    if (act->is_player())
                        break;
                }
            }

            if (targ_prep == "at")
            {
                if (grd(pbolt.target) != DNGN_FLOOR)
                {
                    target = feature_description(grd(pbolt.target),
                                                 NUM_TRAPS, "", DESC_THE,
                                                 false);
                }
                else
                    target = "thin air";
            }

            return;
        }

        bool mons_targ_aligned = false;

        const vector<coord_def> &path = tracer.path_taken;
        for (unsigned int i = 0; i < path.size(); i++)
        {
            const coord_def pos = path[i];

            if (pos == mons->pos())
                continue;

            const monster* m = monster_at(pos);
            if (pos == you.pos())
            {
                // Be egotistical and assume that the monster is aiming at
                // the player, rather than the player being in the path of
                // a beam aimed at an ally.
                if (!mons->wont_attack())
                {
                    targ_prep = "at";
                    target    = "you";
                    break;
                }
                // If the ally is confused or aiming at an invisible enemy,
                // with the player in the path, act like it's targeted at
                // the player if there isn't any visible target earlier
                // in the path.
                else if (target == "nothing")
                {
                    targ_prep         = "at";
                    target            = "you";
                    mons_targ_aligned = true;
                }
            }
            else if (visible_path && m && you.can_see(m))
            {
                bool is_aligned  = mons_aligned(m, mons);
                string name = m->name(DESC_THE);

                if (target == "nothing")
                {
                    mons_targ_aligned = is_aligned;
                    target            = name;
                }
                // If the first target was aligned with the beam source then
                // the first subsequent non-aligned monster in the path will
                // take it's place.
                else if (mons_targ_aligned && !is_aligned)
                {
                    mons_targ_aligned = false;
                    target            = name;
                }
                targ_prep = "at";
            }
            else if (visible_path && target == "nothing")
            {
                int count = 0;
                for (adjacent_iterator ai(pbolt.target); ai; ++ai)
                {
                    const actor* act = monster_at(*ai);
                    if (act && act != mons && you.can_see(act))
                    {
                        targ_prep = "past";
                        if (act->is_player()
                            || one_chance_in(++count))
                        {
                            target = act->name(DESC_THE);
                        }

                        if (act->is_player())
                            break;
                    }
                }
            }
        } // for (unsigned int i = 0; i < path.size(); i++)
    } // if (target == "nothing" && targeted)

    const actor* foe = mons->get_foe();

    // If we still can't find what appears to be the target, and the
    // monster isn't just throwing the spell in a random direction,
    // we should be able to tell what the monster was aiming for if
    // we can see the monster's foe and the beam (or the beam path
    // implied by gesturing).  But only if the beam didn't actually hit
    // anything (but if it did hit something, why didn't that monster
    // show up in the beam's path?)
    if (target == "nothing"
        && (tracer.foe_info.count + tracer.friend_info.count) == 0
        && foe != NULL
        && you.can_see(foe)
        && !mons->confused()
        && visible_path)
    {
        target = foe->name(DESC_THE);
        targ_prep = (pbolt.aimed_at_spot ? "next to" : "past");
    }

    // If the monster gestures to create an invisible beam then
    // assume that anything close to the beam is the intended target.
    // Also, if the monster gestures to create a visible beam but it
    // misses still say that the monster gestured "at" the target,
    // rather than "past".
    if (gestured || target == "nothing")
        targ_prep = "at";

    // "throws whatever at something" is better than "at nothing"
    if (target == "nothing")
        target = "something";
}

void mons_cast_noise(monster* mons, const bolt &pbolt,
                     spell_type spell_cast, unsigned short slot_flags)
{
    bool force_silent = false;
    noise_flag_type noise_flags = NF_NONE;

    if (spell_cast == SPELL_HUNTING_CRY)
        noise_flags = (noise_flag_type)(noise_flags | NF_HUNTING_CRY);

    if (mons->type == MONS_SHADOW_DRAGON)
        // Draining breath is silent.
        force_silent = true;

    const bool unseen    = !you.can_see(mons);
    const bool silent    = silenced(mons->pos()) || force_silent;

    if (unseen && silent)
        return;

    int noise = _noise_level(mons, spell_cast, silent, slot_flags);

    const unsigned int spell_flags = get_spell_flags(spell_cast);
    const bool targeted = (spell_flags & SPFLAG_TARGETING_MASK)
                           && (pbolt.target != mons->pos()
                               || pbolt.visible());

    vector<string> key_list;
    unsigned int num_spell_keys =
        _speech_keys(key_list, mons, pbolt, spell_cast,
                     slot_flags, targeted);

    string msg = _speech_message(key_list, num_spell_keys, silent, unseen);

    if (msg.empty())
    {
        if (silent)
            return;

        noisy(noise, mons->pos(), mons->mid, noise_flags);
        return;
    }

    // FIXME: we should not need to look at the message text.
    const bool gestured = msg.find("Gesture") != string::npos
                          || msg.find(" gesture") != string::npos
                          || msg.find("Point") != string::npos
                          || msg.find(" point") != string::npos;

    string targ_prep = "at";
    string target    = "NO_TARGET";

    if (targeted)
        _speech_fill_target(targ_prep, target, mons, pbolt, gestured);

    msg = replace_all(msg, "@at@",     targ_prep);
    msg = replace_all(msg, "@target@", target);

    string beam_name;
    if (!targeted)
        beam_name = "NON TARGETED BEAM";
    else if (pbolt.name.empty())
        beam_name = "INVALID BEAM";
    else
        beam_name = pbolt.get_short_name();

    msg = replace_all(msg, "@beam@", beam_name);

    const msg_channel_type chan =
        (unseen              ? MSGCH_SOUND :
         mons->friendly()    ? MSGCH_FRIEND_SPELL
                             : MSGCH_MONSTER_SPELL);

    if (silent || noise == 0)
        mons_speaks_msg(mons, msg, chan, true);
    else if (noisy(noise, mons->pos(), mons->mid, noise_flags) || !unseen)
    {
        // noisy() returns true if the player heard the noise.
        mons_speaks_msg(mons, msg, chan);
    }
}

/**
 * Find the best creature for the given monster to toss with its tentacles.
 *
 * @param mons      The monster thinking about tentacle-throwing.
 * @return          The mid_t of the best victim for the monster to throw.
 *                  Could be player, another monster, or MID_NOBODY (none).
 */
static mid_t _get_tentacle_throw_victim(const monster &mons)
{
    mid_t throw_choice = MID_NOBODY;
    int highest_dur = -1;

    if (!mons.constricting)
        return throw_choice;

    for (actor::constricting_t::iterator co = mons.constricting->begin();
         co != mons.constricting->end(); ++co)
    {
        const actor* const victim = actor_by_mid(co->first);
        // Only throw real, living victims.
        if (!victim || !victim->alive())
            continue;

        // Don't try to throw anything constricting you.
        if (mons.is_constricted() &&  mons.constricted_by == co->first)
            continue;

        // Always throw the player, if we can.
        if (victim->is_player())
            return co->first;

        // otherwise throw whomever we've been constricting the longest.
        if (co->second > highest_dur)
        {
            throw_choice = co->first;
            highest_dur = co->second;
        }
    }

    return throw_choice;
}

/**
 * Make the given monster try to throw a constricted victim, if possible.
 *
 * @param mons       The monster doing the throwing.
 * @return           Whether a throw attempt was made.
 */
static bool _mons_consider_tentacle_throwing(const monster &mons)
{
    const mid_t throw_choice = _get_tentacle_throw_victim(mons);
    actor* victim = actor_by_mid(throw_choice);
    if (!victim)
        return false;

    return _tentacle_toss(mons, *victim, mons.get_hit_dice() * 4);
}

static const int MIN_TENTACLE_THROW_DIST = 2;

/**
 * Choose a landing site for a tentacle toss. (Not necessarily the destination
 * of the toss, but the place that the monster is aiming for when throwing.)
 *
 * @param thrower       The monster performing the toss.
 * @param victim        The actor being thrown.
 * @return              The coord_def of one of the best (most dangerous)
 *                      possible landing sites for a toss.
 *                      If no valid site is found, returns the origin (0,0).
 */
static coord_def _choose_tentacle_toss_target(const monster &thrower,
                                              const actor &victim)
{
    int best_site_score = -1;
    vector<coord_def> best_sites;

    for (distance_iterator di(thrower.pos(), true, true, LOS_RADIUS); di; ++di)
    {
        ray_def ray;
        // Unusable landing sites.
        if (victim.pos().distance_from(*di) < MIN_TENTACLE_THROW_DIST
            || actor_at(*di)
            || !thrower.see_cell(*di)
            || !victim.see_cell(*di)
            || !victim.is_habitable(*di)
            || !find_ray(victim.pos(), *di, ray, opc_solid_see))
        {
            continue;
        }

        const int site_score = _throw_site_score(thrower, victim, *di);
        if (site_score > best_site_score)
        {
            best_site_score = site_score;
            best_sites.clear();
        }
        if (site_score == best_site_score)
            best_sites.push_back(*di);
    }

    // No valid landing site found.
    if (!best_sites.size())
        return coord_def(0,0);

    const coord_def best_site = best_sites[random2(best_sites.size())];
    return best_site;
}

/**
 * Find the actual landing place for a tentacle toss.
 *
 * @param thrower       The monster performing the toss.
 * @param victim        The actor being thrown.
 * @param target_site   The intended destination.
 * @return              The actual destination, somewhere along a ray between
 *                      the victim's initial position and the intended
 *                      destination.
 */
static coord_def _choose_tentacle_toss_dest(const monster &thrower,
                                            const actor &victim,
                                            const coord_def &target_site)
{
    ray_def ray;
    vector<coord_weight> dests;
    find_ray(victim.pos(), target_site, ray, opc_solid_see);
    while (ray.advance())
    {
        if (victim.pos().distance_from(ray.pos()) >= MIN_TENTACLE_THROW_DIST
            && !actor_at(ray.pos())
            && victim.is_habitable(ray.pos())
            && thrower.see_cell(ray.pos())
            && victim.see_cell(ray.pos()))
        {
            const int dist = victim.pos().distance_from(ray.pos());
            const int weight = sqr(LOS_RADIUS - dist + 1);
            dests.push_back(coord_weight(ray.pos(), weight));
        }
        if (ray.pos() == target_site)
            break;
    }

    ASSERT(dests.size());
    const coord_def* const choice = random_choose_weighted(dests);
    ASSERT(choice);
    ASSERT(in_bounds(*choice));
    return *choice;
}

/**
 * Actually perform a tentacle throw to the specified destination.
 *
 * @param thrower       The monster doing the tossing.
 * @param victim        The tossee.
 * @param pow           The power of the throw; determines damage.
 * @param chosen_dest   The final destination of the victim.
 */
static void _tentacle_toss_to(const monster &thrower, actor &victim,
                              int pow, const coord_def &chosen_dest)
{
    ASSERT(in_bounds(chosen_dest));

    const bool thrower_seen = you.can_see(&thrower);
    const bool victim_was_seen = you.can_see(&victim);
    const string thrower_name = thrower.name(DESC_THE);

    const int dam = victim.apply_ac(random2(pow));
    victim.stop_being_constricted(true);
    if (victim.is_player())
    {
        mprf("%s throws you!",
             (thrower_seen ? thrower_name.c_str() : "Something"));
        move_player_to_grid(chosen_dest, false);
        ouch(dam, thrower.mindex(), KILLED_BY_BEING_THROWN);
    }
    else
    {
        monster * const vmon = victim.as_monster();
        const string victim_name = victim.name(DESC_THE);
        const coord_def old_pos = victim.pos();

        if (!(vmon->flags & MF_WAS_IN_VIEW))
            vmon->seen_context = SC_THROWN_IN;
        vmon->move_to_pos(chosen_dest);
        vmon->apply_location_effects(old_pos);
        vmon->check_redraw(old_pos);
        if (thrower_seen || victim_was_seen)
        {
            mprf("%s throws %s%s!",
                 (thrower_seen ? thrower_name.c_str() : "Something"),
                 (victim_was_seen ? victim_name.c_str() : "something"),
                 (you.can_see(vmon) ? "" : "out of view"));
        }
        victim.hurt(&thrower, dam, BEAM_NONE, true);
    }
}

/**
 * The actor throws the victim to a habitable square within LOS of the victim
 * and at least as far as a distance of 2 from the thrower, which deals
 * AC-checking damage. A hostile monster prefers to throw the player into a
 * dangerous spot, and a monster throwing another monster prefers to throw far
 * from the player, regardless of alignment.
 * @param thrower  The thrower.
 * @param victim   The victim.
 * @param pow      The throw power, which is the die size for damage.
 * @return         True if the victim was thrown, False otherwise.
 */
static bool _tentacle_toss(const monster &thrower, actor &victim, int pow)
{
    const coord_def throw_target = _choose_tentacle_toss_target(thrower,
                                                                victim);
    if (throw_target.origin())
        return false;

    const coord_def chosen_dest = _choose_tentacle_toss_dest(thrower, victim,
                                                             throw_target);
    _tentacle_toss_to(thrower, victim, pow, chosen_dest);
    return true;
}

/**
 * Score a landing site for purposes of throwing the victim. This uses monster
 * difficulty and number of open (habitable) squares as a score if the victim
 * is the player, or distance from player otherwise.
 * @param   thrower  The thrower.
 * @param   victim   The victim.
 * @param   site     The site to score.
 * @return           An integer score >= 0
 */
static int _throw_site_score(const monster &thrower, const actor &victim,
                             const coord_def &site)
{
    const int open_site_score = 1;
    const monster * const vmons = victim.as_monster();

    // Initial score is just as far away from player as possible, and
    // we stop there if the thrower or victim is friendly.
    int score = you.pos().distance_from(site);
    if (thrower.friendly() || (vmons && vmons->friendly()))
        return score;

    for (adjacent_iterator ai(site); ai; ++ai)
    {
        if (!thrower.see_cell(*ai))
            continue;

        // Being next to open space is bad for players, who thrive in crannies
        // and nooks, like the vermin they are.
        if (victim.is_habitable(*ai))
            score += open_site_score;

        // Being next to dangerous monsters is also bad.
        const monster * const mons = monster_at(*ai);
        if (mons && !mons->friendly()
            && mons != &thrower
            && !mons_is_firewood(mons))
        {
            score += sqr(mons_threat_level(mons) + 2);
        }
    }

    return score;
}

/**
 * Check if a siren or merfolk avatar should sing its song.
 *
 * @param mons   The singing monster.
 * @param avatar Whether to use the more powerful "avatar song".
 * @return       Whether the song should be sung.
 */
static bool _should_siren_sing(monster* mons, bool avatar)
{
    // Don't behold observer in the arena.
    if (crawl_state.game_is_arena())
        return false;

    // Don't behold player already half down or up the stairs.
    if (!you.delay_queue.empty())
    {
        const delay_queue_item delay = you.delay_queue.front();

        if (delay.type == DELAY_ASCENDING_STAIRS
            || delay.type == DELAY_DESCENDING_STAIRS)
        {
            dprf("Taking stairs, don't mesmerise.");
            return false;
        }
    }

    // Won't sing if either of you silenced, or it's friendly,
    // confused, fleeing, or leaving the level.
    if (mons->has_ench(ENCH_CONFUSION)
        || mons_is_fleeing(mons)
        || mons->pacified()
        || mons->friendly()
        || !player_can_hear(mons->pos()))
    {
        return false;
    }

    // Don't even try on berserkers. Sirens know their limits.
    // (merfolk avatars should still sing since their song has other effects)
    if (!avatar && you.berserk())
        return false;

    // If the mer is trying to mesmerise you anew, only sing half as often.
    if (!you.beheld_by(mons) && mons->foe == MHITYOU && you.can_see(mons)
        && coinflip())
    {
        return false;
    }

    // We can do it!
    return true;
}

/**
 * Have a siren or merfolk avatar attempt to mesmerize the player.
 *
 * @param mons   The singing monster.
 * @param avatar Whether to use the more powerful "avatar song".
 */
static void _siren_sing(monster* mons, bool avatar)
{
    const msg_channel_type spl = (mons->friendly() ? MSGCH_FRIEND_SPELL
                                                       : MSGCH_MONSTER_SPELL);
    const bool already_mesmerised = you.beheld_by(mons);

    noisy(LOS_RADIUS, mons->pos(), mons->mid, NF_SIREN);

    if (avatar && !mons->has_ench(ENCH_MERFOLK_AVATAR_SONG))
        mons->add_ench(mon_enchant(ENCH_MERFOLK_AVATAR_SONG, 0, mons, 70));

    if (you.can_see(mons))
    {
        const char * const song_adj = already_mesmerised ? "its luring"
                                                         : "a haunting";
        const string song_desc = make_stringf(" chants %s song.", song_adj);
        simple_monster_message(mons, song_desc.c_str(), spl);
    }
    else
    {
        mprf(MSGCH_SOUND, "You hear %s.",
                          already_mesmerised ? "a luring song" :
                          coinflip()         ? "a haunting song"
                                             : "an eerie melody");

        // If you're already mesmerised by an invisible siren, it
        // can still prolong the enchantment.
        if (!already_mesmerised)
            return;
    }

    // Once mesmerised by a particular monster, you cannot resist anymore.
    if (you.duration[DUR_MESMERISE_IMMUNE]
        || !already_mesmerised
           && (you.check_res_magic(mons->get_hit_dice() * 22 / 3 + 15) > 0
               || you.clarity()))
    {
        canned_msg(you.clarity() ? MSG_YOU_UNAFFECTED : MSG_YOU_RESIST);
        return;
    }

    you.add_beholder(mons);
}

// Checks to see if a particular spell is worth casting in the first place.
static bool _ms_waste_of_time(monster* mon, mon_spell_slot slot)
{
    spell_type monspell = slot.spell;
    actor *foe = mon->get_foe();
    const bool friendly = mon->friendly();

    // Keep friendly summoners from spamming summons constantly.
    if (friendly
        && !foe
        && spell_typematch(monspell, SPTYP_SUMMONING))
    {
        return true;
    }

    if (!mon->wont_attack())
    {
        if (spell_harms_area(monspell) && env.sanctuary_time > 0)
            return true;

        if (spell_harms_target(monspell) && is_sanctuary(mon->target))
            return true;
    }

    if (slot.flags & MON_SPELL_BREATH && mon->has_ench(ENCH_BREATH_WEAPON))
        return true;

    // Don't bother casting a summon spell if we're already at its cap
    if (summons_are_capped(monspell)
        && count_summons(mon, monspell) >= summons_limit(monspell))
    {
        return true;
    }

    // Eventually, we'll probably want to be able to have monsters
    // learn which of their elemental bolts were resisted and have those
    // handled here as well. - bwr
    switch (monspell)
    {
    case SPELL_CALL_TIDE:
        return !player_in_branch(BRANCH_SHOALS)
               || mon->has_ench(ENCH_TIDE)
               || !foe
               || (grd(mon->pos()) == DNGN_DEEP_WATER
                   && grd(foe->pos()) == DNGN_DEEP_WATER);

    case SPELL_BRAIN_FEED:
        return !foe || !foe->is_player();

    case SPELL_VAMPIRIC_DRAINING:
        if (!foe
            || mon->hit_points + 1 >= mon->max_hit_points
            || !adjacent(mon->pos(), foe->pos()))
        {
            return true;
        }
    // fall through
    case SPELL_BOLT_OF_DRAINING:
    case SPELL_AGONY:
    case SPELL_MALIGN_OFFERING:
        return !foe || _foe_should_res_negative_energy(foe);

    case SPELL_MIASMA_BREATH:
        return !foe || foe->res_rotting();

    case SPELL_DISPEL_UNDEAD:
        // [ds] How is dispel undead intended to interact with vampires?
        return !foe || foe->holiness() != MH_UNDEAD;

    case SPELL_CORONA:
        return !foe || foe->backlit() || foe->glows_naturally()
                    || mons_class_flag(foe->type, M_SHADOW);

    case SPELL_BERSERKER_RAGE:
        // Snorg does not go berserk as often until wounded.
        if (mon->type == MONS_SNORG
            && mon->hit_points == mon->max_hit_points
            && !one_chance_in(4))
        {
            return true;
        }
        return !mon->needs_berserk(false);

    case SPELL_FRENZY:
        return !mon->can_go_frenzy();

    case SPELL_HASTE:
        return mon->has_ench(ENCH_HASTE);

    case SPELL_MIGHT:
        return mon->has_ench(ENCH_MIGHT);

    case SPELL_SWIFTNESS:
        return mon->has_ench(ENCH_SWIFT);

    case SPELL_REGENERATION:
        return mon->has_ench(ENCH_REGENERATION)
               || mon->has_ench(ENCH_DEATHS_DOOR);

    case SPELL_INJURY_MIRROR:
        return mon->has_ench(ENCH_MIRROR_DAMAGE)
               || mon->props.exists("mirror_recast_time")
                  && you.elapsed_time < mon->props["mirror_recast_time"].get_int();

    case SPELL_TROGS_HAND:
        return mon->has_ench(ENCH_RAISED_MR)
               || mon->has_ench(ENCH_REGENERATION);

    case SPELL_STONESKIN:
        return mon->has_ench(ENCH_STONESKIN);

    case SPELL_INVISIBILITY:
        return mon->has_ench(ENCH_INVIS)
               || mon->glows_naturally();

    case SPELL_MINOR_HEALING:
    case SPELL_MAJOR_HEALING:
        return mon->hit_points > mon->max_hit_points / 2;

    case SPELL_TELEPORT_SELF:
        // Monsters aren't smart enough to know when to cancel teleport.
        return mon->has_ench(ENCH_TP) || mon->no_tele(true, false);

    case SPELL_BLINK_CLOSE:
        if (!foe || adjacent(mon->pos(), foe->pos()))
            return true;
        // intentional fall-through
    case SPELL_BLINK:
    case SPELL_CONTROLLED_BLINK:
    case SPELL_BLINK_RANGE:
    case SPELL_BLINK_AWAY:
        // Prefer to keep a tornado going rather than blink.
        return mon->no_tele(true, false) || mon->has_ench(ENCH_TORNADO);

    case SPELL_BLINK_OTHER:
    case SPELL_BLINK_OTHER_CLOSE:
        return !foe
                || foe->is_monster()
                    && foe->as_monster()->has_ench(ENCH_DIMENSION_ANCHOR)
                || foe->is_player()
                    && you.duration[DUR_DIMENSION_ANCHOR];

    case SPELL_TELEPORT_OTHER:
        // Monsters aren't smart enough to know when to cancel teleport.
        if (!foe
            || foe->is_player() && you.duration[DUR_TELEPORT]
            || foe->is_monster() && foe->as_monster()->has_ench(ENCH_TP))
        {
            return true;
        }
        // intentional fall-through
    case SPELL_SLOW:
    case SPELL_CONFUSE:
    case SPELL_PAIN:
    case SPELL_BANISHMENT:
    case SPELL_DISINTEGRATE:
    case SPELL_PARALYSE:
    case SPELL_SLEEP:
    case SPELL_HIBERNATION:
    case SPELL_DIMENSION_ANCHOR:
    {
        if ((monspell == SPELL_HIBERNATION || monspell == SPELL_SLEEP)
            && (!foe || foe->asleep() || !foe->can_sleep()))
        {
            return true;
        }

        // Occasionally we don't estimate... just fire and see.
        if (one_chance_in(5))
            return false;

        // Only intelligent monsters estimate.
        int intel = mons_intel(mon);
        if (intel < I_NORMAL)
            return false;

        // We'll estimate the target's resistance to magic, by first getting
        // the actual value and then randomising it.
        int est_magic_resist = 10000;

        if (foe != NULL)
        {
            est_magic_resist = foe->res_magic();

            // now randomise (normal intels less accurate than high):
            if (intel == I_NORMAL)
                est_magic_resist += random2(80) - 40;
            else
                est_magic_resist += random2(30) - 15;
        }

        int power = 12 * mon->spell_hd(monspell)
                       * (monspell == SPELL_PAIN ? 2 : 1);
        power = stepdown_value(power, 30, 40, 100, 120);

        // Determine the amount of chance allowed by the benefit from
        // the spell.  The estimated difficulty is the probability
        // of rolling over 100 + diff on 2d100. -- bwr
        int diff = (monspell == SPELL_PAIN
                    || monspell == SPELL_SLOW
                    || monspell == SPELL_CONFUSE) ? 0 : 50;

        return est_magic_resist - power > diff;
    }

    // Mara shouldn't cast player ghost if he can't see the player
    case SPELL_SUMMON_ILLUSION:
        return !foe || !mon->can_see(foe) || !actor_is_illusion_cloneable(foe);

    case SPELL_AWAKEN_FOREST:
        return mon->has_ench(ENCH_AWAKEN_FOREST)
               || env.forest_awoken_until > you.elapsed_time
               || !forest_near_enemy(mon)
               || you_worship(GOD_FEDHAS);

    case SPELL_DEATHS_DOOR:
        // The caster may be an (undead) enslaved soul.
        return mon->holiness() == MH_UNDEAD
               || mon->has_ench(ENCH_DEATHS_DOOR)
               || mon->has_ench(ENCH_FATIGUE);

    case SPELL_OZOCUBUS_ARMOUR:
        return mon->has_ench(ENCH_OZOCUBUS_ARMOUR);

    case SPELL_BATTLESPHERE:
        return find_battlesphere(mon);

    case SPELL_SPECTRAL_WEAPON:
        return find_spectral_weapon(mon);

    case SPELL_INJURY_BOND:
        for (monster_iterator mi; mi; ++mi)
        {
            if (mons_aligned(mon, *mi) && !mi->has_ench(ENCH_CHARM)
                && *mi != mon && mon->see_cell_no_trans(mi->pos())
                && !mi->has_ench(ENCH_INJURY_BOND))
            {
                    return false; // We found at least one target; that's enough.
            }
        }
        return true;

    case SPELL_GHOSTLY_FIREBALL:
        return !foe || foe->holiness() == MH_UNDEAD;

    case SPELL_BLINK_ALLIES_ENCIRCLE:
        if (!foe || !mon->can_see(foe))
            return true;

        for (monster_near_iterator mi(mon, LOS_NO_TRANS); mi; ++mi)
            if (_valid_encircle_ally(mon, *mi, foe->pos()))
                return false; // We found at least one valid ally; that's enough.
        return true;

    case SPELL_AWAKEN_VINES:
        return !foe
               || mon->has_ench(ENCH_AWAKEN_VINES)
                   && mon->props["vines_awakened"].get_int() >= 3
               || !_awaken_vines(mon, true);

    case SPELL_CONTROL_WINDS:
        return mon->has_ench(ENCH_CONTROL_WINDS);

    case SPELL_WATERSTRIKE:
        return !foe || !feat_is_water(grd(foe->pos()));

    case SPELL_HASTE_PLANTS:
        for (monster_near_iterator mi(mon, LOS_NO_TRANS); mi; ++mi)
        {
            // Isn't useless if there's a single viable target for it
            if (mons_aligned(*mi, mon)
                && _is_hastable_plant(*mi)
                && !mi->has_ench(ENCH_HASTE))
            {
                return false;
            }
        }
        return true;

    // Don't use unless our foe is close to us and there are no allies already
    // between the two of us
    case SPELL_WIND_BLAST:
        if (foe && foe->pos().distance_from(mon->pos()) < 4)
        {
            bolt tracer;
            tracer.target = foe->pos();
            tracer.range  = LOS_RADIUS;
            tracer.hit    = AUTOMATIC_HIT;
            fire_tracer(mon, tracer);

            actor* act = actor_at(tracer.path_taken.back());
            if (act && mons_aligned(mon, act))
                return true;
            else
                return false;
        }
        else
            return true;

    case SPELL_STRIP_RESISTANCE:
        return !foe
               || foe->is_monster()
                  && foe->as_monster()->has_ench(ENCH_LOWERED_MR)
               || foe->is_player() && you.duration[DUR_LOWERED_MR];

    case SPELL_BROTHERS_IN_ARMS:
        return mon->props.exists("brothers_count")
               && mon->props["brothers_count"].get_int() >= 2;

    case SPELL_SUMMON_MUSHROOMS:
        return !foe;

    case SPELL_FREEZE:
        return !foe || !adjacent(mon->pos(), foe->pos());

    case SPELL_DRUIDS_CALL:
        // Don't cast unless there's at least one valid target
        for (monster_iterator mi; mi; ++mi)
            if (_valid_druids_call_target(mon, *mi))
                return false;
        return true;

    // Don't spam mesmerisation if you're already mesmerised
    case SPELL_MESMERISE:
        return you.beheld_by(mon) && coinflip();

    case SPELL_DISCHARGE:
        // TODO: possibly check for friendlies nearby?
        // Perhaps it will be used recklessly like chain lightning...
        return !foe || !adjacent(foe->pos(), mon->pos());

    case SPELL_PORTAL_PROJECTILE:
    {
        bolt beam;
        beam.source    = mon->pos();
        beam.target    = mon->target;
        beam.source_id = mon->mid;
        return !handle_throw(mon, beam, true, true);
    }

    case SPELL_VIRULENCE:
        return !foe || foe->res_poison(false) >= 3;

    case SPELL_IGNITE_POISON_SINGLE:
        return !foe || !ignite_poison_affects(foe);

    case SPELL_FLASH_FREEZE:
        return !foe
               || foe->is_player() && you.duration[DUR_FROZEN]
               || foe->is_monster()
                  && foe->as_monster()->has_ench(ENCH_FROZEN);

    case SPELL_LEGENDARY_DESTRUCTION:
        return !foe || mon->hit_points <= 10;

    case SPELL_BLACK_MARK:
        return mon->has_ench(ENCH_BLACK_MARK);

    case SPELL_GRAND_AVATAR:
        return mon->has_ench(ENCH_GRAND_AVATAR);

    // No need to spam cantrips if we're just travelling around
    case SPELL_CANTRIP:
        return !foe;

    case SPELL_BLINK_ALLIES_AWAY:
        if (!foe || !mon->can_see(foe))
            return true;

        for (monster_near_iterator mi(mon, LOS_NO_TRANS); mi; ++mi)
            if (_valid_blink_away_ally(mon, *mi, foe->pos()))
                return false;
        return true;

    case SPELL_SHROUD_OF_GOLUBRIA:
        return mon->has_ench(ENCH_SHROUD);

    // Don't let clones duplicate anything, to prevent exponential explosion
    case SPELL_FAKE_MARA_SUMMON:
        return mon->has_ench(ENCH_PHANTOM_MIRROR);

    case SPELL_PHANTOM_MIRROR:
        if (!mon->has_ench(ENCH_PHANTOM_MIRROR))
        {
            for (monster_near_iterator mi(mon); mi; ++mi)
            {
                // A single valid target is enough.
                if (_mirrorable(mon, *mi))
                    return false;
            }
        }
        return true;

    case SPELL_THROW_BARBS:
        // Don't fire barbs in melee range.
        return !foe || adjacent(mon->pos(), foe->pos());

    case SPELL_BATTLECRY:
        return !_battle_cry(mon, true);

    case SPELL_SIGNAL_HORN:
        return friendly;

    case SPELL_SEAL_DOORS:
        return friendly || !_seal_doors_and_stairs(mon, true);

    case SPELL_FLAY:
        return !foe || foe->holiness() != MH_NATURAL;

    case SPELL_ANIMATE_DEAD:
    case SPELL_TWISTED_RESURRECTION:
    case SPELL_SIMULACRUM:
        if (friendly && !_animate_dead_okay(monspell))
            return true;

        if (mon->is_summoned())
            return true;

        return monspell == SPELL_ANIMATE_DEAD
               && !animate_dead(mon, 100, SAME_ATTITUDE(mon),
                                mon->foe, mon, "", mon->god, false)
               || monspell == SPELL_TWISTED_RESURRECTION
                  && !twisted_resurrection(mon, 500, SAME_ATTITUDE(mon),
                                           mon->foe, mon->god, false)
               || monspell == SPELL_SIMULACRUM
                  && !monster_simulacrum(mon, false);

    //XXX: unify with the other SPELL_FOO_OTHER spells?
    case SPELL_BERSERK_OTHER:
        return !_incite_monsters(mon, false);

    case SPELL_CAUSE_FEAR:
        return _mons_cause_fear(mon, false) < 0;

    case SPELL_MASS_CONFUSION:
        return _mons_mass_confuse(mon, false) < 0;

    case SPELL_TENTACLE_THROW:
        return _get_tentacle_throw_victim(*mon) == MID_NOBODY;

    case SPELL_CREATE_TENTACLES:
        return !_mons_available_tentacles(mon);

    case SPELL_WORD_OF_RECALL:
        return !_should_recall(mon);

    case SPELL_SHATTER:
        return !mons_shatter(mon, false);

    case SPELL_EPHEMERAL_INFUSION:
        return !_should_ephemeral_infusion(mon);

    case SPELL_SYMBOL_OF_TORMENT:
        return !_trace_los(mon, _torment_vulnerable)
               || you.visible_to(mon)
                  && friendly
                  && !player_res_torment(false)
                  && !player_kiku_res_torment();
    case SPELL_CHAIN_LIGHTNING:
        return !_trace_los(mon, _elec_vulnerable);
    case SPELL_CHAIN_OF_CHAOS:
        return !_trace_los(mon, _dummy_vulnerable);
    case SPELL_CORRUPTING_PULSE:
        return !_trace_los(mon, _mutation_vulnerable)
               || you.visible_to(mon)
                  && friendly;
    case SPELL_TORNADO:
        return mon->has_ench(ENCH_TORNADO)
               || mon->has_ench(ENCH_TORNADO_COOLDOWN)
               || !_trace_los(mon, _tornado_vulnerable);

    case SPELL_ENGLACIATION:
        return !foe
               || !mon->see_cell_no_trans(foe->pos())
               || foe->res_cold() > 0;

    case SPELL_OLGREBS_TOXIC_RADIANCE:
        return mon->has_ench(ENCH_TOXIC_RADIANCE)
               || cast_toxic_radiance(mon, 100, false, true) != SPRET_SUCCESS;
    case SPELL_IGNITE_POISON:
        return cast_ignite_poison(mon, 0, false, true) != SPRET_SUCCESS;

    case SPELL_DRAIN_LIFE:
    case SPELL_OZOCUBUS_REFRIGERATION:
        return cast_los_attack_spell(monspell, mon->spell_hd(monspell),
                                     mon, false) != SPRET_SUCCESS
               || you.visible_to(mon)
                  && friendly
                  && (monspell == SPELL_OZOCUBUS_REFRIGERATION
                      && player_res_cold(false) < 1
                      || monspell == SPELL_DRAIN_LIFE
                         && player_prot_life(false) < 1);

    case SPELL_GLACIATE:
        return !foe
               || !_glaciate_tracer(mon, 12 * mon->spell_hd(monspell),
                                    foe->pos());

    case SPELL_CLOUD_CONE:
        return !foe
               || !mons_should_cloud_cone(mon, 12 * mon->spell_hd(monspell),
                                          foe->pos());

    // Friendly monsters don't use polymorph, as it's likely to cause
    // runaway growth of an enemy.
    case SPELL_POLYMORPH:
        return friendly;

    case SPELL_MALIGN_GATEWAY:
        return !can_cast_malign_gateway();

    case SPELL_LEDAS_LIQUEFACTION:
        return !mon->stand_on_solid_ground()
                || liquefied(mon->pos());

    case SPELL_SIREN_SONG:
        return !_should_siren_sing(mon, false);

    case SPELL_AVATAR_SONG:
        return !_should_siren_sing(mon, true);

    case SPELL_REPEL_MISSILES:
        return mon->has_ench(ENCH_REPEL_MISSILES);

    case SPELL_DEFLECT_MISSILES:
        return mon->has_ench(ENCH_DEFLECT_MISSILES);

    case SPELL_HUNTING_CRY:
        return !foe;

    case SPELL_PARALYSIS_GAZE:
    case SPELL_CONFUSION_GAZE:
    case SPELL_DRAINING_GAZE:
        return !foe || !mon->can_see(foe);

#if TAG_MAJOR_VERSION == 34
    case SPELL_SUMMON_TWISTER:
    case SPELL_SHAFT_SELF:
    case SPELL_MISLEAD:
    case SPELL_SUMMON_SCORPIONS:
    case SPELL_SUMMON_ELEMENTAL:
#endif
    case SPELL_NO_SPELL:
        return true;

    default:
        return false;
    }
}
