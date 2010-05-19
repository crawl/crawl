/*  File:       mon-cast.cc
 *  Summary:    Monster spell casting.
 *  Written by: Linley Henzell
 */

#include "AppHdr.h"
#include "mon-cast.h"

#include "beam.h"
#include "cloud.h"
#include "colour.h"
#include "coordit.h"
#include "delay.h"
#include "database.h"
#include "effects.h"
#include "env.h"
#include "fprop.h"
#include "fight.h"
#include "ghost.h"
#include "items.h"
#include "libutil.h"
#include "misc.h"
#include "message.h"
#include "mon-behv.h"
#include "mon-clone.h"
#include "mon-iter.h"
#include "mon-place.h"
#include "mon-project.h"
#include "terrain.h"
#include "hints.h"
#include "mislead.h"
#include "mgen_data.h"
#include "coord.h"
#include "mon-speak.h"
#include "mon-stuff.h"
#include "mon-util.h"
#include "random.h"
#include "religion.h"
#include "shout.h"
#include "spl-util.h"
#include "spl-cast.h"
#include "spells1.h"
#include "spells3.h"
#include "state.h"
#include "stuff.h"
#include "areas.h"
#include "teleport.h"
#include "traps.h"
#include "view.h"
#include "viewchar.h"
#include "xom.h"

static bool _valid_mon_spells[NUM_SPELLS];

void init_mons_spells()
{
    monsters fake_mon;
    fake_mon.type       = MONS_BLACK_DRACONIAN;
    fake_mon.hit_points = 1;

    bolt pbolt;

    for (int i = 0; i < NUM_SPELLS; i++)
    {
        spell_type spell = (spell_type) i;

        _valid_mon_spells[i] = false;

        if (!is_valid_spell(spell))
            continue;

        if (setup_mons_cast(&fake_mon, pbolt, spell, true))
            _valid_mon_spells[i] = true;
    }
}

bool is_valid_mon_spell(spell_type spell)
{
    if (spell < 0 || spell >= NUM_SPELLS)
        return (false);

    return (_valid_mon_spells[spell]);
}

static void _scale_draconian_breath(bolt& beam, int drac_type)
{
    int scaling = 100;
    switch (drac_type)
    {
    case MONS_RED_DRACONIAN:
        beam.name       = "searing blast";
        beam.aux_source = "blast of searing breath";
        scaling         = 65;
        break;

    case MONS_WHITE_DRACONIAN:
        beam.name       = "chilling blast";
        beam.aux_source = "blast of chilling breath";
        beam.short_name = "frost";
        scaling         = 65;
        break;

    case MONS_PLAYER_GHOST: // draconians only
        beam.name       = "blast of negative energy";
        beam.aux_source = "blast of draining breath";
        beam.flavour    = BEAM_NEG;
        beam.colour     = DARKGREY;
        scaling         = 65;
        break;
    }
    beam.damage.size = scaling * beam.damage.size / 100;
}

static spell_type _draco_type_to_breath(int drac_type)
{
    switch (drac_type)
    {
    case MONS_BLACK_DRACONIAN:   return SPELL_LIGHTNING_BOLT;
    case MONS_MOTTLED_DRACONIAN: return SPELL_STICKY_FLAME_SPLASH;
    case MONS_YELLOW_DRACONIAN:  return SPELL_ACID_SPLASH;
    case MONS_GREEN_DRACONIAN:   return SPELL_POISONOUS_CLOUD;
    case MONS_PURPLE_DRACONIAN:  return SPELL_ISKENDERUNS_MYSTIC_BLAST;
    case MONS_RED_DRACONIAN:     return SPELL_FIRE_BREATH;
    case MONS_WHITE_DRACONIAN:   return SPELL_COLD_BREATH;
    case MONS_PALE_DRACONIAN:    return SPELL_STEAM_BALL;

    // Handled later.
    case MONS_PLAYER_GHOST:      return SPELL_DRACONIAN_BREATH;

    default:
        DEBUGSTR("Invalid monster using draconian breath spell");
        break;
    }

    return (SPELL_DRACONIAN_BREATH);
}

static bool _flavour_benefits_monster(beam_type flavour, monsters & monster)
{
    switch(flavour)
    {
    case BEAM_HASTE:
        return (!monster.has_ench(ENCH_HASTE));

    case BEAM_MIGHT:
        return (!monster.has_ench(ENCH_MIGHT));

    case BEAM_INVISIBILITY:
        return (!monster.has_ench(ENCH_INVIS));

    case BEAM_HEALING:
        return (monster.hit_points != monster.max_hit_points);

    default:
        return false;
    }
}

// Find an allied monster to cast a beneficial beam spell at.
// Only used for haste other at the moment.
static bool _set_allied_target(monsters * caster, bolt & pbolt)
{
    monsters * selected_target = NULL;
    int min_distance = INT_MAX;

    monster_type caster_genus = mons_genus(caster->type);

    for (monster_iterator targ(caster); targ; ++targ)
    {
        if (*targ != caster
            && mons_genus(targ->type) == caster_genus
            && mons_aligned(*targ, caster)
            && !targ->has_ench(ENCH_CHARM)
            && _flavour_benefits_monster(pbolt.flavour, **targ))
        {
            int targ_distance = grid_distance(targ->pos(), caster->pos());
            if (targ_distance < min_distance && targ_distance < pbolt.range)
            {
                min_distance = targ_distance;
                selected_target = *targ;
            }
        }
    }

    if (selected_target)
    {
        pbolt.target = selected_target->pos();
        return (true);
    }

    // Didn't find a target
    return (false);
}

bolt mons_spells( monsters *mons, spell_type spell_cast, int power,
                  bool check_validity )
{
    ASSERT(power > 0);

    bolt beam;

    // Initialise to some bogus values so we can catch problems.
    beam.name         = "****";
    beam.colour       = 1000;
    beam.hit          = -1;
    beam.damage       = dice_def( 1, 0 );
    beam.ench_power   = -1;
    beam.glyph        = 0;
    beam.flavour      = BEAM_NONE;
    beam.thrower      = KILL_MISC;
    beam.is_beam      = false;
    beam.is_explosion = false;

    // Sandblast is different, and gets range updated later
    if (spell_cast != SPELL_SANDBLAST)
        beam.range = spell_range(spell_cast, power, true, false);

    const int drac_type = (mons_genus(mons->type) == MONS_DRACONIAN)
                            ? draco_subspecies(mons) : mons->type;

    spell_type real_spell = spell_cast;

    if (spell_cast == SPELL_DRACONIAN_BREATH)
        real_spell = _draco_type_to_breath(drac_type);

    beam.glyph = dchar_glyph(DCHAR_FIRED_ZAP); // default
    beam.thrower = KILL_MON_MISSILE;
    beam.origin_spell = real_spell;

    // FIXME: this should use the zap_data[] struct from beam.cc!
    switch (real_spell)
    {
    case SPELL_MAGIC_DART:
        beam.colour   = LIGHTMAGENTA;
        beam.name     = "magic dart";
        beam.damage   = dice_def( 3, 4 + (power / 100) );
        beam.hit      = AUTOMATIC_HIT;
        beam.flavour  = BEAM_MMISSILE;
        break;

    case SPELL_THROW_FLAME:
        beam.colour   = RED;
        beam.name     = "puff of flame";
        beam.damage   = dice_def( 3, 5 + (power / 40) );
        beam.hit      = 25 + power / 40;
        beam.flavour  = BEAM_FIRE;
        break;

    case SPELL_THROW_FROST:
        beam.colour   = WHITE;
        beam.name     = "puff of frost";
        beam.damage   = dice_def( 3, 5 + (power / 40) );
        beam.hit      = 25 + power / 40;
        beam.flavour  = BEAM_COLD;
        break;

    case SPELL_SANDBLAST:
        beam.colour   = BROWN;
        beam.name     = "rocky blast";
        beam.damage   = dice_def( 3, 5 + (power / 40) );
        beam.hit      = 20 + power / 40;
        beam.flavour  = BEAM_FRAG;
        beam.range    = 2;      // spell_range() is wrong here
        break;

    case SPELL_DISPEL_UNDEAD:
        beam.flavour  = BEAM_DISPEL_UNDEAD;
        beam.damage   = dice_def( 3, std::min(6 + power / 10, 40) );
        beam.is_beam  = true;
        break;

    case SPELL_PARALYSE:
        beam.flavour  = BEAM_PARALYSIS;
        beam.is_beam  = true;
        break;

    case SPELL_SLOW:
        beam.flavour  = BEAM_SLOW;
        beam.is_beam  = true;
        break;

    case SPELL_HASTE_OTHER:
        beam.flavour  = BEAM_HASTE;
        beam.is_beam  = true;
        break;

    case SPELL_HASTE:              // (self)
        beam.flavour  = BEAM_HASTE;
        break;

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
        beam.flavour  = BEAM_SLEEP;
        beam.is_beam  = true;
        break;

    case SPELL_POLYMORPH_OTHER:
        beam.flavour  = BEAM_POLYMORPH;
        beam.is_beam  = true;
        // Be careful with this one.
        // Having allies mutate you is infuriating.
        beam.foe_ratio = 1000;
        break;

    case SPELL_VENOM_BOLT:
        beam.name     = "bolt of poison";
        beam.damage   = dice_def( 3, 6 + power / 13 );
        beam.colour   = LIGHTGREEN;
        beam.flavour  = BEAM_POISON;
        beam.hit      = 19 + power / 20;
        beam.is_beam  = true;
        break;

    case SPELL_POISON_ARROW:
        beam.name     = "poison arrow";
        beam.damage   = dice_def( 3, 7 + power / 12 );
        beam.colour   = LIGHTGREEN;
        beam.glyph    = dchar_glyph(DCHAR_FIRED_MISSILE);
        beam.flavour  = BEAM_POISON_ARROW;
        beam.hit      = 20 + power / 25;
        break;

    case SPELL_BOLT_OF_MAGMA:
        beam.name     = "bolt of magma";
        beam.damage   = dice_def( 3, 8 + power / 11 );
        beam.colour   = RED;
        beam.flavour  = BEAM_LAVA;
        beam.hit      = 17 + power / 25;
        beam.is_beam  = true;
        break;

    case SPELL_BOLT_OF_FIRE:
        beam.name     = "bolt of fire";
        beam.damage   = dice_def( 3, 8 + power / 11 );
        beam.colour   = RED;
        beam.flavour  = BEAM_FIRE;
        beam.hit      = 17 + power / 25;
        beam.is_beam  = true;
        break;

    case SPELL_THROW_ICICLE:
        beam.name     = "shard of ice";
        beam.damage   = dice_def( 3, 8 + power / 11 );
        beam.colour   = WHITE;
        beam.flavour  = BEAM_ICE;
        beam.hit      = 17 + power / 25;
        break;

    case SPELL_BOLT_OF_COLD:
        beam.name     = "bolt of cold";
        beam.damage   = dice_def( 3, 8 + power / 11 );
        beam.colour   = WHITE;
        beam.flavour  = BEAM_COLD;
        beam.hit      = 17 + power / 25;
        beam.is_beam  = true;
        break;

    case SPELL_PRIMAL_WAVE:
        beam.name     = "great wave of water";
        // Water attack is weaker than the pure elemental damage
        // attacks, but also less resistible.
        beam.damage   = dice_def( 3, 6 + power / 12 );
        beam.colour   = LIGHTBLUE;
        beam.flavour  = BEAM_WATER;
        // Huge wave of water is hard to dodge.
        beam.hit      = 20 + power / 20;
        beam.is_beam  = false;
        beam.glyph    = dchar_glyph(DCHAR_WAVY);
        break;

    case SPELL_FREEZING_CLOUD:
        beam.name     = "freezing blast";
        beam.damage   = dice_def( 2, 9 + power / 11 );
        beam.colour   = WHITE;
        beam.flavour  = BEAM_COLD;
        beam.hit      = 17 + power / 25;
        beam.is_beam  = true;
        beam.is_big_cloud = true;
        break;

    case SPELL_SHOCK:
        beam.name     = "zap";
        beam.damage   = dice_def( 1, 8 + (power / 20) );
        beam.colour   = LIGHTCYAN;
        beam.flavour  = BEAM_ELECTRICITY;
        beam.hit      = 17 + power / 20;
        beam.is_beam  = true;
        break;

    case SPELL_LIGHTNING_BOLT:
        beam.name     = "bolt of lightning";
        beam.damage   = dice_def( 3, 10 + power / 17 );
        beam.colour   = LIGHTCYAN;
        beam.flavour  = BEAM_ELECTRICITY;
        beam.hit      = 16 + power / 40;
        beam.is_beam  = true;
        break;

    case SPELL_INVISIBILITY:
        beam.flavour  = BEAM_INVISIBILITY;
        break;

    case SPELL_FIREBALL:
        beam.colour   = RED;
        beam.name     = "fireball";
        beam.damage   = dice_def( 3, 7 + power / 10 );
        beam.hit      = 40;
        beam.flavour  = BEAM_FIRE;
        beam.foe_ratio = 80;
        beam.is_explosion = true;
        break;

    case SPELL_FIRE_STORM:
        setup_fire_storm(mons, power / 2, beam);
        beam.foe_ratio = random_range(40, 55);
        break;

    case SPELL_ICE_STORM:
        beam.name           = "great blast of cold";
        beam.colour         = BLUE;
        beam.damage         = calc_dice( 10, 18 + power / 2 );
        beam.hit            = 20 + power / 10;    // 50: 25   100: 30
        beam.ench_power     = power;              // used for radius
        beam.flavour        = BEAM_ICE;           // half resisted
        beam.is_explosion   = true;
        beam.foe_ratio      = random_range(40, 55);
        break;

    case SPELL_HELLFIRE_BURST:
        beam.aux_source   = "burst of hellfire";
        beam.name         = "burst of hellfire";
        beam.ex_size      = 1;
        beam.flavour      = BEAM_HELLFIRE;
        beam.is_explosion = true;
        beam.colour       = RED;
        beam.aux_source.clear();
        beam.is_tracer    = false;
        beam.hit          = 20;
        beam.damage       = mons_foe_is_mons(mons) ? dice_def(5, 7)
                                                   : dice_def(3, 15);
        break;

    case SPELL_MINOR_HEALING:
        beam.flavour  = BEAM_HEALING;
        beam.hit      = 25 + (power / 5);
        break;

    case SPELL_TELEPORT_SELF:
        beam.flavour  = BEAM_TELEPORT;
        break;

    case SPELL_TELEPORT_OTHER:
        beam.flavour  = BEAM_TELEPORT;
        beam.is_beam  = true;
        break;

    case SPELL_LEHUDIBS_CRYSTAL_SPEAR:      // was splinters
        beam.name     = "crystal spear";
        beam.damage   = dice_def( 3, 16 + power / 10 );
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
        beam.damage   = dice_def( 3, 6 + power / 13 );
        beam.colour   = DARKGREY;
        beam.flavour  = BEAM_NEG;
        beam.hit      = 16 + power / 35;
        beam.is_beam  = true;
        break;

    case SPELL_ISKENDERUNS_MYSTIC_BLAST: // mystic blast
        beam.colour     = LIGHTMAGENTA;
        beam.name       = "orb of energy";
        beam.short_name = "energy";
        beam.damage     = dice_def( 3, 7 + (power / 14) );
        beam.hit        = 20 + (power / 20);
        beam.flavour    = BEAM_MMISSILE;
        break;

    case SPELL_STEAM_BALL:
        beam.colour   = LIGHTGREY;
        beam.name     = "ball of steam";
        beam.damage   = dice_def( 3, 7 + (power / 15) );
        beam.hit      = 20 + power / 20;
        beam.flavour  = BEAM_STEAM;
        break;

    case SPELL_PAIN:
        beam.flavour    = BEAM_PAIN;
        beam.damage     = dice_def( 1, 7 + (power / 20) );
        beam.ench_power = std::max(50, 8 * mons->hit_dice);
        beam.is_beam    = true;
        break;

    case SPELL_STICKY_FLAME_SPLASH:
    case SPELL_STICKY_FLAME:
        beam.colour   = RED;
        beam.name     = "sticky flame";
        beam.damage   = dice_def( 3, 3 + power / 50 );
        beam.hit      = 18 + power / 15;
        beam.flavour  = BEAM_FIRE;
        break;

    case SPELL_POISONOUS_CLOUD:
        beam.name     = "blast of poison";
        beam.damage   = dice_def( 3, 3 + power / 25 );
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
        beam.damage     = dice_def( 3, 20 );
        beam.hit        = 15 + power / 30;
        beam.flavour    = BEAM_NUKE; // a magical missile which destroys walls
        beam.is_beam    = true;
        break;

    case SPELL_STING:              // sting
        beam.colour   = GREEN;
        beam.name     = "sting";
        beam.damage   = dice_def( 1, 6 + power / 25 );
        beam.hit      = 60;
        beam.flavour  = BEAM_POISON;
        break;

    case SPELL_IRON_SHOT:
        beam.colour   = LIGHTCYAN;
        beam.name     = "iron shot";
        beam.damage   = dice_def( 3, 8 + (power / 9) );
        beam.hit      = 20 + (power / 25);
        beam.glyph    = dchar_glyph(DCHAR_FIRED_MISSILE);
        beam.flavour  = BEAM_MMISSILE;   // similarly unresisted thing
        break;

    case SPELL_STONE_ARROW:
        beam.colour   = LIGHTGREY;
        beam.name     = "stone arrow";
        beam.damage   = dice_def( 3, 5 + (power / 10) );
        beam.hit      = 14 + power / 35;
        beam.glyph    = dchar_glyph(DCHAR_FIRED_MISSILE);
        beam.flavour  = BEAM_MMISSILE;   // similarly unresisted thing
        break;

    case SPELL_POISON_SPLASH:
        beam.colour   = GREEN;
        beam.name     = "splash of poison";
        beam.damage   = dice_def( 1, 4 + power / 10 );
        beam.hit      = 16 + power / 20;
        beam.flavour  = BEAM_POISON;
        break;

    case SPELL_ACID_SPLASH:
        beam.colour   = YELLOW;
        beam.name     = "splash of acid";
        beam.damage   = dice_def( 3, 7 );
        beam.hit      = 20 + (3 * mons->hit_dice);
        beam.flavour  = BEAM_ACID;
        break;

    case SPELL_DISINTEGRATE:
        beam.flavour    = BEAM_DISINTEGRATION;
        beam.ench_power = 50;
        beam.damage     = dice_def( 1, 30 + (power / 10) );
        beam.is_beam    = true;
        break;

    case SPELL_MEPHITIC_CLOUD:          // swamp drake, player ghost
        beam.name     = "foul vapour";
        beam.damage   = dice_def(1,0);
        beam.colour   = GREEN;
        // Well, it works, even if the name isn't quite intuitive.
        beam.flavour  = BEAM_POTION_STINKING_CLOUD;
        beam.hit      = 14 + power / 30;
        beam.ench_power = power; // probably meaningless
        beam.is_explosion = true;
        beam.is_big_cloud = true;
        break;

    case SPELL_MIASMA:            // death drake
        beam.name     = "foul vapour";
        beam.damage   = dice_def( 3, 5 + power / 24 );
        beam.colour   = DARKGREY;
        beam.flavour  = BEAM_MIASMA;
        beam.hit      = 17 + power / 20;
        beam.is_beam  = true;
        beam.is_big_cloud = true;
        break;

    case SPELL_QUICKSILVER_BOLT:   // Quicksilver dragon
        beam.colour     = random_colour();
        beam.name       = "bolt of energy";
        beam.short_name = "energy";
        beam.damage     = dice_def( 3, 25 );
        beam.hit        = 16 + power / 25;
        beam.flavour    = BEAM_MMISSILE;
        break;

    case SPELL_HELLFIRE:           // fiend's hellfire
        beam.name         = "blast of hellfire";
        beam.aux_source   = "blast of hellfire";
        beam.colour       = RED;
        beam.damage       = dice_def( 3, 20 );
        beam.hit          = 24;
        beam.flavour      = BEAM_HELLFIRE;
        beam.is_beam      = true;
        beam.is_explosion = true;
        break;

    case SPELL_METAL_SPLINTERS:
        beam.name       = "spray of metal splinters";
        beam.short_name = "metal splinters";
        beam.damage     = dice_def( 3, 20 + power / 20 );
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
        beam.damage     = dice_def( 3, (mons->hit_dice * 2) );
        beam.colour     = RED;
        beam.hit        = 30;
        beam.flavour    = BEAM_FIRE;
        beam.is_beam    = true;
        break;

    case SPELL_COLD_BREATH:
        beam.name       = "blast of cold";
        beam.aux_source = "blast of icy breath";
        beam.short_name = "frost";
        beam.damage     = dice_def( 3, (mons->hit_dice * 2) );
        beam.colour     = WHITE;
        beam.hit        = 30;
        beam.flavour    = BEAM_COLD;
        beam.is_beam    = true;
        break;

    case SPELL_DRACONIAN_BREATH:
        beam.damage      = dice_def( 3, (mons->hit_dice * 2) );
        beam.hit         = 30;
        beam.is_beam     = true;
        break;

    case SPELL_PORKALATOR:
        beam.name     = "porkalator";
        beam.glyph    = 0;
        beam.flavour  = BEAM_PORKALATOR;
        beam.thrower  = KILL_MON_MISSILE;
        beam.is_beam  = true;
        break;

    case SPELL_IOOD: // tracer only
        beam.flavour  = BEAM_NUKE;
        beam.is_beam  = true;
        break;

    case SPELL_SUNRAY:
        beam.colour   = ETC_HOLY;
        beam.name     = "ray of light";
        beam.damage   = dice_def( 3, 7 + (power / 12) );
        beam.hit      = 10 + power / 25; // lousy accuracy, but ignores RMsl
        beam.flavour  = BEAM_LIGHT;
        break;

    default:
        if (check_validity)
        {
            beam.flavour = NUM_BEAMS;
            return (beam);
        }

        if (!is_valid_spell(real_spell))
            DEBUGSTR("Invalid spell #%d cast by %s", (int) real_spell,
                     mons->name(DESC_PLAIN, true).c_str());

        DEBUGSTR("Unknown monster spell '%s' cast by %s",
                 spell_title(real_spell),
                 mons->name(DESC_PLAIN, true).c_str());

        return (beam);
    }

    if (beam.is_enchantment())
    {
        beam.glyph = 0;
        beam.name = "";
    }

    if (spell_cast == SPELL_DRACONIAN_BREATH)
        _scale_draconian_breath(beam, drac_type);

    // Accuracy is lowered by one quarter if the dragon is attacking
    // a target that is wielding a weapon of dragon slaying (which
    // makes the dragon/draconian avoid looking at the foe).
    // FIXME: This effect is not yet implemented for player draconians
    // or characters in dragon form breathing at monsters wielding a
    // weapon with this brand.
    if (is_dragonkind(mons))
    {
        if (actor *foe = mons->get_foe())
        {
            if (const item_def *weapon = foe->weapon())
            {
                if (get_weapon_brand(*weapon) == SPWPN_DRAGON_SLAYING)
                {
                    beam.hit *= 3;
                    beam.hit /= 4;
                }
            }
        }
    }

    return (beam);
}

static bool _los_free_spell(spell_type spell_cast)
{
    return (spell_cast == SPELL_HELLFIRE_BURST
        || spell_cast == SPELL_BRAIN_FEED
        || spell_cast == SPELL_SMITING
        || spell_cast == SPELL_HAUNT
        || spell_cast == SPELL_FIRE_STORM
        || spell_cast == SPELL_AIRSTRIKE
        || spell_cast == SPELL_MISLEAD);
}

// Set up bolt structure for monster spell casting.
bool setup_mons_cast(monsters *monster, bolt &pbolt, spell_type spell_cast,
                     bool check_validity)
{
    // always set these -- used by things other than fire_beam()

    // [ds] Used to be 12 * MHD and later buggily forced to -1 downstairs.
    // Setting this to a more realistic number now that that bug is
    // squashed.
    pbolt.ench_power = 4 * monster->hit_dice;

    if (spell_cast == SPELL_TELEPORT_SELF)
        pbolt.ench_power = 2000;
    else if (spell_cast == SPELL_SLEEP)
        pbolt.ench_power = 6 * monster->hit_dice;

    pbolt.beam_source = monster->mindex();

    // Convenience for the hapless innocent who assumes that this
    // damn function does all possible setup. [ds]
    if (pbolt.target.origin())
        pbolt.target = monster->target;

    // Set bolt type and range.
    if (_los_free_spell(spell_cast))
    {
        pbolt.range = 0;
        pbolt.glyph = 0;
        switch (spell_cast)
        {
        case SPELL_BRAIN_FEED:
        case SPELL_MISLEAD:
        case SPELL_SMITING:
        case SPELL_AIRSTRIKE:
            return (true);
        default:
            // Other spells get normal setup:
            break;
        }
    }

    // The below are no-ops since they don't involve direct_effect,
    // fire_tracer, or beam.
    switch (spell_cast)
    {
    case SPELL_SUMMON_SMALL_MAMMALS:
    case SPELL_MAJOR_HEALING:
    case SPELL_VAMPIRE_SUMMON:
    case SPELL_SHADOW_CREATURES:       // summon anything appropriate for level
    case SPELL_FAKE_RAKSHASA_SUMMON:
    case SPELL_FAKE_MARA_SUMMON:
    case SPELL_SUMMON_ILLUSION:
    case SPELL_SUMMON_RAKSHASA:
    case SPELL_SUMMON_DEMON:
    case SPELL_SUMMON_UGLY_THING:
    case SPELL_ANIMATE_DEAD:
    case SPELL_CALL_IMP:
    case SPELL_SUMMON_SCORPIONS:
    case SPELL_SUMMON_SWARM:
    case SPELL_SUMMON_UFETUBUS:
    case SPELL_SUMMON_BEAST:       // Geryon
    case SPELL_SUMMON_UNDEAD:      // summon undead around player
    case SPELL_SUMMON_ICE_BEAST:
    case SPELL_SUMMON_MUSHROOMS:
    case SPELL_CONJURE_BALL_LIGHTNING:
    case SPELL_SUMMON_DRAKES:
    case SPELL_SUMMON_HORRIBLE_THINGS:
    case SPELL_HAUNT:
    case SPELL_SYMBOL_OF_TORMENT:
    case SPELL_SUMMON_GREATER_DEMON:
    case SPELL_CANTRIP:
    case SPELL_BERSERKER_RAGE:
    case SPELL_SWIFTNESS:
    case SPELL_WATER_ELEMENTALS:
    case SPELL_FIRE_ELEMENTALS:
    case SPELL_AIR_ELEMENTALS:
    case SPELL_EARTH_ELEMENTALS:
    case SPELL_KRAKEN_TENTACLES:
    case SPELL_BLINK:
    case SPELL_CONTROLLED_BLINK:
    case SPELL_BLINK_RANGE:
    case SPELL_BLINK_AWAY:
    case SPELL_BLINK_CLOSE:
    case SPELL_TOMB_OF_DOROKLOHE:
    case SPELL_CHAIN_LIGHTNING:    // the only user is reckless
    case SPELL_SUMMON_EYEBALLS:
    case SPELL_SUMMON_BUTTERFLIES:
    case SPELL_MISLEAD:
    case SPELL_CALL_TIDE:
    case SPELL_INK_CLOUD:
    case SPELL_SILENCE:
    case SPELL_AWAKEN_FOREST:
        return (true);
    default:
        if (check_validity)
        {
            bolt beam = mons_spells(monster, spell_cast, 1, true);
            return (beam.flavour != NUM_BEAMS);
        }
        break;
    }

    // Need to correct this for power of spellcaster
    int power = 12 * monster->hit_dice;

    bolt theBeam         = mons_spells(monster, spell_cast, power);

    // [ds] remind me again why we're doing this piecemeal copying?
    pbolt.origin_spell   = theBeam.origin_spell;
    pbolt.colour         = theBeam.colour;
    pbolt.range          = theBeam.range;
    pbolt.hit            = theBeam.hit;
    pbolt.damage         = theBeam.damage;

    if (theBeam.ench_power != -1)
        pbolt.ench_power = theBeam.ench_power;

    pbolt.glyph          = theBeam.glyph;
    pbolt.flavour        = theBeam.flavour;
    pbolt.thrower        = theBeam.thrower;
    pbolt.name           = theBeam.name;
    pbolt.short_name     = theBeam.short_name;
    pbolt.is_beam        = theBeam.is_beam;
    pbolt.source         = monster->pos();
    pbolt.is_tracer      = false;
    pbolt.is_explosion   = theBeam.is_explosion;
    pbolt.ex_size        = theBeam.ex_size;

    pbolt.foe_ratio      = theBeam.foe_ratio;

    if (!pbolt.is_enchantment())
        pbolt.aux_source = pbolt.name;
    else
        pbolt.aux_source.clear();

    if (spell_cast == SPELL_HASTE
        || spell_cast == SPELL_MIGHT
        || spell_cast == SPELL_INVISIBILITY
        || spell_cast == SPELL_MINOR_HEALING
        || spell_cast == SPELL_TELEPORT_SELF
        || spell_cast == SPELL_SILENCE)
    {
        pbolt.target = monster->pos();
    }
    else if (spell_cast == SPELL_PORKALATOR && one_chance_in(3))
    {
        monsters*    targ     = NULL;
        int          count    = 0;
        monster_type hog_type = MONS_HOG;
        for (monster_iterator mi(monster); mi; ++mi)
        {
            hog_type = MONS_HOG;
            if (mi->holiness() == MH_DEMONIC)
                hog_type = MONS_HELL_HOG;
            else if (mi->holiness() != MH_NATURAL)
                continue;

            if (mi->type != hog_type
                && mons_aligned(monster, *mi)
                && mons_power(hog_type) + random2(4) >= mons_power(mi->type)
                && (!mi->can_use_spells() || coinflip())
                && one_chance_in(++count))
            {
                targ = *mi;
            }
        }

        if (targ)
        {
            pbolt.target = targ->pos();
#ifdef DEBUG_DIAGNOSTICS
            mprf("Porkalator: targeting %s instead",
                 targ->name(DESC_PLAIN).c_str());
#endif
            monster_polymorph(targ, hog_type);
        }
        // else target remains as specified
    }
    return (true);
}

// Returns a suitable breath weapon for the draconian; does not handle all
// draconians, does fire a tracer.
static spell_type _get_draconian_breath_spell( monsters *monster )
{
    spell_type draco_breath = SPELL_NO_SPELL;

    if (mons_genus( monster->type ) == MONS_DRACONIAN)
    {
        switch (draco_subspecies( monster ))
        {
        case MONS_DRACONIAN:
        case MONS_YELLOW_DRACONIAN:     // already handled as ability
            break;
        default:
            draco_breath = SPELL_DRACONIAN_BREATH;
            break;
        }
    }


    if (draco_breath != SPELL_NO_SPELL)
    {
        // [ds] Check line-of-fire here. It won't happen elsewhere.
        bolt beem;
        setup_mons_cast(monster, beem, draco_breath);

        fire_tracer(monster, beem);

        if (!mons_should_fire(beem))
            draco_breath = SPELL_NO_SPELL;
    }

    return (draco_breath);
}

static bool _is_emergency_spell(const monster_spells &msp, int spell)
{
    // If the emergency spell appears early, it's probably not a dedicated
    // escape spell.
    for (int i = 0; i < 5; ++i)
        if (msp[i] == spell)
            return (false);

    return (msp[5] == spell);
}

// Function should return false if friendlies shouldn't animate any dead.
// Currently, this only happens if the player is in the middle of butchering
// a corpse (infurating), or if they are less than satiated.  Only applies
// to friendly corpse animators. {due}
static bool _animate_dead_okay()
{
    // It's always okay in the arena.
    if (crawl_state.game_is_arena())
        return (true);

    if (is_butchering())
        return (false);

    // TODO: Could probably check herbivorousness here too, but this should be
    // enough for the moment.
    if (you.hunger_state < HS_SATIATED)
        return (false);

    return (true);
}

//---------------------------------------------------------------
//
// handle_spell
//
// Give the monster a chance to cast a spell. Returns true if
// a spell was cast.
//
//---------------------------------------------------------------
bool handle_mon_spell(monsters *monster, bolt &beem)
{
    bool monsterNearby = mons_near(monster);
    bool finalAnswer   = false;   // as in: "Is that your...?" {dlb}
    const spell_type draco_breath = _get_draconian_breath_spell(monster);
    actor *foe = monster->get_foe();

    // A polymorphed unique will retain his or her spells even in another
    // form. If the new form has the SPELLCASTER flag, casting happens as
    // normally, otherwise we need to enforce it, but it only happens with
    // a 50% chance.
    const bool spellcasting_poly(
        !monster->can_use_spells()
        && mons_class_flag(monster->type, M_SPEAKS)
        && monster->has_spells());

    if (is_sanctuary(monster->pos()) && !monster->wont_attack())
        return (false);

    // Yes, there is a logic to this ordering {dlb}:
    if (monster->asleep()
        || monster->submerged()
        || (!monster->can_use_spells()
            && !spellcasting_poly
            && draco_breath == SPELL_NO_SPELL))
    {
        return (false);
    }

    // If the monster's a priest, assume summons come from priestly
    // abilities, in which case they'll have the same god.  If the
    // monster is neither a priest nor a wizard, assume summons come
    // from intrinsic abilities, in which case they'll also have the
    // same god.
    const bool priest = monster->is_priest();
    const bool wizard = monster->is_actual_spellcaster();
    god_type god = (priest || !(priest || wizard)) ? monster->god : GOD_NO_GOD;

    if (silenced(monster->pos())
        && (priest || wizard || spellcasting_poly
            || mons_class_flag(monster->type, M_SPELL_NO_SILENT)))
    {
        return (false);
    }

    // Shapeshifters don't get spells.
    if (monster->is_shapeshifter() && (priest || wizard))
        return (false);
    else if (mons_is_confused(monster, false))
        return (false);
    else if (monster->type == MONS_PANDEMONIUM_DEMON
             && !monster->ghost->spellcaster)
    {
        return (false);
    }
    else if (random2(200) > monster->hit_dice + 50
             || monster->type == MONS_BALL_LIGHTNING && coinflip())
    {
        return (false);
    }
    else if (spellcasting_poly && coinflip()) // 50% chance of not casting
        return (false);
    else
    {
        spell_type spell_cast = SPELL_NO_SPELL;
        monster_spells hspell_pass(monster->spells);

        if (!mon_enemies_around(monster))
        {
            // Force the casting of dig when the player is not visible -
            // this is EVIL!
            // only do this for monsters that are actually seeking out a
            // hostile target -doy
            if (monster->has_spell(SPELL_DIG)
                && mons_is_seeking(monster)
                && !(monster->wont_attack() && monster->foe == MHITYOU))
            {
                spell_cast = SPELL_DIG;
                finalAnswer = true;
            }
            else if ((monster->has_spell(SPELL_MINOR_HEALING)
                         || monster->has_spell(SPELL_MAJOR_HEALING))
                     && monster->hit_points < monster->max_hit_points)
            {
                // The player's out of sight!
                // Quick, let's take a turn to heal ourselves. -- bwr
                spell_cast = monster->has_spell(SPELL_MAJOR_HEALING) ?
                                 SPELL_MAJOR_HEALING : SPELL_MINOR_HEALING;
                finalAnswer = true;
            }
            else if (mons_is_fleeing(monster) || monster->pacified())
            {
                // Since the player isn't around, we'll extend the monster's
                // normal choices to include the self-enchant slot.
                int foundcount = 0;
                for (int i = NUM_MONSTER_SPELL_SLOTS - 1; i >= 0; --i)
                {
                    if (ms_useful_fleeing_out_of_sight(monster, hspell_pass[i])
                        && one_chance_in(++foundcount))
                    {
                        spell_cast = hspell_pass[i];
                        finalAnswer = true;
                    }
                }
            }
            else if (monster->foe == MHITYOU && !monsterNearby)
                return (false);
        }

        // Monsters caught in a net try to get away.
        // This is only urgent if enemies are around.
        if (!finalAnswer && mon_enemies_around(monster)
            && monster->caught() && one_chance_in(4))
        {
            for (int i = 0; i < NUM_MONSTER_SPELL_SLOTS; ++i)
            {
                if (ms_quick_get_away(monster, hspell_pass[i]))
                {
                    spell_cast = hspell_pass[i];
                    finalAnswer = true;
                    break;
                }
            }
        }

        // Promote the casting of useful spells for low-HP monsters.
        // (kraken should always cast their escape spell of inky).
        if (!finalAnswer
            && monster->hit_points < monster->max_hit_points / 4
            && (!one_chance_in(4) || monster->type == MONS_KRAKEN))
        {
            // Note: There should always be at least some chance we don't
            // get here... even if the monster is on its last HP.  That
            // way we don't have to worry about monsters infinitely casting
            // Healing on themselves (e.g. orc high priests).
            if ((mons_is_fleeing(monster) || monster->pacified())
                && ms_low_hitpoint_cast(monster, hspell_pass[5]))
            {
                spell_cast = hspell_pass[5];
                finalAnswer = true;
            }

            if (!finalAnswer)
            {
                int found_spell = 0;
                for (int i = 0; i < NUM_MONSTER_SPELL_SLOTS; ++i)
                {
                    if (ms_low_hitpoint_cast(monster, hspell_pass[i])
                        && one_chance_in(++found_spell))
                    {
                        spell_cast  = hspell_pass[i];
                        finalAnswer = true;
                    }
                }
            }
        }

        if (!finalAnswer)
        {
            // If nothing found by now, safe friendlies and good
            // neutrals will rarely cast.
            if (monster->wont_attack() && !mon_enemies_around(monster)
                && !one_chance_in(10))
            {
                return (false);
            }

            // Remove healing/invis/haste if we don't need them.
            int num_no_spell = 0;

            for (int i = 0; i < NUM_MONSTER_SPELL_SLOTS; ++i)
            {
                if (hspell_pass[i] == SPELL_NO_SPELL)
                    num_no_spell++;
                else if (ms_waste_of_time(monster, hspell_pass[i])
                         || hspell_pass[i] == SPELL_DIG)
                {
                    // Should monster not have selected dig by now,
                    // it never will.
                    hspell_pass[i] = SPELL_NO_SPELL;
                    num_no_spell++;
                }
            }

            // If no useful spells... cast no spell.
            if (num_no_spell == NUM_MONSTER_SPELL_SLOTS
                && draco_breath == SPELL_NO_SPELL)
            {
                return (false);
            }

            const bolt orig_beem = beem;
            // Up to four tries to pick a spell.
            for (int loopy = 0; loopy < 4; ++loopy)
            {
                beem = orig_beem;

                bool spellOK = false;

                // Setup spell - monsters that are fleeing or pacified
                // and leaving the level will always try to choose their
                // emergency spell.
                if (mons_is_fleeing(monster) || monster->pacified())
                {
                    spell_cast = (one_chance_in(5) ? SPELL_NO_SPELL
                                                   : hspell_pass[5]);

                    // Pacified monsters leaving the level won't choose
                    // emergency spells harmful to the area.
                    if (spell_cast != SPELL_NO_SPELL
                        && monster->pacified()
                        && spell_harms_area(spell_cast))
                    {
                        spell_cast = SPELL_NO_SPELL;
                    }
                }
                else
                {
                    // Randomly picking one of the non-emergency spells:
                    spell_cast = hspell_pass[random2(5)];
                }

                if (spell_cast == SPELL_NO_SPELL)
                    continue;

                // Setup the spell.
                setup_mons_cast(monster, beem, spell_cast);

                // Try to find a nearby ally to haste
                if (spell_cast == SPELL_HASTE_OTHER
                    && !_set_allied_target(monster, beem))
                {
                    spell_cast = SPELL_NO_SPELL;
                    continue;
                }

                // Alligators shouldn't spam swiftness.
                if (spell_cast == SPELL_SWIFTNESS
                    && monster->type == MONS_ALLIGATOR
                    && ((long) monster->number + random2avg(170, 5) >=
                        you.num_turns))
                {
                    spell_cast = SPELL_NO_SPELL;
                    continue;
                }

                // And Mara shouldn't cast player ghost if he can't
                // see the player
                if (spell_cast == SPELL_SUMMON_ILLUSION
                    && monster->type == MONS_MARA
                    && (!foe
                        || !monster->can_see(foe)
                        || !actor_is_illusion_cloneable(foe)))
                {
                    spell_cast = SPELL_NO_SPELL;
                    continue;
                }

                // beam-type spells requiring tracers
                if (spell_needs_tracer(spell_cast))
                {
                    const bool explode =
                        spell_is_direct_explosion(spell_cast);
                    fire_tracer(monster, beem, explode);
                    // Good idea?
                    if (mons_should_fire(beem))
                        spellOK = true;
                }
                else
                {
                    // All direct-effect/summoning/self-enchantments/etc.
                    spellOK = true;

                    if (ms_direct_nasty(spell_cast)
                        && mons_aligned(monster, (monster->foe == MHITYOU) ?
                           &you : foe)) // foe=get_foe() is NULL for friendlies
                    {                   // targetting you, which is bad here.
                        spellOK = false;
                    }
                    else if (monster->foe == MHITYOU || monster->foe == MHITNOT)
                    {
                        // XXX: Note the crude hack so that monsters can
                        // use ME_ALERT to target (we should really have
                        // a measure of time instead of peeking to see
                        // if the player is still there). -- bwr
                        if (!you.visible_to(monster)
                            && (monster->target != you.pos() || coinflip()))
                        {
                            spellOK = false;
                        }
                    }
                    else if (!monster->can_see(&menv[monster->foe]))
                    {
                        spellOK = false;
                    }
                    else if (monster->type == MONS_DAEVA
                             && monster->god == GOD_SHINING_ONE)
                    {
                        const monsters *mon = &menv[monster->foe];

                        // Don't allow TSO-worshipping daevas to make
                        // unchivalric magic attacks, except against
                        // appropriate monsters.
                        if (is_unchivalric_attack(monster, mon)
                            && !tso_unchivalric_attack_safe_monster(mon))
                        {
                            spellOK = false;
                        }
                    }
                }

                // If not okay, then maybe we'll cast a defensive spell.
                if (!spellOK)
                {
                    spell_cast = (coinflip() ? hspell_pass[2]
                                             : SPELL_NO_SPELL);

                    // don't cast a targetted spell at the player if the
                    // monster is friendly and targetting the player -doy
                    if ((monster->wont_attack() && monster->foe == MHITYOU) &&
                        spell_needs_tracer(spell_cast) &&
                        spell_needs_foe(spell_cast) &&
                        spell_harms_target(spell_cast)) {
                        spell_cast = SPELL_NO_SPELL;
                    }
                }

                if (spell_cast != SPELL_NO_SPELL)
                    break;
            }
        }

        // If there's otherwise no ranged attack use the breath weapon.
        // The breath weapon is also occasionally used.
        if (draco_breath != SPELL_NO_SPELL
            && (spell_cast == SPELL_NO_SPELL
                 || !_is_emergency_spell(hspell_pass, spell_cast)
                    && one_chance_in(4))
            && !player_or_mon_in_sanct(monster))
        {
            spell_cast = draco_breath;
            finalAnswer = true;
        }

        // Should the monster *still* not have a spell, well, too bad {dlb}:
        if (spell_cast == SPELL_NO_SPELL)
            return (false);

        // Friendly monsters don't use polymorph other, for fear of harming
        // the player.
        if (spell_cast == SPELL_POLYMORPH_OTHER && monster->friendly())
            return (false);

        // Try to animate dead: if nothing rises, pretend we didn't cast it.
        if (spell_cast == SPELL_ANIMATE_DEAD)
        {
            if (monster->friendly() && !_animate_dead_okay())
                return (false);

            if (!animate_dead(monster, 100, SAME_ATTITUDE(monster),
                             monster->foe, monster, "", god, false))
            {
                return (false);
            }
        }

        if (monster->type == MONS_BALL_LIGHTNING)
            monster->hit_points = -1;

        // FINALLY! determine primary spell effects {dlb}:
        if (spell_cast == SPELL_BLINK || spell_cast == SPELL_CONTROLLED_BLINK)
        {
            // Why only cast blink if nearby? {dlb}
            if (monsterNearby)
            {
                mons_cast_noise(monster, beem, spell_cast);
                monster_blink(monster);

                monster->lose_energy(EUT_SPELL);
            }
            else
                return (false);
        }
        else if (spell_cast == SPELL_BLINK_RANGE)
            blink_range(monster);
        else if (spell_cast == SPELL_BLINK_AWAY)
            blink_away(monster);
        else if (spell_cast == SPELL_BLINK_CLOSE)
            blink_close(monster);
        else
        {
            if (spell_needs_foe(spell_cast))
                make_mons_stop_fleeing(monster);

            mons_cast(monster, beem, spell_cast);
            monster->lose_energy(EUT_SPELL);
        }
    } // end "if mons_class_flag(monster->type, M_SPELLCASTER)

    return (true);
}

static int _monster_abjure_square(const coord_def &pos,
                                  int pow, int actual,
                                  int wont_attack)
{
    monsters *target = monster_at(pos);
    if (target == NULL)
        return (0);

    if (!target->alive()
        || ((bool)wont_attack == target->wont_attack()))
    {
        return (0);
    }

    int duration;

    if (!target->is_summoned(&duration))
        return (0);

    pow = std::max(20, fuzz_value(pow, 40, 25));

    if (!actual)
        return (pow > 40 || pow >= duration);

    // TSO and Trog's abjuration protection.
    bool shielded = false;
    if (you.religion == GOD_SHINING_ONE)
    {
        pow = pow * (30 - target->hit_dice) / 30;
        if (pow < duration)
        {
            simple_god_message(" protects your fellow warrior from evil "
                               "magic!");
            shielded = true;
        }
    }
    else if (you.religion == GOD_TROG)
    {
        pow = pow * 4 / 5;
        if (pow < duration)
        {
            simple_god_message(" shields your ally from puny magic!");
            shielded = true;
        }
    }
    else if (is_sanctuary(target->pos()))
    {
        pow = 0;
        mpr("Zin's power protects your fellow warrior from evil magic!",
            MSGCH_GOD);
        shielded = true;
    }

    dprf("Abj: dur: %d, pow: %d, ndur: %d", duration, pow, duration - pow);

    mon_enchant abj = target->get_ench(ENCH_ABJ);
    if (!target->lose_ench_duration(abj, pow))
    {
        if (!shielded)
            simple_monster_message(target, " shudders.");
        return (1);
    }

    return (0);
}

static int _apply_radius_around_square( const coord_def &c, int radius,
                                int (*fn)(const coord_def &, int, int, int),
                                int pow, int par1, int par2)
{
    int res = 0;
    for (int yi = -radius; yi <= radius; ++yi)
    {
        const coord_def c1(c.x - radius, c.y + yi);
        const coord_def c2(c.x + radius, c.y + yi);
        if (in_bounds(c1))
            res += fn(c1, pow, par1, par2);
        if (in_bounds(c2))
            res += fn(c2, pow, par1, par2);
    }

    for (int xi = -radius + 1; xi < radius; ++xi)
    {
        const coord_def c1(c.x + xi, c.y - radius);
        const coord_def c2(c.x + xi, c.y + radius);
        if (in_bounds(c1))
            res += fn(c1, pow, par1, par2);
        if (in_bounds(c2))
            res += fn(c2, pow, par1, par2);
    }
    return (res);
}

static int _monster_abjuration(const monsters *caster, bool actual)
{
    const bool wont_attack = caster->wont_attack();
    int maffected = 0;

    if (actual)
        mpr("Send 'em back where they came from!");

    int pow = std::min(caster->hit_dice * 90, 2500);

    // Abjure radius.
    for (int rad = 1; rad < 5 && pow >= 30; ++rad)
    {
        int number_hit =
            _apply_radius_around_square(caster->pos(), rad,
                                        _monster_abjure_square,
                                        pow, actual, wont_attack);

        maffected += number_hit;

        // Each affected monster drops power.
        //
        // We could further tune this by the actual amount of abjuration
        // damage done to each summon, but the player will probably never
        // notice. :-)
        while (number_hit-- > 0)
            pow = pow * 90 / 100;

        pow /= 2;
    }
    return (maffected);
}


static bool _mons_abjured(monsters *monster, bool nearby)
{
    if (nearby && _monster_abjuration(monster, false) > 0
        && coinflip())
    {
        _monster_abjuration(monster, true);
        return (true);
    }

    return (false);
}

static monster_type _pick_swarmer()
{
    static monster_type swarmers[] =
    {
        MONS_KILLER_BEE, MONS_SCORPION, MONS_WORM,
        MONS_GIANT_MOSQUITO, MONS_GIANT_BEETLE, MONS_GIANT_BLOWFLY,
        MONS_WOLF_SPIDER, MONS_BUTTERFLY, MONS_YELLOW_WASP,
        MONS_GIANT_ANT,
    };

    return (RANDOM_ELEMENT(swarmers));
}

static monster_type _pick_random_wraith()
{
    static monster_type wraiths[] =
    {
        MONS_WRAITH, MONS_SHADOW_WRAITH, MONS_FREEZING_WRAITH,
        MONS_SPECTRAL_WARRIOR, MONS_PHANTOM, MONS_HUNGRY_GHOST,
        MONS_FLAYED_GHOST
    };

    return (RANDOM_ELEMENT(wraiths));
}

static monster_type _pick_horrible_thing()
{
    return (one_chance_in(4) ? MONS_TENTACLED_MONSTROSITY
                             : MONS_ABOMINATION_LARGE);
}

static monster_type _pick_undead_summon()
{
    static monster_type undead[] =
    {
        MONS_NECROPHAGE, MONS_GHOUL, MONS_HUNGRY_GHOST, MONS_FLAYED_GHOST,
        MONS_ZOMBIE_SMALL, MONS_SKELETON_SMALL, MONS_SIMULACRUM_SMALL,
        MONS_FLYING_SKULL, MONS_FLAMING_CORPSE, MONS_MUMMY, MONS_VAMPIRE,
        MONS_WIGHT, MONS_WRAITH, MONS_SHADOW_WRAITH, MONS_FREEZING_WRAITH,
        MONS_SPECTRAL_WARRIOR, MONS_ZOMBIE_LARGE, MONS_SKELETON_LARGE,
        MONS_SIMULACRUM_LARGE, MONS_SHADOW
    };

    return (RANDOM_ELEMENT(undead));
}

static void _do_high_level_summon(monsters *monster, bool monsterNearby,
                                  spell_type spell_cast,
                                  monster_type (*mpicker)(), int nsummons,
                                  god_type god, coord_def *target = NULL)
{
    if (_mons_abjured(monster, monsterNearby))
        return;

    const int duration = std::min(2 + monster->hit_dice / 5, 6);

    for (int i = 0; i < nsummons; ++i)
    {
        monster_type which_mons = mpicker();

        if (which_mons == MONS_NO_MONSTER)
            continue;

        create_monster(
            mgen_data(which_mons, SAME_ATTITUDE(monster), monster,
                      duration, spell_cast, target ? *target : monster->pos(),
                      monster->foe, 0, god));
    }
}

// Returns true if a message referring to the player's legs makes sense.
static bool _legs_msg_applicable()
{
    return (you.species != SP_NAGA
            && (you.species != SP_MERFOLK || !you.swimming()));
}

void mons_cast_haunt(monsters *monster)
{
    coord_def fpos;

    switch (monster->foe)
    {
    case MHITNOT:
        return;

    case MHITYOU:
        fpos = you.pos();
        break;

    default:
        fpos = menv[monster->foe].pos();
    }

    _do_high_level_summon(monster, mons_near(monster), SPELL_HAUNT,
                          _pick_random_wraith, random_range(3, 6),
                          GOD_NO_GOD, &fpos);
}

static void _mons_cast_summon_illusion(monsters *monster, spell_type spell)
{
    actor *foe = monster->get_foe();
    if (!foe || !actor_is_illusion_cloneable(foe))
        return;

    mons_summon_illusion_from(monster, foe, spell);
}

void mons_cast(monsters *monster, bolt &pbolt, spell_type spell_cast,
               bool do_noise, bool special_ability)
{
    // Always do setup.  It might be done already, but it doesn't hurt
    // to do it again (cheap).
    setup_mons_cast(monster, pbolt, spell_cast);

    // single calculation permissible {dlb}
    bool monsterNearby = mons_near(monster);

    int sumcount = 0;
    int sumcount2;
    int duration = 0;

#ifdef DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS, "Mon #%d casts %s (#%d)",
         monster->mindex(), spell_title(spell_cast), spell_cast);
#endif

    if (spell_cast == SPELL_CANTRIP)
        do_noise = false;       // Spell itself does the messaging.

    if (_los_free_spell(spell_cast) && !spell_is_direct_explosion(spell_cast))
    {
        if (monster->foe == MHITYOU || monster->foe == MHITNOT)
        {
            if (monsterNearby)
            {
                if (do_noise)
                    mons_cast_noise(monster, pbolt, spell_cast,
                                    special_ability);
                direct_effect(monster, spell_cast, pbolt, &you);
            }
            return;
        }

        if (do_noise)
            mons_cast_noise(monster, pbolt, spell_cast, special_ability);
        direct_effect(monster, spell_cast, pbolt, monster->get_foe());
        return;
    }

#ifdef ASSERTS
    const unsigned int flags = get_spell_flags(spell_cast);

    ASSERT(!(flags & (SPFLAG_TESTING | SPFLAG_MAPPING)));

    // Targeted spells need a valid target.
    ASSERT(!(flags & SPFLAG_TARGETING_MASK) || in_bounds(pbolt.target));
#endif

    if (do_noise)
        mons_cast_noise(monster, pbolt, spell_cast, special_ability);

    // If the monster's a priest, assume summons come from priestly
    // abilities, in which case they'll have the same god.  If the
    // monster is neither a priest nor a wizard, assume summons come
    // from intrinsic abilities, in which case they'll also have the
    // same god.
    const bool priest = monster->is_priest();
    const bool wizard = monster->is_actual_spellcaster();
    god_type god = (priest || !(priest || wizard)) ? monster->god : GOD_NO_GOD;

    // Used for summon X elemental and nothing else. {bookofjude}
    monster_type summon_type = MONS_NO_MONSTER;

    switch (spell_cast)
    {
    default:
        break;

    case SPELL_MAJOR_HEALING:
        if (monster->heal(50 + random2avg(monster->hit_dice * 10, 2)))
            simple_monster_message(monster, " is healed.");
        return;

    case SPELL_BERSERKER_RAGE:
        monster->go_berserk(true);
        return;

    case SPELL_SWIFTNESS:
        monster->add_ench(ENCH_SWIFT);
        if (monster->type == MONS_ALLIGATOR)
        {
            monster->number = you.num_turns;
            simple_monster_message(monster, " puts on a burst of speed!");
        }
        else
            simple_monster_message(monster, " seems to move somewhat quicker.");
        return;

    case SPELL_SILENCE:
        monster->add_ench(ENCH_SILENCE);
        invalidate_agrid();
        mpr("Everything around you gets eerily quiet.");
        return;

    case SPELL_CALL_TIDE:
        if (player_in_branch(BRANCH_SHOALS))
        {
            const int tide_duration = random_range(80, 200, 2);
            monster->add_ench(mon_enchant(ENCH_TIDE, 0, KC_OTHER,
                                          tide_duration * 10));
            monster->props[TIDE_CALL_TURN] = you.num_turns;
            if (simple_monster_message(
                    monster,
                    " sings a water chant to call the tide!"))
            {
                flash_view_delay(ETC_WATER, 300);
            }
        }
        return;

    case SPELL_INK_CLOUD:
        if (!feat_is_watery(grd(monster->pos())))
            return;

        big_cloud(CLOUD_INK, monster->kill_alignment(), monster->pos(), 30, 30);

        simple_monster_message(
            monster,
            " squirts a massive cloud of ink into the water!");
        return;

    case SPELL_SUMMON_SMALL_MAMMALS:
    case SPELL_VAMPIRE_SUMMON:
        if (spell_cast == SPELL_SUMMON_SMALL_MAMMALS)
            sumcount2 = 1 + random2(4);
        else
            sumcount2 = 3 + random2(3) + monster->hit_dice / 5;

        for (sumcount = 0; sumcount < sumcount2; ++sumcount)
        {
            monster_type rats[] = { MONS_ORANGE_RAT, MONS_GREEN_RAT,
                                    MONS_GREY_RAT,   MONS_RAT };

            if (spell_cast == SPELL_SUMMON_SMALL_MAMMALS)
                rats[0] = MONS_QUOKKA;

            const monster_type mon = (one_chance_in(3) ? MONS_GIANT_BAT
                                                       : RANDOM_ELEMENT(rats));
            create_monster(
                mgen_data(mon, SAME_ATTITUDE(monster), monster,
                          5, spell_cast, monster->pos(), monster->foe, 0, god));
        }
        return;

    case SPELL_SHADOW_CREATURES:       // summon anything appropriate for level
        if (_mons_abjured(monster, monsterNearby))
            return;

        sumcount2 = 1 + random2(4) + random2(monster->hit_dice / 7 + 1);

        for (sumcount = 0; sumcount < sumcount2; ++sumcount)
        {
            create_monster(
                mgen_data(RANDOM_MONSTER, SAME_ATTITUDE(monster), monster,
                          5, spell_cast, monster->pos(), monster->foe, 0, god));
        }
        return;

    case SPELL_WATER_ELEMENTALS:
        if (summon_type == MONS_NO_MONSTER)
            summon_type = MONS_WATER_ELEMENTAL;
        // Deliberate fall through
    case SPELL_EARTH_ELEMENTALS:
        if (summon_type == MONS_NO_MONSTER)
            summon_type = MONS_EARTH_ELEMENTAL;
        // Deliberate fall through
    case SPELL_AIR_ELEMENTALS:
        if (summon_type == MONS_NO_MONSTER)
            summon_type = MONS_AIR_ELEMENTAL;
        // Deliberate fall through
    case SPELL_FIRE_ELEMENTALS:
        if (summon_type == MONS_NO_MONSTER)
            summon_type = MONS_FIRE_ELEMENTAL;

        if (_mons_abjured(monster, monsterNearby))
            return;

        sumcount2 = 1 + random2(4) + random2(monster->hit_dice / 7 + 1);

        for (sumcount = 0; sumcount < sumcount2; sumcount++)
        {
            create_monster(
                mgen_data(summon_type, SAME_ATTITUDE(monster), monster,
                          3, spell_cast, monster->pos(), monster->foe, 0, god));
        }
        return;

    case SPELL_SUMMON_RAKSHASA:
        sumcount2 = 1 + random2(4) + random2(monster->hit_dice / 7 + 1);

        for (sumcount = 0; sumcount < sumcount2; sumcount++)
        {
            create_monster(
                mgen_data(MONS_RAKSHASA, SAME_ATTITUDE(monster), monster,
                          3, spell_cast, monster->pos(), monster->foe, 0, god));
        }
        return;

    case SPELL_SUMMON_ILLUSION:
        _mons_cast_summon_illusion(monster, spell_cast);
        return;

    case SPELL_KRAKEN_TENTACLES:
    {
        int kraken_index = monster->mindex();
        if (invalid_monster_index(duration))
        {
            mpr("Error! Kraken is not a part of the current environment!",
                MSGCH_ERROR);
            return;
        }
        sumcount2 = std::max(random2(9), random2(9)); // up to eight tentacles
        if (sumcount2 == 0)
            return;

        for (sumcount = 0; sumcount < MAX_MONSTERS; ++sumcount)
            if (menv[sumcount].type == MONS_KRAKEN_TENTACLE
                && (int)menv[sumcount].number == kraken_index)
            {
                // Reduce by tentacles already placed.
                sumcount2--;
            }

        for (sumcount = sumcount2; sumcount > 0; --sumcount)
        {
            // Tentacles aren't really summoned (controlled by spell_cast
            // being passed to summon_type), so I'm not sure what the
            // abjuration value (3) is doing there. (jpeg)
            int tentacle = create_monster(
                mgen_data(MONS_KRAKEN_TENTACLE, SAME_ATTITUDE(monster), monster,
                          3, spell_cast, monster->pos(), monster->foe, 0, god,
                          MONS_NO_MONSTER, kraken_index, monster->colour,
                          you.absdepth0, PROX_CLOSE_TO_PLAYER,
                          you.level_type));

            if (tentacle < 0)
            {
                sumcount2--;
            }
            else if (monster->holiness() == MH_UNDEAD)
            {
                menv[tentacle].flags |= MF_HONORARY_UNDEAD;
            }
        }
        if (sumcount2 == 1)
            mpr("A tentacle rises from the water!");
        else if (sumcount2 > 1)
            mpr("Tentacles burst out of the water!");
        return;
    }
    case SPELL_FAKE_MARA_SUMMON:
        // We only want there to be two fakes, which, plus Mara, means
        // a total of three Maras; if we already have two, give up, otherwise
        // we want to summon either one more or two more.
        sumcount2 = 2 - count_mara_fakes();

        for (sumcount = 0; sumcount < sumcount2; sumcount++)
        {
            mgen_data summ_mon =
                mgen_data(MONS_MARA_FAKE, SAME_ATTITUDE(monster),
                          monster, 3, spell_cast, monster->pos(),
                          monster->foe, 0, god);
            // This is somewhat hacky, to prevent "A Mara", and such,
            // as MONS_FAKE_MARA is not M_UNIQUE.
            summ_mon.mname = "Mara";
            summ_mon.extra_flags |= MF_NAME_REPLACE;

            int created = create_monster(summ_mon);
            if (created == -1)
                continue;

            // Mara's clones are special; they have the same stats as him, and
            // are exact clones, so they are created damaged if necessary, with
            // identical enchants and with the same items.
            monsters *new_fake = &menv[created];
            new_fake->hit_points = monster->hit_points;
            new_fake->max_hit_points = monster->max_hit_points;
            mon_enchant_list::iterator ei;
            for (ei = monster->enchantments.begin();
                 ei != monster->enchantments.end(); ++ei)
            {
                new_fake->enchantments.insert(*ei);
            }

            // Code basically lifted from clone_monster. In theory, it
            // only needs to copy weapon and armour slots; instead,
            // copy the whole inventory.
            for (int i = 0; i < NUM_MONSTER_SLOTS; i++)
            {
                const int old_index = monster->inv[i];

                if (old_index == NON_ITEM)
                    continue;

                const int new_index = get_item_slot(0);
                if (new_index == NON_ITEM)
                {
                    new_fake->unequip(mitm[old_index], i, 0, true);
                    new_fake->inv[i] = NON_ITEM;
                    continue;
                }

                new_fake->inv[i] = new_index;
                mitm[new_index]  = mitm[old_index];
                mitm[new_index].set_holding_monster(new_fake->mindex());

                // Mark items as summoned, so there's no way to get three nice
                // weapons or such out of him.
                mitm[new_index].flags |= ISFLAG_SUMMONED;
            }
        }
        return;

    case SPELL_FAKE_RAKSHASA_SUMMON:
        sumcount2 = (coinflip() ? 2 : 3);

        for (sumcount = 0; sumcount < sumcount2; sumcount++)
        {
            create_monster(
                mgen_data(MONS_RAKSHASA_FAKE, SAME_ATTITUDE(monster), monster,
                          3, spell_cast, monster->pos(), monster->foe, 0, god));
        }
        return;

    case SPELL_SUMMON_DEMON: // class 2-4 demons
        if (_mons_abjured(monster, monsterNearby))
            return;

        sumcount2 = 1 + random2(2) + random2(monster->hit_dice / 10 + 1);

        duration  = std::min(2 + monster->hit_dice / 10, 6);
        for (sumcount = 0; sumcount < sumcount2; sumcount++)
        {
            create_monster(
                mgen_data(summon_any_demon(DEMON_COMMON),
                          SAME_ATTITUDE(monster), monster, duration, spell_cast,
                          monster->pos(), monster->foe, 0, god));
        }
        return;

    case SPELL_SUMMON_UGLY_THING:
        if (_mons_abjured(monster, monsterNearby))
            return;

        sumcount2 = 1 + random2(2) + random2(monster->hit_dice / 10 + 1);

        duration  = std::min(2 + monster->hit_dice / 10, 6);
        for (sumcount = 0; sumcount < sumcount2; ++sumcount)
        {
            const monster_type mon = (one_chance_in(3) ? MONS_VERY_UGLY_THING
                                                       : MONS_UGLY_THING);

            create_monster(
                mgen_data(mon, SAME_ATTITUDE(monster), monster,
                          duration, spell_cast, monster->pos(), monster->foe, 0,
                          god));
        }
        return;

    case SPELL_ANIMATE_DEAD:
        // see special handling in mon-stuff::handle_spell() {dlb}
        if (monster->friendly() && !_animate_dead_okay())
            return;

        animate_dead(monster, 5 + random2(5), SAME_ATTITUDE(monster),
                     monster->foe, monster, "", god);
        return;

    case SPELL_CALL_IMP: // class 5 demons
        sumcount2 = 1 + random2(3) + random2(monster->hit_dice / 5 + 1);

        duration  = std::min(2 + monster->hit_dice / 5, 6);
        for (sumcount = 0; sumcount < sumcount2; ++sumcount)
        {
            create_monster(
                mgen_data(summon_any_demon(DEMON_LESSER),
                          SAME_ATTITUDE(monster), monster,
                          duration, spell_cast, monster->pos(), monster->foe, 0,
                          god));
        }
        return;

    case SPELL_SUMMON_SCORPIONS:
        if (_mons_abjured(monster, monsterNearby))
            return;

        sumcount2 = 1 + random2(3) + random2(monster->hit_dice / 5 + 1);

        duration  = std::min(2 + monster->hit_dice / 5, 6);
        for (sumcount = 0; sumcount < sumcount2; ++sumcount)
        {
            create_monster(
                mgen_data(MONS_SCORPION, SAME_ATTITUDE(monster), monster,
                          duration, spell_cast, monster->pos(), monster->foe, 0,
                          god));
        }
        return;

    case SPELL_SUMMON_SWARM:
        _do_high_level_summon(monster, monsterNearby, spell_cast,
                              _pick_swarmer, random_range(3, 6), god);
        return;

    case SPELL_SUMMON_UFETUBUS:
        sumcount2 = 2 + random2(2) + random2(monster->hit_dice / 5 + 1);

        duration  = std::min(2 + monster->hit_dice / 5, 6);

        for (sumcount = 0; sumcount < sumcount2; ++sumcount)
        {
            create_monster(
                mgen_data(MONS_UFETUBUS, SAME_ATTITUDE(monster), monster,
                          duration, spell_cast, monster->pos(), monster->foe, 0,
                          god));
        }
        return;

    case SPELL_SUMMON_BEAST:       // Geryon
        create_monster(
            mgen_data(MONS_BEAST, SAME_ATTITUDE(monster), monster,
                      4, spell_cast, monster->pos(), monster->foe, 0, god));
        return;

    case SPELL_SUMMON_ICE_BEAST:
        create_monster(
            mgen_data(MONS_ICE_BEAST, SAME_ATTITUDE(monster), monster,
                      5, spell_cast, monster->pos(), monster->foe, 0, god));
        return;

    case SPELL_SUMMON_MUSHROOMS:   // Summon swarms of icky crawling fungi.
        if (_mons_abjured(monster, monsterNearby))
            return;

        sumcount2 = 1 + random2(2) + random2(monster->hit_dice / 4 + 1);

        duration  = std::min(2 + monster->hit_dice / 5, 6);
        for (int i = 0; i < sumcount2; ++i)
        {
            create_monster(
                mgen_data(MONS_WANDERING_MUSHROOM, SAME_ATTITUDE(monster),
                          monster, duration, spell_cast, monster->pos(),
                          monster->foe, 0, god));
        }
        return;

    case SPELL_SUMMON_HORRIBLE_THINGS:
        _do_high_level_summon(monster, monsterNearby, spell_cast,
                              _pick_horrible_thing, random_range(3, 5), god);
        return;

    case SPELL_CONJURE_BALL_LIGHTNING:
    {
        const int n = 2 + random2(monster->hit_dice / 4);
        for (int i = 0; i < n; ++i)
        {
            create_monster(
                mgen_data(MONS_BALL_LIGHTNING, SAME_ATTITUDE(monster),
                          monster, 2, spell_cast, monster->pos(), monster->foe,
                          0, god));
        }
        return;
    }

    case SPELL_SUMMON_UNDEAD:      // Summon undead around player.
        _do_high_level_summon(monster, monsterNearby, spell_cast,
                              _pick_undead_summon,
                              2 + random2(2)
                                + random2(monster->hit_dice / 4 + 1), god);
        return;

    case SPELL_SYMBOL_OF_TORMENT:
        if (!monsterNearby || monster->friendly())
            return;

        torment(monster->mindex(), monster->pos());
        return;

    case SPELL_SUMMON_GREATER_DEMON:
        if (_mons_abjured(monster, monsterNearby))
            return;

        sumcount2 = 1 + random2(monster->hit_dice / 10 + 1);

        duration  = std::min(2 + monster->hit_dice / 10, 6);
        for (sumcount = 0; sumcount < sumcount2; ++sumcount)
        {
            create_monster(
                mgen_data(summon_any_demon(DEMON_GREATER),
                          SAME_ATTITUDE(monster), monster,
                          duration, spell_cast, monster->pos(), monster->foe,
                          0, god));
        }
        return;

    // Journey -- Added in Summon Lizards and Draconians
    case SPELL_SUMMON_DRAKES:
        if (_mons_abjured(monster, monsterNearby))
            return;

        sumcount2 = 1 + random2(3) + random2(monster->hit_dice / 5 + 1);

        duration  = std::min(2 + monster->hit_dice / 10, 6);

        {
            std::vector<monster_type> monsters;

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

            for (int i = 0, size = monsters.size(); i < size; ++i)
            {
                create_monster(
                    mgen_data(monsters[i], SAME_ATTITUDE(monster), monster,
                              duration, spell_cast,
                              monster->pos(), monster->foe, 0, god));
            }
        }
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
        if (!mons_near(monster))
            return;

        const bool friendly  = monster->friendly();
        const bool buff_only = !friendly && is_sanctuary(you.pos());
        const msg_channel_type channel = (friendly) ? MSGCH_FRIEND_ENCHANT
                                                    : MSGCH_MONSTER_ENCHANT;

        if (monster->type == MONS_GASTRONOK)
        {
            bool has_mon_foe = !invalid_monster_index(monster->foe);
            std::string slugform = "";
            if (buff_only || crawl_state.game_is_arena() && !has_mon_foe
                || friendly && !has_mon_foe || coinflip())
            {
                slugform = getSpeakString("gastronok_self_buff");
                if (!slugform.empty())
                {
                    slugform = replace_all(slugform, "@The_monster@",
                                           monster->name(DESC_CAP_THE));
                    mpr(slugform.c_str(), channel);
                }
            }
            else if (!friendly && !has_mon_foe)
            {
                mons_cast_noise(monster, pbolt, spell_cast);

                // "Enchant" the player.
                slugform = getSpeakString("gastronok_debuff");
                if (!slugform.empty()
                    && (slugform.find("legs") == std::string::npos
                        || _legs_msg_applicable()))
                {
                    mpr(slugform.c_str());
                }
            }
            else
            {
                // "Enchant" another monster.
                const monsters* foe      = monster->get_foe()->as_monster();
                slugform = getSpeakString("gastronok_other_buff");
                if (!slugform.empty())
                {
                    slugform = replace_all(slugform, "@The_monster@",
                                           foe->name(DESC_CAP_THE));
                    mpr(slugform.c_str(), MSGCH_MONSTER_ENCHANT);
                }
            }
        }
        else
        {
            // Messages about the monster influencing itself.
            const char* buff_msgs[] = { " glows brightly for a moment.",
                                        " looks stronger.",
                                        " becomes somewhat translucent.",
                                        "'s eyes start to glow." };

            // Messages about the monster influencing you.
            const char* other_msgs[] = {
                "You feel troubled.",
                "You feel a wave of unholy energy pass over you."
            };

            if (buff_only || crawl_state.game_is_arena() || x_chance_in_y(2,3))
            {
                simple_monster_message(monster, RANDOM_ELEMENT(buff_msgs),
                                       channel);
            }
            else if (friendly)
            {
                simple_monster_message(monster, " shimmers for a moment.",
                                       channel);
            }
            else // "Enchant" the player.
            {
                mons_cast_noise(monster, pbolt, spell_cast);
                mpr(RANDOM_ELEMENT(other_msgs));
            }
        }
        return;
    }
    case SPELL_BLINK_OTHER:
    {
        // Allow the caster to comment on moving the foe.
        std::string msg = getSpeakString(monster->name(DESC_PLAIN)
                                         + " blink_other");
        if (!msg.empty() && msg != "__NONE")
        {
            mons_speaks_msg(monster, msg, MSGCH_TALK,
                            silenced(you.pos()) || silenced(monster->pos()));
        }
        break;
    }
    case SPELL_BLINK_OTHER_CLOSE:
    {
        // Allow the caster to comment on moving the foe.
        std::string msg = getSpeakString(monster->name(DESC_PLAIN)
                                         + " blink_other_close");
        if (!msg.empty() && msg != "__NONE")
        {
            mons_speaks_msg(monster, msg, MSGCH_TALK,
                            silenced(you.pos()) || silenced(monster->pos()));
        }
        break;
    }
    case SPELL_TOMB_OF_DOROKLOHE:
    {
        sumcount = 0;

        const int hp_lost = monster->max_hit_points - monster->hit_points;

        if (!hp_lost)
            sumcount++;

        const dungeon_feature_type safe_tiles[] = {
            DNGN_SHALLOW_WATER, DNGN_FLOOR, DNGN_FLOOR_SPECIAL, DNGN_OPEN_DOOR
        };

        bool proceed;

        for (adjacent_iterator ai(monster->pos()); ai; ++ai)
        {
            const actor* act = actor_at(*ai);

            // We can blink away the crowd, but only our allies.
            if (act
                && (act->atype() == ACT_PLAYER
                    || (act->atype() == ACT_MONSTER
                        && act->as_monster()->attitude != monster->attitude)))
            {
                sumcount++;
            }

            // Make sure we have a legitimate tile.
            proceed = false;
            for (unsigned int i = 0; i < ARRAYSZ(safe_tiles) && !proceed; ++i)
                if (grd(*ai) == safe_tiles[i] || feat_is_trap(grd(*ai)))
                    proceed = true;

            if (!proceed && grd(*ai) > DNGN_MAX_NONREACH)
                sumcount++;
        }

        if (sumcount)
        {
            monster->blink();
            return;
        }

        sumcount = 0;
        for (adjacent_iterator ai(monster->pos()); ai; ++ai)
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
                    move_items(*ai, monster->pos());

                // All clouds are destroyed.
                if (env.cgrid(*ai) != EMPTY_CLOUD)
                    delete_cloud(env.cgrid(*ai));

                // All traps are destroyed.
                if (trap_def *ptrap = find_trap(*ai))
                    ptrap->destroy();

                // Actually place the wall.
                grd(*ai) = DNGN_ROCK_WALL;
                set_terrain_changed(*ai);
                sumcount++;
            }
        }

        if (sumcount)
        {
            mpr("Walls emerge from the floor!");

            // XXX: Assume that the entombed monster can regenerate.
            // Also, base the regeneration rate on HD to avoid
            // randomness.
            const int tomb_duration =
                hp_lost * std::max(1, monster->hit_dice / 3);
            monster->add_ench(mon_enchant(ENCH_ENTOMBED, 0, KC_OTHER,
                                          tomb_duration * 10));
        }
        return;
    }
    case SPELL_CHAIN_LIGHTNING:
        if (!monsterNearby || monster->friendly())
            return;
        cast_chain_lightning(4 * monster->hit_dice, monster);
        return;
    case SPELL_SUMMON_EYEBALLS:
        if (_mons_abjured(monster, monsterNearby))
            return;

        sumcount2 = 1 + random2(2) + random2(monster->hit_dice / 7 + 1);

        duration = std::min(2 + monster->hit_dice / 10, 6);

        for (sumcount = 0; sumcount < sumcount2; sumcount++)
        {
            const monster_type mon = static_cast<monster_type>(
                random_choose_weighted(100, MONS_GIANT_EYEBALL,
                                        80, MONS_EYE_OF_DRAINING,
                                        60, MONS_GOLDEN_EYE,
                                        40, MONS_SHINING_EYE,
                                        20, MONS_GREAT_ORB_OF_EYES,
                                        10, MONS_EYE_OF_DEVASTATION,
                                        0));

            create_monster(
                mgen_data(mon, SAME_ATTITUDE(monster), monster, duration,
                          spell_cast, monster->pos(), monster->foe, 0, god));
        }
        return;
    case SPELL_SUMMON_BUTTERFLIES:
        if (_mons_abjured(monster, monsterNearby))
            return;

        duration = std::min(2 + monster->hit_dice / 5, 6);
        for (int i = 0; i < 15; ++i)
        {
            create_monster(
                mgen_data(MONS_BUTTERFLY, SAME_ATTITUDE(monster),
                          monster, duration, spell_cast, monster->pos(),
                          monster->foe, 0, god));
        }
        return;
    case SPELL_IOOD:
        cast_iood(monster, 6 * monster->hit_dice, &pbolt);
        return;
    case SPELL_AWAKEN_FOREST:
        duration = 50 + random2(monster->hit_dice * 20);

        monster->add_ench(mon_enchant(ENCH_AWAKEN_FOREST, 0, KC_OTHER, duration));
        // Actually, it's a boolean marker... save for a sanity check.
        env.forest_awoken_until = you.elapsed_time + duration;

        // You may be unable to see the monster, but notice an affected tree.
        forest_message(monster->pos(), "The forest starts to sway and rumble!");
        return;
    }

    // If a monster just came into view and immediately cast a spell,
    // we need to refresh the screen before drawing the beam.
    viewwindow(false);
    if (spell_is_direct_explosion(spell_cast))
    {
        const actor *foe = monster->get_foe();
        const bool need_more = foe && (foe == &you || you.see_cell(foe->pos()));
        pbolt.in_explosion_phase = false;
        pbolt.explode(need_more);
    }
    else
        pbolt.fire();
}

static int _noise_level(const monsters* monster, spell_type spell,
                                  bool silent, bool innate)
{
    const unsigned int flags = get_spell_flags(spell);

    int noise;

    if (silent
        || (innate
            && !mons_class_flag(monster->type, M_NOISY_SPELLS)
            && !(flags & SPFLAG_NOISY)
            && mons_genus(monster->type) != MONS_DRAGON))
    {
        noise = 0;
    }
    else
    {
        if (mons_genus(monster->type) == MONS_DRAGON)
            noise = get_shout_noise_level(S_ROAR);
        else
        {
            // Noise for targeted spells happens at where the spell hits,
            // rather than where the spell is cast. zappy() sets up the
            // noise for beams.
            noise = (flags & SPFLAG_TARGETING_MASK)
                ? 1 : spell_noise(spell);
        }
    }
    return noise;
}

static unsigned int _noise_keys(std::vector<std::string>& key_list,
                                const monsters* monster, const bolt& pbolt,
                                spell_type spell, bool priest, bool wizard,
                                bool innate, bool targeted)
{
    const std::string cast_str = " cast";

    const mon_body_shape shape      = get_mon_shape(monster);
    const std::string    spell_name = spell_title(spell);
    const bool           real_spell = !innate && (priest || wizard);

    // First try the spells name.
    if (shape <= MON_SHAPE_NAGA)
    {
        if (real_spell)
            key_list.push_back(spell_name + cast_str + " real");
        if (mons_intel(monster) >= I_NORMAL)
            key_list.push_back(spell_name + cast_str + " gestures");
    }
    else if (real_spell)
    {
        // A real spell being cast by something with no hands?  Maybe
        // it's a polymorphed spellcaster which kept its original spells.
        // If so, the cast message for it's new type/species/genus probably
        // won't look right.
        if (!mons_class_flag(monster->type, M_ACTUAL_SPELLS | M_PRIEST))
        {
            // XXX: We should probably include the monster's shape,
            // to get a variety of messages.
            if (wizard)
            {
                std::string key = "polymorphed wizard" + cast_str;
                if (targeted)
                    key_list.push_back(key + " targeted");
                key_list.push_back(key);
            }
            else if (priest)
            {
                std::string key = "polymorphed priest" + cast_str;
                if (targeted)
                    key_list.push_back(key + " targeted");
                key_list.push_back(key);
            }
        }
    }

    key_list.push_back(spell_name + cast_str);

    const unsigned int num_spell_keys = key_list.size();

    // Next the monster type name, then species name, then genus name.
    key_list.push_back(mons_type_name(monster->type, DESC_PLAIN) + cast_str);
    key_list.push_back(mons_type_name(mons_species(monster->type), DESC_PLAIN)
                       + cast_str);
    key_list.push_back(mons_type_name(mons_genus(monster->type), DESC_PLAIN)
                       + cast_str);

    // Last, generic wizard, priest or demon.
    if (wizard)
        key_list.push_back((std::string)((shape <= MON_SHAPE_NAGA) ? "" : "non-humanoid ")
                           + "wizard" + cast_str);
    else if (priest)
        key_list.push_back("priest" + cast_str);
    else if (mons_is_demon(monster->type))
        key_list.push_back("demon" + cast_str);

    if (targeted)
    {
        // For targeted spells, try with the targeted suffix first.
        for (unsigned int i = key_list.size() - 1; i >= num_spell_keys; i--)
        {
            std::string str = key_list[i] + " targeted";
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

std::string _noise_message(const std::vector<std::string>& key_list,
                           unsigned int num_spell_keys,
                           bool silent, bool unseen)
{
    std::string prefix;
    if (silent)
        prefix = "silent ";
    else if (unseen)
        prefix = "unseen ";

    std::string msg;
    for (unsigned int i = 0; i < key_list.size(); i++)
    {
        const std::string key = key_list[i];

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

    return (msg);
}

void _noise_fill_target(std::string& targ_prep, std::string& target,
                        const monsters* monster, const bolt& pbolt,
                        bool gestured)
{
    targ_prep = "at";
    target    = "nothing";

    bolt tracer = pbolt;
    // For a targeted but rangeless spell make the range positive so that
    // fire_tracer() will fill out path_taken.
    if (pbolt.range == 0 && pbolt.target != monster->pos())
        tracer.range = ENV_SHOW_DIAMETER;
    fire_tracer(monster, tracer);

    if (pbolt.target == you.pos())
        target = "you";
    else if (pbolt.target == monster->pos())
        target = monster->pronoun(PRONOUN_REFLEXIVE);
    // Monsters should only use targeted spells while foe == MHITNOT
    // if they're targeting themselves.
    else if (monster->foe == MHITNOT && !mons_is_confused(monster, true))
        target = "NONEXISTENT FOE";
    else if (!invalid_monster_index(monster->foe)
             && menv[monster->foe].type == MONS_NO_MONSTER)
    {
        target = "DEAD FOE";
    }
    else if (in_bounds(pbolt.target) && you.see_cell(pbolt.target))
    {
        if (const monsters* mtarg = monster_at(pbolt.target))
        {
            if (you.can_see(mtarg))
                target = mtarg->name(DESC_NOCAP_THE);
        }
    }

    const bool visible_path      = pbolt.visible() || gestured;

    // Monster might be aiming past the real target, or maybe some fuzz has
    // been applied because the target is invisible.
    if (target == "nothing")
    {
        if (pbolt.aimed_at_spot)
        {
            int count = 0;
            for (adjacent_iterator ai(pbolt.target); ai; ++ai)
            {
                const actor* act = actor_at(*ai);
                if (act && act != monster && you.can_see(act))
                {
                    targ_prep = "next to";

                    if (act->atype() == ACT_PLAYER || one_chance_in(++count))
                        target = act->name(DESC_NOCAP_THE);

                    if (act->atype() == ACT_PLAYER)
                        break;
                }
            }
        }

        bool mons_targ_aligned = false;

        const std::vector<coord_def> &path = tracer.path_taken;
        for (unsigned int i = 0; i < path.size(); i++)
        {
            const coord_def pos = path[i];

            if (pos == monster->pos())
                continue;

            const monsters *m = monster_at(pos);
            if (pos == you.pos())
            {
                // Be egotistical and assume that the monster is aiming at
                // the player, rather than the player being in the path of
                // a beam aimed at an ally.
                if (!monster->wont_attack())
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
                bool is_aligned  = mons_aligned(m, monster);
                std::string name = m->name(DESC_NOCAP_THE);

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
                    if (act && act != monster && you.can_see(act))
                    {
                        targ_prep = "past";
                        if (act->atype() == ACT_PLAYER
                            || one_chance_in(++count))
                        {
                            target = act->name(DESC_NOCAP_THE);
                        }

                        if (act->atype() == ACT_PLAYER)
                            break;
                    }
                }
            }
        } // for (unsigned int i = 0; i < path.size(); i++)
    } // if (target == "nothing" && targeted)

    const actor* foe = monster->get_foe();

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
        && !monster->confused()
        && visible_path)
    {
        target = foe->name(DESC_NOCAP_THE);
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

void mons_cast_noise(monsters *monster, const bolt &pbolt,
                     spell_type spell_cast, bool special_ability)
{
    bool force_silent = false;

    spell_type actual_spell = spell_cast;

    if (spell_cast == SPELL_DRACONIAN_BREATH)
    {
        int type = monster->type;
        if (mons_genus(type) == MONS_DRACONIAN)
            type = draco_subspecies(monster);

        switch (type)
        {
        case MONS_MOTTLED_DRACONIAN:
            actual_spell = SPELL_STICKY_FLAME_SPLASH;
            break;

        case MONS_YELLOW_DRACONIAN:
            actual_spell = SPELL_ACID_SPLASH;
            break;

        case MONS_PLAYER_GHOST:
            // Draining breath is silent.
            force_silent = true;
            break;

        default:
            break;
        }
    }
    else if (monster->type == MONS_SHADOW_DRAGON)
        // Draining breath is silent.
        force_silent = true;

    const bool unseen    = !you.can_see(monster);
    const bool silent    = silenced(monster->pos()) || force_silent;
    const bool no_silent = mons_class_flag(monster->type, M_SPELL_NO_SILENT);

    if (unseen && silent)
        return;

    const unsigned int flags = get_spell_flags(actual_spell);

    const bool priest = monster->is_priest();
    const bool wizard = monster->is_actual_spellcaster();
    const bool innate = !(priest || wizard || no_silent)
                        || (flags & SPFLAG_INNATE) || special_ability;

    int noise = _noise_level(monster, actual_spell, silent, innate);

    const bool targeted = (flags & SPFLAG_TARGETING_MASK)
                           && (pbolt.target != monster->pos()
                               || pbolt.visible());

    std::vector<std::string> key_list;
    unsigned int num_spell_keys =
        _noise_keys(key_list, monster, pbolt, actual_spell,
                    priest, wizard, innate, targeted);

    std::string msg = _noise_message(key_list, num_spell_keys,
                                     silent, unseen);

    if (msg.empty())
    {
        if (silent)
            return;

        noisy(noise, monster->pos(), monster->mindex());
        return;
    }

    // FIXME: we should not need to look at the message text.
    const bool gestured = msg.find("Gesture") != std::string::npos
                          || msg.find(" gesture") != std::string::npos
                          || msg.find("Point") != std::string::npos
                          || msg.find(" point") != std::string::npos;

    std::string targ_prep = "at";
    std::string target    = "NO_TARGET";

    if (targeted)
        _noise_fill_target(targ_prep, target, monster, pbolt, gestured);

    msg = replace_all(msg, "@at@",     targ_prep);
    msg = replace_all(msg, "@target@", target);

    std::string beam_name;
    if (!targeted)
        beam_name = "NON TARGETED BEAM";
    else if (pbolt.name.empty())
        beam_name = "INVALID BEAM";
    else
        beam_name = pbolt.get_short_name();

    msg = replace_all(msg, "@beam@", beam_name);

    const msg_channel_type chan =
        (unseen              ? MSGCH_SOUND :
         monster->friendly() ? MSGCH_FRIEND_SPELL
                             : MSGCH_MONSTER_SPELL);

    if (silent)
        mons_speaks_msg(monster, msg, chan, true);
    else if (noisy(noise, monster->pos(), monster->mindex()) || !unseen)
    {
        // noisy() returns true if the player heard the noise.
        mons_speaks_msg(monster, msg, chan);
    }
}
