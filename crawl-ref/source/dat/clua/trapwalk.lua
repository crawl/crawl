---------------------------------------------------------------------------
-- trapwalk.lua:
-- (Thanks to JPEG for this script.)
--
-- Allows travel to cross traps provided you have sufficient HP to survive the
-- trap.
--
-- To use this, add this line to your init.txt:
--   lua_file = lua/trapwalk.lua
-- and add
--   trapwalk_safe_hp = dart:15, needle:25, spear:50
-- or similar to your init.txt.
--
-- What it does:
--
--  * Normally autotravel automatically avoids all traps
--  * This script allows you to customize at which hp what type of trap is
--    regarded as safe for autotravel
---------------------------------------------------------------------------

-- Travel will cross certain traps if you have more than safe_hp hp.

function ch_cross_trap(trap)

   if not options.trapwalk_safe_hp then
      return false
   end

   local opt = options.trapwalk_safe_hp

   local hpstr
   _, _, hpstr = string.find(opt, trap .. "%s*:%s*(%d+)")

   if not hpstr then
      return false
   end

   local safe_hp = tonumber(hpstr)
   local hp = you.hp()

   -- finally compare current hp with safe limit
   return hp >= safe_hp
end
