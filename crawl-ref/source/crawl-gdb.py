# GDB autoload file

import gdb.printing


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
        return ""

    def children(self):
        # needs libstdc++
        vis = gdb.default_visualizer(self.val["hash_map"].dereference())
        if vis:
            return vis.children()

    def display_hint(self):
        return "map"

def build_pretty_printer():
    pp = gdb.printing.RegexpCollectionPrettyPrinter("crawl")
    pp.add_printer('coord_def', '^coord_def$', coord_def_printer)
#   pp.add_printer('actor', '^actor$', actor_printer)
    pp.add_printer('FixedVector', '^FixedVector<.*>$', FixedVector_printer)
    pp.add_printer('CrawlHashTable', '^CrawlHashTable$', CrawlHashTable_printer)
    return pp

gdb.printing.register_pretty_printer(
    gdb.current_objfile(),
    build_pretty_printer())
