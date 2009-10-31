------------------------------------------------------------------------------
-- loadmaps.lua:
--
-- Compiles and loads .des files that Crawl needs.
------------------------------------------------------------------------------

local des_files = file.datadir_files("dat", ".des")

for _, file in ipairs(des_files) do
   dgn.load_des_file(file)
end
