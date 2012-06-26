#ifndef TAG_VERSION_H
#define TAG_VERSION_H

// Character info has its own top-level tag, mismatching majors don't break
// compatibility there.
#define TAG_CHR_FORMAT 0

// Let CDO updaters know if the syntax changes.
#define TAG_MAJOR_VERSION  33

// Minor version will be reset to zero when major version changes.
enum tag_minor_version
{
    TAG_MINOR_INVALID         = -1,
    TAG_MINOR_RESET           = 0, // Minor tags were reset
    TAG_MINOR_ROOT_BRANCH,         // Save root branch (zotdef in Zot)
    TAG_MINOR_KNOWN_MISSILES,      // Remember pluses/type of missiles
    TAG_MINOR_NO_MISSILE_PLUSES,   // ... or not
    TAG_MINOR_ONE_TIME_ABILITIES,  // Split one time abilities away from other gifts
    TAG_MINOR_SUNLIGHT,            // Sunlight lasts some time
    TAG_MINOR_AUTOPICKUP_TABLE,    // Toggle autopickup from the known item menu
    TAG_MINOR_DETECT_CURSE_REMOVAL,// Convert detect curse -> remove curse
    TAG_MINOR_OBJ_RODS,            // Rods are a separate item class
    TAG_MINOR_MONSTER_JEWELLERY,   // Allow monsters to wear rings/amulets
    TAG_MINOR_UNIFIED_PORTALS,     // Use branch enums for remembered portals.
    TAG_MINOR_CLEAR_APTABLE,       // Reset old broken force_autopickup tables.
    NUM_TAG_MINORS,
    TAG_MINOR_VERSION = NUM_TAG_MINORS - 1
};

#endif
