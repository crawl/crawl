# Dungeon Crawl Stone Soup: Spicy fork

---

This fork of DCSS is intended to be a stable and playable fork to migrate many of the circus animals fork changes to, in a way that is fully configurable. In other words, with no options set, this fork should behave exactly the same as vanilla crawl. All alterations are activated through init settings. This fork will regularly pull in upstream changes to keep it up to date, so whatever new features are added to vanilla crawl will come over here as well.

Feel free to create issues on github for any problems with this fork. Ideas you have that you think are in line with my goals are also welcome, or concerns you have about future plans.

I'll pull over features little by little from circus animals into this fork.

Current features that differ from vanilla crawl:

# Multiple difficulty levels: (multiple_difficulty_levels=true)
When starting a game, the player will be asked for what difficulty level they want to play.

- difficulty levels:
  - easy:
    - monsters have 20% less health. 
    - monsters do 20% less damage.
    - monsters have 20% slower attack
  - standard:
    - monsters behave exactly as in vanilla crawl
  - challenge:
    - monsters have 10% more health
    - monsters do 10% more damage
    - monsters have 10% faster attack
  - nightmare:
    - monsters have 20% more health
    - monsters do 20% more damage
    - monsters have 20% faster attack

# Different experience sources: (different_experience_sources=true)
Instead of only getting experience from killing monsters, this option introduces several alternatives, opening up new playstyles.

- You will gain experience from killing monsters 
- You will gain experience for each new floor that you enter
- You will gain experience from drinking experience potions that appear once on each floor, based on the floor you drink them on. In other words, saving the experience potion to drink on a lower floor will give greater experience. 
- You will gain experience from pacifying monsters (like before)

However, experience doesn't stack up like before. The amount of experience gained from any of these things are relative to the difficulty of the challenge and level of the player. It's balanced in a way that if you only focussed on killing monsters, you'd end up with about the same experience that you would in normal crawl. On the other hand, you can get a comparable level of experience just focussing on diving as deep as you can and avoiding most monsters. But if you do both, you'll not gain any additional experience. Another example: if you collect a bunch of experience potions and drink them all one after another on a deep dungeon level, your first few will give you a bunch of experience, but each following potion will rapidly diminish in how much experience it provides. 

## To Do:
- one hit kill protection
- foodless mode
- unlimited basic ammo mode
- level cap removal
- spell power cap removal
- instant rest 
