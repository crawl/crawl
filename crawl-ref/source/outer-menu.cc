/**
 * @file
 * @brief Menu button used in non-game menus
**/

#include "AppHdr.h"

#include <unordered_map>

#include "cio.h"
#include "outer-menu.h"
#include "tilepick.h"
#include "tileweb.h"
#include "unwind.h"

using namespace ui;

bool OuterMenu::focus_button_on_mouseenter = false;

MenuButton::MenuButton()
{
    on_hotkey_event([this](const KeyEvent& event) {
        if (numpad_to_regular(event.key(), true) == hotkey)
            return activate();
        return false;
    });
}

bool MenuButton::activate()
{
    auto ev = ActivateEvent();
    ev.set_target(get_shared());
    return ui::raise_event(ev);
}

void MenuButton::_render()
{
#ifdef USE_TILE_LOCAL
    m_buf.draw();
    m_line_buf.draw();
#endif
    m_child->render();
}

SizeReq MenuButton::_get_preferred_size(Direction dim, int prosp_width)
{
    return m_child->get_preferred_size(dim, prosp_width);
}

void MenuButton::_allocate_region()
{
    m_child->allocate_region(m_region);
#ifdef USE_TILE_LOCAL
    m_buf.clear();
    m_line_buf.clear();
    if (focused)
    {
        const VColour bg = active ?
            VColour(0, 0, 0, 255) : VColour(255, 255, 255, 25);
        m_buf.add(m_region.x, m_region.y, m_region.ex(), m_region.ey(), bg);
    }

    const auto darken = [](VColour c) {
        c.r /= 2;
        c.g /= 2;
        c.b /= 2;
        return c;
    };

    const VColour border = active ? VColour(34, 34, 34, 255) :
        focused ? term_colours[highlight_colour] :
        hovered ? darken(term_colours[highlight_colour]) :
        VColour(0, 0, 0, 0);
    m_line_buf.add_square(m_region.x+1, m_region.y+1,
        m_region.ex(), m_region.ey(), border);
#endif
}

#ifndef USE_TILE_LOCAL
void MenuButton::recolour_descendants(const shared_ptr<Widget>& node)
{
    auto tw = dynamic_pointer_cast<Text>(node);
    if (tw)
    {
        if (focused)
        {
            // keep the original colour so we can restore it on unfocus
            const auto& first_op = tw->get_text().ops[0];
            fg_normal = first_op.type == FSOP_COLOUR ? first_op.colour : LIGHTGREY;
        }

        const colour_t fg = focused ? fg_highlight : fg_normal;
        const colour_t bg = focused ? highlight_colour : colour_t{BLACK};
        formatted_string new_contents;
        new_contents.textcolour(fg);
        new_contents.cprintf("%s", tw->get_text().tostring().c_str());
        tw->set_text(move(new_contents));
        tw->set_bg_colour(static_cast<COLOURS>(bg));
        return;
    }
    node->for_each_child([this](shared_ptr<Widget>& child) {
        recolour_descendants(child);
    });
}
#endif

bool MenuButton::on_event(const Event& event)
{
    if (Bin::on_event(event))
        return true;

    bool old_focused = focused, old_active = active, old_hovered = hovered;

    if (event.type() == Event::Type::MouseEnter)
        hovered = true;
    if (event.type() == Event::Type::MouseLeave)
        hovered = false;
    if (event.type() == Event::Type::FocusIn)
        focused = true;
    if (event.type() == Event::Type::FocusOut)
        focused = false;

#ifndef USE_TILE_LOCAL
    if (event.type() == Event::Type::FocusIn || event.type() == Event::Type::FocusOut)
        recolour_descendants(shared_from_this());
#endif

#ifdef USE_TILE_LOCAL
    if (event.type() == Event::Type::MouseEnter)
        wm->set_mouse_cursor(MOUSE_CURSOR_POINTER);
    if (event.type() == Event::Type::MouseLeave)
        wm->set_mouse_cursor(MOUSE_CURSOR_ARROW);
#endif

    if (event.type() == Event::Type::MouseDown
        && static_cast<const MouseEvent&>(event).button() == MouseEvent::Button::Left)
    {
        active = true;
    }
    if (event.type() == Event::Type::MouseUp
        && static_cast<const MouseEvent&>(event).button() == MouseEvent::Button::Left)
    {
        active = false;
    }

    if (old_focused != focused || old_active != active || old_hovered != hovered)
        _queue_allocation();

    const bool left_mouse_button = old_active && !active;
    if (left_mouse_button)
        return activate();
    else if (event.type() == Event::Type::KeyDown)
    {
        const auto key = numpad_to_regular(static_cast<const KeyEvent&>(event).key(), true);
        if (key == CK_ENTER || key == ' ')
            return activate();
    }

    return old_active != active;
}

#ifdef USE_TILE_WEB
static void serialize_image(const Image* image)
{
    tile_def tile = image->get_tile();
    tiles.json_open_object();
    tiles.json_write_int("t", tile.tile);
    tiles.json_write_int("tex", get_tile_texture(tile.tile));
    if (tile.ymax != TILE_Y)
        tiles.json_write_int("ymax", tile.ymax);
    tiles.json_close_object();
}

void MenuButton::serialize()
{
    tiles.json_write_string("description", description);
    tiles.json_write_int("hotkey", hotkey);
    tiles.json_write_int("highlight_colour", highlight_colour);
    if (auto text = dynamic_cast<Text*>(m_child.get()))
        tiles.json_write_string("label", text->get_text().to_colour_string(LIGHTGREY));
    else if (auto box = dynamic_cast<Box*>(m_child.get()))
    {
        if (box->num_children() <= 1)
            return;
        tiles.json_open_array("tile");
        if (auto tile = dynamic_cast<Image*>((*box)[0].get()))
            serialize_image(tile);
        else if (auto tilestack = dynamic_cast<Stack*>(((*box)[0].get())))
        {
            for (const auto &it : *tilestack)
                if (auto t = dynamic_cast<Image*>(it.get()))
                    serialize_image(t);
        }
        tiles.json_close_array();

        tiles.json_open_array("labels");
        for (size_t i = 1; i < box->num_children(); i++)
            if (auto text2 = dynamic_cast<Text*>((*box)[i].get()))
                tiles.json_write_string(text2->get_text().to_colour_string(LIGHTGREY));
        tiles.json_close_array();
    }
}
#endif

OuterMenu::OuterMenu(bool can_shrink, int width, int height)
{
    m_grid = make_shared<Grid>();
    m_grid->stretch_h = true;

    m_grid->on_any_event([this](const Event& ev) {
        return this->scroller_event_hook(ev);
    });

    if (can_shrink)
    {
        auto scroller = make_shared<Scroller>();
        scroller->set_child(m_grid);
        m_root = move(scroller);
    }
    else
        m_root = m_grid;

    add_internal_child(m_root);

    m_width = width;
    m_height = height;
    m_buttons.resize(width * height, nullptr);
    m_description_indexes.resize(width * height, -1);
}

#ifdef USE_TILE_WEB
static bool from_client {false};
static unordered_map<string, OuterMenu*> open_menus;

OuterMenu::~OuterMenu()
{
    if (menu_id)
        open_menus.erase(string(menu_id));
}
#endif

void OuterMenu::_render()
{
    m_root->render();
}

SizeReq OuterMenu::_get_preferred_size(Direction dim, int prosp_width)
{
    return m_root->get_preferred_size(dim, prosp_width);
}

void OuterMenu::_allocate_region()
{
    if (!have_allocated)
    {
        have_allocated = true;
#ifdef USE_TILE_WEB
        if (menu_id)
            open_menus.emplace(menu_id, this);
#endif
        if (m_initial_focus)
            scroll_button_into_view(m_initial_focus);
    }
    m_root->allocate_region(m_region);
}

bool OuterMenu::on_event(const Event& event)
{
    return Widget::on_event(event);
}

void OuterMenu::add_label(shared_ptr<Text> label, int x, int y)
{
    ASSERT(x >= 0 && x < m_width);
    ASSERT(y >= 0 && y < m_height);
    ASSERT(m_buttons[y*m_width + x] == nullptr);
    m_labels.emplace_back(label->get_text(), coord_def(x, y));
    m_grid->add_child(move(label), x, y);
}

void OuterMenu::add_button(shared_ptr<MenuButton> btn, int x, int y)
{
    ASSERT(x >= 0 && x < m_width);
    ASSERT(y >= 0 && y < m_height);

    MenuButton*& item = m_buttons[y*m_width + x];
    ASSERT(item == nullptr);
    item = btn.get();

    // TODO: once we add event capturing, move handlers to menu
    btn->on_focusin_event([this, x, y](const Event&) {
        if (m_description_indexes[y*this->m_width+x] != -1)
            descriptions->current() = m_description_indexes[y*this->m_width+x];
        return false;
    });
    btn->on_mouseenter_event([btn](const MouseEvent&) {
        if (focus_button_on_mouseenter)
            ui::set_focused_widget(btn.get());
        return false;
    });
    btn->on_mousemove_event([btn](const MouseEvent&) {
        ui::set_focused_widget(btn.get());
        return false;
    });

    if (descriptions)
    {
        auto desc_text = make_shared<Text>(formatted_string(btn->description, WHITE));
        desc_text->set_wrap_text(true);
        descriptions->add_child(move(desc_text));
        m_description_indexes[y*m_width + x] = descriptions->num_children()-1;
    }

    Widget *r;
    for (Widget *p = btn.get(); p; p = p->_get_parent())
        r = p;
    m_grid->add_child(r->get_shared(), x, y);
}

void OuterMenu::scroll_button_into_view(MenuButton *btn)
{
    ui::set_focused_widget(btn);

#ifdef USE_TILE_WEB
    if (menu_id)
    {
        tiles.json_open_object();
        tiles.json_write_bool("from_client", from_client);
        tiles.json_write_string("menu_id", menu_id);
        tiles.json_write_int("button_focus", btn->hotkey);
        tiles.ui_state_change("newgame-choice", 0);
    }
#endif

    Widget* gp = btn->_get_parent()->_get_parent();
    Scroller* scroller = dynamic_cast<Scroller*>(gp);
    if (!scroller)
        return;
    const auto btn_reg = btn->get_region(), scr_reg = scroller->get_region();

#ifdef USE_TILE_LOCAL
    constexpr int shade_height = 12;
#else
    constexpr int shade_height = 0;
#endif

    const int btn_top = btn_reg.y, btn_bot = btn_top + btn_reg.height,
                scr_top = scr_reg.y + shade_height,
                scr_bot = scr_reg.y + scr_reg.height - shade_height;
    const int delta = btn_top < scr_top ? btn_top - scr_top :
                btn_bot > scr_bot ? btn_bot - scr_bot : 0;
    scroller->set_scroll(scroller->get_scroll() + delta);
}

bool OuterMenu::move_button_focus(int fx, int fy, int dx, int dy, int limit)
{
    MenuButton* btn = nullptr;
    int nx, ny;
    for (nx = fx+dx, ny = fy+dy;
            nx >= 0 && nx < m_width && ny >= 0 && ny < m_height;
            nx += dx, ny += dy)
    {
        if (m_buttons[ny*m_width + nx])
        {
            btn = m_buttons[ny*m_width + nx];
            if (--limit == 0)
                break;
        }
    }

    if (btn)
        scroll_button_into_view(btn);
    else
    {
        int idx = dx == 0 ? 1+dy : 2-dx; // dx,dy --> CSS direction index
        if (auto linked = linked_menus[idx].lock())
        {
            int lfx = dx == 0 ? min(fx, linked->m_width-1) : dx < 0 ? linked->m_width : -1;
            int lfy = dy == 0 ? min(fy, linked->m_height-1) : dy < 0 ? linked->m_height : -1;
            return linked->move_button_focus(lfx, lfy, dx, dy, 1);
        }
    }
    return btn != nullptr;
}

bool OuterMenu::scroller_event_hook(const Event& ev)
{
    focus_button_on_mouseenter = ev.type() == Event::Type::MouseWheel;

    if (ev.type() != Event::Type::KeyDown)
        return false;

    const auto key = numpad_to_regular(static_cast<const KeyEvent&>(ev).key(), true);

    if (key == CK_DOWN || key == CK_UP || key == CK_LEFT || key == CK_RIGHT
            || key == CK_HOME || key == CK_END || key == CK_PGUP || key == CK_PGDN)
    {
        const int dx = key == CK_LEFT ? -1 : key == CK_RIGHT ? 1 : 0;
        const int dy = key == CK_UP || key == CK_PGUP || key == CK_HOME ? -1
            : key == CK_DOWN || key == CK_PGDN || key == CK_END ? 1 : 0;
        ASSERT((dx == 0) != (dy == 0));

        const Widget* focus = ui::get_focused_widget();
        int fx = -1, fy = -1;
        for (int x = 0; x < m_width; x++)
            for (int y = 0; y < m_height; y++)
                if (focus == m_buttons[y*m_width + x])
                {
                    fx = x;
                    fy = y;
                    break;
                }
        ASSERT(fx >= 0 && fy >= 0);

        const int pagesz = m_root->get_region().height / focus->get_region().height - 1;
        const int limit = (key == CK_HOME || key == CK_END) ? INT_MAX
            : (key == CK_PGUP || key == CK_PGDN) ? pagesz : 1;

        return move_button_focus(fx, fy, dx, dy, limit);
    }

    return false;
}

MenuButton* OuterMenu::get_button(int x, int y)
{
    ASSERT(x >= 0 && x < m_width);
    ASSERT(y >= 0 && y < m_height);
    return m_buttons[y*m_width + x];
}

MenuButton* OuterMenu::get_button_by_id(int id)
{
    auto it = find_if(m_buttons.begin(), m_buttons.end(),
            [id](MenuButton*& btn) { return btn && btn->id == id; });
    return it == m_buttons.end() ? nullptr : *it;
}

#ifdef USE_TILE_WEB
void OuterMenu::serialize(string name)
{
    tiles.json_open_object(name);
    tiles.json_write_string("menu_id", menu_id);
    tiles.json_write_int("width", m_width);
    tiles.json_write_int("height", m_height);
    tiles.json_open_array("buttons");
    for (size_t i = 0; i < m_buttons.size(); i++)
    {
        if (!m_buttons[i])
            continue;
        tiles.json_open_object();
        tiles.json_write_int("x", i % m_width);
        tiles.json_write_int("y", i / m_width);
        m_buttons[i]->serialize();
        tiles.json_close_object();
    }
    tiles.json_close_array();
    tiles.json_open_array("labels");
    for (auto label : m_labels)
    {
        tiles.json_open_object();
        tiles.json_write_int("x", label.second.x);
        tiles.json_write_int("y", label.second.y);
        tiles.json_write_string("label", label.first.to_colour_string(LIGHTGREY));
        tiles.json_close_object();
    }
    tiles.json_close_array();
    tiles.json_close_object();
}

void OuterMenu::recv_outer_menu_focus(const char *menu_id, int hotkey)
{
    auto open_menu = open_menus.find(menu_id);
    if (open_menu == open_menus.end())
        return;

    unwind_bool tmp(from_client, true);

    auto menu = open_menu->second;
    for (auto btn : menu->m_buttons)
        if (btn && btn->hotkey == hotkey)
            menu->scroll_button_into_view(btn);
}
#endif
