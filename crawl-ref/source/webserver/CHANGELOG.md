# Changelog for the DCSS webtiles server

This file tracks major changes to the Dungeon Crawl Stone Soup webtiles
server; game changes (including webtiles UI aside from the lobby) go in the
main changelog. The version numbers here are crawl version numbers as an
organizing principle, though it is important to know that the webtiles server
as normally installed (e.g on dgamelaunch-config setups) always runs trunk
code. This file is update at least at major releases.

## [0.30-a0 through 0.30-a0-29-g142975f185]

New features:
- Option `load_logging_rate`: if set to a number `n` other than 0, logs a
  message every `n` seconds indicating player load. 0 by default.
- Option `slow_callback_alert`: set to a number `n`, logs callbacks that take
  longer than `n` seconds (accepts values like 0.25), and can give an
  indication of why players might be experiencing freezes. Defaults to `None`.

Fixes, improvements, changes:
- Fixed a major source of blocking/freezes on Tornado 6, when players manage
  to fill the UDS socket buffer with key input.
- A bug that was preventing change email/password dialogues from proceeding was
  fixed.

## [0.29.0] - Shooting Stars - 2022-08-23

Major changes:
- **Python 2.7 is officially deprecated.** It is highly recommended to move to
  a recent Python 3 + Tornado version (tested through Python 3.10 and
  Tornado 6.2).
- Many new community management features developed during the 0.29 cycle.

Documentation:
- Changelog added. (Notes back to X.XX are retroactive)

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
  a collapsable admin panel in the lobby visible to admin users.

Fixes, improvements, changes:
- Various fixes to loggin and error handling
- stuck processes are now killed with SIGABRT, rather than SIGTERM
