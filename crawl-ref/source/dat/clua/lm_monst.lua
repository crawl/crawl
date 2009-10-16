---------------------------------------------------------------------------
-- lm_monst.lua
-- Create a monster on certain events
--------------------------------------------------------------------------

--------------------------------------------------------------------------------------
-- This marker creates a monster on certain events.  It uses the following parameters:
--
-- * death_monster: The name of the monster who's death triggers the creation of
--       the new monster.
--
-- * new_monster: The name of the monster to create.
--
-- * message: Message to give when monster is created, regardless of whether
--       or not the player can see the marker.
--
-- * message_seen: Message to give if the player can see the marker.
--
-- * message_unseen: Message to give if the player can't see the marker.
--
-- NOTE: If the feature where the marker is isn't habitable by the new monster,
-- the feature will be changed to the monster's primary habitat.
--------------------------------------------------------------------------------------

-- TODO:
--  * Place more than one monster.
--  * Place monster displaced from marker.

MonsterOnTrigger = { CLASS = "MonsterOnTrigger" }
MonsterOnTrigger.__index = MonsterOnTrigger

function MonsterOnTrigger:_new()
  local pm = { }
  setmetatable(pm, self)
  self.__index = self

  return pm
end

function MonsterOnTrigger:new(pars)
  pars = pars or { }

  if not pars.death_monster or pars.death_monster == "" then
    error("Must provide death_monster")
  end

  if not pars.new_monster or pars.new_monster == "" then
    error("Must provide new_monster")
  end

  pars.message_seen   = pars.message_seen or pars.message or ""
  pars.message_unseen = pars.message_unseen or pars.message or ""

  local pm = self:_new()
  pm.message_seen    = pars.message_seen
  pm.message_unseen  = pars.message_unseen
  pm.death_monster   = pars.death_monster
  pm.new_monster     = pars.new_monster
  pm.props           = pars

  return pm
end

function MonsterOnTrigger:property(marker, pname)
  return self.props[pname] or ''
end

function MonsterOnTrigger:write(marker, th)
  lmark.marshall_table(th, self)
end

function MonsterOnTrigger:read(marker, th)
  return MonsterOnTrigger:new(lmark.unmarshall_table(th))
end

function MonsterOnTrigger:activate(marker)
  dgn.register_listener(dgn.dgn_event_type('monster_dies'), marker)
end

function MonsterOnTrigger:event(marker, ev)
  local midx = ev:arg1()
  local mons = dgn.mons_from_index(midx)

  if not mons then
    error("MonsterOnTrigger:event() didn't get a valid monster index")
  end

  if mons.name == self.death_monster then
    local x,     y     = marker:pos()
    local you_x, you_y = you.pos()

    if x == you_x and y == you_y then
      return
    end

    -- you.losight() seems to be necessary if the player has been moved by
    -- a wizard command and then the marker triggered by another wizard
    -- command, since then no turns will have been taken and the LOS info
    -- won't have been updated.
    you.losight()
    local see_grid = you.see_grid(x, y)

    if (not dgn.create_monster(x, y, self.new_monster)) then
      return
    elseif self.message_seen ~= "" and see_grid then
      crawl.mpr(self.message_seen)
    elseif self.message_unseen ~= "" and not see_grid then
      crawl.mpr(self.message_unseen)
    end

    dgn.remove_listener(marker)
    dgn.remove_marker(marker)
  end
end

function monster_on_trigger(pars)
  return MonsterOnTrigger:new(pars)
end
