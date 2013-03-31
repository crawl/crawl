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

    pp.add_printer('FixedVector', '^FixedVector<.*>$', FixedVector_printer)
    pp.add_printer('FixedArray', '^FixedArray<.*>$', FixedVector_printer)

    pp.add_printer('CrawlHashTable', '^CrawlHashTable$', CrawlHashTable_printer)
    pp.add_printer('CrawlVector', '^CrawlVector$', CrawlVector_printer)
    pp.add_printer('CrawlStoreValue', '^CrawlStoreValue$', CrawlStoreValue_printer)

    return pp

gdb.printing.register_pretty_printer(
    gdb.current_objfile(),
    build_pretty_printer())
