#ifndef SPELL_ZAP_H
#define SPELL_ZAP_H

zap_type spell_to_zap(spell_type spell);

// Translate from spell to zap power (usually identical).
int spell_zap_power(spell_type spell, int powc);

#endif
