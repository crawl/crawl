------------------------------------------------------------------------------
-- loadmaps.lua:
--
-- Compiles and loads .des files that Crawl needs.
------------------------------------------------------------------------------

local des_files = file.datadir_files_recursive("dat/des", ".des")

for _, file in ipairs(des_files) do
  dgn.register_des_file("des/" .. file)
end
