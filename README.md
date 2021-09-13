# dk-crawl

A fork of [crawl](https://github.com/crawl/crawl.git) to explore the feasibility of adding Azure Active Directory B2C support for user authorization.

## Setup Azure Active Directory B2C environment

* step 1

* step 2

## Setup VSCode/python dev environment

* Install crawl build deps

    ```sh
    sudo apt install build-essential libncursesw5-dev bison flex liblua5.1-0-dev \
    libsqlite3-dev libz-dev pkg-config python3-yaml binutils-gold python-is-python3 \
    libsdl2-image-dev libsdl2-mixer-dev libsdl2-dev \
    libfreetype6-dev libpng-dev fonts-dejavu-core advancecomp pngcrush python3-pip
    ```

* Compile crawl with webtiles and dgamelaunch

    ```sh
    cd ./crawl-ref/source
    make -j4 WEBTILES=y DGAMELAUNCH=y
    ```

* Create a folder for crawldata.

* Edit config.py to point to your crawldata folder.

* Set environment variables for AAD_B2C client id and secret.

    ```sh
    export B2C_CLIENT_ID="<replace with client id of B2C application>"
    export B2C_CLIENT_SECRET="<replace with client secret of B2C application"
    export KV_CLIENT_ID="<replace with client id of SPN used for Keyvault authentication>"
    export KV_CLIENT_SECRET="<replace with client secret of SPN used for Keyvault authentication>"
    export KV_TENANT_ID="<replace with tenant id of SPN used for Keyvault authentication>"
    export KV_NAME="<replace with name of keyvault>"
    ```

* Set up a Python virtualenv. From ./crawl-ref/source/

    ```sh
    sudo apt install python3-pip
    pip install virtualenv
    python3 -m virtualenv -p python3 webserver/venv
    . ./webserver/venv/bin/activate
    pip install -r webserver/requirements/dev.py3.txt
    pip install azure-identity azure-keyvault-secrets msal
    ```

## Debugging

    * Adjust ./.vscode/launch.json as needed

    * Select venv python binary with Ctrl-Shift-P -> Python: Select interpreter

## Problems to solve

    * "password" for user registration

    * enforce unique crawl handles through B2C graph calls at registration time

    * oauth attack vectors

    * comparison to crawl origin and feasibility of PR

    * add additional IDPs (goog, fb)
