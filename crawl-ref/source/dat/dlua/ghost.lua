-- This function should be called in every ghost vault, since it sets the
-- minimal tags we want as well as the common ghost chance. If you want your
-- vault to be less common, use a WEIGHT statement; don't set a difference
-- CHANCE from the one here.
function ghost_vault_setup(e)
  e.tags("chance_player_ghost allow_dup luniq_player_ghost")
  e.tags("no_tele_into no_trap_gen no_monster_gen")

  if not you.in_branch("Vaults") then
      e.tags("chance_player_ghost")
      e.chance("10%")
  end
end
