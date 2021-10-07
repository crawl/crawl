# Webtiles server

This is the Webtiles server for Dungeon Crawl Stone Soup.

It's a HTTP server that allows users to play DCSS in a web browser.

You can use it for small, personal setups or for large public servers.

## Contents

* [Prerequisites](#prerequisites)
* [Running the server for testing purposes](#running-the-server-for-testing-purposes)
* [Running a production server](#running-a-production-server)
* [Contributing](#contributing)

## Prerequisites

To run the server, you need:

* Linux or macOS (other platforms are untested)
* Python 2.7 or newer (Python 3.6+ is preferred)
* The Python dependencies specified in `requirements/`
* A build of DCSS with webtiles support

You'll need to compile DCSS with `make WEBTILES=y` to get a suitable binary. For
publicly accessible servers, you should also use `USE_DGAMELAUNCH=y`; this
disables some things like Wizmode, and enables the milestone and player location
display in the lobby.

## Running the server for testing purposes

1. `cd` to the Crawl source directory
2. Compile Crawl with `make WEBTILES=y`
3. Set up a Python virtualenv:

    ```sh
    python3 -m virtualenv -p python3 webserver/venv
    . ./webserver/venv/bin/activate
    pip install -r webserver/requirements/dev.py3.txt
    ```

4. Run the server: `python webserver/server.py`

    (You need to activate the virtualenv every time you start the server)

5. Browse to [localhost:8080](http://localhost:8080/) and you should see the lobby.

When developing, you'll probably want to automatically log in as a
testing user and disable caching of non-game-data files; see the
`autologin` and `no_cache` options in webserver/config.py for this.

## Running a production server

Most production servers use [crawl/dgamelaunch-config](https://github.com/crawl/dgamelaunch-config)
which is a management layer that interacts with the webtiles server and
dgamelaunch (SSH) service.

Use the requirements files `requirements/base.{py2,py3}.txt`.

The server can be configured by modifying the file `config.py`. Most of
the options are commented or should be self-evident. Suggestions:

* Set uid and gid to a non-privileged user
* Enable logging to a file in `logging_config`
* If required, write a script that initializes  user-specific data, like copying
  a default rc file if the user doesn't yet have one. You can have the script be
  run on every login by setting `init_player_program`. There is an example
  script in `util/webtiles-init-player.sh`, but you will probably want to
  customise it.

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
