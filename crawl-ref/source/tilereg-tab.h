#ifdef USE_TILE_LOCAL
#pragma once

#include "command-type.h"
#include "tilereg-grid.h"

// A region that contains multiple region, selectable by tabs.
class TabbedRegion : public GridRegion
{
public:
    TabbedRegion(const TileRegionInit &init);

    virtual ~TabbedRegion();

    enum
    {
        TAB_OFS_UNSELECTED,
        TAB_OFS_MOUSEOVER,
        TAB_OFS_SELECTED,
        TAB_OFS_MAX
    };

    int push_tab_region(GridRegion *reg, tileidx_t tile_tab);
    int push_tab_button(command_type cmd, tileidx_t tile_tab);
    GridRegion *get_tab_region(int idx);
    tileidx_t get_tab_tile(int idx);
    void activate_tab(int idx);
    void deactivate_tab();
    int active_tab() const;
    int num_tabs() const;
    int min_height_for_items() const;
    void enable_tab(int idx);
    void disable_tab(int idx);
    int find_tab(string tab_name) const;

    void set_small_layout(bool use_small_layout, const coord_def &windowsz);

    virtual void update() override;
    virtual void clear() override;
    virtual void render() override;
    virtual void on_resize() override;
    virtual int handle_mouse(MouseEvent &event) override;
    virtual bool update_tip_text(string &tip) override;
    virtual bool update_tab_tip_text(string &tip, bool active) override;
    virtual bool update_alt_text(string &alt) override;

    virtual const string name() const override { return ""; }

protected:
    virtual void pack_buffers() override;
    virtual void draw_tag() override;
    virtual void activate() override {}

    bool invalid_index(int idx) const;
    bool active_is_valid() const;
    // Returns the tab the mouse is over, -1 if none.
    int get_mouseover_tab(MouseEvent &event) const;
    void set_icon_pos(int idx);
    void reset_icons(int from_idx);

    int m_active;
    int m_mouse_tab;
    bool m_use_small_layout;
    bool m_is_deactivated;
    TileBuffer m_buf_gui;

    struct TabInfo
    {
        GridRegion *reg;
        command_type cmd;
        tileidx_t tile_tab;
        int ofs_y;
        int min_y;
        int max_y;
        int height;
        bool enabled;
    };
    vector<TabInfo> m_tabs;

private:
    int _push_tab(GridRegion *reg, command_type cmd, tileidx_t tile_tab);
};

#endif
