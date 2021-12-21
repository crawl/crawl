# Dungeon Crawl Stone Soup webtiles server

This is the Webtiles server for Dungeon Crawl Stone Soup. It is a server that
allows users to play DCSS in a web browser. You can use it for small, personal
setups or for large public servers. It consists of three main parts:

* A python package that implements the server, using the
  [Tornado](https://www.tornadoweb.org/en/stable/) library. By and large, this
  code should work with any version of DCSS that supports webtiles.
* Static html/javascript, and other support files that are independent
  of the version of DCSS that is running, found in [templates/](templates/),
  [static/](static/), and [contrib/](contrib/).
* Version-specific html/javascript, found in [game_data/](game_data/). Files
  here are specific to the version of the crawl binary at the same point in
  the repository.

The entry point for running the server is `server.py` in this directory; see
`config.py` and `games.d/` for more on configuring the server. See
[webtiles/README.md](webtiles/README.md) for a brief overview of the python
app's code.

## Contents

* [Prerequisites](#prerequisites)
* [Running the server for testing purposes](#running-the-server-for-testing-purposes)
* [Running a production server](#running-a-production-server)
* [Contributing](#contributing)

## Prerequisites

To run the server, you need:

* Linux or macOS (other platforms are untested).
* Python 3.6 or newer. (Earlier versions will currently work but are
  deprecated.)
* The Python dependencies specified in `requirements/`, in particular,
  to just run the server, Tornado 6+  and `pyyaml` (also needed for building).
  Earlier versions of Tornado are deprecated.
* A build of DCSS with webtiles support.

You'll need to compile DCSS with `make WEBTILES=y` to get a suitable binary. For
publicly accessible servers, you should also use `USE_DGAMELAUNCH=y`; this
disables some things like Wizmode, and enables the milestone and player location
display in the lobby.

## Running the server for testing purposes

One way to install the prerequisites cleanly is to use a virtual environment.
You can also install them manually using e.g. `pip` or `conda` and skip step 3.

1. `cd` to the Crawl source directory.
2. Compile Crawl with `make WEBTILES=y` (or `make debug WEBTILES=y`).
3. Set up a Python virtualenv:

    ```sh
    python3 -m virtualenv -p python3 webserver/venv
    . ./webserver/venv/bin/activate
    pip install -r webserver/requirements/dev.py3.txt
    ```

4. Run the server: `python webserver/server.py`

    (You need to activate the virtualenv every time you start the server)

5. Browse to [localhost:8080](http://localhost:8080/) and you should see the lobby.

When developing, you may want to automatically log in as a testing user and
disable caching of non-game-data files; see the `autologin` and `no_cache`
options in webserver/config.py for this.

## Running a production server

Most production servers use
[crawl/dgamelaunch-config](https://github.com/crawl/dgamelaunch-config)
which is a management layer that interacts with the webtiles server and
dgamelaunch (SSH) service. A production server will typically need to run the
version-independent webtiles server from the crawl repository's current trunk,
and can support running multiple versions of the version-specific code. (When
running from the repository directly, only one version can be used at a time.)

Use the requirements files `requirements/base.py3.txt`.

The server can be configured by modifying the file `config.py`. Most of
the options are commented or should be self-evident. Suggestions:

* Set uid and gid to a non-privileged user
* Enable logging to a file in `logging_config`
* If required, write a script that initializes  user-specific data, like copying
  a default rc file if the user doesn't yet have one. You can have the script be
  run on every login by setting `init_player_program`. There is an example
  script in `util/webtiles-init-player.sh`, but you will probably want to
  customise it. `dgamelaunch-config` provides such a script.

## Contributing

For Python developers, several utilities are available:

* `make format` -- format code
* `make lint` -- run several Python linters (with Flake8)
* `tox` -- run tests on all supported Python versions
* `requirements.in/sync.sh` -- update requirements files


### Code Coverage

```sh
coverage run --source . --omit 'venv/*,*_test.py' -m pytest
coverage html
open htmlcov/index.html
```
