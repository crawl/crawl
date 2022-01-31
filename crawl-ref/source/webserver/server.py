#!/usr/bin/env python
from __future__ import absolute_import

import os

import webtiles, webtiles.server, webtiles.config

def server_main():
    # load config values from the traditional module location, which should
    # be in the same directory as this file
    import config as server_config
    webtiles.config.init_config_from_module(server_config)

    # server_path tells where to look for games.d. We can't default this,
    # because this directory is outside the module, and though it is set in
    # the current default `config.py` file, we still need to set it here for
    # backwards-compatibility reasons with dgamelaunch-config setups.
    # TODO: is there a better way to handle this?
    webtiles.config.server_path = os.path.dirname(os.path.abspath(__file__))

    webtiles.server.run()

if __name__ == "__main__":
    server_main()
