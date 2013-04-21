# GDB autoload file

import gdb.printing


class NeedLibstdcxxPrinters(Exception):
    def __str__(self):
        return """Oops!!!
The libstdc++ pretty printers seem to be missing.
See http://gcc.gnu.org/onlinedocs/libstdc++/manual/debug.html for
installation instructions.

(Alternatively, see "help disable pretty-printer" to learn how to
disable the failing printers.)"""


class coord_def_printer:
    def __init__(self, val):
        self.val = val

    def to_string(self):
        return "({0[x]}, {0[y]})".format(self.val)


class actor_printer:
    """Print an actor object."""

    def __init__(self, val):
        self.val = val

    def to_string(self):
        return "{} #{:#x} at {}".format(
            self.val["type"], int(self.val["mid"]), self.val["position"])


class item_def_printer:
    def __init__(self, val):
        self.val = val

    def to_string(self):
        return None

    def children(self):
        # Since C++ hasn't got dependant types, we have to simulate them here.
        def f(fname):
            return (fname, self.val[fname])
        def g(fname, typename):
            return (fname, self.val[fname].cast(gdb.lookup_type(typename)))

        ty = str(self.val['base_type'])
        sub_type_type = {
            'OBJ_WEAPONS':   'weapon_type',
            'OBJ_MISSILES':  'missile_type',
            'OBJ_ARMOUR':    'armour_type',
            'OBJ_WANDS':     'wand_type',
            'OBJ_FOOD':      'food_type',
            'OBJ_SCROLLS':   'scroll_type',
            'OBJ_JEWELLERY': 'jewellery_type',
            'OBJ_POTIONS':   'potion_type',
            'OBJ_BOOKS':     'book_type',
            'OBJ_STAVES':    'stave_type',
            'OBJ_ORBS':      'orb_type',
            'OBJ_MISCELLANY':'misc_item_type',
            'OBJ_CORPSES':   'corpse_type',
            # 'OBJ_GOLD':
            'OBJ_RODS':      'rod_type'
            }.get(f, 'uint8_t')
        sub_ty = str(self.val['sub_type'].cast(gdb.lookup_type(sub_type_type)))

        yield f('base_type')
        yield g('sub_type', sub_type_type)

        if ty == 'OBJ_CORPSE' or (ty == 'OBJ_FOOD' and sub_ty == 'FOOD_CHUNK'):
            yield f('mon_type')
        else:
            yield f('plus')

        plus2typename = {
            ('OBJ_ARMOUR', 'ARM_HELMET'):'helmet_desc_type',
            ('OBJ_ARMOUR', 'ARM_BOOTS'):'type_boots',
            ('OBJ_ARMOUR', 'ARM_GLOVES'):'gloves_desc_type',
            }.get((ty, sub_ty), 'short')
        yield g('plus2', plus2typename)

        yield f('special')
        yield f('colour')
        yield f('rnd')
        yield f('quantity')
        yield f('flags')

        yield f('pos')
        yield f('link')
        yield f('slot')

        yield f('orig_place')
        yield f('orig_monnum')

        yield f('inscription')
        yield f('props')


class FixedVector_printer:
    def __init__(self, val):
        self.val = val

    def to_string(self):
        return self.val['mData']


# Pretty printers for "store.h"
class CrawlHashTable_printer:
    def __init__(self, val):
        self.val = val

    def to_string(self):
        return None

    def children(self):
        if not self.val["hash_map"]:
            return ()

        vis = gdb.default_visualizer(self.val["hash_map"].dereference())
        if vis:
            return vis.children()
        else:
            raise NeedLibstdcxxPrinters

    def display_hint(self):
        return "map"

class CrawlVector_printer:
    def __init__(self, val):
        self.val = val

    def to_string(self):
        return None

    def children(self):
        if not self.val["vec"]:
            return ()

        vis = gdb.default_visualizer(self.val["vec"])
        if vis:
            return vis.children()
        else:
            raise NeedLibstdcxxPrinters

    def display_hint(self):
        return "array"

class CrawlStoreValue_printer:
    def __init__(self, val):
        self.val = val

    def to_string(self):
        ty = str(self.val['type'])
        u = self.val['val']

        def phelper(typename):
            return u['ptr'].cast(gdb.lookup_type(typename).pointer()).dereference()

        if ty == 'SV_NONE': val = '<nothing>'
        elif ty == 'SV_BOOL': val = u['boolean']
        elif ty == 'SV_BYTE': val = u['byte']
        elif ty == 'SV_SHORT': val = u['_short']
        elif ty == 'SV_INT': val = u['_int']
        elif ty == 'SV_FLOAT': val = u['_float']
        elif ty == 'SV_INT64': val = u['_int64']
        elif ty == 'SV_STR':
            val = phelper('std::basic_string<char, std::char_traits<char>, std::allocator<char> >')
        elif ty == 'SV_COORD':
            val = phelper('coord_def')
        elif ty == 'SV_ITEM':
            val = phelper('item_def')
        elif ty == 'SV_HASH':
            val = phelper('CrawlHashTable')
        elif ty == 'SV_VEC':
            val = phelper('CrawlVector')
        elif ty == 'SV_LEV_ID':
            val = phelper('level_id')
        elif ty == 'SV_LEV_POS':
            val = phelper('level_pos')
        elif ty == 'SV_MONST':
            val = actor_printer(phelper('monster')).to_string()
        elif ty == 'SV_LUA':
            val = phelper('dlua_chunk')
        else:
            raise "Unknown type: %s" % ty

        # return '[%s] %s' % (ty, val)
        return val


def build_pretty_printer():
    pp = gdb.printing.RegexpCollectionPrettyPrinter("crawl")

    pp.add_printer('coord_def', '^coord_def$', coord_def_printer)
#   pp.add_printer('actor', '^actor$', actor_printer)
    pp.add_printer('item_def', '^item_def$', item_def_printer)

    pp.add_printer('FixedVector', '^FixedVector<.*>$', FixedVector_printer)
    pp.add_printer('FixedArray', '^FixedArray<.*>$', FixedVector_printer)

    pp.add_printer('CrawlHashTable', '^CrawlHashTable$', CrawlHashTable_printer)
    pp.add_printer('CrawlVector', '^CrawlVector$', CrawlVector_printer)
    pp.add_printer('CrawlStoreValue', '^CrawlStoreValue$', CrawlStoreValue_printer)

    return pp

gdb.printing.register_pretty_printer(
    gdb.current_objfile(),
    build_pretty_printer())
