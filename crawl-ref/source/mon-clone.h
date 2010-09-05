#ifndef MON_CLONE_H
#define MON_CLONE_H

// Formerly in mon-stuff:
bool mons_clonable(const monsters* orig, bool needs_adjacent = true);
int  clone_mons(const monsters* orig, bool quiet = false,
                bool* obvious = NULL, coord_def pos = coord_def(0, 0) );

void mons_summon_illusion_from(monsters* mons, actor *foe,
                               spell_type spell_cast = SPELL_NO_SPELL);

bool actor_is_illusion_cloneable(actor *target);

#endif
