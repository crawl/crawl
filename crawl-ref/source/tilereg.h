#ifdef USE_TILE_LOCAL
#pragma once

class ImageManager;
struct MouseEvent;

class Region
{
public:
    Region();
    virtual ~Region();

    void resize(int mx, int my);
    void place(int sx, int sy, int margin);
    void place(int sx, int sy, int ex, int ey, int margin);
    void place(int sx, int sy);
    void resize_to_fit(int wx, int wy);

    // Returns true if the mouse position is over the region
    // If true, then cx and cy are set in the range [0..mx-1], [0..my-1]
    virtual bool mouse_pos(int mouse_x, int mouse_y, int &cx, int &cy);

    virtual bool inside(int px, int py);
    virtual bool update_tip_text(string &tip) { return false; }
    virtual bool update_alt_text(string &alt) { return false; }
    virtual int handle_mouse(MouseEvent &event) = 0;

    virtual void render() = 0;
    virtual void clear() = 0;

    const virtual int grid_width_to_pixels(int x) const;
    const virtual int grid_height_to_pixels(int y) const;

    // Geometry
    // <-----------------wx----------------------->
    // sx     ox                                ex
    // |margin| text/tile area            |margin|

    // Offset in pixels
    int ox;
    int oy;

    // Unit size
    int dx;
    int dy;

    // Region size in dx/dy
    int mx;
    int my;

    // Width of the region in pixels
    int wx;
    int wy;

    // Start position in pixels (top left)
    int sx;
    int sy;

    // End position in pixels (bottom right)
    int ex;
    int ey;

protected:
    virtual void recalculate();
    virtual void calculate_grid_size(int inner_x, int inner_y);
    virtual void on_resize() = 0;
    void set_transform(bool no_scaling = false);
};

class FontWrapper;

// A convenience class for holding all the data that TileRegion and
// derived classes need in their constructors.
class TileRegionInit
{
public:
    TileRegionInit(ImageManager *_im, FontWrapper *_font, int _tx, int _ty) :
        im(_im), tag_font(_font), tile_x(_tx), tile_y(_ty) { }
    TileRegionInit() { }

    ImageManager *im;
    FontWrapper *tag_font;
    int tile_x;
    int tile_y;
};

class TileRegion : public Region
{
public:
    TileRegion(const TileRegionInit &init);
    ~TileRegion();

protected:
    ImageManager *m_image;
    FontWrapper *m_tag_font;
    bool m_dirty;
};

// An abstract tiles-only region that takes control over all input.
class ControlRegion : public Region
{
public:
    virtual void run() = 0;
};

#endif
