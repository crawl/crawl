---------------------------------------------------------------------------
-- lm_monst.lua
-- Create a monster on certain events
--------------------------------------------------------------------------

-------------------------------------------------------------------------------
-- This marker creates a monster on certain events. It uses the following
-- parameters:
--
-- * death_monster: The name of the monster who's death triggers the creation
--       of the new monster.
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
-------------------------------------------------------------------------------

-- TODO:
--  * Place more than one monster.
--  * Place monster displaced from marker.
--  * Be triggered more than once.

crawl_require('dlua/lm_trig.lua')

MonsterOnTrigger       = util.subclass(Triggerable)
MonsterOnTrigger.CLASS = "MonsterOnTrigger"

function MonsterOnTrigger:new(pars)
  pars = pars or { }

  if not pars.new_monster or pars.new_monster == "" then
    error("Must provide new_monster")
  end

  pars.message_seen   = pars.message_seen or pars.message or ""
  pars.message_unseen = pars.message_unseen or pars.message or ""

  local mot = self.super.new(self)

  mot.message_seen    = pars.message_seen
  mot.message_unseen  = pars.message_unseen
  mot.death_monster   = pars.death_monster
  mot.new_monster     = pars.new_monster
  mot.props           = util.cathash(mot.props or { }, pars)

  return mot
end

function MonsterOnTrigger:write(marker, th)
  MonsterOnTrigger.super.write(self, marker, th)

  file.marshall(th, self.message_seen)
  file.marshall(th, self.message_unseen)
  file.marshall(th, self.new_monster)
  lmark.marshall_table(th, self.props)
end

function MonsterOnTrigger:read(marker, th)
  MonsterOnTrigger.super.read(self, marker, th)

  self.message_seen   = file.unmarshall_string(th)
  self.message_unseen = file.unmarshall_string(th)
  self.new_monster    = file.unmarshall_string(th)
  self.props          = lmark.unmarshall_table(th)

  setmetatable(self, MonsterOnTrigger)

  return self
end

function MonsterOnTrigger:on_trigger(triggerer, marker, ev)
  local x,     y     = marker:pos()
  local you_x, you_y = you.pos()

  if x == you_x and y == you_y then
    return
  end

  if dgn.mons_at(x, y) then
    return
  end

  local wanted_feat = self.props.monster_place_feature
  if wanted_feat and wanted_feat ~= dgn.grid(x, y) then
    return
  end

  local see_cell = you.see_cell(x, y)

  if (not dgn.create_monster(x, y, self.new_monster)) then
    return
  elseif self.message_seen ~= "" and see_cell then
    crawl.mpr(self.message_seen)
  elseif self.message_unseen ~= "" and not see_cell then
    crawl.mpr(self.message_unseen)
  end

  self:remove(marker)
end

function monster_on_death(pars)
  local death_monster = pars.death_monster or pars.target

  pars.death_monster = nil
  pars.target        = nil

  local mod = MonsterOnTrigger:new(pars)

  mod:add_triggerer(
                     DgnTriggerer:new {
                       type   = "monster_dies",
                       target = death_monster
                     }
                   )

  return mod
end
