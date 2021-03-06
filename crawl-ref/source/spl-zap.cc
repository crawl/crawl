#include "AppHdr.h"

#include "spl-zap.h"

#include "beam.h"
#include "stepdown.h"
#include "zap-type.h"

static pair<spell_type, zap_type> _spl_zaps[] =
{
    { SPELL_MAGIC_DART, ZAP_MAGIC_DART },
    { SPELL_FORCE_LANCE, ZAP_FORCE_LANCE },
    { SPELL_THROW_FLAME, ZAP_THROW_FLAME },
    { SPELL_THROW_FROST, ZAP_THROW_FROST },
    { SPELL_PAIN, ZAP_PAIN },
    { SPELL_FLAME_TONGUE, ZAP_FLAME_TONGUE },
    { SPELL_SHOCK, ZAP_SHOCK },
    { SPELL_STING, ZAP_STING },
    { SPELL_CIGOTUVIS_PLAGUE, ZAP_CIGOTUVIS_PLAGUE },
    { SPELL_BOLT_OF_FIRE, ZAP_BOLT_OF_FIRE },
    { SPELL_BOLT_OF_COLD, ZAP_BOLT_OF_COLD },
    { SPELL_PRIMAL_WAVE, ZAP_PRIMAL_WAVE },
    { SPELL_SANDBLAST, ZAP_SANDBLAST },
    { SPELL_STONE_ARROW, ZAP_STONE_ARROW },
    { SPELL_POISON_ARROW, ZAP_POISON_ARROW },
    { SPELL_IRON_SHOT, ZAP_IRON_SHOT },
    { SPELL_LIGHTNING_BOLT, ZAP_LIGHTNING_BOLT },
    { SPELL_BOLT_OF_MAGMA, ZAP_BOLT_OF_MAGMA },
    { SPELL_VENOM_BOLT, ZAP_VENOM_BOLT },
    { SPELL_BOLT_OF_DRAINING, ZAP_BOLT_OF_DRAINING },
    { SPELL_LEHUDIBS_CRYSTAL_SPEAR, ZAP_LEHUDIBS_CRYSTAL_SPEAR },
    { SPELL_LEHUDIBS_CRYSTAL_SHOT, ZAP_LEHUDIBS_CRYSTAL_SHOT },
    { SPELL_FIREBALL, ZAP_FIREBALL },
    { SPELL_MEPHITIC_CLOUD, ZAP_MEPHITIC },
    { SPELL_VIOLENT_UNRAVELLING, ZAP_UNRAVELLING },
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
    { SPELL_HURL_DAMNATION, ZAP_HURL_DAMNATION },
    { SPELL_CORONA, ZAP_CORONA },
    { SPELL_ENSLAVEMENT, ZAP_ENSLAVEMENT },
    { SPELL_BANISHMENT, ZAP_BANISHMENT },
    { SPELL_SLOW, ZAP_SLOW },
    { SPELL_CONFUSE, ZAP_CONFUSE },
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
    { SPELL_BECKONING, ZAP_BECKONING },
    { SPELL_DEBUGGING_RAY, ZAP_DEBUGGING_RAY },
    { SPELL_OZOCUBUS_REFRIGERATION, ZAP_REFRIGERATE},
    { SPELL_DRAIN_LIFE, ZAP_DRAIN_LIFE},
    { SPELL_SONIC_WAVE, ZAP_SONIC_WAVE},
    { SPELL_STARBURST, ZAP_BOLT_OF_FIRE },
    { SPELL_IGNITION, ZAP_IGNITION },
    { SPELL_FIRE_STORM, ZAP_FIRE_STORM },
    { SPELL_ERINGYAS_ROOTSPIKE, ZAP_ERINYA_ROOT_SPIKE },
    { SPELL_OLGREBS_LAST_MERCY, ZAP_OLGREB_LAST_MERCY },

    // monster-specific
    { SPELL_SLUG_DART, ZAP_SLUG_DART },
    { SPELL_PRIMAL_WAVE, ZAP_PRIMAL_WAVE },
    { SPELL_BLINKBOLT, ZAP_BLINKBOLT },
    { SPELL_STEAM_BALL, ZAP_BREATHE_STEAM },
    { SPELL_DIMENSION_ANCHOR, ZAP_DIMENSION_ANCHOR },
    { SPELL_STRIP_RESISTANCE, ZAP_VULNERABILITY },
    { SPELL_SENTINEL_MARK, ZAP_SENTINEL_MARK },
    { SPELL_VIRULENCE, ZAP_VIRULENCE },
    { SPELL_SAP_MAGIC, ZAP_SAP_MAGIC },
    { SPELL_DRAIN_MAGIC, ZAP_DRAIN_MAGIC },
    { SPELL_HARPOON_SHOT, ZAP_HARPOON_SHOT},
    { SPELL_THROW_PIE, ZAP_THROW_PIE},
    { SPELL_HOLY_BREATH, ZAP_HOLY_BREATH },
    { SPELL_SPIT_ACID, ZAP_SPIT_ACID },
    { SPELL_CHAOS_BREATH, ZAP_CHAOS_BREATH },
    { SPELL_COLD_BREATH, ZAP_COLD_BREATH },
    { SPELL_FIRE_BREATH, ZAP_FIRE_BREATH },
    { SPELL_SEARING_BREATH, ZAP_SEARING_BREATH },
    { SPELL_ELECTRICAL_BOLT, ZAP_ELECTRICAL_BOLT },
    { SPELL_FLAMING_CLOUD, ZAP_FLAMING_CLOUD },
    { SPELL_NOXIOUS_CLOUD, ZAP_NOXIOUS_CLOUD },
    { SPELL_POISONOUS_CLOUD, ZAP_POISONOUS_CLOUD },
    { SPELL_ENERGY_BOLT, ZAP_ENERGY_BOLT },
    { SPELL_METAL_SPLINTERS, ZAP_METAL_SPLINTERS },
    { SPELL_THORN_VOLLEY, ZAP_THORN_VOLLEY },
    { SPELL_CALL_DOWN_DAMNATION, ZAP_CALL_DOWN_DAMNATION },
    { SPELL_SPIT_POISON, ZAP_SPIT_POISON },
    { SPELL_MIASMA_BREATH, ZAP_MIASMA_BREATH },
    { SPELL_PETRIFYING_CLOUD, ZAP_PETRIFYING_CLOUD },
    { SPELL_GHOSTLY_FIREBALL, ZAP_GHOSTLY_FIREBALL },
    { SPELL_FLASH_FREEZE, ZAP_FLASH_FREEZE },
    { SPELL_SPIT_LAVA, ZAP_SPIT_LAVA },
    { SPELL_THROW_BARBS, ZAP_THROW_BARBS },
    { SPELL_FOXFIRE, ZAP_FOXFIRE },

    // These are all for zap -> spell lookup.
    { SPELL_QUICKSILVER_BOLT, ZAP_QUICKSILVER_BOLT },
    { SPELL_QUICKSILVER_BOLT, ZAP_BREATHE_POWER },
    { SPELL_STICKY_FLAME, ZAP_STICKY_FLAME },
    { SPELL_STICKY_FLAME_RANGE, ZAP_STICKY_FLAME_RANGE },
    { SPELL_DAZZLING_SPRAY, ZAP_DAZZLING_SPRAY },
    { SPELL_STEAM_BALL, ZAP_BREATHE_STEAM },
    { SPELL_ORB_OF_ELECTRICITY, ZAP_ORB_OF_ELECTRICITY },
    { SPELL_CHILLING_BREATH, ZAP_BREATHE_FROST },
    { SPELL_HEAL_WOUNDS, ZAP_HEAL_WOUNDS },
    { SPELL_ICEBLAST, ZAP_ICEBLAST },
    { SPELL_ACID_SPLASH, ZAP_BREATHE_ACID },
    { SPELL_BORGNJORS_VILE_CLUTCH, ZAP_VILE_CLUTCH},
    { SPELL_TELEPORT, ZAP_TELEPORT_OTHER },
};

zap_type spell_to_zap(spell_type spell)
{
    // This is to make sure that spl-cast.cc doesn't just zap dazzling
    // spray right away.
    if (spell == SPELL_DAZZLING_SPRAY)
        return NUM_ZAPS;

    for (const auto &spzap : _spl_zaps)
        if (spzap.first == spell)
            return spzap.second;

    return NUM_ZAPS;
}

spell_type zap_to_spell(zap_type zap)
{
    for (const auto &spzap : _spl_zaps)
        if (spzap.second == zap)
            return spzap.first;

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
