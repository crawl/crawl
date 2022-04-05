# DCSS Background Definitions

DCSS uses YAML files to define backgrounds (known as jobs in the code); these files can be found in [source/dat/jobs](../../source/dat/jobs).

To create a new background, you put an appropriate YAML file in there and compile DCSS. For any custom background behaviour which the YAML file can't express (starting gods, for example), you'll also need custom C++ code added elsewhere.

## Species Definition Format

You can view existing files to get a sense of the format; it's quite straightforward.

### Supported Keys

| key  | type | required | description |
| ---- | ---- | -------- | ----------- |
| enum | `string` | **Yes** | Set the job enum. Must match the pattern `JOB_[A-Z]+`. |
| name | `string` | **Yes** | Species name, like 'Deep Dwarf' or 'Felid'. Required. Must be unique. |
| short_name | `string` | No | Two letter background code. Must be unique. Defaults to the first two letters of the name. |
| category | `string` | **Yes** | Which column the background is located in on the new game background select screen. Can be `Warrior`, `Adventurer`, `Zealot`, `Warrior-mage`, `Mage`, or `false` (will not be shown on background select screen). To add a new category, see [source/util/job-gen.py](../../source/util/job-gen.py). |
| category_priority | `int` | No | The priority of the background within its category in the new game background select screen. Higher priorities are displayed first. Defaults to 0. |
| str | `int` | **Yes** | Starting strength. Note: these starting stats are added to the player's species' base stats. Currently, all backgrounds' stats (str, int, dex) sum to 12 (except Wanderer), although this is not mandatory. |
| int | `int` | **Yes** | Starting intelligence. |
| dex | `int` | **Yes** | Starting dexterity. |
| equipment | `array of strings` | No | The starting equipment the player gets. Does not include the starting ration, starting weapons that the player gets to choose, or starting ammo from weapons the player has chosen. Items in this list are formatted in the same way as for vaults, that is, according to the following structure: `"<full, singular name> plus:<item enchantment> ego:<brand/ego> q:<quantity>"`, where only the name is a required field.
| weapon_choice | `string` | No | The choice of weapons the background gets before starting the game. Can be: <ul><li>`plain`: falchion, hand axe, mace, etc.</li><li>`good`: long sword, war axe, flail, etc.</li><li>`ranged`: shortbow, sling, crossbow, etc.</li><li>`none` (no choice of weapon) [default]</li></ul> |
| recommended_species | `array of strings` | **Sometimes** | Species the background is recommended for. Species must match the pattern `SP_[A-Z_]+`. For backgrounds with `difficulty` set to `false`, this parameter is irrelevant and therefore optional; otherwise it is required. |
| skills | `map` | No | Each skill must be a valid skill enum value, for example `SK_FIGHTING` for fighting skill, or be `SK_WEAPON`, which is used to allocate weapon skill where the player has a choice of weapon. These skills are followed by the skill level, a positive `int`, which is modified by the species aptitude before the game starts.

### Advanced Keys

You probably don't need or want to use these.

| key  | type | required | description |
| ---- | ---- | -------- | ----------- |
| TAG_MAJOR_VERSION | `int` | No | Wrap the background in `#if TAG_MAJOR_VERSION == [...]` to deprecate it. See the deprecated species [mottled draconian](../../source/dat/species/deprecated-draconian-mottled.yaml) for an example. |
| create_enum | `boolean` | No | Set `false` if the background already has an enum in job-type.h. Any new backgrounds should not set this (and will fail to compile if you try). Defaults to `true`. |
