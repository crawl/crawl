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

* [Running with Docker](#docker)
* [Prerequisites](#prerequisites)
* [Running the server for testing purposes](#running-the-server-for-testing-purposes)
* [Running a production server](#running-a-production-server)
* [Contributing](#contributing)

## Running with Docker

This is the easiest way to run a webtiles server locally for testing.
The only prequisite is Docker itself. If you still want to install and run
everything on your local system, or you you want to run a production server,
skip to [Prerequisites](#prerequisites) and read from there.

### Build the Docker container

This only needs to be done once ever, unless the system dependencies change,
or your Docker cache is cleared.

```
./scripts/build-docker
```

### Start the Docker container

This gives you an interactive prompt inside the container. Port 8080 is
exposed and routed to 127.0.0.1 so you can access webtiles from your
local browser.

```
./scripts/start-docker
```

### Build and run server inside container

Now you have an interactive prompt inside the container you can run
the scripts that were copied there. To build a webtiles build of crawl:

```
./build-crawl
```

(If you want to make a different build of crawl just cd into
`/crawl/crawl-ref/source` and run any usual build command.)

Once a build has been created, run the server:

```
./start-webtiles
```

Since port 8080 is mapped to localhost you should now be able to go to
http://localhost:8080 and play webtiles!

## Prerequisites

From here on are instructions for building and running the webserver on your
own machine or a production server.

To run the server, you need:

* Linux, macOS, or windows using WSL (MinGW webtiles is not supported).
* Python 3.6 or newer. (Earlier versions may work but are not supported.)
* The Python dependencies specified in `requirements/`, in particular,
  minimally to just run the server, you need Tornado 6+  and `pyyaml` (also
  required for building the crawl binary).
* A build of DCSS with webtiles support.

To get webtiles support in the binary, you'll need to compile DCSS with `make
WEBTILES=y` (and any other appropriate options). For publicly accessible
servers, you should also use `USE_DGAMELAUNCH=y`; this disables some things
like Wizmode (except to admin users), and enables the milestone and player
location display in the lobby.

## Running the server for testing purposes

One way to install the prerequisites cleanly is to use a virtual environment.
You can also install them manually using e.g. `pip` or `conda` and skip step 3.

1. Install the crawl repository, build prerequisites (see [INSTALL.md](../../INSTALL.md)),
   and webtiles prerequisites (see below).
2. `cd` to the Crawl source directory. (`crawl-ref/source` from the repository
   root.)
3. Compile Crawl with `make WEBTILES=y` (or `make debug WEBTILES=y`).
4. Run the server: `python3 webserver/server.py`

    If your python binary is named something else (e.g. just `python`) use that
    instead. If you are using a virtualenv, you need to activate it every time
    you start the server)

5. Browse to [localhost:8080](http://localhost:8080/) and you should see the
   lobby.

When developing, you may want to automatically log in as a testing user and
disable caching of non-game-data files; see the `autologin` and `no_cache`
options in webserver/config.py for this.

**Locale issues**: the server requires a UTF-8 locale. If this isn't set, it
is likely as simple as setting an environment variable. For example, when
starting up the server, instead of the above, try:

    LANG=en_US.UTF-8 python3 webserver/server.py

## Installing prerequisites

On linux and MacOS, you will typically need to install just a few python support
packages to get a minimal running webserver. For the core list, see
[requirements/base.py3.txt](requirements/base.py3.txt), and for a full list of packages used in the test
infrastructure, see [requirements/dev.py3.txt](requirements/dev.py3.txt). On
WSL, you will need to first set up the distribution.

### Installing minimal prerequisites manually

You can install the prerequisites manually using e.g. `pip` or `conda`. See the
requirements files mentioned above for a full list, but generally for running
the server, you need:

    pip install pyyaml tornado

or

    conda install pyyaml tornado

The requirements files do pin specific versions, but generally any recent
version of either package should work. Your package manager may also provide
these python libraries in other forms.

### Installing full prerequisites in a virtual environment

If you want all the prerequisites in the dev requirements, the easiest way to
install them cleanly is likely to use a virtual environment. The following
sequence of commands should set this up:

    ```sh
    python3 -m virtualenv -p python3 webserver/venv
    . ./webserver/venv/bin/activate
    pip install -r webserver/requirements/dev.py3.txt
    ```

If you install the prerequisites this way, you will need to reactivate the
venv (line 2 above) each time you run it.

### Setting up WSL for running the webtiles server

First, install a distribution. See [the official WSL docs](https://docs.microsoft.com/en-us/windows/wsl/install) for more detail. These instructions are for Debian.

    wsl --install -d Debian

After a while, you will get to a linux command prompt. From here, you will need
to install the core packages for building crawl as well as the python packages
needed by the webserver. See [INSTALL.md](../../INSTALL.md) for more details on the former.
Here is a sequence of commands that should handle this including tornado and
pyyaml:

    sudo apt-get update
    sudo apt-get install build-essential bzip2 python-minimal ncurses-term locales-all sqlite3 libpcre3 liblua5.1-0 locales autoconf build-essential lsof bison libncursesw5-dev libsqlite3-dev flex sudo libbot-basicbot-perl git python3-yaml lua5.1 liblua5.1-dev man libpng-dev python3-tornado

At this point, you should have enough to build and run the server following the
usual linux instructions. One caveat is that depending on the version of WSL,
you may need to use a different IP address than `127.0.0.1` (aka `localhost`)
to access the webtiles server from a browser. The IP address to use can be
discovered by running from within a WSL shell:

    ifconfig

and looking for the `inet` value. This IP address will *not* be usable outside
of the machine running webtiles server, and configuring it for LAN or internet
use is beyond the scope of this document; for more information, see:
https://github.com/microsoft/WSL/issues/4150.

For a more detailed rundown of this process, see:
[http://crawl.montres.org.uk/wsl-webtiles.txt](http://crawl.montres.org.uk/wsl-webtiles.txt).

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
