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

-- Make an item definition that will randomly choose from combinations of the
-- given tables of weighted item types and optional egos.
--
-- @param items     A table with weapon names as keys and weights as values.
-- @param egos      An optional table with ego names as keys and weights as
--                  values.
-- @param args      An optional string of arguments to use on every item entry.
--                  Should not have leading or trailing whitespace.
-- @param separator An optional separator to use between the item entries.
--                  Defaults to '/', which is appropriate for ITEM statements.
--                  Use '|' if making item statements for use with MONS.
-- @returns A string containing the item definition.
function random_item_def(items, egos, args, separator)
    args = args ~= nil and " " .. args or ""
    separator = separator ~= nil and separator or '/'
    local item_def

    for iname, iweight in pairs(items) do
        -- If we have egos, define an item spec with all item+ego
        -- combinations, each with weight scaled by item rarity and ego
        -- rarity.
        if egos ~= nil then
            for ename, eweight in pairs(egos) do
                if (not iname:find("demon") or ename ~= "holy_wrath")
                   and (not make_arte or ename ~= "none") then
                    def = iname .. args .. " ego:" .. ename .. " w:" ..
                          math.floor(iweight * eweight)
                    if item_def == nil then
                         item_def = def
                    else
                         item_def = item_def .. " " .. separator .. " " .. def
                    end
                end
            end
        -- No egos, so define item spec with all item combinations, each with
        -- weight scaled by item rarity.
        else
            def = iname .. args .. " w:" .. iweight
            if item_def == nil then
                 item_def = def
            else
                 item_def = item_def .. " " .. separator .. " " .. def
            end
        end
    end
    return item_def
end
