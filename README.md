# Dungeon Crawl Stone Soup: Spicy fork

---

This fork of DCSS is intended to be a stable and playable fork to migrate many of the circus animals fork changes to, in a way that is fully configurable. In other words, with no options set, this fork should behave exactly the same as vanilla crawl. All alterations are activated through init settings. This fork will regularly pull in upstream changes to keep it up to date, so whatever new features are added to vanilla crawl will come over here as well.

Feel free to create issues on github for any problems with this fork. Ideas you have that you think are in line with my goals are also welcome, or concerns you have about future plans.

I'll pull over features little by little from circus animals into this fork.

Current features that differ from vanilla crawl:

Multiple difficulty levels:
  to enable in init.txt: multiple_difficulty_levels=true
  when starting a game, it will ask the player for what difficulty level they want to play.
  difficulty levels:
    easy:
      monsters have 20% less health. 
      monsters do 20% less damage.
      monsters have 20% slower attack
    standard:
      monsters behave exactly as in vanilla crawl
    challenge:
      monsters have 10% more health
      monsters do 10% more damage
      monsters have 10% faster attack
    nightmare:
      monsters have 20% more health
      monsters do 20% more damage
      monsters have 20% faster attack

To Do:
  one hit kill protection
  foodless mode
  unlimited basic ammo mode
  
