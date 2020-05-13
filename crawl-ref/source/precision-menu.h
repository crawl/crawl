/**
 * @file
 * @brief Deprecated precision menu code.
**/

#pragma once

class PrecisionMenu;

#include "cio.h"

/**
 * @author Janne "felirx" Lahdenpera
 * Abstract base class interface for all menu items to inherit from.
 * Each item should know how it's rendered.
 * Rendering should only check the item bounds and screen bounds to prevent
 * assertion errors.
 */
class MenuItem
{
public:
    MenuItem();
    virtual ~MenuItem();

    void set_height(const int height);

    void set_id(int id) { m_item_id = id; }
    int get_id() const { return m_item_id; }

    virtual void set_bounds(const coord_def& min_coord, const coord_def& max_coord);
    virtual void set_bounds_no_multiply(const coord_def& min_coord,
                                        const coord_def& max_coord);
    virtual void move(const coord_def& delta);
    virtual const coord_def& get_min_coord() const { return m_min_coord; }
    virtual const coord_def& get_max_coord() const { return m_max_coord; }

    virtual void set_description_text(const string& text) { m_description = text; }
    virtual const string& get_description_text() const { return m_description; }

#ifdef USE_TILE_LOCAL
    virtual bool handle_mouse(const wm_mouse_event&) {return false; }
#endif

    virtual void select(bool toggle);
    virtual void select(bool toggle, int value);
    virtual bool selected() const;
    virtual void allow_highlight(bool toggle);
    virtual bool can_be_highlighted() const;
    virtual void set_highlight_colour(COLOURS colour);
    virtual COLOURS get_highlight_colour() const;
    virtual void set_fg_colour(COLOURS colour);
    virtual void set_bg_colour(COLOURS colour);
    virtual COLOURS get_fg_colour() const;
    virtual COLOURS get_bg_colour() const;

    virtual void set_visible(bool flag);
    virtual bool is_visible() const;

    virtual void render() = 0;

    void add_hotkey(int key);
    const vector<int>& get_hotkeys() const;
    void clear_hotkeys();

    void set_link_left(MenuItem* item);
    void set_link_right(MenuItem* item);
    void set_link_up(MenuItem* item);
    void set_link_down(MenuItem* item);

    MenuItem* get_link_left() const;
    MenuItem* get_link_right() const;
    MenuItem* get_link_up() const;
    MenuItem* get_link_down() const;

protected:
    coord_def m_min_coord;
    coord_def m_max_coord;
    bool m_selected;
    bool m_allow_highlight;
    bool m_dirty;
    bool m_visible;
    vector<int> m_hotkeys;
    string m_description;

    COLOURS m_fg_colour;
    COLOURS m_highlight_colour;
    int m_bg_colour;

    MenuItem* m_link_left;
    MenuItem* m_link_right;
    MenuItem* m_link_up;
    MenuItem* m_link_down;

#ifdef USE_TILE_LOCAL
    // Holds the conversion values to translate unit values to pixel values
    unsigned int m_unit_width_pixels;
    unsigned int m_unit_height_pixels;
    int get_vertical_offset() const;
#endif

    int m_item_id;
};

struct edit_result
{
    edit_result(string txt, int result) : text(txt), reader_result(result) { };
    string text; // the text in the box
    int reader_result; // the result from calling read_line, typically ascii
};

/**
 * Basic Item with string unformatted text that can be selected
 */
class TextItem : public MenuItem
{
public:
    TextItem();
    virtual ~TextItem();

    virtual void set_bounds(const coord_def& min_coord, const coord_def& max_coord) override;
    virtual void set_bounds_no_multiply(const coord_def& min_coord,
                                        const coord_def& max_coord) override;

    virtual void render() override;

    void set_text(const string& text);
    const string& get_text() const;

protected:
    void _wrap_text();

    string m_text;
    string m_render_text;

#ifdef USE_TILE_LOCAL
    FontBuffer m_font_buf;
#endif
};

class EditableTextItem : public TextItem
{
public:
    EditableTextItem();
    edit_result edit(const string *custom_prefill=nullptr,
                     const line_reader::keyproc keyproc_fun=nullptr);
    void set_editable(bool e, int width=-1);

    virtual bool selected() const override;
    virtual bool can_be_highlighted() const override;
    virtual void render() override;

    void set_tag(string t);
    void set_prompt(string p);

protected:
    bool editable;
    bool in_edit_mode;
    int edit_width;

    string tag;
    string prompt;

#ifdef USE_TILE_LOCAL
    LineBuffer m_line_buf;
#endif
};

/**
 * Behaves the same as TextItem, except selection has been overridden to always
 * return false
 */
class NoSelectTextItem : public TextItem
{
public:
    NoSelectTextItem();
    virtual ~NoSelectTextItem();
    virtual bool selected() const override;
    virtual bool can_be_highlighted() const override;
};

/**
 * Behaves the same as TextItem but use formatted text for rendering.
 * It ignores bg_colour.
 * TODO: add bg_colour support to formatted_string and merge this with TextItem.
 */
class FormattedTextItem : public TextItem
{
public:
    virtual void render() override;
};

/**
 * Holds an arbitrary number of tiles, currently rendered on top of each other
 */
#ifdef USE_TILE_LOCAL
class TextTileItem : public TextItem
{
public:
    TextTileItem();
    virtual ~TextTileItem();

    virtual void set_bounds(const coord_def& min_coord, const coord_def& max_coord) override;
    virtual void render() override;

    virtual void add_tile(tile_def tile);
    void clear_tile() { m_tiles.clear(); };

protected:
    vector<tile_def> m_tiles;
    FixedVector<TileBuffer, TEX_MAX> m_tile_buf;
};
#endif

/**
 * Abstract base class interface for all attachable objects.
 * Objects are generally containers that hold MenuItems, however special
 * objects are also possible, for instance MenuDescriptor.
 * All objects should have an unique string name, although the uniqueness
 * is not enforced or checked right now.
 */
class MenuObject
{
public:
    enum InputReturnValue
    {
        INPUT_NO_ACTION,          // Nothing happened
        INPUT_SELECTED,           // Something got selected
        INPUT_DESELECTED,         // Something got deselected
        INPUT_END_MENU_SUCCESS,   // Call the menu to end
        INPUT_END_MENU_ABORT,     // Call the menu to clear all selections and end
        INPUT_ACTIVE_CHANGED,     // Mouse position or keyboard event changed active
        INPUT_FOCUS_RELEASE_UP,   // Selection went out of menu from top
        INPUT_FOCUS_RELEASE_DOWN, // Selection went out of menu from down
        INPUT_FOCUS_RELEASE_LEFT, // Selection went out of menu from left
        INPUT_FOCUS_RELEASE_RIGHT,// Selection went out of menu from right
        INPUT_FOCUS_LOST,         // Eg. the user is moving his mouse somewhere else
    };

    MenuObject();
    virtual ~MenuObject();

    void set_height(const int height);
    void init(const coord_def& min_coord, const coord_def& max_coord,
              const string& name);
    const coord_def& get_min_coord() const { return m_min_coord; }
    const coord_def& get_max_coord() const { return m_max_coord; }
    const string& get_name() const { return m_object_name; }

    virtual void allow_focus(bool toggle);
    virtual bool can_be_focused();

    virtual InputReturnValue process_input(int key) = 0;
#ifdef USE_TILE_LOCAL
    virtual InputReturnValue handle_mouse(const wm_mouse_event& me) = 0;
#endif
    virtual void render() = 0;

    virtual void set_active_item(int ID) = 0;
    virtual void set_active_item(MenuItem* item) = 0;
    virtual void activate_first_item() = 0;
    virtual void activate_last_item() = 0;

    virtual void set_visible(bool flag);
    virtual bool is_visible() const;

    virtual vector<MenuItem*> get_selected_items();
    virtual bool select_item(int index) = 0;
    virtual bool select_item(MenuItem* item) = 0;
    virtual MenuItem* find_item_by_hotkey(int key);
    virtual MenuItem* select_item_by_hotkey(int key);
    virtual void clear_selections();
    virtual MenuItem* get_active_item() = 0;

    virtual bool attach_item(MenuItem* item) = 0;

protected:
    enum Direction
    {
        UP,
        DOWN,
        LEFT,
        RIGHT,
    };
    virtual void _place_items() = 0;
    virtual bool _is_mouse_in_bounds(const coord_def& pos);
    virtual MenuItem* _find_item_by_mouse_coords(const coord_def& pos);
    virtual MenuItem* _find_item_by_direction(const MenuItem* start,
                                              MenuObject::Direction dir) = 0;

    bool m_dirty;
    bool m_allow_focus;
    bool m_visible;

    coord_def m_min_coord;
    coord_def m_max_coord;
    string m_object_name;
    // by default, entries are held in a vector
    // if you need a different behaviour, please override the
    // affected methods
    vector<MenuItem*> m_entries;
#ifdef USE_TILE_LOCAL
    // Holds the conversion values to translate unit values to pixel values
    unsigned int m_unit_width_pixels;
    unsigned int m_unit_height_pixels;
#endif
};

/**
 * Container object that holds MenuItems in a 2d plane.
 * There is no internal hierarchy structure inside the menu, thus the objects
 * are freely placed within the boundaries of the container
 */
class MenuFreeform : public MenuObject
{
public:
    MenuFreeform();
    virtual ~MenuFreeform();

    virtual InputReturnValue process_input(int key) override;
#ifdef USE_TILE_LOCAL
    virtual InputReturnValue handle_mouse(const wm_mouse_event& me) override;
#endif
    virtual void render() override;
    virtual MenuItem* get_active_item() override;
    virtual void set_active_item(int ID) override;
    virtual void set_active_item(MenuItem* item) override;
    virtual void activate_first_item() override;
    virtual void activate_last_item() override;
    void set_default_item(MenuItem* item);
    void activate_default_item();

    virtual bool select_item(int index) override;
    virtual bool select_item(MenuItem* item) override;
    virtual bool attach_item(MenuItem* item) override;

protected:
    virtual void _place_items() override;
    virtual void _set_active_item(MenuItem *item);
    virtual MenuItem* _find_item_by_direction(
            const MenuItem* start, MenuObject::Direction dir) override;

    // cursor position
    MenuItem* m_active_item;
    MenuItem* m_default_item;
};

/**
 * Highlighter object.
 * TILES: It will create a coloured rectangle around the currently active item.
 * CONSOLE: It will muck with the Item background colour, setting it to highlight
 *          colour, reverting the change when active changes.
 */
class BoxMenuHighlighter : public MenuObject
{
public:
    BoxMenuHighlighter(PrecisionMenu* parent);
    virtual ~BoxMenuHighlighter();

    virtual InputReturnValue process_input(int key) override;
#ifdef USE_TILE_LOCAL
    virtual InputReturnValue handle_mouse(const wm_mouse_event& me) override;
#endif
    virtual void render() override;

    // these are not used, clear them
    virtual vector<MenuItem*> get_selected_items() override;
    virtual MenuItem* get_active_item() override { return nullptr; }
    virtual bool attach_item(MenuItem*) override { return false; }
    virtual void set_active_item(int) override {}
    virtual void set_active_item(MenuItem*) override {}
    virtual void activate_first_item() override {}
    virtual void activate_last_item() override {}

    virtual bool select_item(int) override { return false; }
    virtual bool select_item(MenuItem*) override { return false;}
    virtual MenuItem* select_item_by_hotkey(int) override
    {
        return nullptr;
    }
    virtual void clear_selections() override {}

    // Do not allow focus
    virtual void allow_focus(bool) override {}
    virtual bool can_be_focused() override { return false; }

protected:
    virtual void _place_items() override;
    virtual MenuItem* _find_item_by_mouse_coords(const coord_def&) override
    {
        return nullptr;
    }
    virtual MenuItem* _find_item_by_direction(const MenuItem*,
            MenuObject::Direction) override
    {
        return nullptr;
    }

    // Used to pull out currently active item
    PrecisionMenu* m_parent;
    MenuItem* m_active_item;

#ifdef USE_TILE_LOCAL
    LineBuffer m_line_buf;
    ShapeBuffer m_shape_buf;
#else
    COLOURS m_old_bg_colour;
#endif
};

/**
 * Inheritable root node of a menu that holds MenuObjects.
 * It is always full screen.
 * Everything attached to it it owns, thus deleting the memory when it exits the
 * scope.
 * TODO: add multiple paging support
 */
class PrecisionMenu
{
public:
    enum SelectType
    {
        PRECISION_SINGLESELECT,
        PRECISION_MULTISELECT,
    };

    PrecisionMenu();
    virtual ~PrecisionMenu();

    virtual void set_select_type(SelectType flag);
    virtual void clear();

    virtual void draw_menu();
    virtual bool process_key(int key);
#ifdef USE_TILE_LOCAL
    virtual int handle_mouse(const wm_mouse_event& me);
#endif

    // not const on purpose
    virtual vector<MenuItem*> get_selected_items();

    virtual void attach_object(MenuObject* item);
    virtual MenuObject* get_object_by_name(const string& search);
    virtual MenuItem* get_active_item();
    virtual void set_active_object(MenuObject* object);
    virtual void clear_selections();
protected:
    // These correspond to the Arrow keys when used for browsing the menus
    enum Direction
    {
        UP,
        DOWN,
        LEFT,
        RIGHT,
    };
    MenuObject* _find_object_by_direction(const MenuObject* start, Direction dir);

    vector<MenuObject*> m_attached_objects;
    MenuObject* m_active_object;

    SelectType m_select_type;
};
