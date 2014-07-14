#include "AppHdr.h"

#include "feature.h"

#include "colour.h"
#include "options.h"
#include "show.h"
#include "terrain.h"

#include "feature-data.h"
static FixedVector<feature_def, NUM_SHOW_ITEMS> item_defs;
static int feat_index[NUM_FEATURES];
static feature_def invis_fd, cloud_fd;

/** Give a feature_def some reasonable defaults.
 *
 *  XXX: This is kind of what a default constructor is for, but until
 *  we allow C++11-only features we can't have aggregate initialisation
 *  (in feature-data.h) as well as a constructor.
 *
 *  @param[out] fd The new feature_def to be given values.
 */
void init_fd(feature_def &fd)
{
    fd.feat = DNGN_UNSEEN;
    fd.name = fd.vaultname = "";
    fd.dchar = NUM_DCHAR_TYPES;
    fd.symbol = fd.magic_symbol = 0;
    fd.colour = fd.seen_colour = fd.em_colour = fd.seen_em_colour = BLACK;
    fd.map_colour = DARKGREY;
    fd.flags = FFT_NONE;
    fd.minimap = MF_UNSEEN;
}

/** Do the default colour relations on a feature_def.
 *
 *  @param[out] f The feature_def to be filled out.
 */
static void _create_colours(feature_def &f)
{
    if (f.seen_colour == BLACK)
        f.seen_colour = f.map_colour;

    if (f.seen_em_colour == BLACK)
        f.seen_em_colour = f.seen_colour;

    if (f.em_colour == BLACK)
        f.em_colour = f.colour;
}

/** Create the symbol/magic_symbol based on the dchar.
 *
 *  @param[out] f The feature_def to be filled out.
 */
static void _create_symbols(feature_def &f)
{
    if (!f.symbol && f.dchar != NUM_DCHAR_TYPES)
        f.symbol = Options.char_table[f.dchar];

    if (!f.magic_symbol)
        f.magic_symbol = f.symbol;
}

/** Put the feature overrides from the 'feature' option, stored in
 *  Options.feature_overrides, into feat_defs.
 */
static void _apply_feature_overrides()
{
    for (map<dungeon_feature_type, feature_def>::const_iterator fo
         = Options.feature_overrides.begin();
         fo != Options.feature_overrides.end();
         ++fo)
    {
        const feature_def           &ofeat  = fo->second;
        // Replicating get_feature_def since we need not-const.
        feature_def                 &feat   = feat_defs[feat_index[fo->first]];
        ucs_t c;

        if (ofeat.symbol && (c = get_glyph_override(ofeat.symbol)))
            feat.symbol = c;
        if (ofeat.magic_symbol && (c = get_glyph_override(ofeat.magic_symbol)))
            feat.magic_symbol = c;
        if (ofeat.colour)
            feat.colour = ofeat.colour;
        if (ofeat.map_colour)
            feat.map_colour = ofeat.map_colour;
        if (ofeat.seen_colour)
            feat.seen_colour = ofeat.seen_colour;
        if (ofeat.seen_em_colour)
            feat.seen_em_colour = ofeat.seen_em_colour;
        if (ofeat.em_colour)
            feat.em_colour = ofeat.em_colour;
    }
}

static void _init_feature_index()
{
    for (int i = 0; i < NUM_FEATURES; ++i)
        feat_index[i] = -1;

    for (int i = 0; i < (int) ARRAYSZ(feat_defs); ++i)
    {
        const dungeon_feature_type feat = feat_defs[i].feat;
        ASSERT_RANGE(feat, 0, NUM_FEATURES);
        ASSERT(feat_index[feat] == -1);
        feat_index[feat] = i;
    }
}

/** Called at startup to perform fixups on feat_defs and fill in item_defs
 *  and {invis,cloud}_fd.
 */
void init_show_table()
{
    _init_feature_index();
    _apply_feature_overrides();
    for (int i = 0; i < (int) ARRAYSZ(feat_defs); i++)
        _create_symbols(feat_defs[i]);

    for (int i = 0; i < NUM_SHOW_ITEMS; i++)
    {
        show_item_type si = static_cast<show_item_type>(i);
        // SHOW_ITEM_NONE is bogus, but "invis exposed" is an ok placeholder
        COMPILE_CHECK(DCHAR_ITEM_AMULET - DCHAR_ITEM_DETECTED + 2 == NUM_SHOW_ITEMS);
        init_fd(item_defs[si]);
        item_defs[si].minimap = MF_ITEM;
        item_defs[si].dchar = static_cast<dungeon_char_type>(i
            + DCHAR_ITEM_DETECTED - SHOW_ITEM_DETECTED);
        _create_symbols(item_defs[si]);
        _create_colours(item_defs[si]);
    }

    init_fd(invis_fd);
    invis_fd.dchar = DCHAR_INVIS_EXPOSED;
    invis_fd.minimap = MF_MONS_HOSTILE;
    _create_symbols(invis_fd);
    _create_colours(invis_fd);

    init_fd(invis_fd);
    cloud_fd.dchar = DCHAR_CLOUD;
    cloud_fd.minimap = MF_SKIP;
    _create_symbols(cloud_fd);
    _create_colours(cloud_fd);
}

/** Get the feature_def (not necessarily in feat_defs) for this show_type.
 *
 *  @param object The show_type for which a corresponding feature_def needs
 *                to be found.
 *  @returns a feature_def corresponding to it.
 */
const feature_def &get_feature_def(show_type object)
{
    switch (object.cls)
    {
    case SH_INVIS_EXPOSED:
        return invis_fd;
    case SH_CLOUD:
        return cloud_fd;
    case SH_ITEM:
        return item_defs[object.item];
    case SH_MONSTER:
        // If this is a monster that is hidden explicitly, show items if
        // any instead, or the base feature if there are no items.
        if (object.item != SHOW_ITEM_NONE)
            return item_defs[object.item];
    case SH_FEATURE:
        return get_feature_def(object.feat);
    default:
        die("invalid show object: class %d", object.cls);
    }
}

/** Does an entry matching this type exist in feat_defs?
 *
 *  Since there is a gap in the dungeon_feature_type enum, a loop that iterates
 *  over all its entries will run into values that don't correspond to any real
 *  feature type. get_feature_def can't return a good value for this, so anything
 *  that might reference an invalid type should check this function first.
 *
 *  @param feat The dungeon_feature_type that might be invalid.
 *  @returns true if feat exists in feature-data, false otherwise.
 */
bool is_valid_feature_type(dungeon_feature_type feat)
{
    return feat_index[feat] != -1;
}

/** Get the feature_def in feat_defs for this dungeon_feature_type.
 *
 *  @param feat The dungeon_feature_type for which the feature_def needs to be found.
 *  @returns a feature_def with the data in feature-data.h.
 */
const feature_def &get_feature_def(dungeon_feature_type feat)
{
    ASSERT_RANGE(feat, 0, NUM_FEATURES);
    ASSERT(is_valid_feature_type(feat));
    return feat_defs[feat_index[feat]];
}

/** What should be showed for this feat on magic mapping?
 *
 *  @param feat The feat to be hidden by magic mapping.
 *  @returns the converted type.
 */
dungeon_feature_type magic_map_base_feat(dungeon_feature_type feat)
{
    const feature_def& fdef = get_feature_def(feat);
    switch (fdef.dchar)
    {
    case DCHAR_STATUE:
        return DNGN_GRANITE_STATUE;
    case DCHAR_FLOOR:
    case DCHAR_TRAP:
        return DNGN_FLOOR;
    case DCHAR_WAVY:
        return DNGN_SHALLOW_WATER;
    case DCHAR_ARCH:
        return DNGN_UNKNOWN_PORTAL;
    case DCHAR_FOUNTAIN:
        return DNGN_FOUNTAIN_BLUE;
    case DCHAR_WALL:
        return DNGN_ROCK_WALL;
    case DCHAR_ALTAR:
        return DNGN_UNKNOWN_ALTAR;
    default:
        // We could do more, e.g. map the different upstairs together.
        return feat;
    }
}
