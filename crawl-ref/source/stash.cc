/*
 *  File:       stash.cc
 *  Summary:    Classes tracking player stashes
 *  Written by: Darshan Shaligram
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

#include "AppHdr.h"
#include "chardump.h"
#include "clua.h"
#include "describe.h"
#include "itemname.h"
#include "itemprop.h"
#include "files.h"
#include "invent.h"
#include "items.h"
#include "Kills.h"
#include "libutil.h"
#include "menu.h"
#include "misc.h"
#include "overmap.h"
#include "shopping.h"
#include "spl-book.h"
#include "stash.h"
#include "stuff.h"
#include "tags.h"
#include "travel.h"

#include <cctype>
#include <cstdio>
#include <fstream>
#include <algorithm>

#define ST_MAJOR_VER ((unsigned char) 4)
#define ST_MINOR_VER ((unsigned char) 4)

#define LUA_SEARCH_ANNOTATE "ch_stash_search_annotate_item"
#define LUA_DUMP_ANNOTATE   "ch_stash_dump_annotate_item"
#define LUA_VIEW_ANNOTATE   "ch_stash_view_annotate_item"

std::string userdef_annotate_item(const char *s, const item_def *item,
                                  bool exclusive)
{
#ifdef CLUA_BINDINGS
    if (exclusive)
        lua_set_exclusive_item(item);
    std::string ann;
    clua.callfn(s, "u>s", item, &ann);
    if (exclusive)
        lua_set_exclusive_item(NULL);
    return (ann);
    
#else
    return ("");
#endif
}

std::string stash_annotate_item(const char *s,
                                const item_def *item,
                                bool exclusive = false)
{
    std::string text = userdef_annotate_item(s, item, exclusive);
    if ( (item->base_type == OBJ_BOOKS &&
            item_type_known(*item) &&
            item->sub_type != BOOK_MANUAL &&
            item->sub_type != BOOK_DESTRUCTION)
         || count_staff_spells(*item, true) > 1 )
    {
        formatted_string fs;
        item_def dup = *item;
        spellbook_contents( dup, 
                item->base_type == OBJ_BOOKS? 
                    RBOOK_READ_SPELL
                  : RBOOK_USE_STAFF, 
                &fs );
        text += EOL;
        text += fs.tostring(2, -2);
    }
    return text;
}

bool is_stash(int x, int y)
{
    LevelStashes *ls = stashes.find_current_level();
    if (ls)
    {
        Stash *s = ls->find_stash(x, y);
        return s && s->enabled;
    }
    return false;
}

void describe_stash(int x, int y)
{
    LevelStashes *ls = stashes.find_current_level();
    if (ls)
    {
        Stash *s = ls->find_stash(x, y);
        if (s)
        {
            std::string desc = "[Stash: " 
                       + s->description() + "]";
            mpr(desc.c_str());
        }
    }
}

static void fully_identify_item(item_def *item)
{
    if (!item || !is_valid_item(*item))
        return;

    set_ident_flags( *item, ISFLAG_IDENT_MASK );
    if (item->base_type != OBJ_WEAPONS)
        set_ident_type( item->base_type, 
                        item->sub_type, 
                        ID_KNOWN_TYPE );
}

static void save_item(FILE *file, const item_def &item)
{
    writeByte(file, item.base_type);
    writeByte(file, item.sub_type);
    writeShort(file, item.plus);
    writeShort(file, item.plus2);
    writeLong(file, item.special);
    writeShort(file, item.quantity);
    writeByte(file, item.colour);
    writeLong(file, item.flags);
}

static void load_item(FILE *file, item_def &item)
{
    item.base_type  = readByte(file);
    item.sub_type   = readByte(file);
    item.plus       = readShort(file);
    item.plus2      = readShort(file);
    item.special    = readLong(file);
    item.quantity   = readShort(file);
    item.colour     = readByte(file);
    item.flags      = readLong(file);
}

bool Stash::aggressive_verify = true;
std::vector<item_def> Stash::filters;

Stash::Stash(int xp, int yp) : enabled(true), items()
{
    // First, fix what square we're interested in
    if (xp == -1)
    {
        xp = you.x_pos;
        yp = you.y_pos;
    }
    x = (unsigned char) xp;
    y = (unsigned char) yp;
    abspos = GXM * (int) y + x;

    update();
}

bool Stash::are_items_same(const item_def &a, const item_def &b)
{
    const bool same = a.base_type == b.base_type
        && a.sub_type == b.sub_type
        && a.plus == b.plus
        && a.plus2 == b.plus2
        && a.special == b.special
        && a.colour == b.colour
        && a.flags == b.flags
        && a.quantity == b.quantity;

    // Account for rotting meat when comparing items.
    return (same
            || (a.base_type == b.base_type
                && (a.base_type == OBJ_CORPSES
                    || (a.base_type == OBJ_FOOD && a.sub_type == FOOD_CHUNK
                        && b.sub_type == FOOD_CHUNK))
                && a.plus == b.plus));
}

void Stash::filter(const std::string &str)
{
    std::string base = str;

    base.erase(base.find_last_not_of(" \n\t") + 1);
    base.erase(0, base.find_first_not_of(" \n\t"));

    unsigned char subc = 255;
    std::string::size_type cpos = base.find(":", 0);
    if (cpos != std::string::npos)
    {
        std::string subs = base.substr(cpos + 1);
        subc = atoi(subs.c_str());

        base = base.substr(0, cpos);
    }

    unsigned char basec = atoi(base.c_str());
    filter(basec, subc);
}

void Stash::filter(unsigned char base, unsigned char sub)
{
    item_def item;
    item.base_type = base;
    item.sub_type  = sub;

    filters.push_back(item);
}

bool Stash::is_filtered(const item_def &item)
{
    for (int i = 0, count = filters.size(); i < count; ++i)
    {
        const item_def &filter = filters[i];
        if (item.base_type == filter.base_type &&
                (filter.sub_type == 255 ||
                 item.sub_type == filter.sub_type))
            return true;
    }
    return false;
}

bool Stash::unverified() const
{
    return (!verified);
}

bool Stash::pickup_eligible() const
{
    for (int i = 0, size = items.size(); i < size; ++i)
        if (item_needs_autopickup(items[i]))
            return (true);
    return (false);
}

void Stash::update()
{
    int objl = igrd[x][y];
    // If this is your position, you know what's on this square
    if (x == you.x_pos && y == you.y_pos)
    {
        // Zap existing items
        items.clear();

        // Now, grab all items on that square and fill our vector
        while (objl != NON_ITEM)
        {
            if (!is_filtered(mitm[objl]))
                items.push_back(mitm[objl]);
            objl = mitm[objl].link;
        }

        verified = true;
    }
    // If this is not your position, the only thing we can do is verify that
    // what the player sees on the square is the first item in this vector.
    else
    {
        if (objl == NON_ITEM)
        {
            items.clear();
            verified = true;
            return ;
        }

        // There's something on this square. Take a squint at it.
        const item_def &item = mitm[objl];

        if (item.link == NON_ITEM)
            items.clear();

        // We knew of nothing on this square, so we'll assume this is the 
        // only item here, but mark it as unverified unless we can see nothing
        // under the item.
        if (items.size() == 0)
        {
            if (!is_filtered(item))
                items.push_back(item);
            verified = (item.link == NON_ITEM);
            return ;
        }

        // There's more than one item in this pile. As long as the top item is
        // not filtered, we can check to see if it matches what we think the
        // top item is.

        if (is_filtered(item)) return;

        const item_def &first = items[0];
        // Compare these items
        if (!are_items_same(first, item))
        {
            if (aggressive_verify)
            {
                // See if 'item' matches any of the items we have. If it does,
                // we'll just make that the first item and leave 'verified'
                // unchanged.

                // Start from 1 because we've already checked items[0]
                for (int i = 1, count = items.size(); i < count; ++i)
                {
                    if (are_items_same(items[i], item))
                    {
                        // Found it. Swap it to the front of the vector.
                        item_def temp   = items[i];
                        items[i]        = items[0];
                        items[0]        = temp;

                        // We don't set verified to true. If this stash was
                        // already unverified, it remains so.
                        return;
                    }
                }
            }

            // If this is unverified, forget last item on stack. This isn't
            // terribly clever, but it prevents the vector swelling forever.
            if (!verified) items.pop_back();

            // Items are different. We'll put this item in the front of our 
            // vector, and mark this as unverified
            items.insert(items.begin(), item);
            verified = false;
        }
    }
}

// Returns the item name for a given item, with any appropriate
// stash-tracking pre/suffixes.
std::string Stash::stash_item_name(const item_def &item)
{
    char buf[ITEMNAME_SIZE];
    
    if (item.base_type == OBJ_GOLD)
        snprintf(buf, sizeof buf, "%d gold piece%s", item.quantity, 
                    (item.quantity > 1? "s" : ""));
    else
        item_name(item, DESC_NOCAP_A, buf, false);

    return buf;
}

class StashMenu : public InvMenu
{
public:
    StashMenu() : InvMenu(MF_SINGLESELECT), can_travel(false) { }
public:
    bool can_travel;
protected:
    void draw_title();
    bool process_key(int key);
};

void StashMenu::draw_title()
{
    if (title)
    {
        gotoxy(1, 1);
        textcolor(title->colour);
        cprintf( "%s", title->text.c_str());
        if (title->quantity)
            cprintf(", %d item%s", title->quantity, 
                                   title->quantity == 1? "" : "s");
        cprintf(")");
        if (can_travel)
            cprintf("  [ENTER - travel]");
    }
}

bool StashMenu::process_key(int key)
{
    if (key == CK_ENTER)
    {
        // Travel activates.
        lastch = 1;
        return false;
    }
    return Menu::process_key(key);
}

static MenuEntry *stash_menu_fixup(MenuEntry *me)
{
    const item_def *item = static_cast<const item_def *>( me->data );
    if (item->base_type == OBJ_GOLD)
    {
        me->quantity = 0;
        me->colour   = DARKGREY;
    }

    return (me);
}

bool Stash::show_menu(const std::string &prefix, bool can_travel) const
{
    StashMenu menu;

    MenuEntry *mtitle = new MenuEntry("Stash (" + prefix, MEL_TITLE);
    menu.can_travel = can_travel;
    mtitle->quantity = items.size();
    menu.set_title(mtitle);

    menu.load_items( InvMenu::xlat_itemvect(items), stash_menu_fixup);
    std::vector<MenuEntry*> sel;
    while (true)
    {
        sel = menu.show();
        if (menu.getkey() == 1)
            return true;
        
        if (sel.size() != 1)
            break;

        const item_def *item = 
                static_cast<const item_def *>( sel[0]->data );
        describe_item(*item);
    }
    return false;
}

std::string Stash::description() const
{
    if (!enabled || items.empty())
        return ("");

    const item_def &item = items[0];
    std::string desc = stash_item_name(item);
    
    size_t sz = items.size();
    if (sz > 1)
    {
        char additionals[50];
        snprintf(additionals, sizeof additionals, 
                " (+%lu)", 
                (unsigned long) (sz - 1));
        desc += additionals;
    }
    return (desc);
}

bool Stash::matches_search(const std::string &prefix,
                           const base_pattern &search,
                           stash_search_result &res) const
{
    if (!enabled || items.empty()) return false;

    for (unsigned i = 0; i < items.size(); ++i)
    {
        const item_def &item = items[i];
        std::string s = stash_item_name(item);
        std::string ann = stash_annotate_item(
                    LUA_SEARCH_ANNOTATE, &item);
        if (search.matches(prefix + " " + ann + s))
        {
            if (!res.count++)
                res.match = s;
            res.matches += item.quantity;
            continue;
        }

        if (is_dumpable_artifact(item, Options.verbose_dump))
        {
            std::string desc = munge_description(get_item_description(item, 
                                                          Options.verbose_dump,
                                                          true ));
            if (search.matches(desc))
            {
                if (!res.count++)
                    res.match = s;
                res.matches += item.quantity;
            }
        }
    }
    if (res.matches)
    {
        res.stash = this;
        // XXX pos.pos looks lame. Lameness is not solicited.
        res.pos.pos.x = x;
        res.pos.pos.y = y;
    }

    return !!res.matches;
}

void Stash::write(std::ostream &os, 
                  int refx, int refy, 
                  std::string place,
                  bool identify) 
    const
{
    if (!enabled || (items.size() == 0 && verified)) return;
    os << "(" << ((int) x - refx) << ", " << ((int) y - refy)
       << (place.length()? ", " + place : "")
       << ")"
       << std::endl;
    
    char buf[ITEMNAME_SIZE];
    for (int i = 0; i < (int) items.size(); ++i)
    {
        item_def item = items[i];

        if (identify)
            fully_identify_item(&item);

        std::string s = stash_item_name(item);
        strncpy(buf, s.c_str(), sizeof buf);

        std::string ann = userdef_annotate_item(
                LUA_DUMP_ANNOTATE, &item);

        if (!ann.empty())
        {
            trim_string(ann);
            ann = " " + ann;
        }

        os << "  " << buf
           << (!ann.empty()? ann : std::string())
           << (!verified && (items.size() > 1 || i)? " (still there?)" : "")
           << std::endl;

        if (is_dumpable_artifact(item, Options.verbose_dump))
        {
            std::string desc = munge_description(get_item_description(item, 
                                                          Options.verbose_dump,
                                                          true ));
            // Kill leading and trailing whitespace
            desc.erase(desc.find_last_not_of(" \n\t") + 1);
            desc.erase(0, desc.find_first_not_of(" \n\t"));
            // If string is not-empty, pad out to a neat indent
            if (desc.length())
            {
                // Walk backwards and prepend indenting spaces to \n characters
                for (int j = desc.length() - 1; j >= 0; --j)
                    if (desc[j] == '\n')
                        desc.insert(j + 1, " ");
                os << "    " << desc << std::endl;
            }
        }
    }

    if (items.size() <= 1 && !verified)
        os << "  (unseen)" << std::endl;
}

void Stash::save(FILE *file) const
{
    // How many items on this square?
    writeShort(file, (short) items.size());

    writeByte(file, x);
    writeByte(file, y);

    // Note: Enabled save value is inverted logic, so that it defaults to true
    writeByte(file, 
        (unsigned char) ((verified? 1 : 0) | (!enabled? 2 : 0)) );

    // And dump the items individually. We don't bother saving fields we're
    // not interested in (and don't anticipate being interested in).
    for (unsigned i = 0; i < items.size(); ++i)
        save_item(file, items[i]);
}

void Stash::load(FILE *file)
{
    // How many items?
    int count = readShort(file);

    x = readByte(file);
    y = readByte(file);

    unsigned char flags = readByte(file);
    verified = (flags & 1) != 0;

    // Note: Enabled save value is inverted so it defaults to true.
    enabled  = (flags & 2) == 0;

    abspos = GXM * (int) y + x;

    // Zap out item vector, in case it's in use (however unlikely)
    items.clear();
    // Read in the items
    for (int i = 0; i < count; ++i)
    {
        item_def item;
        load_item(file, item);

        items.push_back(item);
    }
}

std::ostream &operator << (std::ostream &os, const Stash &s)
{
    s.write(os);
    return os;
}

ShopInfo::ShopInfo(int xp, int yp) : x(xp), y(yp), name(), shoptype(-1),
                                     visited(false), items()
{
    // Most of our initialization will be done externally; this class is really
    // a mildly glorified struct.
    const shop_struct *sh = get_shop(x, y);
    if (sh)
        shoptype = sh->type;
}

ShopInfo::ShopId::ShopId(int stype) : shoptype(stype), id()
{
    shop_init_id_type( shoptype, id );
}

ShopInfo::ShopId::~ShopId()
{
    shop_uninit_id_type( shoptype, id );
}

void ShopInfo::add_item(const item_def &sitem, unsigned price)
{
    shop_item it;
    it.item  = sitem;
    it.price = price;
    items.push_back(it);
}

std::string ShopInfo::shop_item_name(const shop_item &si) const
{
    char shopitem[ITEMNAME_SIZE * 2];
    std::string itemname = Stash::stash_item_name(si.item);
    snprintf(shopitem, sizeof shopitem, "%s (%u gold)", 
            itemname.c_str(), si.price);
    return shopitem;   
}

std::string ShopInfo::shop_item_desc(const shop_item &si) const
{
    std::string desc;
    if (is_dumpable_artifact(si.item, Options.verbose_dump))
    {
        desc = munge_description(get_item_description(si.item, 
                    Options.verbose_dump,
                    true));
        trim_string(desc);
        
        // Walk backwards and prepend indenting spaces to \n characters
        for (int i = desc.length() - 1; i >= 0; --i)
            if (desc[i] == '\n')
                desc.insert(i + 1, " ");
    }
    return desc;
}

void ShopInfo::describe_shop_item(const shop_item &si) const
{
    describe_item( si.item );
}

bool ShopInfo::show_menu(const std::string &place,
                         bool can_travel) const
{
    ShopId id(shoptype);
    StashMenu menu;

    MenuEntry *mtitle = new MenuEntry(name + " (" + place, MEL_TITLE);
    menu.can_travel = can_travel;
    mtitle->quantity = items.size();
    menu.set_title(mtitle);

    menu_letter hotkey;
    if (items.empty())
    {
        MenuEntry *me = new MenuEntry(
                visited? "  (Shop is empty)" : "  (Shop contents are unknown)",
                MEL_ITEM,
                0,
                0);
        me->colour = DARKGREY;
        menu.add_entry(me);
    }
    else
    {
        for (int i = 0, count = items.size(); i < count; ++i)
        {
            MenuEntry *me = new MenuEntry(shop_item_name(items[i]),
                                          MEL_ITEM,
                                          1,
                                          hotkey++);
            me->data = const_cast<shop_item *>( &items[i] );
            menu.add_entry(me);
        }
    }

    std::vector<MenuEntry*> sel;
    while (true)
    {
        sel = menu.show();
        if (menu.getkey() == 1)
            return true;
        
        if (sel.size() != 1)
            break;

        const shop_item *item = 
                static_cast<const shop_item *>( sel[0]->data );
        describe_shop_item(*item);
    }
    return false;
}

std::string ShopInfo::description() const
{
    return (name);
}

bool ShopInfo::matches_search(const std::string &prefix,
                              const base_pattern &search, 
                              stash_search_result &res) const
{
    if (items.empty() && visited) return false;

    ShopId id(shoptype);
    bool match = false;

    for (unsigned i = 0; i < items.size(); ++i)
    {
        std::string sname = shop_item_name(items[i]);
        std::string ann = stash_annotate_item(
                    LUA_SEARCH_ANNOTATE, &items[i].item, true);
        
        bool thismatch = false;
        if (search.matches(prefix + " " + ann + sname))
            thismatch = true;
        else
        {
            std::string desc = shop_item_desc(items[i]);
            if (search.matches(desc))
                thismatch = true;
        }

        if (thismatch)
        {
            if (!res.count++)
                res.match = sname;
            res.matches++;
        }
    }

    if (!res.matches)
    {
        std::string shoptitle = prefix + " {shop} " + name;
        if (!visited && items.empty())
            shoptitle += "*";
        if (search.matches(shoptitle))
        {
            match = true;
            res.match = name;
        }
    }

    if (match || res.matches)
    {
        res.shop = this;
        res.pos.pos.x = x;
        res.pos.pos.y = y;
    }
    
    return match || res.matches;
}

void ShopInfo::write(std::ostream &os, bool identify) const
{
    ShopId id(shoptype);

    os << "[Shop] " << name << std::endl;
    if (items.size() > 0)
    {
        for (unsigned i = 0; i < items.size(); ++i)
        {
            shop_item item = items[i];

            if (identify)
                fully_identify_item(&item.item);

            os << "  " << shop_item_name(item) << std::endl;
            std::string desc = shop_item_desc(item);
            if (desc.length() > 0)
                os << "    " << desc << std::endl;
        }
    }
    else if (visited)
        os << "  (Shop is empty)" << std::endl;
    else
        os << "  (Shop contents are unknown)" << std::endl;
}

void ShopInfo::save(FILE *file) const
{
    writeShort(file, shoptype);

    int mangledx = (short) x;
    if (!visited)
        mangledx |= 1024;
    writeShort(file, mangledx);
    writeShort(file, (short) y);

    writeShort(file, (short) items.size());

    writeString(file, name);

    for (unsigned i = 0; i < items.size(); ++i)
    {
        save_item(file, items[i].item);
        writeShort(file, (short) items[i].price );
    }
}

void ShopInfo::load(FILE *file)
{
    shoptype = readShort(file);

    x = readShort(file);
    visited = !(x & 1024);
    x &= 0xFF;

    y = readShort(file);

    int itemcount = readShort(file);

    name = readString(file);
    for (int i = 0; i < itemcount; ++i)
    {
        shop_item item;
        load_item(file, item.item);
        item.price = (unsigned) readShort(file);
        items.push_back(item);
    }
}

std::ostream &operator << (std::ostream &os, const ShopInfo &s)
{
    s.write(os);
    return os;
}

LevelStashes::LevelStashes()
{
    branch = you.where_are_you;
    depth  = you.your_level;
}

bool LevelStashes::operator < (const LevelStashes &lev) const
{
    return branch < lev.branch || (branch == lev.branch && depth < lev.depth);
}

bool LevelStashes::isBelowPlayer() const
{
    return branch > you.where_are_you 
        || (branch == you.where_are_you && depth > you.your_level);
}

Stash *LevelStashes::find_stash(int x, int y)
{
    return const_cast<Stash *>(
        const_cast<const LevelStashes *>(this)->find_stash(x, y) );
}

const Stash *LevelStashes::find_stash(int x, int y) const
{
    if (x == -1 || y == -1)
    {
        x = you.x_pos;
        y = you.y_pos;
    }
    const int abspos = (GXM * y) + x;
    c_stashes::const_iterator st = stashes.find(abspos);
    return (st == stashes.end()? NULL : &st->second);
}

const ShopInfo *LevelStashes::find_shop(int x, int y) const
{
    for (unsigned i = 0; i < shops.size(); ++i)
    {
        if (shops[i].isAt(x, y))
            return (&shops[i]);
    }
    return (NULL);
}

bool LevelStashes::shop_needs_visit(int x, int y) const
{
    const ShopInfo *shop = find_shop(x, y);
    return (shop && !shop->is_visited());
}

bool LevelStashes::needs_visit(int x, int y) const
{
    const Stash *s = find_stash(x, y);
    if (s && (s->unverified() || s->pickup_eligible()))
        return (true);

    return (shop_needs_visit(x, y));
}

ShopInfo &LevelStashes::get_shop(int x, int y)
{
    for (unsigned i = 0; i < shops.size(); ++i)
    {
        if (shops[i].isAt(x, y))
            return shops[i];
    }
    ShopInfo si(x, y);
    si.set_name(shop_name(x, y));
    shops.push_back(si);
    return get_shop(x, y);
}

// Updates the stash at (x,y). Returns true if there was a stash at (x,y), false
// otherwise.
bool LevelStashes::update_stash(int x, int y)
{
    Stash *s = find_stash(x, y);
    if (s)
    {
        s->update();
        if (s->empty())
            kill_stash(*s);
        return true;
    }
    return false;
}

// Removes a Stash from the level.
void LevelStashes::kill_stash(const Stash &s)
{
    stashes.erase(s.abs_pos());
}

void LevelStashes::no_stash(int x, int y)
{
    Stash *s = find_stash(x, y);
    bool en = false;
    if (s)
    {
        en = s->enabled = !s->enabled;
        s->update();
        if (s->empty())
            kill_stash(*s);
    }
    else
    {
        Stash newStash(x, y);
        newStash.enabled = false;

        stashes[ newStash.abs_pos() ] = newStash;
    }

    mpr(en? "I'll no longer ignore what I see on this square."
        : "Ok, I'll ignore what I see on this square.");
}

void LevelStashes::add_stash(int x, int y)
{
    Stash *s = find_stash(x, y);
    if (s)
    {
        s->update();
        if (s->empty())
            kill_stash(*s);
    }
    else
    {
        Stash newStash(x, y);
        if (!newStash.empty())
            stashes[ newStash.abs_pos() ] = newStash;
    }
}

bool LevelStashes::isCurrent() const
{
    return branch == you.where_are_you && depth == you.your_level;
}

bool LevelStashes::in_hell() const
{
    return branch >= BRANCH_DIS 
            && branch <= BRANCH_THE_PIT
            && branch != BRANCH_VESTIBULE_OF_HELL;
}

bool LevelStashes::in_branch(int branchid) const
{
    return branch == branchid;
}


std::string LevelStashes::level_name() const
{
    int curr_subdungeon_level = subdungeon_depth( branch, depth );
    return (place_name(
                get_packed_place(branch, curr_subdungeon_level, LEVEL_DUNGEON), 
                true, true));
}

std::string LevelStashes::short_level_name() const
{
    return (short_place_name(
            get_packed_place( branch, 
                subdungeon_depth( branch, depth ),
                LEVEL_DUNGEON ) ));
}

int LevelStashes::count_stashes() const
{
    int rawcount = stashes.size();
    if (!rawcount)
        return (0);

    for (c_stashes::const_iterator iter = stashes.begin(); 
            iter != stashes.end(); iter++)
    {
        if (!iter->second.enabled)
            --rawcount;
    }
    return rawcount;
}

void LevelStashes::get_matching_stashes(
        const base_pattern &search,
        std::vector<stash_search_result> &results)
    const
{
    level_id clid(branch, subdungeon_depth(branch, depth));
    std::string lplace = "{" + short_place_name(clid) + "}";
    for (c_stashes::const_iterator iter = stashes.begin();
            iter != stashes.end(); iter++)
    {
        if (iter->second.enabled)
        {
            stash_search_result res;
            if (iter->second.matches_search(lplace, search, res))
            {
                res.pos.id = clid;
                results.push_back(res);
            }
        }
    }

    for (unsigned i = 0; i < shops.size(); ++i)
    {
        stash_search_result res;
        if (shops[i].matches_search(lplace, search, res))
        {
            res.pos.id = clid;
            results.push_back(res);
        }
    }
}

void LevelStashes::write(std::ostream &os, bool identify) const
{
    if (visible_stash_count() == 0) return ;
    os << level_name() << std::endl;

    for (unsigned i = 0; i < shops.size(); ++i)
    {
        shops[i].write(os, identify);
    }

    if (stashes.size())
    {
        const Stash &s = stashes.begin()->second;
        int refx = s.getX(), refy = s.getY();
        std::string levname = short_level_name();
        for (c_stashes::const_iterator iter = stashes.begin(); 
                iter != stashes.end(); iter++)
        {
            iter->second.write(os, refx, refy, levname, identify);
        }
    }
    os << std::endl;
}

void LevelStashes::save(FILE *file) const
{
    // How many stashes on this level?
    writeShort(file, (short) stashes.size());

    writeByte(file, branch);
    writeShort(file, (short) depth);

    // And write the individual stashes
    for (c_stashes::const_iterator iter = stashes.begin(); 
            iter != stashes.end(); iter++)
        iter->second.save(file);

    writeShort(file, (short) shops.size());
    for (unsigned i = 0; i < shops.size(); ++i)
        shops[i].save(file);
}

void LevelStashes::load(FILE *file)
{
    int size = readShort(file);

    branch = readByte(file);
    depth  = readShort(file);

    stashes.clear();
    for (int i = 0; i < size; ++i)
    {
        Stash s;
        s.load(file);
        if (!s.empty())
            stashes[ s.abs_pos() ] = s;
    }

    shops.clear();
    int shopc = readShort(file);
    for (int i = 0; i < shopc; ++i)
    {
        ShopInfo si(0, 0);
        si.load(file);
        shops.push_back(si);
    }
}

std::ostream &operator << (std::ostream &os, const LevelStashes &ls)
{
    ls.write(os);
    return os;
}

LevelStashes &StashTracker::get_current_level()
{
    std::vector<LevelStashes>::iterator iter = levels.begin();
    for ( ; iter != levels.end() && !iter->isBelowPlayer(); iter++)
    {
        if (iter->isCurrent()) return *iter;
    }
    if (iter == levels.end())
        levels.push_back(LevelStashes());
    else
        levels.insert(iter, LevelStashes());
    return get_current_level();
}

LevelStashes *StashTracker::find_current_level()
{
    std::vector<LevelStashes>::iterator iter = levels.begin();
    for ( ; iter != levels.end() && !iter->isBelowPlayer(); iter++)
    {
        if (iter->isCurrent()) return &*iter;
    }
    return NULL;
}


bool StashTracker::update_stash(int x, int y)
{
    LevelStashes *lev = find_current_level();
    if (lev)
    {
        bool res = lev->update_stash(x, y);
        if (!lev->stash_count())
            remove_level(*lev);
        return res;
    }
    return false;
}

void StashTracker::remove_level(const LevelStashes &ls)
{
    std::vector<LevelStashes>::iterator iter = levels.begin();
    for ( ; iter != levels.end(); ++iter)
    {
        if (&ls == &*iter)
        {
            levels.erase(iter);
            break ;
        }
    }
}

void StashTracker::no_stash(int x, int y)
{
    if (is_level_untrackable())
        return ;
    LevelStashes &current = get_current_level();
    current.no_stash(x, y);
    if (!current.stash_count())
        remove_level(current);
}

void StashTracker::add_stash(int x, int y, bool verbose)
{
    if (is_level_untrackable())
        return ;
    LevelStashes &current = get_current_level();
    current.add_stash(x, y);

    if (verbose)
    {
        Stash *s = current.find_stash(x, y);
        if (s && s->enabled)
            mpr("Added stash.");
    }
    
    if (!current.stash_count())
        remove_level(current);
}

void StashTracker::dump(const char *filename, bool identify) const
{
    std::ofstream outf(filename);
    if (outf)
    {
        write(outf, identify);
        outf.close();
    }
}

void StashTracker::write(std::ostream &os, bool identify) const
{
    os << you.your_name << std::endl << std::endl;
    if (!levels.size())
        os << "  You have no stashes." << std::endl;
    else
    {
        std::vector<LevelStashes>::const_iterator iter = levels.begin();
        for ( ; iter != levels.end(); iter++)
        {
            iter->write(os, identify);
        }
    }
}

void StashTracker::save(FILE *file) const
{
    // Write version info first - major + minor
    writeByte(file, ST_MAJOR_VER);
    writeByte(file, ST_MINOR_VER);

    // How many levels have we?
    writeShort(file, (short) levels.size());

    // And ask each level to write itself to the tag
    std::vector<LevelStashes>::const_iterator iter = levels.begin();
    for ( ; iter != levels.end(); iter++)
        iter->save(file);
}

void StashTracker::load(FILE *file)
{
    // Check version. Compatibility isn't important, since stash-tracking
    // is non-critical.
    unsigned char major = readByte(file),
                  minor = readByte(file);
    if (major != ST_MAJOR_VER || minor != ST_MINOR_VER) return ;

    int count = readShort(file);

    levels.clear();
    for (int i = 0; i < count; ++i)
    {
        LevelStashes st;
        st.load(file);
        if (st.stash_count()) levels.push_back(st);
    }
}

void StashTracker::update_visible_stashes(
                                    StashTracker::stash_update_mode mode)
{
    if (is_level_untrackable())
        return ;

    LevelStashes *lev = find_current_level();
    for (int cy = 1; cy <= 17; ++cy)
    {
        for (int cx = 9; cx <= 25; ++cx)
        {
            int x = you.x_pos + cx - 17, y = you.y_pos + cy - 9;
            if (x < 0 || x >= GXM || y < 0 || y >= GYM)
                continue;
            
            if (!env.show[cx - 8][cy] && !(cx == 17 && cy == 9))
                continue;

            if ((!lev || !lev->update_stash(x, y))
                    && mode == ST_AGGRESSIVE 
                    && igrd[x][y] != NON_ITEM)
            {
                if (!lev)
                    lev = &get_current_level();
                lev->add_stash(x, y);
            }

            if (grd[x][y] == DNGN_ENTER_SHOP)
                get_shop(x, y);
        }
    }

    if (lev && !lev->stash_count())
        remove_level(*lev);
}

#define SEARCH_SPAM_THRESHOLD 400
static std::string lastsearch;
static input_history search_history(15);

void StashTracker::search_stashes()
{
    char prompt[200];
    if (lastsearch.length())
        snprintf(prompt, sizeof prompt,
                "Search for what [Enter for \"%s\"]?",
                lastsearch.c_str());
    else
        snprintf(prompt, sizeof prompt,
                "Search for what?");
    
    mpr(prompt, MSGCH_PROMPT);
    // Push the cursor down to the next line.
    mpr("", MSGCH_PROMPT);

    char buf[400];
    bool validline = 
        !cancelable_get_line(buf, sizeof buf, 80, &search_history);
    mesclr();
    if (!validline || (!*buf && !lastsearch.length()))
        return;

    std::string csearch = *buf? buf : lastsearch;
    std::vector<stash_search_result> results;

    base_pattern *search = NULL;

    text_pattern tpat( csearch, true );
    search = &tpat;

#ifdef CLUA_BINDINGS
    lua_text_pattern ltpat( csearch );
    
    if (lua_text_pattern::is_lua_pattern(csearch))
        search = &ltpat;
#endif

    if (!search->valid())
    {
        mpr("Your search expression is invalid.", MSGCH_PLAIN);
        return ;
    }

    lastsearch = csearch;
    
    get_matching_stashes(*search, results);

    if (results.empty())
    {
        mpr("Can't find anything matching that.",
                MSGCH_PLAIN);
        return;
    }

    if (results.size() > SEARCH_SPAM_THRESHOLD)
    {
        mpr("Too many matches; use a more specific search.", MSGCH_PLAIN);
        return ;
    }

    display_search_results(results);
}

void StashTracker::get_matching_stashes(
        const base_pattern &search,
        std::vector<stash_search_result> &results)
    const
{
    std::vector<LevelStashes>::const_iterator iter = levels.begin();
    for ( ; iter != levels.end(); iter++)
    {
        iter->get_matching_stashes(search, results);
        if (results.size() > SEARCH_SPAM_THRESHOLD)
            return;
    }

    get_matching_features(search, results);

    level_id curr = level_id::get_current_level_id();
    for (unsigned i = 0; i < results.size(); ++i)
        results[i].player_distance = level_distance(curr, results[i].pos.id);

    // Sort stashes so that closer stashes come first and stashes on the same
    // levels with more items come first.
    std::sort(results.begin(), results.end());
}

class StashSearchMenu : public Menu
{
public:
    StashSearchMenu() : Menu(), can_travel(true), meta_key(0) { }

public:
    bool can_travel;
    int meta_key;

protected:
    bool process_key(int key);
    void draw_title();
};

void StashSearchMenu::draw_title()
{
    if (title)
    {
        gotoxy(1, 1);
        textcolor(title->colour);
        cprintf("%d %s%s", title->quantity, title->text.c_str(), 
                           title->quantity > 1? "es" : "");

        if (meta_key)
            draw_title_suffix(" (x - examine)", false);
        else
            draw_title_suffix(" (x - travel; ? - examine)", false);
    }
}

bool StashSearchMenu::process_key(int key)
{
    if (key == '?')
    {
        sel.clear();
        meta_key  = !meta_key;
        update_title();
        return true;
    }
    
    return Menu::process_key(key);
}

void StashTracker::display_search_results(
        std::vector<stash_search_result> &results)
{
    if (results.empty())
        return;
    
    bool travelable = can_travel_interlevel();

    StashSearchMenu stashmenu;
    stashmenu.can_travel = travelable;
    std::string title = "match";

    MenuEntry *mtitle = new MenuEntry(title, MEL_TITLE);
    // Abuse of the quantity field.
    mtitle->quantity = results.size();
    stashmenu.set_title(mtitle);

    menu_letter hotkey;
    for (unsigned i = 0; i < results.size(); ++i, ++hotkey)
    {
        stash_search_result &res = results[i];
        char matchtitle[ITEMNAME_SIZE];
        std::string place = short_place_name(res.pos.id);
        if (res.matches > 1 && res.count > 1)
        {
            snprintf(matchtitle, sizeof matchtitle,
                    "[%s] %s (%d)",
                    place.c_str(),
                    res.match.c_str(),
                    res.matches);
        }
        else
        {
            snprintf(matchtitle, sizeof matchtitle,
                    "[%s] %s",
                    place.c_str(),
                    res.match.c_str());
        }
        std::string mename = matchtitle;
        
        MenuEntry *me = new MenuEntry(mename, MEL_ITEM, 1, hotkey);
        me->data = &res;
        if (res.shop && !res.shop->is_visited())
            me->colour = CYAN;
        stashmenu.add_entry(me);
    }

    stashmenu.set_flags( MF_SINGLESELECT );

    std::vector<MenuEntry*> sel;
    while (true)
    {
        sel = stashmenu.show();

        if (sel.size() == 1 && stashmenu.meta_key)
        {
            stash_search_result *res = 
                static_cast<stash_search_result *>(sel[0]->data);

            bool dotravel = false;
            if (res->shop)
            {
                dotravel = res->shop->show_menu(short_place_name(res->pos.id),
                                                 travelable);
            }
            else if (res->stash)
            {
                dotravel = res->stash->show_menu(short_place_name(res->pos.id),
                                                 travelable);
            }

            if (dotravel && travelable)
            {
                redraw_screen();
                const level_pos lp = res->pos;
                start_translevel_travel(lp);
                return ;
            }
            continue;
        }
        break;
    }

    redraw_screen();
    if (travelable && sel.size() == 1 && !stashmenu.meta_key)
    {
        const stash_search_result *res = 
                static_cast<stash_search_result *>(sel[0]->data);
        const level_pos lp = res->pos;
        start_translevel_travel(lp);
        return ;
    }

}

// Global
StashTracker stashes;
