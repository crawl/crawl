This module contains the core webtiles code for playing Dungeon Crawl Stone
Soup via a browser.

Module organization, highlights:
* The core code runs a single process using `asyncio` via Tornado. (On older
  installs it will not literally use `asyncio`, but Tornado-specific async
  code.) Each playing connection spawns a subprocess in which a crawl binary is
  running. The main IOLoop is started from `server.py` via the `run()` call.
  An example of starting the server is in the (non-module) `../server.py` file.
* Each webtiles connection involves a websockets connection to the javascript
  client and (when playing) a socket connection to a crawl binary subprocess.
  The former is primarily managed by `ws_handler.py` and the latter by
  `process_handler.py` and (for lower-level stuff) `terminal.py`.
* Low-level websockets stuff that is dcss-specific is in `connection.py`.
* Accounts are stored in several databases; see `userdb.py` for the database
  api, and `auth.py` for account authentication code.
* The configuration api and default values are in `config.py`. The
  server-specific values typically come from a configuration file (for historic
  reasons also often named `config.py`) that is *not* part of this module; in
  the main repository a default one that allows running the server
  in-repository is found in `crawl-ref/source/webserver/config.py`.
* The interface with `inotify`, used to detect console games over ssh, can be
  found in `inotify.py`. This is typically only used on a dgamelaunch-config
  setup.
