/**
 * @file
 * @brief Functions used to manage sounds, but see libutil for actually playing them.
**/

#ifndef SOUNDS_H
#define SOUNDS_H

#include "options.h"

// Uncomment (and edit as appropriate) to play sounds.
//
// WARNING: Filenames passed to this command *are not validated in any way*.
//
#define SOUND_PLAY_COMMAND "/usr/bin/play -v .5 \"%s\" 2>/dev/null &"

// Uncomment this to enable playing sounds that play sounds that pause the
// gameplay until they finish.
//
// WARNING: This feature is not fully implemented yet!
//
//#define HOLD_SOUND_PLAY_COMMAND "/usr/bin/play -v .5 \"%s\" 2>/dev/null"

#ifdef USE_SOUND
void play_sound_from_pattern(const string& message);

// This function will return the sound_mapping it finds that matches
// the given string. If none is found, then a sound mapping with an empty
// string for the soundfile is returned.
sound_mapping check_sound_patterns(const string& message);

void play_sound(sound_mapping sound_data);
void play_sound(const char *file, bool interrupt_game = false);
#endif

#endif
