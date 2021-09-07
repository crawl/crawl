# dk-crawl

A fork of [crawl](https://github.com/crawl/crawl.git) to explore the feasibility of adding Azure Active Directory B2C support for user authorization.

## Setup Azure Active Directory B2C environment

* step 1

* step 2

## Setup VSCode/python dev environment

* Compile crawl with webtiles and dgamelaunch

    ```sh
    cd ./crawl-ref/source
    make WEBTILES=y DGAMELAUNCH=y
    ```

* Create a folder for crawldata.

* Edit config.py to point to your crawldata folder.

* Set environment variables for AAD_B2C client id and secret.

    ```sh
    export CLIENT_ID="client id value"
    export CLIENT_SECRET="client secret value"
    ```

* Set up a Python virtualenv. From ./crawl-ref/source/

    ```sh
    python3 -m virtualenv -p python3 webserver/venv
    . ./webserver/venv/bin/activate
    pip install -r webserver/requirements/dev.py3.txt
    ```
