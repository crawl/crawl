/**
 * @file
 * @brief Functions used to manage sounds, but see libutil for actually playing them.
**/

#ifndef SOUNDS_H
#define SOUNDS_H

#include "options.h"

#ifdef USE_SOUND

// Uncomment to play sounds. winmm must be linked in if this is uncommented.
// #define WINMM_PLAY_SOUNDS

// Uncomment (and edit as appropriate) to play sounds.
//
// WARNING: Filenames passed to this command *are not validated in any way*.
//
//#define SOUND_PLAY_COMMAND "/usr/bin/play -v .5 \"%s\" 2>/dev/null &"

// Uncomment this to enable playing sounds that play sounds that pause the
// gameplay until they finish.
//
// WARNING: This feature is not fully implemented yet!
//
//#define HOLD_SOUND_PLAY_COMMAND "/usr/bin/play -v .5 \"%s\" 2>/dev/null"

// This should match up with what's in play_sound, which will prioritize
// the various backends in a certain order.
#if defined(WINMM_PLAY_SOUNDS)
 #define SOUND_BACKEND "Sound support (Windows Multimedia API)"
#elif defined(SOUND_PLAY_COMMAND)
 #define SOUND_BACKEND "Sound support (External command)"
#elif defined(USE_SDL)
 #define SOUND_BACKEND "Sound support (SDL_mixer)"
#endif

// These are generic queues for playing sounds; they're intended for
// console outputs that are either so generic that regexes can't match
// them, or have other issues.
//
// To use them, just include the matching string in your sound option; the
// regex search will use that sound if it's found.
#define FIRE_PROMPT_SOUND "FIRE_PROMPT_SOUND"

void toggle_sound();

void play_sound_from_pattern(const string& message);

// This function will return the sound_mapping it finds that matches
// the given string. If none is found, then a sound mapping with an empty
// string for the soundfile is returned.
sound_mapping check_sound_patterns(const string& message);

void play_sound(sound_mapping sound_data);
void play_sound(const char *file, bool interrupt_game = false);


#endif  // End ifdef USE_SOUND
#endif  // End ifndef SOUNDS_H
