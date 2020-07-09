-- Usage: include this from your rc file, then bind a key to ===damage_tally
--
-- All monsters will be frozen, and total damage done will be shown after
-- every turn.

util.namespace('damt')

function damt.boost_monster_hp()
  crawl.call_dlua(
    "local gxm, gym = dgn.max_bounds();" ..
    "for p in iter.rect_iterator(dgn.point(1, 1), dgn.point(gxm-2, gym-2)) do " ..
    "  local mons = dgn.mons_at(p.x, p.y);" ..
    "  if mons ~= nil and not mons.wont_attack then" ..
    "    mons.set_max_hp(10000);" ..
    "  end;" ..
    "end")
end

-- assumes boost_monster_hp() has been called
function damt.calc_total_damage()
  return crawl.call_dlua(
    "local dam = 0;" ..
    "local gxm, gym = dgn.max_bounds();" ..
    "for p in iter.rect_iterator(dgn.point(1, 1), dgn.point(gxm-2, gym-2)) do " ..
    "  local mons = dgn.mons_at(p.x, p.y);" ..
    "  if mons ~= nil and not mons.wont_attack then" ..
    "    dam = dam + 10000 - mons.hp;" ..
    "  end;" ..
    "end;" ..
    "return dam")
end

function damt.calc_num_damaged()
  return crawl.call_dlua(
    "local cnt = 0;" ..
    "local gxm, gym = dgn.max_bounds();" ..
    "for p in iter.rect_iterator(dgn.point(1, 1), dgn.point(gxm-2, gym-2)) do " ..
    "  local mons = dgn.mons_at(p.x, p.y);" ..
    "  if mons ~= nil and not mons.wont_attack and mons.hp < 10000 then" ..
    "    cnt = cnt + 1;" ..
    "  end;" ..
    "end;" ..
    "return cnt")
end

-- initialize everything
function damage_tally()
  util.foreach({"spawns","mon_act","mon_regen","player_regen","death"},
               function(d)
                 crawl.call_dlua("debug.disable('" .. d .. "')")
               end)
  damt.boost_monster_hp()
  damt.turns   = you.turns()
  damt.time    = you.time()
  damt.mana    = you.mp()
  damt.enabled = true
end

function damt.show()
  local dam = damt.calc_total_damage()
  local cnt = damt.calc_num_damaged()
  local t = you.time()-damt.time;
  local m = damt.mana-you.mp();
  local out = "<green>Turns: </green><lightgreen>" ..
                  (you.turns()-damt.turns) .. "</lightgreen>" ..
              "<green>, time: </green><lightgreen>" ..
                  string.format("%1.1f", t/10) .. "</lightgreen>" ..
              "<green>, damage: </green><lightgreen>" ..
                  dam .. "</lightgreen>"
  if t > 0 then
    out = out .. "<green>, dpt: </green><lightgreen>" ..
                     string.format("%1.1f", dam*10/t) .. "</lightgreen>"
  end
  if m > 0 then
    out = out .. "<green>, dpm: </green><lightgreen>" ..
                     string.format("%1.1f", dam/m) .. "</lightgreen>"
  end
  if cnt > 0 then
    out = out .. "<green>, per-mons avg: </green><lightgreen>" ..
                     string.format("%1.1f", dam/cnt) .. "</lightgreen>"
  end
  crawl.mpr(out)
end

function ready()
  if damt.enabled then
    damt.show()
  end
end
