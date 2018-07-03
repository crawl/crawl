/**
 * @file
 * @brief Menu button used in non-game menus
**/

#pragma once

#include "ui.h"
#include "tilefont.h"

class MenuButton : public ui::Bin
{
    friend class OuterMenu;
public:
    MenuButton() {};

    virtual void _render() override;
    virtual ui::SizeReq _get_preferred_size(ui::Widget::Direction, int) override;
    virtual void _allocate_region() override;
    virtual bool on_event(const wm_event& event) override;

    int id = 0;
    int hotkey = 0;
    COLOURS highlight_colour = LIGHTGREY;

protected:
    bool focused = false;
    bool active = false;
#ifdef USE_TILE_LOCAL
    ShapeBuffer m_buf;
    LineBuffer m_line_buf;
#endif
    static bool focus_on_mouse;
};

class OuterMenu : public ui::Widget
{
public:
    OuterMenu(bool can_shrink, int width, int height);

    virtual void _render() override;
    virtual ui::SizeReq _get_preferred_size(ui::Widget::Direction, int) override;
    virtual void _allocate_region() override;
    virtual bool on_event(const wm_event& event) override;

    virtual shared_ptr<Widget> get_child_at_offset(int x, int y) override {
        return m_root;
    }

    void add_button(shared_ptr<MenuButton> btn, int x, int y);
    void add_label(shared_ptr<ui::Text> label, int x, int y);
    MenuButton* get_button(int x, int y);
    MenuButton* get_button_by_id(int id);
    const vector<MenuButton*>& get_buttons() { return m_buttons; };
    void set_initial_focus(MenuButton *btn) {
        ASSERT(!have_allocated);
        m_initial_focus = btn;
    };

    void scroll_button_into_view(MenuButton *btn);

    function<void(int)> on_button_activated;

    weak_ptr<OuterMenu> linked_menus[4];

protected:
    bool scroller_event_hook(const wm_event& ev);
    bool move_button_focus(int fx, int fy, int dx, int dy);

    shared_ptr<ui::Grid> m_grid;
    shared_ptr<ui::Widget> m_root;
    vector<MenuButton*> m_buttons;
    int m_width;
    int m_height;
    MenuButton* m_initial_focus {nullptr};

    bool have_allocated {false};
};
