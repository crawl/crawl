/**
 * @file
 * @brief Deprecated precision menu code.
**/

#include "AppHdr.h"

#include "menu.h"
#include "precision-menu.h"

#include <functional>

#include "libutil.h"
#include "stringutil.h"

/**
 * Performs regular rectangular AABB intersection between the given AABB
 * rectangle and a item in the menu_entries
 * <pre>
 * start(x,y)------------
 *           |          |
 *           ------------end(x,y)
 * </pre>
 */
static bool _AABB_intersection(const coord_def& item_start,
                              const coord_def& item_end,
                              const coord_def& aabb_start,
                              const coord_def& aabb_end)
{
    // Check for no overlap using equals on purpose to rule out entities
    // that only brush the bounding box
    if (aabb_start.x >= item_end.x)
        return false;
    if (aabb_end.x <= item_start.x)
        return false;
    if (aabb_start.y >= item_end.y)
        return false;
    if (aabb_end.y <= item_start.y)
        return false;
    // We have overlap
    return true;
}

PrecisionMenu::PrecisionMenu() : m_active_object(nullptr),
    m_select_type(PRECISION_SINGLESELECT)
{
}

PrecisionMenu::~PrecisionMenu()
{
    clear();
}

void PrecisionMenu::set_select_type(SelectType flag)
{
    m_select_type = flag;
}

/**
 * Frees all used memory
 */
void PrecisionMenu::clear()
{
    // release all the data reserved
    deleteAll(m_attached_objects);
}

/**
 * Processes user input.
 *
 * Returns:
 * true when a significant event happened, signaling that the player has made a
 * menu ending action like selecting an item in singleselect mode
 * false otherwise
 */
bool PrecisionMenu::process_key(int key)
{
    if (m_active_object == nullptr)
    {
        if (m_attached_objects.empty())
        {
            // nothing to process
            return true;
        }
        else
        {
            // pick the first object possible
            for (auto mobj : m_attached_objects)
            {
                if (mobj->can_be_focused())
                {
                    m_active_object = mobj;
                    break;
                }
            }
        }
    }

#ifdef TOUCH_UI
    if (key == CK_TOUCH_DUMMY)
        return true; // mouse click in title area, which wouldn't usually be handled
#endif
    // Handle CK_MOUSE_CLICK separately
    // This signifies a menu ending action
    if (key == CK_MOUSE_CLICK)
        return true;

    bool focus_find = false;
    PrecisionMenu::Direction focus_direction;
    MenuObject::InputReturnValue input_ret = m_active_object->process_input(key);
    switch (input_ret)
    {
    case MenuObject::INPUT_NO_ACTION:
        break;
    case MenuObject::INPUT_SELECTED:
        if (m_select_type == PRECISION_SINGLESELECT)
            return true;
        else
        {
            // TODO: Handle multiselect somehow
        }
        break;
    case MenuObject::INPUT_DESELECTED:
        break;
    case MenuObject::INPUT_END_MENU_SUCCESS:
        return true;
    case MenuObject::INPUT_END_MENU_ABORT:
        clear_selections();
        return true;
    case MenuObject::INPUT_ACTIVE_CHANGED:
        break;
    case MenuObject::INPUT_FOCUS_RELEASE_UP:
        focus_find = true;
        focus_direction = PrecisionMenu::UP;
        break;
    case MenuObject::INPUT_FOCUS_RELEASE_DOWN:
        focus_find = true;
        focus_direction = PrecisionMenu::DOWN;
        break;
    case MenuObject::INPUT_FOCUS_RELEASE_LEFT:
        focus_find = true;
        focus_direction = PrecisionMenu::LEFT;
        break;
    case MenuObject::INPUT_FOCUS_RELEASE_RIGHT:
        focus_find = true;
        focus_direction = PrecisionMenu::RIGHT;
        break;
    default:
        die("Malformed return value");
        break;
    }
    if (focus_find)
    {
        MenuObject* find_object = _find_object_by_direction(m_active_object,
                                                            focus_direction);
        if (find_object != nullptr)
        {
            m_active_object->set_active_item((MenuItem*)nullptr);
            m_active_object = find_object;
            if (focus_direction == PrecisionMenu::UP)
                m_active_object->activate_last_item();
            else
                m_active_object->activate_first_item();
        }
    }
    // Handle selection of other objects items hotkeys
    for (MenuObject *obj : m_attached_objects)
    {
        MenuItem* tmp = obj->select_item_by_hotkey(key);
        if (tmp != nullptr)
        {
            // was it a toggle?
            if (!tmp->selected())
                continue;
            // it was a selection
            if (m_select_type == PrecisionMenu::PRECISION_SINGLESELECT)
                return true;
        }
    }
    return false;
}

#ifdef USE_TILE_LOCAL
int PrecisionMenu::handle_mouse(const wm_mouse_event &me)
{
    // Feed input to each attached object that the mouse is over
    // The objects are responsible for processing the input
    // This includes, if applicable for instance checking if the mouse
    // is over the item or not
    for (MenuObject *obj : m_attached_objects)
    {
        const MenuObject::InputReturnValue input_return = obj->handle_mouse(me);

        switch (input_return)
        {
        case MenuObject::INPUT_SELECTED:
            m_active_object = obj;
            if (m_select_type == PRECISION_SINGLESELECT)
                return CK_MOUSE_CLICK;
            break;
        case MenuObject::INPUT_ACTIVE_CHANGED:
            // Set the active object to be this one
            m_active_object = obj;
            break;
        case MenuObject::INPUT_END_MENU_SUCCESS:
            // something got clicked that needs to signal the menu to end
            return CK_MOUSE_CLICK;
        case MenuObject::INPUT_END_MENU_ABORT:
            // XXX: For right-click we use CK_MOUSE_CMD to cancel out of the
            // menu, but these mouse-button->key mappings are not very sane.
            clear_selections();
            return CK_MOUSE_CMD;
        case MenuObject::INPUT_FOCUS_LOST:
            // The object lost its focus and is no longer the active one
            if (obj == m_active_object)
                m_active_object = nullptr;
        default:
            break;
        }
    }
    return 0;
}
#endif

void PrecisionMenu::clear_selections()
{
    for (MenuObject *obj : m_attached_objects)
        obj->clear_selections();
}

/**
 * Finds the closest rectangle to given entry start on a cardinal
 * direction from it.
 * If no entries are found, nullptr is returned.
 *
 * TODO: This is exact duplicate of MenuObject::_find_item_by_direction();
 * maybe somehow generalize it and detach it from class?
 */
MenuObject* PrecisionMenu::_find_object_by_direction(const MenuObject* start,
                                                   Direction dir)
{
    if (start == nullptr)
        return nullptr;

    coord_def aabb_start(0,0);
    coord_def aabb_end(0,0);

    // construct the aabb
    switch (dir)
    {
    case UP:
        aabb_start.x = start->get_min_coord().x;
        aabb_end.x = start->get_max_coord().x;
        aabb_start.y = 0; // top of screen
        aabb_end.y = start->get_min_coord().y;
        break;
    case DOWN:
        aabb_start.x = start->get_min_coord().x;
        aabb_end.x = start->get_max_coord().x;
        aabb_start.y = start->get_max_coord().y;
        // we choose an arbitrarily large number here, because
        // tiles saves entry coordinates in pixels, yet console saves them
        // in characters
        // basically, we want the AABB to be large enough to extend to the
        // bottom of the screen in every possible resolution
        aabb_end.y = 32767;
        break;
    case LEFT:
        aabb_start.x = 0; // left of screen
        aabb_end.x = start->get_min_coord().x;
        aabb_start.y = start->get_min_coord().y;
        aabb_end.y = start->get_max_coord().y;
        break;
    case RIGHT:
        aabb_start.x = start->get_max_coord().x;
        // we again want a value that is always larger then the width of screen
        aabb_end.x = 32767;
        aabb_start.y = start->get_min_coord().y;
        aabb_end.y = start->get_max_coord().y;
        break;
    default:
        die("Bad direction given");
    }

    // loop through the entries
    // save the currently closest to the index in a variable
    MenuObject* closest = nullptr;
    for (MenuObject *obj : m_attached_objects)
    {
        if (!obj->can_be_focused())
        {
            // this is a noselect entry, skip it
            continue;
        }

        if (!_AABB_intersection(obj->get_min_coord(), obj->get_max_coord(),
                                aabb_start, aabb_end))
        {
            continue; // does not intersect, continue loop
        }

        // intersects
        // check if it's closer than current
        if (closest == nullptr)
            closest = obj;

        switch (dir)
        {
        case UP:
            if (obj->get_min_coord().y > closest->get_min_coord().y)
                closest = obj;
            break;
        case DOWN:
            if (obj->get_min_coord().y < closest->get_min_coord().y)
                closest = obj;
            break;
        case LEFT:
            if (obj->get_min_coord().x > closest->get_min_coord().x)
                closest = obj;
            break;
        case RIGHT:
            if (obj->get_min_coord().x < closest->get_min_coord().x)
                closest = obj;
        }
    }
    // TODO handle special cases here, like pressing down on the last entry
    // to go the the first item in that line
    return closest;
}

vector<MenuItem*> PrecisionMenu::get_selected_items()
{
    vector<MenuItem*> ret_val;

    for (MenuObject *obj : m_attached_objects)
        for (MenuItem *item : obj->get_selected_items())
            ret_val.push_back(item);

    return ret_val;
}

void PrecisionMenu::attach_object(MenuObject* item)
{
    ASSERT(item != nullptr);
    m_attached_objects.push_back(item);
}

// Predicate for std::find_if
static bool _string_lookup(MenuObject* item, string lookup)
{
    return item->get_name().compare(lookup) == 0;
}

MenuObject* PrecisionMenu::get_object_by_name(const string &search)
{
    auto it = find_if(begin(m_attached_objects), end(m_attached_objects),
                      bind(_string_lookup, placeholders::_1, search));
    return it != m_attached_objects.end() ? *it : nullptr;
}

MenuItem* PrecisionMenu::get_active_item()
{
    if (m_active_object != nullptr)
        return m_active_object->get_active_item();
    return nullptr;
}

void PrecisionMenu::set_active_object(MenuObject* object)
{
    if (object == m_active_object)
        return;

    // is the object attached?
    auto find_val = find(m_attached_objects.begin(), m_attached_objects.end(),
                         object);
    if (find_val != m_attached_objects.end())
    {
        m_active_object = object;
        m_active_object->activate_first_item();
    }
}

void PrecisionMenu::draw_menu()
{
    for (MenuObject *obj : m_attached_objects)
        obj->render();
}

MenuItem::MenuItem(): m_min_coord(0,0), m_max_coord(0,0), m_selected(false),
                      m_allow_highlight(true), m_dirty(false), m_visible(false),
                      m_link_left(nullptr), m_link_right(nullptr),
                      m_link_up(nullptr), m_link_down(nullptr), m_item_id(-1)
{
#ifdef USE_TILE_LOCAL
    m_unit_width_pixels = tiles.get_crt_font()->char_width();
    m_unit_height_pixels = tiles.get_crt_font()->char_height();
#endif

    set_fg_colour(LIGHTGRAY);
    set_bg_colour(BLACK);
    set_highlight_colour(BLACK);
}

MenuItem::~MenuItem()
{
}

#ifdef USE_TILE_LOCAL
void MenuItem::set_height(const int height)
{
    m_unit_height_pixels = height;
}
#endif

/**
 * Override this if you use eg funky different sized fonts, tiles etc
 */
void MenuItem::set_bounds(const coord_def& min_coord, const coord_def& max_coord)
{
#ifdef USE_TILE_LOCAL
    // these are saved in font dx / dy for mouse to work properly
    // remove 1 unit from all the entries because console starts at (1,1)
    // but tiles starts at (0,0)
    m_min_coord.x = (min_coord.x - 1) * m_unit_width_pixels;
    m_min_coord.y = (min_coord.y - 1) * m_unit_height_pixels;
    m_max_coord.x = (max_coord.x - 1) * m_unit_width_pixels;
    m_max_coord.y = (max_coord.y - 1) * m_unit_height_pixels;
#else
    m_min_coord = min_coord;
    m_max_coord = max_coord;
#endif
}

/**
 * This is handly if you are already working with existing multiplied
 * coordinates and modifying them
 */
void MenuItem::set_bounds_no_multiply(const coord_def& min_coord,
                                      const coord_def& max_coord)
{
    m_min_coord = min_coord;
    m_max_coord = max_coord;
}

void MenuItem::move(const coord_def& delta)
{
    m_min_coord += delta;
    m_max_coord += delta;
}

// By default, value does nothing. Override for Items needing it.
void MenuItem::select(bool toggle, int /*value*/)
{
    select(toggle);
}

void MenuItem::select(bool toggle)
{
    m_selected = toggle;
    m_dirty = true;
}

bool MenuItem::selected() const
{
    return m_selected;
}

void MenuItem::allow_highlight(bool toggle)
{
    m_allow_highlight = toggle;
    m_dirty = true;
}

bool MenuItem::can_be_highlighted() const
{
    return m_allow_highlight;
}

void MenuItem::set_highlight_colour(COLOURS colour)
{
    m_highlight_colour = colour;
    m_dirty = true;
}

COLOURS MenuItem::get_highlight_colour() const
{
    return m_highlight_colour;
}

void MenuItem::set_bg_colour(COLOURS colour)
{
    m_bg_colour = colour;
    m_dirty = true;
}

void MenuItem::set_fg_colour(COLOURS colour)
{
    m_fg_colour = colour;
    m_dirty = true;
}

COLOURS MenuItem::get_fg_colour() const
{
    return m_fg_colour;
}

COLOURS MenuItem::get_bg_colour() const
{
    return static_cast<COLOURS> (m_bg_colour);
}

void MenuItem::set_visible(bool flag)
{
    m_visible = flag;
}

bool MenuItem::is_visible() const
{
    return m_visible;
}

void MenuItem::add_hotkey(int key)
{
    m_hotkeys.push_back(key);
}

void MenuItem::clear_hotkeys()
{
    m_hotkeys.clear();
}

const vector<int>& MenuItem::get_hotkeys() const
{
    return m_hotkeys;
}

void MenuItem::set_link_left(MenuItem* item)
{
    m_link_left = item;
}

void MenuItem::set_link_right(MenuItem* item)
{
    m_link_right = item;
}

void MenuItem::set_link_up(MenuItem* item)
{
    m_link_up = item;
}

void MenuItem::set_link_down(MenuItem* item)
{
    m_link_down = item;
}

MenuItem* MenuItem::get_link_left() const
{
    return m_link_left;
}

MenuItem* MenuItem::get_link_right() const
{
    return m_link_right;
}

MenuItem* MenuItem::get_link_up() const
{
    return m_link_up;
}

MenuItem* MenuItem::get_link_down() const
{
    return m_link_down;
}

#ifdef USE_TILE_LOCAL
int MenuItem::get_vertical_offset() const
{
    return m_unit_height_pixels / 2 - tiles.get_crt_font()->char_height() / 2;
}
#endif

TextItem::TextItem()
#ifdef USE_TILE_LOCAL
                        : m_font_buf(tiles.get_crt_font())
#endif
{
}

TextItem::~TextItem()
{
}

/**
 * Rewrap the text if bounds changes
 */
void TextItem::set_bounds(const coord_def& min_coord, const coord_def& max_coord)
{
    MenuItem::set_bounds(min_coord, max_coord);
    _wrap_text();
    m_dirty = true;
}

/**
 * Rewrap the text if bounds changes
 */
void TextItem::set_bounds_no_multiply(const coord_def& min_coord,
                                      const coord_def& max_coord)
{
    MenuItem::set_bounds_no_multiply(min_coord, max_coord);
    _wrap_text();
    m_dirty = true;
}

void TextItem::render()
{
    if (!m_visible)
        return;

#ifdef USE_TILE_LOCAL
    if (m_dirty)
    {
        m_font_buf.clear();
        // TODO: handle m_bg_colour
        m_font_buf.add(m_render_text, term_colours[m_fg_colour],
                       m_min_coord.x, m_min_coord.y + get_vertical_offset());
        m_dirty = false;
    }
    m_font_buf.draw();
#else
    // Clean the drawing area first
    // clear_to_end_of_line does not work for us
    string white_space(m_max_coord.x - m_min_coord.x, ' ');
    textcolour(BLACK);
    for (int i = 0; i < (m_max_coord.y - m_min_coord.y); ++i)
    {
        cgotoxy(m_min_coord.x, m_min_coord.y + i);
        cprintf("%s", white_space.c_str());
    }

    // print each line separately, is there a cleaner solution?
    size_t newline_pos = 0;
    size_t endline_pos = 0;
    for (int i = 0; i < (m_max_coord.y - m_min_coord.y); ++i)
    {
        endline_pos = m_render_text.find('\n', newline_pos);
        cgotoxy(m_min_coord.x, m_min_coord.y + i);
        textcolour(m_fg_colour);
        textbackground(m_bg_colour);
        cprintf("%s", m_render_text.substr(newline_pos,
                endline_pos - newline_pos).c_str());
        if (endline_pos != string::npos)
            newline_pos = endline_pos + 1;
        else
            break;
    }
    // clear text background
    textbackground(BLACK);
#endif
}

void TextItem::set_text(const string& text)
{
    m_text = text;
    _wrap_text();
    m_dirty = true;
}

const string& TextItem::get_text() const
{
    return m_text;
}

/**
 * Wraps and chops the #m_text variable and saves the chopped
 * text to #m_render_text.
 * This is done to preserve the old text in case the text item
 * changes size and could fit more text.
 * Override if you use font with different sizes than CRTRegion font.
 */
void TextItem::_wrap_text()
{
    m_render_text = m_text; // preserve original copy intact
    int max_cols;
    int max_lines;
    max_cols = (m_max_coord.x - m_min_coord.x);
    max_lines = (m_max_coord.y - m_min_coord.y);
#ifdef USE_TILE_LOCAL
    // Tiles saves coordinates in pixels
    max_cols = max_cols / m_unit_width_pixels;
    max_lines = max_lines / m_unit_height_pixels;
#endif
    if (max_cols == 0 || max_lines == 0)
    {
        // escape and set render text to nothing
        m_render_text = "";
        return;
    }

    int num_linebreaks = linebreak_string(m_render_text, max_cols);
    if (num_linebreaks > max_lines)
    {
        size_t pos = 0;
        // find the max_line'th occurrence of '\n'
        for (int i = 0; i < max_lines; ++i)
            pos = m_render_text.find('\n', pos);

        // Chop of all the nonfitting text
        m_render_text = m_render_text.substr(pos);
    }
    // m_render_text now holds the fitting part of the text, ready for render!
}


EditableTextItem::EditableTextItem() : TextItem(),
                        editable(true), in_edit_mode(false), edit_width(-1),
                        tag("generic_text_box")
{
}

void EditableTextItem::set_editable(bool e, int width)
{
    editable = e;
    edit_width = width;
}

/**
 * A rudimentary textbox editing mode.
 *
 * This uses a line_reader to read some text at the location of the TextItem.
 * It does not do anything with the edit results! You will need to call this
 * function at the right point in the gui, and do something appropriate with
 * the results elsewhere.
 *
 * @param custom_prefill a string to populate the box; if null, this will use
 *                          the current text.
 * @param keyproc_fun an optional keyproc for the line_reader
  *                     (see lin_reader::set_keyproc).
 *
 * @return the result of the editing, including the string and the int
 *          returned by the line_reader.
 */
edit_result EditableTextItem::edit(const string *custom_prefill,
                                   const line_reader::keyproc keyproc_fun)
{
    char buf[80];

    if (!editable)
        return edit_result(string(m_text), 0);

    // this is needed because render will get called during the input loop.
    unwind_bool e_mode(in_edit_mode, true);

    int e_width;
    int box_width = m_max_coord.x - m_min_coord.x;
    if (edit_width <= 0)
        e_width = box_width;
    else
        e_width = edit_width;

    e_width = min(e_width, (int) sizeof buf - 1);

    // TODO: make width not dependent on prefill string
    string prefill = make_stringf("%-*s", e_width,
        custom_prefill ? custom_prefill->c_str() : m_text.c_str());

    strncpy(buf, prefill.c_str(), e_width);
    buf[e_width] = 0;

    mouse_control mc(MOUSE_MODE_PROMPT);

#ifdef USE_TILE_LOCAL
    m_line_buf.clear();
    m_line_buf.add_square(m_min_coord.x, m_min_coord.y,
                          m_max_coord.x, m_max_coord.y, term_colours[RED]);
    m_line_buf.draw();

    unwind_bool dirty(m_dirty, false);

    fontbuf_line_reader reader(buf, e_width+1, m_font_buf, 80);
    reader.set_location(coord_def(m_min_coord.x,
                                  m_min_coord.y + get_vertical_offset()));
#else
    line_reader reader(buf, e_width+1, 80);
    reader.set_location(m_min_coord);
#endif

    reader.set_edit_mode(EDIT_MODE_OVERWRITE);
    if (keyproc_fun)
        reader.set_keyproc(keyproc_fun);

#ifdef USE_TILE_WEB
    reader.set_prompt(prompt);
    reader.set_tag(tag);
#endif

    reader.set_colour(COLOUR_INHERIT, m_highlight_colour);
    int result = reader.read_line(false, true);

#ifdef USE_TILE_LOCAL
    m_line_buf.clear();
    m_line_buf.draw();
#endif

    return edit_result(string(buf), result);
}

void EditableTextItem::set_tag(string t)
{
    tag = t;
}

void EditableTextItem::set_prompt(string p)
{
    prompt = p;
}

bool EditableTextItem::selected() const
{
    return false;
}

bool EditableTextItem::can_be_highlighted() const
{
    // TODO: make this work better
    return false;
}

void EditableTextItem::render()
{
#ifdef USE_TILE_LOCAL
    if (in_edit_mode)
    {
        m_line_buf.add_square(m_min_coord.x, m_min_coord.y,
                              m_max_coord.x, m_max_coord.y,
                              term_colours[m_highlight_colour]);
        m_line_buf.draw();
        // this relies on m_font_buf being modified by the reader
        m_font_buf.draw();
    }
    else
    {
        m_line_buf.clear();
        m_line_buf.draw();
        TextItem::render();
    }
#else
    TextItem::render();
#endif //USE_TILE_LOCAL
}

NoSelectTextItem::NoSelectTextItem()
{
}

NoSelectTextItem::~NoSelectTextItem()
{
}

// Do not allow selection
bool NoSelectTextItem::selected() const
{
    return false;
}

// Do not allow highlight
bool NoSelectTextItem::can_be_highlighted() const
{
    return false;
}

void FormattedTextItem::render()
{
    if (!m_visible)
        return;

    if (m_max_coord.x == m_min_coord.x || m_max_coord.y == m_min_coord.y)
        return;

#ifdef USE_TILE_LOCAL
    if (m_dirty)
    {
        m_font_buf.clear();
        // FIXME: m_fg_colour doesn't work here while it works in console.
        m_font_buf.add(formatted_string::parse_string(m_render_text,
                                                      m_fg_colour),
                       m_min_coord.x, m_min_coord.y + get_vertical_offset());
        m_dirty = false;
    }
    m_font_buf.draw();
#else
    // Clean the drawing area first
    // clear_to_end_of_line does not work for us
    ASSERT(m_max_coord.x > m_min_coord.x);
    ASSERT(m_max_coord.y > m_min_coord.y);
    string white_space(m_max_coord.x - m_min_coord.x, ' ');
    for (int i = 0; i < (m_max_coord.y - m_min_coord.y); ++i)
    {
        cgotoxy(m_min_coord.x, m_min_coord.y + i);
        cprintf("%s", white_space.c_str());
    }

    cgotoxy(m_min_coord.x, m_min_coord.y);
    textcolour(m_fg_colour);
    display_tagged_block(m_render_text);
#endif
}

#ifdef USE_TILE_LOCAL
TextTileItem::TextTileItem()
{
    for (int i = 0; i < TEX_MAX; i++)
        m_tile_buf[i].set_tex(&tiles.get_image_manager()->m_textures[i]);
    m_unit_height_pixels = max<int>(m_unit_height_pixels, TILE_Y);
}

TextTileItem::~TextTileItem()
{
}

void TextTileItem::add_tile(tile_def tile)
{
    m_tiles.push_back(tile);
    m_dirty = true;
}

void TextTileItem::set_bounds(const coord_def &min_coord, const coord_def &max_coord)
{
    // these are saved in font dx / dy for mouse to work properly
    // remove 1 unit from all the entries because console starts at (1,1)
    // but tiles starts at (0,0)
    m_min_coord.x = (min_coord.x - 1) * m_unit_width_pixels;
    m_max_coord.x = (max_coord.x - 1) * m_unit_width_pixels + 4;
    m_min_coord.y = (min_coord.y - 1) * m_unit_height_pixels;
    m_max_coord.y = (max_coord.y - 1) * m_unit_height_pixels + 4;
}

void TextTileItem::render()
{
    if (!m_visible)
        return;

    if (m_dirty)
    {
        m_font_buf.clear();
        for (int t = 0; t < TEX_MAX; t++)
            m_tile_buf[t].clear();
        for (const tile_def &tdef : m_tiles)
        {
            int tile      = tdef.tile;
            const auto tex = get_tile_texture(tile);
            m_tile_buf[tex].add_unscaled(tile, m_min_coord.x + 2, m_min_coord.y + 2,
                                         tdef.ymax,
                                         (float)m_unit_height_pixels / TILE_Y);
        }
        // center the text
        // TODO wrap / chop the text
        const int tile_offset = m_tiles.empty() ? 0 : (m_unit_height_pixels + 6);
        m_font_buf.add(m_text, term_colours[m_fg_colour],
                       m_min_coord.x + 2 + tile_offset,
                       m_min_coord.y + 2 + get_vertical_offset());

        m_dirty = false;
    }

    m_font_buf.draw();
    for (int i = 0; i < TEX_MAX; i++)
        m_tile_buf[i].draw();
}
#endif

MenuObject::MenuObject() : m_dirty(false), m_allow_focus(true), m_min_coord(0,0),
                           m_max_coord(0,0), m_object_name("unnamed object")
{
#ifdef USE_TILE_LOCAL
    m_unit_width_pixels = tiles.get_crt_font()->char_width();
    m_unit_height_pixels = tiles.get_crt_font()->char_height();
#endif
}

MenuObject::~MenuObject()
{
}

#ifdef USE_TILE_LOCAL
void MenuObject::set_height(const int height)
{
    m_unit_height_pixels = height;
}
#endif

void MenuObject::init(const coord_def& min_coord, const coord_def& max_coord,
                      const string& name)
{
#ifdef USE_TILE_LOCAL
    // these are saved in font dx / dy for mouse to work properly
    // remove 1 unit from all the entries because console starts at (1,1)
    // but tiles starts at (0,0)
    m_min_coord.x = (min_coord.x - 1) * m_unit_width_pixels;
    m_min_coord.y = (min_coord.y - 1) * m_unit_height_pixels;
    m_max_coord.x = (max_coord.x - 1) * m_unit_width_pixels;
    m_max_coord.y = (max_coord.y - 1) * m_unit_height_pixels;
#else
    m_min_coord = min_coord;
    m_max_coord = max_coord;
#endif
    m_object_name = name;
}

bool MenuObject::_is_mouse_in_bounds(const coord_def& pos)
{
    // Is the mouse in our bounds?
    if (m_min_coord.x > static_cast<int> (pos.x)
        || m_max_coord.x < static_cast<int> (pos.x)
        || m_min_coord.y > static_cast<int> (pos.y)
        || m_max_coord.y < static_cast<int> (pos.y))
    {
        return false;
    }
    return true;
}

MenuItem* MenuObject::_find_item_by_mouse_coords(const coord_def& pos)
{
    // Is the mouse even in bounds?
    if (!_is_mouse_in_bounds(pos))
        return nullptr;

    // Traverse
    for (MenuItem *item : m_entries)
    {
        if (!item->can_be_highlighted())
        {
            // this is a noselect entry, skip it
            continue;
        }
        if (!item->is_visible())
        {
            // this item is not visible, skip it
            continue;
        }
        if (pos.x >= item->get_min_coord().x
            && pos.x <= item->get_max_coord().x
            && pos.y >= item->get_min_coord().y
            && pos.y <= item->get_max_coord().y)
        {
            // We're inside
            return item;
        }
    }

    // nothing found
    return nullptr;
}

MenuItem* MenuObject::find_item_by_hotkey(int key)
{
    // browse through all the Entries
    for (MenuItem *item : m_entries)
        for (int hotkey : item->get_hotkeys())
            if (key == hotkey)
                return item;

    return nullptr;
}

MenuItem* MenuObject::select_item_by_hotkey(int key)
{
    MenuItem* item = find_item_by_hotkey(key);
    if (item)
        select_item(item);
    return item;
}

vector<MenuItem*> MenuObject::get_selected_items()
{
    vector<MenuItem *> result;
    for (MenuItem *item : m_entries)
        if (item->selected())
            result.push_back(item);

    return result;
}

void MenuObject::clear_selections()
{
    for (MenuItem *item : m_entries)
        item->select(false);
}

void MenuObject::allow_focus(bool toggle)
{
    m_allow_focus = toggle;
}

bool MenuObject::can_be_focused()
{
    if (m_entries.empty())
    {
        // Do not allow focusing empty containers by default
        return false;
    }
    return m_allow_focus;
}

void MenuObject::set_visible(bool flag)
{
    m_visible = flag;
}

bool MenuObject::is_visible() const
{
    return m_visible;
}

MenuFreeform::MenuFreeform(): m_active_item(nullptr), m_default_item(nullptr)
{
}

MenuFreeform::~MenuFreeform()
{
    deleteAll(m_entries);
}

void MenuFreeform::set_default_item(MenuItem* item)
{
    m_default_item = item;
}

void MenuFreeform::activate_default_item()
{
    m_active_item = m_default_item;
}

MenuObject::InputReturnValue MenuFreeform::process_input(int key)
{
    if (!m_allow_focus || !m_visible)
        return INPUT_NO_ACTION;

    if (m_active_item == nullptr)
    {
        if (m_entries.empty())
        {
            // nothing to process
            return MenuObject::INPUT_NO_ACTION;
        }
        else if (m_default_item == nullptr)
        {
            // pick the first item possible
            for (auto mentry : m_entries)
            {
                if (mentry->can_be_highlighted())
                {
                    m_active_item = mentry;
                    break;
                }
            }
        }
    }

    if (m_active_item == nullptr && m_default_item != nullptr)
    {
        switch (key)
        {
        case CK_UP:
        case CK_DOWN:
        case CK_LEFT:
        case CK_RIGHT:
        case CK_ENTER:
            set_active_item(m_default_item);
            return MenuObject::INPUT_ACTIVE_CHANGED;
        }
    }

    MenuItem* find_entry = nullptr;
    switch (key)
    {
    case CK_ENTER:
        if (m_active_item == nullptr)
            return MenuObject::INPUT_NO_ACTION;

        select_item(m_active_item);
        if (m_active_item->selected())
            return MenuObject::INPUT_SELECTED;
        else
            return MenuObject::INPUT_DESELECTED;
        break;
    case CK_UP:
        find_entry = _find_item_by_direction(m_active_item, UP);
        if (find_entry != nullptr)
        {
            set_active_item(find_entry);
            return MenuObject::INPUT_ACTIVE_CHANGED;
        }
        else
            return MenuObject::INPUT_FOCUS_RELEASE_UP;
        break;
    case CK_DOWN:
        find_entry = _find_item_by_direction(m_active_item, DOWN);
        if (find_entry != nullptr)
        {
            set_active_item(find_entry);
            return MenuObject::INPUT_ACTIVE_CHANGED;
        }
        else
            return MenuObject::INPUT_FOCUS_RELEASE_DOWN;
        break;
    case CK_LEFT:
        find_entry = _find_item_by_direction(m_active_item, LEFT);
        if (find_entry != nullptr)
        {
            set_active_item(find_entry);
            return MenuObject::INPUT_ACTIVE_CHANGED;
        }
        else
            return MenuObject::INPUT_FOCUS_RELEASE_LEFT;
        break;
    case CK_RIGHT:
        find_entry = _find_item_by_direction(m_active_item, RIGHT);
        if (find_entry != nullptr)
        {
            set_active_item(find_entry);
            return MenuObject::INPUT_ACTIVE_CHANGED;
        }
        else
            return MenuObject::INPUT_FOCUS_RELEASE_RIGHT;
        break;
    default:
        find_entry = select_item_by_hotkey(key);
        if (find_entry != nullptr)
        {
            if (find_entry->selected())
                return MenuObject::INPUT_SELECTED;
            else
                return MenuObject::INPUT_DESELECTED;
        }
        break;
    }
    return MenuObject::INPUT_NO_ACTION;
}

#ifdef USE_TILE_LOCAL
MenuObject::InputReturnValue MenuFreeform::handle_mouse(const wm_mouse_event& me)
{
    if (!m_allow_focus || !m_visible)
        return INPUT_NO_ACTION;

    if (!_is_mouse_in_bounds(coord_def(me.px, me.py)))
    {
        if (m_active_item != nullptr)
        {
            _set_active_item(nullptr);
            return INPUT_FOCUS_LOST;
        }
        else
            return INPUT_NO_ACTION;
    }

    MenuItem* find_item = _find_item_by_mouse_coords(coord_def(me.px, me.py));

    if (find_item && find_item->handle_mouse(me))
        return MenuObject::INPUT_SELECTED; // The object handled the event
    else if (me.event == wm_mouse_event::MOVE)
    {
        if (find_item == nullptr)
        {
            if (m_active_item != nullptr)
            {
                _set_active_item(nullptr);
                return INPUT_NO_ACTION;
            }
        }
        else
        {
            if (m_active_item != find_item)
            {
                set_active_item(find_item);
                return INPUT_ACTIVE_CHANGED;
            }
        }
        return INPUT_NO_ACTION;
    }
    InputReturnValue ret = INPUT_NO_ACTION;
    if (me.event == wm_mouse_event::PRESS)
    {
        if (me.button == wm_mouse_event::LEFT)
        {
            if (find_item != nullptr)
            {
                select_item(find_item);
                if (find_item->selected())
                    ret = INPUT_SELECTED;
                else
                    ret = INPUT_DESELECTED;
            }
        }
        else if (me.button == wm_mouse_event::RIGHT)
            ret = INPUT_END_MENU_ABORT;
    }
    // all the other Mouse Events are uninteresting and are ignored
    return ret;
}
#endif

void MenuFreeform::render()
{
    if (!m_visible)
        return;

    if (m_dirty)
        _place_items();

    for (MenuItem *item : m_entries)
        item->render();
}

/**
 * Handle all the dirtyness here that the MenuItems themselves do not handle
 */
void MenuFreeform::_place_items()
{
    m_dirty = false;
}

MenuItem* MenuFreeform::get_active_item()
{
    return m_active_item;
}

/**
 * Sets item by ID
 * Clears active item if ID not found
 */
void MenuFreeform::set_active_item(int ID)
{
    auto it = find_if(m_entries.begin(), m_entries.end(),
            [=](const MenuItem* item) { return item->get_id() == ID; });
    m_active_item = (it != m_entries.end()) ? *it : nullptr;
    m_dirty = true;
}

/**
 * Sets active item based on index
 * This function is for internal use if object does not have ID set
 */
void MenuFreeform::_set_active_item(MenuItem* item)
{
    ASSERT(!item || item->can_be_highlighted());
    m_active_item = item;
    m_dirty = true;
}

void MenuFreeform::set_active_item(MenuItem* item)
{
    bool present = find(m_entries.begin(), m_entries.end(), item) != m_entries.end();
    m_active_item = (present && item->can_be_highlighted()) ? item : nullptr;
    m_dirty = true;
}

void MenuFreeform::activate_first_item()
{
    auto el = find_if(m_entries.begin(), m_entries.end(),
            [=](const MenuItem* item) { return item->can_be_highlighted(); });
    if (el != m_entries.end())
        _set_active_item(*el);
}

void MenuFreeform::activate_last_item()
{
    auto el = find_if(m_entries.rbegin(), m_entries.rend(),
            [=](const MenuItem* item) { return item->can_be_highlighted(); });
    if (el != m_entries.rend())
        _set_active_item(*el);
}

bool MenuFreeform::select_item(int index)
{
    if (index >= 0 && index < static_cast<int> (m_entries.size()))
    {
        // Flip the selection flag
        m_entries.at(index)->select(!m_entries.at(index)->selected());
    }
    return m_entries.at(index)->selected();
}

bool MenuFreeform::select_item(MenuItem* item)
{
    ASSERT(item != nullptr);

    // Is the given item in menu?
    auto find_val = find(m_entries.begin(), m_entries.end(), item);
    if (find_val != m_entries.end())
    {
        // Flip the selection flag
        item->select(!item->selected());
    }
    return item->selected();
}

bool MenuFreeform::attach_item(MenuItem* item)
{
    // is the item inside boundaries?
    if (   item->get_min_coord().x < m_min_coord.x
        || item->get_min_coord().x > m_max_coord.x
        || item->get_min_coord().y < m_min_coord.y
        || item->get_min_coord().y > m_max_coord.y
        || item->get_max_coord().x < m_min_coord.x
        || item->get_max_coord().x > m_max_coord.x
        || item->get_max_coord().y < m_min_coord.y
        || item->get_max_coord().y > m_max_coord.y)
    {
        return false;
    }
    // It's inside boundaries

    m_entries.push_back(item);
    return true;
}

/**
 * Finds the closest rectangle to given entry begin_index on a caardinal
 * direction from it.
 * if no entries are found, -1 is returned
 */
MenuItem* MenuFreeform::_find_item_by_direction(const MenuItem* start,
                                                MenuObject::Direction dir)
{
    if (start == nullptr)
        return nullptr;

    coord_def aabb_start(0,0);
    coord_def aabb_end(0,0);

    // construct the aabb
    switch (dir)
    {
    case UP:
        if (start->get_link_up())
            return start->get_link_up();

        aabb_start.x = start->get_min_coord().x;
        aabb_end.x = start->get_max_coord().x;
        aabb_start.y = 0; // top of screen
        aabb_end.y = start->get_min_coord().y;
        break;
    case DOWN:
        if (start->get_link_down())
            return start->get_link_down();

        aabb_start.x = start->get_min_coord().x;
        aabb_end.x = start->get_max_coord().x;
        aabb_start.y = start->get_max_coord().y;
        // we choose an arbitrarily large number here, because
        // tiles saves entry coordinates in pixels, yet console saves them
        // in characters
        // basically, we want the AABB to be large enough to extend to the
        // bottom of the screen in every possible resolution
        aabb_end.y = 32767;
        break;
    case LEFT:
        if (start->get_link_left())
            return start->get_link_left();

        aabb_start.x = 0; // left of screen
        aabb_end.x = start->get_min_coord().x;
        aabb_start.y = start->get_min_coord().y;
        aabb_end.y = start->get_max_coord().y;
        break;
    case RIGHT:
        if (start->get_link_right())
            return start->get_link_right();

        aabb_start.x = start->get_max_coord().x;
        // we again want a value that is always larger then the width of screen
        aabb_end.x = 32767;
        aabb_start.y = start->get_min_coord().y;
        aabb_end.y = start->get_max_coord().y;
        break;
    default:
        die("Bad direction given");
    }

    // loop through the entries
    // save the currently closest to the index in a variable
    MenuItem* closest = nullptr;
    for (MenuItem *item : m_entries)
    {
        if (!item->can_be_highlighted())
        {
            // this is a noselect entry, skip it
            continue;
        }
        if (!item->is_visible())
        {
            // this item is not visible, skip it
            continue;
        }
        if (!_AABB_intersection(item->get_min_coord(), item->get_max_coord(),
                                aabb_start, aabb_end))
        {
            continue; // does not intersect, continue loop
        }

        // intersects
        // check if it's closer than current
        if (closest == nullptr)
            closest = item;

        switch (dir)
        {
        case UP:
            if (item->get_min_coord().y > closest->get_min_coord().y)
                closest = item;
            break;
        case DOWN:
            if (item->get_min_coord().y < closest->get_min_coord().y)
                closest = item;
            break;
        case LEFT:
            if (item->get_min_coord().x > closest->get_min_coord().x)
                closest = item;
            break;
        case RIGHT:
            if (item->get_min_coord().x < closest->get_min_coord().x)
                closest = item;
        }
    }
    // TODO handle special cases here, like pressing down on the last entry
    // to go the the first item in that line
    return closest;
}

BoxMenuHighlighter::BoxMenuHighlighter(PrecisionMenu *parent): m_parent(parent),
    m_active_item(nullptr)
{
    ASSERT(parent != nullptr);
}

BoxMenuHighlighter::~BoxMenuHighlighter()
{
}

vector<MenuItem*> BoxMenuHighlighter::get_selected_items()
{
    vector<MenuItem*> ret_val;
    return ret_val;
}

MenuObject::InputReturnValue BoxMenuHighlighter::process_input(int /*key*/)
{
    // just in case we somehow end up processing input of this item
    return MenuObject::INPUT_NO_ACTION;
}

#ifdef USE_TILE_LOCAL
MenuObject::InputReturnValue BoxMenuHighlighter::handle_mouse(const wm_mouse_event &/*me*/)
{
    // we have nothing interesting to do on mouse events because render()
    // always checks if the active has changed
    return MenuObject::INPUT_NO_ACTION;
}
#endif

void BoxMenuHighlighter::render()
{
    if (!m_visible)
        return;

    if (!m_visible)
        return;
    _place_items();
#ifdef USE_TILE_LOCAL
    m_line_buf.draw();
    m_shape_buf.draw();
#else
    if (m_active_item != nullptr)
        m_active_item->render();
#endif
}

void BoxMenuHighlighter::_place_items()
{
    MenuItem* tmp = m_parent->get_active_item();
    if (tmp == m_active_item)
        return;

#ifdef USE_TILE_LOCAL
    m_line_buf.clear();
    m_shape_buf.clear();
    if (tmp != nullptr)
    {
        const VColour& c = term_colours[tmp->get_highlight_colour()];
        const VColour bg_colour(c.r, c.g, c.b, 80);
        const VColour line_colour(c.r, c.g, c.b, 127);
        const coord_def tl = tmp->get_min_coord() + coord_def(1, 1);
        const coord_def br = tmp->get_max_coord();
        m_line_buf.add_square(tl.x, tl.y, br.x, br.y, line_colour);
        m_shape_buf.add(tl.x, tl.y, br.x, br.y, bg_colour);
    }
#else
    // we had an active item before
    if (m_active_item != nullptr)
    {
        // clear the background highlight trickery
        m_active_item->set_bg_colour(m_old_bg_colour);
        // redraw the old item
        m_active_item->render();
    }
    if (tmp != nullptr)
    {
        m_old_bg_colour = tmp->get_bg_colour();
        tmp->set_bg_colour(tmp->get_highlight_colour());
    }
#endif
    m_active_item = tmp;
}
