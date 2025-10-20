# Changelog for the DCSS webtiles server

This file tracks major changes to the Dungeon Crawl Stone Soup webtiles
server; game changes (including webtiles UI aside from the lobby) go in the
main changelog. The version numbers here are crawl version numbers as an
organizing principle, though it is important to know that the webtiles server
as normally installed (e.g on dgamelaunch-config setups) always runs trunk
code. This file is updated at least at major releases.

## [0.34-a0 through 0.34-a0-958-gd0e9a9ddd5]

Major changes:

- **Breaking change**: The `crypt-r` package is now required for python
  versions 3.13 and later.

Fixes, improvements, changes:

- Fix map data desynchronization that could happen when spectators join.

## [0.33.0] - 2025-05-02

Fixes, improvements, changes:

- Fix an issue with the `signal` module that prevented python versions 3.12 and
  later from working.

## [0.32.0] - 2024-08-29

New features:

- A `max_chat_length` option for the maximum length of a send chat message.
  Defaults to 1000 characters, and setting it to 0 or `None` disables chat
  altogether.
- The `bind_nonsecure` option now accepts a value of `"redirect"` to bind the
  insecure port but redirect requests to the SSL port.
- A command-line option `--ssl-port` to bind to an SSL port. Requires proper
  configuration of `ssl_options`.
- Non-yaml files in `games.d/` are now ignored.
- The server now errors when run using an unsupported version of python.

Fixes, improvements, changes:

- Fix some race conditions that could happen when a forked game process failed
  to run for any reason.
- Fix the gameplay idle timer.
- Improve behaviour when the server receives a shutdown signal.

## [0.31.0] - 2023-01-18

Major changes:

- **Breaking change**: Python versions past end of life are **not supported**
  by webtiles server. (This is a policy change as well as a practical change.)
  As of the 0.31 release, this means that all webtiles servers must ensure that
  a Python version of at least 3.8 (end of life: *2024-10*) must be used to
  run the webtiles server (with associated library available in a chroot).

  See https://devguide.python.org/versions/.

New features:

- Webtiles lobby idle timer, defaulting to 3 hours. This can be configured via
  the option `max_lobby_idle_time`.

Fixes, improvements, changes:

- Fixes to handling of idle crawl processes, process cleanup
- Fixes to handling of a full disk

## [0.30.0] - The Reavers Return - 2023-05-05

Major changes:

- Major revamp of game definitions, with the aim of supporting most of the
  dgl-config templating techniques (in slightly different ways) directly in
  the webserver. (This revamp is backwards compatible.)
- The previous tileschat mutelist feature is converted to block rather than
  just mute.
- Improvements to account and community management features.
- Performance improvements, focusing on blocking I/O and related issues.
- **Breaking change**: Python 2 (soft-deprecated in 0.25 in 2020, fully
  deprecated in 0.29 in 2022) is now completely disallowed, as are old Tornado
  versions.

New features:

- Game definition improvements (see `config.py` and `games.d/base.yaml` for
  documentation and examples)
    * Enhanced templating. Game definitions now support templating via version
      number, if the game config sets a version
    * Allow specifying a partial game template, and using that to instantiate
      particular games (e.g. defining a common set of parameters for all
      stable games, and then instantiating them with just a version)
    * Reload on HUP fully supports game removals and additions, and has better
      caching.
- Per-player block features: the previous muting features are converted to
  instead allow players to block others from tileschat. As part of this,
  players may block spectating entirely (`/block [all]`) and block anonymous
  spectating (`/block [anon]`). A new chat command, `/kick`, allows players
  to (only within a game session, not saved across game sessions) kick a
  spectator for a set amount of time (in minutes).
- Username ban management. See documentation in `config.py` for examples, but
  this can be set via the config option `banned`, and if the file
  `banned_players.yml` exists, the server will try to load a list of bad
  usernames from there.
- Improve the CLI for dealing with account bans/holds: this now allows editing
  all dgl flags that are currently in use somewhere via the `wtutil.py`
  script.
- The webserver now loads configuration from `config.yml` if it exists, after
  loading `config.py`. When running in live-debug mode, it will also try
  `debug-config.yml`. Neither of these files are version controlled.
- New server options:
    * Option `allow_anon_spectate`: allows disabling anonymous spectating at a
      server level.
    * Option `load_logging_rate`: if set to a number `n` other than 0, logs a
      message every `n` seconds indicating player load. 0 by default.
    * Option `slow_callback_alert`: set to a number `n`, logs callbacks that
      take longer than `n` seconds (accepts values like 0.25), and can give an
      indication of why players might be experiencing freezes. Default: `None`.
    * Option `slow_io_alert`: if I/O takes longer than this value (in seconds),
      a warning is logged. Default: `0.250`. (Overlaps with callback logging.)
    * Option `milestone_interval`: allow configuring how often milestone tailing
      happens (applicable only to older game versions).
    * Option `enable_ttyrecs`: lets ttyrec saving be disabled at the server
      level, rather than per game version.
- Admin panel updates. This panel now shows socket stats (including lobby), and
  version info about the webserver
- A RESTful lobby endpoint: `/status/lobby/` now provides lobby state directly.
  See https://github.com/crawl/crawl/commit/f932412b2a00 for motivation.
- A RESTful version endpoint: `/status/version/` provides server version
  information. (Not yet game version information.)

Fixes, improvements, changes:

- The webserver will now attempt to impose a `UNIQUE` constraint on the
  username index. For reasons that still remain unclear, extant userdbs may
  violate this constraint. If this happens, the server will still load fine,
  but log a warning suggesting manual intervention. What this means is that a
  server admin will need to go in and carefully remove duplicated username rows
  via `sqlite3` at the command line (care is especially needed if the rows
  appear to have distinct passwords). This warning is safe to ignore in the
  short term, but fixing this will result in a speedup for database access,
  possibly substantial depending on the database age and size.
- Fixes to blocking calls and blocking call handling
    * Fixed a major source of blocking/freezes on Tornado 6, when players manage
      to fill the UDS socket buffer with key input.
    * Improvements when a game ends with many spectators (previously this
      scaled linearly with the number of watchers)
    * Various other (less major) sources of I/O bound blocking
    * Improvements to logging of blocking calls and blocking I/O (see new
      options related to this)
- Many logging improvements; IP is more consistently shown, some error spam is
  reduced.
- Password recovery tokens now expire by default in 12 hours, not 1 hour; this
  time is now configurable via `recovery_token_lifetime`
- A bug that was preventing change email/password dialogues from proceeding was
  fixed.
- The option `games_config_dir` is now deprecated (and is treated as a bool
  for backwards compatibility)
- Improvements to game config validation and error reporting on startup
- Experimental support for asynchronous file IO, via the `aiofiles` package.

## [0.29.0] - Shooting Stars - 2022-08-23

Major changes:

- **Python 2.7 is officially deprecated.** It is highly recommended to move to
  a recent Python 3 + Tornado version (tested through Python 3.10 and
  Tornado 6.2).
- Many new community management features developed during the 0.29 cycle.

Documentation:

- Changelog added. (Notes back to 0.25.0 are retroactive)

New features:

- Community management changes:
  * new config option `nick_check_fun` that, if defined, takes a nickname and
    returns a boolean to indicate whether it is (dis)allowed. This supplements
    an existing regex-based check.
  * new user "account hold" mode. An account in this state can play, but
    doesn't appear in the lobby, can't chat, can't spectate while logged in,
    and can't generate ghosts. This currently can be set via the `wtutil.py`
    command line utility (similar to bans).
  * new config option `new_accounts_disabled` that entirely disables new
    account creation while set.
  * new config option `new_accounts_hold` that puts all new accounts into
    account hold mode. Also, for use with this option, `wtutil.py` has a clo
    that will clear all held accounts at once. Main interface is via the `ban`
    subcommand of `wtutil.py`. (Use `wtutil.py ban --help` to see the options).
  * Note that for dgamelaunch users, as part of hold mode a DGL ban flag is
    set
- New option `hup_reloads_config` that if set to true, will reload the config
  settings and all game modes on a SIGHUP. (This is off by default, because
  historically dgamelaunch-config setups used SIGHUP for restarting the server.)

Fixes, improvements, changes:

- New account logging is clearer, and more easily extracted from the overall
  log: filter on `Registered`. Also, most account-related log messages now
  include the string `[Account]`.
- Compatibility with Tornado 6.2, Python 3.10

## [0.28.0] - The Rise and Fall of Ignis Zotdust and the Spiders from Hell - 2022-02-04

Major changes:

- Major internal code refactor: the server is now its own package (in the python
  sense) and config is separated into a flexible config module that handles
  defaults, and the server-specific `config.py` of old.
- dgamelaunch ban flags are now supported by the webtiles server.
- Administrative command line options (e.g. banning) are now accessed via a
  separate tool (`wtutil.py`) rather than the server script.

New features:

- Command line options via `wtutil.py`:
  * This script now handles any administrative feature accessible via the
    webtiles code that does not involve starting the server. Run it as you
    would `server.py` (e.g. it may need to chroot if you have that set up).
  * Subcommand: `ban` and `password`. Subcommands support `--help`.
  * `./wtutil.py ban`: set, clear ban flags by username, and list bans.
  * `./wtutil.py password`: set and clear password reset tokens by username.
- The webtiles server now respects the dgamelaunch ban flag, so no need to
  use bad passwords to ban players. This flag works for both ssh and webtiles.

Fixes, improvements, changes:

- Improvements to lobby rendering/messaging and its impact on I/O
- Log spam on some error conditions has been reduced
- Lobby glitches on some error conditions have been fixed

## [0.27.0] - The Cursed Flame - 2021-07-30

Fixes, improvements, changes:

- fixed various bugs that should improve performance and reduce lag

## [0.26.0] - Roll Around the Clock - 2021-01-08

New features:

- Added command line and web interfaces for manually setting and clearing
  password reset tokens, for use on servers that do not have reset-by email
  enabled. The web interface is in the admin panel, and is disabled by default.
- Let users change their password via webtiles (previously, only possible in
  dgamelaunch).
- `lobby_remove` messages now contain information about why the game exited.

Fixes, improvements, changes:

- Compatibility with Tornado 6.1

## [0.25.0] - Magic Surges Out of Thin Air - 2020-06-12

Major changes:

- The webtiles server now supports python 3 and Tornado 4+. Patched Tornado
  2.4 (used on many servers) is now (soft) deprecated, and backwards
  compatibility will be maintained for a while.
- new "live-debug" mode: spin up a test instance side-by-side with production
  servers
- YAML game configuration is supported.

New features:

- "live-debug" mode: call `server.py` with the options `--live-debug` and an
  explicit port (e.g. `-p 8081`) to start up a test-instance designed to run
  side-by-side with a production webtiles server, with the same config. You
  can use this before restarting to make sure there won't be any problems.
- YAML based game configuration: the server now supports a YAML format for
  configuring game modes, as an alternative to config.py. (Unfortunately not
  yet supported by dgamelaunch-config, though.) See [./games.d/base.yaml](./games.d/base.yaml)
  for an example.
- Save information can be shown to players in the webtiles lobby, for crawl
  versions that support it. Configurable on a version-by-version basis.
- Add a tool for admins to send messages to all players on the server, via
  a collapsible admin panel in the lobby visible to admin users.

Fixes, improvements, changes:

- Various fixes to login and error handling
- stuck processes are now killed with SIGABRT, rather than SIGTERM
