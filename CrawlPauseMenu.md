# CrawlPauseMenu - Implementation Plan

## Overview

This document outlines the steps needed to implement a pause button and menu system for Dungeon Crawl Stone Soup. The current game only allows changing settings by editing external files and reloading the game, which is not user-friendly. This implementation will add an in-game pause menu that allows players to:

1. Pause the game
2. Resume the game
3. View character statistics
4. Return to the main menu
5. Change basic settings (future enhancement)

## Current Game Architecture Analysis

Based on codebase analysis, here's the relevant architecture:

### Input System
- Main input loop is in `main.cc::_input()` function
- Commands are defined in `command-type.h` as enum values
- Key-to-command mapping handled in `macro.cc`
- There's already a `CMD_GAME_MENU` command with existing menu infrastructure

### Menu System
- Base menu system in `menu.cc` and `menu.h`
- Existing GameMenu class in `main.cc` (around line 1937)
- Menu rendering handled by UI framework in `ui.cc`
- Tile-based rendering support in `tileweb.cc` and `tilesdl.cc`

### Game State Management
- Game state tracked in `state.cc` with `crawl_state` global object
- Turn management and world reactions in `main.cc::world_reacts()`
- Player object (`you`) contains character data

## Implementation Steps

### Step 1: Add Pause Command

**Files to modify:**
- `command-type.h`
- `cmd-keys.h` 
- `macro.cc`

**Changes:**
1. Add `CMD_PAUSE_GAME` to the command enum in `command-type.h`
2. Add default key binding (suggest 'P' or Escape) in `cmd-keys.h`
3. Update command processing maps in `macro.cc`

### Step 2: Create Pause Game State

**Files to modify:**
- `state.h`
- `state.cc`

**Changes:**
1. Add `bool game_paused` flag to `game_state` struct
2. Add methods:
   - `pause_game()`
   - `unpause_game()`
   - `is_game_paused()`

### Step 3: Implement Pause Menu Class

**New file:** `pause-menu.h`
**New file:** `pause-menu.cc`

**Menu Options:**
1. Resume Game
2. Character Stats
3. Return to Main Menu
4. Quit Game

**Features:**
- Inherit from existing Menu class
- Use same styling as GameMenu
- Modal display (blocks game input)
- Keyboard navigation support
- Mouse support for tiles

### Step 4: Integrate Pause Functionality

**Files to modify:**
- `main.cc`

**Changes in `_input()` function:**
1. Check for pause command before other input processing
2. When paused, only process pause menu input
3. Block world reactions and turn progression while paused

**Changes in `process_command()` function:**
1. Add case for `CMD_PAUSE_GAME`
2. Show pause menu when command is triggered

### Step 5: Character Stats Display

**Files to modify:**
- `chardump.cc` (existing character dump functionality)
- `describe.cc` (character description utilities)

**New functionality:**
- Create condensed character stats view
- Show: Level, HP/MP, AC/EV, Skills, Religion, etc.
- Reuse existing dump formatting functions
- Make it scrollable for long stat lists

### Step 6: Pause Menu UI Implementation

**Menu Layout:**
```
┌─────────────────────────────┐
│          GAME PAUSED        │
├─────────────────────────────┤
│  R - Resume Game            │
│  C - Character Stats        │
│  M - Return to Main Menu    │
│  Q - Quit Game              │
├─────────────────────────────┤
│  Esc - Resume Game          │
└─────────────────────────────┘
```

**Files to create/modify:**
- Extend existing GameMenu class or create new PauseMenu class
- Add appropriate tile icons for menu options
- Support both console and tiles interfaces

### Step 7: Save State Handling

**Considerations:**
- Pause should work without triggering save
- Handle pause during animations/delays
- Ensure proper cleanup on unpause
- Handle pause during dangerous situations

### Step 8: Visual Pause Button Implementation

**Files to modify:**
- `tilereg-cmd.cc` (command region handling)
- `tilereg-crt.cc` (CRT region for console mode)
- `tilesdl.cc` (SDL tiles rendering)
- `tileweb.cc` (web tiles)
- `rltiles/dc-gui.txt` (add pause button icon)

**Button Implementation:**
1. **Location**: Bottom-right corner of the screen
2. **Icon**: Pause symbol (⏸️ or similar pause icon)
3. **Visibility**: Always visible during gameplay
4. **States**: Normal, hover, pressed
5. **Size**: Small, unobtrusive (~20x20 pixels)

**Technical Details:**
- Add pause button to UI overlay layer
- Handle mouse hover and click events
- Integrate with existing command region system
- Support both tiles and console modes
- Position relative to screen dimensions

### Step 9: Pause Button Graphics and Icons

**New files:**
- `rltiles/gui/pause_button.png` - Pause button icon
- `rltiles/gui/pause_button_hover.png` - Hover state
- `rltiles/gui/pause_button_pressed.png` - Pressed state

**Files to modify:**
- `rltiles/dc-gui.txt` - Add pause button tile definitions
- `rltiles/tiledef-gui.h` - Add pause button tile IDs

**Icon Design:**
- Simple pause symbol (two vertical bars)
- Fits existing UI style
- Clear visibility against game background
- Hover effect for feedback

### Step 10: Multiplayer/Web Considerations

**Files to check:**
- `tileweb.cc`
- Web frontend JavaScript files
- `webserver/game_data/static/` UI files

**Considerations:**
- Ensure pause works in web tiles
- Handle spectator mode appropriately
- Consider server timeout implications
- Button positioning in web interface
- Touch support for mobile devices

### Step 11: Testing and Edge Cases

**Test scenarios:**
1. Pause during combat
2. Pause during animations
3. Pause with pending input
4. Pause menu navigation
5. Character stats display
6. Return to main menu functionality
7. Multiple pause/unpause cycles
8. **Pause button click detection**
9. **Button hover effects**
10. **Button positioning across screen resolutions**
11. **Mouse vs keyboard pause interaction**

**Edge cases to handle:**
- Pause during delays/animations
- Pause with macros running
- Pause during tutorial mode
- Pause in different game types (Sprint, Tutorial, etc.)
- **Button obscuring important game elements**
- **Button click conflicts with game actions**
- **Responsive positioning on window resize**

## Technical Implementation Details

### Command Processing Flow
```
Input detected → _get_next_cmd() → Check if pause command OR pause button click
                                 ↓
                     If paused → Show pause menu → Process pause menu input
                                                 ↓
                     If not paused → Normal command processing
```

### Pause Button UI Integration
```cpp
// In tilesdl.cc or appropriate UI file
class PauseButton {
private:
    coord_def position;
    bool hover_state;
    bool visible;
    
public:
    void render();
    bool handle_mouse_event(const wm_mouse_event& event);
    void set_position(int screen_width, int screen_height);
    bool is_clicked(const coord_def& mouse_pos);
};
```

### Button Positioning Logic
```cpp
// Bottom-right corner positioning
void PauseButton::set_position(int screen_width, int screen_height) {
    const int margin = 10; // pixels from edge
    const int button_size = 20;
    
    position.x = screen_width - button_size - margin;
    position.y = screen_height - button_size - margin;
}
```

### Pause State Management
```cpp
// In state.h
struct game_state {
    // existing fields...
    bool game_paused;
    
    void pause_game() { game_paused = true; }
    void unpause_game() { game_paused = false; }
    bool is_game_paused() const { return game_paused; }
};
```

### Menu Integration
```cpp
// Extend existing GameMenu or create new PauseMenu
class PauseMenu : public Menu {
public:
    PauseMenu();
    void show_character_stats();
    bool return_to_main_menu();
    // ... other methods
};
```

## File Structure Summary

### New Files
- `pause-menu.h` - Pause menu class declaration
- `pause-menu.cc` - Pause menu implementation
- `pause-button.h` - Pause button UI component declaration  
- `pause-button.cc` - Pause button implementation
- `rltiles/gui/pause_button.png` - Pause button icon
- `rltiles/gui/pause_button_hover.png` - Hover state icon
- `rltiles/gui/pause_button_pressed.png` - Pressed state icon

### Modified Files
- `command-type.h` - Add CMD_PAUSE_GAME
- `cmd-keys.h` - Add key binding for pause
- `macro.cc` - Update command processing
- `state.h` - Add pause state management
- `state.cc` - Implement pause methods
- `main.cc` - Integrate pause in input loop and command processing
- `tilesdl.cc` - Add pause button rendering and mouse handling
- `tileweb.cc` - Web tiles pause button support
- `tilereg-cmd.cc` - Integrate pause button with command regions
- `rltiles/dc-gui.txt` - Add pause button tile definitions
- `rltiles/tiledef-gui.h` - Add pause button tile IDs

## Future Enhancements

1. **Settings Menu**: Add basic option changes (sound, tiles, etc.)
2. **Quick Save**: Add save game option to pause menu
3. **Key Bindings**: Allow customizing pause key
4. **Context Sensitivity**: Different pause options based on game state
5. **Pause Indicators**: Visual indication that game is paused
6. **Button Customization**: Allow moving or hiding the pause button
7. **Button Animation**: Subtle pulse or glow effects
8. **Accessibility**: Keyboard navigation to pause button
9. **Mobile Optimization**: Touch-friendly button sizing for mobile devices

## Compatibility Considerations

- Maintain backward compatibility with existing saves
- Ensure console and tiles versions work identically
- Support all existing game modes (Normal, Tutorial, Sprint, etc.)
- Handle different platforms (Windows, Linux, Mac, Android)

## Implementation Priority

1. **Phase 1** (Core): Steps 1-4 (basic pause/resume functionality)
2. **Phase 2** (Enhanced): Steps 5-7 (character stats and polished UI)
3. **Phase 3** (Visual): Steps 8-9 (pause button implementation and graphics)
4. **Phase 4** (Complete): Steps 10-11 (web/multiplayer support and full testing)

### Phase 3 Details - Pause Button Implementation

**Priority within Phase 3:**
1. Create button graphics and tile definitions
2. Implement basic button rendering in bottom-right corner
3. Add mouse click detection and hover effects
4. Integrate button click with pause command system
5. Test button positioning across different screen resolutions
6. Add web tiles support for the pause button

This implementation leverages the existing menu infrastructure and game state management, minimizing the amount of new code required while providing a robust pause system that fits naturally with Crawl's architecture. The visual pause button adds an intuitive GUI element that makes the pause functionality immediately discoverable to new players.
