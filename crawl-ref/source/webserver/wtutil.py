#!/usr/bin/env python
from __future__ import absolute_import

import os

import webtiles, webtiles.server, webtiles.config

def main():
    import config as server_config
    webtiles.config.init_config_from_module(server_config)
    webtiles.config.server_path = os.path.dirname(os.path.abspath(__file__))
    webtiles.server.run_util()

if __name__ == "__main__":
    main()
