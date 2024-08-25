/**
 * @file
 * @brief Map for monster spells to replaceable player spells. See spelldata
 *  array in spl-data.h for the full list of all spells/effects.
 *
 * Spells with the flag spflag::monster are what should be mapped.
 *
 * TODO: Look at book-type.h/book-data.h later and consider mapping monster
 * types or spellbooks to player spellbooks
**/

#pragma once

#include "spell-type.h"
#include "spl-util.h"
#include "spl-book.h"

static spell_type mon_spell_to_player_equivalent(spell_type spell, int mon_hd)
{
    if (!spell_removed(spell) && is_player_book_spell(spell))
        return spell;

    if(!is_valid_spell(spell))
        return SPELL_NO_SPELL;

    switch (spell)
    {
    // Conjurations
    case SPELL_SLUG_DART:
    case SPELL_FORCE_LANCE:
        return SPELL_MAGIC_DART;
    case SPELL_CALL_DOWN_DAMNATION:
        return SPELL_IOOD;
    case SPELL_STEAM_BALL:
    case SPELL_BOLT_OF_DEVASTATION:
        return SPELL_BATTLESPHERE;

    // Hexes
    case SPELL_HASTE:  // Swiftness from Air?
    case SPELL_CORONA:
    case SPELL_HASTE_OTHER:
    case SPELL_MIGHT:
    case SPELL_MIGHT_OTHER:
        return SPELL_SLOW;
    case SPELL_SLEEP:
        return SPELL_HIBERNATION;
    case SPELL_CONFUSE:
    case SPELL_MASS_CONFUSION:
    case SPELL_INVISIBILITY:
        if (mon_hd < 7)
            return SPELL_CONFUSING_TOUCH;
        else if (mon_hd < 18)
            return SPELL_DAZZLING_FLASH;
        else
            return SPELL_DISCORD;

    // Summonings
    case SPELL_SHADOW_CREATURES:
    case SPELL_SUMMON_DEMON:
    case SPELL_SUMMON_GREATER_DEMON:
    case SPELL_SUMMON_UFETUBUS:
    case SPELL_SUMMON_ILLUSION:
    case SPELL_PHANTOM_MIRROR:
        if (mon_hd < 4)
            return SPELL_CALL_IMP;
        else if (mon_hd < 10)
            return SPELL_CALL_CANINE_FAMILIAR;
        else if (mon_hd < 14)
            return SPELL_SUMMON_HYDRA;
        else
            return SPELL_MONSTROUS_MENAGERIE;
    case SPELL_WOODWEAL:
        return SPELL_SUMMON_FOREST;
    case SPELL_SUMMON_MUSHROOMS:
        return SPELL_SUMMON_CACTUS;
    case SPELL_SUMMON_DRAKES:
    case SPELL_SUMMON_DRAGON:
        if (mon_hd < 14)
            return SPELL_SUMMON_HYDRA;
        else
            return SPELL_DRAGON_CALL;

    // Necromancy
    case SPELL_PAIN:
        return SPELL_SOUL_SPLINTER;
    case SPELL_BOLT_OF_DRAINING:
    case SPELL_BRAIN_BITE:
    case SPELL_SMITING:
        return SPELL_VAMPIRIC_DRAINING;
    case SPELL_MINOR_HEALING:
    case SPELL_MAJOR_HEALING:
    case SPELL_HEAL_OTHER:
    case SPELL_MIASMA_BREATH:
        return SPELL_PUTREFACTION;
    case SPELL_ANIMATE_SKELETON:
        return SPELL_ANIMATE_DEAD;
    case SPELL_AGONIZING_TOUCH:
        return SPELL_CURSE_OF_AGONY;
    case SPELL_SYMBOL_OF_TORMENT:
        return SPELL_ANGUISH;
    case SPELL_SUMMON_UNDEAD:
        return SPELL_HAUNT;

    // Translocations
    case SPELL_BLINK_RANGE:
    case SPELL_BLINK_AWAY:
    case SPELL_BLINK_CLOSE:
    case SPELL_BLINK_OTHER:
    case SPELL_BLINK_OTHER_CLOSE:
    case SPELL_CONTROLLED_BLINK:
        return SPELL_BLINK;
    case SPELL_PORTAL_PROJECTILE:
    case SPELL_BLINK_ALLIES_ENCIRCLE:
        return SPELL_BECKONING;
    case SPELL_BLINKBOLT:
    case SPELL_BANISHMENT:
    case SPELL_BLINK_ALLIES_AWAY:
        return SPELL_TELEPORT_OTHER;

    // Alchemy
    case SPELL_VENOM_BOLT:
    case SPELL_SPIT_POISON:
    case SPELL_SPIT_ACID:
    case SPELL_CAUSTIC_BREATH:
        return SPELL_STING;
    case SPELL_POISON_CLOUD:
    case SPELL_NOXIOUS_CLOUD:
    case SPELL_POISON_ARROW:
        if (mon_hd < 5)
            return SPELL_MERCURY_VAPOURS;
        else if (mon_hd < 10)
            return SPELL_OLGREBS_TOXIC_RADIANCE;
        else
            return SPELL_NOXIOUS_BOG;

    // Fire Magic
    case SPELL_FIRE_BREATH:
        return SPELL_SCORCH;
    case SPELL_PYRE_ARROW:
        return SPELL_STICKY_FLAME;
    case SPELL_BOLT_OF_FIRE:
    case SPELL_BOLT_OF_MAGMA:
    case SPELL_SEARING_BREATH:
        if (mon_hd < 4)
            return SPELL_FOXFIRE;
        else if (mon_hd < 8)
            return SPELL_SCORCH;
        else if (mon_hd < 12)
            return SPELL_FLAME_WAVE;
        else if (mon_hd < 18)
            return SPELL_FIREBALL;
        else
            return SPELL_IGNITION;

    // Ice Magic
    case SPELL_BOLT_OF_COLD:
    case SPELL_THROW_ICICLE:
    case SPELL_COLD_BREATH:
    case SPELL_GLACIAL_BREATH:
        if (mon_hd < 4)
            return SPELL_FREEZE;
        else if (mon_hd < 10)
            return SPELL_FROZEN_RAMPARTS;
        else if (mon_hd < 18)
            return SPELL_OZOCUBUS_REFRIGERATION;
        else
            return SPELL_POLAR_VORTEX;
    case SPELL_WATER_ELEMENTALS:
        return SPELL_SUMMON_ICE_BEAST;

    // Air Magic
    case SPELL_LIGHTNING_BOLT:
    case SPELL_CALL_DOWN_LIGHTNING:
    case SPELL_REPEL_MISSILES:
        if (mon_hd < 5)
            return SPELL_SHOCK;
        else if (mon_hd < 9)
            return SPELL_DISCHARGE;
        else if (mon_hd < 15)
            return SPELL_AIRSTRIKE;
        else if (mon_hd < 22)
            return SPELL_CONJURE_BALL_LIGHTNING;
        else
            return SPELL_CHAIN_LIGHTNING;
    case SPELL_ELECTROLUNGE:
        return SPELL_ELECTRIC_CHARGE;
    case SPELL_AIR_ELEMENTALS:
        return SPELL_SUMMON_LIGHTNING_SPIRE;

    // Earth Magic
    case SPELL_BERSERKER_RAGE:
        return SPELL_SWIFTNESS;
    case SPELL_METAL_SPLINTERS:
    case SPELL_SPLINTERSPRAY:
    case SPELL_IRON_SHOT:
        if (mon_hd < 4)
            return SPELL_SANDBLAST;
        else if (mon_hd < 11)
            return SPELL_STONE_ARROW;
        else if (mon_hd < 20)
            return SPELL_BOMBARD;
        else
            return SPELL_LEHUDIBS_CRYSTAL_SPEAR;
    case SPELL_ROLL:
        return SPELL_BOULDER;
    case SPELL_PETRIFYING_CLOUD:
        return SPELL_PETRIFY;

    default:
        break;
    }
    return SPELL_NO_SPELL;
}

/**
 *
case SPELL_CREATE_TENTACLES:
spschool::none,

case SPELL_SUMMON_EYEBALLS:
spschool::summoning,

case SPELL_EARTH_ELEMENTALS:
spschool::summoning,

case SPELL_FIRE_ELEMENTALS:
spschool::summoning,

case SPELL_FAKE_MARA_SUMMON:
spschool::summoning,

case SPELL_SUMMON_ILLUSION:
spschool::summoning,

case SPELL_PRIMAL_WAVE:
spschool::conjuration | spschool::ice,

case SPELL_CALL_TIDE:
spschool::translocation,

case SPELL_INK_CLOUD:
spschool::conjuration | spschool::ice, // it's a water spell

case SPELL_AWAKEN_FOREST:
spschool::hexes | spschool::summoning,

case SPELL_DRUIDS_CALL:
spschool::summoning,

case SPELL_VANQUISHED_VANGUARD:
spschool::necromancy | spschool::summoning,

case SPELL_SUMMON_HOLIES:
spschool::summoning,

case SPELL_HOLY_FLAMES:
spschool::none,

case SPELL_HOLY_BREATH:
spschool::conjuration,

case SPELL_INJURY_MIRROR:
spschool::none,

case SPELL_DRAIN_LIFE:
spschool::necromancy,

case SPELL_LEDAS_LIQUEFACTION:
spschool::earth | spschool::alchemy,

case SPELL_MESMERISE:
spschool::hexes,

case SPELL_FIRE_SUMMON:
spschool::summoning | spschool::fire,

case SPELL_ENSNARE:
spschool::conjuration | spschool::hexes,

case SPELL_GREATER_ENSNARE:
spschool::conjuration | spschool::hexes,

case SPELL_THUNDERBOLT:
spschool::conjuration | spschool::air,

case SPELL_STICKS_TO_SNAKES:
spschool::summoning,

case SPELL_MALMUTATE:
spschool::alchemy | spschool::hexes,

case SPELL_DAZZLING_FLASH:
spschool::hexes | spschool::fire,

case SPELL_SENTINEL_MARK:
spschool::hexes,

// Ironbrand Convoker version (delayed activation, recalls only humanoids)
case SPELL_WORD_OF_RECALL:
spschool::summoning | spschool::translocation,

case SPELL_INJURY_BOND:
spschool::hexes,

case SPELL_SPECTRAL_CLOUD:
spschool::conjuration | spschool::necromancy,

case SPELL_GHOSTLY_FIREBALL:
spschool::conjuration | spschool::necromancy,

case SPELL_CALL_LOST_SOULS:
spschool::summoning | spschool::necromancy,

case SPELL_DIMENSION_ANCHOR:
spschool::translocation | spschool::hexes,

case SPELL_BLINK_ALLIES_ENCIRCLE:
spschool::translocation,

case SPELL_AWAKEN_VINES:
spschool::hexes | spschool::summoning,

case SPELL_THORN_VOLLEY:
spschool::conjuration | spschool::earth,

case SPELL_WALL_OF_BRAMBLES:
spschool::conjuration | spschool::earth,

case SPELL_WATERSTRIKE:
spschool::ice,

case SPELL_WIND_BLAST:
spschool::air,

case SPELL_STRIP_WILLPOWER:
spschool::hexes,

case SPELL_FUGUE_OF_THE_FALLEN:
spschool::necromancy,

case SPELL_SUMMON_VERMIN:
spschool::summoning,

case SPELL_MALIGN_OFFERING:
spschool::necromancy,

case SPELL_SEARING_RAY:
spschool::conjuration,

case SPELL_INVISIBILITY_OTHER:
spschool::hexes,

case SPELL_VIRULENCE:
spschool::alchemy | spschool::hexes,

case SPELL_ORB_OF_ELECTRICITY:
spschool::conjuration | spschool::air,

case SPELL_FLASH_FREEZE:
spschool::conjuration | spschool::ice,

case SPELL_CREEPING_FROST:
spschool::conjuration | spschool::ice,

case SPELL_LEGENDARY_DESTRUCTION:
spschool::conjuration,

case SPELL_FORCEFUL_INVITATION:
spschool::summoning,

case SPELL_PLANEREND:
spschool::summoning,

case SPELL_CHAIN_OF_CHAOS:
spschool::conjuration,

case SPELL_CALL_OF_CHAOS:
spschool::hexes,

case SPELL_SIGN_OF_RUIN:
spschool::necromancy,

case SPELL_SAP_MAGIC:
spschool::hexes,

case SPELL_MAJOR_DESTRUCTION:
spschool::conjuration,

case SPELL_BLINK_ALLIES_AWAY:
spschool::translocation,

case SPELL_REBOUNDING_BLAZE:
spschool::conjuration | spschool::fire,

case SPELL_REBOUNDING_CHILL:
spschool::conjuration | spschool::ice,

case SPELL_GLACIATE:
spschool::conjuration | spschool::ice,

case SPELL_DRAIN_MAGIC:
spschool::hexes,

case SPELL_CORROSIVE_BOLT:
spschool::conjuration | spschool::alchemy,

case SPELL_BOLT_OF_LIGHT:
spschool::conjuration | spschool::fire | spschool::air,

case SPELL_SERPENT_OF_HELL_GEH_BREATH:
spschool::conjuration,

case SPELL_SERPENT_OF_HELL_COC_BREATH:
spschool::conjuration,

case SPELL_SERPENT_OF_HELL_DIS_BREATH:
spschool::conjuration,

case SPELL_SERPENT_OF_HELL_TAR_BREATH:
spschool::conjuration,

case SPELL_SUMMON_EMPEROR_SCORPIONS:
spschool::summoning | spschool::alchemy,

case SPELL_IRRADIATE:
spschool::conjuration | spschool::alchemy,

case SPELL_SPIT_LAVA:
spschool::conjuration | spschool::fire | spschool::earth,

case SPELL_ELECTRICAL_BOLT:
spschool::conjuration | spschool::air,

case SPELL_FLAMING_CLOUD:
spschool::conjuration | spschool::fire,

case SPELL_THROW_BARBS:
spschool::conjuration,

case SPELL_BATTLECRY:
spschool::hexes,

case SPELL_WARNING_CRY:
spschool::hexes,

case SPELL_HUNTING_CALL:
spschool::hexes,

case SPELL_FUNERAL_DIRGE:
spschool::necromancy,

case SPELL_SEAL_DOORS:
spschool::hexes,

case SPELL_FLAY:
spschool::necromancy,

case SPELL_BERSERK_OTHER:
spschool::hexes,

case SPELL_CORRUPTING_PULSE:
spschool::hexes | spschool::alchemy,

case SPELL_SIREN_SONG:
spschool::hexes,

case SPELL_AVATAR_SONG:
spschool::hexes,

case SPELL_PARALYSIS_GAZE:
spschool::hexes,

case SPELL_CONFUSION_GAZE:
spschool::hexes,

case SPELL_DRAINING_GAZE:
spschool::hexes,

case SPELL_WEAKENING_GAZE:
spschool::hexes,

case SPELL_MOURNING_WAIL:
spschool::necromancy,

case SPELL_DEATH_RATTLE:
spschool::conjuration | spschool::necromancy | spschool::air,

case SPELL_MARCH_OF_SORROWS:
spschool::conjuration | spschool::necromancy | spschool::air,

case SPELL_SUMMON_SCARABS:
spschool::summoning | spschool::necromancy,

case SPELL_THROW_ALLY:
spschool::translocation,

case SPELL_CLEANSING_FLAME:
spschool::none,

// Evoker-only now
case SPELL_GRAVITAS:
spschool::translocation,

case SPELL_VIOLENT_UNRAVELLING:
spschool::hexes | spschool::alchemy,

case SPELL_ENTROPIC_WEAVE:
spschool::hexes,

case SPELL_SUMMON_EXECUTIONERS:
spschool::summoning,

case SPELL_DOOM_HOWL:
spschool::translocation | spschool::hexes,

case SPELL_PRAYER_OF_BRILLIANCE:
spschool::conjuration,

case SPELL_ICEBLAST:
spschool::conjuration | spschool::ice,

case SPELL_SPRINT:
spschool::hexes,

case SPELL_GREATER_SERVANT_MAKHLEB:
spschool::summoning,

case SPELL_BIND_SOULS:
spschool::necromancy | spschool::ice,

case SPELL_INFESTATION:
spschool::necromancy,

case SPELL_STILL_WINDS:
spschool::hexes | spschool::air,

case SPELL_RESONANCE_STRIKE:
spschool::earth,

case SPELL_GHOSTLY_SACRIFICE:
spschool::necromancy,

case SPELL_DREAM_DUST:
spschool::hexes,

case SPELL_BECKONING:
spschool::translocation,

// Monster-only, players can use Qazlal's ability
case SPELL_UPHEAVAL:
spschool::conjuration,

case SPELL_MERCURY_VAPOURS:
spschool::alchemy | spschool::air,

case SPELL_IGNITION:
spschool::fire,

case SPELL_BORGNJORS_VILE_CLUTCH:
spschool::necromancy | spschool::earth,

case SPELL_FASTROOT:
spschool::hexes | spschool::earth,

case SPELL_WARP_SPACE:
spschool::translocation,

case SPELL_HARPOON_SHOT:
spschool::conjuration | spschool::earth,

case SPELL_GRASPING_ROOTS:
spschool::earth,

case SPELL_THROW_PIE:
spschool::conjuration | spschool::hexes,

case SPELL_SPORULATE:
spschool::conjuration | spschool::earth,

case SPELL_STARBURST:
spschool::conjuration | spschool::fire,

case SPELL_FOXFIRE:
spschool::conjuration | spschool::fire,

case SPELL_MARSHLIGHT:
spschool::conjuration | spschool::fire,

case SPELL_HAILSTORM:
spschool::conjuration | spschool::ice,

case SPELL_NOXIOUS_BOG:
spschool::alchemy,

case SPELL_AGONY:
spschool::necromancy,

case SPELL_DISPEL_UNDEAD_RANGE:
spschool::necromancy,

case SPELL_FROZEN_RAMPARTS:
spschool::ice,

case SPELL_MAXWELLS_COUPLING:
spschool::air,

// This "spell" is implemented in a way that ignores all this information,
// and it is never triggered the way spells usually are, but it still has
// a spell-type enum entry. So, use fake data in order to have a valid
// entry here. If it ever were to be castable, this would need some updates.
case SPELL_SONIC_WAVE:
spschool::none,

case SPELL_HURL_SLUDGE:
spschool::alchemy | spschool::conjuration,

case SPELL_SUMMON_TZITZIMITL:
spschool::summoning | spschool::necromancy,

case SPELL_SUMMON_HELL_SENTINEL:
spschool::summoning,

case SPELL_ANIMATE_ARMOUR:
spschool::summoning | spschool::earth,

case SPELL_MANIFOLD_ASSAULT:
spschool::translocation,

case SPELL_CONCENTRATE_VENOM:
spschool::alchemy,

case SPELL_ERUPTION:
spschool::conjuration | spschool::fire | spschool::earth,

case SPELL_PYROCLASTIC_SURGE:
spschool::conjuration | spschool::fire | spschool::earth,

case SPELL_STUNNING_BURST:
spschool::conjuration | spschool::air,

case SPELL_CORRUPT_LOCALE:
spschool::translocation,

case SPELL_CONJURE_LIVING_SPELLS:
spschool::conjuration,

case SPELL_SUMMON_CACTUS:
spschool::summoning,

case SPELL_STOKE_FLAMES:
spschool::fire | spschool::conjuration,

case SPELL_SERACFALL:
spschool::conjuration | spschool::ice,

case SPELL_SCORCH:
spschool::fire,

case SPELL_FLAME_WAVE:
spschool::conjuration | spschool::fire,

case SPELL_ENFEEBLE:
spschool::hexes,

case SPELL_SUMMON_SPIDERS:
spschool::summoning | spschool::alchemy,

case SPELL_ANGUISH:
spschool::hexes | spschool::necromancy,

case SPELL_SUMMON_SCORPIONS:
spschool::summoning | spschool::alchemy,

case SPELL_SHEZAS_DANCE:
spschool::summoning | spschool::earth,

case SPELL_DIVINE_ARMAMENT:
spschool::summoning,

case SPELL_KISS_OF_DEATH:
spschool::conjuration | spschool::necromancy,

case SPELL_JINXBITE:
spschool::hexes,

case SPELL_SIGIL_OF_BINDING:
spschool::hexes,

case SPELL_DIMENSIONAL_BULLSEYE:
spschool::translocation | spschool::hexes,

case SPELL_BOULDER:
spschool::earth | spschool::conjuration,

case SPELL_VITRIFY:
spschool::hexes,

case SPELL_VITRIFYING_GAZE:
spschool::hexes,

case SPELL_CRYSTALLIZING_SHOT:
spschool::conjuration | spschool::earth | spschool::hexes,

case SPELL_TREMORSTONE:
spschool::earth,

case SPELL_REGENERATE_OTHER:
spschool::necromancy,

case SPELL_MASS_REGENERATION:
spschool::necromancy,

case SPELL_NOXIOUS_BREATH:
spschool::conjuration | spschool::air | spschool:: alchemy,

// Dummy spell for the Makhleb ability.
case SPELL_UNLEASH_DESTRUCTION:
spschool::conjuration,

case SPELL_HURL_TORCHLIGHT:
spschool::conjuration | spschool::necromancy,

case SPELL_COMBUSTION_BREATH:
spschool::conjuration | spschool::fire,

case SPELL_NULLIFYING_BREATH:
spschool::conjuration,

case SPELL_STEAM_BREATH:
spschool::conjuration,

case SPELL_MUD_BREATH:
spschool::conjuration | spschool::earth,

case SPELL_GALVANIC_BREATH:
spschool::conjuration | spschool::air,

case SPELL_PILEDRIVER:
spschool::translocation,

case SPELL_GELLS_GAVOTTE:
spschool::translocation,

case SPELL_MAGNAVOLT:
spschool::air | spschool::earth,

case SPELL_FULSOME_FUSILLADE:
spschool::alchemy | spschool::conjuration,

case SPELL_RIMEBLIGHT:
spschool::necromancy | spschool::ice,

case SPELL_HOARFROST_CANNONADE:
spschool::alchemy | spschool::ice,

case SPELL_SEISMIC_SHOCKWAVE:
spschool::alchemy | spschool::earth,

case SPELL_HOARFROST_BULLET:
spschool::conjuration | spschool::ice,

case SPELL_FLASHING_BALESTRA:
spschool::conjuration,

case SPELL_BESTOW_ARMS:
spschool::hexes,

case SPELL_HELLFIRE_MORTAR:
spschool::earth | spschool::fire,

// Dithmenos shadow mimic spells
case SPELL_SHADOW_SHARD:
spschool::earth,

case SPELL_SHADOW_BEAM:
spschool::conjuration,

case SPELL_SHADOW_BALL:
spschool::fire,

case SPELL_CREEPING_SHADOW:
spschool::ice,

case SPELL_SHADOW_TEMPEST:
spschool::air,

case SPELL_SHADOW_PRISM:
spschool::alchemy,


case SPELL_SHADOW_PUPPET:
spschool::summoning,

case SPELL_SHADOW_BIND:
spschool::translocation,

case SPELL_SHADOW_TORPOR:
spschool::hexes,

case SPELL_SHADOW_DRAINING:
spschool::necromancy,

case SPELL_GRAVE_CLAW:
spschool::necromancy,

 */