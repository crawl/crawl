#!/usr/local/bin/python
#
# Module for reading and examining saved games.
#

import os
import struct
import StringIO
import binfile

# ----------------------------------------------------------------------
# Some constants and things
# ----------------------------------------------------------------------

NUM_MONSTER_SPELL_SLOTS = 6
NUM_MONSTER_SLOTS = 10
MONS_PLAYER_GHOST = 400
MONS_PANDEMONIUM_DEMON = 401

class enum(object):
    def __init__(self, values):
        self.s2i = {}
        self.i2s = {}
        cur = 0
        for val in values.split():
            if '=' in val:
                val,cur = val.split('=')
                cur = int(cur)
            self.s2i[val] = cur
            self.i2s[cur] = val
            cur += 1
    def s(self, i): return self.i2s.get(i, str(i))

TAGS = """
    TAG_NO_TAG
    TAG_YOU
    TAG_YOU_ITEMS
    TAG_YOU_DUNGEON
    TAG_LEVEL
    TAG_LEVEL_ITEMS
    TAG_LEVEL_MONSTERS
    TAG_GHOST
    TAG_LEVEL_ATTITUDE
    TAG_LOST_MONSTERS
    TAG_LEVEL_TILES"""
TAGS_NAMES = dict( enumerate(TAGS.split()) )
TAGS_NUMS = dict( (b,a) for (a,b) in enumerate(TAGS.split()) )

tags_enum = enum(TAGS)

object_class_type = enum("""WEAPONS MISSILES ARMOUR WANDS FOOD UNKNOWN_I SCROLLS
    JEWELLERY POTIONS UNKNOWN_II BOOKS STAVES ORBS MISC CORPSE GOLD GEM
    UNASSIGNED=100 RANDOM=255""")
weapon_type = enum("""club mace flail dagger morningstar sling=13 bow crossbow handxbow
    blowgun=42 longbow=45""")
missile_type = enum("""MI_STONE MI_ARROW MI_BOLT MI_DART MI_NEEDLE MI_LARGE_ROCK
    MI_SLING_BULLET MI_JAVELIN MI_THROWING_NET""")
ammo_t = enum("THROW BOW SLING CROSSBOW HANDXBOW BLOWGUN")

# ----------------------------------------------------------------------
# unmarshall helpers
# ----------------------------------------------------------------------

def stream_container(f, reader):
    return [ reader(f) for x in xrange(f.stream1('I')) ]

def stream_array(f, count_type='B', item='B', limit=None):
    num = f.stream1(count_type)
    if limit is not None and num > limit:
        print "Warning: big array! (%d > %d)" % (num, limit)
    if len(item) == 1:
        return f.stream('%d%s' % (num, item))
    else:
        return [ f.stream(item) for x in xrange(num) ]

def stream_map(f, key_type, value_type):
    length = f.stream1('I')

    assert type(key_type)==str
    if len(key_type) == 1:
        def key_reader(): return f.stream1(key_type)
    else:
        def key_reader(): return f.stream(key_type)

    if type(value_type) == str:
        if len(value_type) == 1:
            def value_reader(): return f.stream1(value_type)
        else:
            def value_reader(): return f.stream(value_type)
    else:
        def value_reader(): return value_type()

    return dict( (key_reader(), value_reader())
                 for i in xrange(length) )

def assert_end(f):
    cur = f.tell()
    f.seek(0, os.SEEK_END)
    end = f.tell()
    if cur != end:
        print "  !! cur %d != end %d" % (cur,end)

# ----------------------------------------------------------------------
# Misc sub data structures
# ----------------------------------------------------------------------

def CrawlValue(f):
    type_, flags = f.stream('BB')
    if type_ == 1: return bool(f.stream1('B')) # BOOL
    elif type_ == 2: return f.stream1('B')     # BYTE
    elif type_ == 3: return f.stream1('H')     # SHORT
    elif type_ == 4: return f.stream1('I')     # LONG
    elif type_ == 5: return f.stream1('f')     # FLOAT
    elif type_ == 6: return f.streamString2('f') # STRING
    elif type_ == 7: return f.stream('HH')     # COORD
    elif type_ == 8: return Item(f)            # ITEM
    elif type_ == 9: return CrawlHashTable(f)  # HASH
    elif type_ == 10: return CrawlVector(f)    # VEC


class CrawlHashTable(object):
    def __init__(self, f):
        self.size = f.stream1('B')
        if self.size == 0: return
        self.typeflags = f.stream('BB')
        contents = [ (f.streamString2(), CrawlValue(f)) for x in xrange(self.size) ]


class CrawlVector(object):
    def __init__(self, f):
        self.size = f.stream1('B')
        if self.size == 0: return
        self.max, self.type, self.flags = f.stream('BBB')
        contents = [ CrawlValue(f) for x in xrange(self.size) ]

class Item(object): 
    def __init__(self, f):
        self.base_type, self.sub_type = f.stream('BB')
        self.plusses = f.stream('HH')
        self.special, self.quantity = f.stream('IH')
        self.data2 = f.stream('BHHI')
        self.unused = f.stream('HH')
        self.slot = f.stream1('B')
        self.origs = f.stream('HH')
        self.inscrip = f.streamString2()
        self.props = CrawlHashTable(f)
    def __str__(self):
        ret = "%s" % object_class_type.s(self.base_type)
        if ret == 'WEAPONS':
            ret += '/%s' % weapon_type.s(self.sub_type)
        elif ret == 'MISSILES':
            ret += '/%s' % missile_type.s(self.sub_type)
        else:
            ret += '/%d' % self.sub_type
        return ret


KC_NCATEGORIES = 3
class PlaceInfo(object):
    def __init__(self, f):
        self.data1 = f.stream('IIII')
        self.mon_kill_exp = f.stream('II')
        self.mon_kill_num = f.stream('%dI' % KC_NCATEGORIES)
        self.data2 = f.stream('6I')
        self.data3 = f.stream('6f')


def mon_spells(f): return f.stream('%dH' % NUM_MONSTER_SPELL_SLOTS)
def mon_resist(f): return f.stream('12B')

class Ghost(object):
    def __init__(self, f):
        self.name = f.streamString2()
        self.data1 = f.stream('10HBH')
        self.resist = mon_resist(f)
        self.data2 = f.stream('BBH')
        self.spells = mon_spells(f)


class Monster(object):
    def __init__(self, f):
        def mon_enchant(f): return f.stream('5H')
        self.data1 = f.stream('6B')
        self.pos = f.stream('BB')
        self.targ = f.stream('BB')
        self.data2 = f.stream('II')
        self.ench = [ mon_enchant(f) for x in xrange(f.stream1('H')) ]
        self.ench_count = f.stream1('B')
        self.type = f.stream1('H')
        self.data3 = f.stream('HHHH')
        self.inv = f.stream('%dH' % NUM_MONSTER_SLOTS)
        self.spells = spells(f)
        self.god = f.stream1('B')
        if self.type in (MONS_PLAYER_GHOST, MONS_PANDEMONIUM_DEMON):
            self.ghost = Ghost(f)


class Quiver(object):
    def __init__(self, f):
        self.cooky = f.stream1('H')
        self.last_weapon = Item(f)
        self.last_used_type = f.stream1('I')
        num_last_used = f.stream1('I')
        self.last_used_of_type = [ Item(f) for i in range(num_last_used) ]

    def dump(self):
        print 'last weapon:', self.last_weapon
        for i,item in enumerate(self.last_used_of_type):
            print ' %s: %d %s' % (ammo_t.s(i), item.quantity, item)


def Coord(f): return f.stream('HH')

class MapMarker(object):
    def __init__(self, f):
        self.mark_type = mark_type = f.stream1('H')
        if mark_type == 0:      # map_feature_marker
            self.read_base(f)
            self.feat = f.stream1('H')
        elif mark_type == 1:    # map_lua_marker
            self.read_base(f)
            self.initialized = f.stream1('B')
            if self.initialized:
                chunk = f.streamString2()
                assert False, "Don't know how much else to read"
        elif mark_type == 2:    # map_corruption_marker
            self.read_base(f)
            self.duration, self.radius = f.stream('HH')
        elif mark_type == 3:    # map_wiz_props_marker
            self.read_base(f)
            self.props = [ (f.streamString2(), f.streamString2())
                           for x in xrange(f.stream1('H')) ]
        else:
            assert "Unknown map marker type %d" % mark_type

    def read_base(self, f):
        self.coord = Coord(f)

# ----------------------------------------------------------------------
# Generic tagged file
# ----------------------------------------------------------------------

class TaggedFile(object):
    def __init__(self, fn):
        f = binfile.reader(fn)
        f.byteorder = '>'
        self.f = f
        self.major, self.minor = f.stream('bb')
        self.tags = dict( self._gen_tags() )

    def _gen_tags(self):
        while True:
            try:
                tag_id, size = self.f.stream('HI')
            except struct.error:
                return
            data = self.f.file.read(size)
            tag_name = TAGS_NAMES[tag_id]
            try:  constructor = TAG_TO_CLASS[tag_name]
            except KeyError:
                print "  Found %s (currently unsupported)" % tag_name
            else:
                print "  Found %s (parsing)" % tag_name
                sub_reader = binfile.reader(StringIO.StringIO(data))
                sub_reader.byteorder = '>'
                data = constructor(sub_reader, self.minor)
            yield (tag_id, data)


class TagBase(object): pass


class TagLEVEL_MONSTERS(TagBase):
    def __init__(self, f, minor):
        self.mons_alloc = stream_array(f, 'B', 'H')
    

def TagGHOST(f, minor):
    return [ Ghost(f) for x in xrange(f.stream1('H')) ]


class TagLOST_MONSTERS(TagBase):
    def __init__(self, f, minor):
        def follower():
            return (Monster(f), [Item(f) for x in xrange(NUM_MONSTER_SLOTS)])
        def follower_list():
            return [follower() for x in xrange(f.stream1('H'))]
        def item_list():
            return [Item(f) for x in xrange(f.stream1('H'))]
        # level_id -> follower_list
        self.lost_monst = stream_map(f, 'BIB', follower_list)
        self.lost_item = stream_map(f, 'BIB', item_list)
        assert_end(f.file)


class TagYOU_DUNGEON(TagBase):
    def __init__(self, f, minor):
        self.ucreatures = stream_array(f, 'H', 'B')
        self.c_things = stream_array(f, 'B', 'II')
        self.s_things = stream_array(f, 'H', '%dB' % len(self.c_things))
        # branch_type -> level_id(branchtype, depth, leveltype)
        self.stair_level = stream_map(f, 'I', 'BIB')
        # level_pos -> shop_type
        self.shops_present = stream_map(f, 'IIBIB', 'I')
        # level_pos -> god_type
        self.altars_present = stream_map(f, 'IIBIB', 'I')
        # level_pos -> portal_type
        self.portals_present = stream_map(f, 'IIBIB', 'I')
        # level_id -> string
        self.level_annotations = stream_map(f, 'BIB', f.streamString2)

        self.place_info = PlaceInfo(f)
        self.more_place_info = [PlaceInfo(f) for x in xrange(f.stream1('H'))]
        
        self.uniq_map_tags  = stream_container(f, f.__class__.streamString2)
        self.uniq_map_names = stream_container(f, f.__class__.streamString2)

        assert_end(f.file)


class TagYOU_ITEMS(TagBase):
    def __init__(self, f, minor):
        ninv = f.stream1('B')
        self.inv = [Item(f) for x in range(ninv)]
        ntype, nsubtype = f.stream('BB')
        self.item_desc =  [ f.stream('%dB' % nsubtype) for x in range(ntype) ]
        ident_w, ident_h = f.stream('BB')
        self.identified = [ f.stream('%dB' % ident_h) for x in range(ident_w) ]
        self.uniques = stream_array(f)
        self.books = stream_array(f)
        self.unrandart = stream_array(f, 'H', 'B')
        assert_end(f.file)


class TagYOU(TagBase):
    def __init__(self, f, minor):
        self.name = f.streamString2()
        self.data1 = f.stream('BBBBBH')  # last is pet_target
        self.data2 = f.stream('8B') # last is level_type
        self.level_type_name = f.streamString2()
        self.data3 = f.stream('5B') # species
        self.hp, self.hunger = f.stream('HH')
        self.equip = stream_array(f)
        self.magic = f.stream('BB')
        self.stats = f.stream('BBB')
        self.regen = f.stream('BBH')
        self.xp, self.gold = f.stream('II')
        self.data4 = f.stream('BBI')
        self.max_stats = f.stream('BBB')
        self.hp_magic2 = f.stream('HHHH')
        self.pos = f.stream('HH')
        self.class_name = f.streamString2()
        self.burden = f.stream1('H')
        self.spells = stream_array(f)
        self.spell_letters = stream_array(f)
        self.abil_letters = stream_array(f, 'B', 'H')
        self.skills = stream_array(f, 'B', 'BBIB')
        self.durations = stream_array(f, 'B', 'I')
        self.attributes = stream_array(f)
        self.quiver_old = stream_array(f)
        self.sacrifice = stream_array(f, 'B', 'I')
        self.mutation = stream_array(f, 'H', 'BB')
        self.penance = stream_array(f)
        self.worshipped = stream_array(f)
        self.gifts = f.stream('%dH' % len(self.worshipped))
        self.data5 = f.stream('4B')
        self.elapsed_time = f.stream1('f')
        self.wizard = f.stream1('B')
        self.game_start = f.streamString2()
        self.real_time, self.num_turns = f.stream('II')
        self.data6 = f.stream('HHB')
        self.beheldby = f.stream('%dB' % f.stream1('B'))
        if minor >= 2:
            self.piety_hysteresis = f.stream1('B')
        if minor >= 3:
            self.quiver = Quiver(f)

        cur = f.file.tell()
        f.file.seek(0, os.SEEK_END)
        cur -= f.file.tell()
        assert cur == 0, "%d bytes off" % cur


class TagLEVEL_ATTITUDE(TagBase):
    def __init__(self, f, minor):
        self.monsters = stream_array(f, 'H', 'BH')
        assert_end(f.file)

        
class TagLEVEL_ITEMS(TagBase):
    def __init__(self, f, minor):
        self.traps = stream_array(f, 'H', 'BBB')
        num_items = f.stream1('H')
        self.items = [Item(f) for x in range(num_items)]
        assert_end(f.file)


def gen_run_length_decode(f, value_type, expected):
    while expected > 0:
        run = f.stream1('B')
        value = f.stream1(value_type)
        expected -= run
        assert expected >= 0
        for i in xrange(run):
            yield value

class TagLEVEL(TagBase):
    def __init__(self, f, minor):
        self.colours = f.stream('BB')
        self.level_flags = f.stream('I')
        self.time = f.stream('f')
        self.gx,self.gy = f.stream('HH')
        self.turns = f.stream('I')
        self.grid = [ f.stream('BHHHHH')
                      for i in xrange(self.gx)
                      for j in xrange(self.gy) ]

        expected = self.gx * self.gy
        self.grid_colours = list(gen_run_length_decode(f, 'B', expected))

        self.clouds = stream_array(f, 'H', 'BBBHBH', limit=1000)
        self.shops = stream_array(f, 'B', 'BBBBBBBB', limit=15)
        self.sanctuary = Coord(f)
        self.sanctuary_time = f.stream1('B')
        self.markers = [ MapMarker(f) for x in xrange(f.stream1('H')) ]
        assert_end(f.file)

        
TAG_TO_CLASS = {
    'TAG_YOU' : TagYOU,
    'TAG_YOU_ITEMS': TagYOU_ITEMS,
    'TAG_YOU_DUNGEON': TagYOU_DUNGEON,
    'TAG_LEVEL': TagLEVEL,
    'TAG_LEVEL_ITEMS': TagLEVEL_ITEMS,
    'TAG_LEVEL_MONSTERS': TagLEVEL_MONSTERS,
    'TAG_GHOST': TagGHOST,
    'TAG_LEVEL_ATTITUDE': TagLEVEL_ATTITUDE,
    'TAG_LOST_MONSTERS': TagLOST_MONSTERS,
    # TAG_LEVEL_TILES
}


if __name__ == '__main__':
    testfile = '/Users/pld/src/crawl-ref/play/saves/Hunter-501.sav'
    x = TaggedFile(testfile)
    q = x.tags[TAGS_NUMS['TAG_YOU']].quiver
    q.dump()
