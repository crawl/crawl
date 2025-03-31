#pragma once

#include "ability-type.h"
#include "spell-type.h"
#include "zap-type.h"

zap_type spell_to_zap(spell_type spell);
zap_type ability_to_zap(ability_type abil);
spell_type zap_to_spell(zap_type zap);

// Zaps have their own cap -- translate to spell power.
int spell_zap_power_cap(spell_type spell);
