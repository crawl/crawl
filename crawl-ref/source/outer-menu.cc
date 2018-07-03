/**
 * @file
 * @brief Menu button used in non-game menus
**/

#include "AppHdr.h"

#include "cio.h"
#include "outer-menu.h"
#include "tileweb.h"

using namespace ui;

bool MenuButton::focus_on_mouse = false;

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
        m_buf.add(m_region[0], m_region[1],
                m_region[0]+m_region[2], m_region[1]+m_region[3], bg);
        const VColour mouse_colour = active ?
            VColour(34, 34, 34, 255) : term_colours[highlight_colour];
        m_line_buf.add_square(m_region[0]+1, m_region[1]+1,
                m_region[0]+m_region[2], m_region[1]+m_region[3], mouse_colour);
    }
#endif
}

bool MenuButton::on_event(const wm_event& event)
{
    if (Bin::on_event(event))
        return true;

    bool old_focused = focused, old_active = active;

    if (event.type == WME_MOUSEENTER && focus_on_mouse)
        ui::set_focused_widget(this);
    if (event.type == WME_MOUSEMOTION)
        ui::set_focused_widget(this);

    if (event.type == WME_FOCUSIN)
        focused = true;
    if (event.type == WME_FOCUSOUT)
        focused = false;

#ifdef USE_TILE_LOCAL
    if (event.type == WME_MOUSEENTER)
        wm->set_mouse_cursor(MOUSE_CURSOR_POINTER);
    if (event.type == WME_MOUSELEAVE)
        wm->set_mouse_cursor(MOUSE_CURSOR_ARROW);
#endif

    if (event.type == WME_MOUSEBUTTONDOWN && event.mouse_event.button == MouseEvent::LEFT)
        active = true;
    if (event.type == WME_MOUSEBUTTONUP && event.mouse_event.button == MouseEvent::LEFT)
        active = false;

    if (old_focused != focused || old_active != active)
        _queue_allocation();

    return old_active != active;
}

OuterMenu::OuterMenu(bool can_shrink, int width, int height)
{
    m_grid = make_shared<Grid>();
    m_grid->stretch_h = true;

    m_grid->on(Widget::slots.event, [this](wm_event ev){
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

    m_root->_set_parent(this);

    m_width = width;
    m_height = height;
    m_buttons.resize(width * height, nullptr);
}

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
    if (!have_allocated && on_button_activated)
    {
        have_allocated = true;
        Layout *layout = nullptr;
        for (Widget *w = _get_parent(); w && !layout; w = w->_get_parent())
            layout = dynamic_cast<Layout*>(w);
        ASSERT(layout);
        layout->add_event_filter([this](wm_event ev) {
            if (ev.type != WME_KEYDOWN)
                return false;
            for (const auto& btn : this->m_buttons)
                if (btn && ev.key.keysym.sym == btn->hotkey)
                {
                    this->on_button_activated(btn->id);
                    return true;
                }
            return false;
        });
        if (m_initial_focus)
            scroll_button_into_view(m_initial_focus);
    }
    m_root->allocate_region(m_region);
}

bool OuterMenu::on_event(const wm_event& event)
{
    return Widget::on_event(event);
}

void OuterMenu::add_label(shared_ptr<Text> label, int x, int y)
{
    ASSERT(x >= 0 && x < m_width);
    ASSERT(y >= 0 && y < m_height);
    ASSERT(m_buttons[y*m_width + x] == nullptr);
    m_grid->add_child(move(label), x, y);
}

void OuterMenu::add_button(shared_ptr<MenuButton> btn, int x, int y)
{
    ASSERT(x >= 0 && x < m_width);
    ASSERT(y >= 0 && y < m_height);

    MenuButton*& item = m_buttons[y*m_width + x];
    ASSERT(item == nullptr);
    item = btn.get();

    const int btn_id = btn->id;
    btn->on(Widget::slots.event, [btn_id, this, x, y](const wm_event& ev) {
        if (on_button_activated)
        if ((ev.type == WME_MOUSEBUTTONUP
                && ev.mouse_event.button == MouseEvent::LEFT)
            || (ev.type == WME_KEYDOWN && ev.key.keysym.sym == CK_ENTER))
        {
            this->on_button_activated(btn_id);
            return ev.type == WME_KEYDOWN; // button needs to catch clicks
        }
#ifndef USE_TILE_LOCAL
        if (ev.type == WME_FOCUSIN || ev.type == WME_FOCUSOUT)
        {
            auto btn2 = this->m_buttons[y*this->m_width+x];
            if (auto tw = dynamic_cast<Text*>(btn2->get_child().get()))
                tw->set_bg_colour(ev.type == WME_FOCUSIN ? LIGHTGREY : BLACK);
        }
#endif
        return false;
    });

    m_grid->add_child(move(btn), x, y);
}

void OuterMenu::scroll_button_into_view(MenuButton *btn)
{
    ui::set_focused_widget(btn);
    Widget* gp = btn->_get_parent()->_get_parent();
    Scroller* scroller = dynamic_cast<Scroller*>(gp);
    if (!scroller)
        return;
    i4 btn_reg = btn->get_region(), scr_reg = scroller->get_region();

#ifdef USE_TILE_LOCAL
    constexpr int shade_height = 12;
#else
    constexpr int shade_height = 0;
#endif

    const int btn_top = btn_reg[1], btn_bot = btn_top + btn_reg[3],
                scr_top = scr_reg[1] + shade_height,
                scr_bot = scr_reg[1] + scr_reg[3] - shade_height;
    const int delta = btn_top < scr_top ? btn_top - scr_top :
                btn_bot > scr_bot ? btn_bot - scr_bot : 0;
    scroller->set_scroll(scroller->get_scroll() + delta);
}

bool OuterMenu::move_button_focus(int fx, int fy, int dx, int dy)
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
            return linked->move_button_focus(lfx, lfy, dx, dy);
        }
    }
    return btn != nullptr;
}

bool OuterMenu::scroller_event_hook(const wm_event& ev)
{
    MenuButton::focus_on_mouse = ev.type == WME_MOUSEWHEEL
            || ev.type == WME_KEYDOWN && ev.key.keysym.sym == CK_PGDN
            || ev.type == WME_KEYDOWN && ev.key.keysym.sym == CK_PGUP;

    if (ev.type != WME_KEYDOWN)
        return false;
    int key = ev.key.keysym.sym;

    if (key == CK_DOWN || key == CK_UP || key == CK_LEFT || key == CK_RIGHT)
    {
        const int dx = key == CK_LEFT ? -1 : key == CK_RIGHT ? 1 : 0;
        const int dy = key == CK_UP ? -1 : key == CK_DOWN ? 1 : 0;
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

        return move_button_focus(fx, fy, dx, dy);
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
