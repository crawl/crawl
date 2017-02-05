#include "AppHdr.h"

#include "sound.h"

#include "libutil.h"
#include "options.h"
#include "unicode.h"

#if defined(USE_SOUND) && defined(USE_SDL) && !defined(WINMM_PLAY_SOUNDS)
    #ifdef __ANDROID__
        #include <SDL_mixer.h>
    #else
        #include <SDL2/SDL_mixer.h>
    #endif
    Mix_Chunk* sdl_sound_to_play = nullptr;
#endif

#ifdef USE_SOUND

// Plays a sound based on the given message; it must match a regex pattern
// in an option file for a sound to be played.
void parse_sound(const string& message)
{
    play_sound(check_sound_patterns(message));
}

// This function will return the sound_mapping it finds that matches
// the given string. If none is found, then a sound mapping with an empty
// string for the soundfile is returned.
//
// The rational for NOT playing sounds within this function is to enable
// conditional behaviour from the calling function, i.e. only do something
// when a sound mapping is found.
sound_mapping check_sound_patterns(const string& message)
{
    sound_mapping matched_sound;

    matched_sound.pattern = "";
    matched_sound.soundfile = "";
    matched_sound.interrupt_game = false;

    for (const sound_mapping &sound : Options.sound_mappings)
    {
        // Maybe we should allow message channel matching as for
        // force_more_message?
        if (sound.pattern.matches(message))
        {
            //play_sound(sound.soundfile.c_str(), sound.interrupt_game);
            matched_sound = sound;
            break;
        }
    }

    return matched_sound;
}

void play_sound(sound_mapping sound_data)
{
    if (sound_data.soundfile != "")
        play_sound(sound_data.soundfile.c_str(), sound_data.interrupt_game);
}

// TODO: Make interrupt_game apply to any sound-playing method, not just SOUND_PLAY_COMMAND
// TODO: Add in-game option for disabling interrupt sounds (and sounds in general)
void play_sound(const char *file, bool interrupt_game)
{
    if (Options.sounds_on)
    {
#if defined(WINMM_PLAY_SOUNDS)
        // Check whether file exists, is readable, etc.?
        if (file && *file)
            sndPlaySoundW(OUTW(file), SND_ASYNC | SND_NODEFAULT);

#elif defined(SOUND_PLAY_COMMAND)
        char command[255];
        command[0] = 0;
        if (file && *file && (strlen(file) + strlen(SOUND_PLAY_COMMAND) < 255)
            && shell_safe(file))
        {
    #if defined(HOLD_SOUND_PLAY_COMMAND)
            if (interrupt_game)
                snprintf(command, sizeof command, HOLD_SOUND_PLAY_COMMAND, file);
            else
    #endif
                snprintf(command, sizeof command, SOUND_PLAY_COMMAND, file);

            system(OUTS(command));
        }
#elif defined(USE_SDL)
        static int last_channel = -1;

        if (Options.one_SDL_sound_channel
             && last_channel != -1
             && Mix_Playing(last_channel))
        {
            Mix_HaltChannel(last_channel);
        }
        if (sdl_sound_to_play != nullptr)
            Mix_FreeChunk(sdl_sound_to_play);

        sdl_sound_to_play = Mix_LoadWAV(OUTS(file));
        last_channel = Mix_PlayChannel(-1, sdl_sound_to_play, 0);
#endif
    } // End if (Options.sounds_on)
}

#endif // USE_SOUND
