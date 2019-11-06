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
    MenuButton();

    virtual void _render() override;
    virtual ui::SizeReq _get_preferred_size(ui::Widget::Direction, int) override;
    virtual void _allocate_region() override;
    virtual bool on_event(const ui::Event& event) override;

    int id = 0;
    int hotkey = 0;
    colour_t highlight_colour = LIGHTGREY;
#ifndef USE_TILE_LOCAL
    colour_t fg_highlight = BLACK;
#endif
    string description;

    bool activate();

#ifdef USE_TILE_WEB
    void serialize();
#endif

protected:
    bool can_take_focus() override { return true; };

    bool focused = false;
    bool hovered = false;
    bool active = false;
#ifdef USE_TILE_LOCAL
    ShapeBuffer m_buf;
    LineBuffer m_line_buf;
#endif

#ifndef USE_TILE_LOCAL
    void recolour_descendants(const shared_ptr<Widget>& node);
    colour_t fg_normal = LIGHTGREY;
#endif
};

class OuterMenu : public ui::Widget
{
public:
    OuterMenu(bool can_shrink, int width, int height);

#ifdef USE_TILE_WEB
    ~OuterMenu();
    void serialize(string name);
    static void recv_outer_menu_focus(const char *menu_id, int hotkey);
#endif

    virtual void _render() override;
    virtual ui::SizeReq _get_preferred_size(ui::Widget::Direction, int) override;
    virtual void _allocate_region() override;
    virtual bool on_event(const ui::Event& event) override;

    virtual shared_ptr<Widget> get_child_at_offset(int, int) override {
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

    weak_ptr<OuterMenu> linked_menus[4];
    shared_ptr<ui::Switcher> descriptions;
    const char *menu_id {nullptr};

protected:
    bool scroller_event_hook(const ui::Event& ev);
    bool move_button_focus(int fx, int fy, int dx, int dy, int limit);

    shared_ptr<ui::Grid> m_grid;
    shared_ptr<ui::Widget> m_root;
    vector<MenuButton*> m_buttons;
    vector<pair<formatted_string, coord_def>> m_labels;
    vector<int> m_description_indexes;
    int m_width;
    int m_height;
    MenuButton* m_initial_focus {nullptr};

    bool have_allocated {false};

    static bool focus_button_on_mouseenter;
};
