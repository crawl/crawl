#pragma once

// =========================================================================
//  Debugging Defines
// =========================================================================
#ifdef FULLDEBUG
    // Bounds checking and asserts
    #ifndef DEBUG
    #define DEBUG
    #endif

    // Outputs many "hidden" details, defaults to wizard on.
    #define DEBUG_DIAGNOSTICS
    #define DEBUG_MONINDEX

    // Scan for bad items before every input (may be slow)
    //
    // This function might slow things down quite a bit
    // on slow machines because it's going to go through
    // every item on the level and do string comparisons
    // against the name. Still, it is nice to know the
    // turn in which "bad" items appear.
    #define DEBUG_ITEM_SCAN
    #define DEBUG_MONS_SCAN

    #define DEBUG_BONES
#endif

// on by default (and has been for ~10 years)
#define DEBUG_ITEM_SCAN

#ifdef _DEBUG       // this is how MSVC signals a debug build
    #ifndef DEBUG
    #define DEBUG
    #endif
#endif

#ifdef DEBUG
    #ifdef __MWERKS__
        #define MSIPL_DEBUG_MODE
    #endif
#else
    #if !defined(NDEBUG)
        #define NDEBUG                  // used by <assert.h>
    #endif
#endif

#ifdef DEBUG_DIAGNOSTICS
    #define DEBUG_TESTS
    #define DEBUG_MONSPEAK
    #define DEBUG_STATISTICS
#endif

#ifdef DEBUG_MONSPEAK
    // ensure dprf is available
    #define DEBUG_DIAGNOSTICS
#endif
