# Dungeon Crawl Stone Soup Webtiles Server

This is the Webtiles server and client.

## Prerequisites

* Linux
* Python 2.6/2.7
* Tornado 4.0+
* Dungeon Crawl compiled with WEBTILES=y

Python 2.6 requires the ordereddict module, available
[here](http://pypi.python.org/pypi/ordereddict/).

You should probably run this server using a Python virtualenv. This is a Python
install in a local directory, with dependencies installed locally rather than
system-wide. You will need the `virtualenv` command, then run:

```shell
virtualenv venv
. ./venv/bin/activate
pip install -r requirements.txt
```

## Running the server for testing purposes

1. Compile Crawl with `WEBTILES=y`
2. Copy `webserver/config.toml.sample` to `webserver/config.toml`
3. Activate your Python virtualenv (if you're using one):
`. ./venv/bin/activate`
4. Run `python webserver/server.py` from the source directory.
5. Visit [http://localhost:8080/](http://localhost:8080/)


## Running a real server

The server can be configured by modifying config.toml. When looking for this
file, the server first looks at the WEBTILES_CONF environment variable, then in
the working directory, and then the directory containing the server's python
code.

You should probably set uid and gid to a non-privileged user; you will also
want to enable logging to a file.

For publicly accessible servers, you should compile crawl with
`USE_DGAMELAUNCH=y`. This disables some things like Wizmode, and enables the
milestone and player location display in the lobby.

You will also need a script that initializes user-specific data, like copying a
default rc file if the user doesn't yet have one -- currently this is not done
by the server. You can have the script be run on every login by setting
init_player_program. There is an example script in
util/webtiles-init-player.sh, but you will probably want to customize it.
