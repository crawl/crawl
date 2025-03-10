#include "AppHdr.h"

#ifdef USE_TILE

#include "tiledoll.h"

#include <sys/stat.h>

#include "artefact.h"
#include "files.h"
#include "jobs.h"
#include "makeitem.h"
#include "mon-info.h"
#include "newgame.h"
#include "ng-setup.h"
#include "options.h"
#include "syscalls.h"
#include "tile-player-flag-cut.h"
#ifdef USE_TILE_LOCAL
 #include "tilebuf.h"
#endif
#include "rltiles/tiledef-player.h"
#include "tag-version.h"
#include "tilemcache.h"
#include "tilepick.h"
#include "tilepick-p.h"
#include "transform.h"
#include "unwind.h"

dolls_data::dolls_data()
{
    parts = new tileidx_t[TILEP_PART_MAX];
    memset(parts, 0, TILEP_PART_MAX * sizeof(tileidx_t));
}

dolls_data::dolls_data(const dolls_data& _orig)
{
    parts = new tileidx_t[TILEP_PART_MAX];
    memcpy(parts, _orig.parts, TILEP_PART_MAX * sizeof(tileidx_t));
}

const dolls_data& dolls_data::operator=(const dolls_data& other)
{
    if (this != &other)
        memcpy(parts, other.parts, TILEP_PART_MAX * sizeof(tileidx_t));
    return *this;
}

dolls_data::~dolls_data()
{
    delete[] parts;
    parts = nullptr;
}

bool dolls_data::operator==(const dolls_data& other) const
{
    for (unsigned int i = 0; i < TILEP_PART_MAX; i++)
        if (parts[i] != other.parts[i]) return false;
    return true;
}

dolls_data player_doll;

bool save_doll_data(int mode, int num, const dolls_data* dolls)
{
    // Save mode, num, and all dolls into dolls.txt.
    string dollsTxtString = datafile_path("dolls.txt", false, true);

    struct stat stFileInfo;
    stat(dollsTxtString.c_str(), &stFileInfo);

    // Write into the current directory instead if we didn't find the file
    // or don't have write permissions.
    const char *dollsTxt = (dollsTxtString.c_str()[0] == 0
                              || !(stFileInfo.st_mode & S_IWUSR)) ? "dolls.txt"
                            : dollsTxtString.c_str();

    FILE *fp = nullptr;
    if ((fp = fopen_u(dollsTxt, "w+")) != nullptr)
    {
        fprintf(fp, "MODE=%s\n",
                    (mode == TILEP_MODE_EQUIP)   ? "EQUIP" :
                    (mode == TILEP_MODE_LOADING) ? "LOADING"
                                                 : "DEFAULT");

        fprintf(fp, "NUM=%02d\n", num == -1 ? 0 : num);

        // Print some explanatory comments. May contain no spaces!
        fprintf(fp, "#Legend:\n");
        fprintf(fp, "#***:equipment/123:index/000:none\n");
        fprintf(fp, "#Shadow/Base/Cloak/Boots/Legs/Body/Gloves/Weapon/Shield/"
                    "Hair/Beard/Helmet/Halo/Enchant/DrcWing\n");
        fprintf(fp, "#Sh:Bse:Clk:Bts:Leg:Bdy:Glv:Wpn:Shd:Hai:Brd:Hlm:Hal:Enc:Drc:Wng\n");
        char fbuf[80];
        for (unsigned int i = 0; i < NUM_MAX_DOLLS; ++i)
        {
            tilep_print_parts(fbuf, dolls[i]);
            fprintf(fp, "%s", fbuf);
        }
        fclose(fp);

        return true;
    }

    return false;
}

bool load_doll_data(const char *fn, dolls_data *dolls, int max,
                    tile_doll_mode *mode, int *cur)
{
    char fbuf[1024];
    FILE *fp  = nullptr;

    string dollsTxtString = datafile_path(fn, false, true);

    struct stat stFileInfo;
    stat(dollsTxtString.c_str(), &stFileInfo);

    // Try to read from the current directory instead if we didn't find the
    // file or don't have reading permissions.
    const char *dollsTxt = (dollsTxtString.c_str()[0] == 0
                              || !(stFileInfo.st_mode & S_IRUSR)) ? "dolls.txt"
                            : dollsTxtString.c_str();

    if ((fp = fopen_u(dollsTxt, "r")) == nullptr)
    {
        // File doesn't exist. By default, use equipment settings.
        *mode = TILEP_MODE_EQUIP;
        return false;
    }
    else
    {
        memset(fbuf, 0, sizeof(fbuf));
        // Read mode from file.
        if (fscanf(fp, "%1023s", fbuf) != EOF)
        {
            if (strcmp(fbuf, "MODE=DEFAULT") == 0)
                *mode = TILEP_MODE_DEFAULT;
            else if (strcmp(fbuf, "MODE=EQUIP") == 0)
                *mode = TILEP_MODE_EQUIP; // Nothing else to be done.
        }
        // Read current doll from file.
        if (fscanf(fp, "%1023s", fbuf) != EOF)
        {
            if (strncmp(fbuf, "NUM=", 4) == 0)
            {
                sscanf(fbuf, "NUM=%d", cur);
                if (*cur < 0 || *cur >= NUM_MAX_DOLLS)
                    *cur = 0;
            }
        }

        if (max == 1)
        {
            // Load only one doll, either the one defined by NUM or
            // use the default/equipment setting.
            if (*mode != TILEP_MODE_LOADING)
            {
                if (*mode == TILEP_MODE_DEFAULT)
                    tilep_job_default(you.char_class, &dolls[0]);

                // If we don't need to load a doll, return now.
                fclose(fp);
                return true;
            }

            int count = 0;
            while (fscanf(fp, "%1023s", fbuf) != EOF)
            {
                if (fbuf[0] == '#') // Skip comment lines.
                    continue;

                if (*cur == count++)
                {
                    tilep_scan_parts(fbuf, dolls[0], you.species,
                                     you.experience_level);
                    break;
                }
            }
        }
        else // Load up to max dolls from file.
        {
            for (int count = 0; count < max && fscanf(fp, "%1023s", fbuf) != EOF;)
            {
                if (fbuf[0] == '#') // Skip comment lines.
                    continue;

                tilep_scan_parts(fbuf, dolls[count++], you.species,
                                 you.experience_level);
            }
        }

        fclose(fp);
        return true;
    }
}

void init_player_doll()
{
    dolls_data dolls[NUM_MAX_DOLLS];

    for (int i = 0; i < NUM_MAX_DOLLS; i++)
        for (int j = 0; j < TILEP_PART_MAX; j++)
            dolls[i].parts[j] = TILEP_SHOW_EQUIP;

    tile_doll_mode mode = TILEP_MODE_LOADING;
    int cur = 0;
    load_doll_data("dolls.txt", dolls, NUM_MAX_DOLLS, &mode, &cur);

    if (mode == TILEP_MODE_LOADING)
    {
        player_doll = dolls[cur];
        tilep_race_default(you.species, you.experience_level, &player_doll);
        return;
    }

    for (int i = 0; i < TILEP_PART_MAX; i++)
        player_doll.parts[i] = TILEP_SHOW_EQUIP;
    tilep_race_default(you.species, you.experience_level, &player_doll);

    if (mode == TILEP_MODE_EQUIP)
        return;

    tilep_job_default(you.char_class, &player_doll);
}

static int _get_random_doll_part(int p)
{
    ASSERT_RANGE(p, 0, TILEP_PART_MAX + 1);
    return tile_player_part_start[p]
           + random2(tile_player_part_count[p]);
}

static void _fill_doll_part(dolls_data &doll, int p)
{
    ASSERT_RANGE(p, 0, TILEP_PART_MAX + 1);
    doll.parts[p] = _get_random_doll_part(p);
}

void create_random_doll(dolls_data &rdoll)
{
    // All dolls roll for these.
    _fill_doll_part(rdoll, TILEP_PART_BODY);
    _fill_doll_part(rdoll, TILEP_PART_HAND1);
    _fill_doll_part(rdoll, TILEP_PART_LEG);
    _fill_doll_part(rdoll, TILEP_PART_BOOTS);
    _fill_doll_part(rdoll, TILEP_PART_HAIR);

    // The following are only rolled with 50% chance.
    if (coinflip())
        _fill_doll_part(rdoll, TILEP_PART_CLOAK);
    if (coinflip())
        _fill_doll_part(rdoll, TILEP_PART_ARM);
    if (coinflip())
        _fill_doll_part(rdoll, TILEP_PART_HAND2);
    if (coinflip())
        _fill_doll_part(rdoll, TILEP_PART_HELM);

    if (one_chance_in(4))
        _fill_doll_part(rdoll, TILEP_PART_BEARD);
}

// Deterministically pick a pair of trousers for this character to use
// for SHOW_EQUIP, as there's no corresponding item slot for this.
static tileidx_t _random_trousers()
{
    int offset = static_cast<int>(you.species) * 9887
                 + static_cast<int>(you.char_class) * 8719;
    const char *name = you.your_name.c_str();
    for (int i = 0; i < 8 && *name; ++i, ++name)
        offset += *name * 4643;

    const int range = TILEP_LEG_LAST_NORM - TILEP_LEG_FIRST_NORM + 1;
    return TILEP_LEG_FIRST_NORM + offset % range;
}

void fill_doll_equipment(dolls_data &result)
{
    // Equipment-using forms
    switch (you.form)
    {
    case transformation::tree:
        result.parts[TILEP_PART_BASE]    = TILEP_TRAN_TREE;
        result.parts[TILEP_PART_HELM]    = 0; // fixme, should show up
        result.parts[TILEP_PART_DRCWING] = 0;
        result.parts[TILEP_PART_HAIR]    = 0;
        result.parts[TILEP_PART_BEARD]   = 0;
        result.parts[TILEP_PART_LEG]     = 0;
        result.parts[TILEP_PART_SHADOW]  = 0;
        break;
    case transformation::beast:
        switch (you.species)
        {
        case SP_ARMATAUR:
        case SP_NAGA:
        case SP_FELID:
        case SP_OCTOPODE:
        case SP_DJINNI:  break;
        default:
            result.parts[TILEP_PART_BASE] = TILEP_TRAN_BEAST;
            result.parts[TILEP_PART_LEG]     = 0;
            break;
        }
        break;
    case transformation::statue:
        tileidx_t ch;
        switch (you.species)
        {
#if TAG_MAJOR_VERSION == 34
        case SP_CENTAUR:
#endif
        case SP_ARMATAUR: ch = TILEP_TRAN_STATUE_ARMATAUR;  break;
        case SP_NAGA:    ch = TILEP_TRAN_STATUE_NAGA;     break;
        case SP_FELID:   ch = TILEP_TRAN_STATUE_FELID;    break;
        case SP_OCTOPODE:ch = TILEP_TRAN_STATUE_OCTOPODE; break;
        case SP_DJINNI:  ch = TILEP_TRAN_STATUE_DJINN;    break;
        default:         ch = TILEP_TRAN_STATUE_HUMANOID; break;
        }
        result.parts[TILEP_PART_BASE]    = ch;
        result.parts[TILEP_PART_HAIR]    = 0;
        result.parts[TILEP_PART_LEG]     = 0;
        break;
    case transformation::storm:
        switch (you.species)
        {
        case SP_FELID:    ch = TILEP_TRAN_STORM_FELID;       break;
        case SP_OCTOPODE: ch = TILEP_TRAN_STORM_OCTOPODE;    break;
        default:          ch = TILEP_TRAN_STORM_HUMANOID;    break;
        }
        result.parts[TILEP_PART_BASE]    = ch;
        result.parts[TILEP_PART_HELM]    = 0;
        result.parts[TILEP_PART_DRCWING] = 0;
        result.parts[TILEP_PART_HAIR]    = 0;
        result.parts[TILEP_PART_BEARD]   = 0;
        result.parts[TILEP_PART_LEG]     = 0;
        result.parts[TILEP_PART_BOOTS]   = 0;
        result.parts[TILEP_PART_BODY]    = 0;
        result.parts[TILEP_PART_ARM]     = 0;
        result.parts[TILEP_PART_CLOAK]   = 0;
        result.parts[TILEP_PART_SHADOW]  = 0;
        break;
    case transformation::death:
        switch (you.species)
        {
#if TAG_MAJOR_VERSION == 34
        case SP_CENTAUR:
#endif
        case SP_ARMATAUR: ch = TILEP_TRAN_LICH_ARMATAUR;  break;
        case SP_NAGA:    ch = TILEP_TRAN_LICH_NAGA;     break;
        case SP_FELID:   ch = TILEP_TRAN_LICH_FELID;    break;
        case SP_OCTOPODE:ch = TILEP_TRAN_LICH_OCTOPODE; break;
        case SP_DJINNI:  ch = TILEP_TRAN_LICH_DJINN;    break;
        default:         ch = TILEP_TRAN_LICH_HUMANOID; break;
        }
        result.parts[TILEP_PART_BASE]    = ch;
        result.parts[TILEP_PART_HAIR]    = 0;
        result.parts[TILEP_PART_BEARD]   = 0;
        result.parts[TILEP_PART_LEG]     = 0;

        // fixme: these should show up, but look ugly with the lich tile
        result.parts[TILEP_PART_HELM]    = 0;
        result.parts[TILEP_PART_BOOTS]   = 0;
        result.parts[TILEP_PART_BODY]    = 0;
        result.parts[TILEP_PART_ARM]     = 0;
        result.parts[TILEP_PART_CLOAK]   = 0;
        break;
    case transformation::vampire:
        switch (you.species)
        {
#if TAG_MAJOR_VERSION == 34
        case SP_CENTAUR:
#endif
        case SP_ARMATAUR:   ch = TILEP_TRAN_VAMPIRE_ARMATAUR;   break;
        case SP_NAGA:       ch = TILEP_TRAN_VAMPIRE_NAGA;       break;
        case SP_FELID:      ch = TILEP_TRAN_VAMPIRE_FELID;      break;
        case SP_OCTOPODE:   ch = TILEP_TRAN_VAMPIRE_OCTOPODE;   break;
        case SP_DJINNI:     ch = TILEP_TRAN_VAMPIRE_DJINN;      break;
        case SP_TENGU:      ch = TILEP_TRAN_VAMPIRE_TENGU;      break;
        default:            ch = TILEP_TRAN_VAMPIRE;            break;
        }
        result.parts[TILEP_PART_BASE]    = ch;
        result.parts[TILEP_PART_HAIR]    = 0;
        result.parts[TILEP_PART_BEARD]   = 0;
        result.parts[TILEP_PART_LEG]     = 0;
        // Not melded, but don't fit the base tile
        result.parts[TILEP_PART_HELM]    = 0;
        result.parts[TILEP_PART_BOOTS]   = 0;
        result.parts[TILEP_PART_BODY]    = 0;
        result.parts[TILEP_PART_ARM]     = 0;
        result.parts[TILEP_PART_CLOAK]   = 0;
        break;
    default:
        // A monster tile is being used for the player.
        if (player_uses_monster_tile())
        {
            result.parts[TILEP_PART_BASE]    = tileidx_player_mons();
            result.parts[TILEP_PART_HAIR]    = 0;
            result.parts[TILEP_PART_BEARD]   = 0;
            result.parts[TILEP_PART_LEG]     = 0;
            result.parts[TILEP_PART_HELM]    = 0;
            result.parts[TILEP_PART_BOOTS]   = 0;
            result.parts[TILEP_PART_BODY]    = 0;
            result.parts[TILEP_PART_ARM]     = 0;
            result.parts[TILEP_PART_CLOAK]   = 0;
        }
        break;
    }

    // Base tile.
    if (result.parts[TILEP_PART_BASE] == TILEP_SHOW_EQUIP)
        tilep_race_default(you.species, you.experience_level, &result);

    // Main hand.
    if (result.parts[TILEP_PART_HAND1] == TILEP_SHOW_EQUIP)
    {
        if (you.form == transformation::blade_hands)
        {
            const bool natasha = Options.tile_use_monster == MONS_NATASHA;
            const auto base_tile = result.parts[TILEP_PART_BASE];
            if (natasha
                || is_player_tile(base_tile, TILEP_BASE_FELID)
                || is_player_tile(base_tile, TILEP_BASE_FELID_SILLY))
            {
                // TODO: add variant blade hands for alt felid tiles
                if (natasha || base_tile == TILEP_BASE_FELID)
                    result.parts[TILEP_PART_HAND1] = TILEP_HAND1_BLADEHAND_FE;
            }
            else if (is_player_tile(base_tile, TILEP_BASE_OCTOPODE))
                result.parts[TILEP_PART_HAND1] = TILEP_HAND1_BLADEHAND_OP;
            else
                result.parts[TILEP_PART_HAND1] = TILEP_HAND1_BLADEHAND;
        }
        else
        {
            item_def* wpn = you.weapon();
            if (wpn)
                result.parts[TILEP_PART_HAND1] = tilep_equ_weapon(*wpn);
            else
                result.parts[TILEP_PART_HAND1] = 0;
        }
    }
    // Off hand.
    if (result.parts[TILEP_PART_HAND2] == TILEP_SHOW_EQUIP)
    {
        if (you.form == transformation::blade_hands)
        {
            if (is_player_tile(result.parts[TILEP_PART_BASE], TILEP_BASE_OCTOPODE))
                result.parts[TILEP_PART_HAND2] = TILEP_HAND1_BLADEHAND_OP;
            else if (is_player_tile(result.parts[TILEP_PART_BASE], TILEP_BASE_FELID)
                     || Options.tile_use_monster == MONS_NATASHA)
                result.parts[TILEP_PART_HAND2] = TILEP_HAND1_BLADEHAND_FE;
            else result.parts[TILEP_PART_HAND2] = TILEP_HAND1_BLADEHAND;
        }
        else
        {
            if (item_def* wpn = you.offhand_weapon())
                result.parts[TILEP_PART_HAND2] = mirror_weapon(*wpn);
            else if (item_def* item = you.equipment.get_first_slot_item(SLOT_OFFHAND))
                result.parts[TILEP_PART_HAND2] = tilep_equ_shield(*item);
            else
                result.parts[TILEP_PART_HAND2] = 0;
        }
    }
    // Body armour.
    if (result.parts[TILEP_PART_BODY] == TILEP_SHOW_EQUIP)
    {
        if (you.form == transformation::maw
                 // non-body-armour wearing species would need this tile to go elswhere
                 // except for draconians
                 && (species::is_draconian(you.species)
                     || you_can_wear(SLOT_BODY_ARMOUR, false) != false))
        {
            const auto maw = TILEP_BODY_MAW_FORM;
            const int count = tile_player_count(maw);
            result.parts[TILEP_PART_BODY] = maw + you.frame_no % count;
        }
        else if (item_def* armour = you.body_armour())
            result.parts[TILEP_PART_BODY] = tilep_equ_armour(*armour);
        else
            result.parts[TILEP_PART_BODY] = 0;
    }
    // Helmet.
    if (result.parts[TILEP_PART_HELM] == TILEP_SHOW_EQUIP)
    {
        if (item_def* helm = you.equipment.get_first_slot_item(SLOT_HELMET))
            result.parts[TILEP_PART_HELM] = tilep_equ_helm(*helm);
        else if (you.get_mutation_level(MUT_HORNS) > 0)
        {
            if (you.species == SP_FELID)
            {
                if (is_player_tile(result.parts[TILEP_PART_BASE],
                                  TILEP_BASE_FELID))
                {
                    // Felid horns are offset by the tile variant.
                    result.parts[TILEP_PART_HELM] = TILEP_HELM_HORNS_CAT
                        + result.parts[TILEP_PART_BASE] - TILEP_BASE_FELID;
                }
                else if (is_player_tile(result.parts[TILEP_PART_BASE],
                                        TILEP_BASE_FELID_SILLY))
                {
                    result.parts[TILEP_PART_HELM] = TILEP_HELM_HORNS_CAT_SILLY
                        + result.parts[TILEP_PART_BASE] - TILEP_BASE_FELID_SILLY;
                }
                else if (is_player_tile(result.parts[TILEP_PART_BASE],
                                  TILEP_TRAN_STATUE_FELID))
                {
                    result.parts[TILEP_PART_HELM] = TILEP_HELM_HORNS_CAT_STATUE;
                }
            }
            else if (species::is_draconian(you.species))
                result.parts[TILEP_PART_HELM] = TILEP_HELM_HORNS_DRAC;
            else
                switch (you.get_mutation_level(MUT_HORNS))
                {
                    case 1:
                        result.parts[TILEP_PART_HELM] = TILEP_HELM_HORNS1;
                        break;
                    case 2:
                        result.parts[TILEP_PART_HELM] = TILEP_HELM_HORNS2;
                        break;
                    case 3:
                        result.parts[TILEP_PART_HELM] = TILEP_HELM_HORNS3;
                        break;
                }
        }
        else
            result.parts[TILEP_PART_HELM] = 0;
    }
    // Cloak.
    if (result.parts[TILEP_PART_CLOAK] == TILEP_SHOW_EQUIP)
    {
        if (item_def* cloak = you.equipment.get_first_slot_item(SLOT_CLOAK))
        {
            result.parts[TILEP_PART_CLOAK] = tilep_equ_cloak(*cloak);

            // Gets an additional tile on the head
            if (is_unrandom_artefact(*cloak, UNRAND_FISTICLOAK))
                result.parts[TILEP_PART_HELM] = TILEP_HELM_FUNGAL_FISTICLOAK_FRONT;
        }
        else
            result.parts[TILEP_PART_CLOAK] = 0;
    }
    // Leg.
    if (result.parts[TILEP_PART_LEG] == TILEP_SHOW_EQUIP
        && you.species != SP_REVENANT)
    {
        result.parts[TILEP_PART_LEG] = _random_trousers();
    }

    // Boots.
    if (result.parts[TILEP_PART_BOOTS] == TILEP_SHOW_EQUIP)
    {
        if (item_def* boots = you.equipment.get_first_slot_item(SLOT_BOOTS))
            result.parts[TILEP_PART_BOOTS] = tilep_equ_boots(*boots);
        else if (item_def* barding = you.equipment.get_first_slot_item(SLOT_BARDING))
            result.parts[TILEP_PART_BOOTS] = tilep_equ_boots(*barding);
        else
            result.parts[TILEP_PART_BOOTS] = 0;
    }
    // Gloves.
    if (result.parts[TILEP_PART_ARM] == TILEP_SHOW_EQUIP)
    {
        if (item_def* gloves = you.equipment.get_first_slot_item(SLOT_GLOVES))
            result.parts[TILEP_PART_ARM] = tilep_equ_gloves(*gloves);
        else if (you.has_mutation(MUT_TENTACLE_SPIKE))
            result.parts[TILEP_PART_ARM] = TILEP_ARM_OCTOPODE_SPIKE;
        else if (you.has_claws(false) >= 3)
            result.parts[TILEP_PART_ARM] = TILEP_ARM_CLAWS;
        else
            result.parts[TILEP_PART_ARM] = 0;
    }
    // Halo.
    if (result.parts[TILEP_PART_HALO] == TILEP_SHOW_EQUIP)
    {
        const bool halo = you.haloed();
        result.parts[TILEP_PART_HALO] = halo ? TILEP_HALO_TSO : 0;
    }
    // Enchantments.
    if (result.parts[TILEP_PART_ENCH] == TILEP_SHOW_EQUIP)
    {
        result.parts[TILEP_PART_ENCH] =
            (you.duration[DUR_STICKY_FLAME] ? TILEP_ENCH_STICKY_FLAME : 0);
    }
    // Draconian head/wings.
    if (species::is_draconian(you.species))
    {
        tileidx_t base = 0;
        tileidx_t wing = 0;
        tilep_draconian_init(you.species, you.experience_level, &base, &wing);

        if (result.parts[TILEP_PART_DRCWING] == TILEP_SHOW_EQUIP)
            result.parts[TILEP_PART_DRCWING] = wing;
    }
    // Shadow.
    if (result.parts[TILEP_PART_SHADOW] == TILEP_SHOW_EQUIP)
        result.parts[TILEP_PART_SHADOW] = TILEP_SHADOW_SHADOW;

    // Various other slots.
    for (int i = 0; i < TILEP_PART_MAX; i++)
        if (result.parts[i] == TILEP_SHOW_EQUIP)
            result.parts[i] = 0;
}

void fill_doll_for_newgame(dolls_data &result, const newgame_def& ng)
{
    for (int j = 0; j < TILEP_PART_MAX; j++)
        result.parts[j] = TILEP_SHOW_EQUIP;

    unwind_var<player> unwind_you(you);
    you = player();

    // The following is part of the new game setup code from _setup_generic()
    you.your_name  = ng.name;
    you.species    = ng.species;
    you.char_class = ng.job;
    you.chr_class_name = get_job_name(you.char_class);

    species_stat_init(you.species);
    update_vision_range();
    job_stat_init(you.char_class);
    give_basic_mutations(you.species);
    give_items_skills(ng);

    for (int i = 0; i < ENDOFPACK; ++i)
    {
        auto &item = you.inv[i];
        if (item.defined())
            item_colour(item);
    }

    fill_doll_equipment(result);
}

// Writes equipment information into per-character doll file.
void save_doll_file(writer &dollf)
{
    dolls_data result = player_doll;
    fill_doll_equipment(result);

    // Write into file.
    char fbuf[80];
    tilep_print_parts(fbuf, result);
    dollf.write(fbuf, strlen(fbuf));
}

void reveal_bardings(const tileidx_t *parts, int (&flags)[TILEP_PART_MAX])
{
    const tileidx_t base = parts[TILEP_PART_BASE];
    if (is_player_tile(base, TILEP_BASE_NAGA)
        || is_player_tile(base, TILEP_BASE_ARMATAUR))
    {
        flags[TILEP_PART_BOOTS] = TILEP_FLAG_NORMAL;
    }
}

#ifdef USE_TILE_LOCAL
void pack_doll_buf(SubmergedTileBuffer& buf, const dolls_data &doll,
                   int x, int y, bool submerged, bool ghost)
{
    int p_order[TILEP_PART_MAX];
    int flags[TILEP_PART_MAX];
    tilep_fill_order_and_flags(doll, p_order, flags);

    // Set up mcache data based on equipment. We don't need this lookup if both
    // pairs of offsets are defined in Options.
    int draw_info_count = 0, dind = 0;
    mcache_entry *entry = nullptr;
    tile_draw_info dinfo[mcache_entry::MAX_INFO_COUNT];
    if (player_uses_monster_tile())
    {
        monster_info minfo(MONS_PLAYER, MONS_PLAYER);
        minfo.props[MONSTER_TILE_KEY] = int(doll.parts[TILEP_PART_BASE]);
        item_def *item;
        if (const item_def* eq = you.equipment.get_first_slot_item(SLOT_WEAPON))
        {
            item = new item_def(*eq);
            minfo.inv[MSLOT_WEAPON].reset(item);
        }
        if (const item_def* eq = you.equipment.get_first_slot_item(SLOT_OFFHAND))
        {
            item = new item_def(*eq);
            minfo.inv[MSLOT_SHIELD].reset(item);
        }
        tileidx_t mcache_idx = mcache.register_monster(minfo);
        if (mcache_idx)
        {
            entry = mcache.get(mcache_idx);
            draw_info_count = entry->info(&dinfo[0]);
        }
    }

    // A higher index here means that the part should be drawn on top.
    // This is drawn in reverse order because this could be a ghost
    // or being drawn in water, in which case we want the top-most part
    // to blend with the background underneath and not with the parts
    // underneath. Parts drawn afterwards will not obscure parts drawn
    // previously, because "i" is passed as the depth below.
    for (int i = TILEP_PART_MAX - 1; i >= 0; --i)
    {
        int p = p_order[i];

        if (!doll.parts[p] || flags[p] == TILEP_FLAG_HIDE)
            continue;

        if (p == TILEP_PART_SHADOW && (submerged || ghost))
            continue;

        int ofs_x = 0, ofs_y = 0;
        if ((p == TILEP_PART_HAND1 && you.equipment.get_first_slot_item(SLOT_WEAPON)
             || p == TILEP_PART_HAND2 && you.equipment.get_first_slot_item(SLOT_OFFHAND))
            && dind < draw_info_count - 1)
        {
            ofs_x = dinfo[draw_info_count - dind - 1].ofs_x;
            ofs_y = dinfo[draw_info_count - dind - 1].ofs_y;
            ++dind;
        }

        const int ymax = flags[p] == TILEP_FLAG_CUT_BOTTOM ? 18 : TILE_Y;
        buf.add(doll.parts[p], x, y, i, submerged, ghost, ofs_x, ofs_y, ymax);
    }
}
#endif

// Pack the appropriate tiles to represent a given paperdoll, in the right order
void pack_tilep_set(vector<tile_def>& tileset, const dolls_data &doll)
{
    int p_order[TILEP_PART_MAX];
    int flags[TILEP_PART_MAX];
    tilep_fill_order_and_flags(doll, p_order, flags);

    for (int i = 0; i < TILEP_PART_MAX; ++i)
    {
        const int p   = p_order[i];
        const tileidx_t idx = doll.parts[p];
        if (idx == 0 || idx == TILEP_SHOW_EQUIP || flags[p] == TILEP_FLAG_HIDE)
            continue;

        ASSERT_RANGE(idx, TILE_MAIN_MAX, TILEP_PLAYER_MAX);

        const int ymax = flags[p] == TILEP_FLAG_CUT_BOTTOM ? 18 : TILE_Y;
        tileset.emplace_back(idx, ymax);
    }
}

#endif
