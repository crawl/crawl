#pragma once

#include "zap-type.h"

zap_type spell_to_zap(spell_type spell);
spell_type zap_to_spell(zap_type zap);

// Translate from spell to zap power (usually identical).
int spell_zap_power(spell_type spell, int powc);

// Zaps have their own cap -- translate to spell power.
int spell_zap_power_cap(spell_type spell);

