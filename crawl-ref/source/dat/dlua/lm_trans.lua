------------------------------------------------------------------------------
-- lm_trans.lua:
-- Between-vault transporter markers. For transporters within the same vault,
-- it's not necessary to use this class, e.g.:
--
-- KFEAT: A = transporter B
--
-- will work to make a transporter from A to the location of glyph B within the
-- same vault. For transporters between vaults, use the trans_loc{} and
-- trans_dest_loc{} markers in the respective vaults. For the vault with the
-- transporter:
--
-- MARKER A = lua:trans_loc("my_special_name")
--
-- and for the vault with the destination:
--
-- MARKER B = lua:trans_dest_loc("my_special_name")
--
-- where "my_special_name" is a unique string used in both markers. Then in
-- either vault (or in the parent vault if subvaulting), add an epilogue:
--
-- epilogue{{
--   make_transporters_from_markers()
-- }}
------------------------------------------------------------------------------

require('dlua/util.lua')

-- A subclass of PortalDescriptor that sets the _transporter property
-- with a unique name.
Transporter = util.subclass(PortalDescriptor)
Transporter.CLASS = "Transporter"

function Transporter:new(name)
  if not name then
    error("No transporter name provided")
  end

  local t = self.super.new(self)
  t.props = {_transporter = name}

  return t
end

function transp_loc(name)
  return Transporter:new(name)
end

-- A subclass of PortalDescriptor that sets the _transporter_dest property
-- with a unique name.
TransporterDestination = util.subclass(PortalDescriptor)
TransporterDestination.CLASS = "TransporterDestination"

function TransporterDestination:new(name)
  if not name then
    error("No transporter destination name provided")
  end

  local t = self.super.new(self)
  t.props = {_transporter_dest = name}

  return t
end

function transp_dest_loc(name)
  return TransporterDestination:new(name)
end

function make_transporters_from_markers()
    local dest_markers = dgn.find_markers_by_prop("_transporter_dest");
    local dest_map = {}
    for _, dmarker in ipairs(dest_markers) do
        local name = dmarker:property("_transporter_dest")
        if dest_map[name] ~= nil then
            error("Multiple transporter destination markers with name " ..
                  name)
        end
        dest_map[name] = dmarker
    end

    local tele_markers = dgn.find_markers_by_prop("_transporter");
    for _, tmarker in ipairs(tele_markers) do
        local name = tmarker:property("_transporter")
        if dest_map[name] == nil then
            error("Transporter with name " .. name ..
                  "has no destination marker.")
        end
        local tx, ty = tmarker:pos()
        local dx, dy = dest_map[name]:pos()
        dgn.place_transporter(tx, ty, dx, dy)
    end
end
