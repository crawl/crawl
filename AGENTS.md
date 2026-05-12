# Agent Notes

## WebTiles PWA Orientation

When working on the local WebTiles PWA, start with these files before doing a broad search:

- `crawl-ref/source/webserver/static/pwa/mobile-pwa.js`: opt-in PWA bootstrap, `window.DCSS_PWA`, mobile viewport, WebSocket session tagging, touch keyboard, service worker registration, and PWA resize/layout hooks.
- `crawl-ref/source/webserver/static/pwa/mobile-pwa.css`: PWA-only layout, popup/menu sizing, hidden desktop panes, message sizing, and bottom keyboard styling.
- `crawl-ref/source/webserver/static/pwa/manifest.webmanifest`: install metadata and `/?pwa=1` start URL.
- `crawl-ref/source/webserver/static/pwa/service-worker.js`: root-scoped service worker cache for `/static/pwa/*`. Bump `CACHE_NAME` if installed PWAs keep stale PWA assets.
- `crawl-ref/source/webserver/static/pwa/icons/`: install icons and touch icon assets.

Server/template entry points for the PWA path:

- `crawl-ref/source/webserver/templates/client.html`: conditionally includes the manifest, PWA CSS, and PWA JS when `pwa_enabled` is true.
- `crawl-ref/source/webserver/webtiles/server.py`: detects `?pwa=1` or iPhone/iPod user agents, renders `client.html`, serves `/service-worker.js` from `static/pwa`, and can disable static caching via `no_cache`.
- `crawl-ref/source/webserver/webtiles/ws_handler.py`: handles the `pwa_session` query parameter used to isolate local PWA sessions.

WebTiles game client files that the PWA currently depends on or patches:

- `crawl-ref/source/webserver/game_data/static/game.js`: main WebTiles layout; consult this for map/message sizing, `viewport_show_diameter`, and `$(window).height()` behavior.
- `crawl-ref/source/webserver/game_data/static/options.js`: applies `window.DCSS_PWA.optionMinimums`.
- `crawl-ref/source/webserver/game_data/static/minimap.js`: suppresses minimap behavior while PWA mode is active.
- `crawl-ref/source/webserver/game_data/static/messages.js`: message log DOM and runtime message font option.
- `crawl-ref/source/webserver/game_data/static/style.css`: base desktop WebTiles CSS, menu styles, stat column, and message styles that PWA CSS overrides.
- `crawl-ref/source/webserver/game_data/static/player.js`: stat DOM updates, including HP/MP text and desktop bars.
- `crawl-ref/source/webserver/game_data/templates/game.html`: in-game DOM structure for `#left_column`, `#right_column`, `#stats`, `#messages`, and `#dungeon`.

Local PWA smoke test:

1. Run the webserver from `crawl-ref/source`, not the repo root.
2. Use `webserver/config.yml` with `dgl_mode: false` for the direct local game path.
3. Start with `python webserver/server.py` or the repo virtualenv equivalent.
4. Open `http://localhost:8080/?pwa=1`.
5. In Chrome DevTools, use phone device emulation around 375x812 to match the current mobile screenshots.

Keep PWA changes additive and gated behind `window.DCSS_PWA` or `html.dcss-pwa-mobile` unless the normal desktop WebTiles flow is explicitly in scope.
