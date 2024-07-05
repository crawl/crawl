# DCSS Species Definitions

DCSS uses YAML files to define species, these files can be found in [source/dat/species](../../source/dat/species).

To create a new species, you put an appropriate YAML file in there, manually add the species enum name to `util/species-gen/species-type-header.txt `, and compile DCSS. For any custom species behaviour which the YAML file can't express (for example, Octopode's eight ring slots), you'll also need custom C++ code added elsewhere, typically in the form of new (or old) mutations.

## Species Definition Format

You can view existing files to get a sense of the format, it's quite straightforward.

### Supported Keys

| key  | type | required | description |
| ---- | ---- | -------- | ----------- |
| enum | `string` | **Yes** | Set the species enum. Must match the pattern `SP_[A-Z_]+`, and be present in `util/species-gen/species-type-header.txt `. |
| monster | `string` | **Yes** | Species' corresponding monster. Must match the pattern `MONS_[A-Z_]+`. |
| name | `string` | **Yes** | Species name, like 'Deep Dwarf' or 'Felid'. Required. Must be unique. |
| short_name | `string` | No | Two letter species code. Must be unique (excepting special cases like Draconian sub-species). Defaults to the first two letters of the name. |
| adjective | `string` | No | Species adjective, like 'Dwarven' or 'Feline'. Defaults to `$name`. |
| genus | `string` | No | Species genus, like 'Dwarf' or 'Cat'. Defaults to `$name`.
| difficulty | `string` | **Yes** | Which column the species is located in on the new game species select screen. Can be `Simple`, `Intermediate`, `Advanced`, or `false` (will not be shown on species select screen). |
| difficulty_priority | `int` | No | The priority of the species in the new game species select screen. Higher priorities are displayed first. Defaults to 0. |
| species_flags | `array of strings` | No | Set a few species flags. Defaults to none. The available flags are: <!-- raw html is the only way to embed multi-line content in a markdown table eek --><ul><li>**draconian** - Marks the species as part of the draconian family.</li><li>**no_hair** - Species lacks hair (ensures they don't get hair-raising messages).</li><li>**no bones** - Species has no bones.</li></ul><li>**small_torso** - Species' torso is a size smaller than their species size (eg Centaur, Naga). Allows for example large species to use medium armour.</li><li>**barding** - Species uses a barding instead of boots.</li><li>**no_feet** - Species lacks feet (ensures they don't get foot-related messages).</li><li>**no_blood** - Species lacks blood (ensures they don't get blood-related messages).</li><li>**no_eyes** - Species lacks blood (ensures they don't get eye-related messages). They also can't be blinded.</li><li>**no_ears** - Species lacks ears (ensures they don't get ear-related messages). |
| aptitudes | `map` | **Yes** | The available keys are:<ul><li>**xp**: `(int)` xp aptitude for the species. Required.</li><li>**hp**: `(int)` hp aptitude for the species. Required.</li><li>**mp_mod**: `(int)` starting mp modifier for the species. Required.</li><li>**wl**: `(int)` Willpower gained per XL. Required.</li><li>All other species aptitudes can be specified here. For example: `fighting: 1`. If not specified, the aptitude defaults to 0. For skills the species cannot train, use `false`.</li></ul>
| can_swim | `boolean` | No | If true, the monster can move in deep water. Defaults to false. |
| undead | `str` | No | Undead type. Controls many things about how the species works, but most notably base resists and hunger. Defaults to alive. The available settings are:<ul><li>**alive** - Not undead</li><li>**undead** - Ghouls, Mummies</li><li>**semi_undead** - Vampires</li></ul> |
| size | `str` | No | Species size. The available sizes are: tiny, little, small, medium, large, big, giant. Defaults to medium. |
| str | `int` | **Yes** | Starting strength. Note: starting stats are modified by the background player chooses. |
| int | `int` | **Yes** | Starting intelligence. |
| dex | `int` | **Yes** | Starting dexterity. |
| levelup_stat_frequency | `int` | **Yes** | How frequently (in XLs) the species' periodic stat-up occurs. Set to `28` if the species doesn't gain species stat-ups on level gain. Note that all species gain a user-selectable stat every three levels independently of this value, unless hard-coded otherwise. |
| levelup_stats | `array of strings` or `str` | No | Which stats the species' periodic stat-up can randomly select from, every `levelup_stat_frequency` levels. Defaults (on str `default`) to `[str, dex, int]`. Set to `[]` if the species doesn't gain species stat-ups on level gain. |
| mutations | `map {xl: {mut: amt}}` | No | Innate mutations for the species, and the XL it gains them. The map is keyed by XL, then each XL is another map containing the mutation enum name keying the number of levels to change. Look at Deep Dwarf for a good example. |
| fake_mutations | `array of maps [{'long': str, 'short': str}, ...]` | No | A list of fake mutations the species starts with. Fake mutations do nothing except show up in the mutations list, and are used to communicate species-specific special behaviour. Each fake mutation has a short and long description, for the `%` and `A` screens respectively. Look at Barachi for a good example. Note that for a particular fake mutation, either the long or short description can be omitted (see White Draconian). |
| recommended_jobs | `array of strings` | **Sometimes** | Backgrounds the species is recommended for. Backgrounds are written in plaintext, e.g. `alchemist` or `fire elementalist`. For species with `difficulty` set to `false`, this parameter is irrelevant and therefore optional; otherwise it is required. |
| recommended_weapons | `array of strings` | No | Weapons, identified by skill, recommended for the species. Entries must match a known weapon skill, in plaintext (`long blades`, etc). Defaults to all weapons except short blades and unarmed combat. |
| walking_verb | `string` | No | What "verb" should be used to describe the species' movement style? The "verb" will have "er" or "ing" appended when used. Examples: "Slid" (Naga), "Glid" (Tengu). Defaults to "Walk". |
| altar_action | `string` | No | When praying at an altar, print `You $altar_action the altar of foo.` for the species. Defaults to "kneel at". |
| child_name | `string` | No | Species child name, like 'Pup' or 'Kitten'. Defaults to 'Child'. |
| orc_name | `string` | No | Species name if orcish from worshipping Beogh. Defaults to 'Orc'. |

### Advanced Keys

You probably don't need or want to use these.

| key  | type | required | description |
| ---- | ---- | -------- | ----------- |
| TAG_MAJOR_VERSION | `int` | No | Wrap the species in `#if TAG_MAJOR_VERSION == [...]` to deprecate it. See existing deprecated species for examples. |
| create_enum | `boolean` | No | Set `false` if the species has a hardcoded enum in species-type.h. New species should use the default value. Defaults to `false`. |
