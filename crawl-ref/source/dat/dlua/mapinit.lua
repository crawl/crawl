-----------------------------------------------------------------
-- Register tags that are used only as flags.
--
-- This is used when validating maps to reject maps that cannot
-- be placed by DEPTH, PLACE or TAGS.
-----------------------------------------------------------------

local function register_flags(tags)
  local pieces = crawl.split(tags, " ")
  for _, flag in ipairs(pieces) do
    dgn.map_register_flag(flag)
  end
end

register_flags("minivault mini_float allow_dup extra")
register_flags("no_hmirror no_vmirror no_rotate no_pool_fixup")
register_flags("no_wall_fixup dummy transparent no_monster_gen")
register_flags("no_item_gen no_trap_gen ruin")
register_flags("generate_awake no_dump generate_loot overwritable")
register_flags("water_ok unrand place_unique luniq uniq")
register_flags("patrolling")
