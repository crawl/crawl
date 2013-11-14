#include "AppHdr.h"

#include "spl-zap.h"

#include "beam.h"
#include "stuff.h"

zap_type spell_to_zap(spell_type spell)
{
    switch (spell)
    {
    case SPELL_MAGIC_DART:
        return ZAP_MAGIC_DART;
    case SPELL_FORCE_LANCE:
        return ZAP_FORCE_LANCE;
    case SPELL_THROW_FLAME:
        return ZAP_THROW_FLAME;
    case SPELL_THROW_FROST:
        return ZAP_THROW_FROST;
    case SPELL_PAIN:
        return ZAP_PAIN;
    case SPELL_FLAME_TONGUE:
        return ZAP_FLAME_TONGUE;
    case SPELL_SHOCK:
        return ZAP_SHOCK;
    case SPELL_STING:
        return ZAP_STING;
    case SPELL_BOLT_OF_FIRE:
        return ZAP_BOLT_OF_FIRE;
    case SPELL_BOLT_OF_COLD:
        return ZAP_BOLT_OF_COLD;
    case SPELL_PRIMAL_WAVE:
        return ZAP_PRIMAL_WAVE;
    case SPELL_STONE_ARROW:
        return ZAP_STONE_ARROW;
    case SPELL_POISON_ARROW:
        return ZAP_POISON_ARROW;
    case SPELL_IRON_SHOT:
        return ZAP_IRON_SHOT;
    case SPELL_LIGHTNING_BOLT:
        return ZAP_LIGHTNING_BOLT;
    case SPELL_BOLT_OF_MAGMA:
        return ZAP_BOLT_OF_MAGMA;
    case SPELL_VENOM_BOLT:
        return ZAP_VENOM_BOLT;
    case SPELL_BOLT_OF_DRAINING:
        return ZAP_BOLT_OF_DRAINING;
    case SPELL_LEHUDIBS_CRYSTAL_SPEAR:
        return ZAP_LEHUDIBS_CRYSTAL_SPEAR;
    case SPELL_FIREBALL:
        return ZAP_FIREBALL;
    case SPELL_BOLT_OF_INACCURACY:
        return ZAP_BOLT_OF_INACCURACY;
    case SPELL_STICKY_FLAME:
        return ZAP_STICKY_FLAME;
    case SPELL_STICKY_FLAME_RANGE:
        return ZAP_STICKY_FLAME_RANGE;
    case SPELL_DISPEL_UNDEAD:
        return ZAP_DISPEL_UNDEAD;
    case SPELL_ISKENDERUNS_MYSTIC_BLAST:
        return ZAP_ISKENDERUNS_MYSTIC_BLAST;
    case SPELL_AGONY:
        return ZAP_AGONY;
    case SPELL_DISINTEGRATE:
        return ZAP_DISINTEGRATE;
    case SPELL_THROW_ICICLE:
        return ZAP_THROW_ICICLE;
    case SPELL_PORKALATOR:
        // Wizard mode only.
        return ZAP_PORKALATOR;
    case SPELL_HELLFIRE:
        // Should only be available from Staff of Dispater and Sceptre
        // of Asmodeus.
        return ZAP_HELLFIRE;
    case SPELL_ICE_STORM:
        return ZAP_ICE_STORM;
    case SPELL_CORONA:
        return ZAP_CORONA;
    case SPELL_SLOW:
        return ZAP_SLOW;
    case SPELL_CONFUSE:
        return ZAP_CONFUSE;
    case SPELL_ENSLAVEMENT:
        return ZAP_ENSLAVEMENT;
    case SPELL_HIBERNATION:
        return ZAP_HIBERNATION;
    case SPELL_SLEEP:
        return ZAP_SLEEP;
    case SPELL_PARALYSE:
        return ZAP_PARALYSE;
    case SPELL_PETRIFY:
        return ZAP_PETRIFY;
    case SPELL_POLYMORPH:
        return ZAP_POLYMORPH;
    case SPELL_TELEPORT_OTHER:
        return ZAP_TELEPORT_OTHER;
    case SPELL_INNER_FLAME:
        return ZAP_INNER_FLAME;
    case SPELL_HASTE:
        return ZAP_HASTE;
    case SPELL_INVISIBILITY:
        return ZAP_INVISIBILITY;
    case SPELL_DIG:
        return ZAP_DIG;
    case SPELL_HOLY_LIGHT:
        return ZAP_HOLY_LIGHT;
    case SPELL_DEBUGGING_RAY:
        return ZAP_DEBUGGING_RAY;
    default:
        return NUM_ZAPS;
    }
}

int spell_zap_power(spell_type spell, int pow)
{
    switch (spell)
    {
    case SPELL_CORONA:
        return pow + 10;
    case SPELL_HIBERNATION:
        return stepdown_value(pow * 9 / 10, 5, 35, 45, 50);
    default:
        return pow;
    }
}

int spell_zap_power_cap(spell_type spell)
{
    const zap_type zap = spell_to_zap(spell);

    if (zap == NUM_ZAPS)
        return 0;

    const int cap = zap_power_cap(zap);

    switch (spell)
    {
    case SPELL_CORONA:
        return max<int>(cap - 10, 0);
    case SPELL_HIBERNATION:
        return 50;
    default:
        return cap;
    }
}
