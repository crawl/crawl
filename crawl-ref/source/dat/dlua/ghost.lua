-- Integer value from 0 to 100 giving the chance that a ghost-allowing level
-- will attempt to place a ghost vault.
_GHOST_CHANCE_PERCENT = 10

-- Common setup we want regardless of the branch of the ghost vault.
function ghost_setup_common(e)
     e.tags("allow_dup luniq_player_ghost")
     e.tags("no_tele_into no_trap_gen no_monster_gen")
end

-- For vaults that want more control over their tags, use this function to
-- properly set the common chance value and tag.
function set_ghost_chance(e)
    e.tags("chance_player_ghost")
    e.chance(math.floor(_GHOST_CHANCE_PERCENT / 100 * 10000))
end

-- This function should be called in every ghost vault that doesn't place in
-- the Vaults branch. It sets the minimal tags we want as well as the common
-- ghost chance. If you want your vault to be less common, use a WEIGHT
-- statement; don't set a difference CHANCE from the one here.
function ghost_setup(e)
    ghost_setup_common(e)

    set_ghost_chance(e)
end

-- Every Vaults room that places a ghost should call this function.
function vaults_ghost_setup(e)
    ghost_setup_common(e)

    -- Vaults branch layout expects vaults_ghost as a tag selector.
    e.tags("vaults_ghost")
end
