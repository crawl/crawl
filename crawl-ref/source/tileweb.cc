#include "AppHdr.h"

#ifdef USE_TILE_WEB

#include "artefact.h"
#include "cio.h"
#include "coord.h"
#include "directn.h"
#include "env.h"
#include "files.h"
#include "libutil.h"
#include "macro.h"
#include "map_knowledge.h"
#include "menu.h"
#include "message.h"
#include "mon-util.h"
#include "options.h"
#include "player.h"
#include "state.h"
#include "stuff.h"
#include "tiledef-dngn.h"
#include "tiledef-gui.h"
#include "tiledef-main.h"
#include "tiledef-player.h"
#include "tiledef-icons.h"
#include "tilemcache.h"
#include "tilepick.h"
#include "tilepick-p.h"
#include "tileweb.h"
#include "tileview.h"
#include "travel.h"
#include "unicode.h"
#include "view.h"
#include "viewgeom.h"

#include "json.h"

#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdarg.h>
#include <errno.h>



static unsigned int get_milliseconds()
{
    // This is Unix-only, but so is Webtiles at the moment.
    timeval tv;
    gettimeofday(&tv, NULL);

    return ((unsigned int) tv.tv_sec) * 1000 + tv.tv_usec / 1000;
}


// Helper for json.h
struct JsonWrapper
{
    JsonWrapper(JsonNode* n) : node(n)
    { }

    ~JsonWrapper()
    {
        if (node)
            json_delete(node);
    }

    JsonNode* operator->()
    {
        return node;
    }

    void check(JsonTag tag)
    {
        if (!node || node->tag != tag)
        {
            throw malformed;
        }
    }

    JsonNode* node;

    static class MalformedException { } malformed;
};


TilesFramework tiles;

TilesFramework::TilesFramework()
    : m_crt_mode(CRT_NORMAL),
      m_controlled_from_web(false),
      m_view_loaded(false),
      m_next_view_tl(0, 0),
      m_next_view_br(-1, -1),
      m_current_flash_colour(BLACK),
      m_next_flash_colour(BLACK),
      m_need_full_map(true),
      m_text_crt("crt"),
      m_text_menu("menu_txt"),
      m_text_stat("stats"),
      m_text_message("messages"),
      m_print_fg(15)
{
    screen_cell_t default_cell;
    default_cell.tile.bg = TILE_FLAG_UNSEEN;
    m_next_view.init(default_cell);
    m_current_view.init(default_cell);
}

TilesFramework::~TilesFramework()
{
}

void TilesFramework::shutdown()
{
    close(m_sock);
    remove(m_sock_name.c_str());
}

void TilesFramework::draw_doll_edit()
{
}

bool TilesFramework::initialise()
{
    // Init socket
    m_sock = socket(PF_UNIX, SOCK_DGRAM, 0);
    if (m_sock < 0)
    {
        die("Can't open the webtiles socket!");
    }
    sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, m_sock_name.c_str());
    if (bind(m_sock, (sockaddr*) &addr, sizeof (sockaddr_un)))
    {
        die("Can't bind the webtiles socket!");
    }

    int bufsize = 64 * 1024;
    if (setsockopt(m_sock, SOL_SOCKET, SO_SNDBUF, &bufsize, sizeof (bufsize)))
    {
        die("Can't set buffer size!");
    }
    m_max_msg_size = bufsize;

    if (m_await_connection)
        _await_connection();

    _send_version();

    // Initially, switch to CRT.
    cgotoxy(1, 1, GOTO_CRT);

    return (true);
}

void TilesFramework::write_message(const char *format, ...)
{
    for (unsigned int i = 0; i < m_prefixes.size(); ++i)
        m_msg_buf.append(m_prefixes[i].data());
    m_prefixes.clear();

    char buf[2048];
    int len;

    va_list  argp;
    va_start(argp, format);
    if ((len = vsnprintf(buf, sizeof (buf), format, argp)) >= sizeof (buf))
        die("Webtiles message too long! (%d)", len);
    va_end(argp);

    m_msg_buf.append(buf);
}

void TilesFramework::finish_message()
{
    m_prefixes.clear();
    if (m_msg_buf.size() == 0)
        return;

    m_msg_buf.append("\n");
    const char* fragment_start = m_msg_buf.data();
    const char* data_end = m_msg_buf.data() + m_msg_buf.size();
    while (fragment_start < data_end)
    {
        int fragment_size = data_end - fragment_start;
        if (fragment_size > m_max_msg_size)
            fragment_size = m_max_msg_size;

        for (unsigned int i = 0; i < m_dest_addrs.size(); ++i)
        {
            int retries = 10;
            while (sendto(m_sock, fragment_start, fragment_size, 0,
                          (sockaddr*) &m_dest_addrs[i], sizeof (sockaddr_un)) == -1)
            {
                if (--retries <= 0)
                    die("Socket write error: %s", strerror(errno));

                if (errno == ECONNREFUSED || errno == ENOENT)
                {
                    // the other side is dead
                    m_dest_addrs.erase(m_dest_addrs.begin() + i);
                    i--;
                    break;
                }
                else if (errno == ENOBUFS)
                {
                    // Wait for half a second, then try again
                    usleep(500 * 1000);
                }
                else
                    die("Socket write error: %s", strerror(errno));
            }
        }

        fragment_start += fragment_size;
    }
    m_msg_buf.clear();
}

void TilesFramework::send_message(const char *format, ...)
{
    for (unsigned int i = 0; i < m_prefixes.size(); ++i)
        m_msg_buf.append(m_prefixes[i].data());
    m_prefixes.clear();

    char buf[2048];
    int len;

    va_list  argp;
    va_start(argp, format);
    if ((len = vsnprintf(buf, sizeof (buf), format, argp)) >= sizeof (buf))
        die("Webtiles message too long! (%d)", len);
    va_end(argp);

    m_msg_buf.append(buf);

    finish_message();
}

void TilesFramework::_await_connection()
{
    while (m_dest_addrs.size() == 0)
    {
        _receive_control_message();
    }
}

wint_t TilesFramework::_receive_control_message()
{
    char buf[4096]; // Should be enough for client->server messages
    sockaddr_un srcaddr;
    socklen_t srcaddr_len;

    srcaddr_len = sizeof (srcaddr);

    int len = recvfrom(m_sock, buf, sizeof (buf),
                       0,
                       (sockaddr *) &srcaddr, &srcaddr_len);

    if (len == -1)
    {
        die("Socket read error: %s", strerror(errno));
    }

    std::string data(buf, len);
    try
    {
        return _handle_control_message(srcaddr, data);
    }
    catch (JsonWrapper::MalformedException&)
    {
        dprf("Malformed control message!");
        return 0;
    }
}

wint_t TilesFramework::_handle_control_message(sockaddr_un addr, std::string data)
{
    JsonWrapper obj = json_decode(data.c_str());
    obj.check(JSON_OBJECT);

    JsonWrapper msg = json_find_member(obj.node, "msg");
    msg.check(JSON_STRING);
    std::string msgtype(msg->string_);

    int c = 0;

    if (msgtype == "attach")
    {
        JsonWrapper primary = json_find_member(obj.node, "primary");
        primary.check(JSON_BOOL);

        m_dest_addrs.push_back(addr);
        m_controlled_from_web = primary->bool_;
    }
    else if (msgtype == "key")
    {
        JsonWrapper keycode = json_find_member(obj.node, "keycode");
        keycode.check(JSON_NUMBER);

        c = (int) keycode->number_;
    }
    else if (msgtype == "spectator_joined")
    {
        _send_everything();
    }
    else if (msgtype == "menu_scroll")
    {
        JsonWrapper first = json_find_member(obj.node, "first");
        first.check(JSON_NUMBER);
        // last visible item is sent too, but currently unused

        if (!m_menu_stack.empty() && m_menu_stack.back().menu != NULL)
        {
            m_menu_stack.back().menu->webtiles_scroll((int) first->number_);
        }
    }
    else if (msgtype == "*request_menu_range")
    {
        JsonWrapper start = json_find_member(obj.node, "start");
        start.check(JSON_NUMBER);
        JsonWrapper end = json_find_member(obj.node, "end");
        end.check(JSON_NUMBER);

        if (!m_menu_stack.empty() && m_menu_stack.back().menu != NULL)
        {
            m_menu_stack.back().menu->webtiles_handle_item_request((int) start->number_,
                                                                   (int) end->number_);
        }
    }

    return c;
}

bool TilesFramework::await_input(wint_t& c, bool block)
{
    int result;
    fd_set fds;
    int maxfd = m_sock;

    while (true)
    {
        do
        {
            FD_ZERO(&fds);
            FD_SET(STDIN_FILENO, &fds);
            FD_SET(m_sock, &fds);

            if (block)
            {
                result = select(maxfd + 1, &fds, NULL, NULL, NULL);
            }
            else
            {
                timeval timeout;
                timeout.tv_sec = 0;
                timeout.tv_usec = 0;

                result = select(maxfd + 1, &fds, NULL, NULL, &timeout);
            }
        }
        while (result == -1 && errno == EINTR);

        if (result == 0)
        {
            return false;
        }
        else if (result > 0)
        {
            if (FD_ISSET(m_sock, &fds))
            {
                c = _receive_control_message();

                if (c > 0)
                    return true;
            }

            if (FD_ISSET(STDIN_FILENO, &fds))
            {
                c = 0;
                return true;
            }
        }
        else if (errno == EBADF)
        {
            // This probably means that stdin got closed because of a
            // SIGHUP. We'll just return.
            c = 0;
            return false;
        }
        else
        {
            die("select error: %s", strerror(errno));
        }
    }
}

void TilesFramework::push_prefix(const std::string& prefix)
{
    m_prefixes.push_back(prefix);
}

void TilesFramework::pop_prefix(const std::string& suffix)
{
    if (!m_prefixes.empty())
    {
        m_prefixes.pop_back();
    }
    else
        write_message("%s", suffix.c_str());
}

bool TilesFramework::prefix_popped()
{
    return m_prefixes.empty();
}

void TilesFramework::_send_version()
{
    std::string title = CRAWL " " + Version::Long();
    send_message("{msg:\"version\",text:\"%s\"}", title.c_str());

#ifdef WEB_DIR_PATH
    // The star signals a message to the server
    send_message("*{\"msg\":\"client_path\",\"path\":\"%s\"}", WEB_DIR_PATH);
#endif
}

void TilesFramework::push_menu(Menu* m)
{
    MenuInfo mi;
    mi.menu = m;
    m_menu_stack.push_back(mi);
    m->webtiles_write_menu();
    tiles.send_message();
}

void TilesFramework::push_crt_menu(std::string tag)
{
    MenuInfo mi;
    mi.menu = NULL;
    mi.tag = tag;
    m_menu_stack.push_back(mi);

    json_open_object();
    json_write_string("msg", "menu");
    json_write_string("type", "crt");
    json_write_string("tag", tag);
    json_close_object();
    send_message();
}

void TilesFramework::pop_menu()
{
    if (m_menu_stack.empty()) return;
    MenuInfo mi = m_menu_stack.back();
    m_menu_stack.pop_back();
    send_message("{msg:'close_menu'}");
}

void TilesFramework::close_all_menus()
{
    while (m_menu_stack.size())
        pop_menu();
}

static void _send_ui_state(WebtilesUIState state)
{
    tiles.json_open_object();
    tiles.json_write_string("msg", "ui_state");
    tiles.json_write_int("state", state);
    tiles.json_close_object();
    tiles.send_message();
}

void TilesFramework::set_ui_state(WebtilesUIState state)
{
    if (m_ui_state == state) return;

    m_ui_state = state;
    _send_ui_state(state);
}


static void _send_doll(const dolls_data &doll, bool submerged, bool ghost)
{
    // Ordered from back to front.
    int p_order[TILEP_PART_MAX] =
    {
        // background
        TILEP_PART_SHADOW,
        TILEP_PART_HALO,
        TILEP_PART_ENCH,
        TILEP_PART_DRCWING,
        TILEP_PART_CLOAK,
        // player
        TILEP_PART_BASE,
        TILEP_PART_BOOTS,
        TILEP_PART_LEG,
        TILEP_PART_BODY,
        TILEP_PART_ARM,
        TILEP_PART_HAIR,
        TILEP_PART_BEARD,
        TILEP_PART_HELM,
        TILEP_PART_HAND1,
        TILEP_PART_HAND2,
        TILEP_PART_DRCHEAD
    };

    int flags[TILEP_PART_MAX];
    tilep_calc_flags(doll, flags);

    // For skirts, boots go under the leg armour.  For pants, they go over.
    if (doll.parts[TILEP_PART_LEG] < TILEP_LEG_SKIRT_OFS)
    {
        p_order[7] = TILEP_PART_BOOTS;
        p_order[6] = TILEP_PART_LEG;
    }

    // Special case bardings from being cut off.
    const bool is_naga = is_player_tile(doll.parts[TILEP_PART_BASE],
                                        TILEP_BASE_NAGA);

    if (doll.parts[TILEP_PART_BOOTS] >= TILEP_BOOTS_NAGA_BARDING
        && doll.parts[TILEP_PART_BOOTS] <= TILEP_BOOTS_NAGA_BARDING_RED)
    {
        flags[TILEP_PART_BOOTS] = is_naga ? TILEP_FLAG_NORMAL : TILEP_FLAG_HIDE;
    }

    const bool is_cent = is_player_tile(doll.parts[TILEP_PART_BASE],
                                        TILEP_BASE_CENTAUR);

    if (doll.parts[TILEP_PART_BOOTS] >= TILEP_BOOTS_CENTAUR_BARDING
        && doll.parts[TILEP_PART_BOOTS] <= TILEP_BOOTS_CENTAUR_BARDING_RED)
    {
        flags[TILEP_PART_BOOTS] = is_cent ? TILEP_FLAG_NORMAL : TILEP_FLAG_HIDE;
    }

    tiles.write_message("doll:[");

    for (int i = 0; i < TILEP_PART_MAX; ++i)
    {
        int p = p_order[i];

        if (!doll.parts[p] || flags[p] == TILEP_FLAG_HIDE)
            continue;

        if (p == TILEP_PART_SHADOW && (submerged || ghost))
            continue;

        int ymax = TILE_Y;

        if (flags[p] == TILEP_FLAG_CUT_CENTAUR
            || flags[p] == TILEP_FLAG_CUT_NAGA)
        {
            ymax = 18;
        }

        tiles.write_message("[%d,%d],", doll.parts[p], ymax);
    }

    tiles.write_message("],");
}

static void _send_mcache(mcache_entry *entry, bool submerged)
{
    bool trans = entry->transparent();
    if (trans)
        tiles.write_message("trans:true,");

    const dolls_data *doll = entry->doll();
    if (doll)
        _send_doll(*doll, submerged, trans);
    else
        tiles.write_message("doll:[],");

    tiles.write_message("mcache:[");

    tile_draw_info dinfo[mcache_entry::MAX_INFO_COUNT];
    int draw_info_count = entry->info(&dinfo[0]);
    for (int i = 0; i < draw_info_count; i++)
    {
        tiles.write_message("[%d,%d,%d],", dinfo[i].idx, dinfo[i].ofs_x, dinfo[i].ofs_y);
    }

    tiles.write_message("],");
}

static bool _in_water(const packed_cell &cell)
{
    return ((cell.bg & TILE_FLAG_WATER) && !(cell.fg & TILE_FLAG_FLYING));
}

static bool _needs_flavour(const packed_cell &cell)
{
    tileidx_t bg_idx = cell.bg & TILE_FLAG_MASK;
    if (bg_idx >= TILE_DNGN_WAX_WALL)
        return true; // Needs flv.floor
    if (cell.is_liquefied || cell.is_bloody ||
        cell.is_moldy || cell.glowing_mold)
        return true; // Needs flv.special
    return false;
}

inline unsigned _get_brand(int col)
{
    return (col & COLFLAG_FRIENDLY_MONSTER) ? Options.friend_brand :
           (col & COLFLAG_NEUTRAL_MONSTER)  ? Options.neutral_brand :
           (col & COLFLAG_ITEM_HEAP)        ? Options.heap_brand :
           (col & COLFLAG_WILLSTAB)         ? Options.stab_brand :
           (col & COLFLAG_MAYSTAB)          ? Options.may_stab_brand :
           (col & COLFLAG_FEATURE_ITEM)     ? Options.feature_item_brand :
           (col & COLFLAG_TRAP_ITEM)        ? Options.trap_item_brand :
           (col & COLFLAG_REVERSE)          ? CHATTR_REVERSE
                                            : CHATTR_NORMAL;
}

void TilesFramework::_send_cell(const coord_def &gc,
                                const screen_cell_t &current_sc, const screen_cell_t &next_sc,
                                const map_cell &current_mc, const map_cell &next_mc,
                                std::map<uint32_t, coord_def>& new_monster_locs,
                                bool force_full)
{
    if ((force_full && next_mc.feat()) || current_mc.feat() != next_mc.feat())
        write_message("f:%d,", next_mc.feat());

    if (next_mc.monsterinfo())
        _send_monster(gc, next_mc.monsterinfo(), new_monster_locs, force_full);
    else if (!force_full && current_mc.monsterinfo())
        write_message("mon:null,");

    map_feature mf = get_cell_map_feature(next_mc);
    if ((force_full && mf)
        || (get_cell_map_feature(current_mc) != mf))
    {
        write_message("mf:%u,", mf);
    }

    // Glyph and colour
    ucs_t glyph = next_sc.glyph;
    if (force_full ? (glyph != ' ') : (current_sc.glyph != glyph))
    {
        if (glyph == '\\')
            write_message("g:'\\\\',");
        else if (glyph == '\'')
            write_message("g:'\\'',");
        else
            write_message("g:'%lc',", glyph);
    }
    if (force_full ? (next_sc.colour != 7) : (current_sc.colour != next_sc.colour))
    {
        int col = next_sc.colour;
        col = (_get_brand(col) << 4) | (col & 0xF);
        write_message("col:%d,", col);
    }

    push_prefix("t:{");
    {
        // Tile data
        const packed_cell &next_pc = next_sc.tile;
        const packed_cell &current_pc = current_sc.tile;

        const tileidx_t fg_idx = next_pc.fg & TILE_FLAG_MASK;

        const bool in_water = _in_water(next_pc);
        bool fg_changed = false;

        if ((force_full && next_pc.fg) || next_pc.fg != current_pc.fg)
        {
            fg_changed = true;

            write_message("fg:%u,", next_pc.fg);
            if (fg_idx && fg_idx <= TILE_MAIN_MAX)
            {
                write_message("base:%d,", tileidx_known_base_item(fg_idx));
            }
        }

        if ((force_full && next_pc.bg != TILE_FLAG_UNSEEN)
            || next_pc.bg != current_pc.bg)
            write_message("bg:%u,", next_pc.bg);

        if ((force_full && next_pc.is_bloody)
            || next_pc.is_bloody != current_pc.is_bloody)
            write_message("bloody:%u,", next_pc.is_bloody);

        if ((force_full && next_pc.is_silenced)
            || next_pc.is_silenced != current_pc.is_silenced)
            write_message("silenced:%u,", next_pc.is_silenced);

        if ((force_full && next_pc.halo)
            || next_pc.halo != current_pc.halo)
            write_message("halo:%u,", next_pc.halo);

        if ((force_full && next_pc.is_moldy)
            || next_pc.is_moldy != current_pc.is_moldy)
            write_message("moldy:%u,", next_pc.is_moldy);

        if ((force_full && next_pc.glowing_mold)
            || next_pc.glowing_mold != current_pc.glowing_mold)
            write_message("glowing_mold:%u,", next_pc.glowing_mold);

        if ((force_full && next_pc.is_sanctuary)
            || next_pc.is_sanctuary != current_pc.is_sanctuary)
            write_message("sanctuary:%u,", next_pc.is_sanctuary);

        if ((force_full && next_pc.is_liquefied)
            || next_pc.is_liquefied != current_pc.is_liquefied)
            write_message("liquefied:%u,", next_pc.is_liquefied);

        if ((force_full && next_pc.orb_glow)
            || next_pc.orb_glow != current_pc.orb_glow)
            write_message("orb_glow:%u,", next_pc.orb_glow);

        if ((force_full && next_pc.swamp_tree_water)
            || next_pc.swamp_tree_water != current_pc.swamp_tree_water)
            write_message("swtree:%u,", next_pc.swamp_tree_water);

        if ((force_full && next_pc.blood_rotation)
            || next_pc.blood_rotation != current_pc.blood_rotation)
            write_message("bloodrot:%d,", next_pc.blood_rotation);

        if (_needs_flavour(next_pc) &&
            ((next_pc.flv.floor != current_pc.flv.floor)
             || (next_pc.flv.special != current_pc.flv.special)
             || !_needs_flavour(current_pc)
             || force_full))
        {
            write_message("flv:{f:%d,", next_pc.flv.floor);
            if (next_pc.flv.special)
                write_message("s:%d,", next_pc.flv.special);
            write_message("},");
        }

        if (fg_idx >= TILEP_MCACHE_START)
        {
            if (fg_changed)
            {
                mcache_entry *entry = mcache.get(fg_idx);
                if (entry)
                {
                    _send_mcache(entry, in_water);
                }
                else
                    write_message("doll:[[%d,%d]],", TILEP_MONS_UNKNOWN, TILE_Y);
            }
        }
        else if (fg_idx == TILEP_PLAYER)
        {
            bool player_doll_changed = false;
            dolls_data result = player_doll;
            fill_doll_equipment(result);
            if (result != last_player_doll)
            {
                player_doll_changed = true;
                last_player_doll = result;
            }
            if (fg_changed || player_doll_changed)
            {
                _send_doll(last_player_doll, in_water, false);
            }
        }
        else if (fg_idx >= TILE_MAIN_MAX)
        {
            if (fg_changed)
            {
                write_message("doll:[[%d,%d]],", fg_idx, TILE_Y);
                // TODO: _transform_add_weapon
            }
        }

        bool overlays_changed = false;

        if (next_pc.num_dngn_overlay != current_pc.num_dngn_overlay)
            overlays_changed = true;
        else
        {
            for (int i = 0; i < next_pc.num_dngn_overlay; i++)
            {
                if (next_pc.dngn_overlay[i] != current_pc.dngn_overlay[i])
                {
                    overlays_changed = true;
                    break;
                }
            }
        }

        if ((force_full && next_pc.num_dngn_overlay)
            || overlays_changed)
        {
            write_message("ov:[");
            for (int i = 0; i < next_pc.num_dngn_overlay; ++i)
                write_message("%d,", next_pc.dngn_overlay[i]);
            write_message("],");
        }
    }
    pop_prefix("},");
}

void TilesFramework::_send_map(bool force_full)
{
    std::map<uint32_t, coord_def> new_monster_locs;

    force_full = force_full || m_need_full_map;
    m_need_full_map = false;

    push_prefix("{msg:\"map\",");

    if (force_full)
    {
        write_message("clear:1,");
    }

    coord_def last_gc(0, 0);
    bool send_gc = true;

    push_prefix("cells:[");
    for (int y = 0; y < GYM; y++)
        for (int x = 0; x < GXM; x++)
        {
            coord_def gc(x, y);

            if (!is_dirty(gc) && !force_full)
                continue;
            else
                mark_clean(gc);

            if (m_origin.equals(-1, -1))
                m_origin = gc;

            if (send_gc
                || last_gc.x + 1 != gc.x
                || last_gc.y != gc.y)
            {
                std::ostringstream xy;
                xy << "{x:" << (x - m_origin.x) << ",y:" << (y - m_origin.y) << ",";
                push_prefix(xy.str());
            }
            else
            {
                push_prefix("{");
            }

            _send_cell(gc, m_current_view(gc), m_next_view(gc),
                       m_current_map_knowledge(gc), env.map_knowledge(gc),
                       new_monster_locs, force_full);

            if (prefix_popped())
            {
                send_gc = false;
                last_gc = gc;
            }
            pop_prefix("},");
        }
    pop_prefix("]");
    pop_prefix("}");

    m_current_map_knowledge = env.map_knowledge;
    m_current_view = m_next_view;

    m_monster_locs = new_monster_locs;
}

void TilesFramework::_send_monster(const coord_def &gc, const monster_info* m,
                                   std::map<uint32_t, coord_def>& new_monster_locs,
                                   bool force_full)
{
    if (m->client_id)
    {
        std::ostringstream id;
        id << "mon:{id:" << m->client_id << ",";
        push_prefix(id.str());
        new_monster_locs[m->client_id] = gc;
    }
    else
        push_prefix("mon:{");

    const monster_info* last = NULL;
    std::map<uint32_t, coord_def>::const_iterator it =
        m_monster_locs.find(m->client_id);
    if (m->client_id == 0 || it == m_monster_locs.end())
    {
        last = m_current_map_knowledge(gc).monsterinfo();

        if (last && (last->client_id != m->client_id))
            write_message(""); // Force sending at least the id
    }
    else
    {
        last = m_current_map_knowledge(it->second).monsterinfo();

        if (it->second != gc)
            write_message(""); // As above
    }

    if (last == NULL)
        force_full = true;

    if (force_full || (last->full_name() != m->full_name()))
        write_message("name:'%s',",
                      replace_all_of(m->full_name(), "'", "\\'").c_str());

    if (force_full || (last->pluralised_name() != m->pluralised_name()))
        write_message("plural:'%s',",
                      replace_all_of(m->pluralised_name(), "'", "\\'").c_str());

    if (force_full || (last->type != m->type))
    {
        write_message("type:%d,", m->type);

        // TODO: get this information to the client in another way
        write_message("typedata:{avghp:%d,", mons_avg_hp(m->type));
        if (mons_class_flag(m->type, M_NO_EXP_GAIN))
            write_message("no_exp:1,");
        write_message("},");
    }

    if (force_full || (last->attitude != m->attitude))
        write_message("att:%d,", m->attitude);

    if (force_full || (last->base_type != m->base_type))
        write_message("btype:%d,", m->base_type);

    if (force_full || (last->threat != m->threat))
        write_message("threat:%d,", m->threat);

    pop_prefix("},");
}

void TilesFramework::load_dungeon(const crawl_view_buffer &vbuf,
                                  const coord_def &gc)
{
    if (vbuf.size().equals(0, 0))
        return;

    m_view_loaded = true;

    if (m_ui_state == UI_CRT)
    {
        set_ui_state(UI_NORMAL);
    }

    m_next_flash_colour = you.flash_colour;
    if (m_next_flash_colour == BLACK)
        m_next_flash_colour = viewmap_flash_colour();

    // First re-render the area that was covered by vbuf the last time
    for (int y = m_next_view_tl.y; y <= m_next_view_br.y; y++)
        for (int x = m_next_view_tl.x; x <= m_next_view_br.x; x++)
        {
            if (x < 0 || x >= GXM || y < 0 || y >= GYM)
                continue;

            if (!crawl_view.in_viewport_g(coord_def(x, y)))
            {
                coord_def grid(x, y);
                screen_cell_t *cell = &m_next_view(grid);

                draw_cell(cell, grid, false, m_next_flash_colour);
                cell->tile.flv = env.tile_flv(grid);
                pack_cell_overlays(grid, &(cell->tile));

                mark_dirty(grid);
            }
        }

    m_next_view_tl = view2grid(coord_def(1, 1));
    m_next_view_br = view2grid(crawl_view.viewsz);

    // Copy vbuf into m_next_view
    for (int y = 0; y < vbuf.size().y; y++)
        for (int x = 0; x < vbuf.size().x; x++)
        {
            coord_def pos(x+1, y+1);
            coord_def grid = view2grid(pos);

            if (grid.x < 0 || grid.x >= GXM || grid.y < 0 || grid.y >= GYM)
                continue;

            screen_cell_t *cell = &m_next_view(grid);

            *cell = ((const screen_cell_t *) vbuf)[x + vbuf.size().x * y];
            cell->tile.flv = env.tile_flv(grid);
            pack_cell_overlays(grid, &(cell->tile));

            mark_dirty(grid);
        }

    m_next_gc = gc;
}

void TilesFramework::load_dungeon(const coord_def &cen)
{
    unwind_var<coord_def> viewp(crawl_view.viewp, cen - crawl_view.viewhalfsz);
    unwind_var<coord_def> vgrdc(crawl_view.vgrdc, cen);
    unwind_var<coord_def> vlos1(crawl_view.vlos1);
    unwind_var<coord_def> vlos2(crawl_view.vlos2);

    m_next_gc = cen;

    crawl_view.calc_vlos();
    viewwindow(false, true);
    place_cursor(CURSOR_MAP, cen);
}

static const int min_stat_height = 12;
static const int stat_width = 42;

static void _send_layout_data()
{
    tiles.json_open_object();
    tiles.json_write_string("msg", "layout");

    tiles.json_write_int("stat_width", crawl_view.hudsz.x);
    tiles.json_write_int("show_diameter", ENV_SHOW_DIAMETER);
    tiles.json_write_int("gxm", GXM);
    tiles.json_write_int("gym", GYM);
    tiles.json_write_int("msg_height", crawl_view.msgsz.y);

    tiles.json_close_object();

    tiles.send_message();
}

void TilesFramework::resize()
{
    // Send the client the necessary data to do the layout
    _send_layout_data();

    m_text_message.resize(crawl_view.msgsz.x, crawl_view.msgsz.y);
    m_text_stat.resize(crawl_view.hudsz.x, crawl_view.hudsz.y);
    m_text_crt.resize(crawl_view.termsz.x, crawl_view.termsz.y);
    m_text_menu.resize(crawl_view.termsz.x, crawl_view.termsz.y);
}

/*
  Send everything a newly joined spectator needs
 */
void TilesFramework::_send_everything()
{
    _send_version();
    _send_layout_data();

    send_message("{msg:\"vgrdc\",x:%d,y:%d}",
                 m_current_gc.x - m_origin.x, m_current_gc.y - m_origin.y);
    send_message("{msg:\"flash\",col:%d}", m_current_flash_colour);

    _send_map(true);
    finish_message();

    send_message("{msg:'redraw'}");

    // UI State
    _send_ui_state(m_ui_state);

    // Menus
    json_open_object();
    json_write_string("msg", "init_menus");
    json_open_array("menus");
    for (unsigned int i = 0; i < m_menu_stack.size(); ++i)
    {
        if (m_menu_stack[i].menu)
            m_menu_stack[i].menu->webtiles_write_menu();
        else
        {
            json_open_object();
            json_write_string("msg", "menu");
            json_write_string("type", "crt");
            json_write_string("tag", m_menu_stack[i].tag);
            json_close_object();
        }
    }
    json_close_array();
    json_close_object();
    send_message();

    m_text_crt.send(true);
    m_text_stat.send(true);
    m_text_message.send(true);
    m_text_menu.send(true);
}

void TilesFramework::clrscr()
{
    m_text_crt.clear();
    m_text_menu.clear();

    this->cgotoxy(1, 1);

    set_need_redraw();
}

void TilesFramework::cgotoxy(int x, int y, GotoRegion region)
{
    m_print_x = x - 1;
    m_print_y = y - 1;
    switch (region)
    {
    case GOTO_CRT:
        switch (m_crt_mode)
        {
        case CRT_DISABLED:
            m_print_area = NULL;
            break;
        case CRT_NORMAL:
            set_ui_state(UI_CRT);
            m_print_area = &m_text_crt;
            break;
        case CRT_MENU:
            m_print_area = &m_text_menu;
            break;
        }
        break;
    case GOTO_MSG:
        set_ui_state(UI_NORMAL);
        m_print_area = &m_text_message;
        break;
    case GOTO_STAT:
        set_ui_state(UI_NORMAL);
        m_print_area = &m_text_stat;
        break;
    default:
        m_print_area = NULL;
        break;
    }
    m_cursor_region = region;
}

void TilesFramework::redraw()
{
    if (!has_receivers()) return;

    m_text_crt.send();
    m_text_stat.send();
    m_text_message.send();
    m_text_menu.send();

    if (m_need_redraw)
    {
        push_prefix("{msg:'multi',msgs:[");
        if (m_current_gc != m_next_gc)
        {
            if (m_origin.equals(-1, -1))
                m_origin = m_next_gc;
            write_message("{msg:'vgrdc',x:%d,y:%d},",
                          m_next_gc.x - m_origin.x,
                          m_next_gc.y - m_origin.y);
            m_current_gc = m_next_gc;
        }

        if (m_current_flash_colour != m_next_flash_colour)
        {
            write_message("{msg:'flash',col:%d},",
                          m_next_flash_colour);
            m_current_flash_colour = m_next_flash_colour;
        }

        if (m_view_loaded)
        {
            push_prefix("");
            _send_map(false);
            pop_prefix(",");
        }
        pop_prefix("{msg:'redraw'}]}");
        finish_message();
    }

    m_need_redraw = false;
    m_last_tick_redraw = get_milliseconds();
}

void TilesFramework::update_minimap(const coord_def& gc)
{
    if (gc.x < 0 || gc.x >= GXM ||
        gc.y < 0 || gc.y >= GYM)
        return;

    if (you.see_cell(gc))
        return; // This will get updated by load_dungeon.
                // Also, it's possible that tile_bg is not yet
                // initialized, which could lead to problems
                // if we try to draw in-los cells.

    screen_cell_t *cell = &m_next_view(gc);

    draw_cell(cell, gc, false, m_next_flash_colour);
    cell->tile.flv = env.tile_flv(gc);
    pack_cell_overlays(gc, &(cell->tile));

    mark_dirty(gc);
}

void TilesFramework::clear_minimap()
{
    m_origin = coord_def(-1, -1);
    // Changing the origin invalidates coordinates on the client side
    m_current_gc = coord_def(-1, -1);
    m_need_full_map = true;
}

void TilesFramework::update_minimap_bounds()
{
}

void TilesFramework::update_tabs()
{
}

void TilesFramework::place_cursor(cursor_type type, const coord_def &gc)
{
    // This is mainly copied from DungeonRegion::place_cursor.
    coord_def result = gc;

    // If we're only looking for a direction, put the mouse
    // cursor next to the player to let them know that their
    // spell/wand will only go one square.
    if (mouse_control::current_mode() == MOUSE_MODE_TARGET_DIR
        && type == CURSOR_MOUSE && gc != INVALID_COORD)
    {
        coord_def delta = gc - you.pos();

        int ax = abs(delta.x);
        int ay = abs(delta.y);

        result = you.pos();
        if (1000 * ay < 414 * ax)
            result += (delta.x > 0) ? coord_def(1, 0) : coord_def(-1, 0);
        else if (1000 * ax < 414 * ay)
            result += (delta.y > 0) ? coord_def(0, 1) : coord_def(0, -1);
        else if (delta.x > 0)
            result += (delta.y > 0) ? coord_def(1, 1) : coord_def(1, -1);
        else if (delta.x < 0)
            result += (delta.y > 0) ? coord_def(-1, 1) : coord_def(-1, -1);
    }

    if (m_cursor[type] != result)
    {
        m_cursor[type] = result;
        if (type == CURSOR_MOUSE)
            m_last_clicked_grid = coord_def();

        if (result == NO_CURSOR)
        {
            send_message("{msg:\"cursor\",id:%d}", type);
            return;
        }
        else
        {
            send_message("{msg:\"cursor\",id:%d,loc:{x:%d,y:%d}}", type,
                    result.x - m_origin.x, result.y - m_origin.y);
        }
    }
}

void TilesFramework::clear_text_tags(text_tag_type type)
{
}

void TilesFramework::add_text_tag(text_tag_type type, const std::string &tag,
                                  const coord_def &gc)
{
}

void TilesFramework::add_text_tag(text_tag_type type, const monster_info& mon)
{
}

const coord_def &TilesFramework::get_cursor() const
{
    return m_cursor[CURSOR_MOUSE];
}

void TilesFramework::add_overlay(const coord_def &gc, tileidx_t idx)
{
    if (idx >= TILE_MAIN_MAX)
        return;

    m_has_overlays = true;

    send_message("{msg:'overlay',idx:%d,x:%d,y:%d}", idx,
            gc.x - m_origin.x, gc.y - m_origin.y);
}

void TilesFramework::clear_overlays()
{
    if (m_has_overlays)
        send_message("{msg:'clear_overlays'}");

    m_has_overlays = false;
}

void TilesFramework::set_need_redraw(unsigned int min_tick_delay)
{
    unsigned int ticks = (get_milliseconds() - m_last_tick_redraw);
    if (min_tick_delay && ticks <= min_tick_delay)
        return;

    m_need_redraw = true;
}

bool TilesFramework::need_redraw() const
{
    return m_need_redraw;
}

void TilesFramework::textcolor(int col)
{
    m_print_fg = col & 0xF;
    m_print_bg = (col >> 4) & 0xF;
}

void TilesFramework::textbackground(int col)
{
    m_print_bg = col;
}

void TilesFramework::put_string(char *buffer)
{
    // This basically just converts buffer to ucs_t and then uses put_ucs_string
    ucs_t buf2[1024], c;

    int j = 0;

    int clen;
    do
    {
        buffer += clen = utf8towc(&c, buffer);

        // TODO: use wcwidth() to handle widths!=1:
        // *  2 for CJK chars -- add a zero-width blank?
        // *  0 for combining characters -- would need extra support
        // * -1 for non-printable stuff -- assert or ignore
        buf2[j] = c;
        j++;

        if (c == 0 || j == (ARRAYSZ(buf2) - 1))
        {
            if (c != 0)
                buf2[j + 1] = 0;

            if (j - 1 != 0)
            {
                put_ucs_string(buf2);
            }
        }
    } while (clen);
}

void TilesFramework::put_ucs_string(ucs_t *str)
{
    if (m_print_area == NULL)
        return;

    while (*str)
    {
        if (*str == '\r')
            continue;

        if (*str == '\n')
        {
            m_print_x = 0;
            m_print_y++;
            // TODO: Clear end of line?
        }
        else
        {
            if (m_print_x >= m_print_area->mx)
            {
                m_print_x = 0;
                m_print_y++;
            }

            if (m_print_y < m_print_area->my)
                m_print_area->put_character(*str, m_print_fg, m_print_bg,
                                            m_print_x, m_print_y);

            m_print_x++;
        }

        str++;
    }
}

void TilesFramework::clear_to_end_of_line()
{
    if (m_print_area == NULL ||
        m_print_y >= m_print_area->my)
        return;

    for (int x = m_print_x; x < m_print_area->mx; ++x)
        m_print_area->put_character(' ', m_print_fg, m_print_bg, x, m_print_y);
}


void TilesFramework::write_message_escaped(const std::string& s)
{
    m_msg_buf.reserve(m_msg_buf.size() + s.size());

    for (size_t i = 0; i < s.size(); ++i)
    {
        char c = s[i];
        if (c == '"')
            m_msg_buf.append("\\\"");
        else if (c == '\\')
            m_msg_buf.append("\\\\");
        else if (c == '\n')
            m_msg_buf.append("\\\n");
        else
            m_msg_buf.append(1, c);
    }
}

void TilesFramework::json_open_object(const std::string& name)
{
    json_write_comma();
    if (!name.empty())
        json_write_name(name);

    write_message("{");
    json_object_level++;
    need_comma = false;
}

void TilesFramework::json_close_object()
{
    write_message("}");
    json_object_level--;
    need_comma = true;
}

void TilesFramework::json_open_array(const std::string& name)
{
    json_write_comma();
    if (!name.empty())
        json_write_name(name);

    write_message("[");
    json_object_level++;
    need_comma = false;
}

void TilesFramework::json_close_array()
{
    write_message("]");
    json_object_level--;
    need_comma = true;
}

void TilesFramework::json_write_comma()
{
    if (json_object_level > 0 && need_comma)
    {
        write_message(",");
        need_comma = false;
    }
}

void TilesFramework::json_write_name(const std::string& name)
{
    json_write_comma();

    write_message("\"");
    write_message_escaped(name);
    write_message("\":");
}

void TilesFramework::json_write_int(int value)
{
    json_write_comma();

    write_message("%d", value);

    need_comma = true;
}

void TilesFramework::json_write_int(const std::string& name, int value)
{
    if (!name.empty())
        json_write_name(name);

    json_write_int(value);
}

void TilesFramework::json_write_string(const std::string& value)
{
    json_write_comma();

    write_message("\"");
    write_message_escaped(value);
    write_message("\"");

    need_comma = true;
}

void TilesFramework::json_write_string(const std::string& name,
                                       const std::string& value)
{
    if (!name.empty())
        json_write_name(name);

    json_write_string(value);
}
#endif
