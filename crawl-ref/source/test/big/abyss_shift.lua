crawl.message("Testing abyssal shifts.")

debug.goto_place("Abyss")
test.regenerate_level()

for i = 0, 10000 do
  you.teleport_to(68, 5 + crawl.random2(50))
end
