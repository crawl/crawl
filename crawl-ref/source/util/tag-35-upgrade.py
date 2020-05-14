#!/usr/bin/env python3

"""Prepare non-C++ files for TAG_MAJOR_VERSION 35 upgrade.

Since most of these files have only basic support for TAG_MAJOR_VERSION, these
edits are done destructively in-place.

You can manually bump the TAG_MAJOR_VERSION by editing tag-version.h.

Bonus: fully remove TAG_MAJOR_VERSION < 35 blocks from C++ files by running:

    util/tag-major-upgrade -t 35
"""

import logging
import os

logging.basicConfig(level=logging.INFO)

OLD_TAG = 34
NEW_TAG = OLD_TAG + 1


def _cleanup_textfile(file_name: str, comment_prefix: str = "#") -> None:
    """Remove old data from a text file with comments.

    We look for lines that are `# start TAG_MAJOR_VERSION == X`. Then behaviour
    until `# end TAG_MAJOR_VERSION` depends on the value of X:
    1. If X == $OLD_TAG, delete all lines
    2. If X == $NEW_TAG, trim `{comment_prefix} ` from each line

    You can customise the comment format, to handle lua files.
    """
    PREFIX_LEN = len(comment_prefix) + 1
    START_MARKER_PREFIX = f"{comment_prefix} start TAG_MAJOR_VERSION == "
    END_MARKER = f"{comment_prefix} end TAG_MAJOR_VERSION\n"
    with open(file_name) as f:
        lines = f.readlines()
    in_tagged_section = None
    with open(file_name, "w") as f:
        for line in lines:
            # If we're in old tag, delete to the end
            if in_tagged_section == OLD_TAG:
                if line == END_MARKER:
                    logging.info("Trimmed old section in %s", file_name)
                    in_tagged_section = None
                continue
            # If we're in new tag, trim the comment prefix
            elif in_tagged_section == NEW_TAG:
                if line == END_MARKER:
                    logging.info("Uncommented new section in %s", file_name)
                    in_tagged_section = None
                    continue
                else:
                    f.write(line[PREFIX_LEN:])
            # If this starts a tag, identify if it's old or new
            elif line.startswith(START_MARKER_PREFIX):
                tag = int(line.strip().rsplit()[-1])
                in_tagged_section = tag
                continue
            # This is a normal line, write it out
            else:
                f.write(line)
        if in_tagged_section is not None:
            raise RuntimeError(f"Didn't find end tag ({END_MARKER!r}) in {file_name}")


def _delete_file(file_name: str) -> None:
    if not os.path.isfile(file_name):
        raise ValueError(f"Asked to delete non-existent file {file_name}")
    logging.info("Deleting %s", file_name)
    os.unlink(file_name)


if __name__ == "__main__":
    SPECIES_BASE = "dat/species"
    for entry in os.scandir(SPECIES_BASE):
        _cleanup_textfile(os.path.join(SPECIES_BASE, entry.name))

    _cleanup_textfile("art-data.txt")
    _cleanup_textfile("util/art-data.pl")

    _cleanup_textfile("rltiles/dc-feat.txt")
    _cleanup_textfile("rltiles/dc-item.txt")
    _cleanup_textfile("rltiles/dc-misc.txt")
    _cleanup_textfile("rltiles/dc-player.txt")

    _cleanup_textfile("dat/des/branches/abyss.des", comment_prefix="--")
    _cleanup_textfile("dat/des/branches/hell.des", comment_prefix="--")
    _cleanup_textfile("dat/des/branches/pan.des", comment_prefix="--")
    # This file has comments in two formats
    _cleanup_textfile("dat/des/branches/tomb.des", comment_prefix="--")
    _cleanup_textfile("dat/des/branches/tomb.des")
    _cleanup_textfile("dat/des/portals/trove.des", comment_prefix="--")
    _cleanup_textfile("dat/des/sprint/zigsprint.des", comment_prefix="--")
    _cleanup_textfile("dat/dlua/v_layouts.lua", comment_prefix="--")
    _delete_file("dat/des/altar/pakellas_experiments.des")
    _delete_file("dat/des/branches/blade.des")
    _delete_file("dat/des/branches/temple_compat.des")
    _delete_file("dat/des/builder/layout_forest.des")
    _delete_file("dat/des/variable/compat.des")

    _delete_file("dat/species/deprecated-draconian-mottled.yaml")
