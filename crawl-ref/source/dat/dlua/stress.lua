-- helper functions for stress tests

util.namespace("stress")

function stress.awaken_level()
  local gxm, gym = dgn.max_bounds()
  local wander = mons.behaviour("wander")
  for p in iter.rect_iterator(dgn.point(1, 1), dgn.point(gxm-2, gym-2)) do
    local mons = dgn.mons_at(p.x, p.y)
    if mons ~= nil then
      mons.beh = wander
    end
  end
end

function stress.entomb()
  local x, y = you.pos()
  for p in iter.rect_iterator(dgn.point(x-1, y-1), dgn.point(x+1, y+1)) do
    if x ~= p.x or y ~= p.y then
      dgn.terrain_changed(p.x, p.y, "metal_wall", false, false)
    end
  end
end

function stress.boost_monster_hp()
  local gxm, gym = dgn.max_bounds()
  for p in iter.rect_iterator(dgn.point(1, 1), dgn.point(gxm-2, gym-2)) do
    local mons = dgn.mons_at(p.x, p.y)
    if mons ~= nil then
      mons.set_max_hp(10000)
    end
  end
end

function stress.fill_level(x)
  -- this will impact the player's position, potentially forcing a tele...
  local gxm, gym = dgn.max_bounds()
  for p in iter.rect_iterator(dgn.point(1, 1), dgn.point(gxm-2, gym-2)) do
    dgn.terrain_changed(p.x, p.y, x, false, false)
  end
end
