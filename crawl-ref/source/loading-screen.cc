#include "AppHdr.h"

#ifdef USE_TILE_LOCAL

#include "loading-screen.h"
#include "files.h"
#include "options.h"
#include "random.h"
#include "state.h"
#include "ui.h"

using namespace ui;

class UIShrinkableImage : public Widget
{
public:
    UIShrinkableImage(string img_path) : m_buf(true, false) {
        m_img.load_texture(img_path.c_str(), MipMapOptions::CREATE);
        m_buf.set_tex(&m_img);
    };
    virtual void _render() override;
    virtual SizeReq _get_preferred_size(Direction dim, int prosp_width) override;
    virtual void _allocate_region() override;
protected:
    float scale;
    GenericTexture m_img;
    VertBuffer m_buf;
};

void UIShrinkableImage::_render()
{
    int iw = m_img.orig_width()*scale, ih = m_img.orig_height()*scale;
    int dx = (m_region.width-iw)/2, dy = (m_region.height-ih)/2;
    int x = m_region.x + dx, y = m_region.y + dy;
    GLWPrim rect(x, y, x+iw, y+ih);
    rect.set_tex(0, 0, (float)m_img.orig_width()/m_img.width(),
            (float)m_img.orig_height()/m_img.height());
    m_buf.clear();
    m_buf.add_primitive(rect);
    m_buf.draw();
}

SizeReq UIShrinkableImage::_get_preferred_size(Direction dim, int /*prosp_width*/)
{
    return { 0, (int)(!dim ? m_img.orig_width() : m_img.orig_height()) };
}

void UIShrinkableImage::_allocate_region()
{
    float iw = m_img.orig_width(), ih = m_img.orig_height();
    scale = min({1.0f, m_region.width/iw, m_region.height/ih});
}

static shared_ptr<Text> loading_text;
static shared_ptr<ui::Popup> popup;
static string load_complete_msg = "Loading complete, press any key to start.";

static const string _get_title_image()
{
    vector<string> files = get_title_files();
    return files[random2(files.size())];
}

void loading_screen_open()
{
    if (!crawl_state.title_screen || in_headless_mode())
        return;
    auto splash = make_shared<UIShrinkableImage>(_get_title_image());
    loading_text = make_shared<Text>();
    loading_text->set_margin_for_sdl(15, 0, 0, 0);
    auto vbox = make_shared<Box>(Widget::VERT);
    vbox->set_cross_alignment(Widget::CENTER);
    vbox->add_child(std::move(splash));
    vbox->add_child(loading_text);
    FontWrapper *font = tiles.get_crt_font();
    vbox->min_size().width = font->string_width(load_complete_msg.c_str());
    popup = make_shared<ui::Popup>(std::move(vbox));
    ui::push_layout(popup);
}

void loading_screen_close()
{
    if (!crawl_state.title_screen || in_headless_mode())
        return;
    bool done = Options.tile_skip_title
        || crawl_state.test || crawl_state.script;
    popup->on_keydown_event([&](const KeyEvent&) { return done = true; });
    if (!done)
        loading_screen_update_msg(load_complete_msg);
    while (!done && !crawl_state.seen_hups)
        ui::pump_events();
    ui::pop_layout();
    loading_text = nullptr;
    popup = nullptr;
}

void loading_screen_update_msg(string message)
{
    if (!crawl_state.title_screen || in_headless_mode())
        return;
    loading_text->set_text(message);
    ui::pump_events(0);
}

#endif
