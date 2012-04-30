---------------------------------------------------------------------------
-- lm_flags.lua
--------------------------------------------------------------------------
-- This file exists only for save compatibility, please remove it after
-- TAG_MAJOR_VERSION goes past 33.
--------------------------------------------------------------------------

require('dlua/lm_trig.lua')

ChangeFlags       = util.subclass(Triggerable)
ChangeFlags.CLASS = "ChangeFlags"

function ChangeFlags:write(marker, th)
  ChangeFlags.super.write(self, marker, th)

  file.marshall(th, "")
  file.marshall(th, "")
  file.marshall(th, "")
  lmark.marshall_table(th, {})
end

function ChangeFlags:read(marker, th)
  ChangeFlags.super.read(self, marker, th)

  file.unmarshall_string(th)
  file.unmarshall_string(th)
  file.unmarshall_string(th)
  lmark.unmarshall_table(th)

  setmetatable(self, ChangeFlags)

  return self
end
