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

static void _fill_doll_equipment_all_melded(dolls_data& result)
{
    // The only parts shown are TILEP_PART_BASE, TILEP_PART_HALO, and
    // TILEP_PART_ENCH
    result.parts[TILEP_PART_SHADOW] = 0;
    result.parts[TILEP_PART_CLOAK] = 0;
    result.parts[TILEP_PART_BOOTS] = 0;
    result.parts[TILEP_PART_LEG] = 0;
    result.parts[TILEP_PART_BODY] = 0;
    result.parts[TILEP_PART_ARM] = 0;
    result.parts[TILEP_PART_HAND1] = 0;
    result.parts[TILEP_PART_HAND1_MIRROR] = 0;
    result.parts[TILEP_PART_HAND2] = 0;
    result.parts[TILEP_PART_HAIR] = 0;
    result.parts[TILEP_PART_BEARD] = 0;
    result.parts[TILEP_PART_HELM] = 0;
    result.parts[TILEP_PART_DRCWING] = 0;
}

static void _fill_doll_equipment_default(dolls_data& result)
{
    // A monster tile is being used for the player.
    if (player_uses_monster_tile())
    {
        result.parts[TILEP_PART_BASE] = tileidx_player_mons();
        result.parts[TILEP_PART_HAIR] = 0;
        result.parts[TILEP_PART_BEARD] = 0;
        result.parts[TILEP_PART_LEG] = 0;
        result.parts[TILEP_PART_HELM] = 0;
        result.parts[TILEP_PART_BOOTS] = 0;
        result.parts[TILEP_PART_BODY] = 0;
        result.parts[TILEP_PART_ARM] = 0;
        result.parts[TILEP_PART_CLOAK] = 0;
    }
}

void fill_doll_equipment(dolls_data &result)
{
    tileidx_t ch;

    switch (you.form)
    {
    case transformation::spider:
        if (you.species == SP_GARGOYLE)
            ch = TILEP_TRAN_SPIDER_GARGOYLE;
        else
            ch = TILEP_TRAN_SPIDER;
        result.parts[TILEP_PART_BASE] = ch;
        _fill_doll_equipment_all_melded(result);
        break;
    case transformation::statue:
        switch (you.species)
        {
#if TAG_MAJOR_VERSION == 34
        case SP_CENTAUR:
#endif
        case SP_ANEMOCENTAUR: ch = TILEP_TRAN_STATUE_ANEMOCENTAUR;  break;
        case SP_NAGA:         ch = TILEP_TRAN_STATUE_NAGA;          break;
        case SP_FELID:        ch = TILEP_TRAN_STATUE_FELID;         break;
        case SP_OCTOPODE:     ch = TILEP_TRAN_STATUE_OCTOPODE;      break;
        case SP_DJINNI:       ch = TILEP_TRAN_STATUE_DJINN;         break;
        default:              ch = TILEP_TRAN_STATUE_HUMANOID;      break;
        }
        result.parts[TILEP_PART_BASE]    = ch;
        result.parts[TILEP_PART_HAIR]    = 0;
        result.parts[TILEP_PART_LEG]     = 0;
        break;
    case transformation::serpent:
        if (you.species == SP_FELID)
            ch = TILEP_TRAN_SERPENT_FELID;
        else if (you.species == SP_GARGOYLE)
            ch = TILEP_TRAN_SERPENT_GARGOYLE;
        else
            ch = TILEP_TRAN_SERPENT;
        result.parts[TILEP_PART_BASE] = ch;
        _fill_doll_equipment_all_melded(result);
        break;
    case transformation::dragon:
    {
        switch (you.species)
        {
        case SP_OCTOPODE:          ch = TILEP_TRAN_DRAGON_OCTOPODE; break;
        case SP_FELID:             ch = TILEP_TRAN_DRAGON_FELID;    break;
        case SP_GARGOYLE:          ch = TILEP_TRAN_DRAGON_GARGOYLE; break;
        case SP_BLACK_DRACONIAN:   ch = TILEP_TRAN_DRAGON_BLACK;    break;
        case SP_YELLOW_DRACONIAN:  ch = TILEP_TRAN_DRAGON_YELLOW;   break;
        case SP_GREY_DRACONIAN:    ch = TILEP_TRAN_DRAGON_GREY;     break;
        case SP_GREEN_DRACONIAN:   ch = TILEP_TRAN_DRAGON_GREEN;    break;
        case SP_PALE_DRACONIAN:    ch = TILEP_TRAN_DRAGON_PALE;     break;
        case SP_PURPLE_DRACONIAN:  ch = TILEP_TRAN_DRAGON_PURPLE;   break;
        case SP_WHITE_DRACONIAN:   ch = TILEP_TRAN_DRAGON_WHITE;    break;
        case SP_RED_DRACONIAN:     ch = TILEP_TRAN_DRAGON_RED;      break;
        default:                   ch = TILEP_TRAN_DRAGON;          break;
        }
        result.parts[TILEP_PART_BASE] = ch;
        _fill_doll_equipment_all_melded(result);
        break;
    }
    case transformation::death:
        switch (you.species)
        {
#if TAG_MAJOR_VERSION == 34
        case SP_CENTAUR:
#endif
        case SP_ANEMOCENTAUR:  ch = TILEP_TRAN_LICH_ANEMOCENTAUR;  break;
        case SP_DJINNI:        ch = TILEP_TRAN_LICH_DJINN;     break;
        case SP_GARGOYLE:      ch = TILEP_TRAN_LICH_GARGOYLE;  break;
        case SP_NAGA:          ch = TILEP_TRAN_LICH_NAGA;      break;
        case SP_FELID:         ch = TILEP_TRAN_LICH_FELID;     break;
        case SP_OCTOPODE:      ch = TILEP_TRAN_LICH_OCTOPODE;  break;
        default:               ch = TILEP_TRAN_LICH_HUMANOID;  break;
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
    case transformation::bat:
        if (you.species == SP_GARGOYLE)
            ch = TILEP_TRAN_BAT_GARGOYLE;
        else
            ch = TILEP_TRAN_BAT;
        result.parts[TILEP_PART_BASE] = ch;
        _fill_doll_equipment_all_melded(result);
        break;
#if TAG_MAJOR_VERSION == 34
    case transformation::porcupine:
#endif
    case transformation::pig:
        if (you.species == SP_GARGOYLE)
            ch = TILEP_TRAN_PIG_GARGOYLE;
        else
            ch = TILEP_TRAN_PIG;
        result.parts[TILEP_PART_BASE] = ch;
        _fill_doll_equipment_all_melded(result);
        break;
    case transformation::tree:
        result.parts[TILEP_PART_BASE]    = TILEP_TRAN_TREE;
        result.parts[TILEP_PART_HELM]    = 0; // fixme, should show up
        result.parts[TILEP_PART_DRCWING] = 0;
        result.parts[TILEP_PART_HAIR]    = 0;
        result.parts[TILEP_PART_BEARD]   = 0;
        result.parts[TILEP_PART_LEG]     = 0;
        result.parts[TILEP_PART_SHADOW]  = 0;
        break;
    case transformation::wisp:
        result.parts[TILEP_PART_BASE] = TILEP_TRAN_WISP;
        _fill_doll_equipment_all_melded(result);
        break;
    case transformation::jelly:
        result.parts[TILEP_PART_BASE] = TILEP_TRAN_JELLY;
        _fill_doll_equipment_all_melded(result);
        break;
    case transformation::fungus:
        result.parts[TILEP_PART_BASE] = TILEP_TRAN_MUSHROOM;
        _fill_doll_equipment_all_melded(result);
        break;
    case transformation::storm:
        switch (you.species)
        {
        case SP_FELID:    ch = TILEP_TRAN_STORM_FELID;       break;
        case SP_OCTOPODE: ch = TILEP_TRAN_STORM_OCTOPODE;    break;
        default:          ch = TILEP_TRAN_STORM_HUMANOID;    break;
        }
        result.parts[TILEP_PART_BASE] = ch;
        _fill_doll_equipment_all_melded(result);
        break;
    case transformation::quill:
        result.parts[TILEP_PART_HAIR] = 0;
        if (you.species == SP_FELID)
            result.parts[TILEP_PART_BASE] = TILEP_TRAN_QUILL_FELID;
        else if (you.species == SP_OCTOPODE)
            result.parts[TILEP_PART_BASE] = TILEP_TRAN_QUILL_OCTOPODE;
        else
        {
            switch (you.species)
            {
            case SP_OCTOPODE:       ch = TILEP_TRAN_QUILL_OCTOPODE;     break;
            case SP_FELID:          ch = TILEP_TRAN_QUILL_FELID;        break;
            case SP_ANEMOCENTAUR:   ch = TILEP_BODY_QUILL_ANEMOCENTAUR; break;
            case SP_DJINNI:         ch = TILEP_BODY_QUILL_DJINN;        break;
            case SP_GARGOYLE:       ch = TILEP_BODY_QUILL_GARGOYLE;     break;
            case SP_NAGA:           ch = TILEP_BODY_QUILL_NAGA;         break;
            default:                ch = TILEP_BODY_QUILL_HUMANOID;     break;
            }
            result.parts[TILEP_PART_LEG] = 0;
            result.parts[TILEP_PART_HELM] = ch;
        }
        break;
    case transformation::maw:
        _fill_doll_equipment_default(result);
        if (result.parts[TILEP_PART_BODY] == TILEP_SHOW_EQUIP)
        {
            // non-body-armour wearing species would need this tile to go elswhere
            // except for draconians
            const tileidx_t maw = TILEP_BODY_MAW_FORM;
            const unsigned int count = tile_player_count(maw);
            result.parts[TILEP_PART_BODY] = maw + you.frame_no % count;
        }
        break;
    case transformation::flux:
        switch (you.species)
        {
        case SP_OCTOPODE:
            result.parts[TILEP_PART_BASE] = TILEP_TRAN_FLUX_OCTOPODE;
            result.parts[TILEP_PART_HAIR] = 0;
            break;
        case SP_FELID:
            result.parts[TILEP_PART_BASE] = TILEP_TRAN_FLUX_FELID;
            result.parts[TILEP_PART_HAIR] = 0;
            break;
        default:
            result.parts[TILEP_PART_BODY] = TILEP_TRAN_FLUX_HUMANOID;
            break;
        }
        break;
    case transformation::eel_hands:
        switch (you.species)
        {
        case SP_OCTOPODE:
            result.parts[TILEP_PART_HAND1] = TILEP_HAND1_EEL_FORM_OCTOPODE;
            break;
        case SP_FELID:
            result.parts[TILEP_PART_BASE] = TILEP_TRAN_EEL_FELID;
            break;
        default:
            result.parts[TILEP_PART_HAND1] = TILEP_HAND1_EEL_FORM_HUMANOID;
            break;
        }
        break;
    case transformation::blade:
        switch (you.species)
        {
        case SP_OCTOPODE:
            result.parts[TILEP_PART_BEARD] = TILEP_BEARD_BLADE_FORM_OCTOPODE;
            break;
        case SP_FELID:
            result.parts[TILEP_PART_BASE] = TILEP_TRAN_BLADE_FELID;
            break;
        default:
            result.parts[TILEP_PART_CLOAK] = TILEP_CLOAK_BLADE_FORM_HUMANOID;
            break;
        }
        break;
    case transformation::spore:
        switch (you.species)
        {
        case SP_ANEMOCENTAUR:
            result.parts[TILEP_PART_HAND2] = TILEP_HAND2_SPORE_FORM_ANEMOCENTAUR;
            break;
        case SP_DJINNI:
            result.parts[TILEP_PART_HAND2] = TILEP_HAND2_SPORE_FORM_DJINNI;
            break;
        case SP_NAGA:
            result.parts[TILEP_PART_HAND2] = TILEP_HAND2_SPORE_FORM_NAGA;
            break;
        case SP_OCTOPODE:
            result.parts[TILEP_PART_HAND2] = TILEP_HAND2_SPORE_FORM_OCTOPODE;
            break;
        case SP_FELID:
            result.parts[TILEP_PART_BASE] = TILEP_TRAN_SPORE_FELID;
            break;
        default:
            result.parts[TILEP_PART_HAND2] = TILEP_HAND2_SPORE_FORM_HUMANOID;
            break;
        }
        break;
    case transformation::slaughter:
    {
        switch (you.species)
        {
        case SP_ANEMOCENTAUR:   ch = TILEP_TRAN_SLAUGHTER_ANEMOCENTAUR; break;
        case SP_GARGOYLE:       ch = TILEP_TRAN_SLAUGHTER_GARGOYLE;     break;
        case SP_NAGA:           ch = TILEP_TRAN_SLAUGHTER_NAGA;         break;
        case SP_FELID:          ch = TILEP_TRAN_SLAUGHTER_FELID;        break;
        case SP_OCTOPODE:       ch = TILEP_TRAN_SLAUGHTER_OCTOPODE;     break;
        default:                ch = TILEP_TRAN_SLAUGHTER_HUMANOID;     break;
        }
        result.parts[TILEP_PART_BASE] = ch;
        _fill_doll_equipment_all_melded(result);
        break;
    }
    case transformation::vampire:
        switch (you.species)
        {
#if TAG_MAJOR_VERSION == 34
        case SP_CENTAUR:
#endif
        case SP_ANEMOCENTAUR:   ch = TILEP_TRAN_VAMPIRE_ANEMOCENTAUR;   break;
        case SP_DJINNI:         ch = TILEP_TRAN_VAMPIRE_DJINN;          break;
        case SP_GARGOYLE:       ch = TILEP_TRAN_VAMPIRE_GARGOYLE;       break;
        case SP_NAGA:           ch = TILEP_TRAN_VAMPIRE_NAGA;           break;
        case SP_TENGU:          ch = TILEP_TRAN_VAMPIRE_TENGU;          break;
        case SP_FELID:          ch = TILEP_TRAN_VAMPIRE_FELID;          break;
        case SP_OCTOPODE:       ch = TILEP_TRAN_VAMPIRE_OCTOPODE;       break;
        default:                ch = TILEP_TRAN_VAMPIRE;                break;
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
    case transformation::bat_swarm:
        if (you.species == SP_GARGOYLE)
            ch = TILEP_TRAN_BAT_SWARM_GARGOYLE;
        else
            ch = TILEP_TRAN_BAT_SWARM;
        result.parts[TILEP_PART_BASE] = ch;
        _fill_doll_equipment_all_melded(result);
        break;
    case transformation::rime_yak:
        if (you.species == SP_GARGOYLE)
            ch = TILEP_TRAN_RIME_YAK_GARGOYLE;
        else
            ch = TILEP_TRAN_RIME_YAK;
        result.parts[TILEP_PART_BASE] = ch;
        _fill_doll_equipment_all_melded(result);
        break;
    case transformation::hive:
        switch (you.species)
        {
        case SP_OCTOPODE:
            result.parts[TILEP_PART_BASE] = TILEP_TRAN_HIVE_OCTOPODE;
            break;
        case SP_FELID:
            result.parts[TILEP_PART_BASE] = TILEP_TRAN_HIVE_FELID;
            break;
        default:
            result.parts[TILEP_PART_BODY] = TILEP_BODY_HIVE_FORM;
            break;
        }
        result.parts[TILEP_PART_HAIR] = 0;
        break;
    case transformation::aqua:
        switch (you.species)
        {
        case SP_ANEMOCENTAUR:   ch = TILEP_TRAN_AQUA_ANEMOCENTAUR;  break;
        case SP_DJINNI:         ch = TILEP_TRAN_AQUA_DJINN;         break;
        case SP_GARGOYLE:       ch = TILEP_TRAN_AQUA_GARGOYLE;      break;
        case SP_NAGA:           ch = TILEP_TRAN_AQUA_NAGA;          break;
        case SP_FELID:          ch = TILEP_TRAN_AQUA_FELID;         break;
        case SP_OCTOPODE:       ch = TILEP_TRAN_AQUA_OCTOPODE;      break;
        default:                ch = TILEP_TRAN_AQUA_HUMANOID;      break;
        }
        result.parts[TILEP_PART_BASE]    = ch;
        result.parts[TILEP_PART_HAIR]    = 0;
        break;
    case transformation::sphinx:
        if (you.species == SP_FELID)
            ch = TILEP_TRAN_SPHINX_FELID;
        else if (you.species == SP_GARGOYLE)
        {
            if (you.equipment.get_first_slot_item(SLOT_BARDING))
                ch = TILEP_TRAN_SPHINX_BARDING_GARGOYLE;
            else
                ch = TILEP_TRAN_SPHINX_GARGOYLE;
        }
        else if (you.equipment.get_first_slot_item(SLOT_BARDING))
            ch = TILEP_TRAN_SPHINX_BARDING;
        else
            ch = TILEP_TRAN_SPHINX;
        result.parts[TILEP_PART_BASE] = ch;
        _fill_doll_equipment_all_melded(result);
        break;
    case transformation::werewolf:
        switch (you.species)
        {
        case SP_ANEMOCENTAUR:   ch = TILEP_TRAN_WEREWOLF_ANEMOCENTAUR;  break;
        case SP_DJINNI:         ch = TILEP_TRAN_WEREWOLF_DJINN;         break;
        case SP_GARGOYLE:       ch = TILEP_TRAN_WEREWOLF_GARGOYLE;      break;
        case SP_NAGA:           ch = TILEP_TRAN_WEREWOLF_NAGA;          break;
        case SP_FELID:          ch = TILEP_TRAN_WEREWOLF_FELID;         break;
        case SP_OCTOPODE:       ch = TILEP_TRAN_WEREWOLF_OCTOPODE;      break;
        default:                ch = TILEP_TRAN_WEREWOLF_HUMANOID;      break;
        }
        result.parts[TILEP_PART_BASE]    = ch;
        result.parts[TILEP_PART_HAIR]    = 0;
        result.parts[TILEP_PART_LEG]     = 0;
        break;
    case transformation::walking_scroll:
        result.parts[TILEP_PART_BASE] = TILEP_TRAN_WALKING_SCROLL;
        _fill_doll_equipment_all_melded(result);
        break;
    case transformation::sun_scarab:
        if (you.species == SP_GARGOYLE)
            ch = TILEP_TRAN_SUN_SCARAB_GARGOYLE;
        else
            ch = TILEP_TRAN_SUN_SCARAB;
        result.parts[TILEP_PART_BASE] = ch;
        _fill_doll_equipment_all_melded(result);
        break;
    case transformation::medusa:
        switch (you.species)
        {
        case SP_OCTOPODE:
            result.parts[TILEP_PART_BASE] = TILEP_TRAN_MEDUSA_OCTOPODE;
            break;
        case SP_FELID:
            result.parts[TILEP_PART_BASE] = TILEP_TRAN_MEDUSA_FELID;
            break;
        case SP_ANEMOCENTAUR:
            result.parts[TILEP_PART_HELM] = TILEP_HELM_MEDUSA_FORM_ANEMOCENTAUR;
            result.parts[TILEP_PART_CLOAK] = TILEP_CLOAK_MEDUSA_FORM_ANEMOCENTAUR;
            break;
        default:
            result.parts[TILEP_PART_HELM] = TILEP_HELM_MEDUSA_FORM_HUMANOID;
            result.parts[TILEP_PART_CLOAK] = TILEP_CLOAK_MEDUSA_FORM_HUMANOID;
            break;
        }
        result.parts[TILEP_PART_HAIR] = 0;
        break;
    default:
        _fill_doll_equipment_default(result);
        break;
    }

    // XXX: The checks to hide pants per-species fail if they're not using a
    //      normal base species tile. But no form is going to *give* them pants,
    //      surely, so remind them they shouldn't have any.
    if (you.species == SP_FELID
        || you.species == SP_OCTOPODE
        || you.species == SP_DJINNI
        || you.species == SP_ANEMOCENTAUR
        || you.species == SP_NAGA)
    {
        result.parts[TILEP_PART_LEG] = 0;
    }

    // Base tile.
    if (result.parts[TILEP_PART_BASE] == TILEP_SHOW_EQUIP)
        tilep_race_default(you.species, you.experience_level, &result);

    // Main hand.
    if (result.parts[TILEP_PART_HAND1] == TILEP_SHOW_EQUIP)
    {
        item_def* wpn = you.weapon();
        if (wpn)
            result.parts[TILEP_PART_HAND1] = tilep_equ_weapon(*wpn);
        else
            result.parts[TILEP_PART_HAND1] = 0;
    }
    // Off hand.
    if (result.parts[TILEP_PART_HAND2] == TILEP_SHOW_EQUIP)
    {
        if (item_def* wpn = you.offhand_weapon())
            result.parts[TILEP_PART_HAND2] = mirror_weapon(*wpn);
        else if (item_def* item = you.equipment.get_first_slot_item(SLOT_OFFHAND))
            result.parts[TILEP_PART_HAND2] = tilep_equ_shield(*item);
        else
            result.parts[TILEP_PART_HAND2] = 0;
    }
    // Body armour.
    if (result.parts[TILEP_PART_BODY] == TILEP_SHOW_EQUIP)
    {
        if (item_def* armour = you.body_armour())
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
    if (you.duration[DUR_STAMPEDE])
    {
        if (you.has_mutation(MUT_NORTH_WIND))
            result.parts[TILEP_PART_DRCWING] = TILEP_STAMPEDE_WIND_NORTH;
        else if (you.has_mutation(MUT_SOUTH_WIND))
            result.parts[TILEP_PART_DRCWING] = TILEP_STAMPEDE_WIND_SOUTH;
        else if (you.has_mutation(MUT_EAST_WIND))
            result.parts[TILEP_PART_DRCWING] = TILEP_STAMPEDE_WIND_EAST;
        else if (you.has_mutation(MUT_WEST_WIND))
            result.parts[TILEP_PART_DRCWING] = TILEP_STAMPEDE_WIND_WEST;
        else
            result.parts[TILEP_PART_DRCWING] = TILEP_STAMPEDE_WIND;
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

    for (int i = 0; i < MAX_GEAR; ++i)
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
        || is_player_tile(base, TILEP_BASE_ANEMOCENTAUR))
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

    // Draw ghosts in reverse order so that parts blend with the background
    // underneath and not with the parts underneath. Parts drawn afterwards will
    // not obscure parts drawn previously, because "i" is passed as the depth below.
    //
    // Players should be properly drawn back-to-front so that translucent
    // equipment doesn't erase parts of the player and replace it with floor.
    const int start =   ghost ? TILEP_PART_MAX - 1 : 0;
    const int end =     ghost ? -1 : TILEP_PART_MAX;
    const int step =    ghost ? -1 : 1;
    for (int i = start; i != end; i += step)
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
