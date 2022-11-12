/*******************************************************************************************
*
*   Sound-settings v1.0.0 - Tool Description
*
*   MODULE USAGE:
*       #define GUI_SOUND-SETTINGS_IMPLEMENTATION
*       #include "gui_sound-settings.h"
*
*       INIT: GuiSound-settingsState state = InitGuiSound-settings();
*       DRAW: GuiSound-settings(&state);
*
*   LICENSE: Propietary License
*
*   Copyright (c) 2022 raylib technologies. All Rights Reserved.
*
*   Unauthorized copying of this file, via any medium is strictly prohibited
*   This project is proprietary and confidential unless the owner allows
*   usage in any other form by expresely written permission.
*
**********************************************************************************************/

#include <windows.h>
#include "raylib.h"

#undef RAYGUI_IMPLEMENTATION
#include "raygui.h"

#include <string.h>     // Required for: strcpy()

#ifndef GUI_SOUND-SETTINGS_H
#define GUI_SOUND-SETTINGS_H

typedef struct {
    float Slider001Value;

    

} GuiSound-settingsState;

#ifdef __cplusplus
extern "C" {            // Prevents name mangling of functions
#endif


GuiSound-settingsState InitGuiSound-settings(void);
void GuiSound-settings(GuiSound-settingsState *state);

//function to take the value from the slider and convert it to be used with windows API
DWORD VolumeValue(const int percentage);

static void VolumeLevel();

#ifdef __cplusplus
}
#endif

#endif // GUI_SOUND-SETTINGS_H


#if defined(GUI_SOUND-SETTINGS_IMPLEMENTATION)

#include "raygui.h"

//----------------------------------------------------------------------------------
// Global Variables Definition
//----------------------------------------------------------------------------------
//...

//----------------------------------------------------------------------------------
// Internal Module Functions Definition
//----------------------------------------------------------------------------------
//...

//----------------------------------------------------------------------------------
// Module Functions Definition
//----------------------------------------------------------------------------------
GuiSound-settingsState InitGuiSound-settings(void)
{
    GuiSound-settingsState state = { 0 };

    state.Slider001Value = 0.0f;

    // Custom variables initialization

    return state;
}


void GuiSound-settings(GuiSound-settingsState *state)
{
    GuiGroupBox((Rectangle){ 528, 152, 192, 96 }, "Sound Settings");
    state->Slider001Value = GuiSlider((Rectangle){ 576, 192, 112, 16 }, "Volume", NULL, state->Slider001Value, 0, 100);
}


DWORD VolumeValue(const int percentage) {
    int x = min(100, max(0, percentage)); // not needed?

    //the values for waveOutSetVolume are on a scale from 0-65535
    //we need to scale our percentage to those values
    //this next line scales it for one-channel audio
    const WORD wVol = static_cast<WORD>( ::MulDiv(65535, x, 100));
    //scales for two-channel audio
    const DWORD returnVol = ((wVol << 16) | wVol);
    return returnVol;

}
//Sets the volume level based on a slider bar float value input
static void VolumeLevel()
{


    // TODO: Implement control logic

}

#endif // GUI_SOUND-SETTINGS_IMPLEMENTATION
