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
    NUM_TAG_MINORS,
    TAG_MINOR_VERSION = NUM_TAG_MINORS - 1
};

#endif
