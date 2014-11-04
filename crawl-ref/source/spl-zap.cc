#include "AppHdr.h"

#include "spl-zap.h"

#include "beam.h"
#include "stepdown.h"

static int _spl_zaps[][2] =
{
    { SPELL_MAGIC_DART, ZAP_MAGIC_DART },
    { SPELL_FORCE_LANCE, ZAP_FORCE_LANCE },
    { SPELL_THROW_FLAME, ZAP_THROW_FLAME },
    { SPELL_THROW_FROST, ZAP_THROW_FROST },
    { SPELL_PAIN, ZAP_PAIN },
    { SPELL_FLAME_TONGUE, ZAP_FLAME_TONGUE },
    { SPELL_SHOCK, ZAP_SHOCK },
    { SPELL_STING, ZAP_STING },
    { SPELL_BOLT_OF_FIRE, ZAP_BOLT_OF_FIRE },
    { SPELL_BOLT_OF_COLD, ZAP_BOLT_OF_COLD },
    { SPELL_PRIMAL_WAVE, ZAP_PRIMAL_WAVE },
    { SPELL_STONE_ARROW, ZAP_STONE_ARROW },
    { SPELL_POISON_ARROW, ZAP_POISON_ARROW },
    { SPELL_IRON_SHOT, ZAP_IRON_SHOT },
    { SPELL_LIGHTNING_BOLT, ZAP_LIGHTNING_BOLT },
    { SPELL_BOLT_OF_MAGMA, ZAP_BOLT_OF_MAGMA },
    { SPELL_VENOM_BOLT, ZAP_VENOM_BOLT },
    { SPELL_BOLT_OF_DRAINING, ZAP_BOLT_OF_DRAINING },
    { SPELL_LEHUDIBS_CRYSTAL_SPEAR, ZAP_LEHUDIBS_CRYSTAL_SPEAR },
    { SPELL_FIREBALL, ZAP_FIREBALL },
    { SPELL_BOLT_OF_INACCURACY, ZAP_BOLT_OF_INACCURACY },
    { SPELL_DISPEL_UNDEAD, ZAP_DISPEL_UNDEAD },
    { SPELL_ISKENDERUNS_MYSTIC_BLAST, ZAP_ISKENDERUNS_MYSTIC_BLAST },
    { SPELL_AGONY, ZAP_AGONY },
    { SPELL_DISINTEGRATE, ZAP_DISINTEGRATE },
    { SPELL_THROW_ICICLE, ZAP_THROW_ICICLE },
    // Wizard mode only.
    { SPELL_PORKALATOR, ZAP_PORKALATOR },
    // Should only be available from Staff of Dispater and Sceptre
    // of Asmodeus.
    { SPELL_HELLFIRE, ZAP_HELLFIRE },
    { SPELL_CORONA, ZAP_CORONA },
    { SPELL_SLOW, ZAP_SLOW },
    { SPELL_CONFUSE, ZAP_CONFUSE },
    { SPELL_ENSLAVEMENT, ZAP_ENSLAVEMENT },
    { SPELL_HIBERNATION, ZAP_HIBERNATION },
    { SPELL_SLEEP, ZAP_SLEEP },
    { SPELL_PARALYSE, ZAP_PARALYSE },
    { SPELL_PETRIFY, ZAP_PETRIFY },
    { SPELL_POLYMORPH, ZAP_POLYMORPH },
    { SPELL_TELEPORT_OTHER, ZAP_TELEPORT_OTHER },
    { SPELL_INNER_FLAME, ZAP_INNER_FLAME },
    { SPELL_HASTE, ZAP_HASTE },
    { SPELL_INVISIBILITY, ZAP_INVISIBILITY },
    { SPELL_DIG, ZAP_DIG },
    { SPELL_EXPLOSIVE_BOLT, ZAP_EXPLOSIVE_BOLT },
    { SPELL_CRYSTAL_BOLT, ZAP_CRYSTAL_BOLT },
    { SPELL_TUKIMAS_DANCE, ZAP_TUKIMAS_DANCE },
    { SPELL_CORROSIVE_BOLT, ZAP_CORROSIVE_BOLT },
    { SPELL_DEBUGGING_RAY, ZAP_DEBUGGING_RAY },

    // These are all for zap -> spell lookup.
    { SPELL_QUICKSILVER_BOLT, ZAP_QUICKSILVER_BOLT },
    { SPELL_QUICKSILVER_BOLT, ZAP_BREATHE_POWER },
    { SPELL_STICKY_FLAME, ZAP_STICKY_FLAME },
    { SPELL_STICKY_FLAME_RANGE, ZAP_STICKY_FLAME_RANGE },
    { SPELL_STICKY_FLAME_SPLASH, ZAP_BREATHE_STICKY_FLAME },
    { SPELL_DAZZLING_SPRAY, ZAP_DAZZLING_SPRAY },
    { SPELL_STEAM_BALL, ZAP_BREATHE_STEAM },
    { SPELL_ORB_OF_ELECTRICITY, ZAP_ORB_OF_ELECTRICITY },
    { SPELL_CHILLING_BREATH, ZAP_BREATHE_FROST },
};

zap_type spell_to_zap(spell_type spell)
{
    // This is to make sure that spl-cast.cc doesn't just zap dazzling
    // spray right away.
    if (spell == SPELL_DAZZLING_SPRAY)
        return NUM_ZAPS;

    for (size_t i = 0; i < ARRAYSZ(_spl_zaps); i++)
        if (_spl_zaps[i][0] == spell)
            return (zap_type) _spl_zaps[i][1];

    return NUM_ZAPS;
}

spell_type zap_to_spell(zap_type zap)
{
    for (size_t i = 0; i < ARRAYSZ(_spl_zaps); i++)
        if (_spl_zaps[i][1] == zap)
            return (spell_type) _spl_zaps[i][0];

    return SPELL_NO_SPELL;
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
