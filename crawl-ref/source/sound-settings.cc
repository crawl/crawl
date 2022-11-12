#include "sound-settings.h"

int main()
{
   
    int screenWidth = 800;
    int screenHeight = 450;

    InitWindow(screenWidth, screenHeight, "sound-settings");

    
    float Slider001Value = 0.0f;
    

    SetTargetFPS(60);

    // Main game loop
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        
        BeginDrawing();

            ClearBackground(GetColor(GuiGetStyle(DEFAULT, BACKGROUND_COLOR))); 

            GuiGroupBox((Rectangle){ 528, 152, 192, 96 }, "Sound Settings");
       
                   // TODO: Attach function to slider float value once function is written into header file
       
            Slider001Value = GuiSlider((Rectangle){ 576, 192, 112, 16 }, "Volume", NULL, Slider001Value, 0, 100);
            

        EndDrawing();
        
    }

    
    CloseWindow();        // Close window and OpenGL context
    

    return 0;
}


